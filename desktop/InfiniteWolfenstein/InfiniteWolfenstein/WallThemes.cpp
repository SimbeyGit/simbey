#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "WallTextures.h"
#include "WallThemes.h"

CWallTheme::CWallTheme () :
	m_pidxWalls(NULL), m_cWalls(0),
	m_pidxDecorations(NULL), m_cDecorations(0)
{
}

CWallTheme::~CWallTheme ()
{
	__delete_array m_pidxWalls;
	__delete_array m_pidxDecorations;
}

CWallThemeNamespace::CWallThemeNamespace (RSTRING rstrNamespaceW)
{
}

CWallThemeNamespace::~CWallThemeNamespace ()
{
	m_mapWallThemes.DeleteAll();
}

HRESULT CWallThemeNamespace::Add (CWallTextures* pWalls, RSTRING rstrNameW, IJSONObject* pItem)
{
	HRESULT hr;
	CWallTheme* pWallTheme = NULL;

	pWallTheme = __new CWallTheme;
	CheckAlloc(pWallTheme);

	Check(LoadTextures(pWalls, pItem, L"walls", &pWallTheme->m_cWalls, &pWallTheme->m_pidxWalls));
	Check(LoadTextures(pWalls, pItem, L"decorations", &pWallTheme->m_cDecorations, &pWallTheme->m_pidxDecorations));
	Check(m_mapWallThemes.Add(rstrNameW, pWallTheme));

Cleanup:
	if(FAILED(hr))
		__delete pWallTheme;
	return hr;
}

HRESULT CWallThemeNamespace::Find (RSTRING rstrNameW, __deref_out CWallTheme** ppItem)
{
	return m_mapWallThemes.Find(rstrNameW, ppItem);
}

HRESULT CWallThemeNamespace::LoadTextures (CWallTextures* pWalls, IJSONObject* pItem, PCWSTR pcwzList, __out sysint* pcTextures, __deref_out sysint** ppidxTextures)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONArray> srData;
	RSTRING rstrTextureW = NULL;

	Check(pItem->FindNonNullValueW(pcwzList, &srv));
	Check(srv->GetArray(&srData));
	srv.Release();

	*pcTextures = srData->Count();
	*ppidxTextures = __new sysint[*pcTextures];
	CheckAlloc(*ppidxTextures);

	for(sysint i = 0; i < *pcTextures; i++)
	{
		Check(srData->GetString(i, &rstrTextureW));
		Check(pWalls->Resolve(rstrTextureW, (*ppidxTextures) + i));
		RStrRelease(rstrTextureW); rstrTextureW = NULL;
	}

Cleanup:
	RStrRelease(rstrTextureW);
	return hr;
}
