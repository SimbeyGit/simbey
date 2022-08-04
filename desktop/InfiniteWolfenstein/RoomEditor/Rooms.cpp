#include <windows.h>
#include <SIFRibbon.h>
#include <StringGalleryItem.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "Library\Core\MemoryStream.h"
#include "Library\Util\TextHelpers.h"
#include "Library\GraphCtrl.h"
#include "Rooms.h"

#define	CELL_SCALE			16.0f

CRoom::CRoom () :
	m_rstrName(NULL),
	m_pCells(NULL),
	m_fEnableRotation(false)
{
}

CRoom::~CRoom ()
{
	for(sysint i = 0; i < m_aCells.Length(); i++)
		SafeRelease(m_aCells[i].pEntity);

	SafeRelease(m_pCells);
	RStrRelease(m_rstrName);
}

HRESULT CRoom::Initialize (RSTRING rstrName, IJSONArray* pPalette, IJSONArray* pData)
{
	HRESULT hr;

	RStrSet(m_rstrName, rstrName);
	Check(JSONCreateArray(&m_pCells));

	if(pData)
	{
		sysint cData = pData->Count();
		for(sysint i = 0; i < cData; i++)
		{
			TStackRef<IJSONObject> srData;

			Check(pData->GetObject(i, &srData));
			Check(CopyFromPalette(pPalette, srData));
		}
	}

Cleanup:
	return hr;
}

HRESULT CRoom::RemoveCell (INT x, INT z)
{
	HRESULT hr;
	sysint idxCell;

	CheckNoTrace(FindCell(x, z, &idxCell));
	SafeRelease(m_aCells[idxCell].pEntity);
	m_aCells.Remove(idxCell, NULL);
	Check(m_pCells->Remove(idxCell));

Cleanup:
	return hr;
}

HRESULT CRoom::AddCell (INT x, INT z, IJSONObject* pType)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONObject> srClone;
	sysint idxCell;
	PAINTABLE* pPaintable;

	Check(JSONCloneObject(pType, &srv, FALSE));
	Check(srv->GetObject(&srClone));

	if(SUCCEEDED(FindCell(x, z, &idxCell)))
	{
		pPaintable = m_aCells.GetItemPtr(idxCell);
		Check(m_pCells->Replace(idxCell, srv));
	}
	else
	{
		Check(m_aCells.AppendSlot(&pPaintable));
		pPaintable->x = x;
		pPaintable->z = z;
		Check(m_pCells->Add(srv));
	}

	srv.Release();
	Check(JSONCreateInteger(x, &srv));
	Check(srClone->AddValueW(L"x", srv));

	srv.Release();
	Check(JSONCreateInteger(z, &srv));
	Check(srClone->AddValueW(L"z", srv));

	srv.Release();
	Check(srClone->FindNonNullValueW(L"cr", &srv));
	Check(srv->GetDWord(&pPaintable->cr));

	// If there is an entity, add it back to the cloned type object.
	if(pPaintable->pEntity)
	{
		srv.Release();
		Check(JSONWrapObject(pPaintable->pEntity, &srv));
		Check(srClone->AddValueW(L"entity", srv));
	}

Cleanup:
	return hr;
}

HRESULT CRoom::SetEntity (INT x, INT z, IJSONObject* pEntity)
{
	HRESULT hr;
	sysint idxCell;
	TStackRef<IJSONObject> srCell, srEntity;
	TStackRef<IJSONValue> srv;

	Check(FindCell(x, z, &idxCell));
	Check(m_pCells->GetObject(idxCell, &srCell));

	Check(JSONCreateObject(&srEntity));

	Check(pEntity->FindNonNullValueW(L"entity", &srv));
	Check(srEntity->AddValueW(L"type", srv));
	srv.Release();
	Check(pEntity->FindNonNullValueW(L"cr", &srv));
	Check(srEntity->AddValueW(L"cr", srv));
	srv.Release();

	Check(JSONWrapObject(srEntity, &srv));
	Check(srCell->AddValueW(L"entity", srv));

	SafeRelease(m_aCells[idxCell].pEntity);
	m_aCells[idxCell].pEntity = srEntity.Detach();

Cleanup:
	return hr;
}

