// ScanPixelsDlg.h : header file
//

#if !defined(AFX_SCANPIXELSDLG_H__04E8CE8B_DB3C_4FB5_9498_776760E64A0F__INCLUDED_)
#define AFX_SCANPIXELSDLG_H__04E8CE8B_DB3C_4FB5_9498_776760E64A0F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

/////////////////////////////////////////////////////////////////////////////
// CScanPixelsDlg dialog

#define TIMER_SCAN       0
#define TIMER_ACTIONSET  2

class CScanPixelsDlg : public CDialog
{
// Construction
public:
	UINT    m_unIDClient;
	int     m_unTimerSpeed;
	int     m_unTimerAction;
	bool    m_bContinues;
	int     m_nySrc;
	int     m_nxSrc;
	CScanPixelsDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	//{{AFX_DATA(CScanPixelsDlg)
	enum { IDD = IDD_SCANPIXELS_DIALOG };
	CButton	m_chIsRealTime;
	CSpinButtonCtrl	m_SpinAction;
	CSpinButtonCtrl	m_SpinSpeed;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CScanPixelsDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	virtual void PostNcDestroy();
	//}}AFX_VIRTUAL

// Implementation
protected:
	enum { kErrorMapSize = 120 };

	HICON       m_hIcon;
	void        CreateHole(CRgn& rgn);
	CRect       m_rctClientFrame;
	const char* m_lpszClassName;
	HWND        m_hWndFrame;
	void        CaptureWnd(CWnd* wnd, HWND hwndClientArea, BOOL FullWnd);
	BOOL        ComparePixels(int nX, int nY);
	void        DrawFlagWorking(bool bSet);

	int     m_Error[kErrorMapSize][kErrorMapSize];
	int     m_ErrorDetected;
	UINT    m_MsgPixelScan;
	CString m_strTextString;

	bool    m_bCaptured;
	bool    m_bHoleStarted;
	int     m_nScanX;
	int     m_nScanY;
	bool    m_bSendOnRise;
	bool    m_bSendOnFall;

	// Generated message map functions
	//{{AFX_MSG(CScanPixelsDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnButtonStart();
	afx_msg void OnMove(int x, int y);
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnDeltaposSpinSpeed(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDeltaposSpinActionset(NMHDR* pNMHDR, LRESULT* pResult);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SCANPIXELSDLG_H__04E8CE8B_DB3C_4FB5_9498_776760E64A0F__INCLUDED_)
