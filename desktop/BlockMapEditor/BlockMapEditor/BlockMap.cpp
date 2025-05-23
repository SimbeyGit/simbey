#include <windows.h>
#include <math.h>
#include "Library\Core\CoreDefs.h"
#include "Published\SimbeyCore.h"
#include "Published\JSON.h"
#include "PaintItems.h"
#include "BlockMap.h"

VOID CUndoRedoObjectFlags::Undo (class CBlockMap* pMap)
{
	pMap->InternalSetObjectFlags(m_idxCell, m_nOldObjectFlags);
}

VOID CUndoRedoObjectFlags::Redo (class CBlockMap* pMap)
{
	pMap->InternalSetObjectFlags(m_idxCell, m_nNewObjectFlags);
}

VOID CUndoRedoCellData::Undo (class CBlockMap* pMap)
{
	for(sysint i = 0; i < m_aOld.Length(); i++)
	{
		CELLDATA& data = m_aOld[i];
		pMap->InternalSetCellData(data.idxCell, data.pOld);
	}
}

VOID CUndoRedoCellData::Redo (class CBlockMap* pMap)
{
	for(sysint i = 0; i < m_aOld.Length(); i++)
		pMap->InternalSetCellData(m_aOld[i].idxCell, m_pNew);
}

CBlockMap::CBlockMap () :
	m_nLighting(192),
	m_nHighestFloor(0),
	m_pMap(NULL),
	m_idxChangePtr(0)
{
}

CBlockMap::~CBlockMap ()
{
	m_aChangeStack.DeleteAll();
	__delete_array m_pMap;
}

HRESULT CBlockMap::Initialize (VOID)
{
	HRESULT hr;

	m_pMap = __new MAP_BLOCK[MAP_WIDTH * MAP_HEIGHT];
	CheckAlloc(m_pMap);
	ZeroMemory(m_pMap, sizeof(MAP_BLOCK) * MAP_WIDTH * MAP_HEIGHT);

	for(INT i = 0; i < MAP_WIDTH * MAP_HEIGHT; i++)
		m_pMap[i].nObjectFlags = 7 | 0x0100 /* single player */;	// By default, objects are present for all difficulties

	hr = S_OK;

Cleanup:
	return hr;
}

VOID CBlockMap::Paint (IGrapher* pGraph)
{
	FLOAT x1, y1, x2, y2;

	pGraph->GetGraphBounds(&x1, &y1, &x2, &y2);
	x1 /= CELL_SCALE;
	y1 /= CELL_SCALE;
	x2 /= CELL_SCALE;
	y2 /= CELL_SCALE;

	x1 = floor(x1);
	y1 = floor(y1);
	x2 = ceil(x2);
	y2 = ceil(y2);

	INT xStart = (INT)x1 + MAP_WIDTH / 2;
	INT yStart = (INT)y1 + MAP_HEIGHT / 2;
	INT xEnd = (INT)x2 + MAP_WIDTH / 2;
	INT yEnd = (INT)y2 + MAP_HEIGHT / 2;

	if(xStart < 0)
		xStart = 0;
	if(yStart < 0)
		yStart = 0;
	if(xEnd > MAP_WIDTH)
		xEnd = MAP_WIDTH;
	if(yEnd > MAP_HEIGHT)
		yEnd = MAP_HEIGHT;

	MAP_BLOCK* pMap = m_pMap + yStart * MAP_WIDTH;

	FLOAT yPos = (FLOAT)(yStart - MAP_HEIGHT / 2) * CELL_SCALE;
	for(INT y = yStart; y < yEnd; y++)
	{
		FLOAT xPos = (FLOAT)(xStart - MAP_WIDTH / 2) * CELL_SCALE;
		for(INT x = xStart; x < xEnd; x++)
		{
			MAP_BLOCK* pCell = pMap + x;

			if(pCell->pBlock)
				pCell->pBlock->Paint(pGraph, xPos, yPos);
			if(pCell->pObject)
				pCell->pObject->Paint(pGraph, xPos, yPos);

			xPos += CELL_SCALE;
		}
		yPos += CELL_SCALE;

		pMap += MAP_WIDTH;
	}
}

