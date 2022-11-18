#include <math.h>
#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Crypto\MD5.h"
#include "3rdParty\glew\include\GL\glew.h"
#include "WallTextures.h"
#include "Rooms.h"
#include "LevelGenerator.h"

const POINT c_ptDir[] =
{
	{ -1, 0 },	// West
	{ 0, -1 },	// North
	{ 1, 0 },	// East
	{ 0, 1 }	// South
};

CDungeonRegion::CDungeonRegion (INT xRegion, INT zRegion) :
	m_pChunk(NULL),
	m_rstrMusic(NULL),
	m_xRegion(xRegion),
	m_zRegion(zRegion),
	m_xStartCell(-1),
	m_zStartCell(-1),
	m_nList(glGenLists(1))
{
	ZeroMemory(m_bRegion, sizeof(m_bRegion));
}

CDungeonRegion::~CDungeonRegion ()
{
	Reset();
	glDeleteLists(m_nList, 1);
}

VOID CDungeonRegion::Reset (VOID)
{
	ZeroMemory(m_bRegion, sizeof(m_bRegion));
	m_aEntities.DeleteAll();

	if(m_rstrMusic)
	{
		RStrRelease(m_rstrMusic);
		m_rstrMusic = NULL;
	}
}

BLOCK_DATA* CDungeonRegion::GetBlock (INT x, INT z)
{
	if(x < 0 || x >= REGION_WIDTH)
		return NULL;
	if(z < 0 || z >= REGION_WIDTH)
		return NULL;
	return m_bRegion + (REGION_WIDTH * z + x);
}

BLOCK_DATA* CDungeonRegion::GetBlockReduced (INT x, INT z)
{
	if(x < 1 || x >= REGION_WIDTH - 1)
		return NULL;
	if(z < 1 || z >= REGION_WIDTH - 1)
		return NULL;
	return m_bRegion + (REGION_WIDTH * z + x);
}

VOID CDungeonRegion::DrawEntities (MODEL_LIST* pModels)
{
	glBegin(GL_QUADS);
	for(sysint i = 0; i < m_aEntities.Length(); i++)
		m_aEntities[i]->Draw(pModels);
	glEnd();
}

HRESULT CDungeonRegion::AddActiveEntity (CEntity* pEntity)
{
	return m_aActive.InsertSorted(pEntity);
}

HRESULT CDungeonRegion::RemoveActiveEntity (CEntity* pEntity)
{
	return m_aActive.Remove(pEntity);
}

VOID CDungeonRegion::UpdateActiveEntities (CLevelRenderer* pRenderer)
{
	for(sysint i = 0; i < m_aActive.Length(); i++)
		m_aActive[i]->Update(pRenderer, this);
}

VOID CDungeonRegion::FindRandomStart (CLevelGenerator* pGenerator)
{
	CMersenneTwister mt;
	TArray<POINT> aStarts;
	BLOCK_DATA* pbRegion = m_bRegion;

	for(INT z = 0; z < REGION_WIDTH; z++)
	{
		for(INT x = 0; x < REGION_WIDTH; x++)
		{
			if(TYPE_ANY_SWITCH == pbRegion->idxBlock)
			{
				POINT pt = {x, z};
				aStarts.Append(pt);
			}

			pbRegion++;
		}
	}

	pGenerator->RandomizeLevelRegion(&mt, m_xRegion, m_zRegion);
	DWORD idxStart = mt.Random(aStarts.Length());

	POINT& pt = aStarts[idxStart];
	if(TYPE_ANY_FLOOR == m_bRegion[pt.y * REGION_WIDTH + (pt.x - 1)].idxBlock)
	{
		m_xStartCell = pt.x - 1;
		m_zStartCell = pt.y;
	}
	else if(TYPE_ANY_FLOOR == m_bRegion[pt.y * REGION_WIDTH + (pt.x + 1)].idxBlock)
	{
		m_xStartCell = pt.x + 1;
		m_zStartCell = pt.y;
	}
	else if(TYPE_ANY_FLOOR == m_bRegion[(pt.y - 1) * REGION_WIDTH + pt.x].idxBlock)
	{
		m_xStartCell = pt.x;
		m_zStartCell = pt.y - 1;
	}
	else if(TYPE_ANY_FLOOR == m_bRegion[(pt.y + 1) * REGION_WIDTH + pt.x].idxBlock)
	{
		m_xStartCell = pt.x;
		m_zStartCell = pt.y + 1;
	}
}

CLevelGenerator::CLevelGenerator (CWallTextures* pWalls, CWallThemes* pWallThemes, CChunkThemes* pChunkThemes, CLevels* pLevels, CRooms* pRooms, CRooms* pSpecial, CModels* pModels, DWORD dwSeed) :
	m_pWalls(pWalls),
	m_pWallThemes(pWallThemes),
	m_pChunkThemes(pChunkThemes),
	m_pLevels(pLevels),
	m_pRooms(pRooms),
	m_pSpecial(pSpecial),
	m_pModels(pModels),
	m_dwSeed(dwSeed),
	m_nLevel(-1),
	m_pLevelDef(NULL),
	m_xHellStart(-1),
	m_zHellStart(-1)
{
}

CLevelGenerator::~CLevelGenerator ()
{
}

