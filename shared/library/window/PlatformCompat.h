#pragma once

#ifdef _WIN32_WCE
	#define	FIRST_WM_MESSAGE		WM_CREATE
	#define	FINAL_WM_MESSAGE		WM_DESTROY

	#undef SetWindowLongPtr
	#undef GetWindowLongPtr

	#define	SetWindowLongPtr		SetWindowLong
	#define	GetWindowLongPtr		GetWindowLong

	#define	__int3264				long
#else
	#define	FIRST_WM_MESSAGE		WM_NCCREATE
	#define	FINAL_WM_MESSAGE		WM_NCDESTROY
#endif

#ifdef	_WIN64
	#define	GWL_WNDPROC				GWLP_WNDPROC
	#define	GWL_USERDATA			GWLP_USERDATA

	#define	DWL_USER				DWLP_USER
	#define	DWL_DLGPROC				DWLP_DLGPROC

	typedef INT_PTR					DLGRESULT;
#else
	typedef BOOL					DLGRESULT;
#endif
