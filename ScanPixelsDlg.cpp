// ScanPixelsDlg.cpp : implementation file
//

#include "stdafx.h"
#include "ScanPixels.h"
#include "ScanPixelsDlg.h"

#include <stdlib.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

namespace {

// Offsets from the dialog's screen origin to the top-left of the scan frame.
// Account for the window non-client area + frame's position inside the dialog.
const int kCaptureOffsetX = 132;
const int kCaptureOffsetY = 12;

// Scan walk starts a few pixels in from the frame edge to avoid the border.
const int kScanStartX = 5;
const int kScanStartY = 5;

// Status flag indicators ("LED" rectangles) painted on the dialog.
const int kFlagW = 28;
const int kFlagH = 8;

// Default values for the speed/action timers (milliseconds).
const int kDefaultScanSpeedMs = 10;
const int kDefaultActionMs    = 1;

// Spin control range cap.
const int kSpinMax = 100000;

// Error-state values used by m_ErrorDetected.
const int kErrorNone     = 0;  // pixels match
const int kErrorFresh    = 1;  // pixels differ this tick
const int kErrorReported = 2;  // mismatch already broadcast

}  // namespace

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CScanPixelsDlg dialog

CScanPixelsDlg::CScanPixelsDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CScanPixelsDlg::IDD, pParent)
	, m_unIDClient(0)
	, m_unTimerSpeed(kDefaultScanSpeedMs)
	, m_unTimerAction(kDefaultActionMs)
	, m_bContinues(false)
	, m_nySrc(0)
	, m_nxSrc(0)
	, m_lpszClassName(NULL)
	, m_hWndFrame(NULL)
	, m_ErrorDetected(kErrorNone)
	, m_MsgPixelScan(0)
	, m_bCaptured(false)
	, m_bHoleStarted(false)
	, m_nScanX(kScanStartX)
	, m_nScanY(kScanStartY)
	, m_bSendOnRise(true)
	, m_bSendOnFall(true)
{
	//{{AFX_DATA_INIT(CScanPixelsDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_MsgPixelScan = ::RegisterWindowMessage(_T("WHM_SCANPXL"));

	for (int i = 0; i < kErrorMapSize; ++i)
		for (int j = 0; j < kErrorMapSize; ++j)
			m_Error[i][j] = 0;
}

void CScanPixelsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CScanPixelsDlg)
	DDX_Control(pDX, IDC_CHECK_REALTIME, m_chIsRealTime);
	DDX_Control(pDX, IDC_SPIN_ACTIONSET, m_SpinAction);
	DDX_Control(pDX, IDC_SPIN_SPEED, m_SpinSpeed);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CScanPixelsDlg, CDialog)
	//{{AFX_MSG_MAP(CScanPixelsDlg)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_START, OnButtonStart)
	ON_WM_MOVE()
	ON_WM_TIMER()
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_SPEED, OnDeltaposSpinSpeed)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_ACTIONSET, OnDeltaposSpinActionset)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CScanPixelsDlg message handlers

void CScanPixelsDlg::CreateHole(CRgn& rgn)
{
	CRect WindowRect;
	GetWindowRect(WindowRect);

	CRgn WindowRgn;
	WindowRgn.CreateRectRgn(0, 0, WindowRect.Width(), WindowRect.Height());

	// ThisRgn accumulates all hole regions across calls to this function.
	static CRgn ThisRgn;

	if (!m_bHoleStarted)
	{
		ThisRgn.DeleteObject();
		ThisRgn.CreateRectRgn(0, 0, 0, 0);
		ThisRgn.CopyRgn(&rgn);
		m_bHoleStarted = true;
	}
	else
	{
		ThisRgn.CombineRgn(&ThisRgn, &rgn, RGN_OR);
	}

	CRgn HoleRgn;
	HoleRgn.CreateRectRgn(0, 0, 0, 0);
	HoleRgn.CombineRgn(&ThisRgn, &WindowRgn, RGN_XOR);
	SetWindowRgn((HRGN)HoleRgn.m_hObject, TRUE);
}