HRESULT CLevelGenerator::SetLevel (INT nLevel)
{
	HRESULT hr;
	TArray<CLevelDef*> aLevels, aRandom;
	CMd5 md5;
	BYTE bDigest[16];

	m_nLevel = nLevel;

	Check(m_pLevels->GetItems(aLevels));

	if(-1 == nLevel)
	{
		for(sysint i = 0; i < aLevels.Length(); i++)
		{
			CLevelDef* pLevelDef = aLevels[i];
			if(-1 == pLevelDef->m_nMinimumAllowed)
				Check(aRandom.Append(pLevelDef));
		}
	}
	else
	{
		for(sysint i = 0; i < aLevels.Length(); i++)
		{
			CLevelDef* pLevelDef = aLevels[i];
			if(nLevel >= pLevelDef->m_nMinimumAllowed && -1 != pLevelDef->m_nMinimumAllowed)
				Check(aRandom.Append(pLevelDef));
		}
	}

	CheckIf(0 == aRandom.Length(), E_FAIL);

	md5.AddData((PBYTE)&m_dwSeed, sizeof(m_dwSeed));
	md5.AddData((PBYTE)&nLevel, sizeof(nLevel));
	md5.GetDigest(bDigest);

	{
		CMersenneTwister mt(*reinterpret_cast<DWORD*>(bDigest));
		m_pLevelDef = aRandom[mt.Random(aRandom.Length())];
	}

Cleanup:
	return hr;
}

VOID CLevelGenerator::SetHellStart (INT xHellStart, INT zHellStart)
{
	m_xHellStart = xHellStart;
	m_zHellStart = zHellStart;
}

RSTRING CLevelGenerator::GetLevelName (VOID)
{
	return m_pLevelDef ? m_pLevelDef->m_rstrLevelNameW : NULL;
}

HRESULT CLevelGenerator::GenerateRegion (CDungeonRegion* pRegion, CLevelRenderer* pRenderer)
{
	HRESULT hr;
	CMersenneTwister mt;
	IJSONArray* pMusic;
	TArray<BLOCK_DATA*> aPriorityConnectors;

	CheckIf(0 == m_pLevelDef->m_cChunks || 0 == m_pLevelDef->m_nChunkMaxRandom, E_FAIL);

	RandomizeLevelRegion(&mt, pRegion->m_xRegion, pRegion->m_zRegion);

	ZeroMemory(m_nFloor, sizeof(m_nFloor));
	m_nCurrentFloor = 0;

	pRegion->m_pChunk = TPickRandomTheme<CChunkTheme>(mt, m_pLevelDef->m_nChunkMaxRandom, m_pLevelDef->m_pChunks, m_pLevelDef->m_cChunks);
	CheckIf(NULL == pRegion->m_pChunk, E_UNEXPECTED);

	if(pRegion->m_rstrMusic)
	{
		RStrRelease(pRegion->m_rstrMusic);
		pRegion->m_rstrMusic = NULL;
	}

	pMusic = pRegion->m_pChunk->m_pMusic;
	Check(pMusic->GetString(static_cast<sysint>(mt.Random(pMusic->Count())), &pRegion->m_rstrMusic));

	// Plot the edge connectors first.
	Check(PlotEdgeConnector(mt, pRegion, -1, false, 0, -1, SLP(L"UpConnector")));
	Check(PlotEdgeConnector(mt, pRegion, -1, true, -1, 0, SLP(L"LeftConnector")));
	Check(PlotEdgeConnector(mt, pRegion, REGION_WIDTH, false, 0, 1, SLP(L"DownConnector")));
	Check(PlotEdgeConnector(mt, pRegion, REGION_WIDTH, true, 1, 0, SLP(L"RightConnector")));

	if(-1 < m_nLevel)
	{
		// Next, plot the elevators (they won't collide with the edge connectors).
		if(0 < m_nLevel)
			Check(PlotElevators(mt, pRegion, -1));
		if(m_nLevel < INT_MAX)
			Check(PlotElevators(mt, pRegion, 1));
	}

	// If we're on the "first" level, we need to plot a start room (collisions are now possible).
	if(0 == m_nLevel && 0 == pRegion->m_xRegion && 0 == pRegion->m_zRegion)
		Check(PlotStartRoom(mt, pRegion));
	else if(-1 == m_nLevel && 0 == pRegion->m_xRegion && 0 == pRegion->m_zRegion)
		Check(PlotHellStart(mt, pRegion));
	else if(pRegion->m_pChunk->m_fSpear)
		Check(PlotSpearRoom(mt, pRegion));

	// Save a copy of the edge/elevator/start connectors.
	for(sysint i = 0; i < m_aConnectors.Length(); i++)
		Check(aPriorityConnectors.Append(m_aConnectors[i]));

	// With the edge connectors, elevators, and possibly a start room placed, now it's time to randomly place all the other rooms!
	PlotFabricatedRooms(mt, pRegion);

	// Ensure the "priority" connectors are connected to something.
	for(sysint i = 0; i < aPriorityConnectors.Length(); i++)
	{
		if(TYPE_CONNECTOR == aPriorityConnectors[i]->idxBlock)
			TraceTunnelFromConnector(pRegion, aPriorityConnectors[i]);
	}

	// Draw some tunnel connections between other rooms.
	DWORD cTunnels = mt.Random(5, 15);
	for(DWORD i = 0; i < cTunnels; i++)
		TraceTunnelFromConnector(pRegion, m_aConnectors[mt.Random(m_aConnectors.Length())]);

	FillEmptyBlocks(mt, pRegion);
	Check(SetupDoors(mt, pRegion, pRenderer));

Cleanup:
	m_aRoomConnections.Clear();
	m_aConnectors.Clear();
	return hr;
}

