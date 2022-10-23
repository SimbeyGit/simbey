#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\MemoryStream.h"
#include "Library\Util\Formatting.h"
#include "Library\Util\StreamHelpers.h"
#include "Published\JSON.h"
#include "CombatScreen.h"
#include "CombatSpells.h"
#include "SpellBook.h"
#include "UnitStats.h"

#define	TILE_WIDTH			32
#define	TILE_HEIGHT			16

#define	MAP_WIDTH			32
#define	MAP_HEIGHT			32

#define	MOVE_TICKS			7

#define	CBT_BUTTON_WIDTH	42
#define	CBT_BUTTON_HEIGHT	16

#define	ATTACK_ANI_TICKS	55

const POINT c_ptMoveOffset[8][8] =
{
	{	// Up (0)
		{ 0, -2 },
		{ 0, -2 },
		{ 0, -2 },
		{ 0, -2 },
		{ 0, -2 },
		{ 0, -2 },
		{ 0, -2 },
		{ 0, -2 }
	},
	{	// Up/Right (1)
		{ 2, -1 },
		{ 2, -1 },
		{ 2, -1 },
		{ 2, -1 },
		{ 2, -1 },
		{ 2, -1 },
		{ 2, -1 },
		{ 2, -1 }
	},
	{	// Right (2)
		{ 4, 0 },
		{ 4, 0 },
		{ 4, 0 },
		{ 4, 0 },
		{ 4, 0 },
		{ 4, 0 },
		{ 4, 0 },
		{ 4, 0 }
	},
	{	// Down/Right (3)
		{ 2, 1 },
		{ 2, 1 },
		{ 2, 1 },
		{ 2, 1 },
		{ 2, 1 },
		{ 2, 1 },
		{ 2, 1 },
		{ 2, 1 }
	},
	{	// Down (4)
		{ 0, 2 },
		{ 0, 2 },
		{ 0, 2 },
		{ 0, 2 },
		{ 0, 2 },
		{ 0, 2 },
		{ 0, 2 },
		{ 0, 2 }
	},
	{	// Down/Left (5)
		{ -2, 1 },
		{ -2, 1 },
		{ -2, 1 },
		{ -2, 1 },
		{ -2, 1 },
		{ -2, 1 },
		{ -2, 1 },
		{ -2, 1 }
	},
	{	// Left (6)
		{ -4, 0 },
		{ -4, 0 },
		{ -4, 0 },
		{ -4, 0 },
		{ -4, 0 },
		{ -4, 0 },
		{ -4, 0 },
		{ -4, 0 }
	},
	{	// Up/Left (7)
		{ -2, -1 },
		{ -2, -1 },
		{ -2, -1 },
		{ -2, -1 },
		{ -2, -1 },
		{ -2, -1 },
		{ -2, -1 },
		{ -2, -1 }
	}
};

const POINT c_ptFigureOffset1[1] =
{
	{ 0, 0 }
};

const POINT c_ptFigureOffset2[2] =
{
	{ 3, -2 },
	{ -3, 2 }
};

const POINT c_ptFigureOffset3[3] =
{
	{ 0, -5 },
	{ 5, 4 },
	{ -5, 4 }
};

const POINT c_ptFigureOffset4[4] =
{
	{ 0, -5 },
	{ -6, 0 },
	{ 6, 0 },
	{ 0, 5 }
};

const POINT c_ptFigureOffset5[5] =
{
	{ -7, -6 },
	{ 7, -6 },
	{ 0, 0 },
	{ -7, 6 },
	{ 7, 6 }
};

const POINT c_ptFigureOffset6[6] =
{
	{ -8, 0 },
	{ -3, -4 },
	{ 2, -8 },
	{ 1, 5 },
	{ 6, 1 },
	{ 11, -1 }
};

const POINT c_ptFigureOffset7[7] =
{
	{ -8, 0 },
	{ -3, -4 },
	{ 2, -8 },
	{ 0, 0 },
	{ 1, 5 },
	{ 6, 1 },
	{ 11, -1 }
};

const POINT c_ptFigureOffset8[8] =
{
	{ -8, 0 },
	{ -3, -4 },
	{ 2, -8 },
	{ -2, 1 },
	{ 2, -1 },
	{ 1, 5 },
	{ 6, 1 },
	{ 11, -1 }
};

const POINT* c_ptFigureOffsets[8] =
{
	c_ptFigureOffset1,
	c_ptFigureOffset2,
	c_ptFigureOffset3,
	c_ptFigureOffset4,
	c_ptFigureOffset5,
	c_ptFigureOffset6,
	c_ptFigureOffset7,
	c_ptFigureOffset8
};

const POINT c_rgDefense[] =
{
	{ 12, 18 },
	{ 12, 19 },
	{ 12, 17 },
	{ 12, 20 },
	{ 11, 18 },
	{ 11, 19 },
	{ 11, 17 },
	{ 11, 20 },
	{ 12, 16 },
	{ 11, 16 }
};

const POINT c_rgOffense[] =
{
	{ 20, 18 },
	{ 20, 19 },
	{ 20, 17 },
	{ 20, 20 },
	{ 21, 18 },
	{ 21, 19 },
	{ 21, 17 },
	{ 21, 20 },
	{ 20, 16 },
	{ 21, 16 }
};

const RECT c_rcFloatingTiles =
{
	8, 15,
	14, 21
};

const struct
{
	PCWSTR pcwzAbility;
	PCWSTR pcwzRandomType;
	BOOL fAllowDouble;
} c_rgRandom[] =
{
	{ L"Agility", L"any", TRUE },
	{ L"Arcane Power", L"mage", TRUE },
	{ L"Armsmaster", L"fighter", TRUE },
	{ L"Blademaster", L"fighter", TRUE },
	{ L"Caster", L"mage", FALSE },
	{ L"Charmed", L"any", FALSE },
	{ L"Constitution", L"fighter", TRUE },
	{ L"Leadership", L"fighter", TRUE },
	{ L"Legendary", L"fighter", TRUE },
	{ L"Lucky", L"any", FALSE },
	{ L"Might", L"fighter", TRUE },
	{ L"Noble", L"any", FALSE },
	{ L"Prayermaster", L"mage", TRUE },
	{ L"Sage", L"mage", TRUE }
};

const RECT c_rgButtons[] =
{
	{ 230, 5, 230 + CBT_BUTTON_WIDTH, 5 + CBT_BUTTON_HEIGHT },
	{ 272, 5, 272 + CBT_BUTTON_WIDTH, 5 + CBT_BUTTON_HEIGHT },
	{ 230, 21, 230 + CBT_BUTTON_WIDTH, 21 + CBT_BUTTON_HEIGHT },
	{ 272, 21, 272 + CBT_BUTTON_WIDTH, 21 + CBT_BUTTON_HEIGHT },
	{ 230, 37, 230 + CBT_BUTTON_WIDTH, 37 + CBT_BUTTON_HEIGHT },
	{ 272, 37, 272 + CBT_BUTTON_WIDTH, 37 + CBT_BUTTON_HEIGHT }
};

CProjectileData::CProjectileData (ISimbeyInterchangeAnimator* pAnimator, IJSONObject* pProjectile)
{
	SetInterface(m_pAnimator, pAnimator);
	SetInterface(m_pProjectile, pProjectile);
}

CProjectileData::~CProjectileData ()
{
	SafeRelease(m_pAnimator);
	SafeRelease(m_pProjectile);
}

CUnitData::CUnitData (RSTRING rstrName, CSIFPackage* pUnitPackage, CSIFPackage* pImport, IJSONObject* pData)
{
	RStrSet(m_rstrName, rstrName);
	SetInterface(m_pUnitPackage, pUnitPackage);
	SetInterface(m_pImport, pImport);
	SetInterface(m_pData, pData);
}

CUnitData::~CUnitData ()
{
	SafeRelease(m_pData);
	SafeRelease(m_pImport);
	SafeRelease(m_pUnitPackage);
	RStrRelease(m_rstrName);
}

CObject::CObject (INT xTile, INT yTile) :
	m_cVisible(0),
	m_xTile(xTile),
	m_yTile(yTile)
{
}

CObject::~CObject ()
{
	for(sysint i = 0; i < m_aSprites.Length(); i++)
		m_aSprites[i].pSprite->Release();
}

HRESULT CObject::AddSprite (ISimbeyInterchangeSprite* pSprite, bool fVisible)
{
	HRESULT hr;
	SPRITE_VIS item = { pSprite, fVisible };

	Check(m_aSprites.Append(&item));
	pSprite->AddRef();
	if(fVisible)
		m_cVisible++;

Cleanup:
	return hr;
}

HRESULT CObject::CloneSprite (__deref_out ISimbeyInterchangeSprite** ppSprite)
{
	return m_aSprites[0].pSprite->Clone(ppSprite);
}

HRESULT CObject::UpdateVisList (CSIFCanvas* pCanvas, sysint nLayer, INT cVisible, bool fDeferredRemoval)
{
	HRESULT hr = S_FALSE;

	for(sysint i = 0; i < m_aSprites.Length(); i++)
	{
		bool fUpdate = i < cVisible;
		SPRITE_VIS& sprite = m_aSprites[i];
		if(sprite.fVisible != fUpdate)
		{
			sprite.fVisible = fUpdate;
			if(fUpdate)
			{
				ISimbeyInterchangeSprite* pPrev = 0 < i ? m_aSprites[i - 1].pSprite : NULL;
				Check(pCanvas->AddSpriteAfter(nLayer, sprite.pSprite, pPrev, NULL));
			}
			else if(fDeferredRemoval)
				Check(pCanvas->RemoveSpriteLater(nLayer, sprite.pSprite));
			else
				Check(pCanvas->RemoveSprite(nLayer, sprite.pSprite));
		}
	}

Cleanup:
	return hr;
}

HRESULT CObject::GetFirstVisibleSprite (__deref_out ISimbeyInterchangeSprite** ppSprite)
{
	for(sysint i = 0; i < m_aSprites.Length(); i++)
	{
		if(m_aSprites[i].fVisible)
		{
			SetInterface(*ppSprite, m_aSprites[i].pSprite);
			return S_OK;
		}
	}
	return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
}

HRESULT CObject::GetLastVisibleSprite (__deref_out ISimbeyInterchangeSprite** ppSprite)
{
	for(sysint i = m_aSprites.Length() - 1; i >= 0; i--)
	{
		if(m_aSprites[i].fVisible)
		{
			SetInterface(*ppSprite, m_aSprites[i].pSprite);
			return S_OK;
		}
	}
	return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
}

VOID CObject::ShowOrHide (CSIFCanvas* pCanvas, sysint nLayer, BOOL fShow)
{
	for(sysint i = 0; i < m_aSprites.Length(); i++)
	{
		if(static_cast<BOOL>(m_aSprites[i].fVisible) != fShow)
		{
			SPRITE_VIS& sprite = m_aSprites[i];
			if(fShow)
				pCanvas->AddSprite(nLayer, sprite.pSprite, NULL);
			else
				pCanvas->RemoveSprite(nLayer, sprite.pSprite);
			sprite.fVisible = !sprite.fVisible;
		}
	}
}

VOID CObject::ShiftObject (INT xShift, INT yShift)
{
	for(sysint i = 0; i < m_aSprites.Length(); i++)
	{
		INT x, y;
		ISimbeyInterchangeSprite* pSprite = m_aSprites[i].pSprite;

		pSprite->GetPosition(x, y);
		pSprite->SetPosition(x + xShift, y + yShift);
	}
}

CMovingObject::CMovingObject (IJSONObject* pDef,
	RSTRING rstrOwner,
	INT xTile, INT yTile,
	INT nDirection, INT nLevel,
	INT (*pfnBaseAnimation)(INT),
	FMOD::Sound* pMove, FMOD::Sound* pMelee, FMOD::Sound* pRange) :
	CObject(xTile, yTile),
	m_nDirection(nDirection),
	m_nLevel(nLevel),
	m_pfnBaseAnimation(pfnBaseAnimation),
	m_pMove(pMove),
	m_pMelee(pMelee),
	m_pRange(pRange)
{
	SetInterface(m_pDef, pDef);
	RStrSet(m_rstrOwner, rstrOwner);
}

CMovingObject::~CMovingObject ()
{
	RStrRelease(m_rstrOwner);
	SafeRelease(m_pDef);
}

HRESULT CMovingObject::GetObjectDef (__deref_out IJSONObject** ppDef)
{
	SetInterface(*ppDef, m_pDef);
	return S_OK;
}

VOID CMovingObject::UpdateDirection (INT nDirection)
{
	if(nDirection != m_nDirection)
	{
		m_nDirection = nDirection;
		for(sysint i = 0; i < m_aSprites.Length(); i++)
			m_aSprites[i].pSprite->SelectAnimation(m_pfnBaseAnimation(nDirection));
	}
}

VOID CMovingObject::UpdateMovement (INT nMoveFrame)
{
	ShiftObject(c_ptMoveOffset[m_nDirection][nMoveFrame].x, c_ptMoveOffset[m_nDirection][nMoveFrame].y);
}

VOID CMovingObject::SelectBaseAnimation (VOID)
{
	INT nSelect = m_pfnBaseAnimation(m_nDirection), nAnimation, nFrame, cTicks;
	for(sysint i = 0; i < m_aSprites.Length(); i++)
	{
		ISimbeyInterchangeSprite* pSprite = m_aSprites[i].pSprite;
		pSprite->GetCurrentAnimation(&nAnimation, &nFrame, &cTicks);
		if(nSelect != nAnimation)
			pSprite->SelectAnimation(nSelect);
	}
}

VOID CMovingObject::SelectMoveAnimation (VOID)
{
	INT nSelect = MovingAnimation(m_nDirection), nAnimation, nFrame, cTicks;
	for(sysint i = 0; i < m_aSprites.Length(); i++)
	{
		ISimbeyInterchangeSprite* pSprite = m_aSprites[i].pSprite;
		pSprite->GetCurrentAnimation(&nAnimation, &nFrame, &cTicks);
		if(nSelect != nAnimation)
			pSprite->SelectAnimation(nSelect);
	}
}

VOID CMovingObject::SelectAttackAnimation (VOID)
{
	INT nSelect = AttackingAnimation(m_nDirection), nAnimation, nFrame, cTicks;
	for(sysint i = 0; i < m_aSprites.Length(); i++)
	{
		ISimbeyInterchangeSprite* pSprite = m_aSprites[i].pSprite;
		pSprite->GetCurrentAnimation(&nAnimation, &nFrame, &cTicks);
		if(nSelect != nAnimation)
			pSprite->SelectAnimation(nSelect);
	}
}

VOID CMovingObject::SelectRangeAnimation (VOID)
{
	INT nSelect = RangeAttackAnimation(m_nDirection), nAnimation, nFrame, cTicks;
	for(sysint i = 0; i < m_aSprites.Length(); i++)
	{
		ISimbeyInterchangeSprite* pSprite = m_aSprites[i].pSprite;
		pSprite->GetCurrentAnimation(&nAnimation, &nFrame, &cTicks);
		if(nSelect != nAnimation)
			pSprite->SelectAnimation(nSelect);
	}
}

VOID CMovingObject::FaceObject (CObject* pObject)
{
	if(m_xTile > pObject->m_xTile)
	{
		if(m_yTile > pObject->m_yTile)
			UpdateDirection(0);
		else if(m_yTile < pObject->m_yTile)
			UpdateDirection(6);
		else
			UpdateDirection(7);
	}
	else if(m_xTile < pObject->m_xTile)
	{
		if(m_yTile > pObject->m_yTile)
			UpdateDirection(2);
		else if(m_yTile < pObject->m_yTile)
			UpdateDirection(4);
		else
			UpdateDirection(3);
	}
	else
	{
		if(m_yTile > pObject->m_yTile)
			UpdateDirection(1);
		else if(m_yTile < pObject->m_yTile)
			UpdateDirection(5);
		else
			Assert(false);
	}
}

