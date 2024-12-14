#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\ISeekableStream.h"
#include "Library\Util\Formatting.h"
#include "ConfigDlg.h"
#include "BlockMap.h"
#include "PaintItems.h"
#include "MapThing.h"
#include "MapLinedef.h"
#include "MapLine.h"
#include "MapBox.h"

CMapConvert::CMapConvert ()
{
	m_pProgress = NULL;
	m_lpList = NULL;
	m_lpVertices = NULL;
	m_lpSectors = NULL;
	m_lpThings = NULL;
	m_iNextSector = 0;
	m_lpAssign = NULL;

	lstrcpyA(m_szCeiling,"FLAT19");
	lstrcpyA(m_szFloor,"FLOOR7_1");

	m_iVertStrip = 0;
	m_iHorzStrip = 0;
	m_iNextPoly = 1;
	m_lpQueue = NULL;

	m_iLightLevel = 160;

	m_bUseAutoDoors = TRUE;
}

CMapConvert::~CMapConvert ()
{
	ResetConversion();
}

VOID CMapConvert::ResetConversion (VOID)
{
	LPLINE_LIST lpLine;
	LPSECTOR_LIST lpSector;
	LPVERTEX_LIST lpVertex;
	LPTHING_LIST lpThing;
	LPPOLY_QUEUE lpPoly;

	while(m_lpList)
	{
		lpLine = m_lpList;
		m_lpList = m_lpList->Next;
		__delete lpLine->lpLine;
		__delete lpLine;
	}
	while(m_lpSectors)
	{
		lpSector = m_lpSectors;
		m_lpSectors = m_lpSectors->Next;
		__delete lpSector;
	}
	while(m_lpVertices)
	{
		lpVertex = m_lpVertices;
		m_lpVertices = m_lpVertices->Next;
		__delete lpVertex;
	}
	while(m_lpThings)
	{
		lpThing = m_lpThings;
		m_lpThings = m_lpThings->Next;
		__delete lpThing->lpThing;
		__delete lpThing;
	}
	while(m_lpQueue)
	{
		lpPoly = m_lpQueue;
		m_lpQueue = m_lpQueue->Next;
		__delete lpPoly;
	}

	m_iNextSector = 0;

	m_iVertStrip = 0;
	m_iHorzStrip = 0;
	m_iNextPoly = 1;
}

HRESULT CMapConvert::RunConversion (IMapConvertProgress* pProgress, PCSTR pcszLevel, ISeekableStream* pFile, LPWOLFDATA lpMap, CConfigDlg* pdlgConfig)
{
	HRESULT hr;
	INT cSectorTable = lpMap->xSize * lpMap->zSize;

	m_pProgress = pProgress;

	CopyTexture(m_szFloor, pdlgConfig->m_wzFloorName);
	CopyTexture(m_szCeiling, pdlgConfig->m_wzCeilingName);

	m_lpSectorTable = __new USHORT[cSectorTable];
	CheckAlloc(m_lpSectorTable);
	ZeroMemory(m_lpSectorTable, sizeof(USHORT) * cSectorTable);

	Check(BuildLineStructure(lpMap));
	BuildSectors(lpMap);
	Check(BuildCages(lpMap));
	Check(BuildSecretDoors(lpMap));
	Check(EndSpotTriggers(lpMap));
	Check(BuildSkyLights(lpMap));

	MergeLines();

	BuildOutsideViews(lpMap);
	BuildPlayerStarts(lpMap);
	BuildThings(lpMap);
	BuildPolys(lpMap);
	FixSidedefs();
	BuildVertexList();

	CheckIf(NULL == m_lpList, E_FAIL);
	Check(BuildWadFile(pcszLevel, pFile, pdlgConfig));

Cleanup:
	SafeDeleteArray(m_lpSectorTable);
	m_pProgress = NULL;
	return hr;
}

SHORT CMapConvert::GetNewSector (VOID)
{
	return m_iNextSector++;
}

CMapLine* CMapConvert::FindLine (LPVERTEX lpvFrom, LPVERTEX lpvTo)
{
	LPLINE_LIST lpList = m_lpList;
	CMapLine* lpLine;
	while(lpList)
	{
		lpLine = lpList->lpLine;
		if(lpLine->m_vFrom.x == lpvFrom->x && lpLine->m_vFrom.y == lpvFrom->y)
		{
			if(lpLine->m_vTo.x == lpvTo->x && lpLine->m_vTo.y == lpvTo->y)
				return lpLine;
		}

		// TJL (AFADoomer)
		if(lpLine->m_vTo.x == lpvFrom->x && lpLine->m_vTo.y == lpvFrom->y)
		{
			if(lpLine->m_vFrom.x == lpvTo->x && lpLine->m_vFrom.y == lpvTo->y)
				return lpLine;
		}

		lpList = lpList->Next;
	}
	return NULL;
}

VOID CMapConvert::RemoveLine (CMapLine* pLine)
{
	LPLINE_LIST lpList = m_lpList, pPrev = NULL;

	while(lpList)
	{
		if(pLine == lpList->lpLine)
		{
			if(pPrev)
				pPrev->Next = lpList->Next;
			else
				m_lpList = lpList->Next;
			__delete lpList->lpLine;
			__delete lpList;
			break;
		}

		pPrev = lpList;
		lpList = lpList->Next;
	}
}

LPLINE_LIST CMapConvert::FindNext (LPVERTEX lpvFrom, LPLINE_LIST* lplpPrev)
{
	LPLINE_LIST lpList = m_lpList, lpPrev = NULL;
	LPVERTEX lpv;
	while(lpList)
	{
		lpv = &lpList->lpLine->m_vFrom;
		if(lpv->x == lpvFrom->x && lpv->y == lpvFrom->y)
			break;
		lpPrev = lpList;
		lpList = lpList->Next;
	}
	*lplpPrev = lpPrev;
	return lpList;
}

USHORT CMapConvert::FindVertex (LPVERTEX lpVertex)
{
	LPVERTEX_LIST lpList = m_lpVertices;
	USHORT v = 0;
	while(lpList)
	{
		if(lpList->Vertex.x == lpVertex->x && lpList->Vertex.y == lpVertex->y)
			return v;
		v++;
		lpList = lpList->Next;
	}
	return 65535;
}

USHORT CMapConvert::FindSector (WOLFDATA* pData, INT x, INT y)
{
	USHORT iSector = 65535;
	if(x >= 0 && x < pData->xSize && y >= 0 && y < pData->zSize)
		iSector = m_lpSectorTable[y * pData->xSize + x];
	return iSector;
}

SECTOR* CMapConvert::FindSector (USHORT iSector)
{
	SECTOR* lpSector = NULL;
	LPSECTOR_LIST lpList = m_lpSectors;
	INT i = 0;
	while(lpList)
	{
		if(i++ == iSector)
		{
			lpSector = &lpList->Sector;
			break;
		}
		lpList = lpList->Next;
	}
	return lpSector;
}

VOID CMapConvert::PositionLine (WOLFDATA* pData, CMapLine* lpLine, INT x, INT y, INT iDir)
{
	INT xHalf = (pData->xSize * 64) / 2;
	INT yHalf = (pData->zSize * 64) / 2;

	switch(iDir)
	{
	case LINE_LEFT:
		lpLine->m_vFrom.x = x * 64 - xHalf;
		lpLine->m_vFrom.y = yHalf - (y + 1) * 64;
		lpLine->m_vTo.x = lpLine->m_vFrom.x;
		lpLine->m_vTo.y = lpLine->m_vFrom.y + 64;
		break;
	case LINE_RIGHT:
		lpLine->m_vFrom.x = (x + 1) * 64 - xHalf;
		lpLine->m_vFrom.y = yHalf - y * 64;
		lpLine->m_vTo.x = lpLine->m_vFrom.x;
		lpLine->m_vTo.y = lpLine->m_vFrom.y - 64;
		break;
	case LINE_ABOVE:
		lpLine->m_vFrom.x = x * 64 - xHalf;
		lpLine->m_vFrom.y = yHalf - y * 64;
		lpLine->m_vTo.x = lpLine->m_vFrom.x + 64;
		lpLine->m_vTo.y = lpLine->m_vFrom.y;
		break;
	case LINE_BELOW:
		lpLine->m_vFrom.x = (x + 1) * 64 - xHalf;
		lpLine->m_vFrom.y = yHalf - (y + 1) * 64;
		lpLine->m_vTo.x = lpLine->m_vFrom.x - 64;
		lpLine->m_vTo.y = lpLine->m_vFrom.y;
		break;
	}
}

VOID CMapConvert::PositionThing (WOLFDATA* pData, CMapThing* lpThing, INT x, INT y)
{
	INT xHalf = (pData->xSize * 64) / 2;
	INT zHalf = (pData->zSize * 64) / 2;

	lpThing->m_x = (x * 64 - xHalf) + 32;
	lpThing->m_y = (zHalf - y * 64) - 32;
}

VOID CMapConvert::FlipLinedef (CMapLine* lpLine, BOOL bAll)
{
	VERTEX vTemp;
	CopyMemory(&vTemp,&lpLine->m_vFrom,sizeof(VERTEX));
	CopyMemory(&lpLine->m_vFrom,&lpLine->m_vTo,sizeof(VERTEX));
	CopyMemory(&lpLine->m_vTo,&vTemp,sizeof(VERTEX));
	if(bAll)
	{
		SIDEDEF sTemp;
		CopyMemory(&sTemp,&lpLine->m_sRight,sizeof(SIDEDEF));
		CopyMemory(&lpLine->m_sRight,&lpLine->m_sLeft,sizeof(SIDEDEF));
		CopyMemory(&lpLine->m_sLeft,&sTemp,sizeof(SIDEDEF));
	}
}

VOID CMapConvert::FillSector (LPSECTOR lpSector, SHORT yFloor)
{
	lpSector->Floor = yFloor;
	lpSector->Ceiling = yFloor + 64;
	lstrcpyA(lpSector->szFloor,m_szFloor);
	lstrcpyA(lpSector->szCeiling,m_szCeiling);
	lpSector->Light = m_iLightLevel;
	lpSector->Special = 0;
	lpSector->Tag = 0;
}

HRESULT CMapConvert::AddLine (CMapLine* lpLine)
{
	HRESULT hr;
	LPLINE_LIST lpNew = __new LINE_LIST;

	if(lpNew)
	{
		lpNew->lpLine = __new CMapLine(lpLine);
		if(lpNew->lpLine)
		{
			lpNew->Next = m_lpList;
			m_lpList = lpNew;
			m_pProgress->ReportAddLine(lpLine);
			hr = S_OK;
		}
		else
		{
			__delete lpNew;
			hr = E_OUTOFMEMORY;
		}
	}
	else
		hr = E_OUTOFMEMORY;

	return hr;
}