BOOL CLevelGenerator::CheckElevatorConnection (CDungeonRegion* pRegion, INT xBlock, INT zBlock, INT nLevelOffset)
{
	CMersenneTwister mtEdge;

	if(-1 == nLevelOffset && 0 == m_nLevel)
		return FALSE;
	if(1 == nLevelOffset && INT_MAX == m_nLevel)
		return FALSE;

	RandomizeRegionEdge(&mtEdge, pRegion->m_xRegion, m_nLevel, pRegion->m_zRegion,
		pRegion->m_xRegion, m_nLevel + nLevelOffset, pRegion->m_zRegion);

	INT cElevators = mtEdge.Random(1, 3);
	for(INT i = 0; i < cElevators; i++)
	{
		INT x, z;
		GetNextElevator(mtEdge, x, z);
		if(xBlock == x && zBlock == z)
			return TRUE;
	}

	return FALSE;
}

VOID CLevelGenerator::RandomizeLevelRegion (CMersenneTwister* pmt, INT xRegion, INT zRegion)
{
	CMd5 md5;
	BYTE bDigest[16];

	md5.AddData((PBYTE)&m_dwSeed, sizeof(m_dwSeed));
	md5.AddData((PBYTE)&m_nLevel, sizeof(m_nLevel));
	md5.AddData((PBYTE)&xRegion, sizeof(xRegion));
	md5.AddData((PBYTE)&zRegion, sizeof(zRegion));
	md5.GetDigest(bDigest);
	pmt->Randomize(bDigest, sizeof(bDigest));
}

VOID CLevelGenerator::RandomizeRegionEdge (CMersenneTwister* pmt, INT xRegionA, INT yLevelA, INT zRegionA, INT xRegionB, INT yLevelB, INT zRegionB)
{
	CMd5 md5;
	BYTE bDigest[16];

	md5.AddData((PBYTE)&m_dwSeed, sizeof(m_dwSeed));

	if(xRegionA < xRegionB || yLevelA < yLevelB || zRegionA < zRegionB)
	{
		AddRegionCoords(md5, xRegionA, yLevelA, zRegionA);
		AddRegionCoords(md5, xRegionB, yLevelB, zRegionB);
	}
	else
	{
		AddRegionCoords(md5, xRegionB, yLevelB, zRegionB);
		AddRegionCoords(md5, xRegionA, yLevelA, zRegionA);
	}

	md5.GetDigest(bDigest);
	pmt->Randomize(bDigest, sizeof(bDigest));
}

VOID CLevelGenerator::AddRegionCoords (CMd5& md5, INT xRegion, INT yLevel, INT zRegion)
{
	md5.AddData((PBYTE)&xRegion, sizeof(xRegion));
	md5.AddData((PBYTE)&yLevel, sizeof(xRegion));
	md5.AddData((PBYTE)&zRegion, sizeof(zRegion));
}

VOID CLevelGenerator::GetNextElevator (CMersenneTwister& mtEdge, __out INT& x, __out INT& z)
{
	static const INT nPadding = 8;
	static const INT nSpacing = 16;
	x = mtEdge.Random((REGION_WIDTH - nPadding * 2) / nSpacing) * nSpacing + nPadding;
	z = mtEdge.Random((REGION_WIDTH - nPadding * 2) / nSpacing) * nSpacing + nPadding;
}

HRESULT CLevelGenerator::CanPlaceRoom (CDungeonRegion* pRegion, CRoom* pRoom, INT xPlace, INT zPlace, INT xOffset, INT zOffset)
{
	sysint cBlocks = pRoom->m_aBlocks.Length();
	BLOCK_DATA* pbRegion = pRegion->m_bRegion;

	if(!pRoom->CheckBounds(xPlace, zPlace, xOffset, zOffset))
		return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);

	for(sysint i = 0; i < cBlocks; i++)
	{
		const ROOM_BLOCK& block = pRoom->m_aBlocks[i];
		sysint idxBlock = block.idxBlock;

		if(TYPE_ANCHOR != idxBlock)
		{
			INT x = xPlace + (block.x - xOffset);
			INT z = zPlace + (block.z - zOffset);

			Assert(x >= 0 && x < REGION_WIDTH);
			Assert(z >= 0 && z < REGION_WIDTH);

			BLOCK_DATA* pbTarget = pbRegion + REGION_WIDTH * z + x;
			sysint idxTarget = pbTarget->idxBlock;

			if(TYPE_EMPTY != idxTarget)
			{
				if(TYPE_ANY_FLOOR == idxTarget && TYPE_ANY_FLOOR == idxBlock)
				{
					// Floor tiles can overlap.
				}
				else if(TYPE_TUNNEL_START == idxTarget || TYPE_CONNECTOR == idxTarget)
				{
					// Tunnel and Connector tiles can be overwritten.
				}
				else
				{
					// The rooms are colliding!
					return HRESULT_FROM_WIN32(ERROR_ALREADY_ASSIGNED);
				}
			}
		}
	}

	return S_OK;
}