HRESULT CMovingObject::CreateProjectiles (CIsometricTranslator* pIsometric, CSIFCanvas* pMain, sysint nUnitLayer, TRStrMap<CProjectileData*>* pmapProjectiles, CProjectileAction* pAction)
{
	HRESULT hr;
	TStackRef<IJSONValue> srvRange, srvName, srvVelocity;
	RSTRING rstrName = NULL;
	CProjectileData* pProjectile;
	INT xViewStart, yViewStart;
	INT xViewEnd, yViewEnd;
	DOUBLE rVelocity;

	Check(JSONGetValueFromObject(m_pDef, SLP(L"base:stats:range"), &srvRange));
	Check(JSONGetValue(srvRange, SLP(L"name"), &srvName));
	Check(srvName->GetString(&rstrName));

	Check(pmapProjectiles->Find(rstrName, &pProjectile));
	Check(pProjectile->m_pProjectile->FindNonNullValueW(L"velocity", &srvVelocity));

	pIsometric->TileToView(m_xTile, m_yTile, &xViewStart, &yViewStart);
	pIsometric->TileToView(pAction->m_pTarget->m_xTile, pAction->m_pTarget->m_yTile, &xViewEnd, &yViewEnd);

	for(sysint i = 0; i < m_aSprites.Length(); i++)
	{
		if(m_aSprites[i].fVisible)
		{
			INT x, y, xOffset, yOffset, nSprite;
			TStackRef<ISimbeyInterchangeSprite> srSprite;

			m_aSprites[i].pSprite->GetPosition(x, y);
			xOffset = x - xViewStart;
			yOffset = y - yViewStart;

			Check(pProjectile->m_pAnimator->CreateSprite(&srSprite));
			Check(srSprite->SelectAnimation(m_nDirection));
			srSprite->SetPosition(x, y);
			pAction->AddProjectile(srSprite, xViewEnd + xOffset, yViewEnd + yOffset);
			Check(pMain->AddSprite(nUnitLayer, srSprite, &nSprite));
		}
	}

	Check(srvVelocity->GetDouble(&rVelocity));
	pAction->SetUnitVector(xViewStart, yViewStart, xViewEnd, yViewEnd, rVelocity);

Cleanup:
	RStrRelease(rstrName);
	return hr;
}

HRESULT CMovingObject::CreateBloodOverlays (CCombatScreen* pCombat, CSIFCanvas* pMain, sysint nLayer, ISimbeyInterchangeAnimator* pBlood, INT nAnimation)
{
	HRESULT hr = S_FALSE;

	for(sysint i = 0; i < m_aSprites.Length(); i++)
	{
		if(m_aSprites[i].fVisible)
		{
			INT x, y;
			TStackRef<ISimbeyInterchangeSprite> srSprite;

			m_aSprites[i].pSprite->GetPosition(x, y);
			Check(pBlood->CreateSprite(&srSprite));
			srSprite->SetPosition(x, y);
			Check(srSprite->SelectAnimation(nAnimation));
			Check(pCombat->AddEffect(pMain, nLayer, srSprite, m_aSprites[i].pSprite, ATTACK_ANI_TICKS));
		}
	}

Cleanup:
	return hr;
}

bool CMovingObject::HasSameOwner (CMovingObject* pOther)
{
	INT nResult = 0;
	RStrCompareRStr(m_rstrOwner, pOther->m_rstrOwner, &nResult);
	return 0 == nResult;
}

bool CMovingObject::CanRangeAttack (CMovingObject* pOther)
{
	TStackRef<IJSONValue> srv;
	return (abs(pOther->m_xTile - m_xTile) > 1 || abs(pOther->m_yTile - m_yTile) > 1) &&
		SUCCEEDED(JSONGetValueFromObject(m_pDef, SLP(L"base:stats:range"), &srv));
}

bool CMovingObject::CanMeleeAttack (CMovingObject* pOther)
{
	TStackRef<IJSONValue> srv;
	return (abs(pOther->m_xTile - m_xTile) <= 1 && abs(pOther->m_yTile - m_yTile) <= 1) &&
		SUCCEEDED(JSONGetValueFromObject(m_pDef, SLP(L"base:stats:melee"), &srv));
}

HRESULT CMovingObject::ReplaceAnimator (ISimbeyInterchangeAnimator* pAnimator, CSIFCanvas* pMain, sysint nUnitLayer)
{
	HRESULT hr = S_FALSE;

	for(sysint i = 0; i < m_aSprites.Length(); i++)
	{
		SPRITE_VIS& sprite = m_aSprites[i];
		INT x, y;
		INT nAnimation, nFrame, cTicks;
		TStackRef<ISimbeyInterchangeSprite> srSprite;

		sprite.pSprite->GetPosition(x, y);
		sprite.pSprite->GetCurrentAnimation(&nAnimation, &nFrame, &cTicks);
		Check(pAnimator->CreateSprite(&srSprite));
		srSprite->SetPosition(x, y);
		Check(srSprite->SelectAnimation(nAnimation, nFrame, cTicks));
		pMain->ReplaceSprite(nUnitLayer, sprite.pSprite, srSprite);
		sprite.pSprite->Release();
		sprite.pSprite = srSprite.Detach();
	}

Cleanup:
	return hr;
}

CMoveUnitAction::CMoveUnitAction (CCombatScreen* pWindow, CSIFCanvas* pMain, CIsometricTranslator* pIsometric, CObject* pSelected) :
	CAction(pWindow),
	m_pMain(pMain),
	m_pIsometric(pIsometric),
	m_pSelected(pSelected),
	m_cMoveTicks(0),
	m_nMoveFrame(0),
	m_nMoveIndex(0)
{
}

CMoveUnitAction::~CMoveUnitAction ()
{
}

VOID CMoveUnitAction::Update (VOID)
{
	if(0 == --m_cMoveTicks)
	{
		AddRef();

		m_nMoveFrame--;

		m_pSelected->UpdateMovement(m_nMoveFrame);

		if(0 == m_nMoveFrame)
			SelectNextMovement();
		else
			m_cMoveTicks = MOVE_TICKS;

		m_pScreen->SortUnitLayer();

		Release();
	}
}

VOID CMoveUnitAction::SelectNextMovement (VOID)
{
	sysint cPath = m_aPath.Length();
	if(++m_nMoveIndex == cPath)
	{
		m_aPath.Clear();
		m_pSelected->SelectBaseAnimation();

		m_pScreen->ClearAction(this);
	}
	else
	{
		INT nMoveIndex = (cPath - m_nMoveIndex) - 1;
		INT xTile = m_aPath[nMoveIndex].x;
		INT yTile = m_aPath[nMoveIndex].y;
		INT nDirection;

		INT xTarget, yTarget, xFrom, yFrom;
		m_pIsometric->TileToView(xTile, yTile, &xTarget, &yTarget);
		m_pIsometric->TileToView(m_pSelected->m_xTile, m_pSelected->m_yTile, &xFrom, &yFrom);

		m_cMoveTicks = MOVE_TICKS;
		m_nMoveFrame = 8;

		if(yTarget < yFrom)
		{
			if(xTarget < xFrom)
				nDirection = 7;
			else if(xTarget > xFrom)
				nDirection = 1;
			else
				nDirection = 0;
		}
		else if(yTarget > yFrom)
		{
			if(xTarget < xFrom)
				nDirection = 5;
			else if(xTarget > xFrom)
				nDirection = 3;
			else
				nDirection = 4;
		}
		else if(xTarget < xFrom)
			nDirection = 6;
		else
			nDirection = 2;

		m_pSelected->UpdateDirection(nDirection);
		m_pSelected->SelectMoveAnimation();

		m_pSelected->m_xTile = xTile;
		m_pSelected->m_yTile = yTile;

		m_pScreen->PlaySound(m_pSelected->GetMovingSound());
	}
}

CMergeUnitAction::CMergeUnitAction (CCombatScreen* pWindow, CSIFCanvas* pMain, CIsometricTranslator* pIsometric, CObject* pSelected, sysint nUnitLayer, INT xTarget, INT yTarget) :
	CAction(pWindow),
	m_pMain(pMain),
	m_pIsometric(pIsometric),
	m_pSelected(pSelected),
	m_nUnitLayer(nUnitLayer),
	m_pOriginalAnimator(NULL),
	m_xTarget(xTarget),
	m_yTarget(yTarget),
	m_cTicks(0),
	m_cMergeShift(0),
	m_eState(Idle)
{
}

CMergeUnitAction::~CMergeUnitAction ()
{
	SafeRelease(m_pOriginalAnimator);
}

VOID CMergeUnitAction::Update (VOID)
{
	switch(m_eState)
	{
	case Idle:
		{
			TStackRef<ISimbeyInterchangeSprite> srSprite;
			INT xView, yView;
			INT xTarget, yTarget;

			m_pSelected->GetLastVisibleSprite(&srSprite);
			srSprite->GetAnimator(&m_pOriginalAnimator);

			m_pIsometric->TileToView(m_pSelected->m_xTile, m_pSelected->m_yTile, &xView, &yView);
			m_pIsometric->TileToView(m_xTarget, m_yTarget, &xTarget, &yTarget);
			m_xOffset = xTarget - xView;
			m_yOffset = yTarget - yView;

			m_pScreen->PlaySound(m_pSelected->GetMovingSound());
			m_pSelected->SelectMoveAnimation();

			m_eState = Down;
			m_cTicks = 1;
		}
		__fallthrough;
	case Down:
		if(0 == --m_cTicks)
		{
			m_pSelected->ShiftObject(0, 1);
			m_cTicks = 3;

			if(16 == ++m_cMergeShift)
			{
				m_pSelected->m_xTile = m_xTarget;
				m_pSelected->m_yTile = m_yTarget;
				m_pSelected->ShiftObject(m_xOffset, m_yOffset);
				m_eState = Up;
			}

			UpdateSpriteSize();
		}
		break;
	case Up:
		if(0 == --m_cTicks)
		{
			m_pSelected->ShiftObject(0, -1);
			m_cTicks = 2;

			if(0 == --m_cMergeShift)
			{
				m_pSelected->SelectBaseAnimation();
				static_cast<CMovingObject*>(m_pSelected)->ReplaceAnimator(m_pOriginalAnimator, m_pMain, m_nUnitLayer);
				m_pScreen->ClearAction(this);
			}
			else
				UpdateSpriteSize();
		}
		break;
	}
}

HRESULT CMergeUnitAction::UpdateSpriteSize (VOID)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeAnimator> srDup;
	INT cImages;

	Check(m_pOriginalAnimator->Duplicate(&srDup));
	cImages = srDup->GetImageCount();
	for(INT i = 0; i < cImages; i++)
	{
		PBYTE pBits;
		INT nWidth, nHeight, xOffset, yOffset;

		Check(srDup->GetImage(i, &pBits, &nWidth, &nHeight));
		Check(srDup->GetImageOffset(i, &xOffset, &yOffset));
		Check(srDup->SetImage(i, FALSE, pBits, nWidth, max(nHeight - m_cMergeShift, 0), xOffset, yOffset));
	}

	static_cast<CMovingObject*>(m_pSelected)->ReplaceAnimator(srDup, m_pMain, m_nUnitLayer);

Cleanup:
	return hr;
}

CProjectileAction::CProjectileAction (CCombatScreen* pWindow, CMovingObject* pSource, CMovingObject* pTarget) :
	CAction(pWindow),
	m_pSource(pSource),
	m_pTarget(pTarget),
	m_fImpacted(FALSE),
	m_rx(0.0),
	m_ry(0.0),
	m_cSteps(0)
{
}

CProjectileAction::~CProjectileAction ()
{
	for(sysint i = 0; i < m_aSprites.Length(); i++)
		m_aSprites[i]->Release();
}

HRESULT CProjectileAction::AddProjectile (ISimbeyInterchangeSprite* pSprite, INT xTarget, INT yTarget)
{
	HRESULT hr;
	INT x, y;
	POINT pt;

	pSprite->GetPosition(x, y);
	pt.x = x;
	pt.y = y;
	Check(m_aStarts.Append(pt));

	Check(m_aSprites.Append(pSprite));
	pSprite->AddRef();

Cleanup:
	return hr;
}

VOID CProjectileAction::SetUnitVector (INT x1, INT y1, INT x2, INT y2, DOUBLE rVelocity)
{
	INT xDelta, yDelta;
	DOUBLE rDistance;

	xDelta = x2 - x1;
	yDelta = y2 - y1;

	rDistance = sqrt((DOUBLE)(xDelta * xDelta) + (DOUBLE)(yDelta * yDelta));
	m_vX = (DOUBLE)xDelta / rDistance;
	m_vY = (DOUBLE)yDelta / rDistance;

	m_vX *= rVelocity;
	m_vY *= rVelocity;

	m_xStart = x1;
	m_yStart = y1;

	m_xTarget = x2;
	m_yTarget = y2;

	m_cSteps = static_cast<INT>(floor(rDistance / rVelocity));
}

VOID CProjectileAction::Update (VOID)
{
	if(m_fImpacted)
	{
		INT nAnimation, nFrame, cTicks;
		m_aSprites[0]->GetCurrentAnimation(&nAnimation, &nFrame, &cTicks);
		if(9 == nAnimation)
		{
			for(sysint i = 0; i < m_aSprites.Length(); i++)
				m_pScreen->RemoveUnitSprite(m_aSprites[i]);
			m_pScreen->ClearAction(this);
		}
	}
	else
	{
		m_rx += m_vX;
		m_ry += m_vY;

		for(sysint i = 0; i < m_aSprites.Length(); i++)
			m_aSprites[i]->SetPosition(m_aStarts[i].x + (INT)m_rx, m_aStarts[i].y + (INT)m_ry);

		if(0 == --m_cSteps)
		{
			m_fImpacted = TRUE;

			for(sysint i = 0; i < m_aSprites.Length(); i++)
			{
				m_aSprites[i]->SelectAnimation(8);
				m_pScreen->BringSpriteToTop(m_aSprites[i], m_pTarget);
			}

			m_pScreen->PerformAttack(m_pSource, m_pTarget, TRUE);
		}
		else
			m_pScreen->SortUnitLayer();
	}
}

CEffect::CEffect (CCombatScreen* pScreen, CSIFCanvas* pCanvas, sysint nLayer, ISimbeyInterchangeSprite* pSprite, INT cTicksRemaining) :
	CAction(pScreen),
	m_pCanvas(pCanvas),
	m_nLayer(nLayer),
	m_pSprite(pSprite),
	m_cTicksRemaining(cTicksRemaining)
{
	m_pScreen->AddRef();
	m_pSprite->AddRef();
}

CEffect::~CEffect ()
{
	m_pSprite->Release();
	m_pScreen->Release();
}

VOID CEffect::Update (VOID)
{
	if(0 == --m_cTicksRemaining)
	{
		m_pCanvas->RemoveSprite(m_nLayer, m_pSprite);
		m_pScreen->ClearAction(this);
	}
}

CUnitUpdater::CUnitUpdater (CCombatScreen* pScreen, CMovingObject* pUnit, CSIFCanvas* pCanvas, sysint nLayer, INT cVisible, INT cTicksRemaining) :
	CAction(pScreen),
	m_pUnit(pUnit),
	m_pCanvas(pCanvas),
	m_nLayer(nLayer),
	m_cVisible(cVisible),
	m_cTicksRemaining(cTicksRemaining)
{
	m_pScreen->AddRef();
}

CUnitUpdater::~CUnitUpdater ()
{
	m_pScreen->Release();
}

VOID CUnitUpdater::Update (VOID)
{
	if(0 == --m_cTicksRemaining)
	{
		m_pUnit->UpdateVisList(m_pCanvas, m_nLayer, m_cVisible);
		if(0 == m_cVisible)
			m_pScreen->RemoveUnitFromBoard(m_pUnit);
		m_pScreen->UpdateStatsPanel();
		m_pScreen->ClearAction(this);
	}
}

CCombatBar::CCombatBar () :
	m_pSurface(NULL),
	m_pBar(NULL),
	m_pCombatUI(NULL),
	m_pUnitStats(NULL),
	m_pCombatBarFont(NULL),
	m_nBaseLayer(-1),
	m_nStatsLayer(-1),
	m_idxPressed(-1),
	m_rstrLeft(NULL),
	m_rstrRight(NULL)
{
	ZeroMemory(m_pButtons, sizeof(m_pButtons));
	m_idxPressed = -1;
}

CCombatBar::~CCombatBar ()
{
	for(INT i = 0; i < ARRAYSIZE(m_pButtons); i++)
		SafeRelease(m_pButtons[i]);

	SafeRelease(m_pCombatUI);
	SafeRelease(m_pUnitStats);
	SafeRelease(m_pCombatBarFont);

	RStrRelease(m_rstrLeft);
	RStrRelease(m_rstrRight);
}

