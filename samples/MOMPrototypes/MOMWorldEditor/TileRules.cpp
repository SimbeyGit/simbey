#include <windows.h>
#include "TileRules.h"

template <typename T>
HRESULT TLoadSmoothingObjects (TArray<T*>& aObjects, IJSONValue* pvArray)
{
	HRESULT hr;
	TStackRef<IJSONArray> srArray;
	T* pObject = NULL;

	Check(pvArray->GetArray(&srArray));

	for(sysint i = 0; i < srArray->Count(); i++)
	{
		TStackRef<IJSONObject> srObjectData;

		Check(srArray->GetObject(i, &srObjectData));

		pObject = __new T;
		CheckAlloc(pObject);
		Check(pObject->Initialize(srObjectData));
		Check(aObjects.Append(pObject));
		pObject = NULL;
	}

Cleanup:
	__delete pObject;
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
// CSmoothingCondition
///////////////////////////////////////////////////////////////////////////////

CSmoothingCondition::CSmoothingCondition () :
	m_pnDirections(NULL),
	m_cDirections(0)
{
}

CSmoothingCondition::~CSmoothingCondition ()
{
	__delete_array m_pnDirections;
}

HRESULT CSmoothingCondition::Initialize (IJSONObject* pCondition)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONArray> srDirections;

	Check(pCondition->FindNonNullValueW(L"repetitions", &srv));
	Check(srv->GetInteger(&m_cRepetitions));
	srv.Release();

	Check(pCondition->FindNonNullValueW(L"value", &srv));
	Check(srv->GetInteger(&m_nValue));
	srv.Release();

	Check(pCondition->FindNonNullValueW(L"directions", &srv));
	Check(srv->GetArray(&srDirections));

	m_cDirections = srDirections->Count();
	m_pnDirections = __new INT[m_cDirections];
	CheckAlloc(m_pnDirections);
	for(sysint i = 0; i < m_cDirections; i++)
	{
		srv.Release();
		Check(srDirections->GetValue(i, &srv));
		Check(srv->GetInteger(m_pnDirections + i));
	}

Cleanup:
	return hr;
}

BOOL CSmoothingCondition::Match (PCWSTR pcwzKey)
{
	INT nCount = 0;
	WCHAR wchMatch = m_nValue + L'0';

	for(INT i = 0; i < m_cDirections; i++)
	{
		if(pcwzKey[m_pnDirections[i] - 1] == wchMatch)
			nCount++;
	}

	return nCount == m_cRepetitions;
}

///////////////////////////////////////////////////////////////////////////////
// CSmoothingSet
///////////////////////////////////////////////////////////////////////////////

CSmoothingSet::CSmoothingSet ()
{
}

CSmoothingSet::~CSmoothingSet ()
{
}

HRESULT CSmoothingSet::Initialize (IJSONObject* pSet)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;

	Check(pSet->FindNonNullValueW(L"direction", &srv));
	Check(srv->GetInteger(&m_nDirection));
	srv.Release();

	Check(pSet->FindNonNullValueW(L"value", &srv));
	Check(srv->GetInteger(&m_nValue));

Cleanup:
	return hr;
}

VOID CSmoothingSet::Apply (PWSTR pwzKey)
{
	pwzKey[m_nDirection - 1] = L'0' + m_nValue;
}

///////////////////////////////////////////////////////////////////////////////
// CSmoothingReduction
///////////////////////////////////////////////////////////////////////////////

CSmoothingReduction::CSmoothingReduction () :
	m_rstrDescription(NULL)
{
}

CSmoothingReduction::~CSmoothingReduction ()
{
	m_aConditions.DeleteAll();
	m_aSets.DeleteAll();
	RStrRelease(m_rstrDescription);
}

HRESULT CSmoothingReduction::Initialize (IJSONObject* pReduction)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;

	Check(pReduction->FindNonNullValueW(L"description", &srv));
	Check(srv->GetString(&m_rstrDescription));
	srv.Release();

	Check(pReduction->FindNonNullValueW(L"conditions", &srv));
	Check(TLoadSmoothingObjects(m_aConditions, srv));
	srv.Release();

	Check(pReduction->FindNonNullValueW(L"set", &srv));
	Check(TLoadSmoothingObjects(m_aSets, srv));

Cleanup:
	return hr;
}

BOOL CSmoothingReduction::Execute (PWSTR pwzKey)
{
	for(sysint i = 0; i < m_aConditions.Length(); i++)
	{
		if(!m_aConditions[i]->Match(pwzKey))
			return FALSE;
	}

	for(sysint i = 0; i < m_aSets.Length(); i++)
		m_aSets[i]->Apply(pwzKey);

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
// CSmoothingSystem
///////////////////////////////////////////////////////////////////////////////

CSmoothingSystem::CSmoothingSystem () :
	m_rstrDescription(NULL),
	m_nMaxValueEachDirection(NULL)
{
}

CSmoothingSystem::~CSmoothingSystem ()
{
	m_aReductions.DeleteAll();
	RStrRelease(m_rstrDescription);
}

HRESULT CSmoothingSystem::Initialize (IJSONObject* pSystem)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONArray> srReductions;
	CSmoothingReduction* pReduction = NULL;

	Check(pSystem->FindNonNullValueW(L"description", &srv));
	Check(srv->GetString(&m_rstrDescription));
	srv.Release();

	Check(pSystem->FindNonNullValueW(L"maxValueEachDirection", &srv));
	Check(srv->GetInteger(&m_nMaxValueEachDirection));
	srv.Release();

	if(SUCCEEDED(pSystem->FindNonNullValueW(L"reductions", &srv)))
		Check(TLoadSmoothingObjects(m_aReductions, srv));

Cleanup:
	__delete pReduction;
	return hr;
}

HRESULT CSmoothingSystem::Smooth (PWSTR pwzKey)
{
	INT cSmoothed = 0;

	for(sysint i = 0; i < m_aReductions.Length(); i++)
	{
		if(m_aReductions[i]->Execute(pwzKey))
			cSmoothed++;
	}

	return 0 < cSmoothed ? S_OK : E_FAIL;
}

///////////////////////////////////////////////////////////////////////////////
// CTileRuleSet
///////////////////////////////////////////////////////////////////////////////

CTileRuleSet::CTileRuleSet () :
	m_pSame(NULL), m_pBorder(NULL), m_pSpecial(NULL),
	m_pAltTiles(NULL), m_rstrTransition(NULL),
	m_pSmoothingSystem(NULL)
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

HRESULT CTileRuleSet::Initialize (TRStrMap<CSmoothingSystem*>& mapSmoothingSystems, IJSONValue* pvRuleSet)
{
	HRESULT hr;
	TStackRef<IJSONObject> srRuleSet;
	TStackRef<IJSONValue> srv;
	RSTRING rstrSmoothing = NULL;

	Check(pvRuleSet->GetObject(&srRuleSet));

	Check(srRuleSet->FindNonNullValueW(L"smoothing", &srv));
	Check(srv->GetString(&rstrSmoothing));
	srv.Release();

	Check(mapSmoothingSystems.Find(rstrSmoothing, &m_pSmoothingSystem));

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
	RStrRelease(rstrSmoothing);
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

HRESULT CTileRuleSet::Smooth (PWSTR pwzKey)
{
	return m_pSmoothingSystem->Smooth(pwzKey);
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

HRESULT CTileRules::Initialize (TRStrMap<CSmoothingSystem*>& mapSmoothingSystems, IJSONValue* pvRules)
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
		Check(pRuleSet->Initialize(mapSmoothingSystems, srv));
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
