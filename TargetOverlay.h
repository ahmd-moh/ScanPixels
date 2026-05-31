// TargetOverlay.h : frameless top-most green-rectangle window.
//
// The window region is built as outer rect minus inner rect, so the window
// physically IS the green border (four strips); the interior is genuinely
// click-through to whatever is underneath. Border supports drag-to-move
// and corner-grip drag-to-resize via WM_NCHITTEST.

#if !defined(AFX_TARGETOVERLAY_H__INCLUDED_)
#define AFX_TARGETOVERLAY_H__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif

class CTargetOverlay : public CWnd
{
public:
	CTargetOverlay();
	virtual ~CTargetOverlay();

	BOOL  Create(const CRect& screenRect, CWnd* pOwner = NULL);
	CRect GetScreenRect() const;
	void  SetScreenRect(const CRect& screenRect);

	// When locked, the overlay ignores mouse input: it cannot be moved or
	// resized, and its cursor reverts to the default arrow.
	void  SetLocked(bool locked);
	bool  IsLocked() const { return m_locked; }

	enum { kBorderThickness = 3 };
	enum { kCornerGrip      = 20 };  // matches the visible corner L arm length

	// Interactive-resize bounds. Min keeps the corner Ls + centre cross legible;
	// max sits slightly above the engine's 120x120 scan window so the visible
	// frame never advertises more area than is actually being watched.
	enum { kMinTrackW = 60,  kMinTrackH = 60  };
	enum { kMaxTrackW = 144, kMaxTrackH = 122 };

protected:
	//{{AFX_MSG(CTargetOverlay)
	afx_msg void    OnPaint();
	afx_msg LRESULT OnNcHitTest(CPoint point);
	afx_msg void    OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL    OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void    OnNcLButtonDown(UINT nHitTest, CPoint point);
	afx_msg void    OnGetMinMaxInfo(MINMAXINFO* lpMMI);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	void RebuildBorderRegion(int cx, int cy);

	bool m_locked;
};

#endif // !defined(AFX_TARGETOVERLAY_H__INCLUDED_)