HRESULT CCombatBar::Initialize (CCombatScreen* pWindow, CSIFSurface* pSurface, const RECT* pcrc)
{
	HRESULT hr;

	Check(pSurface->AddCanvas(pcrc, TRUE, &m_pBar));
	m_pScreen = pWindow;
	m_pSurface = pSurface;

Cleanup:
	return hr;
}

VOID CCombatBar::SetNames (RSTRING rstrLeft, RSTRING rstrRight)
{
	RStrReplace(&m_rstrLeft, rstrLeft);
	RStrReplace(&m_rstrRight, rstrRight);
}

HRESULT CCombatBar::Load (ISimbeyInterchangeAnimator* pAnimator, ISimbeyInterchangeAnimator* pUnitStats, ISimbeyInterchangeFileFont* pYellowFont, ISimbeyInterchangeFileFont* pCombatBarFont)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeSprite> srBar, srButton;
	TStackRef<CDrawText> srText;
	CInteractiveLayer* pLayer = NULL;

	SetInterface(m_pCombatUI, pAnimator);
	SetInterface(m_pUnitStats, pUnitStats);
	SetInterface(m_pCombatBarFont, pCombatBarFont);

	Check(static_cast<CInteractiveCanvas*>(m_pBar)->AddInteractiveLayer(FALSE, FALSE, 0, this, &pLayer));
	m_nBaseLayer = pLayer->GetLayer();

	Check(m_pCombatUI->CreateSprite(&srBar));
	Check(m_pBar->AddSprite(m_nBaseLayer, srBar, NULL));

	for(INT i = 0; i < ARRAYSIZE(c_rgButtons); i++)
	{
		Check(m_pCombatUI->CreateSprite(&srButton));
		srButton->SelectAnimation(i * 2 + 1);
		srButton->SetPosition(c_rgButtons[i].left, c_rgButtons[i].top);
		Check(m_pBar->AddSprite(m_nBaseLayer, srButton, NULL));
		m_pButtons[i] = srButton.Detach();
	}

	srText.Attach(__new CDrawText(pYellowFont, m_rstrLeft, DT_CENTER));
	CheckAlloc(srText);
	srText->SetPosition(64, 0);
	Check(m_pBar->AddSprite(m_nBaseLayer, srText, NULL));
	srText.Release();

	srText.Attach(__new CDrawText(pYellowFont, m_rstrRight, DT_CENTER));
	CheckAlloc(srText);
	srText->SetPosition(450, 0);
	Check(m_pBar->AddSprite(m_nBaseLayer, srText, NULL));
	srText.Release();

	Check(m_pBar->AddLayer(TRUE, FALSE, 0, &m_nStatsLayer));

Cleanup:
	SafeRelease(pLayer);
	return hr;
}

HRESULT CCombatBar::Update (CMovingObject* pSelected)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeSprite> srSprite;
	TStackRef<IJSONObject> srStats;
	TStackRef<IJSONValue> srv;
	TStackRef<CDrawText> srText;
	TStackRef<CDrawSolid> srSolid;
	RSTRING rstrName = NULL;
	RSTRING rstrRangeType = NULL;

	m_pBar->ClearLayer(m_nStatsLayer);

	Check(m_pScreen->ExpandStats(pSelected->m_pDef, pSelected->m_nLevel, &srStats));

	Check(JSONGetValueFromObject(pSelected->m_pDef, SLP(L"base:name"), &srv));
	Check(srv->GetString(&rstrName));
	srv.Release();

	srText.Attach(__new CDrawText(m_pCombatBarFont, rstrName, DT_CENTER));
	CheckAlloc(srText);
	srText->SetPosition(180, 0);
	Check(m_pBar->AddSprite(m_nStatsLayer, srText, NULL));
	srText.Release();

	if(SUCCEEDED(srStats->FindNonNullValueW(L"melee", &srv)))
	{
		Check(PlaceStat(209, 14, srStats, L"melee"));
		srv.Release();
	}

	if(SUCCEEDED(srStats->FindNonNullValueW(L"range", &srv)))
	{
		Check(PlaceStat(209, 27, srStats, L"range"));
		srv.Release();
	}

	Check(PlaceStat(209, 40, pSelected->m_pDef, L"moveStat"));

	srSolid.Attach(__new CDrawSolid(0xFF000000 | RGB(0, 255, 0), 50, 2));
	CheckAlloc(srSolid);
	srSolid->SetPosition(140, 49);
	Check(m_pBar->AddSprite(m_nStatsLayer, srSolid, NULL));
	srSolid.Release();

	srSolid.Attach(__new CDrawSolid(0xFF000000 | RGB(0, 0, 0), 50, 1));
	CheckAlloc(srSolid);
	srSolid->SetPosition(140, 51);
	Check(m_pBar->AddSprite(m_nStatsLayer, srSolid, NULL));
	srSolid.Release();

	Check(pSelected->CloneSprite(&srSprite));
	srSprite->SetPosition(152, 32);
	srSprite->SelectAnimation(9);
	Check(m_pBar->AddSprite(m_nStatsLayer, srSprite, NULL));

Cleanup:
	RStrRelease(rstrRangeType);
	RStrRelease(rstrName);
	return hr;
}

VOID CCombatBar::Clear (VOID)
{
	m_pBar->ClearLayer(m_nStatsLayer);
}

// ILayerInputHandler

BOOL CCombatBar::ProcessMouseInput (LayerInput::Mouse eType, WPARAM wParam, LPARAM lParam, INT xView, INT yView, LRESULT& lResult)
{
	if(LayerInput::Move == eType)
		m_pScreen->UpdateMouse(lParam);
	else if(LayerInput::LButtonDown == eType)
	{
		if(0 <= m_idxPressed)
			m_pButtons[m_idxPressed]->SelectAnimation(m_idxPressed * 2 + 1);

		m_idxPressed = GetButtonFromPoint(xView, yView);
		for(INT i = 0; i < ARRAYSIZE(c_rgButtons); i++)
		{
			if(xView >= c_rgButtons[i].left && xView < c_rgButtons[i].right && yView >= c_rgButtons[i].top && yView < c_rgButtons[i].bottom)
			{
				m_pButtons[i]->SelectAnimation(i * 2 + 2);
				static_cast<CInteractiveSurface*>(m_pSurface)->CaptureCanvas(static_cast<CInteractiveCanvas*>(m_pBar));
				break;
			}
		}
	}
	else if(LayerInput::LButtonUp == eType)
	{
		if(-1 != m_idxPressed)
		{
			static_cast<CInteractiveSurface*>(m_pSurface)->ReleaseCapture();

			if(GetButtonFromPoint(xView, yView) == m_idxPressed)
			{
				if(0 == m_idxPressed)
					m_pScreen->ShowSpellBook();
				else if(2 == m_idxPressed)
					m_pScreen->ShowSelectedUnitInfo();
				else
				{
					// Activate!
					MessageBeep(MB_OK);
				}
			}

			m_pButtons[m_idxPressed]->SelectAnimation(m_idxPressed * 2 + 1);
			m_idxPressed = -1;
		}
	}

	return TRUE;
}

BOOL CCombatBar::ProcessKeyboardInput (LayerInput::Keyboard eType, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return FALSE;
}

HRESULT CCombatBar::PlaceStat (INT x, INT y, IJSONObject* pSource, PCWSTR pcwzField)
{
	return m_pScreen->PlaceStat(m_pBar, m_pCombatBarFont, m_nStatsLayer, x, y, pSource, pcwzField);
}

INT CCombatBar::GetButtonFromPoint (INT xView, INT yView)
{
	for(INT i = 0; i < ARRAYSIZE(c_rgButtons); i++)
	{
		if(xView >= c_rgButtons[i].left && xView < c_rgButtons[i].right && yView >= c_rgButtons[i].top && yView < c_rgButtons[i].bottom)
			return i;
	}
	return -1;
}

CCombatScreen::CCombatScreen (IScreenHost* pHost, CInteractiveSurface* pSurface, CSIFPackage* pPackage, ISimbeyInterchangeFileFont* pYellowFont, ISimbeyInterchangeFileFont* pSmallYellowFont, IJSONObject* pWizard, IJSONObject* pPlacements) :
	CBaseScreen(pHost, pSurface, pPackage),
	m_pMain(NULL),
	m_pMouse(NULL),
	m_pStats(NULL),
	m_Isometric(TILE_WIDTH, TILE_HEIGHT),
	m_pCombat(NULL),
	m_pMoveTo(NULL),
	m_pSelectedTile(NULL),
	m_pCombatStats(NULL),
	m_pCombatMelee(NULL),
	m_pSelected(NULL),
	m_pCastSpell(NULL),
	m_pCombatBarFont(NULL),
	m_pFonts(NULL),
	m_pMoveType(NULL),
	m_xHoverTile(-1),
	m_yHoverTile(-1),
	m_pUnitStats(NULL),
	m_pBlood(NULL),
	m_pCombatBar(NULL),
	m_pBackground(NULL),
	m_dblBackground(0.0),
	m_pCornerA(NULL),
	m_pCornerB(NULL),
	m_pCornerC(NULL),
	m_pCornerD(NULL)
{
	SetInterface(m_pWizard, pWizard);
	SetInterface(m_pPlacements, pPlacements);

	SetInterface(m_pYellowFont, pYellowFont);
	SetInterface(m_pSmallYellowFont, pSmallYellowFont);
}

CCombatScreen::~CCombatScreen ()
{
	Assert(NULL == m_pCombat);

	Assert(0 == m_aActions.Length());
	Assert(0 == m_aObjects.Length());

	SafeRelease(m_pCornerA);
	SafeRelease(m_pCornerB);
	SafeRelease(m_pCornerC);
	SafeRelease(m_pCornerD);

	Assert(NULL == m_pCombatBar);
	Assert(NULL == m_pCastSpell);
	Assert(0 == m_aViews.Length());

	SafeRelease(m_pFonts);
	SafeRelease(m_pCombatBarFont);
	SafeRelease(m_pSmallYellowFont);
	SafeRelease(m_pYellowFont);

	SafeRelease(m_pPlacements);
	SafeRelease(m_pWizard);
}

HRESULT CCombatScreen::Initialize (VOID)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	ISimbeyInterchangeFile* pSIF = NULL;
	TStackRef<IPersistedFile> srFont;
	ULARGE_INTEGER uliFont;
	DWORD cParams;

	INT xScroll, yScroll;
	INT xSize, ySize;
	RECT rc;

	m_pSurface->GetViewSize(&xSize, &ySize);
	rc.left = 0;
	rc.top = 0;
	rc.right = xSize;
	rc.bottom = ySize - 58;

	if(SUCCEEDED(m_pPlacements->FindNonNullValueW(L"floating", &srv)))
	{
		bool fFloating;

		Check(srv->GetBoolean(&fFloating));

		if(fFloating)
		{
			sysint nLayer;
			Check(m_pSurface->AddCanvas(&rc, FALSE, &m_pBackground));
			Check(m_pBackground->AddLayer(FALSE, FALSE, 0, &nLayer));
		}
	}

	m_Isometric.TileToView(MAP_WIDTH / 2, MAP_HEIGHT / 2, &xScroll, &yScroll);
	Check(m_pSurface->AddCanvas(&rc, TRUE, &m_pMain));
	m_pMain->SetScroll(xScroll + TILE_WIDTH / 2, yScroll);

	rc.top = rc.bottom;
	rc.bottom = ySize;
	m_pCombatBar = __new CCombatBar;
	CheckAlloc(m_pCombatBar);
	Check(m_pCombatBar->Initialize(this, m_pSurface, &rc));

	Check(m_pSurface->AddCanvas(NULL, FALSE, &m_pMouse));
	Check(m_pMouse->AddLayer(FALSE, FALSE, 0, NULL));

	Check(m_pPackage->OpenSIF(L"graphics\\CombatBarFont.sif", &pSIF));
	Check(sifCreateFontFromSIF(pSIF, TRUE, &m_pCombatBarFont));

	Check(sifCreateFontCollection(&m_pFonts));

	Check(m_pPackage->OpenFile(SLP(L"fonts\\Aclonica.ttf"), &srFont, &uliFont));
	Check(m_pFonts->LoadStreamFont(srFont, uliFont.LowPart));
	srFont.Release();

	Check(m_pPackage->OpenFile(SLP(L"fonts\\DreamOrphanage.ttf"), &srFont, &uliFont));
	Check(m_pFonts->LoadStreamFont(srFont, uliFont.LowPart));
	srFont.Release();

	Check(m_pPackage->OpenFile(SLP(L"fonts\\KingthingsPetrock.ttf"), &srFont, &uliFont));
	Check(m_pFonts->LoadStreamFont(srFont, uliFont.LowPart));

	Check(m_pHost->GetVM()->FindFunction(L"ExpandStats", &m_idxExpandStats, &cParams));
	CheckIf(2 != cParams, E_UNEXPECTED);

	Check(m_pHost->GetVM()->FindFunction(L"CreateCombatObject", &m_idxCreateCombatObject, &cParams));
	CheckIf(4 != cParams, E_UNEXPECTED);

	Check(m_pHost->GetVM()->FindFunction(L"ApplyHealing", &m_idxApplyHealing, &cParams));
	CheckIf(3 != cParams, E_UNEXPECTED);

	Check(LoadData());
	Check(LoadProjectiles());
	Check(LoadSprites());
	Check(LoadMusic());

Cleanup:
	if(FAILED(hr))
		OnDestroy();
	SafeRelease(pSIF);
	return hr;
}

VOID CCombatScreen::PlaySound (FMOD::Sound* pSound)
{
	m_pHost->PlaySound(FMOD_CHANNEL_FREE, pSound, false, NULL);
}

VOID CCombatScreen::ClearAction (CAction* pAction)
{
	for(sysint i = 0; i < m_aActions.Length(); i++)
	{
		if(m_aActions[i] == pAction)
		{
			m_aActions.Remove(i, NULL);
			break;
		}
	}

	if(0 == m_aActions.Length() && m_pSelectedTile &&
		S_FALSE == m_pMain->FindSprite(m_nTileEffectsLayer, m_pSelectedTile, NULL))
	{
		INT xIso, yIso, xView, yView;

		m_pMain->AddSprite(m_nTileEffectsLayer, m_pSelectedTile, NULL);
		m_Isometric.TileToView(m_pSelected->m_xTile, m_pSelected->m_yTile, &xIso, &yIso);
		m_Isometric.IsometricToView(m_pMain, xIso, yIso, &xView, &yView);
		m_pSelectedTile->SetPosition(xView, yView);
	}

	if(0 == m_aActions.Length() && NULL == m_pSelectedTile && m_pMoveType)
		m_pMoveType->SelectAnimation(3);
}

VOID CCombatScreen::RemoveUnitSprite (ISimbeyInterchangeSprite* pSprite)
{
	m_pMain->RemoveSprite(m_nUnitLayer, pSprite);
}

VOID CCombatScreen::SortUnitLayer (VOID)
{
	m_Isometric.SortIsometricLayer(m_pMain, m_nUnitLayer);
}

VOID CCombatScreen::BringSpriteToTop (ISimbeyInterchangeSprite* pSprite, CMovingObject* pUnit)
{
	if(pUnit)
	{
		TStackRef<ISimbeyInterchangeSprite> srLast;

		if(SUCCEEDED(pUnit->GetLastVisibleSprite(&srLast)))
		{
			m_pMain->RemoveSprite(m_nUnitLayer, pSprite);
			m_pMain->AddSpriteAfter(m_nUnitLayer, pSprite, srLast, NULL);
			return;
		}
	}

	m_pMain->BringSpriteToTop(m_nUnitLayer, pSprite);
}

HRESULT CCombatScreen::GetDataStat (RSTRING rstrType, __deref_out IJSONValue** ppvStat)
{
	HRESULT hr;
	TStackRef<IJSONObject> srType;

	Check(JSONFindArrayObject(m_pCombatStats, RSTRING_CAST(L"stat"), rstrType, &srType, NULL));
	Check(srType->FindNonNullValueW(L"icon", ppvStat));

Cleanup:
	return hr;
}