HRESULT CRoom::RemoveEntity (INT x, INT z)
{
	HRESULT hr;
	TStackRef<IJSONObject> srCell;
	sysint idxCell;

	Check(FindCell(x, z, &idxCell));
	Check(m_pCells->GetObject(idxCell, &srCell));

	Check(srCell->RemoveValueW(L"entity"));
	SafeRelease(m_aCells[idxCell].pEntity);

Cleanup:
	return hr;
}

HRESULT CRoom::Serialize (__deref_out IJSONValue** ppvRoom)
{
	HRESULT hr;
	TStackRef<IJSONObject> srRoom;
	TStackRef<IJSONArray> srPalette, srData;
	TStackRef<IJSONValue> srv;
	sysint cCells = m_pCells->Count();
	RSTRING rstrTypeName = NULL;

	Check(RStrCreateW(LSP(L"type"), &rstrTypeName));

	Check(JSONCreateObject(&srRoom));
	Check(JSONCreateArray(&srPalette));
	Check(JSONCreateArray(&srData));

	Check(JSONCreateString(m_rstrName, &srv));
	Check(srRoom->AddValueW(L"name", srv));
	srv.Release();

	Check(JSONWrapArray(srPalette, &srv));
	Check(srRoom->AddValueW(L"palette", srv));
	srv.Release();

	Check(JSONWrapArray(srData, &srv));
	Check(srRoom->AddValueW(L"data", srv));

	for(sysint i = 0; i < cCells; i++)
	{
		TStackRef<IJSONObject> srCell, srClone;

		srv.Release();
		Check(m_pCells->GetObject(i, &srCell));
		Check(JSONCloneObject(srCell, &srv, FALSE));
		Check(srv->GetObject(&srClone));
		Check(srData->Add(srv));
		Check(ReformatToPalette(rstrTypeName, srClone, srPalette));
	}

	if(m_fEnableRotation)
	{
		srv.Release();
		Check(JSONParse(NULL, SLP(L"true"), &srv));
		Check(srRoom->AddValueW(L"rotation", srv));
	}

	Check(JSONWrapObject(srRoom, ppvRoom));

Cleanup:
	RStrRelease(rstrTypeName);
	return hr;
}

HRESULT CRoom::GetCellData (INT x, INT z, __deref_out IJSONObject** ppCell, __deref_out IJSONObject** ppEntity)
{
	HRESULT hr;
	sysint idxCell;

	Check(FindCell(x, z, &idxCell));
	Check(m_pCells->GetObject(idxCell, ppCell));
	SetInterface(*ppEntity, m_aCells[idxCell].pEntity);

Cleanup:
	return hr;
}

HRESULT CRoom::FindCell (INT x, INT z, __out sysint* pidxCell)
{
	for(sysint i = 0; i < m_aCells.Length(); i++)
	{
		PAINTABLE& p = m_aCells[i];
		if(p.x == x && p.z == z)
		{
			*pidxCell = i;
			return S_OK;
		}
	}
	return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
}

