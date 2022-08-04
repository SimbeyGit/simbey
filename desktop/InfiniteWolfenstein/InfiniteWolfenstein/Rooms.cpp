#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Published\JSON.h"
#include "WallTextures.h"
#include "Rooms.h"

CRoom::CRoom ()
{
	m_rcExtents.left = INT_MAX;
	m_rcExtents.top = INT_MAX;
	m_rcExtents.right = INT_MIN;
	m_rcExtents.bottom = INT_MIN;
}

CRoom::~CRoom ()
{
	for(sysint i = 0; i < m_aBlocks.Length(); i++)
		SafeRelease(m_aBlocks[i].pEntity);
}

BOOL CRoom::CheckBounds (INT xPlace, INT zPlace, INT xOffset, INT zOffset)
{
	INT x = xPlace + (m_rcExtents.left - xOffset);
	INT z = zPlace + (m_rcExtents.top - zOffset);
	if(x < 0 || z < 0)
		return FALSE;

	x = xPlace + (m_rcExtents.right - xOffset);
	z = zPlace + (m_rcExtents.bottom - zOffset);
	if(x >= REGION_WIDTH || z >= REGION_WIDTH)
		return FALSE;

	return TRUE;
}

CRooms::CRooms ()
{
}

CRooms::~CRooms ()
{
	m_mapRooms.DeleteAll();
}

HRESULT CRooms::Add (CWallTextures* pTextures, IJSONArray* pRooms)
{
	HRESULT hr = S_FALSE;
	RSTRING rstrNameW = NULL;
	sysint cRooms = pRooms->Count();

	for(sysint i = 0; i < cRooms; i++)
	{
		TStackRef<IJSONObject> srRoom;
		TStackRef<IJSONValue> srv;
		TStackRef<IJSONArray> srPalette, srData;

		Check(pRooms->GetObject(i, &srRoom));
		Check(srRoom->FindNonNullValueW(L"name", &srv));
		Check(srv->GetString(&rstrNameW));
		srv.Release();

		Check(srRoom->FindNonNullValueW(L"palette", &srv));
		Check(srv->GetArray(&srPalette));
		srv.Release();

		Check(srRoom->FindNonNullValueW(L"data", &srv));
		Check(srv->GetArray(&srData));
		Check(AddRoom(pTextures, rstrNameW, srPalette, srData));
		srv.Release();

		if(SUCCEEDED(srRoom->FindNonNullValueW(L"rotation", &srv)))
		{
			bool fRotation;
			Check(srv->GetBoolean(&fRotation));
			if(fRotation)
				Check(RotateAndAdd(pTextures, rstrNameW, srPalette, srData));
		}

		RStrRelease(rstrNameW); rstrNameW = NULL;
	}

Cleanup:
	RStrRelease(rstrNameW);
	return hr;
}

HRESULT CRooms::Find (RSTRING rstrRoomW, __deref_out CRoom** ppRoom)
{
	return m_mapRooms.Find(rstrRoomW, ppRoom);
}

HRESULT CRooms::AddRoom (CWallTextures* pTextures, RSTRING rstrNameW, IJSONArray* pPalette, IJSONArray* pData)
{
	HRESULT hr;
	RSTRING rstrTypeW = NULL;
	sysint cData = pData->Count();
	CRoom* pRoom = __new CRoom;

	CheckAlloc(pRoom);

	for(sysint i = 0; i < cData; i++)
	{
		TStackRef<IJSONObject> srData, srType;
		TStackRef<IJSONValue> srvType, srvX, srvZ, srvEntity;
		sysint idxType;
		ROOM_BLOCK block;

		Check(pData->GetObject(i, &srData));
		Check(srData->FindNonNullValueW(L"type", &srvType));
		Check(srData->FindNonNullValueW(L"x", &srvX));
		Check(srData->FindNonNullValueW(L"z", &srvZ));

		Check(srvType->GetInteger(&idxType));
		Check(srvX->GetInteger(&block.x));
		Check(srvZ->GetInteger(&block.z));

		Check(pPalette->GetObject(idxType, &srType));

		srvType.Release();
		Check(srType->FindNonNullValueW(L"type", &srvType));
		Check(srvType->GetString(&rstrTypeW));
		Check(pTextures->Resolve(rstrTypeW, &block.idxBlock));
		RStrRelease(rstrTypeW); rstrTypeW = NULL;

		if(TYPE_ANCHOR != block.idxBlock)
		{
			if(block.x < pRoom->m_rcExtents.left)
				pRoom->m_rcExtents.left = block.x;
			if(block.x > pRoom->m_rcExtents.right)
				pRoom->m_rcExtents.right = block.x;

			if(block.z < pRoom->m_rcExtents.top)
				pRoom->m_rcExtents.top = block.z;
			if(block.z > pRoom->m_rcExtents.bottom)
				pRoom->m_rcExtents.bottom = block.z;
		}

		if(TYPE_CONNECTOR == block.idxBlock)
			Check(pRoom->m_aConnectors.Append(pRoom->m_aBlocks.Length()));
		else if(TYPE_ANCHOR == block.idxBlock)
			Check(pRoom->m_aAnchors.Append(pRoom->m_aBlocks.Length()));

		if(SUCCEEDED(srData->FindNonNullValueW(L"entity", &srvEntity)))
			Check(srvEntity->GetObject(&block.pEntity));
		else
			block.pEntity = NULL;

		Check(pRoom->m_aBlocks.Append(block));
	}

	Check(m_mapRooms.Add(rstrNameW, pRoom));
	pRoom = NULL;

Cleanup:
	__delete pRoom;
	RStrRelease(rstrTypeW);
	return hr;
}

HRESULT CRooms::RotateAndAdd (CWallTextures* pTextures, RSTRING rstrNameW, IJSONArray* pPalette, IJSONArray* pData)
{
	HRESULT hr;
	RSTRING rstrRotatedW = NULL;
	TStackRef<IJSONArray> srRotated;
	sysint cBlocks;

	Check(RStrFormatW(&rstrRotatedW, L"%r-AutoRotated", rstrNameW));
	Check(JSONCreateArray(&srRotated));

	cBlocks = pData->Count();
	for(sysint i = 0; i < cBlocks; i++)
	{
		TStackRef<IJSONObject> srBlock, srNew;
		TStackRef<IJSONValue> srv;
		INT x, z;

		Check(pData->GetObject(i, &srBlock));
		Check(JSONCloneObject(srBlock, &srv, FALSE));
		Check(srv->GetObject(&srNew));
		Check(srRotated->Add(srv));

		srv.Release();
		Check(srBlock->FindNonNullValueW(L"x", &srv));
		Check(srv->GetInteger(&x));

		srv.Release();
		Check(srBlock->FindNonNullValueW(L"z", &srv));
		Check(srv->GetInteger(&z));

		srv.Release();
		Check(JSONCreateInteger(-z, &srv));
		Check(srNew->AddValueW(L"x", srv));

		srv.Release();
		Check(JSONCreateInteger(x, &srv));
		Check(srNew->AddValueW(L"z", srv));
	}

	Check(AddRoom(pTextures, rstrRotatedW, pPalette, srRotated));

Cleanup:
	RStrRelease(rstrRotatedW);
	return hr;
}
