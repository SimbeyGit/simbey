#include <windows.h>
#include <gdiplus.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Util\Formatting.h"
#include "UnitStats.h"

CUnitStats::CUnitStats (CSIFSurface* pSurface, IPopupHost* pScreen) :
	m_pSurface(pSurface),
	m_pScreen(pScreen),
	m_pCanvas(NULL),
	m_pLayer(NULL),
	m_pSIF(NULL)
{
}

CUnitStats::~CUnitStats ()
{
	if(m_pSIF)
	{
		m_pSIF->Close();
		m_pSIF->Release();
	}

	Assert(NULL == m_pLayer);
	Assert(NULL == m_pCanvas);
}

HRESULT CUnitStats::Initialize (IJSONObject* pDef, IJSONObject* pStats, INT nLevel)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeFileLayer> srBackground;
	TStackRef<ISimbeyInterchangeSprite> srSprite;
	INT xView, yView;
	RECT rc;
	sysint idxUnitStats;

	CSIFPackage* pPackage = m_pScreen->GetPackage();
	Check(pPackage->OpenSIF(L"combat stats\\unit_details.sif", &m_pSIF));
	Check(m_pSIF->FindLayer(L"UnitDetails.png", &srBackground, NULL));
	Check(srBackground->GetPosition(&rc));

	m_pSurface->GetViewSize(&xView, &yView);
	Check(m_pSurface->AddCanvas(NULL, TRUE, &m_pCanvas));

	m_xStats = xView / 2 - (rc.right - rc.left) / 2;
	m_yStats = yView / 2 - (rc.bottom - rc.top) / 2;

	Check(m_pCanvas->AddLayer(FALSE, FALSE, 0, &m_idxBackground));
	Check(sifCreateStaticSprite(srBackground, m_xStats, m_yStats, &srSprite));
	Check(m_pCanvas->AddSprite(m_idxBackground, srSprite, &idxUnitStats));

	Check(static_cast<CInteractiveCanvas*>(m_pCanvas)->AddInteractiveLayer(TRUE, FALSE, 0, this, &m_pLayer));

Cleanup:
	return hr;
}

VOID CUnitStats::Close (VOID)
{
	m_pScreen->RemovePopup(this);
}

// IPopupView

VOID CUnitStats::Destroy (VOID)
{
	if(m_pCanvas)
	{
		SafeRelease(m_pLayer);
		m_pSurface->RemoveCanvas(m_pCanvas);
		m_pCanvas = NULL;
	}
}

// ILayerInputHandler

BOOL CUnitStats::ProcessMouseInput (LayerInput::Mouse eType, WPARAM wParam, LPARAM lParam, INT xView, INT yView, LRESULT& lResult)
{
	if(LayerInput::Move == eType)
		m_pScreen->UpdateMouse(lParam);

	return FALSE;
}

BOOL CUnitStats::ProcessKeyboardInput (LayerInput::Keyboard eType, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	if(LayerInput::KeyUp == eType && VK_ESCAPE == wParam)
	{
		Close();
		return TRUE;
	}

	return FALSE;
}
