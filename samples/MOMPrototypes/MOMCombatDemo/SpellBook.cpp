#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "CombatSpells.h"
#include "SpellBook.h"

CSpellBook::CSpellBook (CSIFSurface* pSurface, CCombatScreen* pWindow, IJSONObject* pWizard) :
	m_pSurface(pSurface),
	m_pScreen(pWindow),
	m_pWizard(pWizard),
	m_pCanvas(NULL),
	m_pLayer(NULL),
	m_pSIF(NULL),
	m_pCloseButton(NULL)
{
	m_pWizard->AddRef();
}

CSpellBook::~CSpellBook ()
{
	SafeRelease(m_pCloseButton);
	SafeRelease(m_pWizard);

	if(m_pSIF)
	{
		m_pSIF->Close();
		m_pSIF->Release();
	}

	Assert(NULL == m_pLayer);
	Assert(NULL == m_pCanvas);
}

HRESULT CSpellBook::Initialize (VOID)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeFileLayer> srBackground, srCloseButton;
	TStackRef<ISimbeyInterchangeSprite> srSprite;
	INT xView, yView, xClose, yClose;
	RECT rc;
	sysint idxSpellBook;

	CSIFPackage* pPackage = m_pScreen->GetPackage();
	Check(pPackage->OpenSIF(L"spell book\\spellbook.sif", &m_pSIF));
	Check(m_pSIF->FindLayer(L"background.png", &srBackground, NULL));
	Check(srBackground->GetPosition(&rc));

	m_pSurface->GetViewSize(&xView, &yView);
	Check(m_pSurface->AddCanvas(NULL, TRUE, &m_pCanvas));

	xClose = xView / 2 - (rc.right - rc.left) / 2;
	yClose = yView / 2 - (rc.bottom - rc.top) / 2;

	Check(m_pCanvas->AddLayer(FALSE, FALSE, 0, &m_idxBackground));
	Check(sifCreateStaticSprite(srBackground, xClose, yClose, &srSprite));
	Check(m_pCanvas->AddSprite(m_idxBackground, srSprite, &idxSpellBook));

	xClose += 159;
	yClose += 168;

	Check(m_pSIF->FindLayer(L"closeButtonPressedInline.png", &srCloseButton, NULL));
	Check(srCloseButton->GetPosition(&rc));
	Check(sifCreateStaticSprite(srCloseButton, xClose, yClose, &m_pCloseButton));

	Check(static_cast<CInteractiveCanvas*>(m_pCanvas)->AddInteractiveLayer(TRUE, FALSE, 0, this, &m_pLayer));

Cleanup:
	return hr;
}

VOID CSpellBook::Destroy (VOID)
{
	if(m_pCanvas)
	{
		SafeRelease(m_pLayer);
		m_pSurface->RemoveCanvas(m_pCanvas);
		m_pCanvas = NULL;
	}
}

VOID CSpellBook::Close (VOID)
{
	m_pScreen->RemoveSpellBook();
}

// ILayerInputHandler

BOOL CSpellBook::ProcessMouseInput (LayerInput::Mouse eType, WPARAM wParam, LPARAM lParam, INT xView, INT yView, LRESULT& lResult)
{
	if(LayerInput::Move == eType)
		m_pScreen->UpdateMouse(lParam);
	else if(LayerInput::LButtonDown == eType)
	{
		if(MouseOverCloseButton(xView, yView))
			m_pCanvas->AddSprite(m_idxBackground, m_pCloseButton, NULL);
		return TRUE;
	}
	else if(LayerInput::LButtonUp == eType)
	{
		if(S_OK == m_pCanvas->FindSprite(m_idxBackground, m_pCloseButton, NULL))
		{
			m_pCanvas->RemoveSprite(m_idxBackground, m_pCloseButton);
			if(MouseOverCloseButton(xView, yView))
				Close();
		}
		else
		{
			RSTRING rstrName = NULL;
			HRESULT hr;

			switch(rand() % 4)
			{
			case 0:
				hr = RStrCreateW(LSP(L"Phantom Warriors"), &rstrName);
				break;
			case 1:
				hr = RStrCreateW(LSP(L"Hell Hounds"), &rstrName);
				break;
			case 2:
				hr = RStrCreateW(LSP(L"Earth Elemental"), &rstrName);
				break;
			case 3:
				hr = RStrCreateW(LSP(L"Skeletons"), &rstrName);
				break;
			}

			if(SUCCEEDED(hr))
			{
				TStackRef<IJSONValue> srvName;
				RSTRING rstrCasterW;

				SideAssertHr(m_pWizard->FindNonNullValueW(L"name", &srvName));
				SideAssertHr(srvName->GetString(&rstrCasterW));
				m_pScreen->AttachSpellCaster(__new CCastSummonSpell(rstrCasterW, rstrName));
				RStrRelease(rstrCasterW);
				RStrRelease(rstrName);

				Close();
			}
		}

		return TRUE;
	}

	return FALSE;
}

BOOL CSpellBook::ProcessKeyboardInput (LayerInput::Keyboard eType, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	if(LayerInput::KeyUp == eType && VK_ESCAPE == wParam)
	{
		Close();
		return TRUE;
	}

	return FALSE;
}

BOOL CSpellBook::MouseOverCloseButton (INT x, INT y)
{
	INT xBtn, yBtn, xSize, ySize;

	m_pCloseButton->GetFrameOffset(xBtn, yBtn);
	m_pCloseButton->GetCurrentFrameSize(&xSize, &ySize);

	return x >= xBtn && y >= yBtn && x < xBtn + xSize && y < yBtn + ySize;
}
