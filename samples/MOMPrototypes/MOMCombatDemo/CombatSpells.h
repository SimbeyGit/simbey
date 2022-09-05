#pragma once

#include "CombatScreen.h"

class CSummonSpell :
	public CAction,
	public ISpriteAnimationCompleted
{
	enum State
	{
		Idle,
		Spawn,
		Up,
		Done
	};

private:
	RSTRING m_rstrCaster;
	RSTRING m_rstrName;

	CSIFCanvas* m_pCanvas;
	sysint m_nLayer;
	CIsometricTranslator* m_pIsometric;
	INT m_xTile;
	INT m_yTile;

	State m_eState;

	INT m_cFrame;
	INT m_cTicks;
	CMovingObject* m_pObject;
	ISimbeyInterchangeAnimator* m_pOriginalAnimator;

	INT m_cLocks;

public:
	IMP_UNKNOWN(CAction)

	CSummonSpell (RSTRING rstrCaster, RSTRING rstrName, CCombatScreen* pScreen, CSIFCanvas* pCanvas, sysint nLayer, CIsometricTranslator* pIsometric, INT xTile, INT yTile);
	~CSummonSpell ();

	virtual VOID Update (VOID);

	// ISpriteAnimationCompleted
	virtual VOID OnSpriteAnimationCompleted (ISimbeyInterchangeSprite* pSprite, INT nAnimation);

private:
	HRESULT Start (VOID);
	HRESULT UpdateSpriteSize (VOID);
};

class CBaseIrreversibleDamageSpell :
	public CAction,
	public ISpriteAnimationCompleted
{
private:
	RSTRING m_rstrCaster;

	CSIFCanvas* m_pCanvas;
	sysint m_nLayer;
	CIsometricTranslator* m_pIsometric;
	INT m_xTile;
	INT m_yTile;

	BOOL m_fIdle;
	CMovingObject* m_pTarget;

public:
	CBaseIrreversibleDamageSpell (RSTRING rstrCaster, CCombatScreen* pScreen, CSIFCanvas* pCanvas, sysint nLayer, CIsometricTranslator* pIsometric, INT xTile, INT yTile);
	~CBaseIrreversibleDamageSpell ();

	virtual VOID Update (VOID);

	// ISpriteAnimationCompleted
	virtual VOID OnSpriteAnimationCompleted (ISimbeyInterchangeSprite* pSprite, INT nAnimation);

protected:
	virtual HRESULT GetAnimationAndSound (__out RSTRING* prstrAnimation, __out RSTRING* prstrSound, __out bool* pfUnderObject) = 0;
	virtual BOOL CheckDestroyUnit (CMovingObject* pTarget) = 0;

private:
	HRESULT Start (VOID);
};

class CCracksCall : public CBaseIrreversibleDamageSpell
{
public:
	IMP_UNKNOWN(CAction)

	CCracksCall (RSTRING rstrCaster, CCombatScreen* pScreen, CSIFCanvas* pCanvas, sysint nLayer, CIsometricTranslator* pIsometric, INT xTile, INT yTile);
	~CCracksCall ();

protected:
	virtual HRESULT GetAnimationAndSound (__out RSTRING* prstrAnimation, __out RSTRING* prstrSound, __out bool* pfSortSprites);
	virtual BOOL CheckDestroyUnit (CMovingObject* pTarget);
};

class CDisintegrate : public CBaseIrreversibleDamageSpell
{
public:
	IMP_UNKNOWN(CAction)

	CDisintegrate (RSTRING rstrCaster, CCombatScreen* pScreen, CSIFCanvas* pCanvas, sysint nLayer, CIsometricTranslator* pIsometric, INT xTile, INT yTile);
	~CDisintegrate ();

protected:
	virtual HRESULT GetAnimationAndSound (__out RSTRING* prstrAnimation, __out RSTRING* prstrSound, __out bool* pfSortSprites);
	virtual BOOL CheckDestroyUnit (CMovingObject* pTarget);
};

class CCastSpell
{
public:
	enum Target
	{
		EmptyTile = 1,
		FriendlyUnits = 2,
		EnemyUnits = 4
	};

protected:
	RSTRING m_rstrCaster;
	Target m_eTarget;

public:
	CCastSpell (RSTRING rstrCaster, Target eTarget);
	virtual ~CCastSpell ();

	HRESULT Query (INT xTile, INT yTile, __in_opt CMovingObject* pUnit);
	virtual HRESULT Cast (CCombatScreen* pScreen, CSIFCanvas* pCanvas, sysint nLayer, CIsometricTranslator* pIsometric, INT xTile, INT yTile, __in_opt CMovingObject* pUnit, __deref_out CAction** ppAction) = 0;
};

class CCastSummonSpell : public CCastSpell
{
private:
	RSTRING m_rstrUnit;

public:
	CCastSummonSpell (RSTRING rstrCaster, RSTRING rstrUnit);
	~CCastSummonSpell ();

	// CCastSpell
	virtual HRESULT Cast (CCombatScreen* pScreen, CSIFCanvas* pCanvas, sysint nLayer, CIsometricTranslator* pIsometric, INT xTile, INT yTile, __in_opt CMovingObject* pUnit, __deref_out CAction** ppAction);
};

class CCastTargetSpell : public CCastSpell
{
private:
	RSTRING m_rstrSpell;

public:
	CCastTargetSpell (RSTRING rstrCaster, RSTRING rstrSpell);
	~CCastTargetSpell ();

	// CCastSpell
	virtual HRESULT Cast (CCombatScreen* pScreen, CSIFCanvas* pCanvas, sysint nLayer, CIsometricTranslator* pIsometric, INT xTile, INT yTile, __in_opt CMovingObject* pUnit, __deref_out CAction** ppAction);
};
