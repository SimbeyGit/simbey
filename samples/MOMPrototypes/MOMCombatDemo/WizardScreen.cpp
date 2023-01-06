#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\MemoryStream.h"
#include "CombatScreen.h"
#include "WizardScreen.h"

CButton::CButton () :
	m_x(0),
	m_y(0),
	m_pButton(NULL),
	m_pLabel(NULL)
{
}

CButton::~CButton ()
{
	SafeRelease(m_pLabel);
	SafeRelease(m_pButton);
}

CWizardButton::CWizardButton () :
	m_rstrName(NULL),
	m_pWizard(NULL)
{
}

CWizardButton::~CWizardButton ()
{
	SafeRelease(m_pWizard);
	RStrRelease(m_rstrName);
}

VOID CWizardButton::OnHighlight (CWizardScreen* pScreen)
{
	pScreen->ShowWizard(m_pWizard);
}

VOID CWizardButton::OnPressed (CWizardScreen* pScreen)
{
	pScreen->SelectWizard(m_pWizard);
}

CFlagButton::CFlagButton (COLORREF crFlag) :
	m_crFlag(crFlag)
{
}

VOID CFlagButton::OnHighlight (CWizardScreen* pScreen)
{
}

VOID CFlagButton::OnPressed (CWizardScreen* pScreen)
{
	pScreen->SelectFlagColor(m_crFlag);
}

CWizardScreen::CWizardScreen (IScreenHost* pHost, CInteractiveSurface* pSurface, CSIFPackage* pPackage, ISimbeyInterchangeFileFont* pYellowFont, ISimbeyInterchangeFileFont* pSmallYellowFont) :
	CBaseScreen(pHost, pSurface, pPackage),
	m_pMain(NULL),
	m_pMouse(NULL),
	m_pDreamOrphanage6A(NULL),
	m_pDreamOrphanage6B(NULL),
	m_pMoveType(NULL),
	m_pButtons(NULL),
	m_pPicks(NULL),
	m_pWizards(NULL),
	m_pFlagTemplate(NULL),
	m_pData(NULL),
	m_pSelectedWizard(NULL),
	m_pHighlight(NULL),
	m_pPressed(NULL)
{
	SetInterface(m_pYellowFont, pYellowFont);
	SetInterface(m_pSmallYellowFont, pSmallYellowFont);
}

CWizardScreen::~CWizardScreen ()
{
	m_pSurface->RemoveCanvas(m_pMouse); m_pMouse = NULL;
	m_pSurface->RemoveCanvas(m_pMain); m_pMain = NULL;

	m_aButtons.DeleteAll();
	SafeRelease(m_pSelectedWizard);
	SafeRelease(m_pData);

	SafeRelease(m_pFlagTemplate);
	SafeRelease(m_pWizards);
	SafeRelease(m_pPicks);
	SafeRelease(m_pButtons);
	SafeRelease(m_pMoveType);

	SafeRelease(m_pDreamOrphanage6A);
	SafeRelease(m_pDreamOrphanage6B);
	SafeRelease(m_pSmallYellowFont);
	SafeRelease(m_pYellowFont);
}