HRESULT CCombatScreen::FindUnitAbility (IJSONObject* pUnit, RSTRING rstrAbility, __deref_out IJSONObject** ppAbility)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONObject> srBase;
	TStackRef<IJSONArray> srAbilities;

	Check(pUnit->FindNonNullValueW(L"base", &srv));
	Check(srv->GetObject(&srBase));
	srv.Release();

	CheckNoTrace(srBase->FindNonNullValueW(L"abilities", &srv));
	Check(srv->GetArray(&srAbilities));

	CheckNoTrace(JSONFindArrayObject(srAbilities, RSTRING_CAST(L"name"), rstrAbility, ppAbility, NULL));

Cleanup:
	return hr;
}

HRESULT CCombatScreen::PlaceStat (CSIFCanvas* pCanvas, ISimbeyInterchangeFileFont* pFont, sysint nLayer, INT x, INT y, IJSONObject* pSource, PCWSTR pcwzField, bool fStatSprite)
{
	HRESULT hr;
	RSTRING rstrStat = NULL, rstrValue = NULL;
	TStackRef<CDrawText> srText;
	TStackRef<ISimbeyInterchangeSprite> srIcon;
	TStackRef<IJSONValue> srv;
	INT nValue;

	CheckNoTrace(pSource->FindNonNullValueW(pcwzField, &srv));
	if(JSON::Object == srv->GetType())
	{
		TStackRef<IJSONObject> srStat;

		Check(srv->GetObject(&srStat));
		srv.Release();

		Check(srStat->FindNonNullValueW(L"stat", &srv));
		Check(srv->GetString(&rstrStat));
		srv.Release();

		Check(srStat->FindNonNullValueW(L"value", &srv));
	}
	else
		Check(RStrCreateW(TStrLenAssert(pcwzField), pcwzField, &rstrStat));

	Check(srv->GetInteger(&nValue));
	Check(RStrFormatW(&rstrValue, L"%d", nValue));
	srv.Release();

	srText.Attach(__new CDrawText(pFont, rstrValue, DT_RIGHT));
	CheckAlloc(srText);
	srText->SetPosition(x, y);
	Check(pCanvas->AddSprite(nLayer, srText, NULL));

	if(fStatSprite && SUCCEEDED(GetDataStat(rstrStat, &srv)))
	{
		INT nStat;
		Check(srv->GetInteger(&nStat));
		Check(m_pUnitStats->CreateSprite(&srIcon));
		Check(srIcon->SelectAnimation(nStat));
		srIcon->SetPosition(x + 3, y + 2);
		Check(pCanvas->AddSprite(nLayer, srIcon, NULL));
	}

Cleanup:
	RStrRelease(rstrValue);
	RStrRelease(rstrStat);
	return hr;
}

// IScreen

VOID CCombatScreen::OnDestroy (VOID)
{
	if(m_pCombat)
	{
		QVMReleaseObject(m_pCombat);
		m_pCombat = NULL;
	}

	for(sysint i = m_aViews.Length() - 1; i >= 0; i--)
		m_aViews[i]->Destroy();
	m_aViews.Clear();

	SafeRelease(m_pCombatBar);
	SafeDelete(m_pCastSpell);

	m_aActions.Clear();
	m_aObjects.DeleteAll();
	m_aRemoved.DeleteAll();

	for(sysint i = 0; i < m_mapCombatAnimations.Length(); i++)
		(*m_mapCombatAnimations.GetValuePtr(i))->Release();
	m_mapCombatAnimations.Clear();

	for(sysint i = 0; i < m_mapUnits.Length(); i++)
		__delete *m_mapUnits.GetValuePtr(i);
	m_mapUnits.Clear();

	for(sysint i = 0; i < m_mapProjectiles.Length(); i++)
		__delete *m_mapProjectiles.GetValuePtr(i);
	m_mapProjectiles.Clear();

	m_pSurface->RemoveCanvas(m_pStats); m_pStats = NULL;
	m_pSurface->RemoveCanvas(m_pMouse); m_pMouse = NULL;
	m_pSurface->RemoveCanvas(m_pMain); m_pMain = NULL;

	SafeRelease(m_pBlood);
	SafeRelease(m_pUnitStats);
	SafeRelease(m_pCombatStats);
	SafeRelease(m_pCombatMelee);

	SafeRelease(m_pMoveType);
	SafeRelease(m_pSelectedTile);
	SafeRelease(m_pMoveTo);
}

VOID CCombatScreen::OnUpdateFrame (VOID)
{
	INT xOffset = 0, yOffset = 0;

	if(m_pHost->IsPressed(VK_UP))
		yOffset -= 15;
	else if(m_pHost->IsPressed(VK_DOWN))
		yOffset += 15;
	if(m_pHost->IsPressed(VK_LEFT))
		xOffset -= 15;
	else if(m_pHost->IsPressed(VK_RIGHT))
		xOffset += 15;

	if(xOffset || yOffset)
	{
		INT xScroll, yScroll;
		m_pMain->GetScroll(&xScroll, &yScroll);
		xScroll += xOffset;
		yScroll += yOffset;
		m_pMain->SetScroll(xScroll, yScroll);
	}

	for(sysint i = m_aActions.Length() - 1; i >= 0; i--)
		m_aActions[i]->Update();

	if(m_pBackground)
	{
		INT xScroll = (INT)(64.0 + sin(m_dblBackground * 3.1415 / 180.0) * 64.0);
		m_pBackground->SetScroll(xScroll, 0);

		m_dblBackground += 0.25;
		if(m_dblBackground >= 360.0)
			m_dblBackground -= 360.0;
	}
}

VOID CCombatScreen::OnNotifyFinished (BOOL fCompleted)
{
	if(fCompleted)
		m_pHost->PlayMIDI(&m_midiFile);
}

VOID CCombatScreen::OnChangeActive (BOOL fActive)
{
}

// ILayerInputHandler

BOOL CCombatScreen::ProcessMouseInput (LayerInput::Mouse eType, WPARAM wParam, LPARAM lParam, INT xView, INT yView, LRESULT& lResult)
{
	if(LayerInput::Move == eType)
	{
		INT xIso, yIso;
		INT xTile, yTile;

		m_Isometric.ViewToIsometric(m_pMain, xView, yView, &xIso, &yIso);
		m_Isometric.ViewToTile(xIso, yIso, &xTile, &yTile);
		m_Isometric.TileToView(xTile, yTile, &xIso, &yIso);

		m_Isometric.IsometricToView(m_pMain, xIso, yIso, &xView, &yView);
		m_pMoveTo->SetPosition(xView, yView);

		if(m_pCastSpell)
		{
			HRESULT hrSpell;
			CObject* pObject = FindObject(xTile, yTile);
			if(pObject)
			{
				if(pObject->CanBeMoved())
					hrSpell = m_pCastSpell->Query(xTile, yTile, static_cast<CMovingObject*>(pObject));
				else
					hrSpell = S_FALSE;
			}
			else
				hrSpell = m_pCastSpell->Query(xTile, yTile, NULL);
			if(S_OK == hrSpell)
				m_pMoveType->SelectAnimation(3);
			else
				m_pMoveType->SelectAnimation(0);
		}
		else if(m_pSelected && 0 == m_aActions.Length())
		{
			if(xTile == m_pSelected->m_xTile && yTile == m_pSelected->m_yTile)
				m_pMoveType->SelectAnimation(0);
			else
			{
				CObject* pObject = FindObject(xTile, yTile);
				if(pObject)
				{
					if(pObject->CanBeMoved())
					{
						CMovingObject* pOther = static_cast<CMovingObject*>(pObject);
						CMovingObject* pSelected = static_cast<CMovingObject*>(m_pSelected);

						if(pSelected->HasSameOwner(pOther))
							m_pMoveType->SelectAnimation(0);
						else if(pSelected->CanRangeAttack(pOther))
							m_pMoveType->SelectAnimation(2);
						else if(pSelected->CanMeleeAttack(pOther))
							m_pMoveType->SelectAnimation(1);
						else
							m_pMoveType->SelectAnimation(0);
					}
					else
						m_pMoveType->SelectAnimation(0);
				}
				else
					m_pMoveType->SelectAnimation(3);
			}
		}

		m_pSurface->TranslateClientPointToCanvas(LOWORD(lParam), HIWORD(lParam), m_pMouse, &xView, &yView);
		m_pMoveType->SetPosition(xView, yView);

		UpdateStatsPanel(xTile, yTile);
	}
	else if(LayerInput::LButtonDown == eType)
	{
		INT xIso, yIso;
		INT xTile, yTile;

		m_Isometric.ViewToIsometric(m_pMain, xView, yView, &xIso, &yIso);
		m_Isometric.ViewToTile(xIso, yIso, &xTile, &yTile);
		m_Isometric.TileToView(xTile, yTile, &xIso, &yIso);

		m_Isometric.IsometricToView(m_pMain, xIso, yIso, &xView, &yView);
		m_pMoveTo->SetPosition(xView, yView);

		if(0 == m_aActions.Length())
		{
			if(m_pCastSpell)
			{
				HRESULT hrSpell;
				CObject* pObject = FindObject(xTile, yTile);
				CAction* pSpellAction = NULL;
				if(pObject)
				{
					if(pObject->CanBeMoved())
						hrSpell = m_pCastSpell->Cast(this, m_pMain, m_nUnitLayer, &m_Isometric, xTile, yTile, static_cast<CMovingObject*>(pObject), &pSpellAction);
					else
						hrSpell = S_FALSE;
				}
				else
					hrSpell = m_pCastSpell->Cast(this, m_pMain, m_nUnitLayer, &m_Isometric, xTile, yTile, NULL, &pSpellAction);
				if(pSpellAction)
				{
					m_aActions.Append(pSpellAction);
					pSpellAction->Release();
					SafeDelete(m_pCastSpell);
				}
				else if(FAILED(hrSpell))
					SafeDelete(m_pCastSpell);
			}
			else if(m_pSelected)
			{
				CObject* pObject = FindObject(xTile, yTile);
				if(pObject)
				{
					if(pObject->CanBeMoved())
					{
						CMovingObject* pOther = static_cast<CMovingObject*>(pObject);
						CMovingObject* pSelected = static_cast<CMovingObject*>(m_pSelected);

						if(!pSelected->HasSameOwner(pOther))
						{
							pSelected->FaceObject(pOther);

							if(pSelected->CanRangeAttack(pOther))
							{
								CProjectileAction* pAction = __new CProjectileAction(this, pSelected, pOther);
								if(pAction)
								{
									pSelected->SelectRangeAnimation();
									pSelected->CreateProjectiles(&m_Isometric, m_pMain, m_nUnitLayer, &m_mapProjectiles, pAction);
									m_pHost->PlaySound(FMOD_CHANNEL_FREE, m_pSelected->GetRangeSound(), false, NULL);

									m_aActions.Append(pAction);
									pAction->Release();
								}
							}
							else if(pSelected->CanMeleeAttack(pOther))
							{
								pSelected->SelectAttackAnimation();
								pOther->FaceObject(pSelected);
								pOther->SelectAttackAnimation();
								PerformAttack(pSelected, pOther, FALSE);
								m_pHost->PlaySound(FMOD_CHANNEL_FREE, m_pSelected->GetMeleeSound(), false, NULL);
							}
						}
					}
				}
				else
				{
					TStackRef<IJSONObject> srAbility;

					Assert(m_pSelected->CanBeMoved());

					if(SUCCEEDED(FindUnitAbility(static_cast<CMovingObject*>(m_pSelected)->m_pDef, RSTRING_CAST(L"Merging"), &srAbility)))
						StartMergingAction(xTile, yTile);
					else
						StartMovingAction(xTile, yTile);
				}
			}
		}
	}
	else if(LayerInput::RButtonDown == eType)
	{
		if(0 == m_aActions.Length())
		{
			HRESULT hr;
			CObject* pObject;

			INT xIso, yIso;
			INT xTile, yTile;

			m_Isometric.ViewToIsometric(m_pMain, xView, yView, &xIso, &yIso);
			m_Isometric.ViewToTile(xIso, yIso, &xTile, &yTile);
			m_Isometric.TileToView(xTile, yTile, &xIso, &yIso);

			pObject = FindObject(xTile, yTile);
			if(pObject && pObject->CanBeMoved())
			{
				if(NULL == m_pSelectedTile)
				{
					TStackRef<ISimbeyInterchangeAnimator> srAnimator;

					Check(LoadAnimator(SLP(L"graphics\\MoveTo.json"), L"graphics\\Selected.sif", &srAnimator, FALSE));
					Check(PlaceTile(m_pMain, xTile, yTile, m_nTileEffectsLayer, srAnimator, 0, &m_pSelectedTile));
				}
				else
				{
					INT xView, yView;

					m_Isometric.IsometricToView(m_pMain, xIso, yIso, &xView, &yView);
					m_pSelectedTile->SetPosition(xView, yView);
				}

				m_pSelected = pObject;
				m_pCombatBar->Update(static_cast<CMovingObject*>(m_pSelected));
			}
		}
	}

Cleanup:
	return TRUE;
}

BOOL CCombatScreen::ProcessKeyboardInput (LayerInput::Keyboard eType, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return FALSE;
}

HRESULT CCombatScreen::StartMergingAction (INT xTile, INT yTile)
{
	HRESULT hr;
	CMergeUnitAction* pAction = __new CMergeUnitAction(this, m_pMain, &m_Isometric, m_pSelected, m_nUnitLayer, xTile, yTile);

	CheckAlloc(pAction);
	m_aActions.Append(pAction);
	hr = S_OK;

Cleanup:
	SafeRelease(pAction);
	return hr;
}

HRESULT CCombatScreen::StartMovingAction (INT xTile, INT yTile)
{
	HRESULT hr;
	CAStar2D aStar;
	CMoveUnitAction* pAction = NULL;

	Check(aStar.PreInit());
	Check(aStar.FindPath(m_pSelected->m_xTile, m_pSelected->m_yTile, xTile, yTile, 64, this));

	pAction = __new CMoveUnitAction(this, m_pMain, &m_Isometric, m_pSelected);
	CheckAlloc(pAction);

	aStar.GetPath(xTile, yTile, &pAction->m_aPath);
	pAction->SelectNextMovement();
	m_aActions.Append(pAction);

	if(m_pSelectedTile)
		m_pMain->RemoveSprite(m_nTileEffectsLayer, m_pSelectedTile);

Cleanup:
	SafeRelease(pAction);
	return hr;
}

HRESULT CCombatScreen::UpdateStatsPanel (INT xTile, INT yTile)
{
	HRESULT hr;
	CObject* pObject;

	CheckIfIgnore(xTile == m_xHoverTile && yTile == m_yHoverTile, S_FALSE);

	m_xHoverTile = xTile;
	m_yHoverTile = yTile;

	pObject = FindObject(xTile, yTile);
	if(pObject && pObject->CanBeMoved())
	{
		CMovingObject* pMoving = static_cast<CMovingObject*>(pObject);
		TStackRef<IJSONObject> srDef;
		sysint nLayer;
		RECT rcStats;

		if(m_pStats)
		{
			m_pSurface->RemoveCanvas(m_pStats);
			m_pStats = NULL;
		}

		rcStats.left = 306;
		rcStats.top = 5;
		rcStats.right = 506;
		rcStats.bottom = 110;
		Check(m_pSurface->AddCanvas(&rcStats, FALSE, &m_pStats));
		Check(m_pStats->AddLayer(FALSE, FALSE, 0x30000000 | RGB(0, 0, 0), &nLayer));

		Check(pMoving->GetObjectDef(&srDef));
		Check(UpdateUnitStats(nLayer, srDef, pMoving->m_nLevel));
	}
	else if(m_pStats)
	{
		m_pSurface->RemoveCanvas(m_pStats);
		m_pStats = NULL;
	}

	hr = S_OK;

Cleanup:
	return hr;
}