BOOL CScanPixelsDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_lpszClassName = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW,
		NULL, CreateSolidBrush(RGB(0, 0, 0)), NULL);

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	CRect rct;
	GetDlgItem(IDC_STATIC_FLAMEPIXELS)->GetClientRect(&rct);

	HWND hWndDlg = GetDlgItem(IDC_STATIC_FLAMEPIXELS)->m_hWnd;

	CRgn rgn;
	rgn.CreateRectRgn(140, 40, rct.right + 10, rct.bottom + 32);
	CreateHole(rgn);

	m_hWndFrame = CreateWindowEx(WS_EX_CONTROLPARENT, m_lpszClassName,
		_T(""), WS_VISIBLE | WS_CHILD,
		0, 0, 120, 120, hWndDlg, NULL, NULL, NULL);

	GetDlgItem(IDC_STATIC_FLAMEPIXELS)->GetClientRect(m_rctClientFrame);

	m_bContinues = false;

	m_SpinAction.SetRange32(0, kSpinMax);
	m_SpinSpeed.SetRange32(0, kSpinMax);

	SetDlgItemInt(IDC_EDIT2, m_unTimerSpeed);
	SetDlgItemInt(IDC_EDIT1, m_unTimerAction);

	::srand(static_cast<unsigned>(::GetTickCount()));
	m_unIDClient = static_cast<UINT>(::rand());

	m_strTextString.Format(_T("ScanPixels [ID Control: %u]"), m_unIDClient);
	SetWindowText(m_strTextString);

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CScanPixelsDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CScanPixelsDlg::OnPaint()
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
HCURSOR CScanPixelsDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CScanPixelsDlg::CaptureWnd(CWnd* wnd, HWND hwndClientArea, BOOL FullWnd)
{
	CWnd* pWndClientTo = CWnd::FromHandle(hwndClientArea);
	if (pWndClientTo == NULL || wnd == NULL)
		return;

	CClientDC ClientDC(pWndClientTo);

	CDC dc;
	if (FullWnd)
	{
		HDC hdc = ::GetWindowDC(wnd->m_hWnd);
		dc.Attach(hdc);
	}
	else
	{
		HDC hdc = ::GetDC(wnd->m_hWnd);
		dc.Attach(hdc);
	}

	CDC memDC;
	memDC.CreateCompatibleDC(&ClientDC);

	CRect r;
	if (FullWnd)
		pWndClientTo->GetWindowRect(&r);
	else
		pWndClientTo->GetClientRect(&r);

	CSize sz(r.Width(), r.Height());

	CBitmap bm;
	bm.CreateCompatibleBitmap(&dc, sz.cx, sz.cy);

	CBitmap* oldbm = memDC.SelectObject(&bm);

	ClientDC.BitBlt(0, 0, sz.cx, sz.cy,
		&dc,
		m_nxSrc + kCaptureOffsetX,
		m_nySrc + kCaptureOffsetY,
		SRCCOPY);

	memDC.SelectObject(oldbm);

	bm.Detach();  // make sure bitmap not deleted with CBitmap object
}

void CScanPixelsDlg::OnButtonStart()
{
	if (m_bContinues)
	{
		// Currently scanning -> stop.
		KillTimer(TIMER_SCAN);
		KillTimer(TIMER_ACTIONSET);
		m_bContinues = false;
		SetDlgItemText(IDC_BUTTON_START, _T("Start Monitor"));

		GetDlgItem(IDC_SPIN_SPEED)->EnableWindow(TRUE);
		GetDlgItem(IDC_EDIT1)->EnableWindow(TRUE);
		GetDlgItem(IDC_EDIT2)->EnableWindow(TRUE);
		GetDlgItem(IDC_CHECK_REALTIME)->EnableWindow(TRUE);
		GetDlgItem(IDC_SPIN_ACTIONSET)->EnableWindow(TRUE);
		return;
	}

	// Currently stopped -> take a fresh snapshot and start scanning.
	CaptureWnd(GetDesktopWindow(), m_hWndFrame, TRUE);
	m_bCaptured = true;

	m_ErrorDetected = kErrorNone;
	m_nScanX = kScanStartX;
	m_nScanY = kScanStartY;
	m_bSendOnRise = true;
	m_bSendOnFall = true;

	m_bContinues = true;
	SetDlgItemText(IDC_BUTTON_START, _T("Stop Monitor"));
	SetTimer(TIMER_SCAN, m_unTimerSpeed, NULL);

	GetDlgItem(IDC_SPIN_SPEED)->EnableWindow(FALSE);
	GetDlgItem(IDC_EDIT1)->EnableWindow(FALSE);
	GetDlgItem(IDC_EDIT2)->EnableWindow(FALSE);
	GetDlgItem(IDC_CHECK_REALTIME)->EnableWindow(FALSE);
	GetDlgItem(IDC_SPIN_ACTIONSET)->EnableWindow(FALSE);
}

void CScanPixelsDlg::OnMove(int x, int y)
{
	CDialog::OnMove(x, y);

	m_nxSrc = x;
	m_nySrc = y;
}

