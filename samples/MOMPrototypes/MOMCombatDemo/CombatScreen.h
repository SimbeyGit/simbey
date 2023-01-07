#pragma once

#include "Library\Core\BaseUnknown.h"
#include "Library\Core\RStrMap.h"
#include "Library\Core\RefArray.h"
#include "Library\Spatial\AStar2D.h"
#include "Published\QuadooVM.h"
#include "..\Shared\TileSet.h"
#include "BaseScreen.h"
#include "PopupHost.h"

interface IJSONValue;
interface IJSONObject;
interface IJSONArray;

class CSmoothingSystem;
class CTileRules;
class CTileSet;

class CCombatScreen;
class CSpellBook;
class CCastSpell;

class CAction;
class CMoveUnitAction;
class CProjectileAction;

class CProjectileData
{
public:
	ISimbeyInterchangeAnimator* m_pAnimator;
	IJSONObject* m_pProjectile;

public:
	CProjectileData (ISimbeyInterchangeAnimator* pAnimator, IJSONObject* pProjectile);
	~CProjectileData ();
};

class CUnitData
{
public:
	RSTRING m_rstrName;
	CSIFPackage* m_pUnitPackage;
	CSIFPackage* m_pImport;
	IJSONObject* m_pData;

public:
	CUnitData (RSTRING rstrName, CSIFPackage* pUnitPackage, CSIFPackage* pImport, IJSONObject* pData);
	~CUnitData ();
};

class CObject
{
protected:
	struct SPRITE_VIS
	{
		ISimbeyInterchangeSprite* pSprite;
		bool fVisible;
	};

	TArray<SPRITE_VIS> m_aSprites;
	INT m_cVisible;

public:
	INT m_xTile, m_yTile;

public:
	CObject (INT xTile, INT yTile);
	virtual ~CObject ();

	HRESULT AddSprite (ISimbeyInterchangeSprite* pSprite, bool fVisible);
	HRESULT CloneSprite (__deref_out ISimbeyInterchangeSprite** ppSprite);
	HRESULT UpdateVisList (CSIFCanvas* pCanvas, sysint nLayer, INT cVisible, bool fDeferredRemoval = false);
	HRESULT GetFirstVisibleSprite (__deref_out ISimbeyInterchangeSprite** ppSprite);
	HRESULT GetLastVisibleSprite (__deref_out ISimbeyInterchangeSprite** ppSprite);
	VOID ShowOrHide (CSIFCanvas* pCanvas, sysint nLayer, BOOL fShow);

	virtual bool CanBeMoved (VOID) { return false; }
	virtual FMOD::Sound* GetMovingSound (VOID) { return NULL; }
	virtual FMOD::Sound* GetMeleeSound (VOID) { return NULL; }
	virtual FMOD::Sound* GetRangeSound (VOID) { return NULL; }
	virtual VOID UpdateDirection (INT nDirection) {}
	virtual VOID UpdateMovement (INT nMoveFrame) {}
	virtual VOID SelectBaseAnimation (VOID) {}
	virtual VOID SelectMoveAnimation (VOID) {}
	virtual VOID SelectAttackAnimation (VOID) {}
	virtual VOID SelectRangeAnimation (VOID) {}

	VOID ShiftObject (INT xShift, INT yShift);
};

class CMovingObject : public CObject
{
public:
	IJSONObject* m_pDef;
	CSIFPackage* m_pPackage;
	RSTRING m_rstrOwner;
	INT m_nDirection;
	INT m_nLevel;
	INT (*m_pfnBaseAnimation)(INT);
	FMOD::Sound* m_pMove;
	FMOD::Sound* m_pMelee;
	FMOD::Sound* m_pRange;

public:
	CMovingObject (IJSONObject* pDef, CSIFPackage* pPackage, RSTRING rstrOwner, INT xTile, INT yTile, INT nDirection, INT nLevel, INT (*pfnBaseAnimation)(INT), FMOD::Sound* pMove, FMOD::Sound* pMelee, FMOD::Sound* pRange);
	~CMovingObject ();

	HRESULT GetObjectDef (__deref_out IJSONObject** ppDef);
	HRESULT LoadPortrait (__deref_out ISimbeyInterchangeFileLayer** ppPortrait);

