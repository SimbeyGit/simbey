#pragma once

#include "Library\Core\BaseUnknown.h"
#include "Library\Window\BaseWindow.h"
#include "Surface\SIFSurface.h"

class CLargeCombatRenderer :
	public CBaseUnknown,
	public CBaseWindow
{
protected:
	HINSTANCE m_hInstance;

	CSIFSurface m_Surface;
	CSIFCanvas* m_pMain;
	CIsometricTranslator m_Isometric;

	BOOL m_fActive;

	bool m_fKeys[256];

	ISimbeyInterchangeFile* m_pSIF;
	ISimbeyInterchangeFileLayer* m_pTile;

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
		HANDLE_WM(WM_SIZE, OnSize)
		HANDLE_WM(WM_CLOSE, OnClose)
		HANDLE_WM(WM_ACTIVATE, OnActivate)
		HANDLE_WM(WM_MOUSEMOVE, OnMouseMove)
		HANDLE_WM(WM_LBUTTONDOWN, OnLButtonDown)
		HANDLE_WM(WM_LBUTTONUP, OnLButtonUp)
		HANDLE_WM(WM_RBUTTONDOWN, OnRButtonDown)
		HANDLE_WM(WM_KEYDOWN, OnKeyDown)
		HANDLE_WM(WM_KEYUP, OnKeyUp)
		HANDLE_WM(WM_SETCURSOR, OnSetCursor)
	END_WM_MAP

	CLargeCombatRenderer (HINSTANCE hInstance);
	virtual ~CLargeCombatRenderer ();

	static HRESULT Register (HINSTANCE hInstance);
	static HRESULT Unregister (HINSTANCE hInstance);

	HRESULT Initialize (INT nWidth, INT nHeight, INT nCmdShow);

	VOID Run (VOID);

protected:
	virtual HINSTANCE GetInstance (VOID);
	virtual VOID OnFinalDestroy (HWND hwnd);

	virtual HRESULT FinalizeAndShow (DWORD dwStyle, INT nCmdShow);

	DECL_WM_HANDLER(OnCreate);
	DECL_WM_HANDLER(OnPaint);
	DECL_WM_HANDLER(OnEraseBackground);
	DECL_WM_HANDLER(OnSize);
	DECL_WM_HANDLER(OnClose);
	DECL_WM_HANDLER(OnActivate);
	DECL_WM_HANDLER(OnMouseMove);
	DECL_WM_HANDLER(OnLButtonDown);
	DECL_WM_HANDLER(OnLButtonUp);
	DECL_WM_HANDLER(OnRButtonDown);
	DECL_WM_HANDLER(OnKeyDown);
	DECL_WM_HANDLER(OnKeyUp);
	DECL_WM_HANDLER(OnSetCursor);
};