HRESULT CBlockMap::Load (IResolveItemPalette* pResolve, PCWSTR pcwzFile, PWSTR pwzCeiling, PWSTR pwzFloor, PWSTR pwzCutout)
{
	HRESULT hr;
	TStackRef<CFileStream> srFile;
	DWORD cbPalette, cbData, cb;
	BYTE bFormat[4];
	TArray<CPaintItem*> aPalette;
	MAP_BLOCK* pMap = m_pMap;
	INT cch;

	m_aChangeStack.DeleteAll();
	m_idxChangePtr = 0;

	Check(CFileStream::Open(pcwzFile, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL, &srFile, NULL));
	Check(srFile->Read(&bFormat, sizeof(bFormat), &cb));

	if(bFormat[0] == 'J' && bFormat[1] == 'S' && bFormat[2] == 'O' && bFormat[3] == 'N')
		Check(ReadJSONProperties(srFile, pwzCeiling, pwzFloor, pwzCutout));
	else
	{
		// Old format
		CopyMemory(&m_nLighting, bFormat, sizeof(m_nLighting));

		Check(srFile->Read(&cch, sizeof(cch), &cb));
		if(0 < cb)
		{
			Check(srFile->Read(pwzCeiling, cch * sizeof(WCHAR), &cb));
			pwzCeiling[cch] = L'\0';
		}

		Check(srFile->Read(&cch, sizeof(cch), &cb));
		if(0 < cb)
		{
			Check(srFile->Read(pwzFloor, cch * sizeof(WCHAR), &cb));
			pwzFloor[cch] = L'\0';
		}
	}

	Check(srFile->Read(&cbPalette, sizeof(cbPalette), &cb));
	Check(LoadItemPalette(pResolve, srFile, cbPalette, aPalette));

	Check(srFile->Read(&cbData, sizeof(cbData), &cb));

	for(INT z = 0; z < MAP_HEIGHT; z++)
	{
		for(INT x = 0; x < MAP_WIDTH; x++)
		{
			DWORD rgBlocks[2];

			Check(srFile->Read(rgBlocks, sizeof(rgBlocks), &cb));

			if(rgBlocks[0] != (DWORD)-1)
				pMap->pBlock = aPalette[rgBlocks[0]];
			else
				pMap->pBlock = NULL;

			if(rgBlocks[1] != (DWORD)-1)
				pMap->pObject = aPalette[rgBlocks[1]];
			else
				pMap->pObject = NULL;

			Check(srFile->Read(&pMap->nObjectFlags, sizeof(pMap->nObjectFlags), &cb));

			pMap++;
		}
	}

Cleanup:
	return hr;
}

HRESULT CBlockMap::Save (PCWSTR pcwzFile, PCWSTR pcwzCeiling, PCWSTR pcwzFloor, PCWSTR pcwzCutout)
{
	HRESULT hr;
	MAP_BLOCK* pMap = m_pMap;
	BYTE bFormat[4];
	CMemoryStream stmProperties, stmPalette, stmPalItem;
	TMap<CPaintItem*, DWORD> mapPalette;
	TArray<CPaintItem*> aPalette;
	CMemoryStream stmData;
	TStackRef<CFileStream> srFile;
	DWORD cb;

	for(INT z = 0; z < MAP_HEIGHT; z++)
	{
		for(INT x = 0; x < MAP_WIDTH; x++)
		{
			Check(SerializeItem(mapPalette, aPalette, &stmData, pMap->pBlock));
			Check(SerializeItem(mapPalette, aPalette, &stmData, pMap->pObject));
			Check(stmData.Write(&pMap->nObjectFlags, sizeof(pMap->nObjectFlags), &cb));
			pMap++;
		}
	}

	Check(stmPalette.Write(&m_nHighestFloor, sizeof(m_nHighestFloor), &cb));

	for(sysint i = 0; i < aPalette.Length(); i++)
	{
		Check(aPalette[i]->Serialize(&stmPalItem));
		Check(InsertStream(&stmPalette, &stmPalItem));
		stmPalItem.Reset();
	}

	Check(CFileStream::Open(pcwzFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL, &srFile, NULL));

	bFormat[0] = 'J';
	bFormat[1] = 'S';
	bFormat[2] = 'O';
	bFormat[3] = 'N';
	Check(srFile->Write(bFormat, sizeof(bFormat), &cb));

	Check(SerializeProperties(&stmProperties, pcwzCeiling, pcwzFloor, pcwzCutout));
	Check(InsertStream(srFile, &stmProperties));

	Check(InsertStream(srFile, &stmPalette));
	Check(InsertStream(srFile, &stmData));

Cleanup:
	return hr;
}

