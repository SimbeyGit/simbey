#pragma once

#include "Library\Core\Array.h"
#include "Library\Core\BaseUnknown.h"
#include "Library\Util\RString.h"
#include "..\Published\SIF.h"

namespace LayerRender
{
	enum Type
	{
		Masked,
		Tile,
		Blended,
		Colorized
	};
}

struct LAYER
{
	BOOL fPerformTicks;
	LayerRender::Type eRender;
	COLORREF crFill;
	TArray<ISimbeyInterchangeSprite*> aSprites;
	SIF_LINE_OFFSET* pslOffsets;
};

struct REMOVE_LATER
{
	ISimbeyInterchangeSprite* pSprite;
	sysint nLayer;
};

class CDrawText :
	public CBaseUnknown,
	public ISimbeyInterchangeSprite
{
private:
	ISimbeyInterchangeFileFont* m_pFont;
	RSTRING m_rstrText;
	BOOL m_fCenter;
	INT m_x, m_y;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(ISimbeyInterchangeSprite)
	END_UNK_MAP

	CDrawText (ISimbeyInterchangeFileFont* pFont, RSTRING rstrText, BOOL fCenter);
	~CDrawText ();

	// ISimbeyInterchangeSprite
	IFACEMETHOD(SelectAnimation) (INT nAnimation, INT nFrame, INT cTicks);
	IFACEMETHOD_(VOID, GetCurrentAnimation) (__out INT* pnAnimation, __out INT* pnFrame, __out INT* pcTicks);
	IFACEMETHOD_(VOID, GetCurrentFrameSize) (__out INT* pxSize, __out INT* pySize);
	IFACEMETHOD_(VOID, GetCurrentHitBox) (__out RECT* prcHitBox);

	IFACEMETHOD_(VOID, UpdateFrameTick) (VOID);
	IFACEMETHOD_(VOID, SetPosition) (INT x, INT y);
	IFACEMETHOD_(VOID, GetPosition) (__out INT& x, __out INT& y);

	IFACEMETHOD_(BOOL, DrawMaskedToDIB24) (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24);
	IFACEMETHOD_(BOOL, DrawTileToDIB24) (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24, SIF_LINE_OFFSET* pslOffsets);
	IFACEMETHOD_(BOOL, DrawBlendedToDIB24) (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24);
	IFACEMETHOD_(BOOL, DrawColorizedToDIB24) (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24);

	IFACEMETHOD_(COLORREF, GetColorized) (VOID);
	IFACEMETHOD_(BOOL, SetColorized) (COLORREF cr);

	IFACEMETHOD(Clone) (__deref_out ISimbeyInterchangeSprite** ppSprite) { return E_NOTIMPL; }
	IFACEMETHOD_(VOID, GetFrameOffset) (__out INT& x, __out INT& y);
	IFACEMETHOD(GetFrameImage) (__out PBYTE* ppBits32P, __out INT* pnWidth, __out INT* pnHeight) { return E_NOTIMPL; }
	IFACEMETHOD(SetAnimationCompletedCallback) (__in_opt ISpriteAnimationCompleted* pCallback) { return E_NOTIMPL; }
	IFACEMETHOD(GetAnimator) (__deref_out ISimbeyInterchangeAnimator** ppAnimator) { return E_NOTIMPL; }
};

class CDrawSolid :
	public CBaseUnknown,
	public ISimbeyInterchangeSprite
{
private:
	COLORREF m_crSolid;
	RECT m_rcSolid;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(ISimbeyInterchangeSprite)
	END_UNK_MAP

	CDrawSolid (COLORREF crSolid, INT nWidth, INT nHeight);
	~CDrawSolid ();

	// ISimbeyInterchangeSprite
	IFACEMETHOD(SelectAnimation) (INT nAnimation, INT nFrame, INT cTicks);
	IFACEMETHOD_(VOID, GetCurrentAnimation) (__out INT* pnAnimation, __out INT* pnFrame, __out INT* pcTicks);
	IFACEMETHOD_(VOID, GetCurrentFrameSize) (__out INT* pxSize, __out INT* pySize);
	IFACEMETHOD_(VOID, GetCurrentHitBox) (__out RECT* prcHitBox);

	IFACEMETHOD_(VOID, UpdateFrameTick) (VOID);
	IFACEMETHOD_(VOID, SetPosition) (INT x, INT y);
	IFACEMETHOD_(VOID, GetPosition) (__out INT& x, __out INT& y);

	IFACEMETHOD_(BOOL, DrawMaskedToDIB24) (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24);
	IFACEMETHOD_(BOOL, DrawTileToDIB24) (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24, SIF_LINE_OFFSET* pslOffsets);
	IFACEMETHOD_(BOOL, DrawBlendedToDIB24) (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24);
	IFACEMETHOD_(BOOL, DrawColorizedToDIB24) (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24);

	IFACEMETHOD_(COLORREF, GetColorized) (VOID);
	IFACEMETHOD_(BOOL, SetColorized) (COLORREF cr);

	IFACEMETHOD(Clone) (__deref_out ISimbeyInterchangeSprite** ppSprite) { return E_NOTIMPL; }
	IFACEMETHOD_(VOID, GetFrameOffset) (__out INT& x, __out INT& y);
	IFACEMETHOD(GetFrameImage) (__out PBYTE* ppBits32P, __out INT* pnWidth, __out INT* pnHeight) { return E_NOTIMPL; }
	IFACEMETHOD(SetAnimationCompletedCallback) (__in_opt ISpriteAnimationCompleted* pCallback) { return E_NOTIMPL; }
	IFACEMETHOD(GetAnimator) (__deref_out ISimbeyInterchangeAnimator** ppAnimator) { return E_NOTIMPL; }

	COLORREF GetSolidColor (VOID);
	VOID SetSolidColor (COLORREF cr);
};