HRESULT CLevelGenerator::PlotEdgeConnector (CMersenneTwister& mt, CDungeonRegion* pRegion, INT nPlacement, bool fVertical, INT xRegionOffset, INT zRegionOffset, PCWSTR pcwzConnector, INT cchConnector)
{
	static const INT nPadding = 8;
	static const INT nSpacing = 8;
	HRESULT hr;
	RSTRING rstrConnectorW = NULL;
	CRoom* pConnector;
	CMersenneTwister mtEdge;

	Check(RStrCreateW(cchConnector, pcwzConnector, &rstrConnectorW));
	Check(m_pSpecial->Find(rstrConnectorW, &pConnector));
	CheckIfA(1 != pConnector->m_aAnchors.Length(), E_UNEXPECTED);

	ROOM_BLOCK& anchor = pConnector->m_aBlocks[pConnector->m_aAnchors[0]];

	RandomizeRegionEdge(&mtEdge, pRegion->m_xRegion, m_nLevel, pRegion->m_zRegion,
		pRegion->m_xRegion + xRegionOffset, m_nLevel, pRegion->m_zRegion + zRegionOffset);

	INT cConnectors = mtEdge.Random(1, 5);
	for(INT i = 0; i < cConnectors; i++)
	{
		INT nTry = mtEdge.Random((REGION_WIDTH - nPadding * 2) / nSpacing) * nSpacing + nPadding;
		if(fVertical)
			PlaceRoom(mt, pRegion, pConnector, pRegion->m_pChunk->m_pFill, nPlacement, nTry, anchor.x, anchor.z);
		else
			PlaceRoom(mt, pRegion, pConnector, pRegion->m_pChunk->m_pFill, nTry, nPlacement, anchor.x, anchor.z);
	}

Cleanup:
	RStrRelease(rstrConnectorW);
	return hr;
}

HRESULT CLevelGenerator::PlotElevators (CMersenneTwister& mt, CDungeonRegion* pRegion, INT nLevelOffset)
{
	static PCWSTR pcwzDir[4] = { L"Left", L"Up", L"Right", L"Down" };
	HRESULT hr = S_FALSE;
	CMersenneTwister mtEdge;
	RSTRING rstrElevatorW = NULL;

	RandomizeRegionEdge(&mtEdge, pRegion->m_xRegion, m_nLevel, pRegion->m_zRegion,
		pRegion->m_xRegion, m_nLevel + nLevelOffset, pRegion->m_zRegion);

	INT cElevators = mtEdge.Random(1, 3);
	for(INT i = 0; i < cElevators; i++)
	{
		INT x, z;
		GetNextElevator(mtEdge, x, z);

		CMersenneTwister mtDir(x * z);
		INT nDir = mtDir.Random(4);
		CRoom* pElevator;

		Check(RStrFormatW(&rstrElevatorW, L"%lsElevator", pcwzDir[nDir]));
		Check(m_pSpecial->Find(rstrElevatorW, &pElevator));
		CheckIfA(1 != pElevator->m_aAnchors.Length(), E_UNEXPECTED);

		ROOM_BLOCK& anchor = pElevator->m_aBlocks[pElevator->m_aAnchors[0]];
		PlaceRoom(mt, pRegion, pElevator, pRegion->m_pChunk->m_pFill, x, z, anchor.x, anchor.z);

		RStrRelease(rstrElevatorW); rstrElevatorW = NULL;
	}

Cleanup:
	RStrRelease(rstrElevatorW);
	return hr;
}

HRESULT CLevelGenerator::PlotStartRoom (CMersenneTwister& mt, CDungeonRegion* pRegion)
{
	static const INT nPadding = 16;
	HRESULT hr;
	RSTRING rstrStartW = NULL;
	CRoom* pStart;

	Check(RStrCreateW(LSP(L"Start"), &rstrStartW));
	Check(m_pSpecial->Find(rstrStartW, &pStart));
	CheckIfA(1 != pStart->m_aAnchors.Length(), E_UNEXPECTED);

	ROOM_BLOCK& anchor = pStart->m_aBlocks[pStart->m_aAnchors[0]];

	do
	{
		INT x = mt.Random((REGION_WIDTH - nPadding * 2)) + nPadding;
		INT z = mt.Random((REGION_WIDTH - nPadding * 2)) + nPadding;

		hr = PlaceRoom(mt, pRegion, pStart, pRegion->m_pChunk->m_pFill, x, z, anchor.x, anchor.z);
	} while(FAILED(hr));

Cleanup:
	RStrRelease(rstrStartW);
	return hr;
}

HRESULT CLevelGenerator::PlotHellStart (CMersenneTwister& mt, CDungeonRegion* pRegion)
{
	HRESULT hr;
	RSTRING rstrHellStartW = NULL;
	RSTRING rstrTypeW = NULL;
	CRoom* pHellStart;

	Check(RStrCreateW(LSP(L"HellStart"), &rstrHellStartW));
	Check(m_pSpecial->Find(rstrHellStartW, &pHellStart));

	for(sysint i = 0; i < pHellStart->m_aBlocks.Length(); i++)
	{
		ROOM_BLOCK& block = pHellStart->m_aBlocks[i];
		if(block.pEntity)
		{
			TStackRef<IJSONValue> srv;
			INT nResult;

			Check(block.pEntity->FindNonNullValueW(L"type", &srv));
			Check(srv->GetString(&rstrTypeW));
			Check(RStrCompareIW(rstrTypeW, L"iw.StartNorth", &nResult));
			if(0 == nResult)
			{
				CWallTheme* pWallTheme = TPickRandomTheme<CWallTheme>(mt, pRegion->m_pChunk->m_nRoomMaxRandom, pRegion->m_pChunk->m_pRooms, pRegion->m_pChunk->m_cRooms);
				Check(PlaceRoom(mt, pRegion, pHellStart, pWallTheme, m_xHellStart, m_zHellStart, block.x, block.z));
				break;
			}
			RStrRelease(rstrTypeW); rstrTypeW = NULL;
		}
	}

Cleanup:
	RStrRelease(rstrTypeW);
	RStrRelease(rstrHellStartW);
	return hr;
}

