#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "InteractiveSurface.h"

///////////////////////////////////////////////////////////////////////////////
// CInteractiveLayer
///////////////////////////////////////////////////////////////////////////////

CInteractiveLayer::CInteractiveLayer (ILayerInputHandler* pHandler, CInteractiveCanvas* pCanvas, sysint nLayer) :
	m_pHandler(pHandler),
	m_pCanvas(pCanvas),
	m_nLayer(nLayer)
{
	m_pCanvas->AddRef();
	m_pHandler->AddRef();
}

CInteractiveLayer::~CInteractiveLayer ()
{
	m_pHandler->Release();
	m_pCanvas->Release();
}

BOOL CInteractiveLayer::ProcessMouseInput (LayerInput::Mouse eType, WPARAM wParam, LPARAM lParam, INT xView, INT yView, LRESULT& lResult)
{
	return m_pHandler->ProcessMouseInput(eType, wParam, lParam, xView, yView, lResult);
}

BOOL CInteractiveLayer::ProcessKeyboardInput (LayerInput::Keyboard eType, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return m_pHandler->ProcessKeyboardInput(eType, wParam, lParam, lResult);
}

VOID CInteractiveLayer::Clear (VOID)
{
	m_pCanvas->ClearLayer(m_nLayer);
}

HRESULT CInteractiveLayer::AddSprite (ISimbeyInterchangeSprite* pSprite, __out_opt sysint* pnSprite)
{
	return m_pCanvas->AddSprite(m_nLayer, pSprite, pnSprite);
}

HRESULT CInteractiveLayer::AddSpriteAfter (ISimbeyInterchangeSprite* pSprite, ISimbeyInterchangeSprite* pInsertAfter, __out_opt sysint* pnSprite)
{
	return m_pCanvas->AddSpriteAfter(m_nLayer, pSprite, pInsertAfter, pnSprite);
}

HRESULT CInteractiveLayer::RemoveSprite (ISimbeyInterchangeSprite* pSprite)
{
	return m_pCanvas->RemoveSprite(m_nLayer, pSprite);
}

HRESULT CInteractiveLayer::FindSprite (ISimbeyInterchangeSprite* pSprite, __out_opt sysint* pnSprite)
{
	return m_pCanvas->FindSprite(m_nLayer, pSprite, pnSprite);
}

HRESULT CInteractiveLayer::BringSpriteToTop (ISimbeyInterchangeSprite* pSprite)
{
	return m_pCanvas->BringSpriteToTop(m_nLayer, pSprite);
}

HRESULT CInteractiveLayer::Sort (INT (WINAPI* pfnCallback)(ISimbeyInterchangeSprite** ppSpriteA, ISimbeyInterchangeSprite** ppSpriteB, PVOID pParam), PVOID pvParam)
{
	return m_pCanvas->SortLayer(m_nLayer, pfnCallback, pvParam);
}

///////////////////////////////////////////////////////////////////////////////
// CInteractiveCanvas
///////////////////////////////////////////////////////////////////////////////

CInteractiveCanvas::CInteractiveCanvas ()
{
}

CInteractiveCanvas::~CInteractiveCanvas ()
{
	Assert(0 == m_aInteractive.Length());
}

BOOL CInteractiveCanvas::ProcessMouseInput (LayerInput::Mouse eType, WPARAM wParam, LPARAM lParam, INT xView, INT yView, LRESULT& lResult)
{
	for(sysint i = m_aInteractive.Length() - 1; i >= 0; i--)
	{
		if(m_aInteractive[i]->ProcessMouseInput(eType, wParam, lParam, xView, yView, lResult))
			return TRUE;
	}
	return FALSE;
}

BOOL CInteractiveCanvas::ProcessKeyboardInput (LayerInput::Keyboard eType, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	for(sysint i = m_aInteractive.Length() - 1; i >= 0; i--)
	{
		if(m_aInteractive[i]->ProcessKeyboardInput(eType, wParam, lParam, lResult))
			return TRUE;
	}
	return FALSE;
}

