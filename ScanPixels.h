// ScanPixels.h : main header file for the SCANPIXELS application
//

#if !defined(AFX_SCANPIXELS_H__403708DA_D863_4C51_AB7A_2F3382A3D67D__INCLUDED_)
#define AFX_SCANPIXELS_H__403708DA_D863_4C51_AB7A_2F3382A3D67D__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

/////////////////////////////////////////////////////////////////////////////
// CScanPixelsApp:
// See ScanPixels.cpp for the implementation of this class
//

class CScanPixelsApp : public CWinApp
{
public:
	CScanPixelsApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CScanPixelsApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CScanPixelsApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SCANPIXELS_H__403708DA_D863_4C51_AB7A_2F3382A3D67D__INCLUDED_)
