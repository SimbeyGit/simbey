#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\MemoryStream.h"
#include "Library\Util\Formatting.h"
#include "Library\Util\FileStream.h"
#include "Published\JSON.h"
#include "Dir.h"
#include "TileRules.h"
#include "TileSet.h"

///////////////////////////////////////////////////////////////////////////////
// CTile
///////////////////////////////////////////////////////////////////////////////

CTile::CTile (CTileSet* pTileSet, RSTRING rstrKey, ISimbeyInterchangeSprite* pSprite) :
	m_pTileSet(pTileSet),
	m_pSprite(pSprite),
	m_fIsSprite(true)
{
	RStrSet(m_rstrKey, rstrKey);
	m_pSprite->AddRef();
}

CTile::CTile (CTileSet* pTileSet, RSTRING rstrKey, ISimbeyInterchangeAnimator* pAnimator) :
	m_pTileSet(pTileSet),
	m_pAnimator(pAnimator),
	m_fIsSprite(false)
{
	RStrSet(m_rstrKey, rstrKey);
	m_pAnimator->AddRef();
}

CTile::~CTile ()
{
	if(m_fIsSprite)
		m_pSprite->Release();
	else
		m_pAnimator->Release();
	RStrRelease(m_rstrKey);
}

HRESULT CTile::CreateSprite (__deref_out ISimbeyInterchangeSprite** ppSprite)
{
	HRESULT hr;

	if(m_fIsSprite)
		Check(m_pSprite->Clone(ppSprite));
	else
	{
		TStackRef<ISimbeyInterchangeSprite> srSprite;

		Check(m_pAnimator->CreateSprite(&srSprite));
		Check(srSprite->SelectAnimation(0, rand() % m_pAnimator->GetImageCount()));
		*ppSprite = srSprite.Detach();
	}

Cleanup:
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
// CTileSet
///////////////////////////////////////////////////////////////////////////////

CTileSet::CTileSet (RSTRING rstrName)
{
	RStrSet(m_rstrName, rstrName);
}

CTileSet::~CTileSet ()
{
	for(sysint i = 0; i < m_mapTiles.Length(); i++)
	{
		TArray<CTile*>* pTiles = *(m_mapTiles.GetValuePtr(i));
		pTiles->DeleteAll();
		__delete pTiles;
	}
	RStrRelease(m_rstrName);
}

BOOL CTileSet::IsTileSet (PCWSTR pcwzTileSet)
{
	INT nResult;
	return SUCCEEDED(RStrCompareW(m_rstrName, pcwzTileSet, &nResult)) && 0 == nResult;
}

HRESULT CTileSet::AddVariant (RSTRING rstrKey, ISimbeyInterchangeSprite* pSprite)
{
	HRESULT hr;
	TArray<CTile*>* pTiles;
	CTile* pTile = NULL;

	Check(EnsureTilesArray(rstrKey, &pTiles));

	pTile = __new CTile(this, rstrKey, pSprite);
	CheckAlloc(pTile);
	Check(pTiles->Append(pTile));

Cleanup:
	return hr;
}

HRESULT CTileSet::AddVariant (RSTRING rstrKey, ISimbeyInterchangeAnimator* pAnimator)
{
	HRESULT hr;
	TArray<CTile*>* pTiles;
	CTile* pTile = NULL;

	Check(EnsureTilesArray(rstrKey, &pTiles));

	pTile = __new CTile(this, rstrKey, pAnimator);
	CheckAlloc(pTile);
	Check(pTiles->Append(pTile));

Cleanup:
	return hr;
}

HRESULT CTileSet::FindFromKey (RSTRING rstrKey, __deref_out TArray<CTile*>** ppTiles)
{
	return m_mapTiles.Find(rstrKey, ppTiles);
}

HRESULT CTileSet::EnsureTilesArray (RSTRING rstrKey, __deref_out TArray<CTile*>** ppTiles)
{
	HRESULT hr;
	TArray<CTile*>** ppTilesPtr = m_mapTiles[rstrKey];

	CheckAlloc(ppTilesPtr);
	if(NULL == *ppTilesPtr)
	{
		*ppTilesPtr = __new TArray<CTile*>;
		CheckAlloc(*ppTilesPtr);
	}

	*ppTiles = *ppTilesPtr;
	hr = S_OK;

Cleanup:
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
// CPlaceItem
///////////////////////////////////////////////////////////////////////////////

HRESULT CPlaceItem::CreatePlaceItem (CTileRules* pTileRules, MAPTILE* pWorld, INT xWorld, INT yWorld, INT x, INT y, __deref_out CPlaceItem** ppItem)
{
	INT xTile = x;
	CTile* pOther = NULL;

	// Wrap the sides of the world.
	if(x < 0)
		x += xWorld;
	else if(x >= xWorld)
		x -= xWorld;

	if(y >= 0 && y < yWorld)
		pOther = pWorld[y * xWorld + x].pTile;

	*ppItem = __new CPlaceItem(pTileRules, pOther, xTile, x, y);
	return *ppItem ? S_OK : E_OUTOFMEMORY;
}

CPlaceItem::CPlaceItem (CTileRules* pTileRules, CTile* pTile, INT xTile, INT x, INT y) :
	m_pTileRules(pTileRules),
	m_pTile(pTile),
	m_xTile(xTile),
	m_x(x), m_y(y)
{
	if(m_pTile)
	{
		RStrSet(m_rstrTile, m_pTile->GetTileSet()->GetName());
		SideAssertHr(RStrCopyToW(m_pTile->GetKey(), ARRAYSIZE(m_wzKey), m_wzKey, NULL));
	}
	else
	{
		m_rstrTile = NULL;
		SideAssertHr(TStrCchCpy(m_wzKey, ARRAYSIZE(m_wzKey), L"00000000"));
	}
}

CPlaceItem::~CPlaceItem ()
{
	RStrRelease(m_rstrTile);
}

bool CPlaceItem::IsSameTile (RSTRING rstrTile)
{
	CTileRuleSet* pRuleSet;
	return NULL == m_pTile || (SUCCEEDED(m_pTileRules->GetTileRuleSet(m_rstrTile, &pRuleSet)) && pRuleSet->IsSameTile(rstrTile));
}

bool CPlaceItem::IsBorderTile (RSTRING rstrTile)
{
	CTileRuleSet* pRuleSet;
	return SUCCEEDED(m_pTileRules->GetTileRuleSet(m_rstrTile, &pRuleSet)) && pRuleSet->IsBorderTile(rstrTile);
}

bool CPlaceItem::IsSpecialTile (RSTRING rstrTile)
{
	CTileRuleSet* pRuleSet;
	return SUCCEEDED(m_pTileRules->GetTileRuleSet(m_rstrTile, &pRuleSet)) && pRuleSet->IsSpecialTile(rstrTile);
}

VOID CPlaceItem::SetTileOnly (RSTRING rstrTile)
{
	RStrReplace(&m_rstrTile, rstrTile);
}

HRESULT CPlaceItem::GetTransitionTile (__in_opt RSTRING rstrTile, __out RSTRING* prstrTile)
{
	HRESULT hr;
	CTileRuleSet* pRuleSet;

	Check(m_pTileRules->GetTileRuleSet(rstrTile ? rstrTile : m_rstrTile, &pRuleSet));
	*prstrTile = pRuleSet->GetTransition();
	CheckIfIgnore(NULL == *prstrTile, HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
	RStrAddRef(*prstrTile);

Cleanup:
	return hr;
}

HRESULT CPlaceItem::SetNewKey (TRStrMap<CTileSet*>* pmapTileSets, PCWSTR pcwzKey)
{
	HRESULT hr;
	CTileSet* pTileSet;
	TArray<CTile*>* paTiles;

	Check(pmapTileSets->Find(m_rstrTile, &pTileSet));
	hr = pTileSet->FindFromKey(RSTRING_CAST(pcwzKey), &paTiles);
	if(FAILED(hr))
	{
		CTileRuleSet* pTileRuleSet;
		WCHAR wzSmooth[12];

		Check(TStrCchCpy(wzSmooth, ARRAYSIZE(wzSmooth), pcwzKey));
		Check(m_pTileRules->GetTileRuleSet(m_rstrTile, &pTileRuleSet));

		hr = pTileRuleSet->Smooth(wzSmooth);
		if(FAILED(hr) || FAILED(pTileSet->FindFromKey(RSTRING_CAST(wzSmooth), &paTiles)))
			hr = SmoothTile(pTileSet, wzSmooth, &paTiles);

		if(SUCCEEDED(hr))
			Check(TStrCchCpy(m_wzKey, ARRAYSIZE(m_wzKey), wzSmooth));
		else
		{
			hr = SetAltTile(pmapTileSets, pcwzKey);
			if(FAILED(hr))
			{
				CheckIf(1 != pTileSet->m_mapTiles.Length(), E_FAIL);
				Check(pTileSet->FindFromKey(RSTRING_CAST(L"00000000"), &paTiles));
				Check(TStrCchCpy(m_wzKey, ARRAYSIZE(m_wzKey), L"00000000"));
			}
		}

		Check(pmapTileSets->Find(m_rstrTile, &pTileSet));
		Check(pTileSet->FindFromKey(RSTRING_CAST(m_wzKey), &paTiles));
	}
	else
		Check(TStrCchCpy(m_wzKey, ARRAYSIZE(m_wzKey), pcwzKey));

	m_pTile = (*paTiles)[rand() % paTiles->Length()];

Cleanup:
	return hr;
}

HRESULT CPlaceItem::SmoothTile (CTileSet* pTileSet, PWSTR pwzKey, TArray<CTile*>** ppTiles)
{
	HRESULT hr;
	RSTRING rstrKey = NULL;

	if(pwzKey[Dir::WEST] != L'0' && pwzKey[Dir::NORTH] != L'0')
		pwzKey[Dir::NORTH_WEST] = L'1';

	if(pwzKey[Dir::WEST] != L'0' && pwzKey[Dir::SOUTH] != L'0')
		pwzKey[Dir::SOUTH_WEST] = L'1';

	if(pwzKey[Dir::EAST] != L'0' && pwzKey[Dir::NORTH] != L'0')
		pwzKey[Dir::NORTH_EAST] = L'1';

	if(pwzKey[Dir::EAST] != L'0' && pwzKey[Dir::SOUTH] != L'0')
		pwzKey[Dir::SOUTH_EAST] = L'1';

	Check(RStrCreateW(8, pwzKey, &rstrKey));
	CheckNoTrace(pTileSet->FindFromKey(rstrKey, ppTiles));

Cleanup:
	RStrRelease(rstrKey);
	return hr;
}

HRESULT CPlaceItem::SetAltTile (TRStrMap<CTileSet*>* pmapTileSets, PCWSTR pcwzKey)
{
	HRESULT hr;
	CTileRuleSet* pRuleSet;
	CTileSet* pTileSet;
	RSTRING rstrKey = NULL;
	TArray<CTile*>* pTiles;
	TStackRef<IJSONObject> srData;
	TStackRef<IJSONValue> srv;

	TStackRef<IJSONObject> srMatching, srTile;
	TStackRef<IJSONArray> srSame;
	RSTRING rstrNewTile;

	Check(m_pTileRules->GetTileRuleSet(m_rstrTile, &pRuleSet));
	Check(RStrCreateW(8, pcwzKey, &rstrKey));
	Check(pRuleSet->GetAltTile(rstrKey, &rstrNewTile));
	RStrRelease(m_rstrTile); m_rstrTile = rstrNewTile;

	Check(pmapTileSets->Find(m_rstrTile, &pTileSet));
	Check(pTileSet->FindFromKey(RSTRING_CAST(pcwzKey), &pTiles));

	Check(TStrCchCpy(m_wzKey, ARRAYSIZE(m_wzKey), pcwzKey));

Cleanup:
	RStrRelease(rstrKey);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
// CMapPainter
///////////////////////////////////////////////////////////////////////////////

CMapPainter::CMapPainter (CTileRules* pTileRules, MAPTILE* pWorld, INT xWorld, INT yWorld) :
	m_pTileRules(pTileRules),
	m_pWorld(pWorld),
	m_xWorld(xWorld),
	m_yWorld(yWorld)
{
}

CMapPainter::~CMapPainter ()
{
	m_aAffected.DeleteAll();
}

HRESULT CMapPainter::PaintTile (INT xTile, INT yTile, RSTRING rstrTile)
{
	HRESULT hr;
	CPlaceItem* pItem = NULL;

	Check(CPlaceItem::CreatePlaceItem(m_pTileRules, m_pWorld, m_xWorld, m_yWorld, xTile, yTile, &pItem));

	// Set the new tile onto the placed tile without actually picking a new tile sprite.
	pItem->SetTileOnly(rstrTile);

	// Add the placed tile to the affected array.
	Check(m_aAffected.Append(pItem));
	pItem = NULL;

Cleanup:
	__delete pItem;
	return hr;
}

HRESULT CMapPainter::CheckTransitions (INT xTile, INT yTile, RSTRING rstrTile)
{
	HRESULT hr;
	RSTRING rstrTransition = NULL;
	CPlaceItem* pPlaced = FindItem(m_aAffected, xTile, yTile), *pItem;
	TArray<CPlaceItem*> aTransitions;

	CheckIf(NULL == pPlaced, HRESULT_FROM_WIN32(ERROR_NOT_FOUND));

	// Check the immediately surrounding items for transition changes.
	for(INT i = 0; i < ARRAYSIZE(c_rgDirections); i++)
	{
		INT x = xTile + c_rgDirections[i].x;
		INT y = yTile + c_rgDirections[i].y;

		Check(CPlaceItem::CreatePlaceItem(m_pTileRules, m_pWorld, m_xWorld, m_yWorld, x, y, &pItem));

		if(!pItem->IsSameTile(rstrTile) &&
			!pItem->IsSpecialTile(pPlaced->m_rstrTile) &&
			!pPlaced->IsBorderTile(pItem->m_rstrTile) &&
			!pItem->IsBorderTile(pPlaced->m_rstrTile))
		{
			Check(pPlaced->GetTransitionTile(rstrTile, &rstrTransition));

			pItem->SetTileOnly(rstrTransition);

			RStrRelease(rstrTransition);
			rstrTransition = NULL;

			// Add the item to another temporary array so they can be processed after checking all the transition tiles.
			Check(aTransitions.Append(pItem));
		}

		Check(m_aAffected.Append(pItem));
		pItem = NULL;
	}

	// Add the tiles that surround the transition tiles.
	for(sysint n = 0; n < aTransitions.Length(); n++)
	{
		CPlaceItem* pTransition = aTransitions[n];

		for(INT i = 0; i < ARRAYSIZE(c_rgDirections); i++)
		{
			// Use the unadjusted x-tile coordinate for finding the item.
			INT x = pTransition->m_xTile + c_rgDirections[i].x;
			INT y = pTransition->m_y + c_rgDirections[i].y;

			if(NULL == FindItem(m_aAffected, x, y))
			{
				Check(CPlaceItem::CreatePlaceItem(m_pTileRules, m_pWorld, m_xWorld, m_yWorld, x, y, &pItem));
				Check(m_aAffected.Append(pItem));
				pItem = NULL;
			}
		}
	}

Cleanup:
	return hr;
}

HRESULT CMapPainter::Commit (TRStrMap<CTileSet*>* pmapTileSets, __out_opt TArray<POINT>* paTilesChanged)
{
	HRESULT hr = S_FALSE;

	// Update the tile for every grid item.
	for(sysint i = 0; i < m_aAffected.Length(); i++)
	{
		CPlaceItem* pPlaced = m_aAffected[i];
		if(pPlaced->m_pTile)
		{
			WCHAR wzAdjacent[12];

			GetTileKey(m_pWorld, pPlaced, m_aAffected, wzAdjacent);
			Check(pPlaced->SetNewKey(pmapTileSets, wzAdjacent));

			m_pWorld[pPlaced->m_y * m_xWorld + pPlaced->m_x].pTile = pPlaced->m_pTile;
			if(paTilesChanged)
			{
				POINT pt = { pPlaced->m_x, pPlaced->m_y };
				Check(paTilesChanged->Append(pt));
			}
		}
	}

Cleanup:
	m_aAffected.DeleteAll();
	return hr;
}

CPlaceItem* CMapPainter::FindItem (TArray<CPlaceItem*>& aItems, INT xTile, INT yTile)
{
	for(sysint i = 0; i < aItems.Length(); i++)
	{
		CPlaceItem* pItem = aItems[i];
		if(pItem->m_xTile == xTile && pItem->m_y == yTile)
			return pItem;
	}
	return NULL;
}

VOID CMapPainter::GetTileKey (MAPTILE* pWorld, CPlaceItem* pItem, TArray<CPlaceItem*>& aItems, PWSTR pwzKey)
{
	for(INT i = 0; i < ARRAYSIZE(c_rgDirections); i++)
	{
		INT x = pItem->m_x + c_rgDirections[i].x;
		INT y = pItem->m_y + c_rgDirections[i].y;
		RSTRING rstrOther = NULL;

		if(y >= 0 && y < m_yWorld)
		{
			CPlaceItem* pOther = FindItem(aItems, x, y);
			if(pOther)
				rstrOther = pOther->m_rstrTile;
			else
			{
				// Wrap the sides of the world.
				if(x < 0)
					x += m_xWorld;
				else if(x >= m_xWorld)
					x -= m_xWorld;

				rstrOther = pWorld[m_xWorld * y + x].pTile->GetTileSet()->GetName();
			}

			if(pItem->IsSameTile(rstrOther))
				pwzKey[i] = L'0';
			else if(pItem->IsSpecialTile(rstrOther))
				pwzKey[i] = L'2';
			else
				pwzKey[i] = L'1';
		}
		else
			pwzKey[i] = L'0';
	}

	pwzKey[ARRAYSIZE(c_rgDirections)] = L'\0';
}
