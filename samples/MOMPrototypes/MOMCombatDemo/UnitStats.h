#pragma once

#include "BaseScreen.h"
#include "PopupHost.h"

class CUnitStats :
	public CBaseUnknown,
	public IPopupView,
	public ILayerInputHandler
{
private:
	IPopupHost* m_pScreen;
	CSIFSurface* m_pSurface;

	CSIFCanvas* m_pCanvas;
	CInteractiveLayer* m_pLayer;

	ISimbeyInterchangeFile* m_pSIF;

	sysint m_idxBackground;
	INT m_xStats, m_yStats;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IPopupView)
	END_UNK_MAP

public:
	CUnitStats (CSIFSurface* pSurface, IPopupHost* pScreen);
	~CUnitStats ();

	HRESULT Initialize (IJSONObject* pDef, IJSONObject* pStats, INT nLevel);
	VOID Close (VOID);

	// IPopupView
	virtual VOID Destroy (VOID);

	// ILayerInputHandler
	virtual BOOL ProcessMouseInput (LayerInput::Mouse eType, WPARAM wParam, LPARAM lParam, INT xView, INT yView, LRESULT& lResult);
	virtual BOOL ProcessKeyboardInput (LayerInput::Keyboard eType, WPARAM wParam, LPARAM lParam, LRESULT& lResult);
};
