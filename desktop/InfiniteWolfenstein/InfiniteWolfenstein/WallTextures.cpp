#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\RectPlacement.h"
#include "3rdParty\glew\include\GL\glew.h"
#include "Published\SIFGL.h"
#include "Package\SIFPackage.h"
#include "TemplateData.h"
#include "WallTextures.h"

///////////////////////////////////////////////////////////////////////////////
// CWallNamespace
///////////////////////////////////////////////////////////////////////////////

CWallNamespace::CWallNamespace (RSTRING rstrNamespace, ISimbeyInterchangeFile* pSIF) :
	m_pSIF(pSIF)
{
	RStrSet(m_rstrNamespace, rstrNamespace);
	if(m_pSIF)
		m_pSIF->AddRef();
}

CWallNamespace::~CWallNamespace ()
{
	if(m_pSIF)
	{
		m_pSIF->Close();
		SafeRelease(m_pSIF);
	}
	RStrRelease(m_rstrNamespace);
}

HRESULT CWallNamespace::Add (RSTRING rstrTypeW, sysint idxType)
{
	return m_mapWalls.Add(rstrTypeW, idxType);
}

HRESULT CWallNamespace::Find (RSTRING rstrTextureW, __out sysint* pidxType)
{
	return m_mapWalls.Find(rstrTextureW, pidxType);
}

HRESULT CWallNamespace::GetWalls (__deref_out ISimbeyInterchangeFile** ppSIF, __out TArray<sysint>& aWalls)
{
	HRESULT hr = S_OK;

	aWalls.Clear();

	for(sysint i = 0; i < m_mapWalls.Length(); i++)
		Check(aWalls.Append(*m_mapWalls.GetValuePtr(i)));

	SetInterface(*ppSIF, m_pSIF);

Cleanup:
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
// CWallTextures
///////////////////////////////////////////////////////////////////////////////

CWallTextures::CWallTextures () :
	m_nTextures(0)
{
}

CWallTextures::~CWallTextures ()
{
	if(m_nTextures)
		glDeleteTextures(1, &m_nTextures);

	m_mapNamespaces.DeleteAll();
}

HRESULT CWallTextures::Initialize (VOID)
{
	HRESULT hr;
	WALL_TEXTURE texture = {0};
	RSTRING rstrAnyW = NULL;
	CWallNamespace* pNamespace = NULL;
	bool fAdded = false;

	Check(RStrCreateW(LSP(L"any"), &rstrAnyW));

	pNamespace = __new CWallNamespace(rstrAnyW, NULL);
	CheckAlloc(pNamespace);

	Check(AddType(pNamespace, TYPE_EMPTY, SLP(L"empty"), texture));
	Check(AddType(pNamespace, TYPE_TUNNEL_START, SLP(L"tunnel-start"), texture));
	Check(AddType(pNamespace, TYPE_CONNECTOR, SLP(L"connector"), texture));
	Check(AddType(pNamespace, TYPE_ANCHOR, SLP(L"anchor"), texture));
	Check(AddType(pNamespace, TYPE_ANY_FLOOR, SLP(L"floor"), texture));
	Check(AddType(pNamespace, TYPE_ANY_WALL, SLP(L"wall"), texture));
	Check(AddType(pNamespace, TYPE_ANY_DECORATION, SLP(L"decoration"), texture));
	Check(AddType(pNamespace, TYPE_ANY_DOOR, SLP(L"door"), texture));
	Check(AddType(pNamespace, TYPE_ANY_ELEVATOR_DOOR, SLP(L"elevator-door"), texture));
	Check(AddType(pNamespace, TYPE_ANY_LOCKED_DOOR_GOLD, SLP(L"locked-gold"), texture));
	Check(AddType(pNamespace, TYPE_ANY_LOCKED_DOOR_SILVER, SLP(L"locked-silver"), texture));
	Check(AddType(pNamespace, TYPE_ANY_RAILING, SLP(L"railing"), texture));
	Check(AddType(pNamespace, TYPE_ANY_SWITCH, SLP(L"switch"), texture));

	Check(m_mapNamespaces.Add(rstrAnyW, pNamespace));
	fAdded = true;

Cleanup:
	if(!fAdded)
		__delete pNamespace;
	RStrRelease(rstrAnyW);
	return hr;
}

HRESULT CWallTextures::Load (CSIFPackage* pPackage, BOOL fRequired)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONObject> srData;
	TStackRef<IJSONArray> srDefs;
	CWallNamespace* pNamespace = NULL;

	Check(LoadTextures(pPackage, fRequired, SLP(L"walls"), SLP(L"walls.json"), &srData, &pNamespace));

	if(SUCCEEDED(srData->FindNonNullValueW(L"doors", &srv)))
	{
		Check(srv->GetArray(&srDefs));
		Check(ParseDoors(pNamespace, srDefs)); srDefs.Release();
		srv.Release();
	}

	if(SUCCEEDED(srData->FindNonNullValueW(L"switches", &srv)))
	{
		Check(srv->GetArray(&srDefs));
		Check(ParseSwitches(pNamespace, srDefs)); srDefs.Release();
		srv.Release();
	}

	if(SUCCEEDED(srData->FindNonNullValueW(L"railing", &srv)))
	{
		Check(srv->GetArray(&srDefs));
		Check(ParseRailing(pNamespace, srDefs)); srDefs.Release();
		srv.Release();
	}

	srData.Release();

	Check(LoadTextures(pPackage, fRequired, SLP(L"flats"), SLP(L"flats.json"), &srData, &pNamespace));

Cleanup:
	return hr;
}

