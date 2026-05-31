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

// Status flag indicators ("LED" rectangles) painted on the dialog.
const int kFlagW = 28;
const int kFlagH = 8;

// Default timer values (milliseconds).
const int kDefaultScanSpeedMs = 10;
const int kDefaultActionMs    = 1;

// Spin control range cap.
const int kSpinMax = 100000;

// Default overlay size on first launch.
const int kDefaultOverlayW = 144;
const int kDefaultOverlayH = 122;

// Broadcast state codes (kept compatible with prior listeners).
const int kBroadcastClean         = 0;
const int kBroadcastError         = 1;
const int kBroadcastShapeChange   = 2;  // dHash detected an image-level shape change
const int kBroadcastShapeRestored = 3;  // dHash matches the reference again

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
	, m_MsgPixelScan(0)
	, m_pendingState(kBroadcastClean)
	, m_pendingX(0)
	, m_pendingY(0)
	, m_bActionPending(false)
{
	//{{AFX_DATA_INIT(CScanPixelsDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_MsgPixelScan = ::RegisterWindowMessage(_T("WHM_SCANPXL"));
}

void CScanPixelsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CScanPixelsDlg)
	DDX_Control(pDX, IDC_CHECK_REALTIME, m_chIsRealTime);
	DDX_Control(pDX, IDC_CHECK_IGNORETEXT, m_chIgnoreText);
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
	ON_WM_TIMER()
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_SPEED, OnDeltaposSpinSpeed)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_ACTIONSET, OnDeltaposSpinActionset)
	ON_BN_CLICKED(IDC_CHECK_IGNORETEXT, OnCheckIgnoreText)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CScanPixelsDlg message handlers

BOOL CScanPixelsDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

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

	// Position the green target overlay near the centre of the primary monitor.
	int sw = ::GetSystemMetrics(SM_CXSCREEN);
	int sh = ::GetSystemMetrics(SM_CYSCREEN);
	CRect overlay(
		(sw - kDefaultOverlayW) / 2,
		(sh - kDefaultOverlayH) / 2,
		(sw + kDefaultOverlayW) / 2,
		(sh + kDefaultOverlayH) / 2);
	m_overlay.Create(overlay, this);

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

void CScanPixelsDlg::OnButtonStart()
{
	if (m_bContinues)
	{
		// Currently scanning -> stop.
		KillTimer(TIMER_SCAN);
		KillTimer(TIMER_ACTIONSET);
		m_bContinues = false;
		m_bActionPending = false;
		m_overlay.SetLocked(false);
		SetDlgItemText(IDC_BUTTON_START, _T("Start Monitor"));

		GetDlgItem(IDC_SPIN_SPEED)->EnableWindow(TRUE);
		GetDlgItem(IDC_EDIT1)->EnableWindow(TRUE);
		GetDlgItem(IDC_EDIT2)->EnableWindow(TRUE);
		GetDlgItem(IDC_CHECK_REALTIME)->EnableWindow(TRUE);
		GetDlgItem(IDC_CHECK_IGNORETEXT)->EnableWindow(TRUE);
		GetDlgItem(IDC_SPIN_ACTIONSET)->EnableWindow(TRUE);
		return;
	}

	// Currently stopped -> snapshot whatever is under the overlay and start.
	m_engine.SetIgnoreText(m_chIgnoreText.GetCheck() == BST_CHECKED);
	m_engine.CaptureReference(m_overlay.GetScreenRect());
	m_overlay.SetLocked(true);
	DrawFlagState(true);

	m_bContinues = true;
	m_bActionPending = false;
	SetDlgItemText(IDC_BUTTON_START, _T("Stop Monitor"));
	SetTimer(TIMER_SCAN, m_unTimerSpeed, NULL);

	GetDlgItem(IDC_SPIN_SPEED)->EnableWindow(FALSE);
	GetDlgItem(IDC_EDIT1)->EnableWindow(FALSE);
	GetDlgItem(IDC_EDIT2)->EnableWindow(FALSE);
	GetDlgItem(IDC_CHECK_REALTIME)->EnableWindow(FALSE);
	// Locked during a scan: the ignore-text mode is baked into the engine at
	// CaptureReference time (it allocates learning buffers + seeds prev-luma).
	// Toggling mid-scan would mutate m_ignoreText against half-initialised
	// state, so the checkbox is frozen until Stop.
	GetDlgItem(IDC_CHECK_IGNORETEXT)->EnableWindow(FALSE);
	GetDlgItem(IDC_SPIN_ACTIONSET)->EnableWindow(FALSE);
}

