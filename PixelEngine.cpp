// PixelEngine.cpp

#include "stdafx.h"
#include "PixelEngine.h"
#include <math.h>
#include <vector>

namespace {

// Perceptual match tolerances (HSV). Hue is kept tight so red can't drift into
// yellow; value is loose so the same colour stays a match across lighter/darker
// brightness shifts. Saturation sits in between.
const float kHueTolDeg  = 15.0f;
const float kSatTol     = 0.30f;
const float kValTol     = 0.60f;
const float kGrayThresh = 0.10f;  // below this S, hue is meaningless

// Trigger guard. A cell must mismatch this many sweeps in a row before it
// counts as "flagged", and at least kCellsToTrigger cells must be flagged
// simultaneously for the engine to enter the error state.
const BYTE kConfirmSweeps  = 1;
const int  kCellsToTrigger = 5;

// dHash configuration. Downsample the watched region to (kHashGridW x kHashGridH)
// luminance cells, build a 64-bit hash from horizontal neighbour comparisons,
// and fire shapeEdge when Hamming distance to the reference exceeds the
// threshold. 8 bits / 64 (~12.5%) is the empirically common cutoff.
// In "ignore text" mode the threshold is widened so small text edits like
// "12 MB/sec" -> "13 MB/sec" don't trip it; only major shape shifts do.
const int kHashGridW           = 9;   // 9 columns -> 8 horizontal differences per row
const int kHashGridH           = 8;   // 8 rows  -> 8 * 8 = 64 bits total
const int kHashThresholdNormal = 8;
const int kHashThresholdLoose  = 25;  // ~39% of bits — needs a real structural change

// Box-blur radius applied to the luminance plane *before* dHash downsampling.
// 0 disables. 1 smears single-pixel text strokes into neighbours so a
// "12 MB/sec" -> "13 MB/sec" edit barely perturbs the per-cell average; higher
// values cost linearly more and risk merging genuine structural detail.
const int kHashBlurRadius = 1;

// Adaptive-mask learning. After this many ticks of monitoring with "Ignore text"
// active, the engine commits a noise mask of pixels that drifted at least
// kMaskMismatchThreshold times during learning. At 10ms/tick this is ~8 seconds.
const int kLearningDuration      = 800;
const int kMaskMismatchThreshold = 3;

// Stability detector (Ignore-text mode, post-learning). Compares each frame's
// per-pixel luminance against the *previous* frame on the unmasked pixels.
// Two stages: first wait for activity (so we don't fire on an already-static
// scene), then wait for stillness — a configurable number of consecutive
// ticks with zero motion. This isolates "the task just finished" from "the
// task is currently running" or "nothing is happening yet".
const int kStabilityChangePercent = 20;  // % of unmasked pixels to enter stage 1
const int kStabilityTicksRequired = 300; // ~3 s at 10ms/tick of stillness to fire
const int kStabilityLumaTol       = 5;   // per-pixel |dY| > this counts as motion

void RgbToHsv(COLORREF c, float& h, float& s, float& v)
{
	float r = GetRValue(c) / 255.0f;
	float g = GetGValue(c) / 255.0f;
	float b = GetBValue(c) / 255.0f;
	float mx = (r > g) ? (r > b ? r : b) : (g > b ? g : b);
	float mn = (r < g) ? (r < b ? r : b) : (g < b ? g : b);
	float d  = mx - mn;
	v = mx;
	s = (mx > 0.0f) ? (d / mx) : 0.0f;
	if (d < 1e-6f)            h = 0.0f;
	else if (mx == r)         h = 60.0f * fmodf((g - b) / d, 6.0f);
	else if (mx == g)         h = 60.0f * (((b - r) / d) + 2.0f);
	else                      h = 60.0f * (((r - g) / d) + 4.0f);
	if (h < 0.0f) h += 360.0f;
}

bool ColorsMatchPerceptual(COLORREF a, COLORREF b)
{
	if (a == b) return true;

	float ha, sa, va, hb, sb, vb;
	RgbToHsv(a, ha, sa, va);
	RgbToHsv(b, hb, sb, vb);

	float dV = fabsf(va - vb);

	// Near-gray on BOTH sides: hue is unstable noise, compare brightness only.
	// If only one side is gray, that's a genuine colour shift (e.g. red vs white
	// at the same brightness) and must fall through to the H/S/V check below.
	if (sa < kGrayThresh && sb < kGrayThresh)
		return dV <= kValTol;

	float dS = fabsf(sa - sb);
	float dH = fabsf(ha - hb);
	if (dH > 180.0f) dH = 360.0f - dH;  // wrap on the colour wheel

	return dH <= kHueTolDeg && dS <= kSatTol && dV <= kValTol;
}

// Convert a BGRA32 pixel pointer to a COLORREF (0x00BBGGRR).
inline COLORREF PixelToColorRef(const BYTE* p)
{
	return RGB(p[2], p[1], p[0]);
}

// 8-bit luminance from a BGRA32 pixel. Integer Rec. 601 approximation:
// Y ~= 0.299*R + 0.587*G + 0.114*B, scaled to fixed-point /256.
inline int LumaFromBgra(const BYTE* p)
{
	return (p[2] * 77 + p[1] * 150 + p[0] * 29) >> 8;
}

// Materialise a width*height 8-bit luminance plane from a BGRA32 buffer.
void BuildLumaPlane(const BYTE* pixels, int width, int height, BYTE* outLuma)
{
	const int stride = width * 4;
	for (int y = 0; y < height; ++y)
	{
		const BYTE* row = pixels + y * stride;
		BYTE*       dst = outLuma + y * width;
		for (int x = 0; x < width; ++x)
			dst[x] = static_cast<BYTE>(LumaFromBgra(row + x * 4));
	}
}

// Separable box blur on an 8-bit luminance plane. Edge pixels use a shrunken
// window rather than wraparound/zero-padding so the borders don't get darkened.
void ApplyBoxBlurLuma(BYTE* luma, int width, int height, int radius)
{
	if (radius < 1 || !luma || width <= 0 || height <= 0) return;

	std::vector<BYTE> tmp(static_cast<size_t>(width) * height);

	// Horizontal pass: luma -> tmp.
	for (int y = 0; y < height; ++y)
	{
		const BYTE* row = luma + y * width;
		BYTE*       out = tmp.data() + y * width;
		for (int x = 0; x < width; ++x)
		{
			int x0 = x - radius; if (x0 < 0)      x0 = 0;
			int x1 = x + radius; if (x1 >= width) x1 = width - 1;
			int sum = 0;
			for (int xi = x0; xi <= x1; ++xi) sum += row[xi];
			out[x] = static_cast<BYTE>(sum / (x1 - x0 + 1));
		}
	}

	// Vertical pass: tmp -> luma.
	for (int x = 0; x < width; ++x)
	{
		for (int y = 0; y < height; ++y)
		{
			int y0 = y - radius; if (y0 < 0)       y0 = 0;
			int y1 = y + radius; if (y1 >= height) y1 = height - 1;
			int sum = 0;
			for (int yi = y0; yi <= y1; ++yi) sum += tmp[yi * width + x];
			luma[y * width + x] = static_cast<BYTE>(sum / (y1 - y0 + 1));
		}
	}
}

// Compute a 64-bit dHash. Build a blurred luminance plane (suppresses text-
// scale noise), downsample to kHashGridW x kHashGridH by averaging, then for
// each row emit 8 bits comparing each cell to its right neighbour.
UINT64 ComputeDHash(const BYTE* pixels, int width, int height)
{
	if (!pixels || width <= 0 || height <= 0) return 0;

	std::vector<BYTE> luma(static_cast<size_t>(width) * height);
	BuildLumaPlane(pixels, width, height, luma.data());
	ApplyBoxBlurLuma(luma.data(), width, height, kHashBlurRadius);

	int  sum[kHashGridH][kHashGridW];
	int  cnt[kHashGridH][kHashGridW];
	::ZeroMemory(sum, sizeof(sum));
	::ZeroMemory(cnt, sizeof(cnt));

	for (int y = 0; y < height; ++y)
	{
		int gy = (y * kHashGridH) / height;
		if (gy >= kHashGridH) gy = kHashGridH - 1;
		for (int x = 0; x < width; ++x)
		{
			int gx = (x * kHashGridW) / width;
			if (gx >= kHashGridW) gx = kHashGridW - 1;
			sum[gy][gx] += luma[y * width + x];
			cnt[gy][gx] += 1;
		}
	}

	int avg[kHashGridH][kHashGridW];
	for (int r = 0; r < kHashGridH; ++r)
		for (int c = 0; c < kHashGridW; ++c)
			avg[r][c] = cnt[r][c] ? (sum[r][c] / cnt[r][c]) : 0;

	UINT64 hash = 0;
	int bit = 0;
	for (int r = 0; r < kHashGridH; ++r)
		for (int c = 0; c < kHashGridW - 1; ++c, ++bit)
			if (avg[r][c] > avg[r][c + 1])
				hash |= (UINT64(1) << bit);

	return hash;
}

// Hamming distance between two 64-bit hashes (Kernighan's bit-count).
int Hamming64(UINT64 a, UINT64 b)
{
	UINT64 x = a ^ b;
	int n = 0;
	while (x) { x &= x - 1; ++n; }
	return n;
}

// dHash variant that skips pixels marked noisy in `mask` (1 = skip). Fully
// masked cells fall back to a sentinel luma so the resulting bit is stable
// between ref and live (both will use this same routine with the same mask).
// Blur runs over the full luma plane *before* masking — masked pixels can
// still leak a softened value into nearby unmasked ones, which is fine: we
// just want the cell averages to be stable across ref and live.
UINT64 ComputeDHashMasked(const BYTE* pixels, int width, int height, const BYTE* mask)
{
	if (!pixels || !mask || width <= 0 || height <= 0) return 0;

	std::vector<BYTE> luma(static_cast<size_t>(width) * height);
	BuildLumaPlane(pixels, width, height, luma.data());
	ApplyBoxBlurLuma(luma.data(), width, height, kHashBlurRadius);

	int sum[kHashGridH][kHashGridW];
	int cnt[kHashGridH][kHashGridW];
	::ZeroMemory(sum, sizeof(sum));
	::ZeroMemory(cnt, sizeof(cnt));

	for (int y = 0; y < height; ++y)
	{
		int gy = (y * kHashGridH) / height;
		if (gy >= kHashGridH) gy = kHashGridH - 1;
		for (int x = 0; x < width; ++x)
		{
			if (mask[y * width + x]) continue;  // masked: omit from cell luma
			int gx = (x * kHashGridW) / width;
			if (gx >= kHashGridW) gx = kHashGridW - 1;
			sum[gy][gx] += luma[y * width + x];
			cnt[gy][gx] += 1;
		}
	}

	int avg[kHashGridH][kHashGridW];
	for (int r = 0; r < kHashGridH; ++r)
		for (int c = 0; c < kHashGridW; ++c)
			avg[r][c] = cnt[r][c] ? (sum[r][c] / cnt[r][c]) : 128;

	UINT64 hash = 0;
	int bit = 0;
	for (int r = 0; r < kHashGridH; ++r)
		for (int c = 0; c < kHashGridW - 1; ++c, ++bit)
			if (avg[r][c] > avg[r][c + 1])
				hash |= (UINT64(1) << bit);

	return hash;
}

// Allocate a top-down 32bpp DIB section. outBits receives the pixel buffer.
HBITMAP CreateBgraDib(int width, int height, BYTE** outBits)
{
	BITMAPINFO bi;
	::ZeroMemory(&bi, sizeof(bi));
	bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth       = width;
	bi.bmiHeader.biHeight      = -height;   // negative => top-down
	bi.bmiHeader.biPlanes      = 1;
	bi.bmiHeader.biBitCount    = 32;
	bi.bmiHeader.biCompression = BI_RGB;
	void* bits = NULL;
	HBITMAP hbm = ::CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
	if (outBits) *outBits = static_cast<BYTE*>(bits);
	return hbm;
}

}  // namespace