USHORT CBlockMap::GetHighestFloor (VOID)
{
	return m_nHighestFloor;
}

USHORT CBlockMap::AddFloor (VOID)
{
	return ++m_nHighestFloor;
}

HRESULT CBlockMap::SetCellData (FLOAT x, FLOAT z, CPaintItem* pType)
{
	HRESULT hr;
	CUndoRedoCellData* pCellData = NULL;
	CUndoRedoCellData::CELLDATA* pData;

	x /= CELL_SCALE;
	z /= CELL_SCALE;

	x = floor(x);
	z = floor(z);

	INT xCell = (INT)x + MAP_WIDTH / 2;
	INT zCell = (INT)z + MAP_HEIGHT / 2;

	CheckIf(xCell < 0 || xCell >= MAP_WIDTH, DISP_E_BADINDEX);
	CheckIf(zCell < 0 || zCell >= MAP_HEIGHT, DISP_E_BADINDEX);

	INT idxCell = zCell * MAP_WIDTH + xCell;

	pCellData = __new CUndoRedoCellData(pType);
	CheckAlloc(pCellData);

	Check(pCellData->m_aOld.AppendSlot(&pData));
	pData->idxCell = idxCell;
	pData->pOld = InternalSetCellData(idxCell, pType);
	CheckIfIgnore(pData->pOld == pType, S_FALSE);

	Check(InsertUndoItem(pCellData));
	pCellData = NULL;

Cleanup:
	if(pCellData)
	{
		pCellData->Undo(this);
		__delete pCellData;
	}
	return hr;
}

HRESULT CBlockMap::GetCellData (INT xCell, INT zCell, __deref_out CPaintItem** ppBlock, CPaintItem** ppObject)
{
	HRESULT hr;

	CheckIfIgnore(xCell < 0 || xCell >= MAP_WIDTH, DISP_E_BADINDEX);
	CheckIfIgnore(zCell < 0 || zCell >= MAP_HEIGHT, DISP_E_BADINDEX);

	INT idxCell = zCell * MAP_WIDTH + xCell;
	SetInterface(*ppBlock, m_pMap[idxCell].pBlock);
	SetInterface(*ppObject, m_pMap[idxCell].pObject);
	hr = S_OK;

Cleanup:
	return hr;
}

HRESULT CBlockMap::GetObjectFlags (INT xCell, INT zCell, __out SHORT* pnObjectFlags)
{
	HRESULT hr;

	CheckIfIgnore(xCell < 0 || xCell >= MAP_WIDTH, DISP_E_BADINDEX);
	CheckIfIgnore(zCell < 0 || zCell >= MAP_HEIGHT, DISP_E_BADINDEX);

	INT idxCell = zCell * MAP_WIDTH + xCell;
	*pnObjectFlags = m_pMap[idxCell].nObjectFlags;
	hr = S_OK;

Cleanup:
	return hr;
}

HRESULT CBlockMap::SetObjectFlags (INT xCell, INT zCell, SHORT nObjectFlags)
{
	HRESULT hr;
	CUndoRedoObjectFlags* pFlags = NULL;

	CheckIfIgnore(xCell < 0 || xCell >= MAP_WIDTH, DISP_E_BADINDEX);
	CheckIfIgnore(zCell < 0 || zCell >= MAP_HEIGHT, DISP_E_BADINDEX);

	INT idxCell = zCell * MAP_WIDTH + xCell;
	CheckIfIgnore(m_pMap[idxCell].nObjectFlags == nObjectFlags, S_FALSE);

	pFlags = __new CUndoRedoObjectFlags(idxCell, m_pMap[idxCell].nObjectFlags, nObjectFlags);
	CheckAlloc(pFlags);
	Check(InsertUndoItem(pFlags));
	pFlags = NULL;

	InternalSetObjectFlags(idxCell, nObjectFlags);

Cleanup:
	SafeDelete(pFlags);
	return hr;
}

