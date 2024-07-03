#pragma once

#include "Library\Core\BaseUnknown.h"
#include "Library\Window\BaseMDIFrame.h"
#include "Published\SIF.h"

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

class CImageChild :
	public CBaseUnknown,
	public CBaseMDIChild
{
protected:
	HINSTANCE m_hInstance;
	ISimbeyInterchangeFile* m_pSIF;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IOleWindow)
		UNK_INTERFACE(IBaseWindow)
	END_UNK_MAP

	BEGIN_WM_MAP
		HANDLE_WM(WM_PAINT, OnPaint)
		HANDLE_WM(WM_SIZE, OnSize)
		HANDLE_WM(WM_VSCROLL, OnVScroll)
		HANDLE_WM(WM_HSCROLL, OnHScroll)
		HANDLE_WM(WM_MOUSEWHEEL, OnMouseWheel)
		HANDLE_WM(WM_KEYDOWN, OnKeyDown)
		HANDLE_WM(WM_KEYUP, OnKeyUp)
		HANDLE_WM(WM_SYSKEYDOWN, OnSysKeyDown)
		HANDLE_WM(WM_SYSKEYUP, OnSysKeyUp)
		HANDLE_WM(WM_LBUTTONDOWN, OnLButtonDown)
		HANDLE_WM(WM_MOUSEMOVE, OnMouseMove)
		HANDLE_WM(WM_LBUTTONUP, OnLButtonUp)
	END_WM_MAP

public:
	CImageChild (HINSTANCE hInstance);
	~CImageChild ();

	static HRESULT Register (HINSTANCE hInstance);
	static HRESULT Unregister (HINSTANCE hInstance);

	HRESULT Initialize (CBaseMDIFrame* pFrame, INT nWidth, INT nHeight);
	HRESULT AddLayer (PCWSTR pcwzImageFile);

	INT m_nScreenWidth;
	INT m_nScreenHeight;
	float m_fZoom;
	const float m_fZoomStep;
	const float m_fMaxZoom;
	const float m_fMinZoom;
	INT m_xScrollPos;
	INT m_yScrollPos;
	WCHAR m_wzFileName[MAX_PATH];
	
	BOOL m_bLButtonClicked; 
	INT m_xStartDrag;
	INT m_yStartDrag;
	INT m_xCurrDrag;
	INT m_yCurrDrag;

	TStackRef<ISimbeyInterchangeFileLayer> m_srLayer;
	SIF_SURFACE m_oriSifSurface;
	RECT m_rcLayer;
	SIZE m_sLayer;

	BOOL m_bImageLoaded;

	HCURSOR m_hZoomInCursor;
	HCURSOR m_hDefaultCursor;
	HCURSOR m_hZoomOutCursor;
	BOOL m_bIsAltPressed;
	BOOL m_bIsCtrlPressed;

	BOOL m_bWindowsSizeChanged;
	HBITMAP m_hDIB;
	SIF_SURFACE m_sifSurface;

	void LoadCursors();
	void UpdateCursor();

	void _SetScrollSizes();
	void _SetScrollPos(HWND hWnd, int nBar, int pos);
	void ZoomToRectangle();
	void ZoomIn(int nStep, POINT center);
	void ZoomOut(int nStep, POINT center);
	void CheckZoomValue();
	void SetClientSize(HWND hwnd, int clientWidth, int clientHeight);

protected:
	virtual HINSTANCE GetInstance (VOID);

	HRESULT UpdateTitleWithZoom (VOID);

	DECL_WM_HANDLER(OnSize);
	DECL_WM_HANDLER(OnPaint);
	DECL_WM_HANDLER(OnVScroll);
	DECL_WM_HANDLER(OnHScroll);
	DECL_WM_HANDLER(OnMouseWheel);
	DECL_WM_HANDLER(OnKeyDown);
	DECL_WM_HANDLER(OnKeyUp);
	DECL_WM_HANDLER(OnSysKeyDown);
	DECL_WM_HANDLER(OnSysKeyUp);
	DECL_WM_HANDLER(OnLButtonDown);
	DECL_WM_HANDLER(OnMouseMove);
	DECL_WM_HANDLER(OnLButtonUp);
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

	HRESULT OpenImageWindow (VOID);

	DECL_WM_HANDLER(OnCreate);
	DECL_WM_HANDLER(OnPaint);
	DECL_WM_HANDLER(OnEraseBackground);
	DECL_WM_HANDLER(OnCommand);
	DECL_WM_HANDLER(OnClose);
};