class CDrawPattern :
	public CBaseUnknown,
	public ISimbeyInterchangeSprite
{
private:
	ISimbeyInterchangeFileLayer* m_pLayer;
	PBYTE m_pbTile;
	INT m_x, m_y, m_nWidth, m_nHeight;
	COLORREF m_crColorize;
	BOOL m_fPinned;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(ISimbeyInterchangeSprite)
	END_UNK_MAP

	CDrawPattern (ISimbeyInterchangeFileLayer* pLayer, INT nWidth, INT nHeight, BOOL fPinned);
	~CDrawPattern ();

	HRESULT Initialize (VOID);

	// ISimbeyInterchangeSprite
	IFACEMETHOD(SelectAnimation) (INT nAnimation, INT nFrame, INT cTicks);
	IFACEMETHOD_(VOID, GetCurrentAnimation) (__out INT* pnAnimation, __out INT* pnFrame, __out INT* pcTicks);
	IFACEMETHOD_(VOID, GetCurrentFrameSize) (__out INT* pxSize, __out INT* pySize);
	IFACEMETHOD_(VOID, GetCurrentHitBox) (__out RECT* prcHitBox);

	IFACEMETHOD_(VOID, UpdateFrameTick) (VOID);
	IFACEMETHOD_(VOID, SetPosition) (INT x, INT y);
	IFACEMETHOD_(VOID, GetPosition) (__out INT& x, __out INT& y);

	IFACEMETHOD_(BOOL, DrawMaskedToDIB24) (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24);
	IFACEMETHOD_(BOOL, DrawTileToDIB24) (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24, SIF_LINE_OFFSET* pslOffsets);
	IFACEMETHOD_(BOOL, DrawBlendedToDIB24) (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24);
	IFACEMETHOD_(BOOL, DrawColorizedToDIB24) (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24);

	IFACEMETHOD_(COLORREF, GetColorized) (VOID);
	IFACEMETHOD_(BOOL, SetColorized) (COLORREF cr);

	IFACEMETHOD(Clone) (__deref_out ISimbeyInterchangeSprite** ppSprite) { return E_NOTIMPL; }
	IFACEMETHOD_(VOID, GetFrameOffset) (__out INT& x, __out INT& y);
	IFACEMETHOD(GetFrameImage) (__out PBYTE* ppBits32P, __out INT* pnWidth, __out INT* pnHeight) { return E_NOTIMPL; }
	IFACEMETHOD(SetAnimationCompletedCallback) (__in_opt ISpriteAnimationCompleted* pCallback) { return E_NOTIMPL; }
	IFACEMETHOD(GetAnimator) (__deref_out ISimbeyInterchangeAnimator** ppAnimator) { return E_NOTIMPL; }

private:
	VOID CalculateView (SIF_SURFACE* psifSurface24, __out PBYTE* ppbView, INT& xOffset, INT& yOffset, INT& xView, INT& yView, INT& xTile, INT& yTile);
};

class CSIFCanvas
{
protected:
	SIF_SURFACE m_sifSurface;

	INT m_xScroll, m_yScroll;

	TArray<LAYER*> m_aLayers;
	TArray<REMOVE_LATER> m_aRemove;

public:
	CSIFCanvas ();
	~CSIFCanvas ();

	VOID Configure (const SIF_SURFACE* pcsifSurface);
	VOID Redraw (VOID);

	VOID SetScroll (INT xScroll, INT yScroll);
	VOID GetScroll (__out INT* pxScroll, __out INT* pyScroll);
	VOID OffsetPoint (__inout POINT& pt);
	VOID GetViewSize (__out INT* pxView, __out INT* pyView);

	VOID Tick (VOID);

