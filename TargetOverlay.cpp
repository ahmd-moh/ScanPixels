// TargetOverlay.cpp

#include "stdafx.h"
#include "TargetOverlay.h"

namespace {

const COLORREF kBorderColor = RGB(0, 200, 0);
LPCTSTR        kClassName   = _T("ScanPixelsTargetOverlay");

// Center "+" marker. Each arm reaches kCenterArm pixels from the centre,
// kCenterThick pixels thick. Drawn only when the overlay is big enough
// that the cross has clear air between itself and the border.
const int kCenterArm        = 20;
const int kCenterThick      = 2;
const int kCenterGapToFrame = 8;  // minimum gap between cross tip and border

// Corner L-mark arm length. The overlay always renders as four such marks;
// the centre "+" is added on top of them only while unlocked (editing).
const int kCornerArmLen     = 20;

void EnsureClassRegistered()
{
	static bool s_registered = false;
	if (s_registered) return;

	WNDCLASS wc;
	::ZeroMemory(&wc, sizeof(wc));
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = ::DefWindowProc;
	wc.hInstance     = AfxGetInstanceHandle();
	wc.hCursor       = ::LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = ::CreateSolidBrush(kBorderColor);
	wc.lpszClassName = kClassName;
	::RegisterClass(&wc);

	s_registered = true;
}

}  // namespace

CTargetOverlay::CTargetOverlay()
	: m_locked(false)
{
}

void CTargetOverlay::SetLocked(bool locked)
{
	if (m_locked == locked)
		return;
	m_locked = locked;

	if (GetSafeHwnd())
	{
		CRect rc;
		GetClientRect(&rc);
		RebuildBorderRegion(rc.Width(), rc.Height());
		Invalidate(TRUE);
	}
}

CTargetOverlay::~CTargetOverlay()
{
}

BEGIN_MESSAGE_MAP(CTargetOverlay, CWnd)
	//{{AFX_MSG_MAP(CTargetOverlay)
	ON_WM_PAINT()
	ON_WM_NCHITTEST()
	ON_WM_SIZE()
	ON_WM_SETCURSOR()
	ON_WM_NCLBUTTONDOWN()
	ON_WM_GETMINMAXINFO()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CTargetOverlay::Create(const CRect& screenRect, CWnd* pOwner)
{
	EnsureClassRegistered();

	const DWORD dwStyle   = WS_POPUP | WS_VISIBLE;
	const DWORD dwExStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW;

	BOOL ok = CreateEx(dwExStyle, kClassName, _T("ScanPixels Target"),
		dwStyle,
		screenRect.left, screenRect.top,
		screenRect.Width(), screenRect.Height(),
		pOwner ? pOwner->GetSafeHwnd() : NULL,
		NULL);

	if (!ok)
		return FALSE;

	RebuildBorderRegion(screenRect.Width(), screenRect.Height());
	return TRUE;
}

CRect CTargetOverlay::GetScreenRect() const
{
	CRect r(0, 0, 0, 0);
	if (GetSafeHwnd())
		::GetWindowRect(GetSafeHwnd(), &r);
	return r;
}

void CTargetOverlay::SetScreenRect(const CRect& r)
{
	if (!GetSafeHwnd()) return;
	SetWindowPos(NULL, r.left, r.top, r.Width(), r.Height(),
		SWP_NOZORDER | SWP_NOACTIVATE);
}

void CTargetOverlay::RebuildBorderRegion(int cx, int cy)
{
	if (cx <= 2 * kBorderThickness || cy <= 2 * kBorderThickness)
	{
		// Too small for a hole; window becomes a solid rectangle.
		CRgn rgn;
		rgn.CreateRectRgn(0, 0, cx, cy);
		SetWindowRgn((HRGN)rgn.Detach(), TRUE);
		return;
	}

	// Always: four corner L-marks. Long edges between corners are absent.
	const int t = kBorderThickness;
	int arm = kCornerArmLen;
	const int maxArm = ((cx < cy ? cx : cy) / 2) - 1;
	if (arm > maxArm) arm = maxArm;
	if (arm < t)      arm = t;

	const int rects[8][4] = {
		// top-left L
		{ 0,         0,         arm,       t         },
		{ 0,         0,         t,         arm       },
		// top-right L
		{ cx - arm,  0,         cx,        t         },
		{ cx - t,    0,         cx,        arm       },
		// bottom-left L
		{ 0,         cy - t,    arm,       cy        },
		{ 0,         cy - arm,  t,         cy        },
		// bottom-right L
		{ cx - arm,  cy - t,    cx,        cy        },
		{ cx - t,    cy - arm,  cx,        cy        },
	};

	CRgn shape;
	shape.CreateRectRgn(0, 0, 0, 0);
	for (int i = 0; i < 8; ++i)
	{
		CRgn strip;
		strip.CreateRectRgn(rects[i][0], rects[i][1], rects[i][2], rects[i][3]);
		shape.CombineRgn(&shape, &strip, RGN_OR);
	}

	// While unlocked (editing), overlay the centre "+" move grip on top.
	// The cross vanishes as soon as the overlay locks (recording).
	if (!m_locked)
	{
		const int minDim = (cx < cy) ? cx : cy;

		// Adaptive sizing: shrink the gap and the arm so the cross stays
		// visible even on the smallest 60x60 overlay instead of disappearing.
		int gap = kCenterGapToFrame;
		if (gap > minDim / 8) gap = minDim / 8;
		if (gap < 2)          gap = 2;

		int armCap = (minDim / 2) - kBorderThickness - gap;
		int arm    = (armCap < kCenterArm) ? armCap : kCenterArm;

		if (arm >= 3)
		{
			const int mx = cx / 2;
			const int my = cy / 2;
			const int halfT = kCenterThick / 2;

			CRgn hbar, vbar, cross;
			hbar.CreateRectRgn(mx - arm, my - halfT,
			                   mx + arm, my - halfT + kCenterThick);
			vbar.CreateRectRgn(mx - halfT,            my - arm,
			                   mx - halfT + kCenterThick, my + arm);
			cross.CreateRectRgn(0, 0, 0, 0);
			cross.CombineRgn(&hbar, &vbar, RGN_OR);

			shape.CombineRgn(&shape, &cross, RGN_OR);
		}
	}

	SetWindowRgn((HRGN)shape.Detach(), TRUE);
}