	virtual bool CanBeMoved (VOID) { return true; }
	virtual FMOD::Sound* GetMovingSound (VOID) { return m_pMove; }
	virtual FMOD::Sound* GetMeleeSound (VOID) { return m_pMelee; }
	virtual FMOD::Sound* GetRangeSound (VOID) { return m_pRange; }
	virtual VOID UpdateDirection (INT nDirection);
	virtual VOID UpdateMovement (INT nMoveFrame);
	virtual VOID SelectBaseAnimation (VOID);
	virtual VOID SelectMoveAnimation (VOID);
	virtual VOID SelectAttackAnimation (VOID);
	virtual VOID SelectRangeAnimation (VOID);

	VOID FaceObject (CObject* pObject);
	HRESULT CreateProjectiles (CIsometricTranslator* pIsometric, CSIFCanvas* pMain, sysint nUnitLayer, TRStrMap<CProjectileData*>* pmapProjectiles, CProjectileAction* pAction);
	HRESULT CreateBloodOverlays (CCombatScreen* pCombat, CSIFCanvas* pMain, sysint nLayer, ISimbeyInterchangeAnimator* pBlood, INT nAnimation);

	bool HasSameOwner (CMovingObject* pOther);
	bool CanRangeAttack (CMovingObject* pOther);
	bool CanMeleeAttack (CMovingObject* pOther);

	HRESULT ReplaceAnimator (ISimbeyInterchangeAnimator* pAnimator, CSIFCanvas* pMain, sysint nUnitLayer);

	static INT StandingAnimation (INT nDirection) { return nDirection * 4 + 2; }
	static INT MovingAnimation (INT nDirection) { return nDirection * 4 + 1; }
	static INT AttackingAnimation (INT nDirection) { return nDirection * 4 + 0; }
	static INT RangeAttackAnimation (INT nDirection) { return nDirection * 4 + 3; }
};

class CAction : public CBaseUnknown
{
public:
	CCombatScreen* m_pScreen;

public:
	IMP_BASE_UNKNOWN

	EMPTY_UNK_MAP

	CAction (CCombatScreen* pScreen) : m_pScreen(pScreen) {}
	virtual VOID Update (VOID) = 0;
};

class CMoveUnitAction : public CAction
{
public:
	CSIFCanvas* m_pMain;

	CIsometricTranslator* m_pIsometric;

	CObject* m_pSelected;

	TArray<POINT> m_aPath;
	INT m_cMoveTicks;
	INT m_nMoveFrame;
	INT m_nMoveIndex;

public:
	CMoveUnitAction (CCombatScreen* pWindow, CSIFCanvas* pMain, CIsometricTranslator* pIsometric, CObject* pSelected);
	~CMoveUnitAction ();

	virtual VOID Update (VOID);

	VOID SelectNextMovement (VOID);
};

class CMergeUnitAction : public CAction
{
	enum State
	{
		Idle,
		Down,
		Up
	};

public:
	CSIFCanvas* m_pMain;
	CIsometricTranslator* m_pIsometric;
	CObject* m_pSelected;
	sysint m_nUnitLayer;

	ISimbeyInterchangeAnimator* m_pOriginalAnimator;

	INT m_xTarget, m_yTarget;
	INT m_xOffset, m_yOffset;
	INT m_cTicks;
	INT m_cMergeShift;

	State m_eState;

public:
	CMergeUnitAction (CCombatScreen* pWindow, CSIFCanvas* pMain, CIsometricTranslator* pIsometric, CObject* pSelected, sysint nUnitLayer, INT xTarget, INT yTarget);
	~CMergeUnitAction ();

	virtual VOID Update (VOID);

private:
	HRESULT UpdateSpriteSize (VOID);
};

class CProjectileAction : public CAction
{
public:
	CMovingObject* m_pSource;
	CMovingObject* m_pTarget;
	BOOL m_fImpacted;

	TArray<ISimbeyInterchangeSprite*> m_aSprites;
	TArray<POINT> m_aStarts;

	DOUBLE m_vX, m_vY;
	DOUBLE m_rx, m_ry;
	INT m_xStart, m_yStart;
	INT m_xTarget, m_yTarget;
	INT m_cSteps;

public:
	CProjectileAction (CCombatScreen* pWindow, CMovingObject* pSource, CMovingObject* pTarget);
	~CProjectileAction ();

	HRESULT AddProjectile (ISimbeyInterchangeSprite* pSprite, INT xTarget, INT yTarget);
	VOID SetUnitVector (INT x1, INT y1, INT x2, INT y2, DOUBLE rVelocity);

	virtual VOID Update (VOID);
};

class CEffect : public CAction
{
public:
	CSIFCanvas* m_pCanvas;
	sysint m_nLayer;
	ISimbeyInterchangeSprite* m_pSprite;
	INT m_cTicksRemaining;

public:
	CEffect (CCombatScreen* pScreen, CSIFCanvas* pCanvas, sysint nLayer, ISimbeyInterchangeSprite* pSprite, INT cTicksRemaining);
	~CEffect ();

