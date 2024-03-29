#pragma once

#include "BaseScreen.h"
#include "PopupHost.h"

#define	SPELL_BOOK_PAGE_WIDTH		124
#define	SPELL_BOOK_PAGE_HEIGHT		145

struct SPELL_PAGE
{
	RSTRING rstrGroup;
	INT cSpells;
	IJSONObject* prgSpells[6];
};

class CSpellBookPage :
	public CBaseUnknown,
	public ISimbeyInterchangeSprite
{
private:
	SPELL_PAGE* m_pPage;
	PVOID m_pvTitle;
	PVOID m_pvLabel;
	INT m_x, m_y;

	BYTE m_bits[SPELL_BOOK_PAGE_WIDTH * SPELL_BOOK_PAGE_HEIGHT * 4];

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(ISimbeyInterchangeSprite)
	END_UNK_MAP

	CSpellBookPage (SPELL_PAGE* pPage, PVOID pvTitle, PVOID pvLabel, INT x, INT y);
	~CSpellBookPage ();

	HRESULT Initialize (ISimbeyInterchangeFile* pSIF, INT nMagicPower);

	// ISimbeyInterchangeSprite
	IFACEMETHOD(SelectAnimation) (INT nAnimation, INT nFrame, INT cTicks);
	IFACEMETHOD_(VOID, GetCurrentAnimation) (__out INT* pnAnimation, __out INT* pnFrame, __out INT* pcTicks);
	IFACEMETHOD_(VOID, GetCurrentFrameSize) (__out INT* pxSize, __out INT* pySize);
	IFACEMETHOD_(VOID, GetCurrentHitBox) (__out RECT* prcHitBox);

	IFACEMETHOD_(VOID, UpdateFrameTick) (VOID);
	IFACEMETHOD_(VOID, SetPosition) (INT x, INT y);
	IFACEMETHOD_(VOID, GetPosition) (__out INT& x, __out INT& y);

	IFACEMETHOD_(BOOL, DrawMaskedToDIB24) (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24);
	IFACEMETHOD_(BOOL, DrawTileToDIB24) (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24, SIF_LINE_OFFSET* pslOffsets);
	IFACEMETHOD_(BOOL, DrawBlendedToDIB24) (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24);
	IFACEMETHOD_(BOOL, DrawColorizedToDIB24) (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24);

	IFACEMETHOD_(COLORREF, GetColorized) (VOID);
	IFACEMETHOD_(BOOL, SetColorized) (COLORREF cr);

	IFACEMETHOD(Clone) (__deref_out ISimbeyInterchangeSprite** ppSprite) { return E_NOTIMPL; }
	IFACEMETHOD_(VOID, GetFrameOffset) (__out INT& x, __out INT& y);
	IFACEMETHOD(GetFrameImage) (__out PBYTE* ppBits32P, __out INT* pnWidth, __out INT* pnHeight) { return E_NOTIMPL; }
	IFACEMETHOD(SetAnimationCompletedCallback) (__in_opt ISpriteAnimationCompleted* pCallback) { return E_NOTIMPL; }
	IFACEMETHOD(GetAnimator) (__deref_out ISimbeyInterchangeAnimator** ppAnimator) { return E_NOTIMPL; }

private:
	VOID DrawSpellFiller (INT xFillerStart, INT xFillerEnd, INT yFiller);
};

class CSpellBook :
	public CBaseUnknown,
	public IPopupView,
	public ILayerInputHandler
{
private:
	IPopupHost* m_pScreen;
	CSIFSurface* m_pSurface;
	IJSONObject* m_pWizard;
	ISimbeyFontCollection* m_pFonts;
	INT m_nMagicPower;

	PVOID m_pvTitle, m_pvLabel;

	INT m_xBook, m_yBook;

	TArray<SPELL_PAGE> m_aPages;
	SPELL_PAGE* m_pLeft;
	SPELL_PAGE* m_pRight;

	CSIFCanvas* m_pCanvas;
	CInteractiveLayer* m_pLayer;

	ISimbeyInterchangeFile* m_pSIF;

	sysint m_idxBackground;
	ISimbeyInterchangeSprite* m_pCloseButton;

	ISimbeyInterchangeSprite* m_pTurnLeft, *m_pTurnRight;
	CSpellBookPage* m_pLeftPage, *m_pRightPage;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IPopupView)
	END_UNK_MAP

public:
	CSpellBook (CSIFSurface* pSurface, IPopupHost* pScreen, IJSONObject* pWizard, ISimbeyFontCollection* pFonts, INT nMagicPower);
	~CSpellBook ();

	HRESULT Initialize (IJSONArray* pSpells);
	HRESULT UpdatePageNav (VOID);
	HRESULT StartPageNav (BOOL fForward);
	VOID RemovePageNav (VOID);
	VOID Close (VOID);

	// IPopupView
	virtual VOID Destroy (VOID);

	// ILayerInputHandler
	virtual BOOL ProcessMouseInput (LayerInput::Mouse eType, WPARAM wParam, LPARAM lParam, INT xView, INT yView, LRESULT& lResult);
	virtual BOOL ProcessKeyboardInput (LayerInput::Keyboard eType, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

private:
	HRESULT BuildPageList (IJSONArray* pSpellGroups);
	HRESULT AddSpellGroup (RSTRING rstrGroup, IJSONArray* pSpells);

	HRESULT CreateSpellBookPage (SPELL_PAGE* pPage, INT x, INT y, __deref_opt_out CSpellBookPage** ppPage = NULL);

	BOOL MouseOverSprite (INT x, INT y, ISimbeyInterchangeSprite* pSprite);
	BOOL FindSpellAt (SPELL_PAGE* pPage, INT x, INT y, INT xPage, INT yPage, __deref_out IJSONObject** ppSpell);
};
