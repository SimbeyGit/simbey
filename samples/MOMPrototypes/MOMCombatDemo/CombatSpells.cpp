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
	Check(CBaseScreen::CreateDefaultAnimator(srSIF, TRUE, 12, FALSE, TRUE /* premultiply */, &srAnimator, NULL));
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
	Check(CBaseScreen::CreateDefaultAnimator(srSIF, TRUE, 12, FALSE, TRUE /* premultiply */, &srAnimator, NULL));
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
// CHealingSpell
///////////////////////////////////////////////////////////////////////////////

CHealingSpell::CHealingSpell (RSTRING rstrCaster, CCombatScreen* pScreen, CSIFCanvas* pCanvas, sysint nLayer, CIsometricTranslator* pIsometric, INT xTile, INT yTile) :
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

CHealingSpell::~CHealingSpell ()
{
}

VOID CHealingSpell::Update (VOID)
{
	if(m_fIdle)
	{
		m_fIdle = FALSE;
		Start();
	}
}

// ISpriteAnimationCompleted

VOID CHealingSpell::OnSpriteAnimationCompleted (ISimbeyInterchangeSprite* pSprite, INT nAnimation)
{
	m_pScreen->ApplyHealing(m_pTarget, 5);
	m_pCanvas->RemoveSpriteLater(m_nLayer, pSprite);
	m_pScreen->ClearAction(this);
}