HRESULT CMapConvert::AddImpassible (LPWOLFDATA lpMap, CMapLine* lpLine, INT x, INT y)
{
	HRESULT hr;
	TStackRef<CPaintItem> srBlock, srObject;
	CHAR szTexture[12];
	CMapLine* pFind;

	Check(lpMap->pMap->GetCellData(x, y, &srBlock, &srObject));
	CheckIf(NULL == srBlock || MapCell::Floor == srBlock->GetType(), E_UNEXPECTED);

	lpLine->m_Flags = 1;			// Impassible

	if(MapCell::Elevator == srBlock->GetType())
	{
		CElevatorSwitch* pElevator = srBlock.StaticCast<CElevatorSwitch>();

		CheckIfGetLastError(0 == WideCharToMultiByte(CP_ACP, 0, pElevator->GetTexture()->pcwzName, -1, szTexture, ARRAYSIZE(szTexture), NULL, NULL));

		// TJL - Elevator Switch
		lpLine->m_Flags |= 1024;
		if(pElevator->m_fSecret)
			lpLine->m_Special = 244;
		else
			lpLine->m_Special = 243;
		lpLine->m_Args[1] = 0;
		lpLine->m_Args[2] = 0;
		lpLine->m_Args[3] = 0;
		lpLine->m_Args[4] = 0;
	}
	else
	{
		CTextureItem* pWall = srBlock.StaticCast<CTextureItem>();

		CheckIfGetLastError(0 == WideCharToMultiByte(CP_ACP, 0, pWall->GetTexture()->pcwzName, -1, szTexture, ARRAYSIZE(szTexture), NULL, NULL));
	}

	CopyMemory(lpLine->m_sRight.szMiddle, szTexture, sizeof(lpLine->m_sRight.szMiddle));
	lstrcpyA(lpLine->m_sRight.szLower,"-");
	lstrcpyA(lpLine->m_sRight.szUpper,"-");
	lpLine->m_sRight.Sector = -1;	// Sorted Later

	pFind = FindLine(&lpLine->m_vFrom, &lpLine->m_vTo);
	if(pFind)
	{
		if(0 != memcmp(&lpLine->m_vTo, &pFind->m_vTo, sizeof(VERTEX)))
		{
			lstrcpyA(lpLine->m_sLeft.szMiddle,"-");
			lpLine->m_Flags |= 4;
			lpLine->m_Flags &= ~1;
		}
	}
	else
		Check(AddLine(lpLine));

Cleanup:
	return hr;
}

VOID CMapConvert::AddSidedLine (CMapLine* lpLine, USHORT iSector, PCSTR pcszTexture)
{
	CMapLine* lpFind = FindLine(&lpLine->m_vTo,&lpLine->m_vFrom);
	if(lpFind)
	{
		// Only make the line double-sided if we're working with an existing
		// line whose orientation is aligned with the line we're trying to add.
		if(lpFind->m_vTo == lpLine->m_vTo && lpFind->m_vFrom == lpLine->m_vFrom)
		{
			lpFind->m_Flags = 4;		// Two Sided
			lstrcpyA(lpLine->m_sRight.szMiddle,"-");
			CopyMemory(&lpFind->m_sLeft,&lpLine->m_sRight,sizeof(SIDEDEF));
			lstrcpyA(lpFind->m_sRight.szMiddle,"-");
			lstrcpyA(lpFind->m_sLeft.szLower,"-");
			lstrcpyA(lpFind->m_sLeft.szUpper,"-");
		}
		else
			lpFind->m_sRight.Sector = lpLine->m_sLeft.Sector;
	}
	else
	{
		lpLine->m_sRight.Sector = iSector;
		lpLine->m_Flags = 1;		// Impassible
		lstrcpyA(lpLine->m_sRight.szMiddle,pcszTexture);
		AddLine(lpLine);
	}
}

SHORT CMapConvert::AddSector (LPSECTOR lpSector)
{
	LPSECTOR_LIST lpNew = new SECTOR_LIST;
	CopyMemory(&lpNew->Sector,lpSector,sizeof(SECTOR));
	lpNew->Next = NULL;
	if(m_lpSectors)
	{
		LPSECTOR_LIST lpList = m_lpSectors;
		while(lpList->Next)
			lpList = lpList->Next;
		lpList->Next = lpNew;
	}
	else
		m_lpSectors = lpNew;
	return GetNewSector();
}

VOID CMapConvert::AddThing (CMapThing* lpThing)
{
	LPTHING_LIST lpNew = new THING_LIST;
	lpNew->lpThing = new CMapThing(lpThing);
	lpNew->Next = NULL;
	if(m_lpThings)
	{
		LPTHING_LIST lpList = m_lpThings;
		while(lpList->Next)
			lpList = lpList->Next;
		lpList->Next = lpNew;
	}
	else
		m_lpThings = lpNew;

	m_pProgress->ReportAddThing(lpThing);
}

VOID CMapConvert::CopyTexture (PSTR pszTarget, PCWSTR pcwzTexture)
{
	INT cch = WideCharToMultiByte(CP_ACP, 0, pcwzTexture, TStrLenAssert(pcwzTexture), pszTarget, 8, NULL, NULL);
	if(cch < 8)
		ZeroMemory(pszTarget + cch, 8 - cch);
}

VOID CMapConvert::RotatePoints (VERTEX* prgvPoints, INT cPoints)
{
	for(INT i = 0; i < cPoints; i++)
	{
		VERTEX* pv = prgvPoints + i;
		SwapData(pv->x, pv->y);
		pv->y = -pv->y;
	}
}

VOID CMapConvert::DrawBox (INT x1, INT y1, INT x2, INT y2, SHORT nSector, BOOL fOuterFacing)
{
	CMapBox box;

	PrepareBox(&box, x1, y1, x2, y2, nSector, fOuterFacing);
	CommitBox(&box);
}

VOID CMapConvert::PrepareBox (CMapBox* pBox, INT x1, INT y1, INT x2, INT y2, SHORT nSector, BOOL fOuterFacing)
{
	pBox->m_Box[0].m_vFrom.x = x1;
	pBox->m_Box[0].m_vFrom.y = y1;
	pBox->m_Box[0].m_vTo.x = x1;
	pBox->m_Box[0].m_vTo.y = y2;

	pBox->m_Box[1].m_vFrom.x = pBox->m_Box[0].m_vTo.x;
	pBox->m_Box[1].m_vFrom.y = pBox->m_Box[0].m_vTo.y;
	pBox->m_Box[1].m_vTo.x = x2;
	pBox->m_Box[1].m_vTo.y = y2;

	pBox->m_Box[2].m_vFrom.x = pBox->m_Box[1].m_vTo.x;
	pBox->m_Box[2].m_vFrom.y = pBox->m_Box[1].m_vTo.y;
	pBox->m_Box[2].m_vTo.x = x2;
	pBox->m_Box[2].m_vTo.y = y1;

	pBox->m_Box[3].m_vFrom.x = pBox->m_Box[2].m_vTo.x;
	pBox->m_Box[3].m_vFrom.y = pBox->m_Box[2].m_vTo.y;
	pBox->m_Box[3].m_vTo.x = pBox->m_Box[0].m_vFrom.x;
	pBox->m_Box[3].m_vTo.y = pBox->m_Box[0].m_vFrom.y;

	for(INT i = 0; i < ARRAYSIZE(pBox->m_Box); i++)
	{
		lstrcpyA(pBox->m_Box[i].m_sRight.szLower, "-");
		lstrcpyA(pBox->m_Box[i].m_sRight.szMiddle, "-");
		lstrcpyA(pBox->m_Box[i].m_sRight.szUpper, "-");

		lstrcpyA(pBox->m_Box[i].m_sLeft.szLower, "-");
		lstrcpyA(pBox->m_Box[i].m_sLeft.szMiddle, "-");
		lstrcpyA(pBox->m_Box[i].m_sLeft.szUpper, "-");

		pBox->m_Box[i].m_Flags = 1;
		pBox->m_Box[i].m_sRight.Sector = nSector;
		pBox->m_Box[i].m_sLeft.Sector = -1;

		if(fOuterFacing)
			FlipLinedef(pBox->m_Box + i, FALSE);
	}
}

VOID CMapConvert::CommitBox (CMapBox* pBox)
{
	for(INT i = 0; i < ARRAYSIZE(pBox->m_Box); i++)
		AddLine(pBox->m_Box + i);
}

HRESULT CMapConvert::BuildLineStructure (LPWOLFDATA lpMap)
{
	HRESULT hr;
	CBlockMap* pBlockMap = lpMap->pMap;
	CMapLine Line;

	for(INT y = 0; y < lpMap->zSize; y++)
	{
		for(INT x = 0; x < lpMap->xSize; x++)
		{
			TStackRef<CPaintItem> srBlock, srObject;

			SideAssertHr(pBlockMap->GetCellData(x, y, &srBlock, &srObject));

			if(srBlock && (MapCell::Floor == srBlock->GetType() || (srObject && srObject->GetType() == MapCell::SecretDoor)))
			{
				if(srObject && MapCell::Door == srObject->GetType())
				{
					CDoorObject* pDoor = srObject.StaticCast<CDoorObject>();
					INT iType, bDir, bSilence;

					switch(pDoor->m_eType)
					{
					case CDoorObject::Normal:
						iType = DOOR_UNLOCKED;
						break;
					case CDoorObject::SilverKey:
						iType = DOOR_SILVER;
						break;
					case CDoorObject::GoldKey:
						iType = DOOR_GOLD;
						break;
					case CDoorObject::RubyKey:
						iType = DOOR_RUBY;
						break;
					case CDoorObject::Elevator:
						iType = DOOR_ELEVATOR;
						break;
					}

					bSilence = 0;
					if(pBlockMap->IsSolid(x - 1, y) && pBlockMap->IsSolid(x + 1, y))
					{
						bDir = 1;	// North/South
						if(pBlockMap->GetFloor(x, y - 1) != pBlockMap->GetFloor(x, y + 1))
							bSilence = 1;

						if(pBlockMap->IsSolid(x, y - 1))		// Above
						{
							Line.Reset();
							PositionLine(lpMap, &Line,x,y,LINE_ABOVE);
							AddImpassible(lpMap, &Line, x, y - 1);
						}

						if(pBlockMap->IsSolid(x, y + 1))		// Below
						{
							Line.Reset();
							PositionLine(lpMap, &Line,x,y,LINE_BELOW);
							AddImpassible(lpMap, &Line, x, y + 1);
						}
					}
					else if(pBlockMap->IsSolid(x, y - 1) && pBlockMap->IsSolid(x, y + 1))
					{
						bDir = 0;	// East/West
						if(pBlockMap->GetFloor(x - 1, y) != pBlockMap->GetFloor(x + 1, y))
							bSilence = 1;

						if(pBlockMap->IsSolid(x - 1, y))		// Left
						{
							Line.Reset();
							PositionLine(lpMap, &Line,x,y,LINE_LEFT);
							AddImpassible(lpMap, &Line, x - 1, y);
						}

						if(pBlockMap->IsSolid(x + 1, y))		// Right
						{
							Line.Reset();
							PositionLine(lpMap, &Line,x,y,LINE_RIGHT);
							AddImpassible(lpMap, &Line, x + 1, y);
						}
					}
					else
						Check(E_FAIL);

					BuildDoor(lpMap, x, y, iType, bDir, bSilence, pDoor);
				}
				else
				{
					if(pBlockMap->IsSolid(x - 1, y))		// Left
					{
						Line.Reset();
						PositionLine(lpMap, &Line,x,y,LINE_LEFT);
						AddImpassible(lpMap, &Line, x - 1, y);
					}

					if(pBlockMap->IsSolid(x + 1, y))		// Right
					{
						Line.Reset();
						PositionLine(lpMap, &Line,x,y,LINE_RIGHT);
						AddImpassible(lpMap, &Line, x + 1, y);
					}

					if(pBlockMap->IsSolid(x, y - 1))		// Above
					{
						Line.Reset();
						PositionLine(lpMap, &Line,x,y,LINE_ABOVE);
						AddImpassible(lpMap, &Line, x, y - 1);
					}

					if(pBlockMap->IsSolid(x, y + 1))		// Below
					{
						Line.Reset();
						PositionLine(lpMap, &Line,x,y,LINE_BELOW);
						AddImpassible(lpMap, &Line, x, y + 1);
					}
				}
			}
		}
	}

	hr = S_OK;

Cleanup:
	return hr;
}