HRESULT CLevelGenerator::PlotSpearRoom (CMersenneTwister& mt, CDungeonRegion* pRegion)
{
	HRESULT hr;
	RSTRING rstrSpearW = NULL, rstrSpareW = NULL;
	CRoom* pSpear, *pSpare;

	Check(RStrCreateW(LSP(L"SpearRoom"), &rstrSpearW));
	Check(m_pSpecial->Find(rstrSpearW, &pSpear));
	Check(PlotFabricatedRoom(mt, pRegion, pSpear));

	Check(RStrCreateW(LSP(L"Spare Gold Key"), &rstrSpareW));
	Check(m_pRooms->Find(rstrSpareW, &pSpare));
	Check(PlotFabricatedRoom(mt, pRegion, pSpare));

Cleanup:
	RStrRelease(rstrSpareW);
	RStrRelease(rstrSpearW);
	return hr;
}

HRESULT CLevelGenerator::PlotFabricatedRooms (CMersenneTwister& mt, CDungeonRegion* pRegion)
{
	HRESULT hr;
	sysint cRooms = m_pRooms->Count();
	ULONG cDesired = mt.Random(40, 60);

	for(ULONG i = 0; i < cDesired; i++)
	{
		CRoom* pRoom = m_pRooms->GetRoom(mt.Random(cRooms));
		Check(PlotFabricatedRoom(mt, pRegion, pRoom));
	}

Cleanup:
	return hr;
}

HRESULT CLevelGenerator::PlotFabricatedRoom (CMersenneTwister& mt, CDungeonRegion* pRegion, CRoom* pRoom)
{
	HRESULT hr;
	sysint cConnectors = m_aConnectors.Length();
	sysint cRoomConnectors = pRoom->m_aConnectors.Length();
	CWallTheme* pWallTheme = TPickRandomTheme<CWallTheme>(mt, pRegion->m_pChunk->m_nRoomMaxRandom, pRegion->m_pChunk->m_pRooms, pRegion->m_pChunk->m_cRooms);
	CheckIf(NULL == pWallTheme, E_UNEXPECTED);

	sysint idxStartOuter = mt.Random(cConnectors);
	for(sysint n = 0; n < cConnectors; n++)
	{
		BLOCK_DATA* pBlock = m_aConnectors[(idxStartOuter + n) % cConnectors];
		INT nOffset = static_cast<INT>(pBlock - pRegion->m_bRegion);
		INT z = nOffset / REGION_WIDTH;
		INT x = nOffset % REGION_WIDTH;
		ROOM_CONN rc;

		rc.nFloorA = m_nFloor[z * REGION_WIDTH + x];

		Assert(TYPE_CONNECTOR == pRegion->m_bRegion[z * REGION_WIDTH + x].idxBlock);

		sysint idxStartInner = mt.Random(cRoomConnectors);
		for(sysint c = 0; c < cRoomConnectors; c++)
		{
			sysint idxConnector = pRoom->m_aConnectors[(idxStartInner + c) % cRoomConnectors];
			ROOM_BLOCK& block = pRoom->m_aBlocks[idxConnector];

			if(SUCCEEDED(PlaceRoom(mt, pRegion, pRoom, pWallTheme, x, z, block.x, block.z)))
			{
				rc.nFloorB = m_nCurrentFloor;
				Check(m_aRoomConnections.Append(rc));
				goto Cleanup;
			}
		}
	}

	hr = S_FALSE;

Cleanup:
	return hr;
}

HRESULT CLevelGenerator::TraceTunnelFromConnector (CDungeonRegion* pRegion, BLOCK_DATA* pConnector)
{
	HRESULT hr = S_FALSE;
	INT nOffset = static_cast<INT>(pConnector - pRegion->m_bRegion);
	INT z = nOffset / REGION_WIDTH;
	INT x = nOffset % REGION_WIDTH;

	for(INT nDir = 0; nDir < ARRAYSIZE(c_ptDir); nDir++)
	{
		if(S_OK == TraceTunnelDirNext(pRegion, m_nFloor[nOffset], nDir, x, z, 4))
			hr = S_OK;
	}

	if(S_OK == hr)
	{
		Check(m_aConnectors.Remove(pConnector));
		pConnector->idxBlock = TYPE_ANY_FLOOR;
	}
#ifdef	_DEBUG
	else
	{
		WCHAR wzSeed[128];
		wsprintf(wzSeed, L"Failed: Seed=%u, x=%d, z=%d, rx=%d, rz=%d\r\n", m_dwSeed, x, z, pRegion->m_xRegion, pRegion->m_zRegion);
		OutputDebugStringW(wzSeed);
	}
#endif

Cleanup:
	return hr;
}