void CScanPixelsDlg::OnTimer(UINT nIDEvent)
{
	if (nIDEvent == TIMER_SCAN)
	{
		CPixelEngine::StepResult r = m_engine.Step(m_overlay.GetScreenRect());

		// Flag follows whichever channel is actually broadcasting: per-pixel
		// when Ignore-text is OFF, the shape (bulk-percent) channel when ON.
		// Otherwise a stuck per-pixel state would leave the flag red after
		// the watched region had already returned to matching the reference.
		const bool ignoreText = (m_chIgnoreText.GetCheck() == BST_CHECKED);
		const bool clean = ignoreText
			? (r.shapeState == 0)
			: (r.state == CPixelEngine::kClean);
		DrawFlagMatch(clean);
		DrawFlagWorking(r.rowWrapped);
		DrawFlagState(clean);

		// When "Ignore small/text changes" is checked, the per-pixel detector
		// is silent and only the shape channel (below) speaks. This trades
		// per-pixel precision for immunity to small/text-sized changes.
		if (!ignoreText && r.edge != CPixelEngine::kEdgeNone)
		{
			m_pendingState = clean ? kBroadcastClean : kBroadcastError;
			m_pendingX = r.x;
			m_pendingY = r.y;
			m_bActionPending = true;
			SetTimer(TIMER_ACTIONSET, m_unTimerAction, NULL);
		}

		// Parallel dHash channel. If both fire on the same tick, the shape
		// event wins the broadcast — that's the higher-level signal.
		if (r.shapeEdge != CPixelEngine::kEdgeNone)
		{
			m_pendingState = (r.shapeEdge == CPixelEngine::kEdgeRising)
				? kBroadcastShapeChange
				: kBroadcastShapeRestored;
			m_pendingX = r.x;
			m_pendingY = r.y;
			m_bActionPending = true;
			SetTimer(TIMER_ACTIONSET, m_unTimerAction, NULL);
		}
	}
	else if (nIDEvent == TIMER_ACTIONSET)
	{
		if (m_bActionPending)
		{
			::PostMessage(HWND_BROADCAST, m_MsgPixelScan,
				MAKELONG(m_unIDClient, m_pendingState),
				MAKELONG(m_pendingX, m_pendingY));
			m_bActionPending = false;
		}

		if (!m_chIsRealTime.GetCheck())
			KillTimer(TIMER_ACTIONSET);
	}

	CDialog::OnTimer(nIDEvent);
}

void CScanPixelsDlg::DrawFlagMatch(bool bMatch)
{
	CClientDC ddc(GetDlgItem(IDC_STATIC_FLAG));
	ddc.FillSolidRect(1, 1, kFlagW, kFlagH,
		bMatch ? RGB(244, 0, 0) : RGB(0, 0, 0));
}

void CScanPixelsDlg::DrawFlagWorking(bool bSet)
{
	CClientDC ddc(GetDlgItem(IDC_STATIC_FLAG2));
	ddc.FillSolidRect(1, 1, kFlagW, kFlagH,
		bSet ? RGB(244, 0, 0) : RGB(244, 244, 0));
}

void CScanPixelsDlg::DrawFlagState(bool bClean)
{
	CClientDC ddc(GetDlgItem(IDC_STATIC_FLAG3));
	ddc.FillSolidRect(1, 1, kFlagW, kFlagH,
		bClean ? RGB(24, 244, 24) : RGB(242, 23, 23));
}

void CScanPixelsDlg::OnDeltaposSpinSpeed(NMHDR* pNMHDR, LRESULT* pResult)
{
	NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;

	// UDN_DELTAPOS fires before the change; iPos is current, iDelta is pending.
	int newPos = pNMUpDown->iPos + pNMUpDown->iDelta;
	if (newPos < 0)         newPos = 0;
	if (newPos > kSpinMax)  newPos = kSpinMax;

	CString strSpeed;
	strSpeed.Format(_T("%d"), newPos);
	SetDlgItemText(IDC_EDIT2, strSpeed);

	m_unTimerSpeed = newPos;

	*pResult = 0;
}

void CScanPixelsDlg::OnCheckIgnoreText()
{
	m_engine.SetIgnoreText(m_chIgnoreText.GetCheck() == BST_CHECKED);
}

void CScanPixelsDlg::OnDeltaposSpinActionset(NMHDR* pNMHDR, LRESULT* pResult)
{
	NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;

	int newPos = pNMUpDown->iPos + pNMUpDown->iDelta;
	if (newPos < 0)         newPos = 0;
	if (newPos > kSpinMax)  newPos = kSpinMax;

	CString strSpeed;
	strSpeed.Format(_T("%d"), newPos);
	SetDlgItemText(IDC_EDIT1, strSpeed);

	m_unTimerAction = newPos;

	*pResult = 0;
}

void CScanPixelsDlg::PostNcDestroy()
{
	CDialog::PostNcDestroy();
}