BOOL CBlockMap::IsSolid (INT xCell, INT zCell)
{
	if(xCell < 0 || xCell >= MAP_WIDTH)
		return FALSE;
	if(zCell < 0 || zCell >= MAP_HEIGHT)
		return FALSE;

	INT idxCell = zCell * MAP_WIDTH + xCell;
	if(NULL == m_pMap[idxCell].pBlock)
		return FALSE;

	return m_pMap[idxCell].pBlock->GetType() != MapCell::Floor;
}

USHORT CBlockMap::GetFloor (INT xCell, INT zCell)
{
	if(xCell < 0 || xCell >= MAP_WIDTH)
		return 65535;
	if(zCell < 0 || zCell >= MAP_HEIGHT)
		return 65535;

	INT idxCell = zCell * MAP_WIDTH + xCell;
	if(NULL == m_pMap[idxCell].pBlock || MapCell::Floor != m_pMap[idxCell].pBlock->GetType())
		return 65535;

	return static_cast<CFloorItem*>(m_pMap[idxCell].pBlock)->GetFloor();
}

HRESULT CBlockMap::FindStartSpot (__out INT& xCell, __out INT& zCell)
{
	HRESULT hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	MAP_BLOCK* pMap = m_pMap;

	for(INT z = 0; z < MAP_HEIGHT; z++)
	{
		for(INT x = 0; x < MAP_WIDTH; x++)
		{
			if(pMap->pObject && MapCell::Start == pMap->pObject->GetType())
			{
				// If we find a second start spot, then that's an error.
				CheckIf(S_OK == hr, HRESULT_FROM_WIN32(ERROR_TOO_MANY_OPEN_FILES));
				xCell = x;
				zCell = z;
				hr = S_OK;
			}

			pMap++;
		}
	}

Cleanup:
	return hr;
}

HRESULT CBlockMap::ReplaceWall (CPaintItem* pOld, CPaintItem* pNew)
{
	HRESULT hr;
	CUndoRedoCellData* pCellData = __new CUndoRedoCellData(pNew);
	MAP_BLOCK* pMap = m_pMap;

	CheckAlloc(pCellData);

	for(INT z = 0; z < MAP_HEIGHT; z++)
	{
		for(INT x = 0; x < MAP_WIDTH; x++)
		{
			if(pMap->pBlock == pOld)
			{
				CUndoRedoCellData::CELLDATA* pData;

				Check(pCellData->m_aOld.AppendSlot(&pData));
				pData->idxCell = z * MAP_WIDTH + x;
				pData->pOld = pMap->pBlock;

				pMap->pBlock = pNew;
			}

			pMap++;
		}
	}

	CheckIf(0 == pCellData->m_aOld.Length(), S_FALSE);
	Check(InsertUndoItem(pCellData));
	pCellData = NULL;

Cleanup:
	if(pCellData)
	{
		pCellData->Undo(this);
		__delete pCellData;
	}
	return hr;
}

HRESULT CBlockMap::Undo (VOID)
{
	HRESULT hr;

	CheckIf(!CanUndo(), E_FAIL);
	m_aChangeStack[--m_idxChangePtr]->Undo(this);
	hr = S_OK;

Cleanup:
	return hr;
}

HRESULT CBlockMap::Redo (VOID)
{
	HRESULT hr;

	CheckIf(!CanRedo(), E_FAIL);
	m_aChangeStack[m_idxChangePtr++]->Redo(this);
	hr = S_OK;

Cleanup:
	return hr;
}

