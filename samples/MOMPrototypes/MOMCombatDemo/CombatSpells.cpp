#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\MemoryStream.h"
#include "Library\Util\Formatting.h"
#include "Library\Util\StreamHelpers.h"
#include "Published\JSON.h"
#include "CombatSpells.h"

///////////////////////////////////////////////////////////////////////////////
// CSummonSpell
///////////////////////////////////////////////////////////////////////////////

CSummonSpell::CSummonSpell (RSTRING rstrCaster, RSTRING rstrName, CCombatScreen* pScreen, CSIFCanvas* pCanvas, sysint nLayer, CIsometricTranslator* pIsometric, INT xTile, INT yTile) :
	CAction(pScreen),
	m_pCanvas(pCanvas),
	m_nLayer(nLayer),
	m_pIsometric(pIsometric),
	m_xTile(xTile),
	m_yTile(yTile),
	m_eState(Idle),
	m_pObject(NULL),
	m_pOriginalAnimator(NULL)
{
	RStrSet(m_rstrCaster, rstrCaster);
	RStrSet(m_rstrName, rstrName);
}

CSummonSpell::~CSummonSpell ()
{
	SafeRelease(m_pOriginalAnimator);

	RStrRelease(m_rstrName);
	RStrRelease(m_rstrCaster);
}

VOID CSummonSpell::Update (VOID)
{
	switch(m_eState)
	{
	case Idle:
		m_cTicks = 4;
		m_cFrame = 24;
		m_eState = Spawn;
		m_cLocks = 2;
		Start();
		break;
	case Spawn:
		if(0 == --m_cTicks)
		{
			m_cTicks = 4;

			if(0 == --m_cFrame)
			{
				m_eState = Up;
				m_cFrame = 16;

				// Show the units, then sort while the units are in the final position.
				m_pObject->ShowOrHide(m_pCanvas, m_nLayer, TRUE);
				m_pIsometric->SortIsometricLayer(m_pCanvas, m_nLayer);

				// After sorting, shift the units to the "spawning" location.
				m_pObject->ShiftObject(0, 16);

				// Update the units' heights.
				UpdateSpriteSize();
			}
		}
		break;
	case Up:
		if(0 == --m_cTicks)
		{
			m_cTicks = 5;
			m_pObject->ShiftObject(0, -1);

			if(0 == --m_cFrame)
			{
				m_eState = Done;
				m_pObject->ReplaceAnimator(m_pOriginalAnimator, m_pCanvas, m_nLayer);

				if(0 == --m_cLocks)
					m_pScreen->ClearAction(this);
			}
			else
				UpdateSpriteSize();
		}
		break;
	}
}

// ISpriteAnimationCompleted

VOID CSummonSpell::OnSpriteAnimationCompleted (ISimbeyInterchangeSprite* pSprite, INT nAnimation)
{
	m_pCanvas->RemoveSpriteLater(m_nLayer, pSprite);
	if(0 == --m_cLocks)
		m_pScreen->ClearAction(this);
}

HRESULT CSummonSpell::Start (VOID)
{
	HRESULT hr;
	FMOD::Sound* pSummoning;
	TStackRef<ISimbeyInterchangeFile> srSIF;
	TStackRef<ISimbeyInterchangeAnimator> srAnimator;
	TStackRef<ISimbeyInterchangeSprite> srSprite, srFirstUnit;
	INT xIso, yIso, xView, yView;

	Check(m_pScreen->FindSound(RSTRING_CAST(L"Summoning.mp3"), &pSummoning));
	m_pScreen->PlaySound(pSummoning);

	Check(m_pScreen->GetPackage()->OpenSIF(L"spells\\Summoning\\cast.sif", &srSIF));
	Check(CBaseScreen::CreateDefaultAnimator(srSIF, TRUE, 12, FALSE, &srAnimator, NULL));
	Check(srAnimator->CreateSprite(&srSprite));
	Check(srSprite->SelectAnimation(0));
	Check(srSprite->SetAnimationCompletedCallback(this));

	m_pIsometric->TileToView(m_xTile, m_yTile, &xIso, &yIso);
	m_pIsometric->IsometricToView(m_pCanvas, xIso, yIso, &xView, &yView);
	srSprite->SetPosition(xView, yView - 17);

	Check(m_pScreen->PlaceUnit(m_rstrName, m_rstrCaster, m_xTile, m_yTile, m_nLayer, 0, 0, 0, false, &m_pObject));
	Check(m_pIsometric->SortIsometricLayer(m_pCanvas, m_nLayer));

	Check(m_pObject->GetFirstVisibleSprite(&srFirstUnit));
	Check(srFirstUnit->GetAnimator(&m_pOriginalAnimator));
	Check(m_pCanvas->AddSpriteBefore(m_nLayer, srSprite, srFirstUnit, NULL));

	// Hide the units until the summoning portal is fully formed.
	m_pObject->ShowOrHide(m_pCanvas, m_nLayer, FALSE);

Cleanup:
	if(srSIF)
		srSIF->Close();
	return hr;
}

