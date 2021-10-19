#pragma once

#include "Library\Core\BaseUnknown.h"
#include "BaseScreen.h"

class CDrawSolid;

namespace IntroEvent
{
	enum Type
	{
		Invalid,
		Fader,
		Sound,
		Exit
	};
}

struct INTRO_EVENT
{
	IntroEvent::Type eType;
	INT nFrame;
	union
	{
		struct
		{
			FLOAT rChange;
			FLOAT rStop;
		} fader;
		RSTRING rstrSound;
	};
};

class CIntroScreen :
	public CBaseScreen,
	public ILayerInputHandler
{
protected:
	CSIFCanvas* m_pMain;
	ISimbeyInterchangeFile* m_pIntro;

	MIDI::CFile m_midiFile;
	MIDI::MIDI_RESUME m_mResume;

	ISimbeyInterchangeFileFont* m_pYellowFont;
	ISimbeyInterchangeFileFont* m_pSmallYellowFont;

	INT m_x, m_y;
	INT m_nTicks, m_nFrame;
	FLOAT m_rFadeAlpha;

	CDrawSolid* m_pSolid;
	TArray<INTRO_EVENT> m_aEvents;
	sysint m_nEventPtr;

public:
	IMP_BASE_UNKNOWN

	CIntroScreen (IScreenHost* pHost, CInteractiveSurface* pSurface, CSIFPackage* pPackage, ISimbeyInterchangeFileFont* pYellowFont, ISimbeyInterchangeFileFont* pSmallYellowFont);
	virtual ~CIntroScreen ();

	HRESULT Initialize (VOID);

	// IScreen
	virtual VOID OnDestroy (VOID);
	virtual VOID OnUpdateFrame (VOID);
	virtual VOID OnNotifyFinished (BOOL fCompleted);
	virtual VOID OnChangeActive (BOOL fActive);

	// ILayerInputHandler
	virtual BOOL ProcessMouseInput (LayerInput::Mouse eType, WPARAM wParam, LPARAM lParam, INT xView, INT yView, LRESULT& lResult);
	virtual BOOL ProcessKeyboardInput (LayerInput::Keyboard eType, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

private:
	HRESULT LoadIntroEvents (IJSONValue* pvEvents);
	HRESULT ShowWizardScreen (VOID);
	HRESULT UpdateFrame (VOID);
	HRESULT UpdateEvent (INTRO_EVENT& evt);
};