HRESULT CRoom::CopyFromPalette (IJSONArray* pPalette, IJSONObject* pData)
{
	HRESULT hr;
	RSTRING rstrType = NULL;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONObject> srCell, srPalette;
	DWORD idxPalette;
	PAINTABLE* pPaintable;

	Check(m_aCells.AppendSlot(&pPaintable));

	Check(pData->FindNonNullValueW(L"type", &srv));
	Check(srv->GetDWord(&idxPalette));
	srv.Release();

	Check(pData->FindNonNullValueW(L"x", &srv));
	Check(srv->GetInteger(&pPaintable->x));
	srv.Release();

	Check(pData->FindNonNullValueW(L"z", &srv));
	Check(srv->GetInteger(&pPaintable->z));
	srv.Release();

	if(SUCCEEDED(pData->FindNonNullValueW(L"entity", &srv)))
	{
		Check(srv->GetObject(&pPaintable->pEntity));
		srv.Release();
	}

	Check(JSONCloneObject(pData, &srv, FALSE));
	Check(srv->GetObject(&srCell));
	Check(m_pCells->Add(srv));
	srv.Release();

	// Copy the palette items ("type" and "cr") to the cell object.
	Check(pPalette->GetValue(idxPalette, &srv));
	Check(srv->GetObject(&srPalette));
	Check(JSONMergeObject(srCell, srPalette));
	srv.Release();

	Check(srPalette->FindNonNullValueW(L"cr", &srv));
	Check(srv->GetDWord(&pPaintable->cr));

Cleanup:
	RStrRelease(rstrType);
	return hr;
}

HRESULT CRoom::ReformatToPalette (RSTRING rstrTypeName, IJSONObject* pData, IJSONArray* pPalette)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv, srvColor;
	TStackRef<IJSONObject> srEntry;
	RSTRING rstrType = NULL;
	sysint idxPalette;

	Check(pData->FindNonNullValue(rstrTypeName, &srv));
	Check(srv->GetString(&rstrType));

	Check(pData->RemoveValueW(L"cr", &srvColor));

	if(FAILED(JSONFindArrayObject(pPalette, rstrTypeName, rstrType, &srEntry, &idxPalette)))
	{
		Check(JSONCreateObject(&srEntry));
		Check(srEntry->AddValue(rstrTypeName, srv));
		Check(srEntry->AddValueW(L"cr", srvColor));

		srv.Release();
		Check(JSONWrapObject(srEntry, &srv));
		idxPalette = pPalette->Count();
		Check(pPalette->Add(srv));
	}

	srv.Release();
	Check(JSONCreateInteger(idxPalette, &srv));
	Check(pData->AddValue(rstrTypeName, srv));

Cleanup:
	RStrRelease(rstrType);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////

CRooms::CRooms () :
	m_pSelected(NULL),
	m_hbrConnector(NULL),
	m_hbrAnchor(NULL),
	m_hbrFloor(NULL),
	m_hbrWall(NULL),
	m_hbrDecoration(NULL),
	m_hbrDoor(NULL),
	m_hbrElevatorDoor(NULL),
	m_hbrGoldLocked(NULL),
	m_hbrSilverLocked(NULL),
	m_hbrRailing(NULL),
	m_hbrSwitch(NULL)
{
}

CRooms::~CRooms ()
{
	m_aRooms.DeleteAll();

	DeleteObject(m_hbrTunnelStart);
	DeleteObject(m_hbrConnector);
	DeleteObject(m_hbrAnchor);
	DeleteObject(m_hbrFloor);
	DeleteObject(m_hbrWall);
	DeleteObject(m_hbrDecoration);
	DeleteObject(m_hbrDoor);
	DeleteObject(m_hbrElevatorDoor);
	DeleteObject(m_hbrGoldLocked);
	DeleteObject(m_hbrSilverLocked);
	DeleteObject(m_hbrRailing);
	DeleteObject(m_hbrSwitch);
}