CPixelEngine::CPixelEngine()
	: m_refSize(0, 0)
	, m_x(kScanStart)
	, m_y(kScanStart)
	, m_state(kClean)
	, m_errorCount(0)
	, m_refDib(NULL)
	, m_refPixels(NULL)
	, m_liveDib(NULL)
	, m_livePixels(NULL)
	, m_liveMemDC(NULL)
	, m_liveOldBmp(NULL)
	, m_refHash(0)
	, m_shapeState(0)
	, m_ignoreText(false)
	, m_changeCount(NULL)
	, m_noiseMask(NULL)
	, m_learnTicks(0)
	, m_learned(false)
	, m_prevLumBuf(NULL)
	, m_stabilityStage(0)
	, m_stableTicks(0)
	, m_errorMap(new BYTE[kErrorMapSize][kErrorMapSize]())  // value-init zeroes
{
}

CPixelEngine::~CPixelEngine()
{
	ReleaseBuffers();
	delete[] m_errorMap;
	m_errorMap = NULL;
}

void CPixelEngine::ReleaseBuffers()
{
	if (m_liveMemDC)
	{
		if (m_liveOldBmp) ::SelectObject(m_liveMemDC, m_liveOldBmp);
		::DeleteDC(m_liveMemDC);
		m_liveMemDC  = NULL;
		m_liveOldBmp = NULL;
	}
	if (m_liveDib) { ::DeleteObject(m_liveDib); m_liveDib = NULL; m_livePixels = NULL; }
	if (m_refDib)  { ::DeleteObject(m_refDib);  m_refDib  = NULL; m_refPixels  = NULL; }
	if (m_changeCount) { delete[] m_changeCount; m_changeCount = NULL; }
	if (m_noiseMask)   { delete[] m_noiseMask;   m_noiseMask   = NULL; }
	if (m_prevLumBuf)  { delete[] m_prevLumBuf;  m_prevLumBuf  = NULL; }
	m_learnTicks     = 0;
	m_learned        = false;
	m_stabilityStage = 0;
	m_stableTicks    = 0;
}