void CTargetOverlay::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);
	RebuildBorderRegion(cx, cy);
}

void CTargetOverlay::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
	lpMMI->ptMinTrackSize.x = kMinTrackW;
	lpMMI->ptMinTrackSize.y = kMinTrackH;
	lpMMI->ptMaxTrackSize.x = kMaxTrackW;
	lpMMI->ptMaxTrackSize.y = kMaxTrackH;
}

void CTargetOverlay::OnPaint()
{
	CPaintDC dc(this);
	CRect rc;
	GetClientRect(&rc);
	CBrush green(kBorderColor);
	dc.FillRect(&rc, &green);
}

LRESULT CTargetOverlay::OnNcHitTest(CPoint pt)
{
	// Locked: report HTCLIENT so the system starts no move/size loop.
	if (m_locked)
		return HTCLIENT;

	// The window region only covers the four corner L-marks (always) and
	// the centre "+" (only while unlocked). Corner L → diagonal resize;
	// centre cross → four-way move.
	CRect rc;
	GetWindowRect(&rc);
	int x  = pt.x - rc.left;
	int y  = pt.y - rc.top;
	int w  = rc.Width();
	int h  = rc.Height();
	int mx = w / 2;
	int my = h / 2;

	if (abs(x - mx) <= kCenterArm && abs(y - my) <= kCenterArm)
		return HTCAPTION;

	bool inLeft   = (x < kCornerGrip);
	bool inRight  = (x >= w - kCornerGrip);
	bool inTop    = (y < kCornerGrip);
	bool inBottom = (y >= h - kCornerGrip);

	if (inTop    && inLeft)  return HTTOPLEFT;
	if (inTop    && inRight) return HTTOPRIGHT;
	if (inBottom && inLeft)  return HTBOTTOMLEFT;
	if (inBottom && inRight) return HTBOTTOMRIGHT;

	return HTCAPTION;
}

BOOL CTargetOverlay::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	if (m_locked)
	{
		::SetCursor(::LoadCursor(NULL, IDC_ARROW));
		return TRUE;
	}

	LPCTSTR cur = NULL;
	switch (nHitTest)
	{
	case HTCAPTION:                       cur = IDC_SIZEALL;  break;  // four-way move
	case HTTOPLEFT: case HTBOTTOMRIGHT:   cur = IDC_SIZENWSE; break;  // ⤡
	case HTTOPRIGHT: case HTBOTTOMLEFT:   cur = IDC_SIZENESW; break;  // ⤢
	}
	if (cur != NULL)
	{
		::SetCursor(::LoadCursor(NULL, cur));
		return TRUE;
	}
	return CWnd::OnSetCursor(pWnd, nHitTest, message);
}

void CTargetOverlay::OnNcLButtonDown(UINT nHitTest, CPoint point)
{
	if (m_locked)
		return;

	// WS_POPUP alone doesn't auto-handle SC_SIZE on hit-test. Kick the
	// system into the right modal loop explicitly.
	UINT cmd = 0;
	switch (nHitTest)
	{
	case HTCAPTION:       cmd = SC_MOVE | HTCAPTION;        break;
	case HTTOPLEFT:       cmd = SC_SIZE | WMSZ_TOPLEFT;     break;
	case HTTOPRIGHT:      cmd = SC_SIZE | WMSZ_TOPRIGHT;    break;
	case HTBOTTOMLEFT:    cmd = SC_SIZE | WMSZ_BOTTOMLEFT;  break;
	case HTBOTTOMRIGHT:   cmd = SC_SIZE | WMSZ_BOTTOMRIGHT; break;
	default:
		CWnd::OnNcLButtonDown(nHitTest, point);
		return;
	}

	ReleaseCapture();
	SendMessage(WM_SYSCOMMAND, cmd, MAKELPARAM(point.x, point.y));
}