HRESULT CRooms::Initialize (VOID)
{
	HRESULT hr;

	m_hbrTunnelStart = CreateSolidBrush(COLOR_TUNNEL_START);
	CheckIfGetLastError(NULL == m_hbrTunnelStart);

	m_hbrConnector = CreateSolidBrush(COLOR_CONNECTOR);
	CheckIfGetLastError(NULL == m_hbrConnector);

	m_hbrAnchor = CreateSolidBrush(COLOR_ANCHOR);
	CheckIfGetLastError(NULL == m_hbrAnchor);

	m_hbrFloor = CreateSolidBrush(COLOR_FLOOR);
	CheckIfGetLastError(NULL == m_hbrFloor);

	m_hbrWall = CreateSolidBrush(COLOR_WALL);
	CheckIfGetLastError(NULL == m_hbrWall);

	m_hbrDecoration = CreateSolidBrush(COLOR_DECORATION);
	CheckIfGetLastError(NULL == m_hbrDecoration);

	m_hbrDoor = CreateSolidBrush(COLOR_DOOR);
	CheckIfGetLastError(NULL == m_hbrDoor);

	m_hbrElevatorDoor = CreateSolidBrush(COLOR_ELEVATOR_DOOR);
	CheckIfGetLastError(NULL == m_hbrElevatorDoor);

	m_hbrGoldLocked = CreateSolidBrush(COLOR_GOLD_LOCKED);
	CheckIfGetLastError(NULL == m_hbrGoldLocked);

	m_hbrSilverLocked = CreateSolidBrush(COLOR_SILVER_LOCKED);
	CheckIfGetLastError(NULL == m_hbrSilverLocked);

	m_hbrRailing = CreateSolidBrush(COLOR_RAILING);
	CheckIfGetLastError(NULL == m_hbrRailing);

	m_hbrSwitch = CreateSolidBrush(COLOR_SWITCH);
	CheckIfGetLastError(NULL == m_hbrSwitch);

	hr = S_OK;

Cleanup:
	return hr;
}

HRESULT CRooms::AddRoom (__out sysint* pidxRoom)
{
	HRESULT hr;
	CRoom* pRoom;
	RSTRING rstrUnnamed = NULL;

	*pidxRoom = m_aRooms.Length();

	Check(m_aRooms.AppendNew(&pRoom));
	Check(RStrCreateW(LSP(L"Unnamed"), &rstrUnnamed));
	Check(pRoom->Initialize(rstrUnnamed, NULL, NULL));

Cleanup:
	RStrRelease(rstrUnnamed);
	return hr;
}

HRESULT CRooms::DeleteRoom (sysint idxRoom)
{
	HRESULT hr;
	CRoom* pRoom;

	Check(m_aRooms.RemoveChecked(idxRoom, &pRoom));
	__delete pRoom;

Cleanup:
	return hr;
}