	HRESULT AddLayer (BOOL fPerformTicks, LayerRender::Type eRender, COLORREF crFill, __out_opt sysint* pnLayer);
	HRESULT AddTileLayer (BOOL fPerformTicks, SIF_LINE_OFFSET* pslOffsets, COLORREF crFill, __out_opt sysint* pnLayer);
	VOID ClearLayer (sysint nLayer);
	HRESULT AddSprite (sysint nLayer, ISimbeyInterchangeSprite* pSprite, __out_opt sysint* pnSprite);
	HRESULT AddSpriteBefore (sysint nLayer, ISimbeyInterchangeSprite* pSprite, ISimbeyInterchangeSprite* pInsertBefore, __out_opt sysint* pnSprite);
	HRESULT AddSpriteAfter (sysint nLayer, ISimbeyInterchangeSprite* pSprite, __in_opt ISimbeyInterchangeSprite* pInsertAfter, __out_opt sysint* pnSprite);
	HRESULT RemoveSprite (sysint nLayer, ISimbeyInterchangeSprite* pSprite);
	HRESULT RemoveSpriteLater (sysint nLayer, ISimbeyInterchangeSprite* pSprite);
	HRESULT FindSprite (sysint nLayer, ISimbeyInterchangeSprite* pSprite, __out_opt sysint* pnSprite);
	HRESULT FindSpriteAt (sysint nLayer, INT x, INT y, __deref_out ISimbeyInterchangeSprite** ppSprite);
	HRESULT BringSpriteToTop (sysint nLayer, ISimbeyInterchangeSprite* pSprite);
	HRESULT RemoveHiddenSprites (sysint nLayer);
	HRESULT ReplaceSprite (sysint nLayer, ISimbeyInterchangeSprite* pReplace, ISimbeyInterchangeSprite* pSprite);

	HRESULT SortLayer (sysint nLayer, INT (WINAPI* pfnCallback)(ISimbeyInterchangeSprite** ppSpriteA, ISimbeyInterchangeSprite** ppSpriteB, PVOID pParam), PVOID pvParam);

protected:
	VOID DrawLayer (LAYER* pLayer);
};

struct CANVAS_DESC
{
	RECT rcCanvas;
	BOOL fInteractive;
	CSIFCanvas* pCanvas;
};

class CSIFSurface
{
protected:
	INT m_xSurface, m_ySurface;
	INT m_xStretchSize, m_yStretchSize;
	INT m_yPosition;

	PVOID m_pvBits;
	HBITMAP m_hbmBuffer;
	BITMAPINFO m_bmi;

	BOOL m_fClear;
	COLORREF m_crClear;

	RECT m_rcUnpainted[2];
	INT m_cUnpainted;

	TArray<CANVAS_DESC> m_aCanvas;

public:
	CSIFSurface (INT xSize, INT ySize);
	virtual ~CSIFSurface ();

	VOID EnableClear (COLORREF crClear);
	VOID DisableClear (VOID);

	VOID SetOffset (INT yPosition);
	VOID Position (INT xWin, INT yWin);
	VOID Tick (VOID);
	VOID Redraw (HWND hwnd, __in_opt HDC hdc);
	VOID Destroy (VOID);

	VOID GetViewSize (__out INT* pxView, __out INT* pyView);
	VOID GetUnpaintedRects (__deref_out const RECT** pprcUnpainted, __out INT* pcUnpainted);
	VOID TranslateClientPointToView (INT x, INT y, __deref_out_opt CSIFCanvas** ppCanvas, __out INT* pxView, __out INT* pyView);
	VOID TranslateClientPointToCanvas (INT x, INT y, CSIFCanvas* pCanvas, __out INT* pxView, __out INT* pyView);

	HRESULT AddCanvas (__in_opt const RECT* pcrcCanvas, BOOL fInteractive, __deref_out CSIFCanvas** ppCanvas);
	HRESULT MoveCanvasToTop (CSIFCanvas* pCanvas);
	HRESULT RemoveCanvas (CSIFCanvas* pCanvas);

	DOUBLE GetStretchScale (VOID);

protected:
	HRESULT BuildSurface (HDC hdc);
	VOID ReconfigureAll (VOID);

	virtual HRESULT AllocCanvas (__deref_out CSIFCanvas** ppCanvas);
	virtual VOID FreeCanvas (CSIFCanvas* pCanvas);
};

class CIsometricTranslator
{
private:
	INT m_xTile, m_xHalf;
	INT m_yTile, m_yHalf;

public:
	CIsometricTranslator (INT xTile, INT yTile);
	~CIsometricTranslator () {}

	VOID ViewToIsometric (CSIFCanvas* pCanvas, INT xView, INT yView, __out INT* pxIso, __out INT* pyIso);
	VOID IsometricToView (CSIFCanvas* pCanvas, INT xIso, INT yIso, __out INT* pxView, __out INT* pyView);

	VOID TileToView (int x, int y, __out int* pxView, __out int* pyView);
	VOID ViewToTile (int x, int y, __out int* pxTile, __out int* pyTile);

	VOID GetTileRange (CSIFCanvas* pCanvas, __out INT* pxTileStart, __out INT* pyTileStart, __out INT* pxTileEnd, __out INT* pyTileEnd);

	HRESULT SortIsometricLayer (CSIFCanvas* pCanvas, sysint nLayer);

private:
	static INT WINAPI SortIsometric (ISimbeyInterchangeSprite** ppSpriteA, ISimbeyInterchangeSprite** ppSpriteB, PVOID pvParam);
};
