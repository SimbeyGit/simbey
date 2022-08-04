#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Levels.h"

CLevelDef::CLevelDef (RSTRING rstrLevelNameW) :
	m_nMinimumAllowed(0),
	m_pChunks(NULL),
	m_cChunks(0),
	m_nChunkMaxRandom(0)
{
	RStrSet(m_rstrLevelNameW, rstrLevelNameW);
}

CLevelDef::~CLevelDef ()
{
	__delete_array m_pChunks;
	RStrRelease(m_rstrLevelNameW);
}

CLevelNamespace::CLevelNamespace (RSTRING rstrNamespaceW)
{
}

CLevelNamespace::~CLevelNamespace ()
{
	m_mapLevels.DeleteAll();
}

HRESULT CLevelNamespace::Add (CChunkThemes* pChunkThemes, RSTRING rstrNameW, IJSONObject* pItem)
{
	HRESULT hr;
	CLevelDef* pLevelDef = NULL;
	TStackRef<IJSONValue> srv;

	pLevelDef = __new CLevelDef(rstrNameW);
	CheckAlloc(pLevelDef);

	Check(pItem->FindNonNullValueW(L"minimum", &srv));
	Check(srv->GetInteger(&pLevelDef->m_nMinimumAllowed));

	Check(TParseChanceArray(pChunkThemes, pItem, L"chunks", &pLevelDef->m_pChunks, &pLevelDef->m_cChunks, &pLevelDef->m_nChunkMaxRandom));
	Check(m_mapLevels.Add(rstrNameW, pLevelDef));

Cleanup:
	if(FAILED(hr))
		__delete pLevelDef;
	return hr;
}

HRESULT CLevelNamespace::Find (RSTRING rstrNameW, __deref_out CLevelDef** ppItem)
{
	return m_mapLevels.Find(rstrNameW, ppItem);
}

HRESULT CLevelNamespace::GetItems (__out TArray<CLevelDef*>& aItems)
{
	HRESULT hr = S_FALSE;

	for(sysint i = 0; i < m_mapLevels.Length(); i++)
	{
		CLevelDef* pLevelDef = *(m_mapLevels.GetValuePtr(i));
		Check(aItems.Append(pLevelDef));
	}

Cleanup:
	return hr;
}
