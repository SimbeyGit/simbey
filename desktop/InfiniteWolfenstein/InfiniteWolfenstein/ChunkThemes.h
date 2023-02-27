#pragma once

#include "WallThemes.h"

struct ROOM_DEF
{
	CWallTheme* pTheme;
	INT nMaxRange;
};

class CChunkTheme
{
public:
	CWallTheme* m_pFill;
	INT m_pctDecorations;
	IJSONArray* m_pMusic;
	IJSONArray* m_pDoorTypes;
	IJSONArray* m_pLockedTypes;
	IJSONArray* m_pSplitTypes;
	IJSONArray* m_pElevatorTypes;
	IJSONArray* m_pSwitchTypes;
	IJSONArray* m_pRailingTypes;
	RSTRING m_rstrFloorW, m_rstrCeilingW;
	bool m_fSpear;

	ROOM_DEF* m_pRooms;
	sysint m_cRooms;
	INT m_nRoomMaxRandom;

public:
	CChunkTheme ();
	~CChunkTheme ();
};

class CChunkThemeNamespace
{
private:
	TRStrMap<CChunkTheme*> m_mapChunkThemes;

public:
	CChunkThemeNamespace (RSTRING rstrNamespaceW);
	~CChunkThemeNamespace ();

	HRESULT Add (CWallThemes* pWallThemes, RSTRING rstrNameW, IJSONObject* pItem);
	HRESULT Find (RSTRING rstrNameW, __deref_out CChunkTheme** ppItem);
};

typedef TTemplateData<CChunkThemeNamespace, CWallThemes, CChunkTheme> CChunkThemes;