HRESULT CWallTextures::RebuildTextures (VOID)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeFile> srSIF;
	CRectPlacement Packer;

	if(0 != m_nTextures)
	{
		glDeleteTextures(1, &m_nTextures);
		m_nTextures = 0;
	}

	Check(Packer.Init(1024, 1024));
	Check(sifCreateNew(&srSIF));
	Check(srSIF->SetCanvasPixelSize(1024));

	for(sysint i = 0; i < m_mapNamespaces.Length(); i++)
	{
		CWallNamespace* pNamespace = *m_mapNamespaces.GetValuePtr(i);
		TStackRef<ISimbeyInterchangeFile> srWalls;
		TArray<sysint> aWalls;

		Check(pNamespace->GetWalls(&srWalls, aWalls));
		if(srWalls)
		{
			for(sysint n = 0; n < aWalls.Length(); n++)
			{
				WALL_TEXTURE& texture = m_aWalls[aWalls[n]];
				TStackRef<ISimbeyInterchangeFileLayer> srLayer, srPacked;
				RECT rcPos;
				CRectPlacement::POINTSIZE psRect;

				Assert(texture.idxLayer < srWalls->GetLayerCount());

				Check(srWalls->GetLayerByIndex(texture.idxLayer, &srLayer));
				Check(srLayer->GetPosition(&rcPos));

				psRect.x = 0;
				psRect.y = 0;
				psRect.cx = rcPos.right - rcPos.left;
				psRect.cy = rcPos.bottom - rcPos.top;
				CheckIf(!Packer.AddAtEmptySpotAutoGrow(&psRect, 0, 0), HRESULT_FROM_WIN32(ERROR_OUT_OF_STRUCTURES));
				Check(srSIF->ImportLayer(srLayer, &srPacked));
				srPacked->SetPosition(psRect.x, psRect.y);

				Check(sifGetGLTexturePositionF(srPacked, &texture.rcPos.left, &texture.rcPos.top, &texture.rcPos.right, &texture.rcPos.bottom));
			}
		}
	}

	Check(sifMergeCanvasToOpenGLTexture32(srSIF, 0, 0, 0, 0, &m_nTextures));

Cleanup:
	if(srSIF)
		srSIF->Close();
	return hr;
}

VOID CWallTextures::BindTexture (VOID)
{
	glBindTexture(GL_TEXTURE_2D, m_nTextures);
}