HRESULT CCombatScreen::UpdateUnitStats (sysint nLayer, IJSONObject* pUnit, INT nLevel)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONObject> srDef, srStats;
	RSTRING rstrName = NULL, rstrText = NULL, rstrRangeType = NULL;
	TStackRef<CDrawText> srText;
	TStackRef<CDrawSolid> srSolid;
	DOUBLE dblHealth;
	INT nHealthBarSize;
	COLORREF crHealthBar;

	Check(JSONGetValueFromObject(pUnit, SLP(L"base:name"), &srv));
	Check(srv->GetString(&rstrName));
	srv.Release();

	Check(ExpandStats(pUnit, nLevel, &srStats));
	Check(GetHealthPct(pUnit, srStats, &dblHealth));

	// The HSL value for solid green is (80, 240, 120).
	// Coincidentally, the health bar is also 80 pixels wide.
	nHealthBarSize = static_cast<INT>(80.0 * dblHealth);
	crHealthBar = HSLToRGB(static_cast<DOUBLE>(nHealthBarSize) / 255.0, 240.0 / 255.0, 120.0 / 255.0);

	srText.Attach(__new CDrawText(m_pYellowFont, rstrName, DT_CENTER));
	CheckAlloc(srText);
	srText->SetPosition(100, 2);
	Check(m_pStats->AddSprite(nLayer, srText, NULL));
	srText.Release();

	Check(PlaceStat(nLayer, 40, 25, srStats, L"melee"));
	Check(PlaceStat(nLayer, 120, 25, srStats, L"defense"));

	if(SUCCEEDED(PlaceStat(nLayer, 40, 45, srStats, L"range")))
	{
		TStackRef<IJSONObject> srAbility;

		if(SUCCEEDED(FindUnitAbility(pUnit, RSTRING_CAST(L"Ranged Attack"), &srAbility)))
		{
			Check(PlaceStat(nLayer, 120, 65, srAbility, L"value", false));

			Check(RStrCreateW(LSP(L"ammo"), &rstrText));
			srText.Attach(__new CDrawText(m_pSmallYellowFont, rstrText, DT_LEFT));
			CheckAlloc(srText);
			srText->SetPosition(123, 65);
			Check(m_pStats->AddSprite(nLayer, srText, NULL));
			srText.Release();
			RStrRelease(rstrText); rstrText = NULL;
		}
	}

	Check(PlaceStat(nLayer, 120, 45, srStats, L"resist"));
	Check(PlaceStat(nLayer, 40, 65, pUnit, L"moveStat"));

	Check(RStrCreateW(LSP(L"Hits"), &rstrText));
	srText.Attach(__new CDrawText(m_pSmallYellowFont, rstrText, DT_RIGHT));
	CheckAlloc(srText);
	srText->SetPosition(40, 85);
	Check(m_pStats->AddSprite(nLayer, srText, NULL));

	srSolid.Attach(__new CDrawSolid(0xFF000000 | crHealthBar, nHealthBarSize, 2));
	CheckAlloc(srSolid);
	srSolid->SetPosition(43, 92);
	Check(m_pStats->AddSprite(nLayer, srSolid, NULL));
	srSolid.Release();

	if(nHealthBarSize < 80)
	{
		srSolid.Attach(__new CDrawSolid(0xFF000000 | RGB(20, 20, 20), 80 - nHealthBarSize, 2));
		CheckAlloc(srSolid);
		srSolid->SetPosition(43 + nHealthBarSize, 92);
		Check(m_pStats->AddSprite(nLayer, srSolid, NULL));
		srSolid.Release();
	}

	srSolid.Attach(__new CDrawSolid(0xFF000000 | RGB(0, 0, 0), 80, 1));
	CheckAlloc(srSolid);
	srSolid->SetPosition(43, 94);
	Check(m_pStats->AddSprite(nLayer, srSolid, NULL));
	srSolid.Release();

Cleanup:
	RStrRelease(rstrRangeType);
	RStrRelease(rstrText);
	RStrRelease(rstrName);
	return hr;
}

HRESULT CCombatScreen::PlaceStat (sysint nLayer, INT x, INT y, IJSONObject* pSource, PCWSTR pcwzField, bool fStatSprite)
{
	return PlaceStat(m_pStats, m_pSmallYellowFont, nLayer, x, y, pSource, pcwzField, fStatSprite);
}

HRESULT CCombatScreen::AddEffect (CSIFCanvas* pCanvas, sysint nLayer, ISimbeyInterchangeSprite* pSprite, ISimbeyInterchangeSprite* pAddAfter, INT cTicksRemaining)
{
	HRESULT hr;
	CEffect* pEffect = __new CEffect(this, pCanvas, nLayer, pSprite, cTicksRemaining);

	CheckAlloc(pEffect);
	if(pAddAfter)
		Check(pCanvas->AddSpriteAfter(nLayer, pSprite, pAddAfter, NULL));
	else
		Check(pCanvas->AddSprite(nLayer, pSprite, NULL));
	Check(m_aActions.Append(pEffect));

Cleanup:
	SafeRelease(pEffect);
	return hr;
}

HRESULT CCombatScreen::PerformAttack (CMovingObject* pSource, CMovingObject* pTarget, BOOL fRange)
{
	HRESULT hr;
	QuadooVM::QVARIANT qvArgs[6];
	QuadooVM::QVPARAMS qvParams;

	Check(UpdatePositionInJSON(pSource));
	Check(UpdatePositionInJSON(pTarget));

	qvArgs[5].rstrVal = pSource->m_rstrOwner;
	qvArgs[5].eType = QuadooVM::String;

	qvArgs[4].pJSONObject = pSource->m_pDef;
	qvArgs[4].eType = QuadooVM::JSONObject;

	qvArgs[3].lVal = pSource->m_nLevel;
	qvArgs[3].eType = QuadooVM::I4;

	qvArgs[2].rstrVal = pTarget->m_rstrOwner;
	qvArgs[2].eType = QuadooVM::String;

	qvArgs[1].pJSONObject = pTarget->m_pDef;
	qvArgs[1].eType = QuadooVM::JSONObject;

	qvArgs[0].lVal = pTarget->m_nLevel;
	qvArgs[0].eType = QuadooVM::I4;

	qvParams.pqvArgs = qvArgs;
	qvParams.cArgs = ARRAYSIZE(qvArgs);

	Check(InvokeAttackMethod(pSource, pTarget, fRange ? RSTRING_CAST(L"RangeAttack") : RSTRING_CAST(L"MeleeAttack"), !fRange, &qvParams));

Cleanup:
	return hr;
}

HRESULT CCombatScreen::PerformSpellAttack (IJSONObject* pAbilities, IJSONObject* pWeapon, CMovingObject* pTarget)
{
	HRESULT hr;
	QuadooVM::QVARIANT qvArgs[5];
	QuadooVM::QVPARAMS qvParams;

	Check(UpdatePositionInJSON(pTarget));

	qvArgs[4].pJSONObject = pAbilities;
	qvArgs[4].eType = QuadooVM::JSONObject;

	qvArgs[3].pJSONObject = pWeapon;
	qvArgs[3].eType = QuadooVM::JSONObject;

	qvArgs[2].rstrVal = pTarget->m_rstrOwner;
	qvArgs[2].eType = QuadooVM::String;

	qvArgs[1].pJSONObject = pTarget->m_pDef;
	qvArgs[1].eType = QuadooVM::JSONObject;

	qvArgs[0].lVal = pTarget->m_nLevel;
	qvArgs[0].eType = QuadooVM::I4;

	qvParams.pqvArgs = qvArgs;
	qvParams.cArgs = ARRAYSIZE(qvArgs);

	Check(InvokeAttackMethod(NULL, pTarget, RSTRING_CAST(L"SpellAttack"), FALSE, &qvParams));

Cleanup:
	return hr;
}

HRESULT CCombatScreen::InvokeAttackMethod (__in_opt CMovingObject* pSource, CMovingObject* pTarget, RSTRING rstrAttackMethod, BOOL fUpdateSource, QuadooVM::QVPARAMS* pqvParams)
{
	HRESULT hr;
	QuadooVM::QVARIANT qvResult; qvResult.eType = QuadooVM::Null;
	IJSONObject* pResult;

	CheckIf(NULL == m_pCombat, E_UNEXPECTED);

	Check(m_pCombat->Invoke(NULL, rstrAttackMethod, pqvParams, &qvResult));
	if(QuadooVM::Object == qvResult.eType)
	{
		QuadooVM::QVARIANT qvInner;
		QuadooVM::QVPARAMS qvParams;

		qvParams.cArgs = 0;
		Check(qvResult.pObject->Invoke(NULL, RSTRING_CAST(L"ToString"), &qvParams, &qvInner));
		if(QuadooVM::String == qvInner.eType)
		{
			OutputDebugStringW(RStrToWide(qvInner.rstrVal));
			OutputDebugStringW(L"\r\n");
		}
		QVMClearVariant(&qvInner);
	}
	CheckIf(QuadooVM::JSONObject != qvResult.eType, E_UNEXPECTED);
	pResult = qvResult.pJSONObject;

	Check(ShowBloodSpatter(pResult, L"target_impact", pTarget));
	Check(UpdateUnitAsync(pResult, L"target_figures", pTarget));

	if(fUpdateSource)
	{
		Assert(pSource);
		Check(ShowBloodSpatter(pResult, L"source_impact", pSource));
		Check(UpdateUnitAsync(pResult, L"source_figures", pSource));
	}

Cleanup:
	QVMClearVariant(&qvResult);
	return hr;
}

HRESULT CCombatScreen::ShowBloodSpatter (IJSONObject* pResult, PCWSTR pcwzImpact, CMovingObject* pUnit)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	DOUBLE dblImpact;
	INT nAnimation = 0;

	Check(pResult->FindNonNullValueW(pcwzImpact, &srv));
	Check(srv->GetDouble(&dblImpact));

	if(dblImpact > 0.0)
	{
		nAnimation = static_cast<INT>(dblImpact * 3.0) + 1;
		if(nAnimation > 3)
			nAnimation = 3;
	}
	Check(pUnit->CreateBloodOverlays(this, m_pMain, m_nUnitLayer, m_pBlood, nAnimation));

Cleanup:
	return hr;
}

HRESULT CCombatScreen::UpdateUnitAsync (IJSONObject* pResult, PCWSTR pcwzFigures, CMovingObject* pUnit)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	CUnitUpdater* pUpdater = NULL;
	INT cFigures;

	Check(pResult->FindNonNullValueW(pcwzFigures, &srv));
	Check(srv->GetInteger(&cFigures));

	pUpdater = __new CUnitUpdater(this, pUnit, m_pMain, m_nUnitLayer, cFigures, ATTACK_ANI_TICKS);
	CheckAlloc(pUpdater);
	Check(m_aActions.Append(pUpdater));

Cleanup:
	SafeRelease(pUpdater);
	return hr;
}

HRESULT CCombatScreen::RemoveUnitFromBoard (CMovingObject* pUnit)
{
	HRESULT hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

	for(sysint i = 0; i < m_aObjects.Length(); i++)
	{
		if(m_aObjects[i] == pUnit)
		{
			Check(m_aRemoved.Append(pUnit));
			if(m_pSelected == pUnit)
			{
				if(m_pSelectedTile)
				{
					m_pMain->RemoveSprite(m_nTileEffectsLayer, m_pSelectedTile);
					SafeRelease(m_pSelectedTile);
				}
				m_pSelected = NULL;
				m_pCombatBar->Clear();
			}
			m_aObjects.Remove(i, NULL);
			break;
		}
	}

Cleanup:
	return hr;
}

HRESULT CCombatScreen::ExpandStats (IJSONObject* pBase, INT nLevel, __deref_out IJSONObject** ppStats)
{
	HRESULT hr;
	QuadooVM::QVARIANT qvArg;
	QuadooVM::QVARIANT qvResult; qvResult.eType = QuadooVM::Null;

	qvArg.eType = QuadooVM::I4;
	qvArg.lVal = nLevel;
	Check(m_pHost->GetVM()->PushValue(&qvArg));

	qvArg.eType = QuadooVM::JSONObject;
	qvArg.pJSONObject = pBase;
	Check(m_pHost->GetVM()->PushValue(&qvArg));

	Check(m_pHost->GetVM()->RunFunction(m_idxExpandStats, &qvResult));
	CheckIf(QuadooVM::JSONObject != qvResult.eType, E_UNEXPECTED);

	*ppStats = qvResult.pJSONObject;
	qvResult.eType = QuadooVM::Null;

Cleanup:
	QVMClearVariant(&qvResult);
	return hr;
}

HRESULT CCombatScreen::GetHealthPct (IJSONObject* pUnit, IJSONObject* pStats, __out DOUBLE* pdblHealth)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	INT nDamage, nHits, cFigures, nTotal;

	Check(pUnit->FindNonNullValueW(L"damage", &srv));
	Check(srv->GetInteger(&nDamage));
	srv.Release();

	Check(pStats->FindNonNullValueW(L"hits", &srv));
	Check(srv->GetInteger(&nHits));
	srv.Release();

	// TODO - Fix for hydra!
	Check(JSONGetValueFromObject(pUnit, SLP(L"base:figures"), &srv));
	Check(srv->GetInteger(&cFigures));

	nTotal = cFigures * nHits;
	*pdblHealth = static_cast<DOUBLE>(nTotal - nDamage) / static_cast<DOUBLE>(nTotal);

Cleanup:
	return hr;
}

HRESULT CCombatScreen::UpdateStatsPanel (VOID)
{
	INT xTile = m_xHoverTile, yTile = m_yHoverTile;

	m_xHoverTile = -1;
	m_yHoverTile = -1;

	return UpdateStatsPanel(xTile, yTile);
}

HRESULT CCombatScreen::ShowSpellBook (VOID)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONArray> srSpells;
	CSpellBook* pSpellBook = NULL;
	INT nMagicPower;

	Check(m_pPlacements->FindNonNullValueW(L"spells", &srv));
	Check(srv->GetArray(&srSpells));
	srv.Release();

	Check(m_pPlacements->FindNonNullValueW(L"mana", &srv));
	Check(srv->GetInteger(&nMagicPower));

	pSpellBook = __new CSpellBook(m_pSurface, this, m_pWizard, m_pFonts, nMagicPower);
	CheckAlloc(pSpellBook);

	Check(pSpellBook->Initialize(srSpells));
	Check(AddPopup(pSpellBook));

Cleanup:
	SafeRelease(pSpellBook);
	return hr;
}

HRESULT CCombatScreen::ShowSelectedUnitInfo (VOID)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeSprite> srUnitSprite;
	TStackRef<IJSONObject> srStats;
	CMovingObject* pSelected;
	CUnitStats* pUnitStats = NULL;

	CheckIf(NULL == m_pSelected, S_FALSE);
	pSelected = static_cast<CMovingObject*>(m_pSelected);

	pUnitStats = __new CUnitStats(m_pSurface, m_pFonts, this);
	CheckAlloc(pUnitStats);

	Check(ExpandStats(pSelected->m_pDef, pSelected->m_nLevel, &srStats));

	Check(pSelected->CloneSprite(&srUnitSprite));
	Check(srUnitSprite->SelectAnimation(9));

	Check(pUnitStats->Initialize(pSelected->m_pDef, srStats, pSelected->m_nLevel, srUnitSprite));
	Check(AddPopup(pUnitStats));

Cleanup:
	SafeRelease(pUnitStats);
	return hr;
}

HRESULT CCombatScreen::LoadData (VOID)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv, srvStats, srvMelee;
	TStackRef<IJSONArray> srDirectory, srArray;
	TStackRef<CSIFPackage> srSubPackage;
	sysint cItems;
	RSTRING rstrType = NULL, rstrFile = NULL;

	Check(m_pPackage->GetJSONData(SLP(L"combat stats\\combat_stats.json"), &srv));
	Check(JSONGetValue(srv, SLP(L"stats"), &srvStats));
	Check(srvStats->GetArray(&m_pCombatStats));
	srv.Release();

	Check(m_pPackage->GetJSONData(SLP(L"combat stats\\combat_melee.json"), &srv));
	Check(JSONGetValue(srv, SLP(L"melee"), &srvMelee));
	Check(srvMelee->GetArray(&m_pCombatMelee));
	srv.Release();

	Check(m_pPackage->OpenDirectory(SLP(L"combat stats"), &srSubPackage));
	Check(srSubPackage->GetDirectory(&srDirectory));
	cItems = srDirectory->Count();
	for(sysint i = 0; i < cItems; i++)
	{
		TStackRef<IJSONValue> srvType;

		Check(srDirectory->GetValue(i, &srv));
		if(SUCCEEDED(JSONGetValue(srv, SLP(L"data:type"), &srvType)))
		{
			INT nResult;

			Check(srvType->GetString(&rstrType));
			Check(RStrCompareIW(rstrType, L"animation", &nResult));

			if(0 == nResult)
			{
				TStackRef<IJSONObject> srItem;

				srv.Release();

				Check(srDirectory->GetObject(i, &srItem));
				Check(srItem->FindNonNullValueW(L"data", &srv));
				srItem.Release();
				Check(srv->GetObject(&srItem));
				srv.Release();

				RStrRelease(rstrType); rstrType = NULL;

				Check(srItem->FindNonNullValueW(L"animation", &srv));
				Check(srv->GetString(&rstrType));
				Check(m_mapCombatAnimations.Add(rstrType, srItem));
				srItem->AddRef();
			}

			RStrRelease(rstrType); rstrType = NULL;
		}

		srv.Release();
	}
	srDirectory.Release();
	srSubPackage.Release();

	Check(m_pPackage->OpenDirectory(SLP(L"units"), &srSubPackage));
	Check(LoadUnits(srSubPackage));