BOOL CScanPixelsDlg::ComparePixels(int nX, int nY)
{
	if (nX < 0 || nX >= kErrorMapSize || nY < 0 || nY >= kErrorMapSize)
		return FALSE;

	BOOL bDiffers = FALSE;

	CClientDC ddcFrame   (GetDlgItem(IDC_STATIC_FLAMEPIXELS));
	CClientDC ddcDesktop (GetDesktopWindow());
	CClientDC dflag      (GetDlgItem(IDC_STATIC_FLAG));
	CClientDC dflagDetect(GetDlgItem(IDC_STATIC_FLAG3));

	const COLORREF cFrame   = ddcFrame.GetPixel(nX, nY);
	const COLORREF cLive    = ddcDesktop.GetPixel(
		(m_nxSrc + kCaptureOffsetX) + nX,
		(m_nySrc + kCaptureOffsetY) + nY);

	if (cFrame == cLive)
	{
		dflag.FillSolidRect(1, 1, kFlagW, kFlagH, RGB(244, 0, 0));

		if (m_Error[nX][nY] == (nX + nY))
		{
			m_ErrorDetected = kErrorNone;
			m_Error[nX][nY] = 0;
		}
	}
	else
	{
		dflag.FillSolidRect(1, 1, kFlagW, kFlagH, RGB(0, 0, 0));
		m_Error[nX][nY] = (nX + nY);
		m_ErrorDetected = kErrorFresh;
		bDiffers = TRUE;
	}

	dflagDetect.FillSolidRect(1, 1, kFlagW, kFlagH,
		m_ErrorDetected == kErrorNone ? RGB(24, 244, 24) : RGB(242, 23, 23));

	// Schedule a broadcast on the rising edge of an error...
	if (m_ErrorDetected == kErrorFresh && m_bSendOnRise)
	{
		SetTimer(TIMER_ACTIONSET, m_unTimerAction, NULL);
		m_bSendOnRise = false;
		m_bSendOnFall = true;
	}

	// ...and on the falling edge (back to OK).
	if (m_ErrorDetected == kErrorNone && m_bSendOnFall)
	{
		SetTimer(TIMER_ACTIONSET, m_unTimerAction, NULL);
		m_bSendOnRise = true;
		m_bSendOnFall = false;
	}

	return bDiffers;
}

void CScanPixelsDlg::DrawFlagWorking(bool bSet)
{
	CClientDC ddc(GetDlgItem(IDC_STATIC_FLAG2));
	ddc.FillSolidRect(1, 1, kFlagW, kFlagH,
		bSet ? RGB(244, 0, 0) : RGB(244, 244, 0));
}

void CScanPixelsDlg::OnTimer(UINT nIDEvent)
{
	if (m_ErrorDetected == kErrorFresh)
	{
		m_nScanX = kScanStartX;
		m_nScanY = kScanStartY;
		m_ErrorDetected = kErrorReported;
	}

	// Walk the scan area in a row-major sweep so every pixel is sampled,
	// not only the diagonal.
	m_nScanX++;
	if (m_nScanX >= m_rctClientFrame.Width() / 2)
	{
		m_nScanX = kScanStartX;
		m_nScanY++;
		DrawFlagWorking(true);

		if (m_nScanY >= m_rctClientFrame.Height())
			m_nScanY = kScanStartY;
	}
	else
	{
		DrawFlagWorking(false);
	}

	ComparePixels(m_nScanX, m_nScanY);

	if (nIDEvent == TIMER_ACTIONSET && m_ErrorDetected != kErrorReported)
	{
		::PostMessage(HWND_BROADCAST, m_MsgPixelScan,
			MAKELONG(m_unIDClient, m_ErrorDetected),
			MAKELONG(m_nScanX, m_nScanY));

		if (!m_chIsRealTime.GetCheck())
			KillTimer(TIMER_ACTIONSET);
	}

	CDialog::OnTimer(nIDEvent);
}

void CScanPixelsDlg::OnDeltaposSpinSpeed(NMHDR* pNMHDR, LRESULT* pResult)
{
	NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;

	CString strSpeed;
	strSpeed.Format(_T("%d"), pNMUpDown->iPos);
	SetDlgItemText(IDC_EDIT2, strSpeed);

	m_unTimerSpeed = pNMUpDown->iPos;

	*pResult = 0;
}

void CScanPixelsDlg::OnDeltaposSpinActionset(NMHDR* pNMHDR, LRESULT* pResult)
{
	NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;

	CString strSpeed;
	strSpeed.Format(_T("%d"), pNMUpDown->iPos);
	SetDlgItemText(IDC_EDIT1, strSpeed);

	m_unTimerAction = pNMUpDown->iPos;

	*pResult = 0;
}

void CScanPixelsDlg::PostNcDestroy()
{
	CDialog::PostNcDestroy();
}
