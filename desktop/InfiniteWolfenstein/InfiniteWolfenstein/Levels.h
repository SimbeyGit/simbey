#pragma once

#include "ChunkThemes.h"

struct CHUNK_DEF
{
	CChunkTheme* pTheme;
	INT nMaxRange;
};

class CLevelDef
{
public:
	RSTRING m_rstrLevelNameW;
	INT m_nMinimumAllowed;
	CHUNK_DEF* m_pChunks;
	sysint m_cChunks;
	INT m_nChunkMaxRandom;

public:
	CLevelDef (RSTRING rstrLevelNameW);
	~CLevelDef ();
};

class CLevelNamespace
{
private:
	TRStrMap<CLevelDef*> m_mapLevels;

public:
	CLevelNamespace (RSTRING rstrNamespaceW);
	~CLevelNamespace ();

	HRESULT Add (CChunkThemes* pChunkThemes, RSTRING rstrNameW, IJSONObject* pItem);
	HRESULT Find (RSTRING rstrNameW, __deref_out CLevelDef** ppItem);
	HRESULT GetItems (__out TArray<CLevelDef*>& aItems);
};

typedef TTemplateData<CLevelNamespace, CChunkThemes, CLevelDef> CLevels;