CPaintItem* CBlockMap::InternalSetCellData (INT idxCell, __in_opt CPaintItem* pType)
{
	CPaintItem* pOld = NULL;
	MapCell::Type eType = pType ? pType->GetType() : MapCell::Void;

	switch(eType)
	{
	case MapCell::Void:
		if(m_pMap[idxCell].pObject)
		{
			pOld = m_pMap[idxCell].pObject;
			m_pMap[idxCell].pObject = NULL;
		}
		else
		{
			pOld = m_pMap[idxCell].pBlock;
			m_pMap[idxCell].pBlock = NULL;
		}
		break;
	case MapCell::Wall:
	case MapCell::Floor:
	case MapCell::Elevator:
	case MapCell::WallCage:
		pOld = m_pMap[idxCell].pBlock;
		m_pMap[idxCell].pBlock = pType;
		break;
	case MapCell::Object:
	case MapCell::Door:
	case MapCell::Start:
	case MapCell::SecretDoor:
	case MapCell::End:
	case MapCell::Sky:
		pOld = m_pMap[idxCell].pObject;
		m_pMap[idxCell].pObject = pType;
		break;
	}
	return pOld;
}

VOID CBlockMap::InternalSetObjectFlags (INT idxCell, SHORT nObjectFlags)
{
	m_pMap[idxCell].nObjectFlags = nObjectFlags;
}

HRESULT CBlockMap::InsertUndoItem (CBaseUndoRedo* pItem)
{
	HRESULT hr;

	Check(m_aChangeStack.InsertAt(pItem, m_idxChangePtr));
	m_idxChangePtr++;

	while(m_aChangeStack.Length() > m_idxChangePtr)
	{
		CBaseUndoRedo* pRemoved;
		m_aChangeStack.Remove(m_aChangeStack.Length() - 1, &pRemoved);
		__delete pRemoved;
	}

Cleanup:
	return hr;
}

HRESULT CBlockMap::LoadItemPalette (IResolveItemPalette* pResolve, ISequentialStream* pPalette, DWORD cbPalette, TArray<CPaintItem*>& aPalette)
{
	HRESULT hr;
	PBYTE pbData = __new BYTE[cbPalette], pbPtr;
	DWORD cb;

	CheckAlloc(pbData);
	Check(pPalette->Read(pbData, cbPalette, &cb));
	pbPtr = pbData;

	CheckIf(cbPalette < sizeof(m_nHighestFloor), E_INVALIDARG);
	CopyMemory(&m_nHighestFloor, pbPtr, sizeof(m_nHighestFloor));
	pbPtr += sizeof(m_nHighestFloor);
	cbPalette -= sizeof(m_nHighestFloor);

	Check(pResolve->InitializePaintItems(this));

	while(sizeof(DWORD) < cbPalette)
	{
		TStackRef<CPaintItem> srItem;
		MapCell::Type eType;

		CopyMemory(&cb, pbPtr, sizeof(DWORD));
		pbPtr += sizeof(DWORD);
		cbPalette -= sizeof(DWORD);

		CheckIf(cbPalette < cb, E_UNEXPECTED);

		eType = (MapCell::Type)*pbPtr;
		pbPtr += sizeof(BYTE);
		cbPalette -= sizeof(BYTE);
		cb -= sizeof(BYTE);

		Check(pResolve->ResolveItemPalette(eType, pbPtr, cb, &srItem));
		Check(aPalette.Append(srItem));	// We're not holding a reference

		pbPtr += cb;
		cbPalette -= cb;
	}

Cleanup:
	__delete_array pbData;
	return hr;
}