HRESULT CInteractiveCanvas::AddInteractiveLayer (BOOL fPerformTicks, LayerRender::Type eRender, COLORREF crFill, ILayerInputHandler* pHandler, __deref_out CInteractiveLayer** ppLayer)
{
	HRESULT hr;
	sysint nLayer;
	CInteractiveLayer* pLayer = NULL;

	Check(AddLayer(fPerformTicks, eRender, crFill, &nLayer));
	pLayer = __new CInteractiveLayer(pHandler, this, nLayer);
	CheckAlloc(pLayer);
	Check(m_aInteractive.Append(pLayer));

	if(ppLayer)
		SetInterface(*ppLayer, pLayer);
	pLayer = NULL;

Cleanup:
	SafeRelease(pLayer);
	return hr;
}

VOID CInteractiveCanvas::UnlinkInteractiveLayers (VOID)
{
	for(sysint i = m_aInteractive.Length() - 1; i >= 0; i--)
		m_aInteractive[i]->Release();
	m_aInteractive.Clear();
}

///////////////////////////////////////////////////////////////////////////////
// CInteractiveSurface
///////////////////////////////////////////////////////////////////////////////

CInteractiveSurface::CInteractiveSurface (INT xSize, INT ySize) :
	CSIFSurface(xSize, ySize),
	m_pCapture(NULL)
{
}

CInteractiveSurface::~CInteractiveSurface ()
{
	ReleaseCapture();
	Destroy();
}

BOOL CInteractiveSurface::ProcessLayerInput (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	LayerInput::Mouse eMouse = LayerInput::NotMouse;
	LayerInput::Keyboard eKeyboard = LayerInput::NotKeyboard;

	switch(uMsg)
	{
	case WM_MOUSEMOVE:
		eMouse = LayerInput::Move;
		break;
	case WM_LBUTTONDOWN:
		eMouse = LayerInput::LButtonDown;
		break;
	case WM_LBUTTONUP:
		eMouse = LayerInput::LButtonUp;
		break;
	case WM_RBUTTONDOWN:
		eMouse = LayerInput::RButtonDown;
		break;
	case WM_RBUTTONUP:
		eMouse = LayerInput::RButtonUp;
		break;
	case WM_CHAR:
		eKeyboard = LayerInput::Char;
		break;
	case WM_KEYDOWN:
		eKeyboard = LayerInput::KeyDown;
		break;
	case WM_KEYUP:
		eKeyboard = LayerInput::KeyUp;
		break;
	}

	if(m_pCapture)
	{
		if(eMouse != LayerInput::NotMouse)
		{
			INT xView, yView;

			TranslateClientPointToCanvas(LOWORD(lParam), HIWORD(lParam), m_pCapture, &xView, &yView);
			return m_pCapture->ProcessMouseInput(eMouse, wParam, lParam, xView, yView, lResult);
		}
		else if(eKeyboard != LayerInput::NotKeyboard)
			return m_pCapture->ProcessKeyboardInput(eKeyboard, wParam, lParam, lResult);
	}
	else if(eMouse != LayerInput::NotMouse)
	{
		INT xView, yView;
		CSIFCanvas* pCanvas;

		TranslateClientPointToView(LOWORD(lParam), HIWORD(lParam), &pCanvas, &xView, &yView);
		if(pCanvas)
			return static_cast<CInteractiveCanvas*>(pCanvas)->ProcessMouseInput(eMouse, wParam, lParam, xView, yView, lResult);
	}
	else if(eKeyboard != LayerInput::NotKeyboard)
	{
		for(sysint i = m_aCanvas.Length() - 1; i >= 0; i--)
		{
			if(static_cast<CInteractiveCanvas*>(m_aCanvas[i].pCanvas)->ProcessKeyboardInput(eKeyboard, wParam, lParam, lResult))
				return TRUE;
		}
	}

	return FALSE;
}

VOID CInteractiveSurface::CaptureCanvas (CInteractiveCanvas* pCanvas)
{
	ReplaceInterface(m_pCapture, pCanvas);
}

VOID CInteractiveSurface::ReleaseCapture (VOID)
{
	SafeRelease(m_pCapture);
}

HRESULT CInteractiveSurface::AllocCanvas (__deref_out CSIFCanvas** ppCanvas)
{
	HRESULT hr;

	*ppCanvas = __new CInteractiveCanvas;
	CheckAlloc(*ppCanvas);
	hr = S_OK;

Cleanup:
	return hr;
}

VOID CInteractiveSurface::FreeCanvas (CSIFCanvas* pCanvas)
{
	static_cast<CInteractiveCanvas*>(pCanvas)->UnlinkInteractiveLayers();
	static_cast<CInteractiveCanvas*>(pCanvas)->Release();
}