void CPixelEngine::CaptureReference(const CRect& screenRect)
{
	ReleaseBuffers();

	m_refSize = CSize(screenRect.Width(), screenRect.Height());
	if (m_refSize.cx <= 0) m_refSize.cx = 1;
	if (m_refSize.cy <= 0) m_refSize.cy = 1;

	HDC hdcScreen = ::GetDC(NULL);

	// Reference DIB: filled once from the live desktop.
	m_refDib  = CreateBgraDib(m_refSize.cx, m_refSize.cy, &m_refPixels);
	HDC hdcMemRef = ::CreateCompatibleDC(hdcScreen);
	HGDIOBJ oldRef = ::SelectObject(hdcMemRef, m_refDib);
	::BitBlt(hdcMemRef, 0, 0, m_refSize.cx, m_refSize.cy,
	         hdcScreen, screenRect.left, screenRect.top, SRCCOPY);
	::SelectObject(hdcMemRef, oldRef);
	::DeleteDC(hdcMemRef);

	// Live DIB + its memory DC: kept alive across Step() calls so each tick
	// is one BitBlt from screen, no DC create/destroy churn.
	m_liveDib    = CreateBgraDib(m_refSize.cx, m_refSize.cy, &m_livePixels);
	m_liveMemDC  = ::CreateCompatibleDC(hdcScreen);
	m_liveOldBmp = (HBITMAP)::SelectObject(m_liveMemDC, m_liveDib);

	::ReleaseDC(NULL, hdcScreen);

	// Fingerprint the reference frame so dHash comparisons in Step() are O(1).
	m_refHash = ComputeDHash(m_refPixels, m_refSize.cx, m_refSize.cy);

	// Adaptive-mask buffers + stability-detector previous-luma buffer, sized
	// to the captured area. All start zeroed. The prev-luma buffer is seeded
	// from the first live frame once learning completes (inside Step()).
	const int total = m_refSize.cx * m_refSize.cy;
	m_changeCount = new UINT16[total]();
	m_noiseMask   = new BYTE  [total]();
	m_prevLumBuf  = new BYTE  [total]();
	m_learnTicks     = 0;
	m_learned        = false;
	m_stabilityStage = 0;
	m_stableTicks    = 0;

	Reset();
}