HRESULT CMapConvert::BuildCages (LPWOLFDATA lpMap)
{
	HRESULT hr = S_FALSE;
	CBlockMap* pBlockMap = lpMap->pMap;
	CMapLine Line;

	for(INT y = 0; y < lpMap->zSize; y++)
	{
		for(INT x = 0; x < lpMap->xSize; x++)
		{
			TStackRef<CPaintItem> srBlock, srObject;

			SideAssertHr(pBlockMap->GetCellData(x, y, &srBlock, &srObject));

			if(srBlock && MapCell::WallCage == srBlock->GetType())
			{
				INT nEntranceLine = 0;
				CMapLine* pEntranceLine = NULL;

				for(INT nLine = LINE_LEFT; nLine <= LINE_BELOW; nLine++)
				{
					CMapLine* pLine;

					Line.Reset();
					PositionLine(lpMap, &Line, x, y, nLine);

					pLine = FindLine(&Line.m_vFrom, &Line.m_vTo);
					if(pLine)
					{
						// Make sure there is only one line for this block!
						CheckIf(0 != nEntranceLine, E_FAIL);
						nEntranceLine = nLine;
						pEntranceLine = pLine;
					}
				}

				if(0 != nEntranceLine)
					Check(BuildCage(lpMap, x, y, nEntranceLine, pEntranceLine, srBlock.StaticCast<CWallCage>()));
			}
		}
	}

Cleanup:
	return hr;
}

HRESULT CMapConvert::BuildSecretDoors (LPWOLFDATA lpMap)
{
	HRESULT hr = S_FALSE;
	CBlockMap* pBlockMap = lpMap->pMap;
	CMapLine Line;

	for(INT y = 0; y < lpMap->zSize; y++)
	{
		for(INT x = 0; x < lpMap->xSize; x++)
		{
			TStackRef<CPaintItem> srBlock, srObject;

			SideAssertHr(pBlockMap->GetCellData(x, y, &srBlock, &srObject));

			if(srBlock && srObject && MapCell::SecretDoor == srObject->GetType())
			{
				SECTOR Sector;
				SHORT nSector;
				CMapLine* rgLines[4];
				BYTE bDir = srObject.StaticCast<CSecretDoor>()->GetDirection();

				FillSector(&Sector, lpMap->yFloor);
				Sector.Special |= 1024;
				nSector = AddSector(&Sector);

				for(INT nLine = LINE_LEFT; nLine <= LINE_BELOW; nLine++)
				{
					CMapLine* pLine;

					Line.Reset();
					PositionLine(lpMap, &Line, x, y, nLine);

					pLine = FindLine(&Line.m_vFrom, &Line.m_vTo);
					CheckIf(NULL == pLine, E_FAIL);
					rgLines[nLine - 1] = pLine;

					if(pLine->m_sRight.Sector == -1)
						pLine->m_sRight.Sector = nSector;
					else
					{
						pLine->m_Flags |= 4;	// Add double-sided
						pLine->m_Flags &= ~1;	// Remove impassible
						pLine->m_sLeft.Sector = nSector;
						CopyTexture(pLine->m_sRight.szMiddle, L"-");
						CopyTexture(pLine->m_sLeft.szUpper, L"-");
						CopyTexture(pLine->m_sLeft.szMiddle, L"-");
						CopyTexture(pLine->m_sLeft.szLower, L"-");
					}
				}

				{
					CMapThing Secret;
					LPPOLY_QUEUE lpNew = new POLY_QUEUE;

					if(bDir)	// North/South
					{
						rgLines[LINE_ABOVE - 1]->GetMapLine(&lpNew->Door1);
						rgLines[LINE_BELOW - 1]->GetMapLine(&lpNew->Door2);
					}
					else		// East/West
					{
						rgLines[LINE_LEFT - 1]->GetMapLine(&lpNew->Door1);
						rgLines[LINE_RIGHT - 1]->GetMapLine(&lpNew->Door2);
					}

					lpNew->iType = DOOR_SECRET;
					lpNew->bDir = bDir;
					lpNew->iPoly = m_iNextPoly++;;
					lpNew->pTexture = srBlock.StaticCast<CTextureItem>()->GetTexture();
					lpNew->Next = NULL;

					if(m_lpQueue)
					{
						LPPOLY_QUEUE lpList = m_lpQueue;
						while(lpList->Next)
							lpList = lpList->Next;
						lpList->Next = lpNew;
					}
					else
						m_lpQueue = lpNew;

					PositionThing(lpMap, &Secret, x, y);
					Secret.m_Type = 9046;
					AddThing(&Secret);
				}
			}
		}
	}

Cleanup:
	return hr;
}

HRESULT CMapConvert::EndSpotTriggers (LPWOLFDATA lpMap)
{
	HRESULT hr = S_FALSE;
	CBlockMap* pBlockMap = lpMap->pMap;

	BOOL* pfEndMap = __new BOOL[lpMap->zSize * lpMap->xSize];
	CheckAlloc(pfEndMap);

	for(INT y = 0; y < lpMap->zSize; y++)
	{
		for(INT x = 0; x < lpMap->xSize; x++)
		{
			TStackRef<CPaintItem> srBlock, srObject;

			SideAssertHr(pBlockMap->GetCellData(x, y, &srBlock, &srObject));

			pfEndMap[y * lpMap->xSize + x] = srObject && MapCell::End == srObject->GetType();
		}
	}

	for(INT y = 0; y < lpMap->zSize; y++)
	{
		for(INT x = 0; x < lpMap->xSize; x++)
		{
			if(pfEndMap[y * lpMap->xSize + x])
			{
				SECTOR Sector;
				SHORT nSector;

				FillSector(&Sector, lpMap->yFloor);
				nSector = AddSector(&Sector);
				Check(AssignEndSpotTrigger(lpMap, pfEndMap, x, y, nSector));
			}
		}
	}

Cleanup:
	__delete_array pfEndMap;
	return hr;
}

HRESULT CMapConvert::BuildSkyLights (LPWOLFDATA lpMap)
{
	HRESULT hr = S_FALSE;
	CBlockMap* pBlockMap = lpMap->pMap;

	BOOL* pfSkyMap = __new BOOL[lpMap->zSize * lpMap->xSize];
	CheckAlloc(pfSkyMap);

	for(INT y = 0; y < lpMap->zSize; y++)
	{
		for(INT x = 0; x < lpMap->xSize; x++)
		{
			TStackRef<CPaintItem> srBlock, srObject;

			SideAssertHr(pBlockMap->GetCellData(x, y, &srBlock, &srObject));

			pfSkyMap[y * lpMap->xSize + x] = srObject && MapCell::Sky == srObject->GetType();
		}
	}

	for(INT y = 0; y < lpMap->zSize; y++)
	{
		for(INT x = 0; x < lpMap->xSize; x++)
		{
			if(pfSkyMap[y * lpMap->xSize + x])
			{
				SECTOR Sector;
				SHORT nSector;

				FillSector(&Sector, lpMap->yFloor);
				lstrcpyA(Sector.szCeiling, "F_SKY1");
				Sector.Ceiling += 16;
				Sector.Light += 24;
				nSector = AddSector(&Sector);
				Check(AssignSkyLightTrigger(lpMap, pfSkyMap, x, y, nSector));
			}
		}
	}

Cleanup:
	__delete_array pfSkyMap;
	return hr;
}

HRESULT CMapConvert::AssignEndSpotTrigger (LPWOLFDATA lpMap, BOOL* pfEndMap, INT x, INT y, SHORT nSector)
{
	HRESULT hr;
	CBlockMap* pBlockMap = lpMap->pMap;

	CheckIfIgnore(x < 0 || y < 0 || x >= lpMap->xSize || y >= lpMap->zSize, S_FALSE);
	CheckIfIgnore(!pfEndMap[y * lpMap->xSize + x], S_FALSE);

	pfEndMap[y * lpMap->xSize + x] = FALSE;

	for(INT iDir = LINE_LEFT; iDir <= LINE_BELOW; iDir++)
	{
		INT xAdj = x, yAdj = y;
		TStackRef<CPaintItem> srAdjBlock, srAdjObject;

		switch(iDir)
		{
		case LINE_LEFT:
			xAdj--;
			break;
		case LINE_RIGHT:
			xAdj++;
			break;
		case LINE_ABOVE:
			yAdj--;
			break;
		case LINE_BELOW:
			yAdj++;
			break;
		}

		Check(pBlockMap->GetCellData(xAdj, yAdj, &srAdjBlock, &srAdjObject));

		if(NULL == srAdjObject || MapCell::End != srAdjObject->GetType())
		{
			CMapLine Line;
			PositionLine(lpMap, &Line, x, y, iDir);

			CMapLine* pLine = FindLine(&Line.m_vFrom, &Line.m_vTo);
			if(pLine)
			{
				// If it's a double-sided line, then the map is built wrong.
				CheckIf(pLine->m_Flags & 4, E_FAIL);

				pLine->m_sRight.Sector = nSector;
			}
			else
			{
				Line.m_Flags = 4;
				Line.m_sRight.Sector = m_lpSectorTable[y * lpMap->xSize + x];
				Line.m_sLeft.Sector = nSector;
				Line.m_Special = 75;

				lstrcpyA(Line.m_sRight.szUpper,"-");
				lstrcpyA(Line.m_sRight.szMiddle,"-");
				lstrcpyA(Line.m_sRight.szLower,"-");
				lstrcpyA(Line.m_sLeft.szUpper,"-");
				lstrcpyA(Line.m_sLeft.szMiddle,"-");
				lstrcpyA(Line.m_sLeft.szLower,"-");

				FlipLinedef(&Line, FALSE);
				Check(AddLine(&Line));
			}
		}
	}

	Check(AssignEndSpotTrigger(lpMap, pfEndMap, x + 1, y, nSector));
	Check(AssignEndSpotTrigger(lpMap, pfEndMap, x, y + 1, nSector));
	Check(AssignEndSpotTrigger(lpMap, pfEndMap, x - 1, y, nSector));
	Check(AssignEndSpotTrigger(lpMap, pfEndMap, x, y - 1, nSector));

Cleanup:
	return hr;
}