HRESULT CLevelGenerator::TraceTunnelDirNext (CDungeonRegion* pRegion, INT nSourceFloor, INT nDir, INT x, INT z, INT cMaxTurns)
{
	HRESULT hr;
	INT xTrace = x + c_ptDir[nDir].x;
	INT zTrace = z + c_ptDir[nDir].y;
	BLOCK_DATA* pTrace = pRegion->GetBlockReduced(xTrace, zTrace);

	CheckIf(NULL == pTrace, S_FALSE);

	INT nOffset = static_cast<INT>(pTrace - pRegion->m_bRegion);
	if(TYPE_CONNECTOR == pTrace->idxBlock)
	{
		INT nTraceFloor = m_nFloor[nOffset];
		CheckIf(nSourceFloor == nTraceFloor, S_FALSE);
		CheckIf(AreRoomsConnected(nSourceFloor, nTraceFloor), S_FALSE);
		Check(m_aConnectors.Remove(pTrace));
	}
	else
	{
		CheckIf(TYPE_EMPTY != pTrace->idxBlock && (TYPE_ANY_FLOOR != pTrace->idxBlock && 0 != m_nFloor[nOffset]), S_FALSE);
		Check(TraceTunnelDir(pRegion, nSourceFloor, nDir, xTrace, zTrace, cMaxTurns));
	}

	if(S_OK == hr)
		pTrace->idxBlock = TYPE_ANY_FLOOR;

Cleanup:
	return hr;
}

HRESULT CLevelGenerator::TraceTunnelDir (CDungeonRegion* pRegion, INT nSourceFloor, INT nDir, INT x, INT z, INT cMaxTurns)
{
	HRESULT hr = TraceTunnelDirNext(pRegion, nSourceFloor, nDir, x, z, cMaxTurns);
	if(S_OK == hr)
		return S_OK;

	if(0 < cMaxTurns)
	{
		cMaxTurns--;

		hr = TraceTunnelDirNext(pRegion, nSourceFloor, (nDir + 1) % ARRAYSIZE(c_ptDir), x, z, cMaxTurns);
		if(S_OK == hr)
			return S_OK;
		hr = TraceTunnelDirNext(pRegion, nSourceFloor, (nDir + 3) % ARRAYSIZE(c_ptDir), x, z, cMaxTurns);
		if(S_OK == hr)
			return S_OK;
	}

	return S_FALSE;
}

BOOL CLevelGenerator::AreRoomsConnected (INT nFloorA, INT nFloorB)
{
	for(sysint i = 0; i < m_aRoomConnections.Length(); i++)
	{
		ROOM_CONN& conn = m_aRoomConnections[i];

		if(nFloorA == conn.nFloorA && nFloorB == conn.nFloorB || nFloorA == conn.nFloorB && nFloorB == conn.nFloorA)
			return TRUE;
	}

	return FALSE;
}

HRESULT CLevelGenerator::PlaceRoom (CMersenneTwister& mt, CDungeonRegion* pRegion, CRoom* pRoom, CWallTheme* pTheme, INT xPlace, INT zPlace, INT xOffset, INT zOffset)
{
	HRESULT hr;
	sysint cBlocks = pRoom->m_aBlocks.Length();
	BLOCK_DATA* pbRegion = pRegion->m_bRegion;
	RSTRING rstrTypeW = NULL;

	CheckNoTrace(CanPlaceRoom(pRegion, pRoom, xPlace, zPlace, xOffset, zOffset));

	m_nCurrentFloor++;

	for(sysint i = 0; i < cBlocks; i++)
	{
		const ROOM_BLOCK& block = pRoom->m_aBlocks[i];
		sysint idxBlock = block.idxBlock;

		if(TYPE_ANCHOR != idxBlock)
		{
			INT x = xPlace + (block.x - xOffset);
			INT z = zPlace + (block.z - zOffset);

			Assert(x >= 0 && x < REGION_WIDTH);
			Assert(z >= 0 && z < REGION_WIDTH);

			BLOCK_DATA* pbTarget = pbRegion + REGION_WIDTH * z + x;

			if(TYPE_TUNNEL_START == idxBlock || TYPE_CONNECTOR == idxBlock)
			{
				if(TYPE_TUNNEL_START == pbTarget->idxBlock || TYPE_CONNECTOR == pbTarget->idxBlock)
				{
					if(TYPE_CONNECTOR == pbTarget->idxBlock)
						Check(m_aConnectors.Remove(pbTarget));

					idxBlock = TYPE_ANY_FLOOR;
				}
				else if(TYPE_EMPTY != pbTarget->idxBlock)
					continue;
				else if(TYPE_CONNECTOR == idxBlock)
					Check(m_aConnectors.InsertSorted(pbTarget));
			}
			else if(TYPE_CONNECTOR == pbTarget->idxBlock)
				Check(m_aConnectors.Remove(pbTarget));

			pbTarget->idxBlock = idxBlock;

			switch(idxBlock)
			{
			case TYPE_ANY_WALL:
				idxBlock = pTheme->m_pidxWalls[mt.Random(pTheme->m_cWalls)];
				break;
			case TYPE_ANY_DECORATION:
				if(0 == pTheme->m_cDecorations)
					idxBlock = pTheme->m_pidxWalls[mt.Random(pTheme->m_cWalls)];
				else
					idxBlock = pTheme->m_pidxDecorations[mt.Random(pTheme->m_cDecorations)];
				break;
			case TYPE_ANY_RAILING:
				Check(pRegion->m_pChunk->m_pRailingTypes->GetString(mt.Random(pRegion->m_pChunk->m_pRailingTypes->Count()), &rstrTypeW));
				Check(m_pWalls->m_mapRailingDefs.Find(rstrTypeW, &idxBlock));
				RStrRelease(rstrTypeW); rstrTypeW = NULL;
				break;
			case TYPE_ANY_SWITCH:
				{
					ELEVATOR_DEF* pDef;
					Check(pRegion->m_pChunk->m_pSwitchTypes->GetString(mt.Random(pRegion->m_pChunk->m_pSwitchTypes->Count()), &rstrTypeW));
					Check(m_pWalls->m_mapElevatorDefs.FindPtr(rstrTypeW, &pDef));
					idxBlock = pDef->idxDown;
					RStrRelease(rstrTypeW); rstrTypeW = NULL;

					// The generated elevator positions are actually for the anchor blocks.
					// That's how elevator switches "line up" with the other floors.
					sysint idxAnchor = pRoom->m_aAnchors[0];
					const ROOM_BLOCK& anchor = pRoom->m_aBlocks[idxAnchor];

					Check(AddElevatorSwitch(pRegion, pbTarget,
						xPlace + (anchor.x - xOffset),
						zPlace + (anchor.z - zOffset), pDef->idxUp));
				}
				break;
			}

			if(TYPE_TUNNEL_START != idxBlock && TYPE_CONNECTOR != idxBlock)
			{
				for(INT n = 0; n < ARRAYSIZE(pbTarget->idxSides); n++)
					pbTarget->idxSides[n] = idxBlock;
			}

			if(block.pEntity)
				Check(AddEntity(pRegion, block.pEntity, x, z));

			m_nFloor[z * REGION_WIDTH + x] = m_nCurrentFloor;
		}
	}

Cleanup:
	RStrRelease(rstrTypeW);
	return hr;
}

