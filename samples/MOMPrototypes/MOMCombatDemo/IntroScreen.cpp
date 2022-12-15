#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\MemoryStream.h"
#include "WizardScreen.h"
#include "IntroScreen.h"

CIntroScreen::CIntroScreen (IScreenHost* pHost, CInteractiveSurface* pSurface, CSIFPackage* pPackage, ISimbeyInterchangeFileFont* pYellowFont, ISimbeyInterchangeFileFont* pSmallYellowFont) :
	CBaseScreen(pHost, pSurface, pPackage),
	m_pMain(NULL),
	m_pIntro(NULL),
	m_nTicks(0),
	m_nFrame(-1),
	m_pSolid(NULL),
	m_rFadeAlpha(255.0f)
{
	SetInterface(m_pYellowFont, pYellowFont);
	SetInterface(m_pSmallYellowFont, pSmallYellowFont);

	m_mResume.nResumePoint = 0;
	m_mResume.nPPQN = 0;
}

CIntroScreen::~CIntroScreen ()
{
	for(sysint i = 0; i < m_aEvents.Length(); i++)
	{
		if(IntroEvent::Sound == m_aEvents[i].eType)
			RStrRelease(m_aEvents[i].rstrSound);
	}

	SafeRelease(m_pSmallYellowFont);
	SafeRelease(m_pYellowFont);
}

HRESULT CIntroScreen::Initialize (VOID)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	TStackRef<CInteractiveLayer> srFader;
	CMemoryStream stmMusic;
	ULARGE_INTEGER uliSize;
	INT xView, yView;

	m_pSurface->GetViewSize(&xView, &yView);
	m_x = (xView - 320) / 2;
	m_y = (yView - 200) / 2;

	Check(m_pPackage->GetJSONData(SLP(L"intro\\Animation.json"), &srv));
	Check(LoadIntroEvents(srv));
	m_nEventPtr = 0;

	Check(m_pPackage->OpenSIF(L"intro\\Animation.sif", &m_pIntro));

	Check(m_pSurface->AddCanvas(NULL, TRUE, &m_pMain));
	Check(m_pMain->AddLayer(FALSE, LayerRender::Masked, 0xFF000000, NULL));
	Check(static_cast<CInteractiveCanvas*>(m_pMain)->AddInteractiveLayer(FALSE, LayerRender::Masked, 0, this, &srFader));

	Check(m_pPackage->ReadFile(SLP(L"intro\\Animation.MID"), &stmMusic));
	uliSize.QuadPart = stmMusic.DataRemaining();
	Check(m_pHost->LoadMIDI(&stmMusic, &uliSize, &m_midiFile));
	Check(m_pHost->PlayMIDI(&m_midiFile));

	m_pSolid = __new CDrawSolid(((COLORREF)m_rFadeAlpha) << 24 | RGB(0, 0, 0), 320, 200);
	CheckAlloc(m_pSolid);
	Check(m_pMain->AddSprite(srFader->GetLayer(), m_pSolid, NULL));
	m_pSolid->SetPosition(m_x, m_y);

	Check(UpdateFrame());

Cleanup:
	return hr;
}

// IScreen

VOID CIntroScreen::OnDestroy (VOID)
{
	m_pSurface->RemoveCanvas(m_pMain); m_pMain = NULL;

	if(m_pIntro)
	{
		m_pIntro->Close();
		SafeRelease(m_pIntro);
	}

	SafeRelease(m_pSolid);
}

VOID CIntroScreen::OnUpdateFrame (VOID)
{
	if(m_pHost->IsPressed(27) || m_pHost->IsPressed(13))
		ShowWizardScreen();
	else
	{
		INTRO_EVENT& evt = m_aEvents[m_nEventPtr];
		if(evt.nFrame == m_nFrame && IntroEvent::Fader == evt.eType)
		{
			m_rFadeAlpha += evt.fader.rChange;
			if((evt.fader.rChange < 0.0f && m_rFadeAlpha < evt.fader.rStop) ||
				(evt.fader.rChange > 0.0f && m_rFadeAlpha > evt.fader.rStop))
			{
				m_rFadeAlpha = evt.fader.rStop;
				m_nEventPtr++;

				UpdateFrame();
				UpdateEvent(m_aEvents[m_nEventPtr]);
			}

			m_pSolid->SetSolidColor(((COLORREF)m_rFadeAlpha << 24) | RGB(0, 0, 0));
		}
		else if(++m_nTicks == 9)
		{
			m_nTicks = 0;
			UpdateFrame();
			UpdateEvent(evt);
		}
	}
}

VOID CIntroScreen::OnNotifyFinished (BOOL fCompleted)
{
}