HRESULT CMapConvert::AssignSkyLightTrigger (LPWOLFDATA lpMap, BOOL* pfSkyMap, INT x, INT y, SHORT nSector)
{
	HRESULT hr;
	CBlockMap* pBlockMap = lpMap->pMap;

	CheckIfIgnore(x < 0 || y < 0 || x >= lpMap->xSize || y >= lpMap->zSize, S_FALSE);
	CheckIfIgnore(!pfSkyMap[y * lpMap->xSize + x], S_FALSE);

	pfSkyMap[y * lpMap->xSize + x] = FALSE;

	for(INT iDir = LINE_LEFT; iDir <= LINE_BELOW; iDir++)
	{
		INT xAdj = x, yAdj = y;
		TStackRef<CPaintItem> srAdjBlock, srAdjObject;

		switch(iDir)
		{
		case LINE_LEFT:
			xAdj--;
			break;
		case LINE_RIGHT:
			xAdj++;
			break;
		case LINE_ABOVE:
			yAdj--;
			break;
		case LINE_BELOW:
			yAdj++;
			break;
		}

		Check(pBlockMap->GetCellData(xAdj, yAdj, &srAdjBlock, &srAdjObject));

		if(NULL == srAdjObject || MapCell::Sky != srAdjObject->GetType())
		{
			CMapLine Line;
			PositionLine(lpMap, &Line, x, y, iDir);

			CMapLine* pLine = FindLine(&Line.m_vFrom, &Line.m_vTo);
			if(pLine)
			{
				// If it's a double-sided line, then the map is built wrong.
				CheckIf(pLine->m_Flags & 4, E_FAIL);

				pLine->m_sRight.Sector = nSector;
				pLine->m_sRight.yOffset = -16;
			}
			else
			{
				Line.m_Flags = 4;
				Line.m_sRight.Sector = m_lpSectorTable[y * lpMap->xSize + x];
				Line.m_sLeft.Sector = nSector;

				lstrcpyA(Line.m_sRight.szUpper,"-");
				lstrcpyA(Line.m_sRight.szMiddle,"-");
				lstrcpyA(Line.m_sRight.szLower,"-");
				CopyTexture(Line.m_sLeft.szUpper,L"LTGRAYB0");
				lstrcpyA(Line.m_sLeft.szMiddle,"-");
				lstrcpyA(Line.m_sLeft.szLower,"-");

				FlipLinedef(&Line, FALSE);
				Check(AddLine(&Line));
			}
		}
	}

	Check(AssignSkyLightTrigger(lpMap, pfSkyMap, x + 1, y, nSector));
	Check(AssignSkyLightTrigger(lpMap, pfSkyMap, x, y + 1, nSector));
	Check(AssignSkyLightTrigger(lpMap, pfSkyMap, x - 1, y, nSector));
	Check(AssignSkyLightTrigger(lpMap, pfSkyMap, x, y - 1, nSector));

Cleanup:
	return hr;
}

VOID CMapConvert::BuildDoor (LPWOLFDATA lpMap, INT x, INT y, INT iType, INT bDir, INT bSilence, CDoorObject* pDoor)
{
	CMapLine Ent1, Ent2, Track1, Track2, Door1, Door2, Door3, Door4;
	CMapLine Split1, Split2, Split3, Split4, Split5;
	CMapLine Socket1, Socket2;	// TJL - Added
	CMapLine* lpList[12];		// TJL - Increased for Socket1 and Socket2
	SECTOR Sector;
	INT i;
	SHORT nSector;

	lpList[0] = &Ent1;
	lpList[1] = &Ent2;
	lpList[2] = &Track1;
	lpList[3] = &Track2;
	lpList[4] = &Door1;
	lpList[5] = &Door2;
	lpList[6] = &Split1;
	lpList[7] = &Split2;
	lpList[8] = &Split3;
	lpList[9] = &Split4;
	// TJL - Added
	lpList[10] = &Socket1;
	lpList[11] = &Socket2;
	for(i = 0; i < 12; i++)
	{
		lpList[i]->Reset();
		lstrcpyA(lpList[i]->m_sRight.szUpper,"-");
		lstrcpyA(lpList[i]->m_sRight.szMiddle,"-");
		lstrcpyA(lpList[i]->m_sRight.szLower,"-");
		CopyMemory(&lpList[i]->m_sLeft,&lpList[i]->m_sRight,sizeof(SIDEDEF));
	}
	if(bDir == 0)
	{
		PositionLine(lpMap, &Ent1,x,y,LINE_LEFT);
		PositionLine(lpMap, &Ent2,x,y,LINE_RIGHT);
		PositionLine(lpMap, &Track1,x,y,LINE_ABOVE);
		PositionLine(lpMap, &Track2,x,y,LINE_BELOW);
		Door1 = Ent1;
		Door2 = Ent2;
		Door1.m_vFrom.x += 28;
		Door1.m_vTo.x += 28;
		Door2.m_vFrom.x -= 28;
		Door2.m_vTo.x -= 28;
		Split1 = Track1;
		Split2 = Track1;
		Split3 = Track2;
		Split4 = Track2;

		// TJL
		Socket1 = Door1;
		Socket2 = Door2;
		Socket1.m_vFrom.y -= 63;
		Socket1.m_vTo.y -= 64;
		Socket2.m_vFrom.y -= 64;
		Socket2.m_vTo.y -= 63;

		Split1.m_vTo.x = Split1.m_vFrom.x + 28;
		Split2.m_vFrom.x = Split2.m_vTo.x - 28;
		Split3.m_vTo.x = Split3.m_vFrom.x - 28;
		Split4.m_vFrom.x = Split4.m_vTo.x + 28;
		Track1.m_vFrom.x = Split1.m_vTo.x;
		Track1.m_vTo.x = Split2.m_vFrom.x;
		Track2.m_vFrom.x = Split3.m_vTo.x;
		Track2.m_vTo.x = Split4.m_vFrom.x;

		// TJL
		Track2.m_vFrom.y -= 63;
		Track2.m_vTo.y -= 63;
	}
	else
	{
		PositionLine(lpMap, &Ent1,x,y,LINE_ABOVE);
		PositionLine(lpMap, &Ent2,x,y,LINE_BELOW);
		PositionLine(lpMap, &Track1,x,y,LINE_LEFT);
		PositionLine(lpMap, &Track2,x,y,LINE_RIGHT);
		Door1 = Ent1;
		Door2 = Ent2;
		Door1.m_vFrom.y -= 28;
		Door1.m_vTo.y -= 28;
		Door2.m_vFrom.y += 28;
		Door2.m_vTo.y += 28;
		Split1 = Track1;
		Split2 = Track1;
		Split3 = Track2;
		Split4 = Track2;

		// TJL
		Socket1 = Door1;
		Socket2 = Door2;
		Socket1.m_vFrom.x -= 63;
		Socket1.m_vTo.x -= 64;
		Socket2.m_vFrom.x -= 64;
		Socket2.m_vTo.x -= 63;

		Split1.m_vTo.y = Split1.m_vFrom.y + 28;
		Split2.m_vFrom.y = Split2.m_vTo.y - 28;
		Split3.m_vTo.y = Split3.m_vFrom.y - 28;
		Split4.m_vFrom.y = Split4.m_vTo.y + 28;
		Track1.m_vFrom.y = Split1.m_vTo.y;
		Track1.m_vTo.y = Split2.m_vFrom.y;
		Track2.m_vFrom.y = Split3.m_vTo.y;
		Track2.m_vTo.y = Split4.m_vFrom.y;

		// TJL
		Track1.m_vFrom.x -= 63;
		Track1.m_vTo.x -= 63;
	}
	FlipLinedef(&Door1);
	FlipLinedef(&Door2);

	// Sector
	FillSector(&Sector,lpMap->yFloor);
	nSector = AddSector(&Sector);

	// Door tracks
	lstrcpyA(Track1.m_sRight.szMiddle,"DOORTRAK");
	lstrcpyA(Track2.m_sRight.szMiddle,"DOORTRAK");
	Track1.m_Flags = 17;	// Impassible & Lower unpegged
	Track2.m_Flags = 17;	// Impassible & Lower unpegged
	// TJL - Added
	Socket1.m_Flags = 17;	// Impassible & Lower unpegged
	Socket2.m_Flags = 17;	// Impassible & Lower unpegged
	// Door frames
	for(i = 6; i < 12; i++)
	{
		lstrcpyA(lpList[i]->m_sRight.szMiddle,"DOORFRAM");
		lpList[i]->m_Flags = 1;
		lpList[i]->m_sRight.yOffset = 0;
		lpList[i]->m_sRight.Sector = nSector;
	}
	Split2.m_sRight.xOffset = -28;
	Split4.m_sRight.xOffset = -28;

	// Sector Assignment
	Ent1.m_sRight.Sector = nSector;
	Ent2.m_sRight.Sector = nSector;
	Ent1.m_sLeft.Sector = -1;
	Ent2.m_sLeft.Sector = -1;
	Door1.m_sRight.Sector = nSector;
	Door2.m_sRight.Sector = nSector;

	nSector = AddSector(&Sector);
	Track1.m_sRight.Sector = nSector;
	Track2.m_sRight.Sector = nSector;
	Door1.m_sLeft.Sector = nSector;
	Door2.m_sLeft.Sector = nSector;

	// TJL
	Socket1.m_sRight.Sector = nSector;
	Socket2.m_sRight.Sector = nSector;

	// More line stuff
	if(bSilence)		//TJL - Only silence between sector with different floor types
		Ent1.m_Flags = 68;
	else
		Ent1.m_Flags = 4;
	Ent1.m_Flags |= 0x0800;	// Crossed by monster

	if(bSilence)
		Ent2.m_Flags = 68;
	else
		Ent2.m_Flags = 4;
	Ent2.m_Flags |= 0x0800;	// Crossed by monster

	Door1.m_Flags = 4;
	Door2.m_Flags = 4;
	FlipLinedef(&Ent1);
	FlipLinedef(&Ent2);

	LPPOLY_QUEUE lpNew = __new POLY_QUEUE;
	if(lpNew)
	{
		Door1.GetMapLine(&lpNew->Door1);
		Door2.GetMapLine(&lpNew->Door2);

		lpNew->iType = iType;
		lpNew->bDir = bDir;
		lpNew->iPoly = m_iNextPoly++;
		lpNew->pTexture = pDoor->GetTexture();
		lpNew->Next = NULL;

		if(m_lpQueue)
		{
			LPPOLY_QUEUE lpList = m_lpQueue;
			while(lpList->Next)
				lpList = lpList->Next;
			lpList->Next = lpNew;
		}
		else
			m_lpQueue = lpNew;
		if(m_bUseAutoDoors)
		{
			Ent1.m_Special = 226;
			Ent2.m_Special = 226;
			Ent1.m_Flags |= (512 | 8192);
			Ent2.m_Flags |= (512 | 8192);
			//Ent1.m_Flags |= (2 << 10);
			//Ent2.m_Flags |= (2 << 10);
			Ent1.m_Args[0] = 1;
			Ent2.m_Args[0] = 1;
			Ent1.m_Args[2] = lpNew->iPoly;
			Ent2.m_Args[2] = lpNew->iPoly;
			Ent1.m_Args[3] = bDir + 1;
			Ent2.m_Args[3] = bDir + 1;
			Ent1.m_Args[4] = 5;
			Ent2.m_Args[4] = 5;
		}

		/*
		Door1.m_Special = 226;
		Door1.m_Flags = 1540;
		Door1.m_Args[0] = 1;
		Door1.m_Args[2] = lpNew->iPoly;
		Door1.m_Args[3] = bDir + 1;
		Door1.m_Args[4] = iType - 1;
		Door2.m_Special = 226;
		Door2.m_Flags = 1540;
		Door2.m_Args[0] = 1;
		Door2.m_Args[2] = lpNew->iPoly;
		Door2.m_Args[3] = bDir + 1;
		Door2.m_Args[4] = iType - 1;
		*/
	}

	for(i = 0; i < 12; i++)
	{
		//TJL - Added check for doors next to doors...
		// If the entryway lines already exist, make them double-sided/passable
		if(i == 0 || i == 1)
		{
			CMapLine* lpFind = FindLine(&lpList[i]->m_vTo,&lpList[i]->m_vFrom);
			if(lpFind)
				AddSidedLine(lpList[i], lpList[i]->m_sRight.Sector, "-");
			else if(m_bUseAutoDoors)
				AddLine(lpList[i]);
		}
		else
			AddLine(lpList[i]);
	}
}