HRESULT CLevelGenerator::AddEntity (CDungeonRegion* pRegion, IJSONObject* pEntity, INT x, INT z)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	RSTRING rstrTypeW = NULL;
	PCWSTR pcwzType;

	Check(pEntity->FindNonNullValueW(L"type", &srv));
	Check(srv->GetString(&rstrTypeW));
	pcwzType = RStrToWide(rstrTypeW);

	if(0 == TStrCmpAssert(pcwzType, L"iw.StartNorth"))
	{
		pRegion->m_xStartCell = x;
		pRegion->m_zStartCell = z;
	}
	else
	{
		CModel* pModel;
		CModelEntity* pItem;

		Check(m_pModels->Resolve(rstrTypeW, &pModel));
		if(pModel->m_fObstacle)
			pItem = __new CModelObstacle(pModel);
		else
			pItem = __new CModelItem(pModel);
		CheckAlloc(pItem);

		pRegion->m_bRegion[z * REGION_WIDTH + x].m_pEntities = pItem;
		pItem->m_dp.x = static_cast<DOUBLE>(pRegion->m_xRegion) * REGION_WIDTH + x + 0.5;
		pItem->m_dp.y = 0.0;
		pItem->m_dp.z = static_cast<DOUBLE>(pRegion->m_zRegion) * REGION_WIDTH + z + 0.5;

		Check(pRegion->m_aEntities.Append(pItem));
	}

Cleanup:
	RStrRelease(rstrTypeW);
	return hr;
}

VOID CLevelGenerator::FillEmptyBlocks (CMersenneTwister& mt, CDungeonRegion* pRegion)
{
	CWallTheme* pFill = pRegion->m_pChunk->m_pFill;
	BLOCK_DATA* pbCell = pRegion->m_bRegion;
	ULONG pctDecorations = static_cast<ULONG>(pRegion->m_pChunk->m_pctDecorations);
	for(INT z = 0; z < REGION_WIDTH; z++)
	{
		for(INT x = 0; x < REGION_WIDTH; x++)
		{
			if(TYPE_TUNNEL_START == pbCell->idxBlock || TYPE_CONNECTOR == pbCell->idxBlock || TYPE_ANCHOR == pbCell->idxBlock)
				pbCell->idxBlock = TYPE_EMPTY;

			if(TYPE_EMPTY == pbCell->idxBlock)
			{
				sysint idxBlock;
				if(mt.Random(100) < pctDecorations)
					idxBlock = pFill->m_pidxDecorations[mt.Random(pFill->m_cDecorations)];
				else
					idxBlock = pFill->m_pidxWalls[mt.Random(pFill->m_cWalls)];

				for(INT i = 0; i < ARRAYSIZE(pbCell->idxSides); i++)
					pbCell->idxSides[i] = idxBlock;
			}

			pbCell++;
		}
	}
}