HRESULT CSummonSpell::UpdateSpriteSize (VOID)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeAnimator> srDup;
	INT cImages, cMergeShift = m_cFrame;

	Check(m_pOriginalAnimator->Duplicate(&srDup));
	cImages = srDup->GetImageCount();
	for(INT i = 0; i < cImages; i++)
	{
		PBYTE pBits;
		INT nWidth, nHeight, xOffset, yOffset;

		Check(srDup->GetImage(i, &pBits, &nWidth, &nHeight));
		Check(srDup->GetImageOffset(i, &xOffset, &yOffset));
		Check(srDup->SetImage(i, FALSE, pBits, nWidth, max(nHeight - cMergeShift, 0), xOffset, yOffset));
	}

	m_pObject->ReplaceAnimator(srDup, m_pCanvas, m_nLayer);

Cleanup:
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
// CBaseIrreversibleDamageSpell
///////////////////////////////////////////////////////////////////////////////

CBaseIrreversibleDamageSpell::CBaseIrreversibleDamageSpell (RSTRING rstrCaster, CCombatScreen* pScreen, CSIFCanvas* pCanvas, sysint nLayer, CIsometricTranslator* pIsometric, INT xTile, INT yTile) :
	CAction(pScreen),
	m_pCanvas(pCanvas),
	m_nLayer(nLayer),
	m_pIsometric(pIsometric),
	m_xTile(xTile),
	m_yTile(yTile),
	m_fIdle(TRUE),
	m_pTarget(NULL)
{
}

CBaseIrreversibleDamageSpell::~CBaseIrreversibleDamageSpell ()
{
}

VOID CBaseIrreversibleDamageSpell::Update (VOID)
{
	if(m_fIdle)
	{
		m_fIdle = FALSE;
		Start();
	}
}

// ISpriteAnimationCompleted

VOID CBaseIrreversibleDamageSpell::OnSpriteAnimationCompleted (ISimbeyInterchangeSprite* pSprite, INT nAnimation)
{
	if(CheckDestroyUnit(m_pTarget))
		m_pScreen->DestroyUnit(m_pTarget);
	m_pCanvas->RemoveSpriteLater(m_nLayer, pSprite);
	m_pScreen->ClearAction(this);
}