VOID CMapConvert::BuildPolyDoor (LPWOLFDATA lpMap, CMapLine* lpDoor1, CMapLine* lpDoor2, INT iType, INT bDir, const TEXTURE* pTexture, INT iPoly)
{
	CMapBox box;
	CMapThing Anchor, Start;
	RECT rcBox;
	SHORT sSector;
	INT idxDoor1, idxDoor2, idxStop1, idxStop2, idxPolyTag;

	if(bDir == 0)	// East/West
	{
		INT xSize = lpDoor2->m_vFrom.x - lpDoor1->m_vFrom.x;
		INT ySize = lpDoor1->m_vFrom.y - lpDoor1->m_vTo.y;

		sSector = m_sHorizontal;

		rcBox.left = m_xNextDoor;
		rcBox.right = m_xNextDoor + xSize;
		rcBox.top = (m_yPolyStart - 96) - ySize / 2;
		rcBox.bottom = rcBox.top + ySize;
		m_xNextDoor += xSize + 16;

		idxDoor1 = 0;
		idxDoor2 = 2;
		idxStop1 = 1;
		idxStop2 = 3;
		idxPolyTag = idxStop2;

		Start.m_x = (lpDoor1->m_vTo.x + lpDoor2->m_vTo.x) / 2;
		Start.m_y = (lpDoor1->m_vFrom.y + lpDoor1->m_vTo.y) / 2;
	}
	else			// North/South
	{
		INT xSize = lpDoor1->m_vFrom.x - lpDoor1->m_vTo.x;
		INT ySize = lpDoor1->m_vFrom.y - lpDoor2->m_vFrom.y;

		sSector = m_sVertical;

		rcBox.left = (m_xPolyStart - 96) - xSize / 2;
		rcBox.right = rcBox.left + xSize;
		rcBox.top = m_yNextDoor;
		rcBox.bottom = m_yNextDoor + ySize;
		m_yNextDoor += ySize + 16;

		idxDoor1 = 1;
		idxDoor2 = 3;
		idxStop1 = 0;
		idxStop2 = 2;
		idxPolyTag = idxStop2;

		Start.m_x = (lpDoor1->m_vFrom.x + lpDoor1->m_vTo.x) / 2;
		Start.m_y = (lpDoor1->m_vTo.y + lpDoor2->m_vTo.y) / 2;
	}

	PrepareBox(&box, rcBox.left, rcBox.top, rcBox.right, rcBox.bottom, sSector, TRUE);

	CopyTexture(box.m_Box[idxDoor1].m_sRight.szMiddle, pTexture->pcwzName);
	CopyTexture(box.m_Box[idxDoor2].m_sRight.szMiddle, pTexture->pcwzName);
	box.m_Box[idxDoor1].m_sRight.xOffset = lpDoor1->m_sRight.xOffset;
	box.m_Box[idxDoor1].m_sRight.yOffset = lpDoor1->m_sRight.yOffset;
	box.m_Box[idxDoor2].m_sRight.xOffset = lpDoor2->m_sRight.xOffset;
	box.m_Box[idxDoor2].m_sRight.yOffset = lpDoor2->m_sRight.yOffset;
	if(iType == DOOR_SECRET)
	{
		CopyTexture(box.m_Box[idxStop1].m_sRight.szMiddle, pTexture->pcwzName);
		CopyTexture(box.m_Box[idxStop2].m_sRight.szMiddle, pTexture->pcwzName);
	}
	else
	{
		CopyTexture(box.m_Box[idxStop1].m_sRight.szMiddle, L"DOORSTOP");
		CopyTexture(box.m_Box[idxStop2].m_sRight.szMiddle, L"DOORSTOP");
	}

	box.m_Box[idxDoor1].m_Flags |= 0x0200 /* repeatable */ | 0x0400 /* used by player */;
	box.m_Box[idxDoor1].m_Special = 80;
	box.m_Box[idxDoor1].m_Args[0] = 1;
	box.m_Box[idxDoor1].m_Args[1] = 0;
	box.m_Box[idxDoor1].m_Args[2] = (BYTE)iPoly;
	if(iType == DOOR_SECRET)
	{
		if(bDir)	// North/South
			box.m_Box[idxDoor1].m_Args[3] = 2;
		else		// East/West
			box.m_Box[idxDoor1].m_Args[3] = 1;
	}
	else
		box.m_Box[idxDoor1].m_Args[3] = bDir + 1;
	box.m_Box[idxDoor1].m_Args[4] = iType - 1;

	box.m_Box[idxDoor2].m_Flags |= 0x0200 /* repeatable */ | 0x0400 /* used by player */;
	box.m_Box[idxDoor2].m_Special = 80;
	box.m_Box[idxDoor2].m_Args[0] = 1;
	box.m_Box[idxDoor2].m_Args[1] = 0;
	box.m_Box[idxDoor2].m_Args[2] = (BYTE)iPoly;
	if(iType == DOOR_SECRET)
	{
		if(bDir)		// North/South
			box.m_Box[idxDoor2].m_Args[3] = 4;
		else			// East/West
			box.m_Box[idxDoor2].m_Args[3] = 3;
	}
	else
		box.m_Box[idxDoor2].m_Args[3] = bDir + 1;
	box.m_Box[idxDoor2].m_Args[4] = iType - 1;

	box.m_Box[idxPolyTag].m_Special = 1;
	box.m_Box[idxPolyTag].m_Args[0] = (BYTE)iPoly;

	CommitBox(&box);

	Anchor.m_x = (SHORT)((rcBox.left + rcBox.right) / 2);
	Anchor.m_y = (SHORT)((rcBox.top + rcBox.bottom) / 2);

	Anchor.m_Angle = iPoly;
	Anchor.m_Options = 2023;
	Anchor.m_Type = 9300;
	AddThing(&Anchor);

	Start.m_Angle = iPoly;
	Start.m_Options = 2023;
	Start.m_Type = 9301;
	AddThing(&Start);
}

VOID CMapConvert::BuildSectors (LPWOLFDATA lpMap)
{
	CBlockMap* pMap = lpMap->pMap;
	USHORT* lpBuild = __new USHORT[lpMap->xSize * lpMap->zSize];
	USHORT* lpRow = lpBuild;
	INT x, y;
	SECTOR Sector;

	for(y = 0; y < lpMap->zSize; y++)
	{
		for(x = 0; x < lpMap->xSize; x++)
			lpRow[x] = pMap->GetFloor(x, y);
		lpRow += lpMap->xSize;
	}

	m_lpAssign = lpBuild;

	lpRow = lpBuild;
	for(y = 0; y < lpMap->zSize; y++)
	{
		for(x = 0; x < lpMap->xSize; x++)
		{
			if(lpRow[x] != 65535)
			{
				SHORT nSector;

				FillSector(&Sector, lpMap->yFloor);
				nSector = AddSector(&Sector);
				AssignSector(lpMap, nSector, x, y);
			}
		}
		lpRow += lpMap->xSize;
	}

	m_lpAssign = NULL;
	__delete_array lpBuild;
}

VOID CMapConvert::BuildOutsideViews (LPWOLFDATA lpMap)
{
	LPLINE_LIST pList = m_lpList;

	while(pList)
	{
		PCSTR pcszTexture = pList->lpLine->m_sRight.szMiddle;
		if(0 == memcmp(pcszTexture, SLP("OUTSIDE")) && pcszTexture[7] == '\0')
		{
			CMapLine* lpLine = pList->lpLine;
			INT iDir;
			SECTOR* lpSector;

			if(lpLine->m_vFrom.x == lpLine->m_vTo.x)
			{
				if(lpLine->m_vFrom.y > lpLine->m_vTo.y)
					iDir = LINE_RIGHT;
				else
					iDir = LINE_LEFT;
			}
			else
			{
				if(lpLine->m_vFrom.x < lpLine->m_vTo.x)
					iDir = LINE_ABOVE;
				else
					iDir = LINE_BELOW;
			}
			lpSector = FindSector(lpLine->m_sRight.Sector);
			if(lpSector)
			{
				PCWSTR pcwzTexture = L"GRAYSTN0";
				INT xOutside = MAP_WIDTH / 2 + lpLine->m_vFrom.x / 64, xBlock;
				INT zOutside = MAP_HEIGHT / 2 - lpLine->m_vFrom.y / 64, zBlock;
				TStackRef<CPaintItem> srBlock, srObject;

				// Determine the block coordinates of the "outside" wall cell:
				switch(iDir)
				{
				case LINE_LEFT:
					xOutside--;
					zOutside--;
					break;
				case LINE_RIGHT:
					break;
				case LINE_ABOVE:
					zOutside--;
					break;
				case LINE_BELOW:
					xOutside--;
					break;
				}

				// Now find the block next to the "outside" cell:
				xBlock = xOutside;
				zBlock = zOutside;
				switch(iDir)
				{
				case LINE_LEFT:
					zBlock++;
					break;
				case LINE_RIGHT:
					zBlock--;
					break;
				case LINE_ABOVE:
					xBlock--;
					break;
				case LINE_BELOW:
					xBlock++;
					break;
				}

				if(SUCCEEDED(lpMap->pMap->GetCellData(xBlock, zBlock, &srBlock, &srObject)) && srBlock && srBlock->GetType() == MapCell::Wall)
				{
					CTextureItem* pWall = srBlock.StaticCast<CTextureItem>();
					const TEXTURE* pcTexture = pWall->GetTexture();
					pcwzTexture = pcTexture->pcwzName;
				}

				ExtendOutside(lpSector, lpLine, iDir, pcwzTexture);
			}
		}

		pList = pList->Next;
	}
}

