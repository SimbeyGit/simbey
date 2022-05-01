#pragma once

#include "Library\Core\BaseUnknown.h"
#include "Library\Window\BaseMDIFrame.h"

class CMDIChild :
	public CBaseUnknown,
	public CBaseMDIChild
{
protected:
	HINSTANCE m_hInstance;
	INT m_nType;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IOleWindow)
		UNK_INTERFACE(IBaseWindow)
	END_UNK_MAP

	BEGIN_WM_MAP
		HANDLE_WM(WM_PAINT, OnPaint)
	END_WM_MAP

public:
	CMDIChild (HINSTANCE hInstance, INT nType);
	~CMDIChild ();

	static HRESULT Register (HINSTANCE hInstance);
	static HRESULT Unregister (HINSTANCE hInstance);

	HRESULT Initialize (CBaseMDIFrame* pFrame, INT nWidth, INT nHeight);

protected:
	virtual HINSTANCE GetInstance (VOID);

	DECL_WM_HANDLER(OnPaint);
};

class CMDIWindow :
	public CBaseUnknown,
	public CBaseMDIFrame
{
protected:
	HINSTANCE m_hInstance;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IOleWindow)
		UNK_INTERFACE(IBaseWindow)
	END_UNK_MAP

	BEGIN_WM_MAP
		HANDLE_WM(WM_CREATE, OnCreate)
		HANDLE_WM(WM_PAINT, OnPaint)
		HANDLE_WM(WM_ERASEBKGND, OnEraseBackground)
		HANDLE_WM(WM_COMMAND, OnCommand)
		HANDLE_WM(WM_CLOSE, OnClose)
		DELEGATE_PARENT(CBaseMDIFrame)
	END_WM_MAP

public:
	CMDIWindow (HINSTANCE hInstance);
	virtual ~CMDIWindow ();

	static HRESULT Register (HINSTANCE hInstance);
	static HRESULT Unregister (HINSTANCE hInstance);

	HRESULT Initialize (INT nWidth, INT nHeight, INT nCmdShow);

protected:
	virtual HINSTANCE GetInstance (VOID);
	virtual VOID OnFinalDestroy (HWND hwnd);

	virtual HRESULT FinalizeAndShow (DWORD dwStyle, INT nCmdShow);

	virtual BOOL GetMDIMenuData (__out HMENU* phWindowMenu, __out UINT* pidFirstChild);

	DECL_WM_HANDLER(OnCreate);
	DECL_WM_HANDLER(OnPaint);
	DECL_WM_HANDLER(OnEraseBackground);
	DECL_WM_HANDLER(OnCommand);
	DECL_WM_HANDLER(OnClose);
};