HRESULT CBaseIrreversibleDamageSpell::Start (VOID)
{
	HRESULT hr;
	RSTRING rstrAnimation = NULL, rstrSound = NULL;
	FMOD::Sound* pSound;
	TStackRef<ISimbeyInterchangeFile> srSIF;
	TStackRef<ISimbeyInterchangeAnimator> srAnimator;
	TStackRef<ISimbeyInterchangeSprite> srSprite, srSortUnit;
	INT xIso, yIso, xView, yView;
	CObject* pObject;
	bool fUnderObject;

	Check(GetAnimationAndSound(&rstrAnimation, &rstrSound, &fUnderObject));

	Check(m_pScreen->FindSound(rstrSound, &pSound));
	m_pScreen->PlaySound(pSound);

	Check(m_pScreen->GetPackage()->OpenSIF(RStrToWide(rstrAnimation), &srSIF));
	Check(CBaseScreen::CreateDefaultAnimator(srSIF, TRUE, 12, FALSE, &srAnimator, NULL));
	Check(srAnimator->CreateSprite(&srSprite));
	Check(srSprite->SelectAnimation(0));
	Check(srSprite->SetAnimationCompletedCallback(this));

	m_pIsometric->TileToView(m_xTile, m_yTile, &xIso, &yIso);
	m_pIsometric->IsometricToView(m_pCanvas, xIso, yIso, &xView, &yView);
	srSprite->SetPosition(xView, yView - 17);

	pObject = m_pScreen->FindObject(m_xTile, m_yTile);
	CheckIf(!pObject->CanBeMoved(), E_FAIL);
	m_pTarget = static_cast<CMovingObject*>(pObject);

	Check(m_pIsometric->SortIsometricLayer(m_pCanvas, m_nLayer));

	if(fUnderObject)
	{
		Check(m_pTarget->GetFirstVisibleSprite(&srSortUnit));
		Check(m_pCanvas->AddSpriteBefore(m_nLayer, srSprite, srSortUnit, NULL));
	}
	else
	{
		Check(m_pTarget->GetLastVisibleSprite(&srSortUnit));
		Check(m_pCanvas->AddSpriteAfter(m_nLayer, srSprite, srSortUnit, NULL));
	}

Cleanup:
	if(srSIF)
		srSIF->Close();
	RStrRelease(rstrAnimation);
	RStrRelease(rstrSound);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
// CCracksCall
///////////////////////////////////////////////////////////////////////////////

CCracksCall::CCracksCall (RSTRING rstrCaster, CCombatScreen* pScreen, CSIFCanvas* pCanvas, sysint nLayer, CIsometricTranslator* pIsometric, INT xTile, INT yTile) :
	CBaseIrreversibleDamageSpell(rstrCaster, pScreen, pCanvas, nLayer, pIsometric, xTile, yTile)
{
}

CCracksCall::~CCracksCall ()
{
}

HRESULT CCracksCall::GetAnimationAndSound (__out RSTRING* prstrAnimation, __out RSTRING* prstrSound, __out bool* pfUnderObject)
{
	HRESULT hr;

	Check(RStrCreateW(LSP(L"spells\\CracksCall\\cast.sif"), prstrAnimation));
	Check(RStrCreateW(LSP(L"CracksCall.mp3"), prstrSound));
	*pfUnderObject = true;

Cleanup:
	return hr;
}

BOOL CCracksCall::CheckDestroyUnit (CMovingObject* pTarget)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONObject> srAir;

	Check(JSONGetValueFromObject(pTarget->m_pDef, SLP(L"base:stats:move"), &srv));
	CheckIf(SUCCEEDED(JSONFindArrayObjectIndirect(srv, RSTRING_CAST(L"stat"), RSTRING_CAST(L"air"), &srAir, NULL)), S_FALSE);

Cleanup:
	return S_OK == hr && rand() % 100 < 25;
}

///////////////////////////////////////////////////////////////////////////////
// CDisintegrate
///////////////////////////////////////////////////////////////////////////////

CDisintegrate::CDisintegrate (RSTRING rstrCaster, CCombatScreen* pScreen, CSIFCanvas* pCanvas, sysint nLayer, CIsometricTranslator* pIsometric, INT xTile, INT yTile) :
	CBaseIrreversibleDamageSpell(rstrCaster, pScreen, pCanvas, nLayer, pIsometric, xTile, yTile)
{
}

CDisintegrate::~CDisintegrate ()
{
}

HRESULT CDisintegrate::GetAnimationAndSound (__out RSTRING* prstrAnimation, __out RSTRING* prstrSound, __out bool* pfUnderObject)
{
	HRESULT hr;

	Check(RStrCreateW(LSP(L"spells\\Disintegrate\\cast.sif"), prstrAnimation));
	Check(RStrCreateW(LSP(L"Disintegrate.mp3"), prstrSound));
	*pfUnderObject = false;

Cleanup:
	return hr;
}