	virtual VOID Update (VOID);
};

class CUnitUpdater : public CAction
{
public:
	CMovingObject* m_pUnit;
	CSIFCanvas* m_pCanvas;
	sysint m_nLayer;
	INT m_cVisible;
	INT m_cTicksRemaining;

public:
	CUnitUpdater (CCombatScreen* pScreen, CMovingObject* pUnit, CSIFCanvas* pCanvas, sysint nLayer, INT cVisible, INT cTicksRemaining);
	~CUnitUpdater ();

	virtual VOID Update (VOID);
};

class CCombatBar :
	public CBaseUnknown,
	public ILayerInputHandler
{
private:
	CCombatScreen* m_pScreen;
	CSIFSurface* m_pSurface;
	CSIFCanvas* m_pBar;

	ISimbeyInterchangeAnimator* m_pCombatUI;
	ISimbeyInterchangeAnimator* m_pUnitStats;
	ISimbeyInterchangeFileFont* m_pCombatBarFont;

	sysint m_nBaseLayer;
	sysint m_nStatsLayer;

	ISimbeyInterchangeSprite* m_pButtons[6];
	INT m_idxPressed;

	RSTRING m_rstrLeft, m_rstrRight;

public:
	IMP_BASE_UNKNOWN

	EMPTY_UNK_MAP

	CCombatBar ();
	~CCombatBar ();

	HRESULT Initialize (CCombatScreen* pWindow, CSIFSurface* pSurface, const RECT* pcrc);
	VOID SetNames (RSTRING rstrLeft, RSTRING rstrRight);
	RSTRING GetLeftName (VOID) { return m_rstrLeft; }
	RSTRING GetRightName (VOID) { return m_rstrRight; }
	HRESULT Load (ISimbeyInterchangeAnimator* pAnimator, ISimbeyInterchangeAnimator* pUnitStats, ISimbeyInterchangeFileFont* pYellowFont, ISimbeyInterchangeFileFont* pSmallYellowFont);
	HRESULT Update (CMovingObject* pSelected);
	VOID Clear (VOID);

	// ILayerInputHandler
	virtual BOOL ProcessMouseInput (LayerInput::Mouse eType, WPARAM wParam, LPARAM lParam, INT xView, INT yView, LRESULT& lResult);
	virtual BOOL ProcessKeyboardInput (LayerInput::Keyboard eType, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

	inline CSIFCanvas* GetCanvas (VOID) { return m_pBar; }

private:
	HRESULT PlaceStat (INT x, INT y, IJSONObject* pSource, PCWSTR pcwzField);

	static INT GetButtonFromPoint (INT xView, INT yView);
};

class CCombatScreen :
	public CBaseScreen,
	public ILayerInputHandler,
	public IPopupHost,
	public IAStarCallback2D
{
protected:
	CSIFCanvas* m_pMain;
	CSIFCanvas* m_pMouse;
	CSIFCanvas* m_pStats;

	TRStrMap<CSmoothingSystem*> m_mapSmoothingSystems;
	CTileRules* m_pTileRules;
	IJSONArray* m_pGenerators;
	TRStrMap<CTileSet*> m_mapCombatTiles;
	ISimbeyInterchangeFile* m_pFeatures;

	CIsometricTranslator m_Isometric;

	IJSONObject* m_pWizard;
	IJSONObject* m_pPlacements;
	IQuadooObject* m_pCombat;
	MIDI::CFile m_midiFile;

	sysint m_nTileEffectsLayer;
	sysint m_nUnitLayer;
	ISimbeyInterchangeSprite* m_pMoveTo;
	ISimbeyInterchangeSprite* m_pSelectedTile;

	TRStrMap<CProjectileData*> m_mapProjectiles;
	TRStrMap<IJSONObject*> m_mapCombatAnimations;
	TRStrMap<CUnitData*> m_mapUnits;
	IJSONArray* m_pCombatStats;
	IJSONArray* m_pCombatMelee;

	TArray<CObject*> m_aRemoved;
	TArray<CObject*> m_aObjects;
	CObject* m_pSelected;

	TRefArray<CAction> m_aActions;
	CCastSpell* m_pCastSpell;

	ISimbeyInterchangeFileFont* m_pYellowFont;
	ISimbeyInterchangeFileFont* m_pSmallYellowFont;
	ISimbeyInterchangeFileFont* m_pCombatBarFont;
	ISimbeyFontCollection* m_pFonts;

	ISimbeyInterchangeSprite* m_pMoveType;

	INT m_xHoverTile;
	INT m_yHoverTile;

	ISimbeyInterchangeAnimator* m_pUnitStats;
	ISimbeyInterchangeAnimator* m_pBlood;

	CCombatBar* m_pCombatBar;
	TRefArray<IPopupView> m_aViews;

	ULONG m_idxExpandStats;
	ULONG m_idxCreateCombatObject;
	ULONG m_idxApplyHealing;
	ULONG m_idxGetLiveHeads;

	CSIFCanvas* m_pBackground;
	DOUBLE m_dblBackground;
	ISimbeyInterchangeSprite* m_pCornerA, *m_pCornerB, *m_pCornerC, *m_pCornerD;

public:
	IMP_BASE_UNKNOWN

	CCombatScreen (IScreenHost* pHost, CInteractiveSurface* pSurface, CSIFPackage* pPackage, ISimbeyInterchangeFileFont* pYellowFont, ISimbeyInterchangeFileFont* pSmallYellowFont, IJSONObject* pWizard, IJSONObject* pPlacements);
	virtual ~CCombatScreen ();

	HRESULT Initialize (VOID);

	VOID PlaySound (FMOD::Sound* pSound);
	HRESULT FindSound (RSTRING rstrName, FMOD::Sound** ppSound) { return m_pHost->FindSound(rstrName, ppSound); }

	VOID ClearAction (CAction* pAction);
	VOID RemoveUnitSprite (ISimbeyInterchangeSprite* pSprite);
	VOID SortUnitLayer (VOID);
	VOID BringSpriteToTop (ISimbeyInterchangeSprite* pSprite, CMovingObject* pUnit);
	HRESULT GetDataStat (RSTRING rstrType, __deref_out IJSONValue** ppvStat);
	HRESULT FindUnitAbility (IJSONObject* pUnit, RSTRING rstrAbility, __deref_out IJSONObject** ppAbility);
	HRESULT PlaceStat (CSIFCanvas* pCanvas, ISimbeyInterchangeFileFont* pFont, sysint nLayer, INT x, INT y, IJSONObject* pSource, PCWSTR pcwzField, bool fStatSprite = true);
	HRESULT AddEffect (CSIFCanvas* pCanvas, sysint nLayer, ISimbeyInterchangeSprite* pSprite, ISimbeyInterchangeSprite* pAddAfter, INT cTicksRemaining);

	HRESULT PerformAttack (CMovingObject* pSource, CMovingObject* pTarget, BOOL fRange);
	HRESULT PerformSpellAttack (IJSONObject* pAbilities, IJSONObject* pWeapon, CMovingObject* pTarget);
	HRESULT InvokeAttackMethod (__in_opt CMovingObject* pSource, CMovingObject* pTarget, RSTRING rstrAttackMethod, BOOL fUpdateSource, QuadooVM::QVPARAMS* pqvParams);

	HRESULT ShowBloodSpatter (IJSONObject* pResult, PCWSTR pcwzImpact, CMovingObject* pUnit);
	HRESULT UpdateUnitAsync (IJSONObject* pResult, PCWSTR pcwzFigures, CMovingObject* pUnit);
	HRESULT RemoveUnitFromBoard (CMovingObject* pUnit);
	HRESULT ExpandStats (IJSONObject* pBase, INT nLevel, __deref_out IJSONObject** ppStats);
	HRESULT GetHealthPct (IJSONObject* pUnit, IJSONObject* pStats, __out DOUBLE* pdblHealth);
	HRESULT GetLiveHeads (IJSONObject* pUnit, INT nLevel, __out INT* pcLiveHeads);
	HRESULT UpdateStatsPanel (VOID);
	HRESULT ShowSpellBook (VOID);
	HRESULT ShowSelectedUnitInfo (VOID);

	// IScreen
	virtual VOID OnDestroy (VOID);
	virtual VOID OnUpdateFrame (VOID);
	virtual VOID OnNotifyFinished (BOOL fCompleted);
	virtual VOID OnChangeActive (BOOL fActive);

	// ILayerInputHandler
	virtual BOOL ProcessMouseInput (LayerInput::Mouse eType, WPARAM wParam, LPARAM lParam, INT xView, INT yView, LRESULT& lResult);
	virtual BOOL ProcessKeyboardInput (LayerInput::Keyboard eType, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

	// IPopupHost
	virtual HRESULT AddPopup (IPopupView* pView);
	virtual CSIFPackage* GetPackage (VOID) { return m_pPackage; }
	virtual HRESULT RemovePopup (IPopupView* pView);
	virtual VOID UpdateMouse (LPARAM lParam);
	virtual HRESULT AttachSpellCaster (CCastSpell* pSpell);

protected:
	// IAStarCallback2D
	virtual BOOL GetPathValue (INT x, INT y, INT xFrom, INT yFrom, __out INT* pnValue);

	HRESULT StartMergingAction (INT xTile, INT yTile);
	HRESULT StartMovingAction (INT xTile, INT yTile);

	HRESULT UpdateStatsPanel (INT xTile, INT yTile);
	HRESULT UpdateUnitStats (sysint nLayer, IJSONObject* pUnit, INT nLevel);
	HRESULT PlaceStat (sysint nLayer, INT x, INT y, IJSONObject* pSource, PCWSTR pcwzField, bool fStatSprite = true);

	HRESULT LoadData (VOID);
	HRESULT LoadUnits (CSIFPackage* pSubPackage);
	HRESULT LoadProjectiles (VOID);
	HRESULT LoadSprites (VOID);
	HRESULT LoadCombatStats (VOID);
	HRESULT LoadMusic (VOID);

	HRESULT AllocateCombatWorld (__deref_out MAPTILE** ppWorld);
	HRESULT GenerateCombatWorld (MAPTILE* pWorld, IJSONObject* pGenerator);

	HRESULT PlaceTile (CSIFCanvas* pCanvas, INT xTile, INT yTile, sysint nLayer, ISimbeyInterchangeAnimator* pAnimator, INT nAnimation, __deref_out_opt ISimbeyInterchangeSprite** ppSprite);
	HRESULT PlaceTile (CSIFCanvas* pCanvas, INT xTile, INT yTile, sysint nLayer, CTile* pTile, __deref_out_opt ISimbeyInterchangeSprite** ppSprite);
	HRESULT AddStaticObject (INT xTile, INT yTile, sysint nLayer, ISimbeyInterchangeAnimator* pAnimator, INT nAnimation);
	HRESULT AddMovingObject (IJSONObject* pDef, CSIFPackage* pPackage, RSTRING rstrOwner, INT xTile, INT yTile, sysint nLayer, ISimbeyInterchangeAnimator* pAnimator, INT nDirection, INT nLevel, INT (*pfnBaseAnimation)(INT), FMOD::Sound* pMove, FMOD::Sound* pMelee, FMOD::Sound* pRange, __deref_opt_out CMovingObject** ppObject = NULL);
	HRESULT UpdatePositionInJSON (CMovingObject* pObject);

	HRESULT CreateRandomAbility (IJSONArray* pAbilities, RSTRING rstrRandom, __deref_out IJSONValue** ppvAbility);
	HRESULT AssignRandomStats (CUnitData* pUnitData, __deref_out IJSONObject** ppUnit);

public:
	HRESULT PlaceUnit (RSTRING rstrName, RSTRING rstrOwner, INT xTile, INT yTile, sysint nLayer, INT nDirection, INT nLevel, COLORREF crColorize, bool fEnchanted, __deref_opt_out CMovingObject** ppObject = NULL);
	CObject* FindObject (INT xTile, INT yTile);
	VOID DestroyUnit (CMovingObject* pObject);
	HRESULT ConfigureBackground (ISimbeyInterchangeFile* pLayers, INT idxLayer, INT x, INT y, __deref_out ISimbeyInterchangeSprite** ppSprite);
	HRESULT ApplyHealing (CMovingObject* pTarget, INT nHealingPoints);

protected:
	HRESULT PlaceObjects (sysint nLayer, PCWSTR pcwzGroup, RSTRING rstrOwner, const POINT* prgptPlace, INT nDirection, COLORREF crColorize);
	HRESULT PlaceUnitsAndBuildings (sysint nLayer);

	static HRESULT ApplyEnchantingEffect (SIF_SURFACE* pSurface, const COLORREF* pcrgColors, INT cColors, INT nOffset);
	static HRESULT CreateEnchantedAnimator (CSIFPackage* pPackage, IJSONObject* pDef, PCWSTR pcwzSIF, __deref_out ISimbeyInterchangeAnimator** ppAnimator);
	static HRESULT RecolorizeAnimator (COLORREF crColorize, ISimbeyInterchangeAnimator* pAnimator, __deref_out ISimbeyInterchangeAnimator** ppNew);
	static COLORREF HSLToRGB (DOUBLE h, DOUBLE sl, DOUBLE l);
};
