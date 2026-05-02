// TestPixelProjectDlg.h : header file
//

#if !defined(AFX_TESTPIXELPROJECTDLG_H__3CF16A89_21B3_4993_BA56_5B92B7935413__INCLUDED_)
#define AFX_TESTPIXELPROJECTDLG_H__3CF16A89_21B3_4993_BA56_5B92B7935413__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

/////////////////////////////////////////////////////////////////////////////
// CTestPixelProjectDlg dialog

class CTestPixelProjectDlg : public CDialog
{
// Construction
public:
	CTestPixelProjectDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	//{{AFX_DATA(CTestPixelProjectDlg)
	enum { IDD = IDD_TESTPIXELPROJECT_DIALOG };
	CListBox	m_ListCmd;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTestPixelProjectDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
	//}}AFX_VIRTUAL

// Implementation
protected:
	HICON m_hIcon;
	UINT  m_MsgPixelScan;

	// Generated message map functions
	//{{AFX_MSG(CTestPixelProjectDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnDblclkList1();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TESTPIXELPROJECTDLG_H__3CF16A89_21B3_4993_BA56_5B92B7935413__INCLUDED_)
