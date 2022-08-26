#pragma once

#include "CombatScreen.h"

class CSpellBook :
	public CBaseUnknown,
	public ILayerInputHandler
{
private:
	CCombatScreen* m_pScreen;
	CSIFSurface* m_pSurface;
	IJSONObject* m_pWizard;

	CSIFCanvas* m_pCanvas;
	CInteractiveLayer* m_pLayer;

	ISimbeyInterchangeFile* m_pSIF;

	sysint m_idxBackground;
	ISimbeyInterchangeSprite* m_pCloseButton;

public:
	IMP_BASE_UNKNOWN

	EMPTY_UNK_MAP

public:
	CSpellBook (CSIFSurface* pSurface, CCombatScreen* pWindow, IJSONObject* pWizard);
	~CSpellBook ();

	HRESULT Initialize (VOID);
	VOID Destroy (VOID);
	VOID Close (VOID);

	// ILayerInputHandler
	virtual BOOL ProcessMouseInput (LayerInput::Mouse eType, WPARAM wParam, LPARAM lParam, INT xView, INT yView, LRESULT& lResult);
	virtual BOOL ProcessKeyboardInput (LayerInput::Keyboard eType, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

private:
	BOOL MouseOverCloseButton (INT x, INT y);
};
