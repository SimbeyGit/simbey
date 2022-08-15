#include <windows.h>
#include "TileRules.h"

///////////////////////////////////////////////////////////////////////////////
// CTileRuleSet
///////////////////////////////////////////////////////////////////////////////

CTileRuleSet::CTileRuleSet () :
	m_pSame(NULL), m_pBorder(NULL), m_pSpecial(NULL),
	m_pAltTiles(NULL), m_rstrTransition(NULL)
{
}

CTileRuleSet::~CTileRuleSet ()
{
	SafeRelease(m_pSame);
	SafeRelease(m_pBorder);
	SafeRelease(m_pSpecial);
	SafeRelease(m_pAltTiles);
	RStrRelease(m_rstrTransition);
}

HRESULT CTileRuleSet::Initialize (IJSONValue* pvRuleSet)
{
	HRESULT hr;
	TStackRef<IJSONObject> srRuleSet;
	TStackRef<IJSONValue> srv;

	Check(pvRuleSet->GetObject(&srRuleSet));

	Check(srRuleSet->FindNonNullValueW(L"same", &srv));
	Check(srv->GetArray(&m_pSame));
	srv.Release();

	if(SUCCEEDED(srRuleSet->FindNonNullValueW(L"border", &srv)))
	{
		Check(srv->GetArray(&m_pBorder));
		srv.Release();
	}

	if(SUCCEEDED(srRuleSet->FindNonNullValueW(L"special", &srv)))
	{
		Check(srv->GetArray(&m_pSpecial));
		srv.Release();
	}

	if(SUCCEEDED(srRuleSet->FindNonNullValueW(L"tiles", &srv)))
	{
		Check(srv->GetArray(&m_pAltTiles));
		srv.Release();
	}

	if(SUCCEEDED(srRuleSet->FindNonNullValueW(L"transition", &srv)))
		Check(srv->GetString(&m_rstrTransition));

Cleanup:
	return hr;
}

bool CTileRuleSet::IsSameTile (RSTRING rstrTile)
{
	return IsCompatibleTile(m_pSame, rstrTile);
}

bool CTileRuleSet::IsBorderTile (RSTRING rstrTile)
{
	return IsCompatibleTile(m_pBorder, rstrTile);
}

bool CTileRuleSet::IsSpecialTile (RSTRING rstrTile)
{
	return IsCompatibleTile(m_pSpecial, rstrTile);
}

HRESULT CTileRuleSet::GetAltTile (RSTRING rstrKey, __deref_out RSTRING* prstrTile)
{
	HRESULT hr;
	TStackRef<IJSONObject> srTile;
	TStackRef<IJSONValue> srv;

	CheckIfIgnore(NULL == m_pAltTiles, HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
	Check(JSONFindArrayObject(m_pAltTiles, RSTRING_CAST(L"key"), rstrKey, &srTile, NULL));
	Check(srTile->FindNonNullValueW(L"tile", &srv));
	Check(srv->GetString(prstrTile));

Cleanup:
	return hr;
}

bool CTileRuleSet::IsCompatibleTile (__in_opt IJSONArray* pArray, RSTRING rstrTile)
{
	return pArray && SUCCEEDED(JSONFindArrayString(pArray, rstrTile, NULL));
}

///////////////////////////////////////////////////////////////////////////////
// CTileRules
///////////////////////////////////////////////////////////////////////////////

CTileRules::CTileRules ()
{
}

CTileRules::~CTileRules ()
{
	m_mapTiles.DeleteAll();
}

HRESULT CTileRules::Initialize (IJSONValue* pvRules)
{
	HRESULT hr;
	TStackRef<IJSONObject> srRules;
	RSTRING rstrName = NULL;
	CTileRuleSet* pRuleSet = NULL;

	Check(pvRules->GetObject(&srRules));
	for(sysint i = 0; i < srRules->Count(); i++)
	{
		TStackRef<IJSONValue> srv;

		Check(srRules->GetValueName(i, &rstrName));
		Check(srRules->GetValueByIndex(i, &srv));

		pRuleSet = __new CTileRuleSet;
		CheckAlloc(pRuleSet);
		Check(pRuleSet->Initialize(srv));
		Check(m_mapTiles.Add(rstrName, pRuleSet));
		pRuleSet = NULL;

		RStrRelease(rstrName); rstrName = NULL;
	}

Cleanup:
	__delete pRuleSet;
	RStrRelease(rstrName);
	return hr;
}

HRESULT CTileRules::GetTileRuleSet (RSTRING rstrTile, __deref_out CTileRuleSet** ppTileRuleSet)
{
	return m_mapTiles.Find(rstrTile, ppTileRuleSet);
}
