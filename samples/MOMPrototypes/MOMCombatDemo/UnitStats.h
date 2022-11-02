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
	ISimbeyFontCollection* m_pFonts;
	CSIFSurface* m_pSurface;

	CSIFCanvas* m_pCanvas;
	CInteractiveLayer* m_pLayer;

	ISimbeyInterchangeFile* m_pSIF;
	ISimbeyInterchangeFile* m_pGenerated;
	ISimbeyInterchangeFileLayer* m_pBaseLayer;
	ISimbeyInterchangeFileLayer* m_pAbilitiesLayer;

	sysint m_idxBackground;
	INT m_xStats, m_yStats;

	IJSONArray* m_pAbilities;
	INT m_idxAbilities;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IPopupView)
	END_UNK_MAP

public:
	CUnitStats (CSIFSurface* pSurface, ISimbeyFontCollection* pFonts, IPopupHost* pScreen);
	~CUnitStats ();

	HRESULT Initialize (IJSONObject* pDef, IJSONObject* pStats, INT nLevel, ISimbeyInterchangeSprite* pUnitSprite, ISimbeyInterchangeFileLayer* pPortrait, INT cLiveHeads);
	VOID Close (VOID);

	// IPopupView
	virtual VOID Destroy (VOID);

	// ILayerInputHandler
	virtual BOOL ProcessMouseInput (LayerInput::Mouse eType, WPARAM wParam, LPARAM lParam, INT xView, INT yView, LRESULT& lResult);
	virtual BOOL ProcessKeyboardInput (LayerInput::Keyboard eType, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

private:
	HRESULT RenderUnitStats (ISimbeyInterchangeFileLayer* pTarget, ISimbeyInterchangeFileLayer* pBackground, IJSONObject* pDef, IJSONObject* pStats, INT nLevel, INT cLiveHeads);
	HRESULT RenderComponents (SIF_SURFACE* psifSurface, ISimbeyInterchangeFileLayer** prgComponentTiles, INT xBase, INT yBase, IJSONObject* pDef, IJSONObject* pStats, PCWSTR pcwzStat, ISimbeyInterchangeFile* pCombatStats, IJSONArray* pStatTypes);
	HRESULT RenderUpkeep (SIF_SURFACE* psifSurface, IJSONArray* pUpkeep, INT x, INT y, ISimbeyInterchangeFile* pCombatStats, IJSONArray* pStatTypes);
	HRESULT DrawComponents (SIF_SURFACE* psifSurface, ISimbeyInterchangeFileLayer* pComponent, ISimbeyInterchangeFileLayer* pIcon, INT& xBase, INT& yBase, INT& x, INT& y, INT& idxTile, INT cTiles);

	HRESULT RenderAbilities (IJSONObject* pDef);
};