HRESULT CWizardScreen::Initialize (VOID)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeFile> srSIF;
	TStackRef<ISimbeyInterchangeFileLayer> srLayer;
	TStackRef<ISimbeyInterchangeSprite> srBackground;
	TStackRef<ISimbeyInterchangeAnimator> srAnimator;
	TStackRef<IJSONValue> srv;
	CInteractiveCanvas* pInteractive;
	CMemoryStream stmMusic;
	ULARGE_INTEGER uliSize;
	INT xView, yView, xSize, ySize, x, y;
	PBYTE pBits;
	DWORD cbBits;

	m_pSurface->GetViewSize(&xView, &yView);

	Check(m_pPackage->OpenSIF(L"graphics\\DreamOrphanage6A.sif", &srSIF));
	Check(sifCreateFontFromSIF(srSIF, TRUE, &m_pDreamOrphanage6A));
	srSIF.Release();

	Check(m_pPackage->OpenSIF(L"graphics\\DreamOrphanage6B.sif", &srSIF));
	Check(sifCreateFontFromSIF(srSIF, TRUE, &m_pDreamOrphanage6B));
	srSIF.Release();

	Check(m_pSurface->AddCanvas(NULL, TRUE, &m_pMain));
	Check(m_pMain->AddLayer(FALSE, LayerRender::Masked, 0, NULL));			// Background
	Check(m_pMain->AddLayer(TRUE, LayerRender::Masked, 0, NULL));			// Portrait and picks

	pInteractive = static_cast<CInteractiveCanvas*>(m_pMain);
	Check(pInteractive->AddInteractiveLayer(FALSE, LayerRender::Masked, 0, this, NULL));	// Buttons

	Check(m_pSurface->AddCanvas(NULL, FALSE, &m_pMouse));
	Check(m_pMouse->AddLayer(FALSE, LayerRender::Masked, 0, NULL));

	Check(m_pPackage->ReadFile(SLP(L"title\\title.MID"), &stmMusic));
	uliSize.QuadPart = stmMusic.DataRemaining();
	Check(m_pHost->LoadMIDI(&stmMusic, &uliSize, &m_midiFile));
	Check(m_pHost->PlayMIDI(&m_midiFile));

	Check(LoadAnimator(SLP(L"graphics\\MoveType.json"), L"graphics\\MoveType.sif", &srAnimator, FALSE));

	Check(srAnimator->CreateSprite(&m_pMoveType));
	Check(m_pMoveType->SelectAnimation(3));
	m_pMoveType->SetPosition(xView >> 1, yView >> 1);
	Check(m_pMouse->AddSprite(0, m_pMoveType, NULL));

	Check(m_pPackage->OpenSIF(L"graphics\\new_game_ui.sif", &srSIF));
	Check(srSIF->FindLayer(L"WizardSelection.png", &srLayer, NULL));
	Check(sifCreateStaticSprite(srLayer, 0, 0, &srBackground));

	srBackground->GetCurrentFrameSize(&xSize, &ySize);
	x = (xView - xSize) >> 1;
	y = (yView - ySize) >> 1;
	srBackground->SetPosition(x, y);

	Check(m_pMain->AddSprite(0, srBackground, NULL));

	Check(LoadAnimator(SLP(L"buttons\\animations.json"), L"buttons\\buttons.sif", &m_pButtons, TRUE));
	Check(LoadAnimator(SLP(L"picks\\animations.json"), L"picks\\picks.sif", &m_pPicks, TRUE));
	Check(LoadAnimator(SLP(L"wizards\\animations.json"), L"wizards\\wizards.sif", &m_pWizards, TRUE));

	Check(m_pPackage->GetJSONData(SLP(L"wizards\\wizards.json"), &srv));
	Check(srv->GetObject(&m_pData));

	Check(ShowWizardButtons(x, y));

	Check(srSIF->FindLayer(L"Flag.png", &m_pFlagTemplate, NULL));
	Check(m_pFlagTemplate->GetBitsPtr(&pBits, &cbBits));	// Force load

Cleanup:
	if(srSIF)
		srSIF->Close();
	return hr;
}

HRESULT CWizardScreen::ShowWizardButtons (INT xBackground, INT yBackground)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONArray> srWizards;
	INT xFrame = xBackground + 168, yFrame = yBackground + 28;
	INT x = xFrame, y = yFrame;
	TStackRef<ISimbeyInterchangeSprite> srTitle;
	RSTRING rstrTitle = NULL;
	CWizardButton* pBtn = NULL;

	Check(RStrCreateW(LSP(L"Select Wizard"), &rstrTitle));
	srTitle.Attach(__new CDrawText(m_pYellowFont, rstrTitle, TRUE));
	CheckAlloc(srTitle);
	srTitle->SetPosition(xFrame + 75, yFrame - 25);
	Check(m_pMain->AddSprite(2, srTitle, NULL));

	Check(m_pData->FindNonNullValueW(L"wizards", &srv));
	Check(srv->GetArray(&srWizards));
	srv.Release();

	for(sysint i = 0; i < srWizards->Count(); i++)
	{
		TStackRef<IJSONObject> srWizard;
		TStackRef<ISimbeyInterchangeSprite> srButton, srLabel;

		pBtn = __new CWizardButton;
		CheckAlloc(pBtn);

		pBtn->m_x = x;
		pBtn->m_y = y;
		pBtn->m_xSize = 74;
		pBtn->m_ySize = 21;

		Check(srWizards->GetObject(i, &srWizard));
		Check(srWizard->FindNonNullValueW(L"name", &srv));
		Check(srv->GetString(&pBtn->m_rstrName));
		srv.Release();

		Check(m_pButtons->CreateSprite(&srButton));
		Check(srButton->SelectAnimation(1));
		srButton->SetPosition(x, y);
		Check(m_pMain->AddSprite(2, srButton, NULL));

		srLabel.Attach(__new CDrawText(m_pDreamOrphanage6B, pBtn->m_rstrName, TRUE));
		CheckAlloc(srLabel);
		srLabel->SetPosition(x + 37, y + 1);
		Check(m_pMain->AddSprite(2, srLabel, NULL));

		pBtn->m_pWizard = srWizard.Detach();
		pBtn->m_pButton = srButton.Detach();
		pBtn->m_pLabel = srLabel.Detach();

		Check(m_aButtons.Append(pBtn));
		pBtn = NULL;

		if(i == 6)
		{
			x += 75;
			y = yFrame;
		}
		else
			y += 25;
	}

