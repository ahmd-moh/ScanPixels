// TestPixelProjectDlg.cpp : implementation file
//

#include "stdafx.h"
#include "TestPixelProject.h"
#include "TestPixelProjectDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CTestPixelProjectDlg dialog

CTestPixelProjectDlg::CTestPixelProjectDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CTestPixelProjectDlg::IDD, pParent)
	, m_MsgPixelScan(0)
{
	//{{AFX_DATA_INIT(CTestPixelProjectDlg)
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CTestPixelProjectDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTestPixelProjectDlg)
	DDX_Control(pDX, IDC_LIST1, m_ListCmd);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CTestPixelProjectDlg, CDialog)
	//{{AFX_MSG_MAP(CTestPixelProjectDlg)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_LBN_DBLCLK(IDC_LIST1, OnDblclkList1)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTestPixelProjectDlg message handlers

BOOL CTestPixelProjectDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// Same registered message name as ScanPixels — both processes get
	// the same UINT back from the system.
	m_MsgPixelScan = ::RegisterWindowMessage(_T("WHM_SCANPXL"));

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CTestPixelProjectDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CTestPixelProjectDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

LRESULT CTestPixelProjectDlg::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	if (m_MsgPixelScan != 0 && message == m_MsgPixelScan)
	{
		CString strFormat;
		strFormat.Format(
			_T("--> Message[%u] WPARAM[ ID_SCAN [%u] ERROR_OCC[%u] ]  LParam[X:[ %u ] Y:[ %u ] ]"),
			message,
			LOWORD(wParam), HIWORD(wParam),
			LOWORD(lParam), HIWORD(lParam));
		SetDlgItemText(IDC_EDIT1, strFormat);
		m_ListCmd.AddString(strFormat);
	}

	return CDialog::WindowProc(message, wParam, lParam);
}

void CTestPixelProjectDlg::OnDblclkList1()
{
	m_ListCmd.ResetContent();
}