HRESULT CWallTextures::Resolve (RSTRING rstrTextureW, __out sysint* pidxType)
{
	HRESULT hr;
	RSTRING rstrNamespaceW = NULL, rstrNameW = NULL;
	CWallNamespace* pWallNamespace;

	Check(SplitKey(rstrTextureW, &rstrNamespaceW, &rstrNameW));
	Check(m_mapNamespaces.Find(rstrNamespaceW, &pWallNamespace));
	Check(pWallNamespace->Find(rstrNameW, pidxType));

Cleanup:
	RStrRelease(rstrNameW);
	RStrRelease(rstrNamespaceW);
	return hr;
}

HRESULT CWallTextures::GetPosition (sysint idxType, __out FRECT* pfrcPos)
{
	HRESULT hr;

	CheckIf(idxType >= m_aWalls.Length(), HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
	*pfrcPos = m_aWalls[idxType].rcPos;
	hr = S_OK;

Cleanup:
	return hr;
}

HRESULT CWallTextures::LoadTextures (CSIFPackage* pPackage, BOOL fRequired, PCWSTR pcwzDir, INT cchDir, PCWSTR pcwzDefs, INT cchDefs, __deref_out IJSONObject** ppData, __deref_out CWallNamespace** ppNamespace)
{
	HRESULT hr;
	TStackRef<CSIFPackage> srDir;
	TStackRef<ISimbeyInterchangeFile> srSIF;
	TStackRef<IJSONObject> srData;
	TStackRef<IJSONValue> srv;
	RSTRING rstrSIF = NULL, rstrNamespaceW = NULL;
	CWallNamespace* pNamespace = NULL;
	bool fAdded = false;

	hr = pPackage->OpenDirectory(pcwzDir, cchDir, &srDir);
	if(FAILED(hr))
	{
		CheckIf(!fRequired, S_FALSE);
		Check(hr);
	}

	hr = srDir->GetJSONData(pcwzDefs, cchDefs, &srv);
	if(FAILED(hr))
	{
		CheckIf(!fRequired, S_FALSE);
		Check(hr);
	}

	Check(srv->GetObject(&srData));
	srv.Release();

	Check(srData->FindNonNullValueW(L"sif", &srv));
	Check(srv->GetString(&rstrSIF));
	srv.Release();

	Check(srData->FindNonNullValueW(L"namespace", &srv));
	Check(srv->GetString(&rstrNamespaceW));
	srv.Release();

	hr = srDir->OpenSIF(RStrToWide(rstrSIF), &srSIF);
	CheckIf(fRequired && FAILED(hr), S_FALSE);

	pNamespace = __new CWallNamespace(rstrNamespaceW, srSIF);
	CheckAlloc(pNamespace);

	if(SUCCEEDED(hr))
	{
		DWORD cLayers;
		WALL_TEXTURE texture = {0};
		texture.idxLayer = 0xFFFFFFFF;

		cLayers = srSIF->GetLayerCount();
		for(DWORD i = 0; i < cLayers; i++)
		{
			TStackRef<ISimbeyInterchangeFileLayer> srLayer;
			WCHAR wzName[MAX_PATH];

			texture.idxLayer = i;
			Check(srSIF->GetLayerByIndex(i, &srLayer));
			Check(srLayer->GetName(wzName, ARRAYSIZE(wzName)));
			Check(AddType(pNamespace, m_aWalls.Length(), wzName, TStrLenAssert(wzName), texture));
		}
	}

	Check(m_mapNamespaces.Add(rstrNamespaceW, pNamespace));
	fAdded = true;

	*ppData = srData.Detach();
	*ppNamespace = pNamespace;

Cleanup:
	if(!fAdded)
		__delete pNamespace;
	RStrRelease(rstrNamespaceW);
	RStrRelease(rstrSIF);
	return hr;
}

HRESULT CWallTextures::AddType (CWallNamespace* pNamespace, sysint idxType, PCWSTR pcwzType, INT cchType, const WALL_TEXTURE& texture)
{
	HRESULT hr;
	RSTRING rstrType = NULL;

	Assert(m_aWalls.Length() == idxType);

	Check(RStrCreateW(cchType, pcwzType, &rstrType));
	Check(pNamespace->Add(rstrType, idxType));
	Check(m_aWalls.Append(texture));

Cleanup:
	RStrRelease(rstrType);
	return hr;
}

HRESULT CWallTextures::ParseDoors (CWallNamespace* pNamespace, IJSONArray* pDefs)
{
	HRESULT hr = S_FALSE;
	RSTRING rstrKeyW = NULL;
	sysint cDefs = pDefs->Count();

	for(sysint i = 0; i < cDefs; i++)
	{
		TStackRef<IJSONObject> srDef;
		TStackRef<IJSONValue> srv;
		DOOR_DEF defDoor;

		Check(pDefs->GetObject(i, &srDef));
		Check(srDef->FindNonNullValueW(L"lockable", &srv));
		Check(srv->GetBoolean(&defDoor.fLockable));

		Check(ReadAndResolveName(pNamespace, srDef, L"texture", &defDoor.idxTexture));
		Check(ReadAndResolveName(pNamespace, srDef, L"track", &defDoor.idxTrack));
		Check(GetNamespaceKey(pNamespace, srDef, &rstrKeyW));
		Check(m_mapDoorDefs.Add(rstrKeyW, defDoor));
		RStrRelease(rstrKeyW); rstrKeyW = NULL;
	}

Cleanup:
	RStrRelease(rstrKeyW);
	return hr;
}

HRESULT CWallTextures::ParseSwitches (CWallNamespace* pNamespace, IJSONArray* pDefs)
{
	HRESULT hr = S_FALSE;
	RSTRING rstrKeyW = NULL;
	sysint cDefs = pDefs->Count();

	for(sysint i = 0; i < cDefs; i++)
	{
		TStackRef<IJSONObject> srDef;
		ELEVATOR_DEF defSwitch;

		Check(pDefs->GetObject(i, &srDef));

		Check(ReadAndResolveName(pNamespace, srDef, L"down", &defSwitch.idxDown));
		Check(ReadAndResolveName(pNamespace, srDef, L"up", &defSwitch.idxUp));
		Check(GetNamespaceKey(pNamespace, srDef, &rstrKeyW));
		Check(m_mapElevatorDefs.Add(rstrKeyW, defSwitch));
		RStrRelease(rstrKeyW); rstrKeyW = NULL;
	}

Cleanup:
	RStrRelease(rstrKeyW);
	return hr;
}

HRESULT CWallTextures::ParseRailing (CWallNamespace* pNamespace, IJSONArray* pDefs)
{
	HRESULT hr = S_FALSE;
	RSTRING rstrKeyW = NULL;
	sysint cDefs = pDefs->Count();

	for(sysint i = 0; i < cDefs; i++)
	{
		TStackRef<IJSONObject> srDef;
		sysint idxRailing;

		Check(pDefs->GetObject(i, &srDef));

		Check(ReadAndResolveName(pNamespace, srDef, L"texture", &idxRailing));
		Check(GetNamespaceKey(pNamespace, srDef, &rstrKeyW));
		Check(m_mapRailingDefs.Add(rstrKeyW, idxRailing));
		RStrRelease(rstrKeyW); rstrKeyW = NULL;
	}

Cleanup:
	RStrRelease(rstrKeyW);
	return hr;
}

HRESULT CWallTextures::ReadAndResolveName (CWallNamespace* pNamespace, IJSONObject* pDef, PCWSTR pcwzName, __out sysint* pidxType)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	RSTRING rstrTypeW = NULL;

	Check(pDef->FindNonNullValueW(pcwzName, &srv));
	Check(srv->GetString(&rstrTypeW));
	Check(pNamespace->Find(rstrTypeW, pidxType));

Cleanup:
	RStrRelease(rstrTypeW);
	return hr;
}

HRESULT CWallTextures::GetNamespaceKey (CWallNamespace* pNamespace, IJSONObject* pDef, __out RSTRING* prstrKeyW)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	RSTRING rstrNameW = NULL;

	Check(pDef->FindNonNullValueW(L"name", &srv));
	Check(srv->GetString(&rstrNameW));
	Check(RStrFormatW(prstrKeyW, L"%r.%r", pNamespace->GetNamespace(), rstrNameW));

Cleanup:
	RStrRelease(rstrNameW);
	return hr;
}