Cleanup:
	RStrRelease(rstrType);
	RStrRelease(rstrFile);
	return hr;
}

HRESULT CCombatScreen::LoadUnits (CSIFPackage* pSubPackage)
{
	HRESULT hr;
	TStackRef<IJSONArray> srDirectory;
	TStackRef<IJSONValue> srvData;
	sysint cItems;
	RSTRING rstrName = NULL, rstrImport = NULL;
	CUnitData* pUnitData = NULL;

	Check(pSubPackage->GetDirectory(&srDirectory));
	cItems = srDirectory->Count();

	CheckIf(0 == cItems, S_FALSE);

	for(sysint i = 0; i < cItems; i++)
	{
		TStackRef<IJSONObject> srItem;
		TStackRef<IJSONValue> srv;

		Check(srDirectory->GetObject(i, &srItem));

		if(SUCCEEDED(srItem->FindNonNullValueW(L"dir", &srv)))
		{
			TStackRef<CSIFPackage> srNext;

			srv.Release();
			Check(srItem->FindNonNullValueW(L"name", &srv));
			Check(srv->GetString(&rstrName));

			Check(pSubPackage->OpenDirectory(RStrToWide(rstrName), RStrLen(rstrName), &srNext));
			Check(LoadUnits(srNext));

			RStrRelease(rstrName); rstrName = NULL;
		}
	}

	if(SUCCEEDED(pSubPackage->GetJSONData(SLP(L"data.json"), &srvData)))
	{
		TStackRef<IJSONObject> srData, srStats;
		TStackRef<IJSONValue> srvName, srv;
		TStackRef<CSIFPackage> srImport;

		Check(srvData->GetObject(&srData));
		Check(srData->FindNonNullValueW(L"name", &srvName));
		Check(srvName->GetString(&rstrName));

		if(SUCCEEDED(srData->RemoveValueW(L"import", &srv)))
		{
			Check(srv->GetString(&rstrImport));
			Check(m_pPackage->OpenDirectory(RStrToWide(rstrImport), RStrLen(rstrImport), &srImport));
			RStrRelease(rstrImport); rstrImport = NULL;
			srv.Release();
		}

		Check(srData->FindNonNullValueW(L"stats", &srv));
		Check(srv->GetObject(&srStats));
		srv.Release();

		// Ensure all units have a "melee" statistic.  Create a default stat if it's missing.
		if(!srStats->HasField(RSTRING_CAST(L"melee")))
		{
			Check(JSONParse(NULL, SLP(L"{stat:\"normal melee\",name:\"Sword\",value:0}"), &srv));
			Check(srStats->AddValueW(L"melee", srv));
		}

		pUnitData = __new CUnitData(rstrName, pSubPackage, srImport, srData);
		CheckAlloc(pUnitData);
		Check(m_mapUnits.Add(rstrName, pUnitData));
		pUnitData = NULL;

#ifdef	_DEBUG
		WCHAR wzDebugLine[512];
		Check(Formatting::TPrintF(wzDebugLine, ARRAYSIZE(wzDebugLine), NULL, L"Loaded unit: %r\r\n", rstrName));
		OutputDebugString(wzDebugLine);
#endif
	}

Cleanup:
	SafeDelete(pUnitData);
	RStrRelease(rstrImport);
	RStrRelease(rstrName);
	return hr;
}

HRESULT CCombatScreen::LoadProjectiles (VOID)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONObject> srDef;
	TStackRef<IJSONArray> srArray;
	TStackRef<CSIFPackage> srRangedAttacks;
	sysint c;
	RSTRING rstrName = NULL;
	CProjectileData* pProjectile = NULL;

	Check(m_pPackage->GetJSONData(SLP(L"combat stats\\Projectile.json"), &srv));
	Check(srv->GetObject(&srDef));
	srv.Release();

	Check(m_pPackage->OpenDirectory(SLP(L"ranged attacks"), &srRangedAttacks));
	Check(srRangedAttacks->GetDirectory(&srArray));

	c = srArray->Count();
	for(sysint i = 0; i < c; i++)
	{
		TStackRef<CSIFPackage> srContainer;
		TStackRef<IJSONObject> srItem, srProjectile;
		TStackRef<ISimbeyInterchangeAnimator> srAnimator;

		Check(srArray->GetObject(i, &srItem));
		Check(srItem->FindNonNullValueW(L"name", &srv));
		Check(srv->GetString(&rstrName));
		srv.Release();

		Check(srRangedAttacks->OpenDirectory(RStrToWide(rstrName), RStrLen(rstrName), &srContainer));
		Check(CreateAnimator(srContainer, srDef, L"range_attack.sif", &srAnimator, TRUE));
		Check(srContainer->GetJSONData(SLP(L"range_attack.json"), &srv));
		Check(srv->GetObject(&srProjectile));
		srv.Release();

		pProjectile = __new CProjectileData(srAnimator, srProjectile);
		CheckAlloc(pProjectile);

		Check(m_mapProjectiles.Add(rstrName, pProjectile));
		pProjectile = NULL;

		RStrRelease(rstrName); rstrName = NULL;
	}

Cleanup:
	SafeDelete(pProjectile);
	RStrRelease(rstrName);
	return hr;
}

HRESULT CCombatScreen::LoadSprites (VOID)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeAnimator> srAnimator;
	sysint nLayer;
	INT xTileStart, yTileStart, xTileEnd, yTileEnd;
	CInteractiveLayer* pLayer = NULL;

	Check(m_pMain->AddLayer(FALSE, FALSE, 0, &nLayer));
	Check(LoadAnimator(SLP(L"graphics\\Tiles.json"), L"combat\\terrain\\arcanus\\default\\standard\\terrain.sif", &srAnimator, FALSE));

	if(m_pBackground)
	{
		TStackRef<ISimbeyInterchangeFile> srClouds, srWalls;

		Check(m_pPackage->OpenSIF(L"combat\\clouds\\background\\clouds.sif", &srClouds));
		Check(m_pPackage->OpenSIF(L"combat\\clouds\\default\\walls.sif", &srWalls));

		Check(ConfigureBackground(srClouds, 0,   0,   0, &m_pCornerA));
		Check(m_pBackground->AddSprite(0, m_pCornerA, NULL));

		Check(ConfigureBackground(srClouds, 1, 320,   0, &m_pCornerB));
		Check(m_pBackground->AddSprite(0, m_pCornerB, NULL));

		Check(ConfigureBackground(srClouds, 2,   0, 181, &m_pCornerC));
		Check(m_pBackground->AddSprite(0, m_pCornerC, NULL));

		Check(ConfigureBackground(srClouds, 3, 320, 181, &m_pCornerD));
		Check(m_pBackground->AddSprite(0, m_pCornerD, NULL));

		for(INT y = c_rcFloatingTiles.top; y <= c_rcFloatingTiles.bottom; y++)
		{
			for(INT x = c_rcFloatingTiles.left; x <= c_rcFloatingTiles.right; x++)
			{
				TStackRef<ISimbeyInterchangeSprite> srTile;
				INT xTile, yTile;

				PlaceTile(m_pMain, x, y, nLayer, srAnimator, rand() % 4, &srTile);
				srTile->GetPosition(xTile, yTile);

				if(y == c_rcFloatingTiles.bottom)
				{
					TStackRef<ISimbeyInterchangeSprite> srWall;
					Check(ConfigureBackground(srWalls, 0, xTile, yTile + 8, &srWall));
					Check(m_pMain->AddSprite(nLayer, srWall, NULL));
				}

				if(x == c_rcFloatingTiles.right)
				{
					TStackRef<ISimbeyInterchangeSprite> srWall;
					Check(ConfigureBackground(srWalls, 1, xTile + 15, yTile + 8, &srWall));
					Check(m_pMain->AddSprite(nLayer, srWall, NULL));
				}
			}
		}

		srWalls->Close();
		srClouds->Close();
	}
	else
	{
		INT yMid, nAdjust = 0;

		m_Isometric.GetTileRange(m_pMain, &xTileStart, &yTileStart, &xTileEnd, &yTileEnd);
		yMid = (yTileStart + yTileEnd) / 2;

		for(INT y = yTileStart; y < yTileEnd; y++)
		{
			if(0 <= y && y < MAP_HEIGHT)
			{
				INT xStart = xTileStart - nAdjust;
				INT xEnd = xTileEnd + nAdjust;
				if(0 > xStart)
					xStart = 0;
				if(MAP_WIDTH < xEnd)
					xEnd = MAP_WIDTH;
				for(INT x = xStart; x < xEnd; x++)
					PlaceTile(m_pMain, x, y, nLayer, srAnimator, rand() % 4, NULL);
			}

			if(y < yMid)
				nAdjust++;
			else
				nAdjust--;
		}
	}

	srAnimator.Release();

	Check(m_pMain->AddLayer(TRUE, FALSE, 0, &m_nTileEffectsLayer));

	Check(LoadAnimator(SLP(L"graphics\\MoveTo.json"), L"graphics\\MoveTo.sif", &srAnimator, FALSE));
	Check(PlaceTile(m_pMain, MAP_WIDTH / 2, MAP_HEIGHT / 2, m_nTileEffectsLayer, srAnimator, 0, &m_pMoveTo));

	srAnimator.Release();

	Check(static_cast<CInteractiveCanvas*>(m_pMain)->AddInteractiveLayer(TRUE, FALSE, 0, this, &pLayer));
	m_nUnitLayer = pLayer->GetLayer();

	Check(PlaceUnitsAndBuildings(m_nUnitLayer));
	m_Isometric.SortIsometricLayer(m_pMain, m_nUnitLayer);

	Check(LoadAnimator(SLP(L"graphics\\MoveType.json"), L"graphics\\MoveType.sif", &srAnimator, FALSE));
	Check(PlaceTile(m_pMouse, MAP_WIDTH / 2, MAP_HEIGHT / 2, 0, srAnimator, 3, &m_pMoveType));
	srAnimator.Release();

	Check(LoadCombatStats());

	Check(LoadAnimator(SLP(L"graphics\\CombatUI.json"), L"graphics\\CombatUI.sif", &srAnimator, FALSE));
	Check(m_pCombatBar->Load(srAnimator, m_pUnitStats, m_pYellowFont, m_pCombatBarFont));

	Check(LoadAnimator(SLP(L"combat\\blood\\Blood.json"), L"combat\\blood\\combat_blood.sif", &m_pBlood, TRUE));

Cleanup:
	SafeRelease(pLayer);
	return hr;
}

HRESULT CCombatScreen::LoadCombatStats (VOID)
{
	HRESULT hr;
	ISimbeyInterchangeFile* pSIF = NULL;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONObject> srDef;
	CMemoryStream stmDef;
	DWORD cLayers;
	ULONG cb;
	RSTRING rstrIcon = NULL, rstrImage = NULL;

	CheckIf(NULL == m_pCombatStats, E_UNEXPECTED);

	Check(m_pPackage->OpenSIF(L"combat stats\\combat_stats.sif", &pSIF));

	cLayers = pSIF->GetLayerCount();

	Check(RStrCreateW(LSP(L"icon"), &rstrIcon));
	Check(stmDef.TWrite(SLP(L"{images:["), &cb));

	for(DWORD i = 0; i < cLayers; i++)
	{
		TStackRef<ISimbeyInterchangeFileLayer> srLayer;
		TStackRef<IJSONObject> srStat;
		WCHAR wzFile[MAX_PATH];

		Check(pSIF->GetLayerByIndex(i, &srLayer));
		Check(srLayer->GetName(wzFile, ARRAYSIZE(wzFile)));
		Check(RStrCreateW(TStrLenAssert(wzFile), wzFile, &rstrImage));
		Check(JSONFindArrayObject(m_pCombatStats, RSTRING_CAST(L"image"), rstrImage, &srStat, NULL));
		RStrRelease(rstrImage); rstrImage = NULL;

		Check(JSONCreateInteger(i, &srv));
		Check(srStat->AddValue(rstrIcon, srv));
		srv.Release();

		if(0 < i)
			Check(stmDef.TWrite(SLP(L","), &cb));
		Check(Stream::TPrintF(&stmDef, L"{ layer: %u }", i));
	}

	Check(stmDef.TWrite(SLP(L"],animations:["), &cb));

	for(DWORD i = 0; i < cLayers; i++)
	{
		if(0 < i)
			Check(stmDef.TWrite(SLP(L","), &cb));
		Check(Stream::TPrintF(&stmDef, L"{ frames: [{ image: %u, ticks: 1, xoffset: 0, yoffset: 0 }] }", i));
	}

	Check(stmDef.TWrite(SLP(L"]}"), &cb));
	Check(JSONParse(NULL, stmDef.TGetReadPtr<WCHAR>(), stmDef.DataRemaining() / sizeof(WCHAR), &srv));
	Check(srv->GetObject(&srDef));

	Check(CreateAnimator(pSIF, srDef, &m_pUnitStats, FALSE));

Cleanup:
	RStrRelease(rstrImage);
	RStrRelease(rstrIcon);
	if(pSIF)
	{
		pSIF->Close();
		pSIF->Release();
	}
	return hr;
}

HRESULT CCombatScreen::LoadMusic (VOID)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONArray> srMusic;
	WCHAR wzMusic[MAX_PATH];
	INT cchMusic;
	CMemoryStream stmBattle;
	ULARGE_INTEGER uliSize;
	RSTRING rstrName = NULL;

	Check(m_pWizard->FindNonNullValueW(L"music", &srv));
	Check(srv->GetArray(&srMusic));
	Check(srMusic->GetString(rand() % srMusic->Count(), &rstrName));
	Check(Formatting::TPrintF(wzMusic, ARRAYSIZE(wzMusic), &cchMusic, L"music\\%r", rstrName));

	Check(m_pPackage->ReadFile(wzMusic, cchMusic, &stmBattle));
	uliSize.QuadPart = stmBattle.DataRemaining();
	Check(m_pHost->LoadMIDI(&stmBattle, &uliSize, &m_midiFile));
	Check(m_pHost->PlayMIDI(&m_midiFile));

Cleanup:
	RStrRelease(rstrName);
	return hr;
}

HRESULT CCombatScreen::PlaceTile (CSIFCanvas* pCanvas, INT xTile, INT yTile, sysint nLayer, ISimbeyInterchangeAnimator* pAnimator, INT nAnimation, __deref_out_opt ISimbeyInterchangeSprite** ppSprite)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeSprite> srSprite;
	INT xIso, yIso, x, y;

	m_Isometric.TileToView(xTile, yTile, &xIso, &yIso);
	m_Isometric.IsometricToView(m_pMain, xIso, yIso, &x, &y);

	Check(pAnimator->CreateSprite(&srSprite));
	Check(srSprite->SelectAnimation(nAnimation));
	srSprite->SetPosition(x, y);
	Check(pCanvas->AddSprite(nLayer, srSprite, NULL));

	if(ppSprite)
		*ppSprite = srSprite.Detach();

Cleanup:
	return hr;
}

HRESULT CCombatScreen::AddStaticObject (INT xTile, INT yTile, sysint nLayer, ISimbeyInterchangeAnimator* pAnimator, INT nAnimation)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeSprite> srSprite;
	CObject* pObject = NULL;

	pObject = __new CObject(xTile, yTile);
	CheckAlloc(pObject);

	Check(PlaceTile(m_pMain, xTile, yTile, nLayer, pAnimator, nAnimation, &srSprite));
	Check(pObject->AddSprite(srSprite, true));

	Check(m_aObjects.Append(pObject));
	pObject = NULL;