VOID CMapConvert::BuildPlayerStarts (LPWOLFDATA lpMap)
{
	CMapThing Thing;

	Thing.m_Options = 7;
	for(INT y = 0; y < lpMap->zSize; y++)
	{
		for(INT x = 0; x < lpMap->xSize; x++)
		{
			TStackRef<CPaintItem> srBlock, srObject;

			SideAssertHr(lpMap->pMap->GetCellData(x, y, &srBlock, &srObject));
			if(srObject && MapCell::Start == srObject->GetType())
			{
				// Player Start!
				CStartItem* pStart = srObject.StaticCast<CStartItem>();

				Thing.m_Angle = pStart->GetDirection();
				Thing.m_Type = 1;
				Thing.m_Options |= 775;

				PositionThing(lpMap, &Thing, x, y);
				AddThing(&Thing);
			}
		}
	}
}

VOID CMapConvert::BuildThings (LPWOLFDATA lpMap)
{
	CMapThing Thing;
	Thing.m_z = lpMap->yFloor;

	for(INT y = 0; y < lpMap->zSize; y++)
	{
		for(INT x = 0; x < lpMap->xSize; x++)
		{
			TStackRef<CPaintItem> srBlock, srObject;

			SideAssertHr(lpMap->pMap->GetCellData(x, y, &srBlock, &srObject));
			if(srObject && MapCell::Object == srObject->GetType())
			{
				CActorItem* pActor = srObject.StaticCast<CActorItem>();

				// The options will contain the difficulty level plus the single player flag.
				SideAssertHr(lpMap->pMap->GetObjectFlags(x, y, &Thing.m_Options));

				Thing.m_Type = pActor->GetActorID();
				Thing.m_Angle = pActor->GetDirection();
				PositionThing(lpMap, &Thing, x, y);
				AddThing(&Thing);
			}
		}
	}
}

BOOL CMapConvert::AssignSector (LPWOLFDATA lpMap, SHORT nSector, INT x, INT y)
{
	INT i, index;
	CMapLine Line, *lpFind;
	USHORT f;
	if(x >= 0 && y >= 0 && x < lpMap->xSize && y < lpMap->zSize)
	{
		TStackRef<CPaintItem> srBlock, srObject;

		SideAssertHr(lpMap->pMap->GetCellData(x, y, &srBlock, &srObject));
		if(srObject && MapCell::Door == srObject->GetType())
			f = 65535;
		else
			f = m_lpAssign[y * lpMap->xSize + x];
	}
	else
		f = 65535;
	if(f != 65535)
	{
		index = y * lpMap->xSize + x;
		m_lpAssign[index] = 65535;
		m_lpSectorTable[index] = nSector;
		for(i = LINE_LEFT; i <= LINE_BELOW; i++)
		{
			PositionLine(lpMap, &Line, x, y, i);
			lpFind = FindLine(&Line.m_vFrom,&Line.m_vTo);
			if(lpFind)
				lpFind->m_sRight.Sector = nSector;
		}
		AssignSector(lpMap, nSector, x-1,y);
		AssignSector(lpMap, nSector, x+1,y);
		AssignSector(lpMap, nSector, x,y-1);
		AssignSector(lpMap, nSector, x,y+1);
		return TRUE;
	}
	return FALSE;
}

VOID CMapConvert::MergeDone (CMapLine* lpLine)
{
	m_pProgress->ReportMergeDone(lpLine);
}

VOID CMapConvert::MergeLines (VOID)
{
	LPLINE_LIST lpList, lpFind, lpPrev;
	CMapLine* lpLine, *lpTest;
	do
	{
		lpList = m_lpList;
		while(lpList)
		{
			lpLine = lpList->lpLine;
			lpFind = FindNext(&lpLine->m_vTo,&lpPrev);
			if(lpFind && (lpFind->lpLine->m_sRight.Sector == lpLine->m_sRight.Sector) && (lpFind->lpLine->m_sLeft.Sector == lpLine->m_sLeft.Sector))
			{
				BOOL bNext = FALSE;
				lpTest = lpFind->lpLine;
				if(lpLine->m_vFrom.x == lpLine->m_vTo.x)
				{
					if(lpLine->m_vTo.x == lpTest->m_vTo.x)
						bNext = TRUE;
				}
				else
				{
					if(lpLine->m_vTo.y == lpTest->m_vTo.y)
						bNext = TRUE;
				}
				if(bNext)
				{
					if(TStrCmpNAssert(lpLine->m_sRight.szMiddle, lpTest->m_sRight.szMiddle, ARRAYSIZE(lpTest->m_sRight.szMiddle)) == 0)
					{
						lpLine->m_vTo.x = lpTest->m_vTo.x;
						lpLine->m_vTo.y = lpTest->m_vTo.y;
						if(lpPrev)
							lpPrev->Next = lpFind->Next;
						else
							m_lpList = lpFind->Next;
						delete lpFind->lpLine;
						delete lpFind;
						MergeDone(lpLine);
						break;
					}
				}
			}
			lpList = lpList->Next;
		}
	} while(lpList);
}

VOID CMapConvert::BuildPolys (LPWOLFDATA lpMap)
{
	LPPOLY_QUEUE lpPoly;
	CMapLine* lpDoor1, *lpDoor2;
	SECTOR Sector;

	m_xPolyStart = lpMap->xSize * -32;
	m_xNextDoor = m_xPolyStart;

	m_yPolyStart = lpMap->zSize * -32;
	m_yNextDoor = m_yPolyStart;

	FillSector(&Sector, lpMap->yFloor);
	m_sHorizontal = AddSector(&Sector);
	m_sVertical = AddSector(&Sector);

	DrawBox(m_xNextDoor - 16, m_yNextDoor - 144, lpMap->xSize * 32, m_yNextDoor - 48, m_sHorizontal);
	DrawBox(m_xNextDoor - 144, m_yNextDoor - 16, m_xNextDoor - 48, lpMap->zSize * 32, m_sVertical);

	while(m_lpQueue)
	{
		lpPoly = m_lpQueue;
		m_lpQueue = m_lpQueue->Next;
		lpDoor1 = new CMapLine(&lpPoly->Door1);
		lpDoor2 = new CMapLine(&lpPoly->Door2);
		BuildPolyDoor(lpMap,lpDoor1,lpDoor2,lpPoly->iType,lpPoly->bDir,lpPoly->pTexture,lpPoly->iPoly);
		delete lpDoor2;
		delete lpDoor1;
		delete lpPoly;
	}
}

VOID CMapConvert::FixSidedefs (VOID)
{
	LPLINE_LIST lpList = m_lpList;
	CMapLine* lpLine;
	while(lpList)
	{
		lpLine = lpList->lpLine;
		if(lpLine->m_Flags & 4)
		{
			if(lpLine->m_sRight.Sector == -1)
			{
				FlipLinedef(lpLine);
				lpLine->m_Flags &= ~4;
				lpLine->m_Flags |= 1;
				lstrcpyA(lpLine->m_sRight.szMiddle,"NZ01");
			}
		}
		lpList = lpList->Next;
	}
}

VOID CMapConvert::BuildVertexList (VOID)
{
	LPLINE_LIST lpL = m_lpList;
	LPVERTEX_LIST lpV, lpPrev = NULL;
	LPVERTEX lpVertex;
	INT i;
	while(lpL)
	{
		for(i = 0; i < 2; i++)
		{
			if(i == 0)
				lpVertex = &lpL->lpLine->m_vFrom;
			else
				lpVertex = &lpL->lpLine->m_vTo;
			if(FindVertex(lpVertex) == 65535)
			{
				lpV = new VERTEX_LIST;
				lpV->Vertex.x = lpVertex->x;
				lpV->Vertex.y = lpVertex->y;
				lpV->Next = NULL;
				if(lpPrev)
					lpPrev->Next = lpV;
				else
					m_lpVertices = lpV;
				lpPrev = lpV;
			}
		}
		lpL = lpL->Next;
	}
}