HRESULT CBlockMap::SerializeProperties (CMemoryStream* pProperties, PCWSTR pcwzCeiling, PCWSTR pcwzFloor, PCWSTR pcwzCutout)
{
	HRESULT hr;
	TStackRef<IJSONObject> srProperties;
	TStackRef<IJSONValue> srv;
	CMemoryStream stmPropertiesW;

	Check(JSONCreateObject(&srProperties));

	Check(JSONCreateInteger(m_nLighting, &srv));
	Check(srProperties->AddValueW(L"lighting", srv));
	srv.Release();

	Check(JSONCreateStringW(pcwzCeiling, TStrLenAssert(pcwzCeiling), &srv));
	Check(srProperties->AddValueW(L"ceiling", srv));
	srv.Release();

	Check(JSONCreateStringW(pcwzFloor, TStrLenAssert(pcwzFloor), &srv));
	Check(srProperties->AddValueW(L"floor", srv));
	srv.Release();

	Check(JSONCreateStringW(pcwzCutout, TStrLenAssert(pcwzCutout), &srv));
	Check(srProperties->AddValueW(L"cutout", srv));
	srv.Release();

	Check(JSONSerializeObject(srProperties, &stmPropertiesW));
	Check(ScCopyWideToStreamA(CP_UTF8, pProperties, stmPropertiesW.TGetReadPtr<WCHAR>(), stmPropertiesW.TDataRemaining<WCHAR>(), -1));

Cleanup:
	return hr;
}

HRESULT CBlockMap::ReadJSONProperties (CFileStream* pFile, PWSTR pwzCeiling, PWSTR pwzFloor, PWSTR pwzCutout)
{
	HRESULT hr;
	DWORD cbJSON, cb;
	PSTR pszJSON = NULL;
	RSTRING rstrJSONW = NULL;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONObject> srProperties;

	Check(pFile->Read(&cbJSON, sizeof(cbJSON), &cb));
	pszJSON = __new CHAR[cbJSON + 1];
	CheckAlloc(pszJSON);
	Check(pFile->Read(pszJSON, cbJSON, &cb));
	pszJSON[cbJSON] = '\0';

	Check(RStrConvertToW(CP_UTF8, cbJSON, pszJSON, &rstrJSONW));
	Check(JSONParse(NULL, RStrToWide(rstrJSONW), RStrLen(rstrJSONW), &srv));
	Check(srv->GetObject(&srProperties));
	srv.Release();

	Check(srProperties->FindNonNullValueW(L"lighting", &srv));
	Check(srv->GetInteger(&m_nLighting));

	Check(CopyPropertyTo(srProperties, L"ceiling", pwzCeiling, 12));
	Check(CopyPropertyTo(srProperties, L"floor", pwzFloor, 12));
	Check(CopyPropertyTo(srProperties, L"cutout", pwzCutout, 12));

Cleanup:
	RStrRelease(rstrJSONW);
	__delete_array pszJSON;
	return hr;
}

HRESULT CBlockMap::CopyPropertyTo (IJSONObject* pProperties, PCWSTR pcwzName, __out_ecount(cchMaxTarget) PWSTR pwzTarget, INT cchMaxTarget)
{
	HRESULT hr;
	RSTRING rstrValueW = NULL;
	TStackRef<IJSONValue> srv;

	Check(pProperties->FindNonNullValueW(pcwzName, &srv));
	Check(srv->GetString(&rstrValueW));
	srv.Release();

	Check(RStrCopyToW(rstrValueW, cchMaxTarget, pwzTarget, NULL));

Cleanup:
	RStrRelease(rstrValueW);
	return hr;
}

HRESULT CBlockMap::InsertStream (ISequentialStream* pstmTarget, CMemoryStream* pstmSource)
{
	HRESULT hr;
	DWORD cbSize = pstmSource->DataRemaining(), cb;

	Check(pstmTarget->Write(&cbSize, sizeof(cbSize), &cb));
	Check(pstmTarget->Write(pstmSource->GetReadPtr(), cbSize, &cb));

Cleanup:
	return hr;
}

HRESULT CBlockMap::SerializeItem (TMap<CPaintItem*, DWORD>& mapPalette, TArray<CPaintItem*>& aPalette, CMemoryStream* pstmData, CPaintItem* pItem)
{
	HRESULT hr;
	DWORD idxPalette = (DWORD)-1, cb;

	if(pItem && FAILED(mapPalette.Find(pItem, &idxPalette)))
	{
		idxPalette = (DWORD)aPalette.Length();
		Check(aPalette.Append(pItem));
		Check(mapPalette.Add(pItem, idxPalette));
	}

	Check(pstmData->Write(&idxPalette, sizeof(idxPalette), &cb));

Cleanup:
	return hr;
}
