// TestPixelProject.h : main header file for the TESTPIXELPROJECT application
//

#if !defined(AFX_TESTPIXELPROJECT_H__19401CA2_0D00_4AF9_9508_A4F08578D512__INCLUDED_)
#define AFX_TESTPIXELPROJECT_H__19401CA2_0D00_4AF9_9508_A4F08578D512__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

/////////////////////////////////////////////////////////////////////////////
// CTestPixelProjectApp:
// See TestPixelProject.cpp for the implementation of this class
//

class CTestPixelProjectApp : public CWinApp
{
public:
	CTestPixelProjectApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTestPixelProjectApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CTestPixelProjectApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TESTPIXELPROJECT_H__19401CA2_0D00_4AF9_9508_A4F08578D512__INCLUDED_)
