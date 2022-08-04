#pragma once

#include "Library\Core\RStrMap.h"

#define	REGION_WIDTH			128

interface IJSONArray;
interface IJSONObject;

class CWallTextures;

struct ROOM_BLOCK
{
	INT x, z;
	sysint idxBlock;
	IJSONObject* pEntity;
};

class CRoom
{
public:
	TArray<ROOM_BLOCK> m_aBlocks;
	TArray<sysint> m_aConnectors;	// Index to m_aBlocks
	TArray<sysint> m_aAnchors;		// Index to m_aBlocks
	RECT m_rcExtents;

public:
	CRoom ();
	~CRoom ();

	BOOL CheckBounds (INT xPlace, INT zPlace, INT xOffset, INT zOffset);
};

class CRooms
{
private:
	TRStrMap<CRoom*> m_mapRooms;

public:
	CRooms ();
	~CRooms ();

	HRESULT Add (CWallTextures* pTextures, IJSONArray* pRooms);
	HRESULT Find (RSTRING rstrRoomW, __deref_out CRoom** ppRoom);
	sysint Count (VOID) { return m_mapRooms.Length(); }
	CRoom* GetRoom (sysint idxRoom) { return *m_mapRooms[idxRoom]; }

private:
	HRESULT AddRoom (CWallTextures* pTextures, RSTRING rstrNameW, IJSONArray* pPalette, IJSONArray* pData);
	HRESULT RotateAndAdd (CWallTextures* pTextures, RSTRING rstrNameW, IJSONArray* pPalette, IJSONArray* pData);
};
