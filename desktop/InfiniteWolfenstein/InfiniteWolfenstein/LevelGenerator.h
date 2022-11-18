#pragma once

#include "Library\Core\BaseUnknown.h"
#include "Library\Core\SortedArray.h"
#include "Library\Util\MersenneTwister.h"
#include "Rooms.h"
#include "WallThemes.h"
#include "ChunkThemes.h"
#include "Levels.h"
#include "Models.h"
#include "Entities.h"

#define	MAX_CACHE_SIZE			512

class CMd5;
class CWallTextures;
class CLevelRenderer;
class CLevelGenerator;

struct BLOCK_DATA
{
	CEntity* m_pEntities;
	sysint idxBlock;
	sysint idxSides[4];		// West, North, East, South
};

struct ROOM_CONN
{
	INT nFloorA;
	INT nFloorB;
};

class CDungeonRegion :
	public CBaseUnknown
{
public:
	CChunkTheme* m_pChunk;
	RSTRING m_rstrMusic;
	INT m_xRegion, m_zRegion;
	INT m_xStartCell, m_zStartCell;
	UINT m_nList;

	BLOCK_DATA m_bRegion[REGION_WIDTH * REGION_WIDTH];
	TArray<CEntity*> m_aEntities;

	TSortedArray<CEntity*> m_aActive;

public:
	IMP_BASE_UNKNOWN

	EMPTY_UNK_MAP

public:
	CDungeonRegion (INT xRegion, INT zRegion);
	~CDungeonRegion ();

	VOID Reset (VOID);
	BLOCK_DATA* GetBlock (INT x, INT z);
	BLOCK_DATA* GetBlockReduced (INT x, INT z);
	VOID DrawEntities (MODEL_LIST* pModels);
	HRESULT AddActiveEntity (CEntity* pEntity);
	HRESULT RemoveActiveEntity (CEntity* pEntity);
	VOID UpdateActiveEntities (CLevelRenderer* pRenderer);
	VOID FindRandomStart (CLevelGenerator* pGenerator);
};

class CLevelGenerator
{
private:
	CWallTextures* m_pWalls;
	CWallThemes* m_pWallThemes;
	CChunkThemes* m_pChunkThemes;
	CLevels* m_pLevels;
	CRooms* m_pRooms, *m_pSpecial;
	CModels* m_pModels;
	DWORD m_dwSeed;

	// Selected level data
	INT m_nLevel;
	CLevelDef* m_pLevelDef;

	TSortedArray<BLOCK_DATA*> m_aConnectors;

	INT m_nFloor[REGION_WIDTH * REGION_WIDTH];
	INT m_nCurrentFloor;
	TArray<ROOM_CONN> m_aRoomConnections;

	INT m_xHellStart, m_zHellStart;

public:
	CLevelGenerator (CWallTextures* pWalls, CWallThemes* pWallThemes, CChunkThemes* pChunkThemes, CLevels* pLevels, CRooms* pRooms, CRooms* pSpecial, CModels* pModels, DWORD dwSeed);
	~CLevelGenerator ();

	HRESULT SetLevel (INT nLevel);
	VOID SetHellStart (INT xHellStart, INT zHellStart);
	INT GetLevel (VOID) { return m_nLevel; }
	RSTRING GetLevelName (VOID);
	HRESULT GenerateRegion (CDungeonRegion* pRegion, CLevelRenderer* pRenderer);
	CWallTextures* GetWalls (VOID) { return m_pWalls; }
	CModels* GetModels (VOID) { return m_pModels; }
	BOOL CheckElevatorConnection (CDungeonRegion* pRegion, INT xBlock, INT zBlock, INT nLevelOffset);

	VOID RandomizeLevelRegion (CMersenneTwister* pmt, INT xRegion, INT zRegion);
	VOID RandomizeRegionEdge (CMersenneTwister* pmt, INT xRegionA, INT yLevelA, INT zRegionA, INT xRegionB, INT yLevelB, INT zRegionB);
	VOID AddRegionCoords (CMd5& md5, INT xRegion, INT yLevel, INT zRegion);

	VOID GetNextElevator (CMersenneTwister& mtEdge, __out INT& x, __out INT& z);

private:
	HRESULT CanPlaceRoom (CDungeonRegion* pRegion, CRoom* pRoom, INT xPlace, INT zPlace, INT xOffset, INT zOffset);
	HRESULT PlotEdgeConnector (CMersenneTwister& mt, CDungeonRegion* pRegion, INT nPlacement, bool fVertical, INT xRegionOffset, INT zRegionOffset, PCWSTR pcwzConnector, INT cchConnector);
	HRESULT PlotElevators (CMersenneTwister& mt, CDungeonRegion* pRegion, INT nLevelOffset);
	HRESULT PlotStartRoom (CMersenneTwister& mt, CDungeonRegion* pRegion);
	HRESULT PlotHellStart (CMersenneTwister& mt, CDungeonRegion* pRegion);
	HRESULT PlotSpearRoom (CMersenneTwister& mt, CDungeonRegion* pRegion);
	HRESULT PlotFabricatedRooms (CMersenneTwister& mt, CDungeonRegion* pRegion);
	HRESULT PlotFabricatedRoom (CMersenneTwister& mt, CDungeonRegion* pRegion, CRoom* pRoom);

	HRESULT TraceTunnelFromConnector (CDungeonRegion* pRegion, BLOCK_DATA* pConnector);
	HRESULT TraceTunnelDirNext (CDungeonRegion* pRegion, INT nSourceFloor, INT nDir, INT x, INT z, INT cMaxTurns);
	HRESULT TraceTunnelDir (CDungeonRegion* pRegion, INT nSourceFloor, INT nDir, INT x, INT z, INT cMaxTurns);

	BOOL AreRoomsConnected (INT nFloorA, INT nFloorB);
	HRESULT PlaceRoom (CMersenneTwister& mt, CDungeonRegion* pRegion, CRoom* pRoom, CWallTheme* pTheme, INT xPlace, INT zPlace, INT xOffset, INT zOffset);
	HRESULT AddEntity (CDungeonRegion* pRegion, IJSONObject* pEntity, INT x, INT z);

	VOID FillEmptyBlocks (CMersenneTwister& mt, CDungeonRegion* pRegion);
	HRESULT AddElevatorSwitch (CDungeonRegion* pRegion, BLOCK_DATA* pBlock, INT x, INT z, sysint idxUp);
	HRESULT SetupDoors (CMersenneTwister& mt, CDungeonRegion* pRegion, CLevelRenderer* pRenderer);

	template <typename TTheme, typename TItem>
	TTheme* TPickRandomTheme (CMersenneTwister& mt, INT nMaxRandom, TItem* pItems, INT cItems)
	{
		INT nValue = static_cast<INT>(mt.Random(nMaxRandom));
		for(sysint i = 0; i < cItems; i++)
		{
			if(nValue < pItems[i].nMaxRange)
				return pItems[i].pTheme;
		}
		return NULL;
	}
};
