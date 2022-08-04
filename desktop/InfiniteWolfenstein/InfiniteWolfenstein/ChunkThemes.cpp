#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "ChunkThemes.h"

CChunkTheme::CChunkTheme () :
	m_pMusic(NULL),
	m_pDoorTypes(NULL),
	m_pLockedTypes(NULL),
	m_pElevatorTypes(NULL),
	m_pSwitchTypes(NULL),
	m_pRailingTypes(NULL),
	m_rstrFloorW(NULL),
	m_rstrCeilingW(NULL),
	m_fSpear(false),
	m_pRooms(NULL),
	m_cRooms(NULL),
	m_nRoomMaxRandom(0)
{
}

CChunkTheme::~CChunkTheme ()
{
	__delete_array m_pRooms;

	SafeRelease(m_pMusic);
	SafeRelease(m_pDoorTypes);
	SafeRelease(m_pLockedTypes);
	SafeRelease(m_pElevatorTypes);
	SafeRelease(m_pSwitchTypes);
	SafeRelease(m_pRailingTypes);

	RStrRelease(m_rstrFloorW);
	RStrRelease(m_rstrCeilingW);
}

CChunkThemeNamespace::CChunkThemeNamespace (RSTRING rstrNamespaceW)
{
}

CChunkThemeNamespace::~CChunkThemeNamespace ()
{
	m_mapChunkThemes.DeleteAll();
}

HRESULT CChunkThemeNamespace::Add (CWallThemes* pWallThemes, RSTRING rstrNameW, IJSONObject* pItem)
{
	HRESULT hr;
	CChunkTheme* pChunkTheme = NULL;
	TStackRef<IJSONValue> srv;
	RSTRING rstrFillW = NULL;

	pChunkTheme = __new CChunkTheme;
	CheckAlloc(pChunkTheme);

	Check(pItem->FindNonNullValueW(L"fill", &srv));
	Check(srv->GetString(&rstrFillW));
	srv.Release();

	Check(pWallThemes->Find(rstrFillW, &pChunkTheme->m_pFill));

	Check(pItem->FindNonNullValueW(L"decorations", &srv));
	Check(srv->GetInteger(&pChunkTheme->m_pctDecorations));
	srv.Release();

	Check(pItem->FindNonNullValueW(L"music", &srv));
	Check(srv->GetArray(&pChunkTheme->m_pMusic));
	srv.Release();

	Check(pItem->FindNonNullValueW(L"door", &srv));
	Check(srv->GetArray(&pChunkTheme->m_pDoorTypes));
	srv.Release();

	Check(pItem->FindNonNullValueW(L"locked_door", &srv));
	Check(srv->GetArray(&pChunkTheme->m_pLockedTypes));
	srv.Release();

	Check(pItem->FindNonNullValueW(L"elevator", &srv));
	Check(srv->GetArray(&pChunkTheme->m_pElevatorTypes));
	srv.Release();

	Check(pItem->FindNonNullValueW(L"switch", &srv));
	Check(srv->GetArray(&pChunkTheme->m_pSwitchTypes));
	srv.Release();

	Check(pItem->FindNonNullValueW(L"railing", &srv));
	Check(srv->GetArray(&pChunkTheme->m_pRailingTypes));
	srv.Release();

	Check(pItem->FindNonNullValueW(L"floor", &srv));
	Check(srv->GetString(&pChunkTheme->m_rstrFloorW));
	srv.Release();

	Check(pItem->FindNonNullValueW(L"ceiling", &srv));
	Check(srv->GetString(&pChunkTheme->m_rstrCeilingW));
	srv.Release();

	Check(TParseChanceArray(pWallThemes, pItem, L"rooms", &pChunkTheme->m_pRooms, &pChunkTheme->m_cRooms, &pChunkTheme->m_nRoomMaxRandom));
	Check(m_mapChunkThemes.Add(rstrNameW, pChunkTheme));
	srv.Release();

	if(SUCCEEDED(pItem->FindNonNullValueW(L"spear", &srv)))
		Check(srv->GetBoolean(&pChunkTheme->m_fSpear));

Cleanup:
	RStrRelease(rstrFillW);
	if(FAILED(hr))
		__delete pChunkTheme;
	return hr;
}

HRESULT CChunkThemeNamespace::Find (RSTRING rstrNameW, __deref_out CChunkTheme** ppItem)
{
	return m_mapChunkThemes.Find(rstrNameW, ppItem);
}