void CPixelEngine::Reset()
{
	m_x = kScanStart;
	m_y = kScanStart;
	m_state = kClean;
	m_errorCount = 0;
	m_shapeState = 0;
	::ZeroMemory(m_errorMap, static_cast<size_t>(kErrorMapSize) * kErrorMapSize);
}

CPixelEngine::StepResult CPixelEngine::Step(const CRect& screenRect)
{
	StepResult r;
	r.edge = kEdgeNone;
	r.shapeEdge = kEdgeNone;
	r.shapeState = m_shapeState;  // updated below as detectors run
	r.rowWrapped = false;

	int xLimit = (m_refSize.cx > 0 && m_refSize.cx < kErrorMapSize) ? (int)m_refSize.cx : (int)kErrorMapSize;
	int yLimit = (m_refSize.cy > 0 && m_refSize.cy < kErrorMapSize) ? (int)m_refSize.cy : (int)kErrorMapSize;

	m_x++;
	if (m_x >= xLimit)
	{
		m_x = kScanStart;
		m_y++;
		r.rowWrapped = true;
		if (m_y >= yLimit)
			m_y = kScanStart;
	}

	r.x = m_x;
	r.y = m_y;

	if (!HasReference())
	{
		r.state = m_state;
		return r;
	}

	// Single BitBlt refreshes the whole live buffer. All pixel reads below
	// (this step + the recovery loop) are then raw memory accesses.
	{
		HDC hdcScreen = ::GetDC(NULL);
		::BitBlt(m_liveMemDC, 0, 0, m_refSize.cx, m_refSize.cy,
		         hdcScreen, screenRect.left, screenRect.top, SRCCOPY);
		::ReleaseDC(NULL, hdcScreen);
	}

	const int totalPixels = m_refSize.cx * m_refSize.cy;

	// Adaptive-mask learning. Active only while "Ignore text" is on and we
	// haven't yet finalised the mask. Each tick: count which pixels drifted.
	// At kLearningDuration: pixels with >= kMaskMismatchThreshold mismatches
	// become permanently masked, and the reference hash is recomputed so the
	// dHash channel ignores their contribution from here on.
	if (m_ignoreText && !m_learned && m_changeCount && m_noiseMask)
	{
		for (int i = 0; i < totalPixels; ++i)
		{
			const BYTE* pr = m_refPixels  + i * 4;
			const BYTE* pl = m_livePixels + i * 4;
			if (pr[0] != pl[0] || pr[1] != pl[1] || pr[2] != pl[2])
			{
				if (m_changeCount[i] < 0xFFFF)
					++m_changeCount[i];
			}
		}
		++m_learnTicks;
		if (m_learnTicks >= kLearningDuration)
		{
			for (int i = 0; i < totalPixels; ++i)
				m_noiseMask[i] = (m_changeCount[i] >= kMaskMismatchThreshold) ? 1 : 0;
			m_refHash = ComputeDHashMasked(m_refPixels, m_refSize.cx, m_refSize.cy, m_noiseMask);

			// Seed prev-luma buffer from current live frame so the very first
			// stability tick sees zero motion (live vs itself) instead of a
			// spurious mass change vs the zero-initialised buffer.
			for (int i = 0; i < totalPixels; ++i)
				m_prevLumBuf[i] = (BYTE)LumaFromBgra(m_livePixels + i * 4);

			m_learned        = true;
			m_stabilityStage = 0;
			m_stableTicks    = 0;
		}
	}

	const bool useMask = (m_ignoreText && m_learned && m_noiseMask);

	// Shape-change channel. Two modes:
	//   * Default (Ignore-text OFF): dHash 64-bit fingerprint vs reference.
	//     Fires on small structural changes (a button state flips, an icon
	//     changes). Good general-purpose detector.
	//   * Ignore-text ON (post-learning): bulk-percent check. Counts how
	//     many of the unmasked pixels have meaningfully changed and only
	//     fires when at least kBulkChangePercent of them have. This prevents
	//     a gradually filling progress bar from firing mid-way — by the time
	//     the bar fills to 60%+ of the watched area, the task is essentially
	//     complete, or the dialog has closed and the whole region changed.
	//   During the learning phase (Ignore-text ON but not learned yet) the
	//   channel stays silent — the mask isn't ready, any signal would lie.
	if (m_ignoreText && m_learned && m_prevLumBuf)
	{
		// Stability detector: count motion vs *previous* frame on unmasked
		// luminance. State machine isolates a completed event from ongoing
		// motion or an already-static scene.
		int changedCount  = 0;
		int totalUnmasked = 0;
		for (int i = 0; i < totalPixels; ++i)
		{
			if (m_noiseMask[i]) continue;
			++totalUnmasked;
			int liveLum = LumaFromBgra(m_livePixels + i * 4);
			int diff    = liveLum - (int)m_prevLumBuf[i];
			if (diff < 0) diff = -diff;
			if (diff > kStabilityLumaTol)
				++changedCount;
			m_prevLumBuf[i] = (BYTE)liveLum;
		}

		// Only run the state machine if we haven't already fired this cycle.
		// Once stability fires, m_shapeState stays at 1 until Stop+Start —
		// the user gets one clean event per detected completion.
		if (m_shapeState == 0)
		{
			if (m_stabilityStage == 0)
			{
				int pct = totalUnmasked ? (changedCount * 100 / totalUnmasked) : 0;
				if (pct >= kStabilityChangePercent)
				{
					m_stabilityStage = 1;
					m_stableTicks    = 0;
				}
			}
			else
			{
				if (changedCount == 0)
				{
					++m_stableTicks;
					if (m_stableTicks >= kStabilityTicksRequired)
					{
						m_shapeState = 1;
						r.shapeEdge  = kEdgeRising;
					}
				}
				else
				{
					m_stableTicks = 0;  // motion seen — restart stillness count
				}
			}
		}
	}
	else if (!m_ignoreText)
	{
		UINT64 liveHash = ComputeDHash(m_livePixels, m_refSize.cx, m_refSize.cy);
		bool shapeDiff = (Hamming64(m_refHash, liveHash) > kHashThresholdNormal);
		if (shapeDiff && m_shapeState == 0)
		{
			m_shapeState = 1;
			r.shapeEdge = kEdgeRising;
		}
		else if (!shapeDiff && m_shapeState == 1)
		{
			m_shapeState = 0;
			r.shapeEdge = kEdgeFalling;
		}
	}

	const int stride = m_refSize.cx * 4;

	COLORREF cRef  = PixelToColorRef(m_refPixels  + m_y * stride + m_x * 4);
	COLORREF cLive = PixelToColorRef(m_livePixels + m_y * stride + m_x * 4);

	// Masked pixels are invisible to the per-pixel detector: treat as match.
	const bool cellMasked = useMask && m_noiseMask[m_y * m_refSize.cx + m_x];

	BYTE prev = m_errorMap[m_x][m_y];
	if (cellMasked || ColorsMatchPerceptual(cRef, cLive))
	{
		// Clearing a previously flagged cell drops it out of the error count;
		// clearing a still-pending cell just resets its dwell counter.
		if (prev >= kConfirmSweeps)
		{
			m_errorMap[m_x][m_y] = 0;
			if (m_errorCount > 0) --m_errorCount;
		}
		else if (prev > 0)
		{
			m_errorMap[m_x][m_y] = 0;
		}
	}
	else
	{
		if (prev < kConfirmSweeps)
		{
			BYTE next = static_cast<BYTE>(prev + 1);
			m_errorMap[m_x][m_y] = next;
			if (next == kConfirmSweeps)
			{
				++m_errorCount;
				if (m_state == kClean && m_errorCount >= kCellsToTrigger)
				{
					m_state = kInError;
					r.edge = kEdgeRising;
				}
			}
		}
	}

	// Rapid recovery: while in error, re-check every flagged cell from the
	// live buffer captured above. No extra GDI calls — pure memory reads.
	if (m_state == kInError && m_errorCount > 0)
	{
		for (int yy = 0; yy < kErrorMapSize && m_errorCount > 0; ++yy)
		{
			for (int xx = 0; xx < kErrorMapSize && m_errorCount > 0; ++xx)
			{
				if (m_errorMap[xx][yy] < kConfirmSweeps) continue;

				// Cells outside the captured region (overlay shrank?) can't
				// be re-checked; drop them so we don't stay stuck in error.
				if (xx >= m_refSize.cx || yy >= m_refSize.cy)
				{
					m_errorMap[xx][yy] = 0;
					--m_errorCount;
					continue;
				}

				// Masked cell: clear without comparison.
				if (useMask && m_noiseMask[yy * m_refSize.cx + xx])
				{
					m_errorMap[xx][yy] = 0;
					--m_errorCount;
					continue;
				}

				COLORREF cR = PixelToColorRef(m_refPixels  + yy * stride + xx * 4);
				COLORREF cL = PixelToColorRef(m_livePixels + yy * stride + xx * 4);
				if (ColorsMatchPerceptual(cR, cL))
				{
					m_errorMap[xx][yy] = 0;
					--m_errorCount;
				}
			}
		}

		if (m_errorCount == 0)
		{
			m_state = kClean;
			r.edge  = kEdgeFalling;
		}
	}

	r.state      = m_state;
	r.shapeState = m_shapeState;
	return r;
}