Cleanup:
	RStrRelease(rstrTitle);
	SafeDelete(pBtn);
	return hr;
}

// IScreen

VOID CWizardScreen::OnDestroy (VOID)
{
	m_pSurface->RemoveCanvas(m_pMouse); m_pMouse = NULL;
	m_pSurface->RemoveCanvas(m_pMain); m_pMain = NULL;
}

VOID CWizardScreen::OnUpdateFrame (VOID)
{
}

VOID CWizardScreen::OnNotifyFinished (BOOL fCompleted)
{
	if(fCompleted)
		m_pHost->PlayMIDI(&m_midiFile);
}

VOID CWizardScreen::OnChangeActive (BOOL fActive)
{
}

// ILayerInputHandler

BOOL CWizardScreen::ProcessMouseInput (LayerInput::Mouse eType, WPARAM wParam, LPARAM lParam, INT xView, INT yView, LRESULT& lResult)
{
	if(LayerInput::Move == eType)
	{
		INT idxButton;

		if(NULL == m_pPressed && FindButtonTarget(xView, yView, &idxButton))
		{
			CButton* pBtn = m_aButtons[idxButton];

			if(pBtn != m_pHighlight)
			{
				m_pHighlight = pBtn;
				pBtn->OnHighlight(this);
			}
		}

		m_pSurface->TranslateClientPointToCanvas(LOWORD(lParam), HIWORD(lParam), m_pMouse, &xView, &yView);
		m_pMoveType->SetPosition(xView, yView);
	}
	else if(LayerInput::LButtonDown == eType)
	{
		INT idxButton;

		if(FindButtonTarget(xView, yView, &idxButton))
		{
			CButton* pBtn = m_aButtons[idxButton];

			if(pBtn != m_pPressed)
			{
				ReleasePressed();

				if(FALSE == pBtn->HasPressedAnimations() || SUCCEEDED(pBtn->m_pButton->SelectAnimation(2)))
				{
					if(pBtn->m_pLabel)
					{
						INT x, y;
						pBtn->m_pLabel->GetPosition(x, y);
						pBtn->m_pLabel->SetPosition(x + 1, y + 1);
					}

					m_pPressed = pBtn;
				}
			}
		}
	}
	else if(LayerInput::LButtonUp == eType)
	{
		CStackRef ref(static_cast<IScreen*>(this));
		INT idxButton;

		if(FindButtonTarget(xView, yView, &idxButton))
		{
			CButton* pBtn = m_aButtons[idxButton];

			if(pBtn == m_pPressed)
			{
				ReleasePressed();
				pBtn->OnPressed(this);
			}
		}

		ReleasePressed();
	}

	return TRUE;
}

BOOL CWizardScreen::ProcessKeyboardInput (LayerInput::Keyboard eType, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return FALSE;
}

VOID CWizardScreen::ReleasePressed (VOID)
{
	if(m_pPressed)
	{
		if(m_pPressed->m_pLabel)
		{
			INT x, y;
			m_pPressed->m_pLabel->GetPosition(x, y);
			m_pPressed->m_pLabel->SetPosition(x - 1, y - 1);
		}

		if(m_pPressed->HasPressedAnimations())
			m_pPressed->m_pButton->SelectAnimation(1);

		m_pPressed = NULL;
	}
}

BOOL CWizardScreen::FindButtonTarget (INT x, INT y, __out INT* pidxButton)
{
	for(INT i = 0; i < m_aButtons.Length(); i++)
	{
		CButton* pBtn = m_aButtons[i];

		if(pBtn->m_x <= x && x < pBtn->m_x + pBtn->m_xSize &&
			pBtn->m_y <= y && y < pBtn->m_y + pBtn->m_ySize)
		{
			*pidxButton = i;
			return TRUE;
		}
	}

	return FALSE;
}