HRESULT CHealingSpell::Start (VOID)
{
	HRESULT hr;
	FMOD::Sound* pSound;
	TStackRef<ISimbeyInterchangeFile> srSIF;
	TStackRef<ISimbeyInterchangeAnimator> srAnimator;
	TStackRef<ISimbeyInterchangeSprite> srSprite, srSortUnit;
	INT xIso, yIso, xView, yView;
	CObject* pObject;

	Check(m_pScreen->FindSound(RSTRING_CAST(L"HeavenlyLight.mp3"), &pSound));
	m_pScreen->PlaySound(pSound);

	Check(m_pScreen->GetPackage()->OpenSIF(L"spells\\Healing\\cast.sif", &srSIF));
	Check(CBaseScreen::CreateDefaultAnimator(srSIF, TRUE, 10, FALSE, TRUE /* premultiply */, &srAnimator, NULL));
	Check(srAnimator->CreateSprite(&srSprite));
	Check(srSprite->SelectAnimation(0));
	Check(srSprite->SetAnimationCompletedCallback(this));

	m_pIsometric->TileToView(m_xTile, m_yTile, &xIso, &yIso);
	m_pIsometric->IsometricToView(m_pCanvas, xIso, yIso, &xView, &yView);
	srSprite->SetPosition(xView - 15, yView - 24);

	pObject = m_pScreen->FindObject(m_xTile, m_yTile);
	CheckIf(!pObject->CanBeMoved(), E_FAIL);
	m_pTarget = static_cast<CMovingObject*>(pObject);

	Check(m_pIsometric->SortIsometricLayer(m_pCanvas, m_nLayer));

	Check(m_pTarget->GetLastVisibleSprite(&srSortUnit));
	Check(m_pCanvas->AddSpriteAfter(m_nLayer, srSprite, srSortUnit, NULL));

Cleanup:
	if(srSIF)
		srSIF->Close();
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
// CSpellAttack
///////////////////////////////////////////////////////////////////////////////

CSpellAttack::CSpellAttack (RSTRING rstrCaster, CCombatScreen* pScreen, CSIFCanvas* pCanvas, sysint nLayer, CIsometricTranslator* pIsometric, INT xTile, INT yTile, RSTRING rstrSpell, INT nAdditionalPower) :
	CAction(pScreen),
	m_pCanvas(pCanvas),
	m_nLayer(nLayer),
	m_pIsometric(pIsometric),
	m_xTile(xTile),
	m_yTile(yTile),
	m_pTarget(NULL),
	m_nAdditionalPower(nAdditionalPower),
	m_nFrame(-1),
	m_pSlide(NULL),
	m_pWeapon(NULL),
	m_pSprite(NULL)
{
	RStrSet(m_rstrSpell, rstrSpell);
	RStrSet(m_rstrCaster, rstrCaster);
}

CSpellAttack::~CSpellAttack ()
{
	SafeRelease(m_pSprite);

	SafeRelease(m_pWeapon);
	SafeRelease(m_pSlide);

	RStrRelease(m_rstrCaster);
	RStrRelease(m_rstrSpell);
}

VOID CSpellAttack::Update (VOID)
{
	m_nFrame++;

	if(0 == m_nFrame)
		Start();
	else if(m_nFrame <= m_cSteps)
	{
		TStackRef<IJSONValue> srv;
		INT x, y, xStep = 0, yStep = 0;

		if(SUCCEEDED(m_pSlide->FindNonNullValueW(L"x_step", &srv)))
		{
			srv->GetInteger(&xStep);
			srv.Release();
		}
		if(SUCCEEDED(m_pSlide->FindNonNullValueW(L"y_step", &srv)))
			srv->GetInteger(&yStep);

		m_pSprite->GetPosition(x, y);
		x += xStep;
		y += yStep;
		m_pSprite->SetPosition(x, y);

		if(m_nFrame == m_cSteps)
		{
			TStackRef<IJSONObject> srAbilities;

			JSONCreateObject(&srAbilities);

			m_pSprite->SelectAnimation(1);
			m_pScreen->PerformSpellAttack(srAbilities, m_pWeapon, m_pTarget);
		}
	}
}

// ISpriteAnimationCompleted

VOID CSpellAttack::OnSpriteAnimationCompleted (ISimbeyInterchangeSprite* pSprite, INT nAnimation)
{
	Assert(m_pSprite == pSprite);
	SafeRelease(m_pSprite);

	m_pCanvas->RemoveSpriteLater(m_nLayer, pSprite);
	m_pScreen->ClearAction(this);
}

HRESULT CSpellAttack::Start (VOID)
{
	HRESULT hr;
	TStackRef<CSIFPackage> srSpellDir;
	TStackRef<IJSONObject> srData, srDef;
	TStackRef<IJSONValue> srv;
	TStackRef<ISimbeyInterchangeFile> srSIF;
	TStackRef<ISimbeyInterchangeAnimator> srAnimator;
	TStackRef<ISimbeyInterchangeSprite> srSprite, srSortUnit;
	WCHAR wzSpellDir[MAX_PATH];
	INT cchSpellDir;
	RSTRING rstrSound = NULL;
	FMOD::Sound* pSound;
	INT xIso, yIso, xView, yView;
	INT xStart, yStart;
	CObject* pObject;

	Check(Formatting::TPrintF(wzSpellDir, ARRAYSIZE(wzSpellDir), &cchSpellDir, L"spells\\%r", m_rstrSpell));
	Check(m_pScreen->GetPackage()->OpenDirectory(wzSpellDir, cchSpellDir, &srSpellDir));
	Check(srSpellDir->OpenSIF(L"cast.sif", &srSIF));

	Check(srSpellDir->GetJSONData(SLP(L"data.json"), &srv));
	Check(srv->GetObject(&srData));
	srv.Release();

	Check(srData->FindNonNullValueW(L"slide", &srv));
	Check(srv->GetObject(&m_pSlide));
	srv.Release();

	Check(srData->FindNonNullValueW(L"weapon", &srv));
	Check(srv->GetObject(&m_pWeapon));
	srv.Release();

	if(SUCCEEDED(srData->FindNonNullValueW(L"adjustable_weapon_value", &srv)))
	{
		bool fAdjustable;

		if(SUCCEEDED(srv->GetBoolean(&fAdjustable)) && fAdjustable)
		{
			INT nValue;

			srv.Release();

			Check(m_pWeapon->FindNonNullValueW(L"value", &srv));
			Check(srv->GetInteger(&nValue));
			srv.Release();

			nValue += m_nAdditionalPower;
			Check(JSONCreateInteger(nValue, &srv));
			Check(m_pWeapon->AddValueW(L"value", srv));
		}

		srv.Release();
	}

	Check(srSpellDir->GetJSONData(SLP(L"projectile.json"), &srv));
	Check(srv->GetObject(&srDef));
	srv.Release();

	Check(CBaseScreen::CreateAnimator(srSIF, srDef, &srAnimator, TRUE));
	Check(srData->FindNonNullValueW(L"sound", &srv));
	Check(srv->GetString(&rstrSound));

	Check(m_pScreen->FindSound(rstrSound, &pSound));
	m_pScreen->PlaySound(pSound);

	Check(srAnimator->CreateSprite(&srSprite));
	Check(srSprite->SelectAnimation(0));
	Check(srSprite->SetAnimationCompletedCallback(this));

	m_pIsometric->TileToView(m_xTile, m_yTile, &xIso, &yIso);
	m_pIsometric->IsometricToView(m_pCanvas, xIso, yIso, &xView, &yView);

	srv.Release();
	Check(m_pSlide->FindNonNullValueW(L"x_start", &srv));
	Check(srv->GetInteger(&xStart));

	srv.Release();
	Check(m_pSlide->FindNonNullValueW(L"y_start", &srv));
	Check(srv->GetInteger(&yStart));

	srv.Release();
	Check(m_pSlide->FindNonNullValueW(L"steps", &srv));
	Check(srv->GetInteger(&m_cSteps));

	srSprite->SetPosition(xView + xStart, yView + yStart);

	pObject = m_pScreen->FindObject(m_xTile, m_yTile);
	CheckIf(!pObject->CanBeMoved(), E_FAIL);
	m_pTarget = static_cast<CMovingObject*>(pObject);

	Check(m_pIsometric->SortIsometricLayer(m_pCanvas, m_nLayer));

	Check(m_pTarget->GetLastVisibleSprite(&srSortUnit));
	Check(m_pCanvas->AddSpriteAfter(m_nLayer, srSprite, srSortUnit, NULL));

	m_pSprite = srSprite.Detach();

Cleanup:
	if(srSIF)
		srSIF->Close();
	RStrRelease(rstrSound);
	return hr;
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
	else if(SUCCEEDED(RStrCompareW(m_rstrSpell, L"Ice Bolt", &nResult)) && 0 == nResult)
		*ppAction = __new CSpellAttack(m_rstrCaster, pScreen, pCanvas, nLayer, pIsometric, xTile, yTile, RSTRING_CAST(L"IceBolt"), 40);
	else if(SUCCEEDED(RStrCompareW(m_rstrSpell, L"Fire Bolt", &nResult)) && 0 == nResult)
		*ppAction = __new CSpellAttack(m_rstrCaster, pScreen, pCanvas, nLayer, pIsometric, xTile, yTile, RSTRING_CAST(L"FireBolt"), 20);
	else if(SUCCEEDED(RStrCompareW(m_rstrSpell, L"Doom Bolt", &nResult)) && 0 == nResult)
		*ppAction = __new CSpellAttack(m_rstrCaster, pScreen, pCanvas, nLayer, pIsometric, xTile, yTile, RSTRING_CAST(L"DoomBolt"), 0);
	else
		*ppAction = NULL;

	return *ppAction ? S_OK : E_OUTOFMEMORY;
}

///////////////////////////////////////////////////////////////////////////////
// CCastFriendlySpell
///////////////////////////////////////////////////////////////////////////////

CCastFriendlySpell::CCastFriendlySpell (RSTRING rstrCaster, RSTRING rstrSpell) :
	CCastSpell(rstrCaster, CCastSpell::FriendlyUnits)
{
	RStrSet(m_rstrSpell, rstrSpell);
}

CCastFriendlySpell::~CCastFriendlySpell ()
{
	RStrRelease(m_rstrSpell);
}

// CCastSpell

HRESULT CCastFriendlySpell::Cast (CCombatScreen* pScreen, CSIFCanvas* pCanvas, sysint nLayer, CIsometricTranslator* pIsometric, INT xTile, INT yTile, __in_opt CMovingObject* pUnit, __deref_out CAction** ppAction)
{
	INT nResult;

	if(SUCCEEDED(RStrCompareW(m_rstrSpell, L"Healing", &nResult)) && 0 == nResult)
		*ppAction = __new CHealingSpell(m_rstrCaster, pScreen, pCanvas, nLayer, pIsometric, xTile, yTile);
	else
		*ppAction = NULL;

	return *ppAction ? S_OK : E_OUTOFMEMORY;
}