HRESULT CRooms::SelectRoom (sysint idxRoom)
{
	HRESULT hr;

	if(-1 == idxRoom)
		m_pSelected = NULL;
	else
	{
		CheckIf(idxRoom >= m_aRooms.Length(), HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
		m_pSelected = m_aRooms[idxRoom];
	}
	hr = S_OK;

Cleanup:
	return hr;
}

HRESULT CRooms::GetRoomIndex (__out sysint* pidxRoom)
{
	HRESULT hr;

	CheckIfIgnore(NULL == m_pSelected, E_FAIL);
	CheckIfIgnore(!m_aRooms.IndexOf(m_pSelected, *pidxRoom), E_FAIL);
	hr = S_OK;

Cleanup:
	return hr;
}

HRESULT CRooms::UpdateSelectionData (CSIFRibbon* pRibbon, REFPROPERTYKEY key, const PROPVARIANT* currentValue, PROPVARIANT* newValue)
{
	HRESULT hr = E_NOTIMPL;

	if(UI_PKEY_SelectedItem == key)
	{
		sysint idxRoom;
		Check(GetRoomIndex(&idxRoom));
		newValue->ulVal = idxRoom;
		newValue->vt = VT_UI4;
	}
	else if(UI_PKEY_ItemsSource == key)
	{
		TStackRef<IUICollection> srCollection;

		CheckIf(NULL == currentValue || VT_UNKNOWN != currentValue->vt, E_UNEXPECTED);
		Check(currentValue->punkVal->QueryInterface(&srCollection));
		Check(srCollection->Clear());

		for(sysint i = 0; i < m_aRooms.Length(); i++)
		{
			TStackRef<CStringGalleryItem> srItem;

			Check(CStringGalleryItem::Create(pRibbon, &srItem));
			Check(srItem->SetItemText(RStrToWide(m_aRooms[i]->m_rstrName)));
			Check(srCollection->Add(static_cast<IUISimplePropertySet*>(srItem)));
		}
	}

Cleanup:
	return hr;
}

BOOL CRooms::Paint (IGrapher* pGrapher)
{
	BOOL fPainted = FALSE;

	if(m_pSelected)
	{
		TArray<PAINTABLE>& aCells = m_pSelected->m_aCells;

		COLORREF crPrev = 0xFFFFFFFF;
		HBRUSH hbrPrev = NULL;

		for(sysint i = 0; i < aCells.Length(); i++)
		{
			PAINTABLE& p = aCells[i];
			HBRUSH hbrFill = NULL;

			FLOAT rx = (FLOAT)p.x * CELL_SCALE;
			FLOAT rz = (FLOAT)p.z * CELL_SCALE;

			switch(p.cr)
			{
			case COLOR_TUNNEL_START:
				hbrFill = m_hbrTunnelStart;
				break;
			case COLOR_CONNECTOR:
				hbrFill = m_hbrConnector;
				break;
			case COLOR_ANCHOR:
				hbrFill = m_hbrAnchor;
				break;
			case COLOR_FLOOR:
				hbrFill = m_hbrFloor;
				break;
			case COLOR_WALL:
				hbrFill = m_hbrWall;
				break;
			case COLOR_DECORATION:
				hbrFill = m_hbrDecoration;
				break;
			case COLOR_DOOR:
				hbrFill = m_hbrDoor;
				break;
			case COLOR_ELEVATOR_DOOR:
				hbrFill = m_hbrElevatorDoor;
				break;
			case COLOR_GOLD_LOCKED:
				hbrFill = m_hbrGoldLocked;
				break;
			case COLOR_SILVER_LOCKED:
				hbrFill = m_hbrSilverLocked;
				break;
			case COLOR_RAILING:
				hbrFill = m_hbrRailing;
				break;
			case COLOR_SWITCH:
				hbrFill = m_hbrSwitch;
				break;
			default:
				if(p.cr != crPrev)
				{
					if(hbrPrev)
						DeleteObject(hbrPrev);
					hbrPrev = CreateSolidBrush(p.cr);
					if(hbrPrev)
						crPrev = p.cr;
				}
				hbrFill = hbrPrev;
				break;
			}

			pGrapher->FillRect(rx, 0.0f, rz, rx + CELL_SCALE, 0.0f, rz + CELL_SCALE, hbrFill);

			if(p.pEntity)
			{
				TStackRef<IJSONValue> srv;
				COLORREF cr;

				if(SUCCEEDED(p.pEntity->FindNonNullValueW(L"cr", &srv)) && SUCCEEDED(srv->GetDWord(&cr)))
				{
					HBRUSH hbrTemp = pGrapher->SelectBrush(CreateSolidBrush(cr));
					pGrapher->Ellipse(rx + 4.0f, 0.0f, rz + 4.0f, rx + CELL_SCALE - 4.0f, 0.0f, rz + CELL_SCALE - 4.0f);
					DeleteObject(pGrapher->SelectBrush(hbrTemp));
				}
			}
		}

		if(hbrPrev)
			DeleteObject(hbrPrev);

		fPainted = TRUE;
	}

	return fPainted;
}

HRESULT CRooms::SetCellData (FLOAT rx, FLOAT rz, IJSONObject* pType)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	RSTRING rstrType = NULL;

	CheckIfIgnore(NULL == m_pSelected, E_FAIL);

	Check(pType->FindNonNullValueW(L"type", &srv));
	Check(srv->GetString(&rstrType));

	INT x = (INT)(rx / CELL_SCALE);
	if(rx < 0.0f)
		x--;
	INT z = (INT)(rz / CELL_SCALE);
	if(rz < 0.0f)
		z--;

	if(0 == TStrCmpAssert(RStrToWide(rstrType), L"entity"))
		Check(m_pSelected->SetEntity(x, z, pType));
	else if(0 == TStrCmpAssert(RStrToWide(rstrType), L"any.empty"))
		CheckNoTrace(m_pSelected->RemoveCell(x, z));
	else if(0 == TStrCmpAssert(RStrToWide(rstrType), L"any.delete_entity"))
		CheckNoTrace(m_pSelected->RemoveEntity(x, z));
	else
		Check(m_pSelected->AddCell(x, z, pType));

Cleanup:
	RStrRelease(rstrType);
	return hr;
}

