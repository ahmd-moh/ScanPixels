// PixelEngine.h : pure pixel-diff engine, decoupled from UI.
//
// CPixelEngine owns a reference snapshot of a screen rectangle and walks
// pixels row-major comparing the snapshot against the live desktop. It
// reports rising/falling edge transitions so the caller can broadcast.

#if !defined(AFX_PIXELENGINE_H__INCLUDED_)
#define AFX_PIXELENGINE_H__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif

class CPixelEngine
{
public:
	enum State
	{
		kClean   = 0,
		kInError = 1,
	};

	enum Edge
	{
		kEdgeNone    = 0,
		kEdgeRising  = 1,  // last clean pixel cell newly mismatched
		kEdgeFalling = 2,  // last erroring cell cleared; back to all-clean
	};

	struct StepResult
	{
		State state;
		Edge  edge;        // per-pixel HSV detector (fires on first mismatched cell)
		Edge  shapeEdge;   // shape detector: dHash (normal) or bulk-percent (ignore-text)
		int   shapeState;  // current shape state: 0 = clean, 1 = changed
		int   x;
		int   y;
		bool  rowWrapped;  // y advanced this step (one full row done)
	};

	CPixelEngine();
	~CPixelEngine();

	// Snapshot desktop pixels inside screenRect into the reference bitmap.
	// Resets the scan cursor and clears the error map.
	void CaptureReference(const CRect& screenRect);

	// Advance the cursor one pixel and compare reference vs. live desktop
	// at the corresponding screen location. screenRect is the region's
	// current screen position (may differ from capture time).
	StepResult Step(const CRect& screenRect);

	void  Reset();
	State CurrentState() const { return m_state; }
	bool  HasReference() const { return m_refPixels != NULL; }

	// When true, the dHash channel uses a much looser Hamming threshold so
	// small text-sized fingerprint shifts don't fire; only major structural
	// changes (windows closing, dialogs appearing) cross the bar.
	void  SetIgnoreText(bool b) { m_ignoreText = b; }

private:
	// Max scan area. Larger overlays are clamped to the top-left of this
	// window for now (matches the original 120x120 sweep).
	enum { kErrorMapSize = 144, kScanStart = 5 };

	void ReleaseBuffers();

	CSize   m_refSize;
	int     m_x;
	int     m_y;
	State   m_state;
	int     m_errorCount;

	// 32-bit BGRA pixel buffers backed by DIB sections. One BitBlt per Step()
	// refills m_livePixels; reference is captured once on Start. Index by
	// (y * m_refSize.cx + x) * 4 — B,G,R,A in that byte order.
	HBITMAP m_refDib;
	BYTE*   m_refPixels;
	HBITMAP m_liveDib;
	BYTE*   m_livePixels;
	HDC     m_liveMemDC;
	HBITMAP m_liveOldBmp;

	// dHash channel: 64-bit perceptual fingerprint of the reference frame,
	// compared each tick against the live hash. Fires shapeEdge when the
	// Hamming distance crosses the configured threshold.
	UINT64  m_refHash;
	int     m_shapeState;  // 0 = matching ref, 1 = currently in shape-change
	bool    m_ignoreText;  // dialog-driven: widens dHash threshold

	// Adaptive noise mask. During the first kLearningDuration ticks after
	// CaptureReference (only while m_ignoreText is on), each pixel's mismatch
	// frequency is counted. Pixels that drift too often (e.g. the digits of a
	// file-copy "MB/sec" readout) get tagged in m_noiseMask and are skipped
	// thereafter by both the per-pixel and dHash detectors. The reference
	// hash is rebuilt with the mask applied at the end of learning.
	UINT16* m_changeCount;
	BYTE*   m_noiseMask;
	int     m_learnTicks;
	bool    m_learned;

	// Stability detector (Ignore-text mode, post-learning). Compares live
	// vs previous frame on luminance only. Stage 0 waits for activity (a
	// chunk of unmasked pixels changing); Stage 1 waits for stillness
	// (zero motion for kStabilityTicksRequired consecutive ticks) and then
	// fires the shape rising edge once. Stays fired until next Start.
	BYTE*   m_prevLumBuf;
	int     m_stabilityStage;  // 0 = waiting for change, 1 = waiting for stillness
	int     m_stableTicks;

	// Heap-owned: at kErrorMapSize=144 this is ~20 KB. Embedding it inline
	// would bloat every containing object and, with the dialog living on the
	// stack, can land the array on an uncommitted page during ZeroMemory.
	BYTE    (*m_errorMap)[kErrorMapSize];

	// Non-copyable (owns raw buffers / GDI handles).
	CPixelEngine(const CPixelEngine&);
	CPixelEngine& operator=(const CPixelEngine&);
};

#endif // !defined(AFX_PIXELENGINE_H__INCLUDED_)
