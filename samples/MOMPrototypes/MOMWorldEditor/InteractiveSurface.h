#pragma once

#include "Surface\SIFSurface.h"

class CInteractiveCanvas;

namespace LayerInput
{
	enum Mouse
	{
		NotMouse,
		Move,
		LButtonDown,
		LButtonUp,
		RButtonDown,
		RButtonUp
	};

	enum Keyboard
	{
		NotKeyboard,
		KeyDown,
		KeyUp,
		Char
	};
}

interface __declspec(uuid("FF57381D-1E35-433a-A66A-B15D07097329")) ILayerInputHandler : IUnknown
{
	virtual BOOL ProcessMouseInput (LayerInput::Mouse eType, WPARAM wParam, LPARAM lParam, INT xView, INT yView, LRESULT& lResult) = 0;
	virtual BOOL ProcessKeyboardInput (LayerInput::Keyboard eType, WPARAM wParam, LPARAM lParam, LRESULT& lResult) = 0;
};

class CInteractiveLayer : public CBaseUnknown
{
protected:
	ILayerInputHandler* m_pHandler;
	CInteractiveCanvas* m_pCanvas;
	sysint m_nLayer;

public:
	IMP_BASE_UNKNOWN

	EMPTY_UNK_MAP

public:
	CInteractiveLayer (ILayerInputHandler* pHandler, CInteractiveCanvas* pCanvas, sysint nLayer);
	~CInteractiveLayer ();

	BOOL ProcessMouseInput (LayerInput::Mouse eType, WPARAM wParam, LPARAM lParam, INT xView, INT yView, LRESULT& lResult);
	BOOL ProcessKeyboardInput (LayerInput::Keyboard eType, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

	inline sysint GetLayer (VOID) { return m_nLayer; }
	VOID Clear (VOID);
	HRESULT AddSprite (ISimbeyInterchangeSprite* pSprite, __out_opt sysint* pnSprite);
	HRESULT AddSpriteAfter (ISimbeyInterchangeSprite* pSprite, ISimbeyInterchangeSprite* pInsertAfter, __out_opt sysint* pnSprite);
	HRESULT RemoveSprite (ISimbeyInterchangeSprite* pSprite);
	HRESULT FindSprite (ISimbeyInterchangeSprite* pSprite, __out_opt sysint* pnSprite);
	HRESULT BringSpriteToTop (ISimbeyInterchangeSprite* pSprite);

	HRESULT Sort (INT (WINAPI* pfnCallback)(ISimbeyInterchangeSprite** ppSpriteA, ISimbeyInterchangeSprite** ppSpriteB, PVOID pParam), PVOID pvParam);
};

class CInteractiveCanvas :
	public CBaseUnknown,
	public CSIFCanvas
{
protected:
	TArray<CInteractiveLayer*> m_aInteractive;

public:
	IMP_BASE_UNKNOWN

	EMPTY_UNK_MAP

public:
	CInteractiveCanvas ();
	~CInteractiveCanvas ();

	BOOL ProcessMouseInput (LayerInput::Mouse eType, WPARAM wParam, LPARAM lParam, INT xView, INT yView, LRESULT& lResult);
	BOOL ProcessKeyboardInput (LayerInput::Keyboard eType, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

	HRESULT AddInteractiveLayer (BOOL fPerformTicks, BOOL fColorized, COLORREF crFill, ILayerInputHandler* pHandler, __deref_out CInteractiveLayer** ppLayer);
	VOID UnlinkInteractiveLayers (VOID);
};

class CInteractiveSurface :
	public CBaseUnknown,
	public CSIFSurface
{
protected:
	CInteractiveCanvas* m_pCapture;

public:
	IMP_BASE_UNKNOWN

	EMPTY_UNK_MAP

public:
	CInteractiveSurface (INT xSize, INT ySize);
	~CInteractiveSurface ();

	BOOL ProcessLayerInput (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

	VOID CaptureCanvas (CInteractiveCanvas* pCanvas);
	VOID ReleaseCapture (VOID);

protected:
	virtual HRESULT AllocCanvas (__deref_out CSIFCanvas** ppCanvas);
	virtual VOID FreeCanvas (CSIFCanvas* pCanvas);
};