HRESULT CWizardScreen::FillBooks (RSTRING rstrType, INT cBooks, __inout INT* pidxNext)
{
	HRESULT hr = S_FALSE;
	PCWSTR pcwzType = RStrToWide(rstrType);
	INT idxStart;

	if(0 == TStrCmpAssert(pcwzType, L"chaos"))
		idxStart = 0;
	else if(0 == TStrCmpAssert(pcwzType, L"death"))
		idxStart = 3;
	else if(0 == TStrCmpAssert(pcwzType, L"life"))
		idxStart = 6;
	else if(0 == TStrCmpAssert(pcwzType, L"nature"))
		idxStart = 9;
	else if(0 == TStrCmpAssert(pcwzType, L"sorcery"))
		idxStart = 12;
	else
		Check(E_INVALIDARG);

	for(INT i = 0; i < cBooks; i++)
	{
		TStackRef<ISimbeyInterchangeSprite> srBook;

		Check(m_pPicks->CreateSprite(&srBook));
		Check(srBook->SelectAnimation((rand() % 3) + idxStart));
		srBook->SetPosition((*pidxNext + i) * 9 + 129, 195);
		Check(m_pMain->AddSprite(1, srBook, NULL));
	}

	*pidxNext += cBooks;

Cleanup:
	return hr;
}

HRESULT CWizardScreen::CreateFlagButton (ISimbeyInterchangeFile* pTemp, INT x, INT y, COLORREF crFlag)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeFileLayer> srClone;
	CFlagButton* pFlagButton = NULL;
	PBYTE pBits;
	DWORD cbBits;
	RECT rcPos;
	USHORT nWidth, nHeight;

	Check(m_pFlagTemplate->GetBitsPtr(&pBits, &cbBits));
	Check(m_pFlagTemplate->GetPosition(&rcPos));

	nWidth = static_cast<USHORT>(rcPos.right - rcPos.left);
	nHeight = static_cast<USHORT>(rcPos.bottom - rcPos.top);
	Check(pTemp->AddLayerFromBits(nWidth, nHeight, pBits, 32, nWidth * 4, &srClone, NULL));

	Check(srClone->GetBitsPtr(&pBits, &cbBits));
	for(USHORT yPixel = 0; yPixel < nHeight; yPixel++)
	{
		for(USHORT xPixel = 0; xPixel < nWidth; xPixel++)
		{
			BYTE fColorize = 95 - (pBits[0] - 160);

			pBits[0] = MulDiv(GetRValue(crFlag), fColorize, 95);
			pBits[1] = MulDiv(GetGValue(crFlag), fColorize, 95);
			pBits[2] = MulDiv(GetBValue(crFlag), fColorize, 95);
			pBits += sizeof(DWORD);
		}
	}

	pFlagButton = __new CFlagButton(crFlag);
	CheckAlloc(pFlagButton);

	pFlagButton->m_x = x;
	pFlagButton->m_y = y;
	pFlagButton->m_xSize = 54;
	pFlagButton->m_ySize = 15;

	Check(sifCreateStaticSprite(srClone, 0, 0, &pFlagButton->m_pButton));
	Check(m_pMain->AddSprite(2, pFlagButton->m_pButton, NULL));

	pFlagButton->m_pButton->SetPosition(x, y);

	Check(m_aButtons.Append(pFlagButton));
	pFlagButton = NULL;

Cleanup:
	SafeDelete(pFlagButton);
	return hr;
}

HRESULT CWizardScreen::ShowWizard (IJSONObject* pWizard)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONObject> srBooks;
	TStackRef<IJSONArray> srRetorts;
	TStackRef<ISimbeyInterchangeSprite> srWizard;
	INT nPortrait, idxNext = 0;
	RSTRING rstrType = NULL, rstrRetort = NULL;

	m_pMain->ClearLayer(1);

	Check(pWizard->FindNonNullValueW(L"portrait", &srv));
	Check(srv->GetInteger(&nPortrait));
	srv.Release();

	Check(m_pWizards->CreateSprite(&srWizard));
	Check(srWizard->SelectAnimation(nPortrait));
	srWizard->SetPosition(120, 70);
	Check(m_pMain->AddSprite(1, srWizard, NULL));

	Check(pWizard->FindNonNullValueW(L"books", &srv));
	Check(srv->GetObject(&srBooks));
	srv.Release();

	sysint cSets = srBooks->Count();
	for(sysint i = 0; i < cSets; i++)
	{
		INT cBooks;

		Check(srBooks->GetValueName(i, &rstrType));
		Check(srBooks->GetValueByIndex(i, &srv));
		Check(srv->GetInteger(&cBooks));
		srv.Release();

		Check(FillBooks(rstrType, cBooks, &idxNext));

		RStrRelease(rstrType); rstrType = NULL;
	}

	Check(pWizard->FindNonNullValueW(L"retorts", &srv));
	Check(srv->GetArray(&srRetorts));
	srv.Release();

	sysint cRetorts = srRetorts->Count();
	INT y = 220;
	for(sysint i = 0; i < cRetorts; i++)
	{
		TStackRef<CDrawText> srRetort;

		Check(srRetorts->GetString(i, &rstrRetort));

		srRetort.Attach(__new CDrawText(m_pDreamOrphanage6A, rstrRetort, FALSE));
		CheckAlloc(srRetort);
		srRetort->SetPosition(140, y);
		Check(m_pMain->AddSprite(1, srRetort, NULL));
		y += 10;

		RStrRelease(rstrRetort); rstrRetort = NULL;
	}