HRESULT CRooms::GetCellData (INT x, INT z, __deref_out IJSONObject** ppCell, __deref_out IJSONObject** ppEntity)
{
	HRESULT hr;

	CheckIf(NULL == m_pSelected, E_UNEXPECTED);
	Check(m_pSelected->GetCellData(x, z, ppCell, ppEntity));

Cleanup:
	return hr;
}

HRESULT CRooms::Load (PCWSTR pcwzFile)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONArray> srArray;
	PWSTR pwzJSON = NULL;
	INT cchJSON;
	sysint cRooms;
	RSTRING rstrName = NULL;

	Check(Text::LoadFromFile(pcwzFile, &pwzJSON, &cchJSON));
	Check(JSONParse(NULL, pwzJSON, cchJSON, &srv));
	Check(srv->GetArray(&srArray));

	cRooms = srArray->Count();
	for(sysint i = 0; i < cRooms; i++)
	{
		TStackRef<IJSONObject> srRoom;

		Check(srArray->GetObject(i, &srRoom));

		srv.Release();
		Check(srRoom->FindNonNullValueW(L"name", &srv));
		Check(srv->GetString(&rstrName));

		Check(AddRoom(rstrName, srRoom, NULL));
		RStrRelease(rstrName); rstrName = NULL;
	}

	if(0 < cRooms)
		m_pSelected = m_aRooms[0];

Cleanup:
	RStrRelease(rstrName);
	__delete_array pwzJSON;
	return hr;
}

HRESULT CRooms::AddRoom (RSTRING rstrName, IJSONObject* pRoomDef, __out_opt sysint* pidxRoom)
{
	HRESULT hr;
	TStackRef<IJSONArray> srPalette, srData;
	TStackRef<IJSONValue> srv;
	CRoom* pRoom;

	srv.Release();
	Check(pRoomDef->FindNonNullValueW(L"palette", &srv));
	Check(srv->GetArray(&srPalette));

	srv.Release();
	Check(pRoomDef->FindNonNullValueW(L"data", &srv));
	Check(srv->GetArray(&srData));

	if(pidxRoom)
		*pidxRoom = m_aRooms.Length();

	Check(m_aRooms.AppendNew(&pRoom));
	Check(pRoom->Initialize(rstrName, srPalette, srData));

	srv.Release();
	if(SUCCEEDED(pRoomDef->FindNonNullValueW(L"rotation", &srv)))
		Check(srv->GetBoolean(&pRoom->m_fEnableRotation));

Cleanup:
	return hr;
}

HRESULT CRooms::Save (PCWSTR pcwzFile)
{
	HRESULT hr;
	TStackRef<IJSONArray> srRooms;
	CMemoryStream stmJSON;

	Check(JSONCreateArray(&srRooms));

	for(sysint i = 0; i < m_aRooms.Length(); i++)
	{
		TStackRef<IJSONValue> srvRoom;

		Check(m_aRooms[i]->Serialize(&srvRoom));
		Check(srRooms->Add(srvRoom));
	}

	Check(JSONSerializeArray(srRooms, &stmJSON));
	Check(Text::SaveToFile(stmJSON.TGetReadPtr<WCHAR>(), stmJSON.TDataRemaining<WCHAR>(), pcwzFile));

Cleanup:
	return hr;
}