BOOL CDisintegrate::CheckDestroyUnit (CMovingObject* pTarget)
{
	HRESULT hr;
	TStackRef<IJSONObject> srStats;
	TStackRef<IJSONValue> srv;
	INT nResistance;

	Check(m_pScreen->ExpandStats(pTarget->m_pDef, pTarget->m_nLevel, &srStats));
	Check(srStats->FindNonNullValueW(L"resist", &srv));
	Check(srv->GetInteger(&nResistance));

Cleanup:
	return SUCCEEDED(hr) && nResistance < 10;
}

///////////////////////////////////////////////////////////////////////////////
// CCastSpell
///////////////////////////////////////////////////////////////////////////////

CCastSpell::CCastSpell (RSTRING rstrCaster, Target eTarget) :
	m_eTarget(eTarget)
{
	RStrSet(m_rstrCaster, rstrCaster);
}

CCastSpell::~CCastSpell ()
{
	RStrRelease(m_rstrCaster);
}

HRESULT CCastSpell::Query (INT xTile, INT yTile, __in_opt CMovingObject* pUnit)
{
	HRESULT hr;

	CheckIfIgnore((m_eTarget & EmptyTile) && NULL == pUnit, S_OK);

	if(pUnit)
	{
		INT nResult;

		Check(RStrCompareRStr(m_rstrCaster, pUnit->m_rstrOwner, &nResult));
		CheckIfIgnore(0 == nResult && (m_eTarget & FriendlyUnits), S_OK);
		CheckIfIgnore(0 != nResult && (m_eTarget & EnemyUnits), S_OK);
	}

	hr = S_FALSE;

Cleanup:
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
// CCastSummonSpell
///////////////////////////////////////////////////////////////////////////////

CCastSummonSpell::CCastSummonSpell (RSTRING rstrCaster, RSTRING rstrUnit) :
	CCastSpell(rstrCaster, CCastSpell::EmptyTile)
{
	RStrSet(m_rstrUnit, rstrUnit);
}

CCastSummonSpell::~CCastSummonSpell ()
{
	RStrRelease(m_rstrUnit);
}

// CCastSpell

HRESULT CCastSummonSpell::Cast (CCombatScreen* pScreen, CSIFCanvas* pCanvas, sysint nLayer, CIsometricTranslator* pIsometric, INT xTile, INT yTile, __in_opt CMovingObject* pUnit, __deref_out CAction** ppAction)
{
	*ppAction = __new CSummonSpell(m_rstrCaster, m_rstrUnit, pScreen, pCanvas, nLayer, pIsometric, xTile, yTile);
	return *ppAction ? S_OK : E_OUTOFMEMORY;
}

///////////////////////////////////////////////////////////////////////////////
// CCastTargetSpell
///////////////////////////////////////////////////////////////////////////////

CCastTargetSpell::CCastTargetSpell (RSTRING rstrCaster, RSTRING rstrSpell) :
	CCastSpell(rstrCaster, EnemyUnits)
{
	RStrSet(m_rstrSpell, rstrSpell);
}

CCastTargetSpell::~CCastTargetSpell ()
{
	RStrRelease(m_rstrSpell);
}

// CCastSpell

HRESULT CCastTargetSpell::Cast (CCombatScreen* pScreen, CSIFCanvas* pCanvas, sysint nLayer, CIsometricTranslator* pIsometric, INT xTile, INT yTile, __in_opt CMovingObject* pUnit, __deref_out CAction** ppAction)
{
	INT nResult;

	if(SUCCEEDED(RStrCompareW(m_rstrSpell, L"Cracks Call", &nResult)) && 0 == nResult)
		*ppAction = __new CCracksCall(m_rstrCaster, pScreen, pCanvas, nLayer, pIsometric, xTile, yTile);
	else if(SUCCEEDED(RStrCompareW(m_rstrSpell, L"Disintegrate", &nResult)) && 0 == nResult)
		*ppAction = __new CDisintegrate(m_rstrCaster, pScreen, pCanvas, nLayer, pIsometric, xTile, yTile);
	else
		*ppAction = NULL;

	return *ppAction ? S_OK : E_OUTOFMEMORY;
}