VOID CMapConvert::ExtendOutside (SECTOR* pAdjacent, CMapLine* lpLine, INT iDir, PCWSTR pcwzTexture)
{
	CMapLine Line[9];
	INT x, y, i;
	SHORT iSector[3];
	SECTOR Sector[3];
	ZeroMemory(Sector,sizeof(SECTOR) * 3);

	for(i = 0; i < 9; i++)
	{
		lstrcpyA(Line[i].m_sRight.szUpper,"-");
		lstrcpyA(Line[i].m_sRight.szMiddle,"-");
		lstrcpyA(Line[i].m_sRight.szLower,"-");
		if(i >= 6)
		{
			lstrcpyA(Line[i].m_sLeft.szUpper,"-");
			lstrcpyA(Line[i].m_sLeft.szMiddle,"-");
			lstrcpyA(Line[i].m_sLeft.szLower,"-");
		}
	}
	switch(iDir)
	{
	case LINE_LEFT:
		x = lpLine->m_vFrom.x;
		y = lpLine->m_vFrom.y;
		for(i = 0; i < 3; i++)
		{
			Line[i].m_vFrom.y = y;
			Line[i].m_vTo.y = y;
		}
		Line[0].m_vFrom.x = x;
		Line[0].m_vTo.x = x - 48;
		Line[1].m_vFrom.x = x - 48;
		Line[1].m_vTo.x = x - 56;
		Line[2].m_vFrom.x = x - 56;
		Line[2].m_vTo.x = x - 64;
		x = lpLine->m_vTo.x;
		y = lpLine->m_vTo.y;
		for(i = 3; i < 6; i++)
		{
			Line[i].m_vFrom.y = y;
			Line[i].m_vTo.y = y;
		}
		Line[3].m_vFrom.x = x - 64;
		Line[3].m_vTo.x = x - 56;
		Line[4].m_vFrom.x = x - 56;
		Line[4].m_vTo.x = x - 48;
		Line[5].m_vFrom.x = x - 48;
		Line[5].m_vTo.x = x;
		break;
	case LINE_RIGHT:
		x = lpLine->m_vFrom.x;
		y = lpLine->m_vFrom.y;
		for(i = 0; i < 3; i++)
		{
			Line[i].m_vFrom.y = y;
			Line[i].m_vTo.y = y;
		}
		Line[0].m_vFrom.x = x;
		Line[0].m_vTo.x = x + 48;
		Line[1].m_vFrom.x = x + 48;
		Line[1].m_vTo.x = x + 56;
		Line[2].m_vFrom.x = x + 56;
		Line[2].m_vTo.x = x + 64;
		x = lpLine->m_vTo.x;
		y = lpLine->m_vTo.y;
		for(i = 3; i < 6; i++)
		{
			Line[i].m_vFrom.y = y;
			Line[i].m_vTo.y = y;
		}
		Line[3].m_vFrom.x = x + 64;
		Line[3].m_vTo.x = x + 56;
		Line[4].m_vFrom.x = x + 56;
		Line[4].m_vTo.x = x + 48;
		Line[5].m_vFrom.x = x + 48;
		Line[5].m_vTo.x = x;
		break;
	case LINE_ABOVE:
		x = lpLine->m_vFrom.x;
		y = lpLine->m_vFrom.y;
		for(i = 0; i < 3; i++)
		{
			Line[i].m_vFrom.x = x;
			Line[i].m_vTo.x = x;
		}
		Line[0].m_vFrom.y = y;
		Line[0].m_vTo.y = y + 48;
		Line[1].m_vFrom.y = y + 48;
		Line[1].m_vTo.y = y + 56;
		Line[2].m_vFrom.y = y + 56;
		Line[2].m_vTo.y = y + 64;
		x = lpLine->m_vTo.x;
		y = lpLine->m_vTo.y;
		for(i = 3; i < 6; i++)
		{
			Line[i].m_vFrom.x = x;
			Line[i].m_vTo.x = x;
		}
		Line[3].m_vFrom.y = y + 64;
		Line[3].m_vTo.y = y + 56;
		Line[4].m_vFrom.y = y + 56;
		Line[4].m_vTo.y = y + 48;
		Line[5].m_vFrom.y = y + 48;
		Line[5].m_vTo.y = y;
		break;
	case LINE_BELOW:
		x = lpLine->m_vFrom.x;
		y = lpLine->m_vFrom.y;
		for(i = 0; i < 3; i++)
		{
			Line[i].m_vFrom.x = x;
			Line[i].m_vTo.x = x;
		}
		Line[0].m_vFrom.y = y;
		Line[0].m_vTo.y = y - 48;
		Line[1].m_vFrom.y = y - 48;
		Line[1].m_vTo.y = y - 56;
		Line[2].m_vFrom.y = y - 56;
		Line[2].m_vTo.y = y - 64;
		x = lpLine->m_vTo.x;
		y = lpLine->m_vTo.y;
		for(i = 3; i < 6; i++)
		{
			Line[i].m_vFrom.x = x;
			Line[i].m_vTo.x = x;
		}
		Line[3].m_vFrom.y = y - 64;
		Line[3].m_vTo.y = y - 56;
		Line[4].m_vFrom.y = y - 56;
		Line[4].m_vTo.y = y - 48;
		Line[5].m_vFrom.y = y - 48;
		Line[5].m_vTo.y = y;
		break;
	}
	CopyMemory(&Line[6].m_vFrom,&Line[2].m_vTo,sizeof(VERTEX));
	CopyMemory(&Line[7].m_vFrom,&Line[1].m_vTo,sizeof(VERTEX));
	CopyMemory(&Line[8].m_vFrom,&Line[0].m_vTo,sizeof(VERTEX));
	CopyMemory(&Line[6].m_vTo,&Line[3].m_vFrom,sizeof(VERTEX));
	CopyMemory(&Line[7].m_vTo,&Line[4].m_vFrom,sizeof(VERTEX));
	CopyMemory(&Line[8].m_vTo,&Line[5].m_vFrom,sizeof(VERTEX));
	for(i = 0; i < 3; i++)
	{
		Line[i].m_Flags = 1;
		Line[i + 3].m_Flags = 1;
		CopyTexture(Line[i].m_sRight.szMiddle, pcwzTexture);
		CopyTexture(Line[i + 3].m_sRight.szMiddle, pcwzTexture);
	}
	CopyTexture(Line[6].m_sRight.szMiddle, pcwzTexture);
	CopyTexture(Line[8].m_sLeft.szLower, pcwzTexture);
	Line[6].m_Flags = 1;
	Line[7].m_Flags = 4;
	Line[8].m_Flags = 4;

	iSector[0] = AddSector(Sector + 0);
	iSector[1] = AddSector(Sector + 1);
	iSector[2] = AddSector(Sector + 2);

	lpLine->m_Flags = 4;
	lpLine->m_sLeft.Sector = iSector[0];
	lstrcpyA(lpLine->m_sRight.szMiddle,"-");
	lstrcpyA(lpLine->m_sLeft.szUpper,"-");
	lstrcpyA(lpLine->m_sLeft.szMiddle,"-");
	lstrcpyA(lpLine->m_sLeft.szLower,"-");
	Line[0].m_sRight.Sector = iSector[0];
	Line[1].m_sRight.Sector = iSector[1];
	Line[2].m_sRight.Sector = iSector[2];
	Line[3].m_sRight.Sector = iSector[2];
	Line[4].m_sRight.Sector = iSector[1];
	Line[5].m_sRight.Sector = iSector[0];
	Line[6].m_sRight.Sector = iSector[2];
	Line[7].m_sRight.Sector = iSector[1];
	Line[8].m_sRight.Sector = iSector[0];
	Line[7].m_sLeft.Sector = iSector[2];
	Line[8].m_sLeft.Sector = iSector[1];
	for(i = 0; i < 3; i++)
		FillSector(&Sector[i], pAdjacent->Floor);
	if(iDir == LINE_ABOVE || iDir == LINE_BELOW)
	{
		Sector[0].Light = (m_iLightLevel < 200) ? 200 : m_iLightLevel;
		Sector[1].Light = 255;
		Sector[2].Light = 255;
	}
	else
	{
		Sector[0].Light = (m_iLightLevel > 100) ? 100 : m_iLightLevel;
		Sector[1].Light = (m_iLightLevel > 50) ? 50 : m_iLightLevel;
		Sector[2].Light = (m_iLightLevel > 50) ? 50 : m_iLightLevel;
	}
	//Sector[1].Floor -= 16;
	//Sector[2].Floor -= 16;
	Sector[2].Ceiling -= 64;
	lstrcpyA(Sector[1].szCeiling,"F_SKY1");
	lstrcpyA(Sector[2].szCeiling,"F_SKY1");

	// 07/09/2006 - Added texture offsets
	Line[0].m_sRight.xOffset = 0;
	Line[1].m_sRight.xOffset = 48;
	Line[4].m_sRight.xOffset = 8;
	Line[5].m_sRight.xOffset = 16;

	for(i = 0; i < 3; i++)
	{
		SECTOR* pSector = FindSector(iSector[i]);
		if(pSector)
			CopyMemory(pSector, Sector + i, sizeof(SECTOR));
	}

	for(i = 0; i < 9; i++)
		AddLine(&Line[i]);
}

HRESULT CMapConvert::BuildCage (LPWOLFDATA lpMap, INT x, INT y, INT nLine, CMapLine* pLine, CWallCage* pCage)
{
	HRESULT hr;

	// Defining a North-facing cage:
	VERTEX rgvPoints[] =
	{
		// Row 1
		{ 0, 0 },
		{ 12, 0 },
		{ 52, 0 },
		{ 64, 0 },

		// Row 2
		{ 0, -4 },
		{ 12, -4 },
		{ 52, -4 },
		{ 64, -4 },

		// Row 3
		{ 0, -8 },
		{ 12, -8 },
		{ 52, -8 },
		{ 64, -8 },

		// Row 4
		{ 0, -12 },
		{ 12, -12 },
		{ 52, -12 },
		{ 64, -12 },

		// Row 5
		{ 0, -64 },
		{ 64, -64 }
	};

	SHORT sFacing = pLine->m_sRight.Sector, s1, s2, s3, s4;
	VERTEX vFrom = pLine->m_vFrom;
	VERTEX vTo = pLine->m_vTo;
	SECTOR Sector;
	CMapLine Line, Double, NoTexture;
	LPPOLY_QUEUE lpNew = __new POLY_QUEUE;

	CheckAlloc(lpNew);

	switch(nLine)
	{
	case LINE_LEFT:
		RotatePoints(rgvPoints, ARRAYSIZE(rgvPoints));
		RotatePoints(rgvPoints, ARRAYSIZE(rgvPoints));
		RotatePoints(rgvPoints, ARRAYSIZE(rgvPoints));
		break;
	case LINE_BELOW:
		RotatePoints(rgvPoints, ARRAYSIZE(rgvPoints));
		RotatePoints(rgvPoints, ARRAYSIZE(rgvPoints));
		break;
	case LINE_RIGHT:
		RotatePoints(rgvPoints, ARRAYSIZE(rgvPoints));
		break;
	}

	RemoveLine(pLine);

	for(INT i = 0; i < ARRAYSIZE(rgvPoints); i++)
	{
		rgvPoints[i].x += vTo.x;
		rgvPoints[i].y += vTo.y;
	}

	FillSector(&Sector, lpMap->yFloor);
	Sector.Ceiling = Sector.Floor + 51;
	s1 = AddSector(&Sector);
	s2 = AddSector(&Sector);

	Sector.Light -= 48;
	s3 = AddSector(&Sector);

	Sector.Ceiling = Sector.Floor + 64;
	Sector.Light -= 48;
	s4 = AddSector(&Sector);

	CopyTexture(Line.m_sRight.szUpper, L"-");
	CopyTexture(Line.m_sRight.szMiddle, pCage->GetTexture()->pcwzName);
	CopyTexture(Line.m_sRight.szLower, L"-");
	Line.m_Flags = 1;

	CopyTexture(Double.m_sRight.szUpper, pCage->GetTexture()->pcwzName);
	CopyTexture(Double.m_sRight.szMiddle, L"-");
	CopyTexture(Double.m_sRight.szLower, L"-");
	CopyTexture(Double.m_sLeft.szUpper, L"-");
	CopyTexture(Double.m_sLeft.szMiddle, L"-");
	CopyTexture(Double.m_sLeft.szLower, L"-");
	Double.m_Flags = 4;

	CopyTexture(NoTexture.m_sRight.szUpper, L"-");
	CopyTexture(NoTexture.m_sRight.szMiddle, L"-");
	CopyTexture(NoTexture.m_sRight.szLower, L"-");
	CopyTexture(NoTexture.m_sLeft.szUpper, L"-");
	CopyTexture(NoTexture.m_sLeft.szMiddle, L"-");
	CopyTexture(NoTexture.m_sLeft.szLower, L"-");
	NoTexture.m_Flags = 4;

	Line.m_sRight.Sector = sFacing;

	// Entrance lines for texture alignment

	Line.m_vFrom = rgvPoints[1];
	Line.m_vTo = rgvPoints[0];
	Check(AddLine(&Line));

	Line.m_vFrom = rgvPoints[3];
	Line.m_vTo = rgvPoints[2];
	Check(AddLine(&Line));

	// Top sector (in front of cage door)

	Double.m_vFrom = rgvPoints[2];
	Double.m_vTo = rgvPoints[1];
	Double.m_sRight.Sector = sFacing;
	Double.m_sLeft.Sector = s1;
	Double.m_sRight.xOffset = -8;
	Double.m_sRight.yOffset = -51;
	Check(AddLine(&Double));

	Line.m_vFrom = rgvPoints[5];
	Line.m_vTo = rgvPoints[1];
	Line.m_sRight.Sector = s1;
	Line.m_sRight.xOffset = -4;
	Line.m_sRight.yOffset = 13;
	Check(AddLine(&Line));

	Line.m_vFrom = rgvPoints[2];
	Line.m_vTo = rgvPoints[6];
	Line.m_sRight.Sector = s1;
	Line.m_sRight.xOffset = 0;
	Line.m_sRight.yOffset = 13;
	Check(AddLine(&Line));

	NoTexture.m_vFrom = rgvPoints[6];
	NoTexture.m_vTo = rgvPoints[5];
	NoTexture.m_sRight.Sector = s1;
	NoTexture.m_sLeft.Sector = s2;
	Check(AddLine(&NoTexture));
	NoTexture.GetMapLine(&lpNew->Door1);
	lpNew->Door1.sRight.xOffset = 140;
	lpNew->Door1.sRight.yOffset = 13;

	// Middle sector (cage door)

	Line.m_vFrom = rgvPoints[4];
	Line.m_vTo = rgvPoints[5];
	Line.m_sRight.Sector = s2;
	Check(AddLine(&Line));

	Line.m_vFrom = rgvPoints[6];
	Line.m_vTo = rgvPoints[7];
	Line.m_sRight.Sector = s2;
	Check(AddLine(&Line));

	Line.m_vFrom = rgvPoints[8];
	Line.m_vTo = rgvPoints[4];
	Line.m_sRight.Sector = s2;
	Check(AddLine(&Line));

	Line.m_vFrom = rgvPoints[7];
	Line.m_vTo = rgvPoints[11];
	Line.m_sRight.Sector = s2;
	Check(AddLine(&Line));

	Line.m_vFrom = rgvPoints[9];
	Line.m_vTo = rgvPoints[8];
	Line.m_sRight.Sector = s2;
	Check(AddLine(&Line));

	Line.m_vFrom = rgvPoints[11];
	Line.m_vTo = rgvPoints[10];
	Line.m_sRight.Sector = s2;
	Check(AddLine(&Line));

	NoTexture.m_vFrom = rgvPoints[9];
	NoTexture.m_vTo = rgvPoints[10];
	NoTexture.m_sRight.Sector = s3;
	NoTexture.m_sLeft.Sector = s2;
	Check(AddLine(&NoTexture));
	NoTexture.GetMapLine(&lpNew->Door2);
	lpNew->Door2.sRight.xOffset = 140;
	lpNew->Door2.sRight.yOffset = 13;

	// Bottom sector (below cage door)

	Line.m_vFrom = rgvPoints[13];
	Line.m_vTo = rgvPoints[9];
	Line.m_sRight.Sector = s3;
	Line.m_sRight.xOffset = 0;
	Line.m_sRight.yOffset = 13;
	Check(AddLine(&Line));

	Line.m_vFrom = rgvPoints[10];
	Line.m_vTo = rgvPoints[14];
	Line.m_sRight.Sector = s3;
	Line.m_sRight.xOffset = -4;
	Line.m_sRight.yOffset = 13;
	Check(AddLine(&Line));

	Double.m_vFrom = rgvPoints[13];
	Double.m_vTo = rgvPoints[14];
	Double.m_sRight.Sector = s4;
	Double.m_sLeft.Sector = s3;
	Check(AddLine(&Double));

	// Cell sector

	Line.m_sRight.xOffset = 0;
	Line.m_sRight.yOffset = 0;

	CopyTexture(Line.m_sRight.szMiddle, pCage->GetSecondaryTexture());

	Line.m_vFrom = rgvPoints[12];
	Line.m_vTo = rgvPoints[13];
	Line.m_sRight.Sector = s4;
	Check(AddLine(&Line));

	Line.m_vFrom = rgvPoints[14];
	Line.m_vTo = rgvPoints[15];
	Line.m_sRight.Sector = s4;
	Check(AddLine(&Line));

	Line.m_vFrom = rgvPoints[15];
	Line.m_vTo = rgvPoints[17];
	Line.m_sRight.Sector = s4;
	Check(AddLine(&Line));

	Line.m_vFrom = rgvPoints[17];
	Line.m_vTo = rgvPoints[16];
	Line.m_sRight.Sector = s4;
	Check(AddLine(&Line));

	Line.m_vFrom = rgvPoints[16];
	Line.m_vTo = rgvPoints[12];
	Line.m_sRight.Sector = s4;
	Check(AddLine(&Line));

	// Register the poly object for the cage door

	lpNew->iType = DOOR_CAGE;
	lpNew->bDir = LINE_ABOVE == nLine || LINE_BELOW == nLine;
	lpNew->iPoly = m_iNextPoly++;
	lpNew->pTexture = pCage->GetTexture();
	lpNew->Next = NULL;

	if(m_lpQueue)
	{
		LPPOLY_QUEUE lpList = m_lpQueue;
		while(lpList->Next)
			lpList = lpList->Next;
		lpList->Next = lpNew;
	}
	else
		m_lpQueue = lpNew;
	lpNew = NULL;

Cleanup:
	__delete lpNew;
	return hr;
}