Cleanup:
	RStrRelease(rstrRetort);
	RStrRelease(rstrType);
	return hr;
}

HRESULT CWizardScreen::SelectWizard (IJSONObject* pWizard)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeFile> srSIF;
	TStackRef<ISimbeyInterchangeSprite> srTitle;
	TStackRef<IJSONValue> srv;
	RSTRING rstrTitle = NULL;
	INT xFrame = 264, yFrame = 88;

	Check(JSONCloneObject(pWizard, &srv, FALSE));
	Check(srv->GetObject(&m_pSelectedWizard));

	m_pMain->ClearLayer(2);
	m_aButtons.DeleteAll();

	Check(sifCreateNew(&srSIF));

	Check(RStrCreateW(LSP(L"Select Flag"), &rstrTitle));
	srTitle.Attach(__new CDrawText(m_pYellowFont, rstrTitle, TRUE));
	CheckAlloc(srTitle);
	srTitle->SetPosition(xFrame + 75, yFrame - 25);
	Check(m_pMain->AddSprite(2, srTitle, NULL));

	Check(CreateFlagButton(srSIF, xFrame + 7, yFrame + 7, RGB(255, 64, 64)));
	Check(CreateFlagButton(srSIF, xFrame + 7, yFrame + 27, RGB(64, 255, 64)));
	Check(CreateFlagButton(srSIF, xFrame + 7, yFrame + 47, RGB(64, 64, 255)));
	Check(CreateFlagButton(srSIF, xFrame + 7, yFrame + 67, RGB(255, 255, 64)));
	Check(CreateFlagButton(srSIF, xFrame + 7, yFrame + 87, RGB(64, 255, 255)));
	Check(CreateFlagButton(srSIF, xFrame + 7, yFrame + 107, RGB(200, 0, 200)));
	Check(CreateFlagButton(srSIF, xFrame + 7, yFrame + 127, RGB(255, 160, 0)));
	Check(CreateFlagButton(srSIF, xFrame + 7, yFrame + 147, RGB(80, 80, 88)));

	Check(CreateFlagButton(srSIF, xFrame + 77, yFrame + 7, RGB(255, 120, 190)));
	Check(CreateFlagButton(srSIF, xFrame + 77, yFrame + 27, RGB(70, 40, 120)));
	Check(CreateFlagButton(srSIF, xFrame + 77, yFrame + 47, RGB(0, 160, 190)));
	Check(CreateFlagButton(srSIF, xFrame + 77, yFrame + 67, RGB(230, 190, 160)));
	Check(CreateFlagButton(srSIF, xFrame + 77, yFrame + 87, RGB(80, 160, 120)));
	Check(CreateFlagButton(srSIF, xFrame + 77, yFrame + 107, RGB(100, 88, 70)));
	Check(CreateFlagButton(srSIF, xFrame + 77, yFrame + 127, RGB(212, 255, 60)));
	Check(CreateFlagButton(srSIF, xFrame + 77, yFrame + 147, RGB(255, 255, 255)));

Cleanup:
	RStrRelease(rstrTitle);
	if(srSIF)
		srSIF->Close();
	return hr;
}

HRESULT CWizardScreen::SelectFlagColor (COLORREF crFlag)
{
	HRESULT hr;
	CCombatScreen* pCombat = NULL;
	TStackRef<IJSONObject> srPlacements;
	TStackRef<IJSONValue> srvFlag;

	Check(m_pHost->GetData(L"placements", &srPlacements));

	pCombat = __new CCombatScreen(m_pHost, m_pSurface, m_pPackage, m_pYellowFont, m_pSmallYellowFont, m_pSelectedWizard, srPlacements);
	CheckAlloc(pCombat);

	Check(JSONCreateLongInteger(static_cast<__int64>(crFlag), &srvFlag));
	Check(m_pSelectedWizard->AddValueW(L"flag", srvFlag));

	m_pHost->StopMIDI();
	Check(pCombat->Initialize());
	m_pHost->SwitchToScreen(pCombat);

Cleanup:
	SafeRelease(pCombat);
	return hr;
}