Cleanup:
	SafeDelete(pObject);
	return hr;
}

HRESULT CCombatScreen::AddMovingObject (IJSONObject* pDef, RSTRING rstrOwner, INT xTile, INT yTile, sysint nLayer, ISimbeyInterchangeAnimator* pAnimator, INT nDirection, INT nLevel, INT (*pfnBaseAnimation)(INT), FMOD::Sound* pMove, FMOD::Sound* pMelee, FMOD::Sound* pRange, __deref_opt_out CMovingObject** ppObject)
{
	HRESULT hr;
	TStackRef<IJSONObject> srStats, srUnit, srMove;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONArray> srMoves;
	CMovingObject* pObject = NULL;
	INT cFigures;
	RSTRING rstrMoveType = NULL;

	// Create a new JSON object and add the unit definition to the
	// new object under the "base" name.
	Check(JSONCreateObject(&srUnit));
	Check(JSONWrapObject(pDef, &srv));
	Check(srUnit->AddValueW(L"base", srv));
	srv.Release();

	// Add the "damage" value.
	Check(JSONCreateInteger(0, &srv));
	Check(srUnit->AddValueW(L"damage", srv));
	srv.Release();

	// Get the "figures" value and validate it.
	Check(pDef->FindNonNullValueW(L"figures", &srv));
	Check(srv->GetInteger(&cFigures));
	CheckIf(cFigures > ARRAYSIZE(c_ptFigureOffsets), E_INVALIDARG);
	srv.Release();

	// Get the "stats" data for looking up data.
	Check(pDef->FindNonNullValueW(L"stats", &srv));
	Check(srv->GetObject(&srStats));
	srv.Release();

	// Get the "move" data.
	Check(srStats->FindNonNullValueW(L"move", &srv));
	Check(srv->GetArray(&srMoves));
	srv.Release();

	// Code for an actual game would figure out what kind of movement type matches the terrain.
	Check(RStrCreateW(LSP(L"air"), &rstrMoveType));
	if(FAILED(JSONFindArrayObject(srMoves, RSTRING_CAST(L"stat"), rstrMoveType, &srMove, NULL)))
	{
		RStrRelease(rstrMoveType);

		Check(RStrCreateW(LSP(L"land"), &rstrMoveType));
		if(FAILED(JSONFindArrayObject(srMoves, RSTRING_CAST(L"stat"), rstrMoveType, &srMove, NULL)))
		{
			RStrRelease(rstrMoveType);
			Check(RStrCreateW(LSP(L"water"), &rstrMoveType));
			Check(JSONFindArrayObject(srMoves, RSTRING_CAST(L"stat"), rstrMoveType, &srMove, NULL));
		}
	}

	// Add the selected movement object to the new unit object.
	Check(JSONWrapObject(srMove, &srv));
	Check(srUnit->AddValueW(L"moveStat", srv));

	pObject = __new CMovingObject(srUnit, rstrOwner, xTile, yTile, nDirection, nLevel, pfnBaseAnimation, pMove, pMelee, pRange);
	CheckAlloc(pObject);

	for(INT i = 0; i < cFigures; i++)
	{
		const POINT* pcptOffsets = c_ptFigureOffsets[cFigures - 1];
		TStackRef<ISimbeyInterchangeSprite> srSprite;
		INT x, y;

		Check(PlaceTile(m_pMain, xTile, yTile, nLayer, pAnimator, pfnBaseAnimation(nDirection), &srSprite));
		srSprite->GetPosition(x, y);
		x += pcptOffsets[i].x;
		y += pcptOffsets[i].y;
		srSprite->SetPosition(x, y);
		Check(pObject->AddSprite(srSprite, true));
	}

	Check(m_aObjects.Append(pObject));
	if(ppObject)
		*ppObject = pObject;
	pObject = NULL;

Cleanup:
	RStrRelease(rstrMoveType);
	SafeDelete(pObject);
	return hr;
}

HRESULT CCombatScreen::UpdatePositionInJSON (CMovingObject* pObject)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;

	Check(JSONCreateInteger(pObject->m_xTile, &srv));
	Check(pObject->m_pDef->AddValueW(L"x", srv));
	srv.Release();

	Check(JSONCreateInteger(pObject->m_yTile, &srv));
	Check(pObject->m_pDef->AddValueW(L"y", srv));

Cleanup:
	return hr;
}

HRESULT CCombatScreen::CreateRandomAbility (IJSONArray* pAbilities, RSTRING rstrRandom, __deref_out IJSONValue** ppvAbility)
{
	HRESULT hr;
	TStackRef<IJSONArray> srFound;
	PCWSTR pcwzRandom = RStrToWide(rstrRandom);
	RSTRING rstrAbility = NULL;

	Check(JSONCreateArray(&srFound));

	for(;;)
	{
		INT nPick = rand() % ARRAYSIZE(c_rgRandom);

		if(0 == TStrCmpAssert(c_rgRandom[nPick].pcwzRandomType, pcwzRandom) ||
			0 == TStrCmpAssert(c_rgRandom[nPick].pcwzRandomType, L"any"))
		{
			TStackRef<IJSONValue> srv;
			TStackRef<IJSONObject> srAbility;
			PCWSTR pcwzAbility = c_rgRandom[nPick].pcwzAbility;

			srFound->Clear();

			Check(JSONCreateStringW(pcwzAbility, TStrLenAssert(pcwzAbility), &srv));
			Check(JSONCopyObjects(pAbilities, 0, RSTRING_CAST(L"name"), srv, srFound, CopyObjects::Default));

			if(c_rgRandom[nPick].fAllowDouble)
			{
				if(2 == srFound->Count())
					continue;
			}
			else if(1 == srFound->Count())
				continue;

			Check(srv->GetString(&rstrAbility));
			srv.Release();

			Check(JSONCreateObject(&srAbility));

			Check(JSONCreateString(rstrAbility, &srv));
			Check(srAbility->AddValueW(L"name", srv));
			srv.Release();

			Check(JSONCreateStringW(SLP(L"ability"), &srv));
			Check(srAbility->AddValueW(L"type", srv));
			Check(JSONWrapObject(srAbility, ppvAbility));
			break;
		}
	}

Cleanup:
	RStrRelease(rstrAbility);
	return hr;
}

HRESULT CCombatScreen::AssignRandomStats (CUnitData* pUnitData, __deref_out IJSONObject** ppUnit)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONArray> srAbilities, srCloneAbilities;
	TStackRef<IJSONObject> srClone;
	sysint cAbilities;
	RSTRING rstrRandom = NULL;

	Check(pUnitData->m_pData->FindNonNullValueW(L"abilities", &srv));
	Check(srv->GetArray(&srAbilities));
	srv.Release();

	cAbilities = srAbilities->Count();
	for(sysint i = 0; i < cAbilities; i++)
	{
		TStackRef<IJSONObject> srAbility;

		Check(srAbilities->GetObject(i, &srAbility));
		if(SUCCEEDED(srAbility->FindNonNullValueW(L"random", &srv)))
		{
			Check(srv->GetString(&rstrRandom));
			srv.Release();

			if(NULL == srClone)
			{
				Check(JSONCloneObject(pUnitData->m_pData, &srv, TRUE));
				Check(srv->GetObject(&srClone));
				srv.Release();

				Check(srClone->FindNonNullValueW(L"abilities", &srv));
				Check(srv->GetArray(&srCloneAbilities));
				srv.Release();
			}

			Check(CreateRandomAbility(srCloneAbilities, rstrRandom, &srv));
			Check(srCloneAbilities->Replace(i, srv));
			srv.Release();

			RStrRelease(rstrRandom); rstrRandom = NULL;
		}
	}

	if(srClone)
	{
		TStackRef<IJSONArray> srFound;

		Check(JSONCreateArray(&srFound));

		for(INT i = 0; i < ARRAYSIZE(c_rgRandom); i++)
		{
			if(c_rgRandom[i].fAllowDouble)
			{
				PCWSTR pcwzAbility = c_rgRandom[i].pcwzAbility;
				TStackRef<IJSONValue> srv;
				TStackRef<IJSONObject> srAbility;
				sysint idxAbility;

				Check(JSONCreateStringW(pcwzAbility, TStrLenAssert(pcwzAbility), &srv));
				Check(srv->GetString(&rstrRandom));

				if(SUCCEEDED(JSONFindArrayObject(srCloneAbilities, RSTRING_CAST(L"name"), rstrRandom, &srAbility, &idxAbility)))
				{
					srFound->Clear();

					Check(JSONCopyObjects(srCloneAbilities, idxAbility + 1, RSTRING_CAST(L"name"), srv, srFound, CopyObjects::Remove));
					if(srFound->Count())
					{
						TStackRef<IJSONValue> srvSuper;
						RSTRING rstrSuper;

						Check(RStrFormatW(&rstrSuper, L"Super %r", rstrRandom));
						hr = JSONCreateString(rstrSuper, &srvSuper);
						RStrRelease(rstrSuper);
						Check(hr);

						Check(srAbility->AddValueW(L"name", srvSuper));
						Check(srAbility->AddValueW(L"super", srv));
					}
				}

				RStrRelease(rstrRandom); rstrRandom = NULL;
			}
		}
		*ppUnit = srClone.Detach();
	}
	else
		SetInterface(*ppUnit, pUnitData->m_pData);

Cleanup:
	RStrRelease(rstrRandom);
	return hr;
}

HRESULT CCombatScreen::PlaceUnit (RSTRING rstrName, RSTRING rstrOwner, INT xTile, INT yTile, sysint nLayer, INT nDirection, INT nLevel, COLORREF crColorize, bool fEnchanted, __deref_opt_out CMovingObject** ppObject)
{
	HRESULT hr;
	TStackRef<IJSONValue> srvUnits, srv, srvData;
	TStackRef<IJSONObject> srUnit, srStats;
	TStackRef<ISimbeyInterchangeAnimator> srAnimator;
	IJSONObject* pAnimation;
	RSTRING rstrMelee = NULL, rstrProjectile = NULL, rstrValue = NULL;
	FMOD::Sound* pMoveSound, *pMeleeSound = NULL, *pRangeSound = NULL;
	INT (*pfnBaseAnimation)(INT);
	CUnitData* pUnitData;
	CSIFPackage* pImport;

	Check(m_mapUnits.Find(rstrName, &pUnitData));
	Check(AssignRandomStats(pUnitData, &srUnit));

	Check(srUnit->FindNonNullValueW(L"stats", &srv));
	Check(srv->GetObject(&srStats));
	srv.Release();

	Check(srStats->FindNonNullValueW(L"move", &srv));
	Check(srv->GetArrayItem(0, &srvData));
	srv.Release();

	Check(JSONGetValue(srvData, SLP(L"sound"), &srv));
	Check(srv->GetString(&rstrValue));
	Check(m_pHost->FindSound(rstrValue, &pMoveSound));
	RStrRelease(rstrValue); rstrValue = NULL;
	srv.Release();

	srvData.Release();
	if(SUCCEEDED(srStats->FindNonNullValueW(L"melee", &srvData)))
	{
		TStackRef<IJSONObject> srMelee;

		Check(JSONGetValue(srvData, SLP(L"name"), &srv));
		Check(srv->GetString(&rstrMelee));
		srv.Release();

		Check(JSONFindArrayObject(m_pCombatMelee, RSTRING_CAST(L"name"), rstrMelee, &srMelee, NULL));
		Check(srMelee->FindNonNullValueW(L"sound", &srv));
		Check(srv->GetString(&rstrValue));
		srv.Release();

		Check(m_pHost->FindSound(rstrValue, &pMeleeSound));
		RStrRelease(rstrValue); rstrValue = NULL;
	}

	srvData.Release();
	if(SUCCEEDED(srStats->FindNonNullValueW(L"range", &srvData)))
	{
		CProjectileData* pProjectile;

		Check(JSONGetValue(srvData, SLP(L"name"), &srv));
		Check(srv->GetString(&rstrProjectile));
		srv.Release();

		Check(m_mapProjectiles.Find(rstrProjectile, &pProjectile));
		Check(pProjectile->m_pProjectile->FindNonNullValueW(L"sound", &srv));
		Check(srv->GetString(&rstrValue));
		srv.Release();

		Check(m_pHost->FindSound(rstrValue, &pRangeSound));
		RStrRelease(rstrValue); rstrValue = NULL;
	}

	Check(srUnit->FindNonNullValueW(L"animation", &srv));
	Check(srv->GetString(&rstrValue));
	srv.Release();

	if(0 == TStrCmpAssert(RStrToWide(rstrValue), L"ground_unit"))
		pfnBaseAnimation = CMovingObject::StandingAnimation;
	else
		pfnBaseAnimation = CMovingObject::MovingAnimation;

	// If we're importing from a shared location, then use that for loading the SIFs.
	pImport = pUnitData->m_pImport ? pUnitData->m_pImport : pUnitData->m_pUnitPackage;

	if(fEnchanted)
	{
		RSTRING rstrEnchanted;

		Check(RStrFormatW(&rstrEnchanted, L"enchanted_%r", rstrValue));
		RStrRelease(rstrValue);
		rstrValue = rstrEnchanted;

		Check(m_mapCombatAnimations.Find(rstrValue, &pAnimation));
		Check(CreateEnchantedAnimator(pImport, pAnimation, L"combat.sif", &srAnimator));
	}
	else
	{
		Check(m_mapCombatAnimations.Find(rstrValue, &pAnimation));
		Check(CreateAnimator(pImport, pAnimation, L"combat.sif", &srAnimator, TRUE));
	}

	if(0 != crColorize)
	{
		TStackRef<ISimbeyInterchangeAnimator> srNew;
		Check(RecolorizeAnimator(crColorize, srAnimator, &srNew));
		if(srNew)
			srAnimator = srNew;
	}
	Check(AddMovingObject(srUnit, rstrOwner, xTile, yTile, nLayer, srAnimator, nDirection, nLevel, pfnBaseAnimation, pMoveSound, pMeleeSound, pRangeSound, ppObject));

Cleanup:
	RStrRelease(rstrMelee);
	RStrRelease(rstrProjectile);
	RStrRelease(rstrValue);
	return hr;
}

CObject* CCombatScreen::FindObject (INT xTile, INT yTile)
{
	for(sysint i = 0; i < m_aObjects.Length(); i++)
	{
		CObject* pObject = m_aObjects[i];
		if(xTile == pObject->m_xTile && yTile == pObject->m_yTile)
			return pObject;
	}
	return NULL;
}

VOID CCombatScreen::DestroyUnit (CMovingObject* pObject)
{
	pObject->UpdateVisList(m_pMain, m_nUnitLayer, 0, true);
	RemoveUnitFromBoard(pObject);
	UpdateStatsPanel();
}

HRESULT CCombatScreen::ConfigureBackground (ISimbeyInterchangeFile* pLayers, INT idxLayer, INT x, INT y, __deref_out ISimbeyInterchangeSprite** ppSprite)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeFileLayer> srLayer;

	Check(pLayers->GetLayerByIndex(idxLayer, &srLayer));
	Check(sifCreateStaticSprite(srLayer, 0, 0, ppSprite));
	(*ppSprite)->SetPosition(x, y);

Cleanup:
	return hr;
}

HRESULT CCombatScreen::ApplyHealing (CMovingObject* pTarget, INT nHealingPoints)
{
	HRESULT hr;
	QuadooVM::QVARIANT qvArg, qvFigures; qvFigures.eType = QuadooVM::Null;

	Check(UpdatePositionInJSON(pTarget));

	qvArg.lVal = nHealingPoints;
	qvArg.eType = QuadooVM::I4;
	Check(m_pHost->GetVM()->PushValue(&qvArg));

	qvArg.lVal = pTarget->m_nLevel;
	qvArg.eType = QuadooVM::I4;
	Check(m_pHost->GetVM()->PushValue(&qvArg));

	qvArg.pJSONObject = pTarget->m_pDef;
	qvArg.eType = QuadooVM::JSONObject;
	Check(m_pHost->GetVM()->PushValue(&qvArg));

	Check(m_pHost->GetVM()->RunFunction(m_idxApplyHealing, &qvFigures));
	CheckIf(QuadooVM::I4 != qvFigures.eType, DISP_E_BADVARTYPE);
	Check(pTarget->UpdateVisList(m_pMain, m_nUnitLayer, qvFigures.lVal));
	UpdateStatsPanel();

Cleanup:
	return hr;
}