HRESULT CMapConvert::BuildWadFile (PCSTR pcszMapName, ISeekableStream* pFile, CConfigDlg* pdlgConfig)
{
	HRESULT hr;
	if(pFile)
	{
		PCSTR pcszNames[] = {pcszMapName,"THINGS","LINEDEFS","SIDEDEFS","VERTEXES","SECTORS","BEHAVIOR","SCRIPTS"};
		PCSTR pcszName;
		DWORD dwTemp;
		LONG i, iSize, iOffset;
		HEADER Header;
		DIRECTORY Dir[10];
		LARGE_INTEGER li = {0};

		CopyMemory(Header.ID,"PWAD",4);

		Header.cLumps = 8;

		ZeroMemory(Dir,sizeof(DIRECTORY) * Header.cLumps);
		Header.iDir = 12;
		iOffset = sizeof(HEADER) + sizeof(DIRECTORY) * Header.cLumps;
		pFile->Write(&Header, sizeof(HEADER), &dwTemp);

		// Serialize map data
		// ...and build directory
		li.LowPart = iOffset;
		pFile->Seek(li, FILE_BEGIN, NULL);

		for(i = 0; i < Header.cLumps; i++)
		{
			pcszName = pcszNames[i];
			CopyMemory(Dir[i].Name, pcszName, lstrlenA(pcszName));
			switch(i)
			{
			case 0:	// MAP??
				iSize = 0;
				break;
			case 1:	// THINGS
				iSize = SerializeThings(pFile);
				break;
			case 2:	// LINEDEFS
				iSize = SerializeLinedefs(pFile);
				break;
			case 3:	// SIDEDEFS
				iSize = SerializeSidedefs(pFile);
				break;
			case 4:	// VERTEXES
				iSize = SerializeVertices(pFile);
				break;
			case 5:	// SECTORS
				iSize = SerializeSectors(pFile);
				break;
			case 6:	// BEHAVIOR
				iSize = SerializeLump(pFile, pdlgConfig->m_wzBehaviorPath, "BEHAVIOR.LMP");
				break;
			case 7:	// SCRIPTS
				iSize = SerializeLump(pFile, pdlgConfig->m_wzBehaviorPath, "SCRIPTS.LMP");
				break;
			}
			Dir[i].iOffset = iOffset;
			Dir[i].iSize = iSize;
			iOffset += iSize;
		}

		// Write directory
		li.LowPart = 12;
		hr = pFile->Seek(li, FILE_BEGIN, NULL);
		if(SUCCEEDED(hr))
			hr = pFile->Write(Dir, sizeof(DIRECTORY) * Header.cLumps, &dwTemp);

		if(SUCCEEDED(hr))
		{
			li.LowPart = 0;
			hr = pFile->Seek(li, FILE_BEGIN, NULL);
		}
	}
	else
		hr = E_INVALIDARG;
	return hr;
}

ULONG CMapConvert::SerializeThings (ISeekableStream* pFile)
{
	ULONG cSize = 0, dwWrite;
	LPTHING_LIST lpList = m_lpThings;
	while(lpList)
	{
		THING Thing;
		lpList->lpThing->GetMapThing(&Thing);
		pFile->Write(&Thing, sizeof(THING), &dwWrite);

		cSize += dwWrite;
		lpList = lpList->Next;
	}
	return cSize;
}

ULONG CMapConvert::SerializeLinedefs (ISeekableStream* pFile)
{
	ULONG cSize = 0, dwWrite;
	LPLINE_LIST lpList = m_lpList;
	CMapLine* lpLine;
	CMapLinedef LineDef;
	SHORT sSideDef = 0;
	while(lpList)
	{
		LINEDEF Linedef;

		lpLine = lpList->lpLine;
		LineDef.m_vFrom = FindVertex(&lpLine->m_vFrom);
		LineDef.m_vTo = FindVertex(&lpLine->m_vTo);
		LineDef.m_Flags = lpLine->m_Flags;
		LineDef.m_Special = lpLine->m_Special;
		CopyMemory(LineDef.m_Args,lpLine->m_Args,5);
		LineDef.m_sRight = sSideDef++;
		if(lpLine->m_Flags & 4)
			LineDef.m_sLeft = sSideDef++;
		else
			LineDef.m_sLeft = -1;
		LineDef.GetMapLinedef(&Linedef);
		pFile->Write(&Linedef, sizeof(LINEDEF), &dwWrite);
		cSize += dwWrite;
		lpList = lpList->Next;
	}
	return cSize;
}

ULONG CMapConvert::SerializeSidedefs (ISeekableStream* pFile)
{
	ULONG cSize = 0, dwWrite;
	LPLINE_LIST lpList = m_lpList;
	CMapLine* lpLine;
	while(lpList)
	{
		lpLine = lpList->lpLine;
		pFile->Write(&lpLine->m_sRight, sizeof(SIDEDEF), &dwWrite);
		cSize += dwWrite;
		if(lpLine->m_Flags & 4)
		{
			pFile->Write(&lpLine->m_sLeft, sizeof(SIDEDEF), &dwWrite);
			cSize += dwWrite;
		}
		lpList = lpList->Next;
	}
	return cSize;
}

ULONG CMapConvert::SerializeVertices (ISeekableStream* pFile)
{
	ULONG cSize = 0, dwWrite;
	LPVERTEX_LIST lpList = m_lpVertices;
	while(lpList)
	{
		pFile->Write(&lpList->Vertex, sizeof(VERTEX), &dwWrite);
		cSize += dwWrite;
		lpList = lpList->Next;
	}
	return cSize;
}

ULONG CMapConvert::SerializeSectors (ISeekableStream* pFile)
{
	ULONG cSize = 0, dwWrite;
	LPSECTOR_LIST lpList = m_lpSectors;
	while(lpList)
	{
		pFile->Write(&lpList->Sector, sizeof(SECTOR), &dwWrite);
		cSize += dwWrite;
		lpList = lpList->Next;
	}
	return cSize;
}

ULONG CMapConvert::SerializeLump (ISeekableStream* pFile, PCWSTR pcwzPath, LPSTR lpszFile)
{
	ULONG iSize = 0;
	CHAR szLumpPath[MAX_PATH];
	HANDLE hLump;

	Formatting::TPrintF(szLumpPath, ARRAYSIZE(szLumpPath), NULL, "%ls\\%hs", pcwzPath, lpszFile);
	hLump = CreateFileA(szLumpPath,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	if(hLump != INVALID_HANDLE_VALUE)
	{
		DWORD dwTemp;
		LPBYTE lpData;
		iSize = GetFileSize(hLump,NULL);
		lpData = new BYTE[iSize];
		if(lpData)
		{
			ReadFile(hLump,lpData,iSize,&dwTemp,NULL);
			CloseHandle(hLump);
			pFile->Write(lpData, iSize, &dwTemp);
			delete [] lpData;
		}
		else
		{
			CloseHandle(hLump);
			iSize = 0;
		}
	}
	return iSize;
}