VOID CIntroScreen::OnChangeActive (BOOL fActive)
{
	if(fActive)
		m_pHost->PlayMIDI(&m_midiFile, &m_mResume);
	else
		m_pHost->StopMIDI(&m_mResume);
}

// IInteractiveLayerHandler

BOOL CIntroScreen::ProcessMouseInput (LayerInput::Mouse eType, WPARAM wParam, LPARAM lParam, INT xView, INT yView, LRESULT& lResult)
{
	if(eType == LayerInput::LButtonUp)
	{
		ShowWizardScreen();
		return TRUE;
	}

	return FALSE;
}

BOOL CIntroScreen::ProcessKeyboardInput (LayerInput::Keyboard eType, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return FALSE;
}

HRESULT CIntroScreen::LoadIntroEvents (IJSONValue* pvEvents)
{
	HRESULT hr;
	TStackRef<IJSONArray> srEvents;
	sysint cEvents;
	RSTRING rstrType = NULL;

	Check(pvEvents->GetArray(&srEvents));

	cEvents = srEvents->Count();
	for(sysint i = 0; i < cEvents; i++)
	{
		TStackRef<IJSONObject> srEvent;
		TStackRef<IJSONValue> srv;
		INT nResult;
		INTRO_EVENT* pEvent;

		Check(m_aEvents.AppendSlot(&pEvent));
		pEvent->eType = IntroEvent::Invalid;

		Check(srEvents->GetObject(i, &srEvent));
		Check(srEvent->FindNonNullValueW(L"frame", &srv));
		Check(srv->GetInteger(&pEvent->nFrame));
		srv.Release();

		Check(srEvent->FindNonNullValueW(L"type", &srv));
		Check(srv->GetString(&rstrType));

		Check(RStrCompareW(rstrType, L"fader", &nResult));
		if(0 == nResult)
		{
			srv.Release();
			Check(srEvent->FindNonNullValueW(L"change", &srv));
			Check(srv->GetFloat(&pEvent->fader.rChange));

			srv.Release();
			Check(srEvent->FindNonNullValueW(L"stop", &srv));
			Check(srv->GetFloat(&pEvent->fader.rStop));

			pEvent->eType = IntroEvent::Fader;
		}
		else
		{
			Check(RStrCompareW(rstrType, L"sound", &nResult));
			if(0 == nResult)
			{
				srv.Release();
				Check(srEvent->FindNonNullValueW(L"sound", &srv));
				Check(srv->GetString(&pEvent->rstrSound));

				pEvent->eType = IntroEvent::Sound;
			}
			else
			{
				Check(RStrCompareW(rstrType, L"exit", &nResult));
				CheckIf(0 != nResult, E_FAIL);
				pEvent->eType = IntroEvent::Exit;
			}
		}

		RStrRelease(rstrType); rstrType = NULL;
	}

Cleanup:
	RStrRelease(rstrType);
	return hr;
}

HRESULT CIntroScreen::ShowWizardScreen (VOID)
{
	HRESULT hr;
	CWizardScreen* pWizard = __new CWizardScreen(m_pHost, m_pSurface, m_pPackage, m_pYellowFont, m_pSmallYellowFont);
	if(pWizard)
	{
		m_pHost->StopMIDI();
		hr = pWizard->Initialize();
		m_pHost->SwitchToScreen(pWizard);
		pWizard->Release();
	}
	else
		hr = E_OUTOFMEMORY;
	return hr;
}

HRESULT CIntroScreen::UpdateFrame (VOID)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeFileLayer> srFrame;

	if(SUCCEEDED(m_pIntro->GetLayerByIndex(++m_nFrame, &srFrame)))
	{
		TStackRef<ISimbeyInterchangeSprite> srSprite;

		m_pMain->ClearLayer(0);

		Check(sifCreateStaticSprite(srFrame, 0, 0, &srSprite));
		srSprite->SetPosition(m_x, m_y);

		Check(m_pMain->AddSprite(0, srSprite, NULL));
	}
	else
		hr = S_FALSE;

Cleanup:
	return hr;
}

HRESULT CIntroScreen::UpdateEvent (INTRO_EVENT& evt)
{
	HRESULT hr;

	CheckIfIgnore(evt.nFrame != m_nFrame, S_FALSE);

	if(IntroEvent::Sound == evt.eType)
	{
		FMOD::Sound* pSound;
		Check(m_pHost->FindIntroSound(evt.rstrSound, &pSound));
		m_pHost->PlaySound(FMOD_CHANNEL_FREE, pSound, false, NULL);
		m_nEventPtr++;
	}
	else if(IntroEvent::Exit == evt.eType)
		Check(ShowWizardScreen());
	else
		hr = S_FALSE;

Cleanup:
	return hr;
}
