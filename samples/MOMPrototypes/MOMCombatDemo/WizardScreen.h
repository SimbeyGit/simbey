#pragma once

#include "Library\Core\BaseUnknown.h"
#include "Library\Core\Array.h"
#include "BaseScreen.h"

class CWizardScreen;

class CButton
{
public:
	INT m_x, m_y;
	INT m_xSize, m_ySize;
	ISimbeyInterchangeSprite* m_pButton;
	ISimbeyInterchangeSprite* m_pLabel;
	BOOL m_fPressed;

public:
	CButton ();
	virtual ~CButton ();

	virtual BOOL HasPressedAnimations (VOID) = 0;
	virtual VOID OnHighlight (CWizardScreen* pScreen) = 0;
	virtual VOID OnPressed (CWizardScreen* pScreen) = 0;
};

class CWizardButton : public CButton
{
public:
	RSTRING m_rstrName;
	IJSONObject* m_pWizard;

public:
	CWizardButton ();
	~CWizardButton ();

	virtual BOOL HasPressedAnimations (VOID) { return TRUE; }
	virtual VOID OnHighlight (CWizardScreen* pScreen);
	virtual VOID OnPressed (CWizardScreen* pScreen);
};

class CFlagButton : public CButton
{
public:
	COLORREF m_crFlag;

public:
	CFlagButton (COLORREF crFlag);

	virtual BOOL HasPressedAnimations (VOID) { return FALSE; }
	virtual VOID OnHighlight (CWizardScreen* pScreen);
	virtual VOID OnPressed (CWizardScreen* pScreen);
};

class CWizardScreen :
	public CBaseScreen,
	public ILayerInputHandler
{
protected:
	CSIFCanvas* m_pMain;
	CSIFCanvas* m_pMouse;

	MIDI::CFile m_midiFile;

	ISimbeyInterchangeFileFont* m_pYellowFont;
	ISimbeyInterchangeFileFont* m_pSmallYellowFont;
	ISimbeyInterchangeFileFont* m_pDreamOrphanage6A;
	ISimbeyInterchangeFileFont* m_pDreamOrphanage6B;

	ISimbeyInterchangeSprite* m_pMoveType;
	ISimbeyInterchangeAnimator* m_pButtons;
	ISimbeyInterchangeAnimator* m_pPicks;
	ISimbeyInterchangeAnimator* m_pWizards;

	ISimbeyInterchangeFileLayer* m_pFlagTemplate;

	IJSONObject* m_pData, *m_pSelectedWizard;
	TArray<CButton*> m_aButtons;

	CButton* m_pHighlight;
	CButton* m_pPressed;

public:
	IMP_BASE_UNKNOWN

	CWizardScreen (IScreenHost* pHost, CInteractiveSurface* pSurface, CSIFPackage* pPackage, ISimbeyInterchangeFileFont* pYellowFont, ISimbeyInterchangeFileFont* pSmallYellowFont);
	virtual ~CWizardScreen ();

	HRESULT Initialize (VOID);
	HRESULT ShowWizardButtons (INT xBackground, INT yBackground);

	// IScreen
	virtual VOID OnDestroy (VOID);
	virtual VOID OnUpdateFrame (VOID);
	virtual VOID OnNotifyFinished (BOOL fCompleted);
	virtual VOID OnChangeActive (BOOL fActive);

	// ILayerInputHandler
	virtual BOOL ProcessMouseInput (LayerInput::Mouse eType, WPARAM wParam, LPARAM lParam, INT xView, INT yView, LRESULT& lResult);
	virtual BOOL ProcessKeyboardInput (LayerInput::Keyboard eType, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

protected:
	VOID ReleasePressed (VOID);
	BOOL FindButtonTarget (INT x, INT y, __out INT* pidxButton);
	HRESULT FillBooks (RSTRING rstrType, INT cBooks, __inout INT* pidxNext);
	HRESULT CreateFlagButton (ISimbeyInterchangeFile* pTemp, INT x, INT y, COLORREF crFlag);

public:
	HRESULT ShowWizard (IJSONObject* pWizard);
	HRESULT SelectWizard (IJSONObject* pWizard);
	HRESULT SelectFlagColor (COLORREF crFlag);
};