HRESULT CLevelGenerator::AddElevatorSwitch (CDungeonRegion* pRegion, BLOCK_DATA* pBlock, INT x, INT z, sysint idxUp)
{
	HRESULT hr;
	CElevatorSwitch* pSwitch = __new CElevatorSwitch(pBlock, x, z, idxUp);

	CheckAlloc(pSwitch);
	pBlock->m_pEntities = pSwitch;
	Check(pRegion->m_aEntities.Append(pSwitch));

	pSwitch->m_dp.x = static_cast<DOUBLE>(pRegion->m_xRegion) * REGION_WIDTH + static_cast<DOUBLE>(x) + 0.5;
	pSwitch->m_dp.y = 0.0;
	pSwitch->m_dp.z = static_cast<DOUBLE>(pRegion->m_zRegion) * REGION_WIDTH + static_cast<DOUBLE>(z) + 0.5;

Cleanup:
	return hr;
}

HRESULT CLevelGenerator::SetupDoors (CMersenneTwister& mt, CDungeonRegion* pRegion, CLevelRenderer* pRenderer)
{
	HRESULT hr;
	BLOCK_DATA* pbCell = pRegion->m_bRegion;
	ULONG pctDecorations = static_cast<ULONG>(pRegion->m_pChunk->m_pctDecorations);
	IJSONArray* pDoorTypes = pRegion->m_pChunk->m_pDoorTypes;
	IJSONArray* pLockedTypes = pRegion->m_pChunk->m_pLockedTypes;
	IJSONArray* pElevatorTypes = pRegion->m_pChunk->m_pElevatorTypes;
	TStackRef<IJSONObject> srDoorType;
	TStackRef<IJSONValue> srv;
	RSTRING rstrDoorTypeW = NULL;
	RSTRING rstrSpecialDoorW = NULL;
	DOOR_DEF* pDoorDef;

	Check(pDoorTypes->GetString(mt.Random(pDoorTypes->Count()), &rstrDoorTypeW));
	Check(m_pWalls->m_mapDoorDefs.FindPtr(rstrDoorTypeW, &pDoorDef));

	for(INT z = 0; z < REGION_WIDTH; z++)
	{
		for(INT x = 0; x < REGION_WIDTH; x++)
		{
			switch(pbCell->idxBlock)
			{
			case TYPE_ANY_DOOR:
			case TYPE_ANY_ELEVATOR_DOOR:
			case TYPE_ANY_LOCKED_DOOR_GOLD:
			case TYPE_ANY_LOCKED_DOOR_SILVER:
				{
					BLOCK_DATA* pBlockA = pRegion->GetBlock(x, z - 1);
					BLOCK_DATA* pBlockB = pRegion->GetBlock(x, z + 1);
					INT nTrackA = -1, nTrackB = -1;
					bool fNorthSouth = true;

					if(pBlockA && pBlockB && TYPE_ANY_FLOOR != pBlockA->idxBlock && TYPE_ANY_FLOOR != pBlockB->idxBlock)
					{
						nTrackA = 3;
						nTrackB = 1;
					}
					else
					{
						pBlockA = pRegion->GetBlock(x - 1, z);
						pBlockB = pRegion->GetBlock(x + 1, z);

						if(pBlockA && pBlockB && TYPE_ANY_FLOOR != pBlockA->idxBlock && TYPE_ANY_FLOOR != pBlockB->idxBlock)
						{
							nTrackA = 2;
							nTrackB = 0;
							fNorthSouth = false;
						}
					}

					if(-1 != nTrackA && -1 != nTrackB)
					{
						DOOR_DEF* pApplied;

						switch(pbCell->idxBlock)
						{
						case TYPE_ANY_DOOR:
							pApplied = pDoorDef;
							break;
						case TYPE_ANY_ELEVATOR_DOOR:
							RStrRelease(rstrSpecialDoorW); rstrSpecialDoorW = NULL;
							Check(pElevatorTypes->GetString(mt.Random(pElevatorTypes->Count()), &rstrSpecialDoorW));
							Check(m_pWalls->m_mapDoorDefs.FindPtr(rstrSpecialDoorW, &pApplied));
							break;
						default:
							RStrRelease(rstrSpecialDoorW); rstrSpecialDoorW = NULL;
							Check(pLockedTypes->GetString(mt.Random(pLockedTypes->Count()), &rstrSpecialDoorW));
							Check(m_pWalls->m_mapDoorDefs.FindPtr(rstrSpecialDoorW, &pApplied));
							break;
						}

						pBlockA->idxSides[nTrackA] = pApplied->idxTrack;
						pBlockB->idxSides[nTrackB] = pApplied->idxTrack;

						CDoor* pDoor = __new CDoor(pRenderer, m_pWalls, fNorthSouth, pApplied->idxTexture, pbCell->idxBlock);
						CheckAlloc(pDoor);
						pbCell->m_pEntities = pDoor;

						pDoor->m_dp.x = static_cast<DOUBLE>(pRegion->m_xRegion) * REGION_WIDTH + x + 0.5;
						pDoor->m_dp.y = 0.0;
						pDoor->m_dp.z = static_cast<DOUBLE>(pRegion->m_zRegion) * REGION_WIDTH + z + 0.5;

						Check(pRegion->m_aEntities.Append(pDoor));
					}

					pbCell->idxBlock = TYPE_ANY_FLOOR;
					for(INT n = 0; n < ARRAYSIZE(pbCell->idxSides); n++)
						pbCell->idxSides[n] = TYPE_EMPTY;
				}
				break;
			}

			pbCell++;
		}
	}

Cleanup:
	RStrRelease(rstrSpecialDoorW);
	RStrRelease(rstrDoorTypeW);
	return hr;
}