HRESULT CCombatScreen::PlaceObjects (sysint nLayer, PCWSTR pcwzGroup, RSTRING rstrOwner, const POINT* prgptPlace, INT nDirection, COLORREF crColorize)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONObject> srGroup;
	TStackRef<ISimbeyInterchangeAnimator> srAnimator;
	TStackRef<IJSONArray> srArray;
	sysint cPlacements;
	RSTRING rstrType = NULL;
	INT idxPlaceUnit = 0;

	Check(m_pPlacements->FindNonNullValueW(pcwzGroup, &srv));
	Check(srv->GetObject(&srGroup));
	srv.Release();

	Check(srGroup->FindNonNullValueW(L"objects", &srv));
	Check(srv->GetArray(&srArray));
	srv.Release();

	cPlacements = srArray->Count();

	Check(LoadAnimator(SLP(L"graphics\\Buildings.json"), L"graphics\\Buildings.sif", &srAnimator, FALSE));

	for(sysint i = 0; i < cPlacements; i++)
	{
		TStackRef<IJSONObject> srPlacement;

		Check(srArray->GetObject(i, &srPlacement));

		Check(srPlacement->FindNonNullValueW(L"type", &srv));
		Check(srv->GetString(&rstrType));
		srv.Release();

		if(0 == TStrCmpAssert(RStrToWide(rstrType), L"building"))
		{
			INT x, y;
			INT nObject;

			Check(srPlacement->FindNonNullValueW(L"x", &srv));
			Check(srv->GetInteger(&x));
			srv.Release();

			Check(srPlacement->FindNonNullValueW(L"y", &srv));
			Check(srv->GetInteger(&y));
			srv.Release();

			Check(srPlacement->FindNonNullValueW(L"object", &srv));
			Check(srv->GetInteger(&nObject));
			Check(AddStaticObject(x, y, nLayer, srAnimator, nObject));
		}
		else if(0 == TStrCmpAssert(RStrToWide(rstrType), L"unit"))
		{
			INT nLevel;
			bool fEnchanted;

			RStrRelease(rstrType); rstrType = NULL;

			Check(srPlacement->FindNonNullValueW(L"name", &srv));
			Check(srv->GetString(&rstrType));
			srv.Release();

			Check(srPlacement->FindNonNullValueW(L"level", &srv));
			Check(srv->GetInteger(&nLevel));
			srv.Release();

			if(SUCCEEDED(srPlacement->FindNonNullValueW(L"enchanted", &srv)))
				Check(srv->GetBoolean(&fEnchanted));
			else
				fEnchanted = false;

			CheckIf(idxPlaceUnit == 10, E_FAIL);
			Check(PlaceUnit(rstrType, rstrOwner, prgptPlace[idxPlaceUnit].x, prgptPlace[idxPlaceUnit].y, nLayer, nDirection, nLevel, crColorize, fEnchanted));
			idxPlaceUnit++;
		}
		srv.Release();

		RStrRelease(rstrType); rstrType = NULL;
	}

Cleanup:
	RStrRelease(rstrType);
	return hr;
}

HRESULT CCombatScreen::PlaceUnitsAndBuildings (sysint nLayer)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv, srvOpponent;
	TStackRef<IJSONObject> srOpponent;
	RSTRING rstrLeft = NULL, rstrRight = NULL;
	RSTRING rstrPlayer = NULL;
	COLORREF crColorize;
	QuadooVM::QVARIANT qvArg; qvArg.eType = QuadooVM::Null;
	QuadooVM::QVARIANT qvObject; qvObject.eType = QuadooVM::Null;
	INT nResult;

	Check(m_pPlacements->FindNonNullValueW(L"player", &srv));
	Check(srv->GetString(&rstrPlayer));
	Check(RStrCompareIW(rstrPlayer, L"defense", &nResult));
	srv.Release();

	Check(JSONWrapObject(m_pPlacements, &srv));

	if(0 == nResult)
	{
		Check(JSONGetValue(srv, SLP(L"offense:name"), &srvOpponent));
		Check(srvOpponent->GetString(&rstrRight));
		srvOpponent.Release();

		Check(JSONGetValue(srv, SLP(L"offense:def"), &srvOpponent));
		Check(srvOpponent->GetObject(&srOpponent));
	}
	else
	{
		Check(JSONGetValue(srv, SLP(L"defense:name"), &srvOpponent));
		Check(srvOpponent->GetString(&rstrRight));
		srvOpponent.Release();

		Check(JSONGetValue(srv, SLP(L"defense:def"), &srvOpponent));
		Check(srvOpponent->GetObject(&srOpponent));
	}
	srv.Release();

	Check(m_pWizard->FindNonNullValueW(L"name", &srv));
	Check(srv->GetString(&rstrLeft));
	srv.Release();

	qvArg.pJSONObject = srOpponent;
	qvArg.eType = QuadooVM::JSONObject;
	Check(m_pHost->GetVM()->PushValue(&qvArg));

	qvArg.rstrVal = rstrRight;
	qvArg.eType = QuadooVM::String;
	Check(m_pHost->GetVM()->PushValue(&qvArg));

	qvArg.pJSONObject = m_pWizard;
	qvArg.eType = QuadooVM::JSONObject;
	Check(m_pHost->GetVM()->PushValue(&qvArg));

	qvArg.rstrVal = rstrLeft;
	qvArg.eType = QuadooVM::String;
	Check(m_pHost->GetVM()->PushValue(&qvArg));

	Check(m_pHost->GetVM()->RunFunction(m_idxCreateCombatObject, &qvObject));
	CheckIf(QuadooVM::Object != qvObject.eType, E_UNEXPECTED);
	SetInterface(m_pCombat, qvObject.pObject);

	for(sysint i = 0; i < m_mapProjectiles.Length(); i++)
	{
		QuadooVM::QVPARAMS qvParams;
		QuadooVM::QVARIANT qvArgs[2], qv;
		CProjectileData* pData;

		Check(m_mapProjectiles.GetKeyAndValue(i, &qvArgs[1].rstrVal, &pData));
		qvArgs[1].eType = QuadooVM::String;
		qvArgs[0].pJSONObject = pData->m_pProjectile;
		qvArgs[0].eType = QuadooVM::JSONObject;
		qvParams.cArgs = ARRAYSIZE(qvArgs);
		qvParams.pqvArgs = qvArgs;
		qv.eType = QuadooVM::Null;
		Check(m_pCombat->Invoke(NULL, RSTRING_CAST(L"AddRangeAttack"), &qvParams, &qv));
		QVMClearVariant(&qv);
	}

	Check(m_pWizard->FindNonNullValueW(L"flag", &srv));
	Check(srv->GetDWord(&crColorize));

	if(0 == nResult)
	{
		// Player on defense
		Check(PlaceObjects(nLayer, L"defense", rstrLeft, c_rgDefense, 3, crColorize));
		Check(PlaceObjects(nLayer, L"offense", rstrRight, c_rgOffense, 7, 0 /* No colorization for now */));
	}
	else
	{
		// Player on offense
		Check(PlaceObjects(nLayer, L"defense", rstrLeft, c_rgDefense, 3, 0 /* No colorization for now */));
		Check(PlaceObjects(nLayer, L"offense", rstrRight, c_rgOffense, 7, crColorize));
	}

	m_pCombatBar->SetNames(rstrLeft, rstrRight);

Cleanup:
	QVMClearVariant(&qvObject);
	RStrRelease(rstrPlayer);
	RStrRelease(rstrLeft);
	RStrRelease(rstrRight);
	return hr;
}

// IPopupHost

HRESULT CCombatScreen::AddPopup (IPopupView* pView)
{
	HRESULT hr;

	Check(m_aViews.Append(pView));
	Check(m_pSurface->MoveCanvasToTop(m_pMouse));

Cleanup:
	return hr;
}

HRESULT CCombatScreen::RemovePopup (IPopupView* pView)
{
	for(sysint i = 0; i < m_aViews.Length(); i++)
	{
		if(m_aViews[i] == pView)
		{
			pView->Destroy();
			m_aViews.Remove(i, NULL);
			return S_OK;
		}
	}
	return S_FALSE;
}

VOID CCombatScreen::UpdateMouse (LPARAM lParam)
{
	INT xView, yView;
	m_pSurface->TranslateClientPointToCanvas(LOWORD(lParam), HIWORD(lParam), m_pMouse, &xView, &yView);
	m_pMoveType->SetPosition(xView, yView);
}

HRESULT CCombatScreen::AttachSpellCaster (CCastSpell* pSpell)
{
	HRESULT hr;

	CheckIf(NULL != m_pCastSpell, E_UNEXPECTED);
	Check(m_pMoveType->SelectAnimation(3));
	m_pCastSpell = pSpell;

Cleanup:
	return hr;
}

// IAStarCallback2D

BOOL CCombatScreen::GetPathValue (INT x, INT y, INT xFrom, INT yFrom, __out INT* pnValue)
{
	if(FindObject(x, y))
		return FALSE;

	*pnValue = 1;
	return TRUE;
}

HRESULT CCombatScreen::ApplyEnchantingEffect (SIF_SURFACE* pSurface, const COLORREF* pcrgColors, INT cColors, INT nOffset)
{
	HRESULT hr = S_FALSE;
	TArray<PBYTE> aEnchanted;
	PBYTE pbData = pSurface->pbSurface;
	INT xSize = pSurface->xSize;
	INT ySize = pSurface->ySize;
	DWORD cbRow = xSize * sizeof(DWORD);
	const POINT c_rgAround[] =
	{
		{ -1, -1 },
		{ 0, -1 },
		{ 1, -1 },
		{ -1, 0 },
		{ 1, 0 },
		{ -1, 1 },
		{ 0, 1 },
		{ 1, 1 }
	};

	for(INT y = 0; y < ySize; y++)
	{
		for(INT x = 0; x < xSize; x++)
		{
			PBYTE pbPixel = pbData + x * sizeof(DWORD);
			if(0 == pbPixel[3])
			{
				for(INT i = 0; i < ARRAYSIZE(c_rgAround); i++)
				{
					INT xPixel = x + c_rgAround[i].x;
					INT yPixel = y + c_rgAround[i].y;

					if(xPixel >= 0 && yPixel >= 0 && xPixel < xSize && yPixel < ySize)
					{
						PBYTE pbTest = pSurface->pbSurface + yPixel * cbRow + xPixel * sizeof(DWORD);
						if(255 == pbTest[3])
						{
							Check(aEnchanted.Append(pbPixel));
							break;
						}
					}
				}
			}
		}

		pbData += cbRow;
	}

	for(sysint i = 0; i < aEnchanted.Length(); i++)
	{
		PBYTE pbSet = aEnchanted[i];
		COLORREF cr = pcrgColors[(i + nOffset) % cColors];
		pbSet[0] = GetRValue(cr);
		pbSet[1] = GetGValue(cr);
		pbSet[2] = GetBValue(cr);
		pbSet[3] = 255;
	}

Cleanup:
	return hr;
}

HRESULT CCombatScreen::CreateEnchantedAnimator (CSIFPackage* pPackage, IJSONObject* pDef, PCWSTR pcwzSIF, __deref_out ISimbeyInterchangeAnimator** ppAnimator)
{
	HRESULT hr;
	ISimbeyInterchangeFile* pSIF = NULL;
	ISimbeyInterchangeFile* pNew = NULL;
	DWORD cLayers;
	const COLORREF c_rgColors[] =
	{
		RGB(64, 64, 255),
		RGB(64, 64, 192),
		RGB(64, 255, 64),
		RGB(64, 192, 64)
	};

	Check(pPackage->OpenSIF(pcwzSIF, &pSIF));
	cLayers = pSIF->GetLayerCount();

	Check(sifCreateNew(&pNew));

	for(DWORD i = 0; i < cLayers; i++)
	{
		TStackRef<ISimbeyInterchangeFileLayer> srLayer, srNew;
		RECT rcLayer;
		DWORD cbBits;
		SIF_SURFACE Surface;

		Check(pSIF->GetLayerByIndex(i, &srLayer));
		Check(srLayer->GetPosition(&rcLayer));

		Surface.cBitsPerPixel = 32;
		Surface.xSize = (rcLayer.right - rcLayer.left) + 2;
		Surface.ySize = (rcLayer.bottom - rcLayer.top) + 2;
		Surface.lPitch = Surface.xSize * 4;

		Check(pNew->AddLayer(Surface.xSize, Surface.ySize, &srNew, NULL));
		srNew->SetPosition(rcLayer.left, rcLayer.top);
		Check(srNew->GetBitsPtr(&Surface.pbSurface, &cbBits));
		Check(srLayer->CopyToBits32(&Surface, 1, 1));
		Check(ApplyEnchantingEffect(&Surface, c_rgColors, ARRAYSIZE(c_rgColors), 0));
		srNew.Release();

		Check(pNew->AddLayer(Surface.xSize, Surface.ySize, &srNew, NULL));
		srNew->SetPosition(rcLayer.left, rcLayer.top);
		Check(srNew->GetBitsPtr(&Surface.pbSurface, &cbBits));
		Check(srLayer->CopyToBits32(&Surface, 1, 1));
		Check(ApplyEnchantingEffect(&Surface, c_rgColors, ARRAYSIZE(c_rgColors), 1));
	}

	Check(CreateAnimator(pNew, pDef, ppAnimator, TRUE));

Cleanup:
	if(pNew)
	{
		pNew->Close();
		pNew->Release();
	}
	if(pSIF)
	{
		pSIF->Close();
		pSIF->Release();
	}
	return hr;
}

HRESULT CCombatScreen::RecolorizeAnimator (COLORREF crColorize, ISimbeyInterchangeAnimator* pAnimator, __deref_out ISimbeyInterchangeAnimator** ppNew)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeAnimator> srDup;
	INT cImages = pAnimator->GetImageCount();

	Check(pAnimator->Duplicate(&srDup));

	for(INT i = 0; i < cImages; i++)
	{
		PBYTE pBits32P;
		INT nWidth, nHeight;

		Check(srDup->GetImage(i, &pBits32P, &nWidth, &nHeight));
		for(INT y = 0; y < nHeight; y++)
		{
			for(INT x = 0; x < nWidth; x++)
			{
				// If both the red and blue channels are zero,
				// then this is a pixel we want to recolorize.
				if(pBits32P[0] == 0 && pBits32P[2] == 0)
				{
					BYTE fAmount = pBits32P[1];
					pBits32P[0] = MulDiv(GetRValue(crColorize), fAmount, 255);
					pBits32P[1] = MulDiv(GetGValue(crColorize), fAmount, 255);
					pBits32P[2] = MulDiv(GetBValue(crColorize), fAmount, 255);
				}

				pBits32P += sizeof(DWORD);
			}
		}
	}

	*ppNew = srDup.Detach();

Cleanup:
	return hr;
}

COLORREF CCombatScreen::HSLToRGB (DOUBLE h, DOUBLE sl, DOUBLE l)
{
	DOUBLE v;
	DOUBLE r,g,b;

	r = l;   // default to gray
	g = l;
	b = l;
	v = (l <= 0.5) ? (l * (1.0 + sl)) : (l + sl - l * sl);
	if(v > 0.0)
	{
		DOUBLE m;
		DOUBLE sv;
		INT sextant;
		DOUBLE fract, vsf, mid1, mid2;

		m = l + l - v;
		sv = (v - m ) / v;
		h *= 6.0;
		sextant = static_cast<INT>(h);
		fract = h - sextant;
		vsf = v * sv * fract;
		mid1 = m + vsf;
		mid2 = v - vsf;

		switch(sextant)
		{
		case 0:
			r = v;
			g = mid1;
			b = m;
			break;
		case 1:
			r = mid2;
			g = v;
			b = m;
			break;
		case 2:
			r = m;
			g = v;
			b = mid1;
			break;
		case 3:
			r = m;
			g = mid2;
			b = v;
			break;
		case 4:
			r = mid1;
			g = m;
			b = v;
			break;
		case 5:
			r = v;
			g = m;
			b = mid2;
			break;
		}
	}

	return RGB(static_cast<BYTE>(r * 255.0),
		static_cast<BYTE>(g * 255.0),
		static_cast<BYTE>(b * 255.0));
}
