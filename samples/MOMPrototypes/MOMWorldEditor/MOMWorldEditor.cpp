#include <windows.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Core\MemoryStream.h"
#include "Library\Util\Formatting.h"
#include "Library\Util\FileStream.h"
#include "Library\Util\StreamHelpers.h"
#include "Library\Util\TextHelpers.h"
#include "Library\Window\DialogHost.h"
#include "Library\DPI.h"
#include "Library\ChooseFile.h"
#include "Published\JSON.h"
#include "Ribbon.h"
#include "RibbonMappings.h"
#include "NewWorldDlg.h"
#include "CityDlg.h"
#include "MOMWorldEditor.h"

#define	GAME_TICK_MS		33

#define	SURFACE_WIDTH		512
#define	SURFACE_HEIGHT		320

#define	TILE_WIDTH			20
#define	TILE_HEIGHT			18

const WCHAR c_wzAppClassName[] = L"MOMWorldEditorCls";
const WCHAR c_wzRegAppKey[] = L"Software\\Simbey\\MOMWorldEditor";
const WCHAR c_wzAppWindowPos[] = L"WindowPosition";
const WCHAR c_wzAppTitle[] = L"MOM World Editor";

const WCHAR c_wzFilter[] = L"Master of Magic JSON Map (*.json)\0*.json\0\0";

///////////////////////////////////////////////////////////////////////////////
// CTile
///////////////////////////////////////////////////////////////////////////////

CTile::CTile (RSTRING rstrTileSet, RSTRING rstrKey, ISimbeyInterchangeSprite* pSprite) :
	m_pSprite(pSprite),
	m_fIsSprite(true)
{
	RStrSet(m_rstrTileSet, rstrTileSet);
	RStrSet(m_rstrKey, rstrKey);
	m_pSprite->AddRef();
}

CTile::CTile (RSTRING rstrTileSet, RSTRING rstrKey, ISimbeyInterchangeAnimator* pAnimator) :
	m_pAnimator(pAnimator),
	m_fIsSprite(false)
{
	RStrSet(m_rstrTileSet, rstrTileSet);
	RStrSet(m_rstrKey, rstrKey);
	m_pAnimator->AddRef();
}

CTile::~CTile ()
{
	if(m_fIsSprite)
		m_pSprite->Release();
	else
		m_pAnimator->Release();
	RStrRelease(m_rstrKey);
	RStrRelease(m_rstrTileSet);
}

HRESULT CTile::CreateSprite (__deref_out ISimbeyInterchangeSprite** ppSprite)
{
	HRESULT hr;

	if(m_fIsSprite)
		Check(m_pSprite->Clone(ppSprite));
	else
	{
		TStackRef<ISimbeyInterchangeSprite> srSprite;

		Check(m_pAnimator->CreateSprite(&srSprite));
		Check(srSprite->SelectAnimation(0));
		*ppSprite = srSprite.Detach();
	}

Cleanup:
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
// CTileSet
///////////////////////////////////////////////////////////////////////////////

CTileSet::CTileSet (RSTRING rstrName)
{
	RStrSet(m_rstrName, rstrName);
}

CTileSet::~CTileSet ()
{
	for(sysint i = 0; i < m_mapTiles.Length(); i++)
	{
		TArray<CTile*>* pTiles = *(m_mapTiles.GetValuePtr(i));
		pTiles->DeleteAll();
		__delete pTiles;
	}
	RStrRelease(m_rstrName);
}

HRESULT CTileSet::AddVariant (RSTRING rstrKey, ISimbeyInterchangeSprite* pSprite)
{
	HRESULT hr;
	TArray<CTile*>* pTiles;
	CTile* pTile = NULL;

	Check(EnsureTilesArray(rstrKey, &pTiles));

	pTile = __new CTile(m_rstrName, rstrKey, pSprite);
	CheckAlloc(pTile);
	Check(pTiles->Append(pTile));

Cleanup:
	return hr;
}

HRESULT CTileSet::AddVariant (RSTRING rstrKey, ISimbeyInterchangeAnimator* pAnimator)
{
	HRESULT hr;
	TArray<CTile*>* pTiles;
	CTile* pTile = NULL;

	Check(EnsureTilesArray(rstrKey, &pTiles));

	pTile = __new CTile(m_rstrName, rstrKey, pAnimator);
	CheckAlloc(pTile);
	Check(pTiles->Append(pTile));

Cleanup:
	return hr;
}

HRESULT CTileSet::FindFromKey (RSTRING rstrKey, __deref_out TArray<CTile*>** ppTiles)
{
	return m_mapTiles.Find(rstrKey, ppTiles);
}

HRESULT CTileSet::EnsureTilesArray (RSTRING rstrKey, __deref_out TArray<CTile*>** ppTiles)
{
	HRESULT hr;
	TArray<CTile*>** ppTilesPtr = m_mapTiles[rstrKey];

	CheckAlloc(ppTilesPtr);
	if(NULL == *ppTilesPtr)
	{
		*ppTilesPtr = __new TArray<CTile*>;
		CheckAlloc(*ppTilesPtr);
	}

	*ppTiles = *ppTilesPtr;
	hr = S_OK;

Cleanup:
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
// CPlaceItem
///////////////////////////////////////////////////////////////////////////////

CPlaceItem::CPlaceItem (IJSONObject* pTileRules, CTile* pTile, INT x, INT y) :
	m_pTile(pTile),
	m_x(x), m_y(y)
{
	SetInterface(m_pTileRules, pTileRules);

	if(m_pTile)
	{
		RStrSet(m_rstrTile, m_pTile->GetTileSet());
		SideAssertHr(RStrCopyToW(m_pTile->GetKey(), ARRAYSIZE(m_wzKey), m_wzKey, NULL));
	}
	else
	{
		m_rstrTile = NULL;
		SideAssertHr(TStrCchCpy(m_wzKey, ARRAYSIZE(m_wzKey), L"00000000"));
	}
}

CPlaceItem::~CPlaceItem ()
{
	RStrRelease(m_rstrTile);

	SafeRelease(m_pTileRules);
}

bool CPlaceItem::IsSameTile (RSTRING rstrTile)
{
	return NULL == m_pTile || IsTileCompatible(L"same", rstrTile);
}

bool CPlaceItem::IsTileCompatible (PCWSTR pcwzType, RSTRING rstrTile)
{
	TStackRef<IJSONObject> srData;

	if(SUCCEEDED(GetTileData(m_rstrTile, &srData)))
	{
		TStackRef<IJSONValue> srv;
		TStackRef<IJSONArray> srList;

		if(SUCCEEDED(srData->FindNonNullValueW(pcwzType, &srv)) && SUCCEEDED(srv->GetArray(&srList)))
		{
			if(SUCCEEDED(JSONFindArrayString(srList, rstrTile, NULL)))
				return true;
		}
	}

	return false;
}

VOID CPlaceItem::SetTileOnly (RSTRING rstrTile)
{
	RStrReplace(&m_rstrTile, rstrTile);
}

HRESULT CPlaceItem::SetTileKey (TRStrMap<CTileSet*>* pmapTileSets, Dir::Value eDir, WCHAR wchValue)
{
	HRESULT hr;
	WCHAR wzNewKey[12];

	CheckIfIgnore(m_wzKey[eDir] == wchValue, S_FALSE);
	Check(TStrCchCpy(wzNewKey, ARRAYSIZE(wzNewKey), m_wzKey));

	wzNewKey[eDir] = wchValue;
	Check(SetNewKey(pmapTileSets, wzNewKey, wchValue));

Cleanup:
	return hr;
}

HRESULT CPlaceItem::GetTransitionTile (__in_opt RSTRING rstrTile, __out RSTRING* prstrTile)
{
	HRESULT hr;
	TStackRef<IJSONObject> srData;
	TStackRef<IJSONValue> srv;

	Check(GetTileData(rstrTile ? rstrTile : m_rstrTile, &srData));
	CheckNoTrace(srData->FindNonNullValueW(L"transition", &srv));
	Check(srv->GetString(prstrTile));

Cleanup:
	return hr;
}

HRESULT CPlaceItem::SetNewKey (TRStrMap<CTileSet*>* pmapTileSets, PCWSTR pcwzKey, WCHAR wchHint)
{
	HRESULT hr;
	CTileSet* pTileSet;
	TArray<CTile*>* paTiles;

	Check(pmapTileSets->Find(m_rstrTile, &pTileSet));
	hr = pTileSet->FindFromKey(RSTRING_CAST(pcwzKey), &paTiles);
	if(FAILED(hr))
	{
		if(L'\0' != wchHint)
		{
			WCHAR wzSmooth[12];
			Check(TStrCchCpy(wzSmooth, ARRAYSIZE(wzSmooth), pcwzKey));
			hr = SmoothTile(pTileSet, wzSmooth, wchHint, &paTiles);
			if(SUCCEEDED(hr))
				Check(TStrCchCpy(m_wzKey, ARRAYSIZE(m_wzKey), wzSmooth));
		}

		if(FAILED(hr))
		{
			hr = SetAltTile(pmapTileSets, pcwzKey);
			if(FAILED(hr))
			{
				CheckIf(1 != pTileSet->m_mapTiles.Length(), E_FAIL);
				Check(pTileSet->FindFromKey(RSTRING_CAST(L"00000000"), &paTiles));
				Check(TStrCchCpy(m_wzKey, ARRAYSIZE(m_wzKey), L"00000000"));
			}
		}

		Check(pmapTileSets->Find(m_rstrTile, &pTileSet));
		Check(pTileSet->FindFromKey(RSTRING_CAST(m_wzKey), &paTiles));
	}
	else
		Check(TStrCchCpy(m_wzKey, ARRAYSIZE(m_wzKey), pcwzKey));

	m_pTile = (*paTiles)[rand() % paTiles->Length()];

Cleanup:
	return hr;
}

HRESULT CPlaceItem::SmoothTile (CTileSet* pTileSet, PWSTR pwzKey, WCHAR wchValue, TArray<CTile*>** ppTiles)
{
	if(pwzKey[Dir::WEST] == wchValue && pwzKey[Dir::NORTH] == wchValue)
		pwzKey[Dir::NORTH_WEST] = wchValue;

	if(pwzKey[Dir::WEST] == wchValue && pwzKey[Dir::SOUTH] == wchValue)
		pwzKey[Dir::SOUTH_WEST] = wchValue;

	if(pwzKey[Dir::EAST] == wchValue && pwzKey[Dir::NORTH] == wchValue)
		pwzKey[Dir::NORTH_EAST] = wchValue;

	if(pwzKey[Dir::EAST] == wchValue && pwzKey[Dir::SOUTH] == wchValue)
		pwzKey[Dir::SOUTH_EAST] = wchValue;

	return pTileSet->FindFromKey(RSTRING_CAST(pwzKey), ppTiles);
}

HRESULT CPlaceItem::SetAltTile (TRStrMap<CTileSet*>* pmapTileSets, PCWSTR pcwzKey)
{
	HRESULT hr;
	CTileSet* pTileSet;
	TArray<CTile*>* pTiles;
	TStackRef<IJSONObject> srData;
	TStackRef<IJSONValue> srv;

	TStackRef<IJSONObject> srMatching, srTile;
	TStackRef<IJSONArray> srSame;
	RSTRING rstrNewTile;

	Check(GetTileData(m_rstrTile, &srData));
	Check(srData->FindNonNullValueW(L"tiles", &srv));
	Check(JSONFindArrayObjectIndirect(srv, RSTRING_CAST(L"key"), RSTRING_CAST(pcwzKey), &srTile, NULL));
	srv.Release();

	Check(srTile->FindNonNullValueW(L"tile", &srv));
	Check(srv->GetString(&rstrNewTile));
	RStrRelease(m_rstrTile); m_rstrTile = rstrNewTile;

	Check(pmapTileSets->Find(m_rstrTile, &pTileSet));
	Check(pTileSet->FindFromKey(RSTRING_CAST(pcwzKey), &pTiles));

	Check(TStrCchCpy(m_wzKey, ARRAYSIZE(m_wzKey), pcwzKey));

Cleanup:
	return hr;
}

HRESULT CPlaceItem::GetTileData (RSTRING rstrTile, __deref_out IJSONObject** ppData)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;

	Check(m_pTileRules->FindNonNullValue(rstrTile, &srv));
	Check(srv->GetObject(ppData));

Cleanup:
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
// CMOMWorldEditor
///////////////////////////////////////////////////////////////////////////////

CTerrainGallery::CTerrainGallery (CSIFRibbon* pRibbon, ISimbeyInterchangeFile* pSIF) :
	m_pRibbon(pRibbon),
	m_pSIF(pSIF),
	m_idxSelection(0)
{
	m_pRibbon->AddRef();
	m_pSIF->AddRef();
}

CTerrainGallery::~CTerrainGallery ()
{
	m_pSIF->Close();
	m_pSIF->Release();

	m_pRibbon->Release();
}

HRESULT CTerrainGallery::GetSelectedTile (__out RSTRING* prstrTile)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeFileLayer> srLayer;
	WCHAR wzTile[MAX_PATH];

	Check(m_pSIF->GetLayerByIndex(m_idxSelection, &srLayer));
	Check(srLayer->GetName(wzTile, ARRAYSIZE(wzTile)));
	Check(RStrCreateW(TStrLenAssert(wzTile), wzTile, prstrTile));

Cleanup:
	return hr;
}

UINT CTerrainGallery::GetSelection (VOID)
{
	return m_idxSelection;
}

VOID CTerrainGallery::SetSelection (UINT idxSelection)
{
	m_idxSelection = idxSelection;
}

// IUICollection

HRESULT STDMETHODCALLTYPE CTerrainGallery::GetCount (__out UINT32* pcItems)
{
	*pcItems = m_pSIF->GetLayerCount();
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CTerrainGallery::GetItem (UINT32 index, __deref_out_opt IUnknown** item)
{
	HRESULT hr;
	TStackRef<CSIFRibbonItem> srItem;
	TStackRef<ISimbeyInterchangeFileLayer> srLayer;
	WCHAR wzName[MAX_PATH];

	Check(m_pSIF->GetLayerByIndex(index, &srLayer));
	Check(srLayer->GetName(wzName, ARRAYSIZE(wzName)));
	Check(CSIFRibbonItem::Create(m_pRibbon, m_pSIF, &srItem));
	Check(srItem->SetItemText(wzName));
	srItem->SetItemIcon(srLayer->GetLayerID());
	*item = static_cast<IUISimplePropertySet*>(srItem.Detach());

Cleanup:
	return hr;
}

HRESULT STDMETHODCALLTYPE CTerrainGallery::Add (IUnknown* item)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CTerrainGallery::Insert (UINT32 index, IUnknown* item)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CTerrainGallery::RemoveAt (UINT32 index)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CTerrainGallery::Replace (UINT32 indexReplaced, IUnknown* itemReplaceWith)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CTerrainGallery::Clear (VOID)
{
	return E_NOTIMPL;
}

///////////////////////////////////////////////////////////////////////////////
// Gallery Commands
///////////////////////////////////////////////////////////////////////////////

HRESULT CTerrainCommand::Execute (class CMOMWorldEditor* pEditor, INT xTile, INT yTile)
{
	HRESULT hr;
	RSTRING rstrTile = NULL;
	INT nResult;

	Check(m_pGallery->GetSelectedTile(&rstrTile));
	Check(RStrCompareRStr(m_pWorld[yTile * m_xWorld + xTile].pTile->GetTileSet(), rstrTile, &nResult));
	CheckIfIgnore(0 == nResult, S_FALSE);

	Check(pEditor->PlaceTile(m_pWorld, xTile, yTile, m_pmapTileSets, rstrTile, TRUE));

Cleanup:
	RStrRelease(rstrTile);
	return hr;
}

HRESULT CFeaturesCommand::Execute (class CMOMWorldEditor* pEditor, INT xTile, INT yTile)
{
	HRESULT hr;
	RSTRING rstrTile = NULL;

	Check(m_pGallery->GetSelectedTile(&rstrTile));

	RStrReplace(&m_pWorld[yTile * m_xWorld + xTile].rstrFeature, rstrTile);
	pEditor->ClearTile(xTile, yTile, TRUE);

Cleanup:
	RStrRelease(rstrTile);
	return hr;
}

HRESULT CClearFeatureCommand::Execute (class CMOMWorldEditor* pEditor, INT xTile, INT yTile)
{
	SafeRelease(m_pWorld[yTile * m_xWorld + xTile].pData);
	RStrReplace(&m_pWorld[yTile * m_xWorld + xTile].rstrFeature, NULL);
	pEditor->ClearTile(xTile, yTile, TRUE);
	return S_OK;
}

HRESULT CPlaceCityCommand::Execute (class CMOMWorldEditor* pEditor, INT xTile, INT yTile)
{
	return pEditor->PlaceOrModifyCity(m_pWorld, xTile, yTile);
}

///////////////////////////////////////////////////////////////////////////////
// CMOMWorldEditor
///////////////////////////////////////////////////////////////////////////////

CMOMWorldEditor::CMOMWorldEditor (HINSTANCE hInstance) :
	m_hInstance(hInstance),
	m_pRibbon(NULL),
	m_pPackage(NULL),
	m_pSurface(NULL),
	m_fActive(FALSE),
	m_pTileRules(NULL),
	m_pArcanusTerrain(NULL),
	m_pMyrrorTerrain(NULL),
	m_pFeatures(NULL),
	m_pArcanusWorld(NULL),
	m_pMyrrorWorld(NULL),
	m_eType(World::Arcanus),
	m_pMain(NULL),
	m_pMapTileLayer(NULL),
	m_pMouse(NULL),
	m_fDragging(FALSE),
	m_fPainting(FALSE),
	m_pCommand(NULL)
{
	ZeroMemory(m_fKeys, sizeof(m_fKeys));
}

CMOMWorldEditor::~CMOMWorldEditor ()
{
	for(sysint i = 0; i < m_mapArcanus.Length(); i++)
		__delete *(m_mapArcanus.GetValuePtr(i));
	for(sysint i = 0; i < m_mapMyrror.Length(); i++)
		__delete *(m_mapMyrror.GetValuePtr(i));

	SafeRelease(m_pArcanusTerrain);
	SafeRelease(m_pMyrrorTerrain);

	for(sysint i = 0; i < m_aCityTiles.Length(); i++)
	{
		for(INT n = 0; n < 17; n++)
		{
			SafeRelease(m_aCityTiles[i].pNormal[n]);
			SafeRelease(m_aCityTiles[i].pWalled[n]);
		}
	}

	for(sysint i = 0; i < m_mapFeatures.Length(); i++)
		(*m_mapFeatures.GetValuePtr(i))->Release();

	SafeRelease(m_pFeatures);
	SafeRelease(m_pTileRules);

	SafeRelease(m_pSurface);
	SafeRelease(m_pPackage);
	SafeRelease(m_pRibbon);
}

HRESULT CMOMWorldEditor::Register (HINSTANCE hInstance)
{
	WNDCLASSEX wnd = {0};

	wnd.cbSize = sizeof(WNDCLASSEX);
	wnd.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	wnd.cbClsExtra = 0;
	wnd.cbWndExtra = 0;
	wnd.hInstance = hInstance;
	wnd.hIcon = LoadIcon(hInstance, MAKEINTRESOURCEW(IDI_MAIN));
	wnd.hCursor = NULL; //LoadCursor(NULL, IDC_ARROW);
	wnd.hbrBackground = NULL;
	wnd.lpszMenuName = NULL;
	wnd.lpszClassName = c_wzAppClassName;

	return RegisterClass(&wnd, NULL);
}

HRESULT CMOMWorldEditor::Unregister (HINSTANCE hInstance)
{
	return UnregisterClass(c_wzAppClassName, hInstance);
}

HRESULT CMOMWorldEditor::Initialize (INT nWidth, INT nHeight, INT nCmdShow)
{
	HRESULT hr;
	RECT rect = { 0, 0, nWidth, nHeight };
	TStackRef<IJSONObject> srData;
	TStackRef<IJSONValue> srvRules;
	ISimbeyInterchangeFile* pSIF = NULL;

	Check(CSIFRibbon::Create(DPI::Scale, &m_pRibbon));

	m_pSurface = __new CInteractiveSurface(SURFACE_WIDTH, SURFACE_HEIGHT);
	CheckAlloc(m_pSurface);
	m_pSurface->EnableClear(RGB(255, 255, 255));

	m_pPackage = __new CSIFPackage;
	CheckAlloc(m_pPackage);
	Check(m_pPackage->OpenPackage(L"Assets.pkg"));	// This file must be built by SIFPackage.exe

	Check(m_pPackage->GetJSONData(SLP(L"overland\\terrain\\rules.json"), &srvRules));
	Check(srvRules->GetObject(&m_pTileRules));

	Check(m_pPackage->OpenSIF(L"overland\\features\\features.sif", &pSIF));
	m_pFeatures = __new CTerrainGallery(m_pRibbon, pSIF);
	CheckAlloc(m_pFeatures);
	Check(LoadFeatures(pSIF));
	SafeRelease(pSIF);

	Check(m_pPackage->OpenSIF(L"overland\\cities\\cities.sif", &pSIF));
	Check(LoadCityTiles(pSIF));
	pSIF->Close();
	SafeRelease(pSIF);

	Check(LoadTerrain());

	Check(m_pSurface->AddCanvas(NULL, TRUE, &m_pMain));
	Check(static_cast<CInteractiveCanvas*>(m_pMain)->AddInteractiveLayer(TRUE, FALSE, RGB(0, 0, 0), this, &m_pMapTileLayer));
	Check(m_pMain->AddLayer(FALSE, FALSE, RGB(0, 0, 0), &m_nFeaturesLayer));
	Check(m_pMain->AddLayer(FALSE, FALSE, RGB(0, 0, 0), &m_nCitiesLayer));

	Check(SetupMouse());

	Check(SetupMap(64, 48));

	m_pMain->SetScroll((m_xWorld * TILE_WIDTH) / 2 - SURFACE_WIDTH / 2,
		(m_yWorld * TILE_HEIGHT) / 2 - SURFACE_HEIGHT / 2);
	UpdateVisibleTiles();

	CheckIfGetLastError(!AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE));
	Check(Create(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE, WS_OVERLAPPEDWINDOW,
		c_wzAppClassName, c_wzAppTitle, CW_USEDEFAULT, CW_USEDEFAULT,
		rect.right - rect.left, rect.bottom - rect.top, NULL, nCmdShow));

	Check(m_player.Initialize());

	srand(GetTickCount());

Cleanup:
	if(FAILED(hr) && m_hwnd)
		Destroy();

	if(pSIF)
	{
		pSIF->Close();
		SafeRelease(pSIF);
	}
	return hr;
}

VOID CMOMWorldEditor::Run (VOID)
{
	MSG msg;
	DWORD dwTimer = 0;

	for(;;)
	{
		if(PeekMessage(&msg,NULL,0,0,PM_REMOVE))
		{
			if(msg.message == WM_QUIT)
				break;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if(m_fActive)
		{
			DWORD dwNow = timeGetTime();
			DWORD dwFrame = dwNow - dwTimer;
			if(dwFrame >= GAME_TICK_MS || WAIT_TIMEOUT == MsgWaitForMultipleObjects(0,NULL,FALSE,GAME_TICK_MS - dwFrame,QS_ALLINPUT))
			{
				dwTimer = dwNow;

				m_pSurface->Tick();
				OnUpdateFrame();
				m_pSurface->Redraw(m_hwnd, NULL);
			}
		}
		else
			WaitMessage();
	}
}

// IRibbonHost

HRESULT WINAPI CMOMWorldEditor::GetRibbonSettingsKey (__out_ecount(cchMaxKey) PSTR pszKeyName, INT cchMaxKey)
{
	return TStrCchCpy(pszKeyName, cchMaxKey, "Software\\Simbey\\MOMWorldEditor");
}

HRESULT WINAPI CMOMWorldEditor::GetRibbonSettingsValue (__out_ecount(cchMaxValue) PSTR pszValueName, INT cchMaxValue)
{
	return TStrCchCpy(pszValueName, cchMaxValue, "RibbonSettings");
}

HRESULT WINAPI CMOMWorldEditor::GetRibbonResource (__out HMODULE* phModule, __out_ecount(cchMaxResource) PWSTR pwzResource, INT cchMaxResource)
{
	*phModule = GetModuleHandle(NULL);
	return TStrCchCpy(pwzResource, cchMaxResource, L"APPLICATION_RIBBON");
}

UINT32 WINAPI CMOMWorldEditor::TranslateGroupToImage (UINT32 nID)
{
	// Use the tool-generated MapGroupsToImages() function.
	return MapGroupsToImages(nID);
}

UINT32 WINAPI CMOMWorldEditor::TranslateImageToLargeImage (UINT32 nID)
{
	// Use the tool-generated MapImagesToLargeImages() function.
	return MapImagesToLargeImages(nID);
}

// IUIApplication

HRESULT STDMETHODCALLTYPE CMOMWorldEditor::OnViewChanged (UINT32 viewId, UI_VIEWTYPE typeID, IUnknown* view, UI_VIEWVERB verb, INT32 uReasonCode)
{
	HRESULT hr = S_OK;

	switch(verb)
	{
	case UI_VIEWVERB_CREATE:
		break;
	case UI_VIEWVERB_DESTROY:
		if(m_pRibbon)
			m_pRibbon->SaveSettings();
		break;
	case UI_VIEWVERB_SIZE:
		m_pSurface->SetOffset(m_pRibbon->GetHeight());

		{
			RECT rc;
			GetClientRect(m_hwnd, &rc);
			m_pSurface->Position(rc.right - rc.left, (rc.bottom - rc.top) - m_pRibbon->GetHeight());
		}
		break;
	}

	return hr;
}

HRESULT STDMETHODCALLTYPE CMOMWorldEditor::OnCreateUICommand (UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler** commandHandler)
{
	HRESULT hr;

	if(typeID == UI_COMMANDTYPE_COLLECTION || typeID == UI_COMMANDTYPE_COMMANDCOLLECTION) 
	{
		CheckIf(ID_TERRAIN != commandId && ID_FEATURES != commandId, E_NOTIMPL);
	}

	*commandHandler = this;
	AddRef();
	hr = S_OK;

Cleanup:
	return hr;
}

HRESULT STDMETHODCALLTYPE CMOMWorldEditor::OnDestroyUICommand (UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler* commandHandler)
{
	return S_OK;
}

// IUICommandHandler

HRESULT STDMETHODCALLTYPE CMOMWorldEditor::Execute (UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* currentValue, IUISimplePropertySet* commandExecutionProperties)
{
	HRESULT hr = S_OK;

	if(UI_EXECUTIONVERB_EXECUTE == verb)
	{
		switch(commandId)
		{
		case ID_OPEN:
		case ID_SAVE:
			Check(OpenSaveMap(commandId));
			break;
		case ID_NEW:
			Check(PromptForNewMap());
			break;
		case ID_EXIT:
			PostMessage(m_hwnd, WM_CLOSE, 0, 0);
			break;
		case ID_ARCANUS:
			Check(m_pRibbon->UpdateProperty(ID_ARCANUS, &UI_PKEY_BooleanValue));
			CheckIfIgnore(World::Arcanus == m_eType, S_FALSE);
			m_eType = World::Arcanus;
			Check(m_pRibbon->UpdateProperty(ID_MYRROR, &UI_PKEY_BooleanValue));
			Check(m_pRibbon->UpdateProperty(ID_TERRAIN, &UI_PKEY_ItemsSource));
			Check(m_pRibbon->UpdateProperty(ID_TERRAIN, &UI_PKEY_SelectedItem));
			m_pMapTileLayer->Clear();
			m_pMain->ClearLayer(m_nFeaturesLayer);
			m_pMain->ClearLayer(m_nCitiesLayer);
			Check(UpdateVisibleTiles());
			break;
		case ID_MYRROR:
			Check(m_pRibbon->UpdateProperty(ID_MYRROR, &UI_PKEY_BooleanValue));
			CheckIfIgnore(World::Myrror == m_eType, S_FALSE);
			m_eType = World::Myrror;
			Check(m_pRibbon->UpdateProperty(ID_ARCANUS, &UI_PKEY_BooleanValue));
			Check(m_pRibbon->UpdateProperty(ID_TERRAIN, &UI_PKEY_ItemsSource));
			Check(m_pRibbon->UpdateProperty(ID_TERRAIN, &UI_PKEY_SelectedItem));
			m_pMapTileLayer->Clear();
			m_pMain->ClearLayer(m_nFeaturesLayer);
			m_pMain->ClearLayer(m_nCitiesLayer);
			Check(UpdateVisibleTiles());
			break;
		case ID_TERRAIN:
			if(World::Arcanus == m_eType)
			{
				m_pArcanusTerrain->SetSelection(currentValue->uintVal);
				Check(ReplaceCommand(__new CTerrainCommand(m_pArcanusTerrain, m_pArcanusWorld, m_xWorld, m_yWorld, &m_mapArcanus)));
			}
			else if(World::Myrror == m_eType)
			{
				m_pMyrrorTerrain->SetSelection(currentValue->uintVal);
				Check(ReplaceCommand(__new CTerrainCommand(m_pMyrrorTerrain, m_pMyrrorWorld, m_xWorld, m_yWorld, &m_mapMyrror)));
			}
			break;
		case ID_FEATURES:
			m_pFeatures->SetSelection(currentValue->uintVal);
			Check(ReplaceCommand(__new CFeaturesCommand(m_pFeatures, World::Arcanus == m_eType ? m_pArcanusWorld : m_pMyrrorWorld, m_xWorld, m_yWorld)));
			break;
		case ID_SELECT_CLEAR:
			Check(ReplaceCommand(__new CClearFeatureCommand(m_pFeatures, World::Arcanus == m_eType ? m_pArcanusWorld : m_pMyrrorWorld, m_xWorld, m_yWorld)));
			break;
		case ID_PLACE_CITY:
			Check(ReplaceCommand(__new CPlaceCityCommand(m_pFeatures, World::Arcanus == m_eType ? m_pArcanusWorld : m_pMyrrorWorld, m_xWorld, m_yWorld)));
			break;
		}
	}

Cleanup:
	return hr;
}

HRESULT STDMETHODCALLTYPE CMOMWorldEditor::UpdateProperty (UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT* currentValue, PROPVARIANT* newValue)
{
	HRESULT hr = E_NOTIMPL;

	if(UI_PKEY_LargeImage == key || UI_PKEY_SmallImage == key)
	{
		hr = m_pRibbon->LoadImageForCommand(commandId, key, newValue);
	}
	else if(UI_PKEY_BooleanValue == key)
	{
		if(ID_ARCANUS == commandId)
		{
			newValue->boolVal = World::Arcanus == m_eType ? VARIANT_TRUE : VARIANT_FALSE;
			newValue->vt = VT_BOOL;
			hr = S_OK;
		}
		else if(ID_MYRROR == commandId)
		{
			newValue->boolVal = World::Myrror == m_eType ? VARIANT_TRUE : VARIANT_FALSE;
			newValue->vt = VT_BOOL;
			hr = S_OK;
		}
	}
	else if(UI_PKEY_SelectedItem == key)
	{
		if(ID_TERRAIN == commandId)
		{
			if(World::Arcanus == m_eType)
				newValue->uintVal = m_pArcanusTerrain->GetSelection();
			else if(World::Myrror == m_eType)
				newValue->uintVal = m_pMyrrorTerrain->GetSelection();
			newValue->vt = VT_UI4;
			hr = S_OK;
		}
		else if(ID_FEATURES == commandId)
		{
			newValue->uintVal = m_pFeatures->GetSelection();
			newValue->vt = VT_UI4;
			hr = S_OK;
		}
	}
	else if(UI_PKEY_ItemsSource == key)
	{
		TStackRef<IUICollection> srItems, srSource;
		UINT32 cItems;

		Check(currentValue->punkVal->QueryInterface(&srItems));

		srItems->Clear();

		if(ID_FEATURES == commandId)
			srSource = m_pFeatures;
		else if(World::Arcanus == m_eType)
			srSource = m_pArcanusTerrain;
		else if(World::Myrror == m_eType)
			srSource = m_pMyrrorTerrain;
		else
			Check(E_UNEXPECTED);

		Check(srSource->GetCount(&cItems));

		for(UINT32 i = 0; i < cItems; i++)
		{
			TStackRef<IUnknown> srUnkItem;

			Check(srSource->GetItem(i, &srUnkItem));
			Check(srItems->Insert(i, srUnkItem));
		}
	}

Cleanup:
	return hr;
}

HINSTANCE CMOMWorldEditor::GetInstance (VOID)
{
	return m_hInstance;
}

VOID CMOMWorldEditor::OnFinalDestroy (HWND hwnd)
{
	if(m_pRibbon)
		m_pRibbon->Unload();

	SafeRelease(m_pMouse);
	SafeRelease(m_pMapTileLayer);

	m_pSurface->RemoveCanvas(m_pMain);
	m_pMain = NULL;

	DeleteWorld(m_pArcanusWorld);
	DeleteWorld(m_pMyrrorWorld);

	m_pSurface->Destroy();
	m_player.Stop();

	m_fActive = FALSE;
	PostQuitMessage(0);
}

HRESULT CMOMWorldEditor::FinalizeAndShow (DWORD dwStyle, INT nCmdShow)
{
	HRESULT hr;

	Check(m_pRibbon->Initialize(m_hwnd, this));
	Check(m_pRibbon->SetModes(1));
	LoadWindowPosition(m_hwnd, c_wzAppWindowPos);
	Check(m_pRibbon->UpdateProperty(ID_ARCANUS, &UI_PKEY_BooleanValue));
	Check(__super::FinalizeAndShow(dwStyle, nCmdShow));

Cleanup:
	return hr;
}

BOOL CMOMWorldEditor::OnCreate (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return FALSE;
}

BOOL CMOMWorldEditor::OnPaint (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(m_hwnd, &ps);

	const RECT* prcUnpainted;
	INT cUnpainted;
	m_pSurface->GetUnpaintedRects(&prcUnpainted, &cUnpainted);

	for(INT i = 0; i < cUnpainted; i++)
		FillRect(hdc, prcUnpainted + i, reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

	m_pSurface->Redraw(m_hwnd, hdc);
	EndPaint(m_hwnd, &ps);
	lResult = TRUE;
	return TRUE;
}

BOOL CMOMWorldEditor::OnEraseBackground (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	lResult = TRUE;
	return TRUE;
}

BOOL CMOMWorldEditor::OnSize (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	m_pSurface->Position(LOWORD(lParam), HIWORD(lParam) - m_pRibbon->GetHeight());
	return FALSE;
}

BOOL CMOMWorldEditor::OnClose (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return FALSE;
}

BOOL CMOMWorldEditor::OnActivate (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	m_fActive = (WA_INACTIVE != LOWORD(wParam));
	return FALSE;
}

BOOL CMOMWorldEditor::OnKeyDown (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	m_fKeys[static_cast<BYTE>(wParam)] = true;
	return m_pSurface->ProcessLayerInput(uMsg, wParam, lParam, lResult);
}

BOOL CMOMWorldEditor::OnKeyUp (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	m_fKeys[static_cast<BYTE>(wParam)] = false;
	return m_pSurface->ProcessLayerInput(uMsg, wParam, lParam, lResult);
}

BOOL CMOMWorldEditor::OnSetCursor (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	if(HTCLIENT == LOWORD(lParam))
	{
		POINT pt;

		if(GetCursorPos(&pt))
		{
			const RECT* prcUnpainted;
			INT cUnpainted;

			ScreenToClient(m_hwnd, &pt);
			if(pt.y < static_cast<LONG>(m_pRibbon->GetHeight()))
			{
				SetCursor(LoadCursor(NULL, IDC_ARROW));
				return FALSE;
			}

			m_pSurface->GetUnpaintedRects(&prcUnpainted, &cUnpainted);
			if(0 < cUnpainted)
			{
				for(INT i = 0; i < cUnpainted; i++)
				{
					if(PtInRect(prcUnpainted + i, pt))
					{
						SetCursor(LoadCursor(NULL, IDC_ARROW));
						return FALSE;
					}
				}
			}
		}

		SetCursor(NULL);
		lResult = TRUE;
		return TRUE;
	}
	return FALSE;
}

BOOL CMOMWorldEditor::OnDestroy (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	SafeDelete(m_pCommand);
	SaveWindowPosition(m_hwnd, c_wzAppWindowPos);
	return FALSE;
}

// INotifyFinished

VOID CMOMWorldEditor::OnNotifyFinished (MIDI::CPlayer* pPlayer, BOOL fCompleted)
{
}

// ILayerInputHandler

BOOL CMOMWorldEditor::ProcessMouseInput (LayerInput::Mouse eType, WPARAM wParam, LPARAM lParam, INT xView, INT yView, LRESULT& lResult)
{
	if(LayerInput::LButtonDown == eType)
	{
		m_fPainting = TRUE;
		PlaceSelectedTile(xView, yView);
	}
	else if(LayerInput::LButtonUp == eType)
		m_fPainting = FALSE;
	else if(LayerInput::RButtonDown == eType)
	{
		m_xDrag = xView;
		m_yDrag = yView;
		m_fDragging = TRUE;
	}
	else if(LayerInput::RButtonUp == eType)
		m_fDragging = FALSE;
	else if(LayerInput::Move == eType)
	{
		if(m_fDragging)
		{
			Scroll(m_xDrag - xView, m_yDrag - yView);

			CSIFCanvas* pCanvas;
			m_pSurface->TranslateClientPointToView(LOWORD(lParam), HIWORD(lParam), &pCanvas, &m_xDrag, &m_yDrag);

			xView = m_xDrag;
			yView = m_yDrag;
		}
		else if(m_fPainting)
			PlaceSelectedTile(xView, yView);

		m_pMouse->SetPosition(xView, yView);
	}

	lResult = 0;
	return TRUE;
}

BOOL CMOMWorldEditor::ProcessKeyboardInput (LayerInput::Keyboard eType, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	if(LayerInput::KeyDown == eType)
	{
		switch(wParam)
		{
		case VK_LEFT:
			Scroll(-4, 0);
			break;
		case VK_RIGHT:
			Scroll(4, 0);
			break;
		case VK_UP:
			Scroll(0, -3);
			break;
		case VK_DOWN:
			Scroll(0, 3);
			break;
		}
	}

	lResult = 0;
	return TRUE;
}

VOID CMOMWorldEditor::OnUpdateFrame (VOID)
{
}

HRESULT CMOMWorldEditor::OpenSaveMap (UINT32 commandId)
{
	HRESULT hr;
	CChooseFile Select;

	Check(Select.Initialize());

	if(ID_OPEN == commandId)
	{
		CheckIf(!Select.OpenSingleFile(m_hwnd, c_wzFilter), E_ABORT);
		Check(LoadFrom(Select.GetFile(0)));
	}
	else
	{
		CheckIf(!Select.SaveFile(m_hwnd, c_wzFilter), E_ABORT);
		Check(SaveTo(Select.GetFile(0)));
	}

Cleanup:
	return hr;
}

HRESULT CMOMWorldEditor::LoadFrom (PCWSTR pcwzFile)
{
	HRESULT hr;
	PWSTR pwzJSON = NULL;
	INT cchJSON, nWidth, nHeight;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONObject> srMap;
	MAPTILE* pArcanusWorld = NULL, *pMyrrorWorld = NULL;

	Check(Text::LoadFromFile(pcwzFile, &pwzJSON, &cchJSON));
	Check(JSONParse(NULL, pwzJSON, cchJSON, &srv));
	Check(srv->GetObject(&srMap));
	srv.Release();

	Check(srMap->FindNonNullValueW(L"width", &srv));
	Check(srv->GetInteger(&nWidth));
	srv.Release();

	Check(srMap->FindNonNullValueW(L"height", &srv));
	Check(srv->GetInteger(&nHeight));
	srv.Release();

	Check(LoadWorldFromJSON(m_mapArcanus, srMap, L"arcanus", nWidth, nHeight, &pArcanusWorld));
	Check(LoadWorldFromJSON(m_mapMyrror, srMap, L"myrror", nWidth, nHeight, &pMyrrorWorld));

	DeleteWorld(m_pArcanusWorld);
	DeleteWorld(m_pMyrrorWorld);

	m_pArcanusWorld = pArcanusWorld; pArcanusWorld = NULL;
	m_pMyrrorWorld = pMyrrorWorld; pMyrrorWorld = NULL;

	m_xWorld = nWidth;
	m_yWorld = nHeight;

	m_pMain->SetScroll((m_xWorld * TILE_WIDTH) / 2 - SURFACE_WIDTH / 2,
		(m_yWorld * TILE_HEIGHT) / 2 - SURFACE_HEIGHT / 2);

	m_pMapTileLayer->Clear();
	m_pMain->ClearLayer(m_nFeaturesLayer);
	m_pMain->ClearLayer(m_nCitiesLayer);
	UpdateVisibleTiles();

Cleanup:
	DeleteWorld(pArcanusWorld);
	DeleteWorld(pMyrrorWorld);
	SafeDeleteArray(pwzJSON);
	return hr;
}

HRESULT CMOMWorldEditor::SaveTo (PCWSTR pcwzFile)
{
	HRESULT hr;
	TStackRef<IJSONObject> srMap;
	TStackRef<IJSONValue> srv;
	CMemoryStream stmJSON;

	Check(JSONCreateObject(&srMap));

	Check(JSONCreateInteger(m_xWorld, &srv));
	Check(srMap->AddValueW(L"width", srv));
	srv.Release();

	Check(JSONCreateInteger(m_yWorld, &srv));
	Check(srMap->AddValueW(L"height", srv));
	srv.Release();

	Check(SaveWorldToJSON(srMap, L"arcanus", m_xWorld, m_yWorld, m_pArcanusWorld));
	Check(SaveWorldToJSON(srMap, L"myrror", m_xWorld, m_yWorld, m_pMyrrorWorld));

	Check(JSONSerializeObject(srMap, &stmJSON));
	Check(Text::SaveToFile(stmJSON.TGetReadPtr<WCHAR>(), stmJSON.TDataRemaining<WCHAR>(), pcwzFile));

Cleanup:
	return hr;
}

HRESULT CMOMWorldEditor::PromptForNewMap (VOID)
{
	HRESULT hr;
	CDialogHost host(m_hInstance);
	CNewWorldDlg dlgNewWorld(m_xWorld, m_yWorld);

	Check(host.Display(m_hwnd, &dlgNewWorld));
	CheckIfIgnore(host.GetReturnValue() != IDOK, E_ABORT);

	DeleteWorld(m_pArcanusWorld);
	DeleteWorld(m_pMyrrorWorld);

	Check(SetupMap(dlgNewWorld.m_nWidth, dlgNewWorld.m_nHeight));

	m_pMain->SetScroll((m_xWorld * TILE_WIDTH) / 2 - SURFACE_WIDTH / 2,
		(m_yWorld * TILE_HEIGHT) / 2 - SURFACE_HEIGHT / 2);
	UpdateVisibleTiles();

Cleanup:
	return hr;
}

VOID CMOMWorldEditor::DeleteWorld (MAPTILE*& pWorld)
{
	if(pWorld)
	{
		INT nWorldCells = m_xWorld * m_yWorld;
		for(INT i = 0; i < nWorldCells; i++)
		{
			RStrRelease(pWorld[i].rstrFeature);
			SafeRelease(pWorld[i].pData);
			SafeRelease(pWorld[i].pCity);
			SafeRelease(pWorld[i].pStack);
		}
		SafeDeleteArray(pWorld);
	}
}

HRESULT CMOMWorldEditor::ResetWorldTiles (MAPTILE* pWorld, INT xWorld, INT yWorld, TRStrMap<CTileSet*>& mapTiles)
{
	HRESULT hr;
	MAPTILE* pWorldPtr = pWorld;
	CTileSet* pOcean, *pTundra;
	TArray<CTile*>* pTiles;
	sysint cVariants;
	INT nPrev = -1;

	Check(mapTiles.Find(RSTRING_CAST(L"ocean"), &pOcean));
	Check(mapTiles.Find(RSTRING_CAST(L"tundra"), &pTundra));

	Check(pOcean->FindFromKey(RSTRING_CAST(L"00000000"), &pTiles));

	cVariants = pTiles->Length();
	for(INT y = 0; y < yWorld; y++)
	{
		for(INT x = 0; x < xWorld; x++)
		{
			INT nNext;
			for(;;)
			{
				nNext = rand() % cVariants;
				if(nNext != nPrev)
					break;
			}
			pWorldPtr->pTile = (*pTiles)[nNext];
			pWorldPtr++;
			nPrev = nNext;
		}
	}

	for(INT x = 0; x < xWorld; x++)
	{
		Check(PlaceTile(pWorld, x, 0, &mapTiles, pTundra->m_rstrName, FALSE));
		Check(PlaceTile(pWorld, x, yWorld - 1, &mapTiles, pTundra->m_rstrName, FALSE));

		if(rand() % 10 >= 4)
		{
			Check(PlaceTile(pWorld, x, 1, &mapTiles, pTundra->m_rstrName, FALSE));
			Check(PlaceTile(pWorld, x, yWorld - 2, &mapTiles, pTundra->m_rstrName, FALSE));
		}
	}

Cleanup:
	return hr;
}

HRESULT CMOMWorldEditor::UpdateVisibleTiles (VOID)
{
	HRESULT hr;
	sysint nMapTileLayer = m_pMapTileLayer->GetLayer();
	INT xView, yView, xAdjust, yAdjust;
	RECT rcView;
	MAPTILE* pWorld;

	if(World::Arcanus == m_eType)
		pWorld = m_pArcanusWorld;
	else if(World::Myrror == m_eType)
		pWorld = m_pMyrrorWorld;
	else
		Check(E_UNEXPECTED);

	m_pMain->GetViewSize(&xView, &yView);
	rcView.left = 0;
	rcView.top = 0;
	rcView.right = xView;
	rcView.bottom = yView;

	m_pMain->OffsetPoint(*(POINT*)&rcView);
	m_pMain->OffsetPoint(*((POINT*)&rcView + 1));

	Check(m_pMain->RemoveHiddenSprites(nMapTileLayer));
	Check(m_pMain->RemoveHiddenSprites(m_nFeaturesLayer));
	Check(m_pMain->RemoveHiddenSprites(m_nCitiesLayer));

	xAdjust = rcView.left % TILE_WIDTH;
	if(0 != xAdjust)
	{
		rcView.left -= xAdjust;
		if(rcView.left < 0)
			rcView.left = 0;
	}
	yAdjust = rcView.top % TILE_HEIGHT;
	if(0 != yAdjust)
	{
		rcView.top -= yAdjust;
		if(rcView.top < 0)
			rcView.top = 0;
	}

	for(LONG y = rcView.top; y < rcView.bottom; y += TILE_HEIGHT)
	{
		for(LONG x = rcView.left; x < rcView.right; x += TILE_WIDTH)
		{
			if(FAILED(m_pMain->FindSpriteAt(nMapTileLayer, x, y, NULL)))
			{
				TStackRef<ISimbeyInterchangeSprite> srSprite;
				INT xTile = x / TILE_WIDTH;
				INT yTile = y / TILE_HEIGHT;
				MAPTILE* pMapTile = pWorld + (yTile * m_xWorld + xTile);

				Check(pMapTile->pTile->CreateSprite(&srSprite));
				srSprite->SetPosition(x, y);
				Check(m_pMain->AddSprite(nMapTileLayer, srSprite, NULL));

				if(pMapTile->rstrFeature)
				{
					ISimbeyInterchangeSprite* pFeature;

					Check(m_mapFeatures.Find(pMapTile->rstrFeature, &pFeature));

					srSprite.Release();
					Check(pFeature->Clone(&srSprite));

					srSprite->SetPosition(x, y);
					Check(m_pMain->AddSprite(m_nFeaturesLayer, srSprite, NULL));
				}

				if(pMapTile->pCity)
				{
					TStackRef<IJSONValue> srv;
					INT nPopulation, nTile, nFlag;
					bool fWall;

					Check(pMapTile->pCity->FindNonNullValueW(L"population", &srv));
					Check(srv->GetInteger(&nPopulation));
					srv.Release();

					if(nPopulation < 1000)
					{
						// This is an outpost, but there isn't a special tile for outposts.
						nTile = 0;
					}
					else
					{
						nTile = ((nPopulation / 1000) - 1) / 5;

						if(nTile >= m_aCityTiles.Length())
							nTile = m_aCityTiles.Length() - 1;
					}

					if(SUCCEEDED(pMapTile->pCity->FindNonNullValueW(L"flag", &srv)))
					{
						Check(srv->GetInteger(&nFlag));
						srv.Release();
					}
					else
						nFlag = 0;

					srSprite.Release();
					if(SUCCEEDED(pMapTile->pCity->FindNonNullValueW(L"wall", &srv)) && SUCCEEDED(srv->GetBoolean(&fWall)) && fWall)
						Check(m_aCityTiles[nTile].pWalled[nFlag]->Clone(&srSprite));
					else
						Check(m_aCityTiles[nTile].pNormal[nFlag]->Clone(&srSprite));

					srSprite->SetPosition(x, y);
					Check(m_pMain->AddSprite(m_nCitiesLayer, srSprite, NULL));
				}
			}
		}
	}

Cleanup:
	return hr;
}

VOID CMOMWorldEditor::Scroll (INT x, INT y)
{
	INT xScroll, yScroll;
	m_pMain->GetScroll(&xScroll, &yScroll);
	xScroll += x;
	if(xScroll < 0)
		xScroll = 0;
	else if(xScroll + SURFACE_WIDTH > m_xWorld * TILE_WIDTH)
		xScroll = (m_xWorld * TILE_WIDTH) - SURFACE_WIDTH;
	yScroll += y;
	if(yScroll < 0)
		yScroll = 0;
	else if(yScroll + SURFACE_HEIGHT > m_yWorld * TILE_HEIGHT)
		yScroll = (m_yWorld * TILE_HEIGHT) - SURFACE_HEIGHT;
	m_pMain->SetScroll(xScroll, yScroll);
	UpdateVisibleTiles();
}

HRESULT CMOMWorldEditor::SetupMap (INT xWorld, INT yWorld)
{
	HRESULT hr;

	m_xWorld = xWorld;
	m_yWorld = yWorld;

	INT nWorldCells = m_xWorld * m_yWorld;

	m_pArcanusWorld = __new MAPTILE[nWorldCells];
	CheckAlloc(m_pArcanusWorld);
	ZeroMemory(m_pArcanusWorld, sizeof(MAPTILE) * nWorldCells);

	m_pMyrrorWorld = __new MAPTILE[nWorldCells];
	CheckAlloc(m_pMyrrorWorld);
	ZeroMemory(m_pMyrrorWorld, sizeof(MAPTILE) * nWorldCells);

	Check(ResetWorldTiles(m_pArcanusWorld, m_xWorld, m_yWorld, m_mapArcanus));
	Check(ResetWorldTiles(m_pMyrrorWorld, m_xWorld, m_yWorld, m_mapMyrror));

Cleanup:
	if(FAILED(hr))
	{
		SafeDeleteArray(m_pArcanusWorld);
		SafeDeleteArray(m_pMyrrorWorld);

		m_xWorld = 0;
		m_yWorld = 0;
	}
	return hr;
}

HRESULT CMOMWorldEditor::LoadWorldFromJSON (TRStrMap<CTileSet*>& mapTileSet, IJSONObject* pMap, PCWSTR pcwzWorld, INT xWorld, INT yWorld, MAPTILE** ppWorld)
{
	HRESULT hr;
	MAPTILE* pWorld = NULL;
	INT nWorldCells = xWorld * yWorld;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONArray> srWorld;
	RSTRING rstrTile = NULL, rstrKey = NULL;

	pWorld = __new MAPTILE[nWorldCells];
	CheckAlloc(pWorld);
	ZeroMemory(pWorld, sizeof(MAPTILE) * nWorldCells);

	Check(pMap->FindNonNullValueW(pcwzWorld, &srv));
	Check(srv->GetArray(&srWorld));
	CheckIf(srWorld->Count() != nWorldCells, E_FAIL);

	for(INT i = 0; i < nWorldCells; i++)
	{
		MAPTILE* pCell = pWorld + i;
		TStackRef<IJSONObject> srCell;
		CTileSet* pTileSet;
		TArray<CTile*>* paTiles;

		Check(srWorld->GetObject(i, &srCell));

		srv.Release();
		Check(srCell->FindNonNullValueW(L"tile", &srv));
		Check(srv->GetString(&rstrTile));

		srv.Release();
		Check(srCell->FindNonNullValueW(L"key", &srv));
		Check(srv->GetString(&rstrKey));

		Check(mapTileSet.Find(rstrTile, &pTileSet));
		Check(pTileSet->FindFromKey(rstrKey, &paTiles));
		pCell->pTile = (*paTiles)[rand() % paTiles->Length()];

		srv.Release();
		if(SUCCEEDED(srCell->FindNonNullValueW(L"feature", &srv)))
			Check(srv->GetString(&pCell->rstrFeature));

		srv.Release();
		if(SUCCEEDED(srCell->FindNonNullValueW(L"data", &srv)))
			Check(srv->GetObject(&pCell->pData));

		srv.Release();
		if(SUCCEEDED(srCell->FindNonNullValueW(L"city", &srv)))
			Check(srv->GetObject(&pCell->pCity));

		srv.Release();
		if(SUCCEEDED(srCell->FindNonNullValueW(L"stack", &srv)))
			Check(srv->GetObject(&pCell->pStack));

		RStrRelease(rstrTile); rstrTile = NULL;
		RStrRelease(rstrKey); rstrKey = NULL;
	}

	*ppWorld = pWorld;

Cleanup:
	RStrRelease(rstrTile);
	RStrRelease(rstrKey);
	return hr;
}

HRESULT CMOMWorldEditor::SaveWorldToJSON (IJSONObject* pMap, PCWSTR pcwzWorld, INT xWorld, INT yWorld, MAPTILE* pWorld)
{
	HRESULT hr;
	INT nWorldCells = xWorld * yWorld;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONArray> srWorld;

	Check(JSONCreateArray(&srWorld));

	for(INT i = 0; i < nWorldCells; i++)
	{
		MAPTILE* pCell = pWorld + i;
		TStackRef<IJSONObject> srCell;

		Check(JSONCreateObject(&srCell));

		Check(JSONCreateString(pCell->pTile->GetTileSet(), &srv));
		Check(srCell->AddValueW(L"tile", srv));
		srv.Release();

		Check(JSONCreateString(pCell->pTile->GetKey(), &srv));
		Check(srCell->AddValueW(L"key", srv));
		srv.Release();

		if(pCell->rstrFeature)
		{
			Check(JSONCreateString(pCell->rstrFeature, &srv));
			Check(srCell->AddValueW(L"feature", srv));
			srv.Release();
		}

		if(pCell->pData)
		{
			Check(JSONWrapObject(pCell->pData, &srv));
			Check(srCell->AddValueW(L"data", srv));
			srv.Release();
		}

		if(pCell->pCity)
		{
			Check(JSONWrapObject(pCell->pCity, &srv));
			Check(srCell->AddValueW(L"city", srv));
			srv.Release();
		}

		if(pCell->pStack)
		{
			Check(JSONWrapObject(pCell->pStack, &srv));
			Check(srCell->AddValueW(L"stack", srv));
			srv.Release();
		}

		Check(JSONWrapObject(srCell, &srv));
		Check(srWorld->Add(srv));
		srv.Release();
	}

	Check(JSONWrapArray(srWorld, &srv));
	Check(pMap->AddValueW(pcwzWorld, srv));

Cleanup:
	return hr;
}

HRESULT CMOMWorldEditor::ReplaceCommand (CBaseGalleryCommand* pCommand)
{
	HRESULT hr;

	CheckAlloc(pCommand);

	SafeDelete(m_pCommand);
	m_pCommand = pCommand;
	hr = S_OK;

Cleanup:
	return hr;
}

HRESULT CMOMWorldEditor::BuildPlaceGrid (MAPTILE* pWorld, INT xTile, INT yTile, __out_ecount(9) CPlaceItem** ppPlaceItems)
{
	HRESULT hr = S_OK;

	// Fill in the surrounding tiles.  These might be changed.
	for(INT i = 0; i < ARRAYSIZE(c_rgDirections); i++)
	{
		CTile* pOther = NULL;
		INT x = xTile + c_rgDirections[i].x;
		INT y = yTile + c_rgDirections[i].y;

		// Wrap the sides of the world.
		if(x < 0)
			x = m_xWorld - 1;
		else if(x >= m_xWorld)
			x -= m_xWorld;

		if(y >= 0 && y < m_yWorld)
			pOther = pWorld[y * m_xWorld + x].pTile;

		ppPlaceItems[i] = __new CPlaceItem(m_pTileRules, pOther, x, y);
		CheckAlloc(ppPlaceItems[i]);
	}

	// Set the "center" tile (at the end of the array), which is the primary placement tile.
	ppPlaceItems[ARRAYSIZE(c_rgDirections)] = __new CPlaceItem(m_pTileRules, pWorld[yTile * m_xWorld + xTile].pTile, xTile, yTile);
	CheckAlloc(ppPlaceItems[ARRAYSIZE(c_rgDirections)]);

Cleanup:
	return hr;
}

VOID CMOMWorldEditor::GetTileKey (MAPTILE* pWorld, CPlaceItem* pItem, __in_ecount_opt(cPlaceItems) CPlaceItem** ppPlaceItems, INT cPlaceItems, PWSTR pwzKey)
{
	for(INT i = 0; i < ARRAYSIZE(c_rgDirections); i++)
	{
		INT x = pItem->m_x + c_rgDirections[i].x;
		INT y = pItem->m_y + c_rgDirections[i].y;
		RSTRING rstrOther = NULL;

		// Wrap the sides of the world.
		if(x < 0)
			x = m_xWorld - 1;
		else if(x >= m_xWorld)
			x -= m_xWorld;

		if(y >= 0 && y < m_yWorld)
		{
			for(INT n = 0; n < cPlaceItems; n++)
			{
				CPlaceItem* pOther = ppPlaceItems[n];
				if(pOther->m_x == x && pOther->m_y == y)
				{
					rstrOther = pOther->m_rstrTile;
					break;
				}
			}

			if(NULL == rstrOther)
				rstrOther = pWorld[m_xWorld * y + x].pTile->GetTileSet();

			if(pItem->IsSameTile(rstrOther))
				pwzKey[i] = L'0';
			else if(pItem->IsTileCompatible(L"special", rstrOther))
				pwzKey[i] = L'2';
			else
				pwzKey[i] = L'1';
		}
		else
			pwzKey[i] = L'0';
	}

	pwzKey[ARRAYSIZE(c_rgDirections)] = L'\0';
}

HRESULT CMOMWorldEditor::ClearTile (INT x, INT y, BOOL fActiveWorld)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeSprite> srSprite;

	CheckIfIgnore(!fActiveWorld, S_FALSE);
	CheckNoTrace(m_pMain->FindSpriteAt(m_pMapTileLayer->GetLayer(), x * TILE_WIDTH, y * TILE_HEIGHT, &srSprite));
	Check(m_pMapTileLayer->RemoveSprite(srSprite));
	srSprite.Release();

	if(SUCCEEDED(m_pMain->FindSpriteAt(m_nFeaturesLayer, x * TILE_WIDTH, y * TILE_HEIGHT, &srSprite)))
		Check(m_pMain->RemoveSprite(m_nFeaturesLayer, srSprite));
	srSprite.Release();

	if(SUCCEEDED(m_pMain->FindSpriteAt(m_nCitiesLayer, x * TILE_WIDTH, y * TILE_HEIGHT, &srSprite)))
		Check(m_pMain->RemoveSprite(m_nCitiesLayer, srSprite));

Cleanup:
	return hr;
}

HRESULT CMOMWorldEditor::PlaceSelectedTile (INT x, INT y)
{
	HRESULT hr;
	INT xTile = x / TILE_WIDTH;
	INT yTile = y / TILE_HEIGHT;

	CheckIf(NULL == m_pCommand, E_FAIL);
	Check(m_pCommand->Execute(this, xTile, yTile));

	m_fPainting = m_pCommand->ContinuePainting();
	if(!m_fPainting)
		SafeDelete(m_pCommand);

	Check(UpdateVisibleTiles());

Cleanup:
	return hr;
}

HRESULT CMOMWorldEditor::PlaceTile (MAPTILE* pWorld, INT xTile, INT yTile, TRStrMap<CTileSet*>* pmapTileSets, RSTRING rstrTile, BOOL fActiveWorld)
{
	HRESULT hr;
	WCHAR wzKey[12] = {0};
	CPlaceItem* rgpPlaceItems[9] = {0};
	CPlaceItem* pPlaced;
	RSTRING rstrTransition = NULL;

	Check(BuildPlaceGrid(pWorld, xTile, yTile, rgpPlaceItems));
	pPlaced = rgpPlaceItems[ARRAYSIZE(rgpPlaceItems) - 1];

	// Set the new tile onto the placed tile without actually picking a new tile sprite.
	pPlaced->SetTileOnly(rstrTile);

	for(INT i = 0; i < ARRAYSIZE(c_rgDirections); i++)
	{
		CPlaceItem* pItem = rgpPlaceItems[i];

		// If the other tile is the same type, then match up their keys.
		if(pItem->IsSameTile(rstrTile))
		{
			// Set the other tile's key to match back to this tile.
			if(SUCCEEDED(pItem->SetTileKey(pmapTileSets, ~static_cast<Dir::Value>(i), L'0')))
			{
				// This direction to the other tile is the same tile type.
				wzKey[i] = L'0';
			}
			else
			{
				// This direction to the other tile doesn't have a connecting tile.
				wzKey[i] = L'1';
			}
		}
		else if(pItem->IsTileCompatible(L"special", pPlaced->m_rstrTile))
		{
			// This direction might be able to use the special '2' connecting tile.
			if(SUCCEEDED(pItem->SetTileKey(pmapTileSets, ~static_cast<Dir::Value>(i), L'2')))
			{
				// The placed tile now connects as if it's the "same" tile.
				wzKey[i] = L'0';
			}
			else
			{
				// The special tile connection isn't available for that direction.
				wzKey[i] = L'1';
			}
		}
		else if(pPlaced->IsTileCompatible(L"border", pItem->m_rstrTile))
		{
			// The placed tile supports a border against the adjacent tile.
			wzKey[i] = L'1';
		}
		else if(pItem->IsTileCompatible(L"border", pPlaced->m_rstrTile))
		{
			// Set the other tile's key to border against the placed tile.
			if(SUCCEEDED(pItem->SetTileKey(pmapTileSets, ~static_cast<Dir::Value>(i), L'1')))
			{
				// The placed tile can use its '1' direction since the other tile will border correctly to it.
				wzKey[i] = L'1';
			}
			else
			{
				// We'll set a border tile anyway and try to make it work later.
				wzKey[i] = L'1';
			}
		}
		else
		{
			Check(pPlaced->GetTransitionTile(rstrTile, &rstrTransition));

			if(pPlaced->IsSameTile(rstrTransition))
				wzKey[i] = L'0';
			else
				wzKey[i] = L'1';

			pItem->SetTileOnly(rstrTransition);

			RStrRelease(rstrTransition);
			rstrTransition = NULL;
		}
	}

	// Update the tile for every grid item.
	for(INT i = 0; i < ARRAYSIZE(c_rgDirections); i++)
	{
		CPlaceItem* pItem = rgpPlaceItems[i];
		if(pItem->m_pTile)
		{
			WCHAR wzAdjacent[12];
			Dir::Value eDir = static_cast<Dir::Value>(i);

			GetTileKey(pWorld, pItem, rgpPlaceItems, ARRAYSIZE(rgpPlaceItems), wzAdjacent);

			hr = pItem->SetNewKey(pmapTileSets, wzAdjacent);
			if(SUCCEEDED(hr))
			{
				if(wzAdjacent[~eDir] == L'2')
					wzKey[eDir] = L'0';
			}
			else
			{
				wzAdjacent[~eDir] = L'1';
				if(SUCCEEDED(pItem->SetNewKey(pmapTileSets, wzAdjacent, L'1')))
				{
					// We've set the border for the adjacent tile.  Set the border for the placed tile.
					wzKey[eDir] = L'1';
				}
				else
				{
					// If the original key already has a '1', then try to set the tile with the original key.
					CheckIf(L'1' != pItem->m_wzKey[~eDir], E_FAIL);
					Check(pItem->SetNewKey(pmapTileSets, pItem->m_wzKey));
				}
			}
		}
	}

	// Update the placed tile.
	{
		CPlaceItem* pItem = rgpPlaceItems[ARRAYSIZE(c_rgDirections)];
		hr = pItem->SetNewKey(pmapTileSets, wzKey);
		if(FAILED(hr))
			Check(pItem->SetNewKey(pmapTileSets, wzKey, L'0'));
	}

	for(INT i = 0; i < ARRAYSIZE(rgpPlaceItems); i++)
	{
		CPlaceItem* pItem = rgpPlaceItems[i];
		if(pItem->m_pTile)
		{
			pWorld[pItem->m_y * m_xWorld + pItem->m_x].pTile = pItem->m_pTile;
			ClearTile(pItem->m_x, pItem->m_y, fActiveWorld);
		}
	}

Cleanup:
	RStrRelease(rstrTransition);
	for(INT i = 0; i < ARRAYSIZE(rgpPlaceItems); i++)
		__delete rgpPlaceItems[i];
	return hr;
}

HRESULT CMOMWorldEditor::PlaceOrModifyCity (MAPTILE* pWorld, INT xTile, INT yTile)
{
	HRESULT hr;
	MAPTILE* pTile = pWorld + yTile * m_xWorld + xTile;
	TStackRef<ISimbeyInterchangeSprite> srSprite;
	TStackRef<IJSONObject> srCity(pTile->pCity);
	CDialogHost host(m_hInstance);
	CCityDlg* pdlgCity = NULL;

	if(NULL == srCity)
		Check(JSONCreateObject(&srCity));

	pdlgCity = __new CCityDlg(srCity);
	CheckAlloc(pdlgCity);
	Check(host.Display(m_hwnd, pdlgCity));

	switch(host.GetReturnValue())
	{
	case IDOK:
		ReplaceInterface<IJSONObject>(pTile->pCity, srCity);
		__fallthrough;
	case IDC_DELETE:
		if(IDC_DELETE == host.GetReturnValue())
			SafeRelease(pTile->pCity);
		if(SUCCEEDED(m_pMain->FindSpriteAt(m_pMapTileLayer->GetLayer(), xTile * TILE_WIDTH, yTile * TILE_HEIGHT, &srSprite)))
		{
			m_pMain->RemoveSprite(m_pMapTileLayer->GetLayer(), srSprite);
			srSprite.Release();

			if(SUCCEEDED(m_pMain->FindSpriteAt(m_nFeaturesLayer, xTile * TILE_WIDTH, yTile * TILE_HEIGHT, &srSprite)))
			{
				m_pMain->RemoveSprite(m_nFeaturesLayer, srSprite);
				srSprite.Release();
			}

			if(SUCCEEDED(m_pMain->FindSpriteAt(m_nCitiesLayer, xTile * TILE_WIDTH, yTile * TILE_HEIGHT, &srSprite)))
			{
				m_pMain->RemoveSprite(m_nCitiesLayer, srSprite);
				srSprite.Release();
			}
		}
		Check(UpdateVisibleTiles());
		break;
	}

Cleanup:
	__delete pdlgCity;
	return hr;
}

HRESULT CMOMWorldEditor::LoadFeatures (ISimbeyInterchangeFile* pFeatures)
{
	HRESULT hr;
	DWORD cFeatures = pFeatures->GetLayerCount();
	RSTRING rstrName = NULL;

	for(DWORD i = 0; i < cFeatures; i++)
	{
		TStackRef<ISimbeyInterchangeFileLayer> srLayer;
		TStackRef<ISimbeyInterchangeSprite> srSprite;
		WCHAR wzName[MAX_PATH];

		Check(pFeatures->GetLayerByIndex(i, &srLayer));
		Check(srLayer->GetName(wzName, ARRAYSIZE(wzName)));

		PWSTR pwzExt = const_cast<PWSTR>(TStrRChr(wzName, L'.'));
		if(pwzExt)
		{
			*pwzExt = L'\0';
			Check(srLayer->SetName(wzName));
		}
		Check(RStrCreateW(TStrLenAssert(wzName), wzName, &rstrName));

		Check(sifCreateStaticSprite(srLayer, 0, 0, &srSprite));
		Check(m_mapFeatures.Add(rstrName, srSprite));
		srSprite.Detach();

		RStrRelease(rstrName); rstrName = NULL;
	}

Cleanup:
	RStrRelease(rstrName);
	return hr;
}

HRESULT CMOMWorldEditor::LoadCityTiles (ISimbeyInterchangeFile* pCityTiles)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeFile> srFlags;
	INT nTile = 1;
	static COLORREF rgcrFlags[] =
	{
		RGB(255, 64, 64),
		RGB(64, 255, 64),
		RGB(64, 64, 255),
		RGB(255, 255, 64),
		RGB(64, 255, 255),
		RGB(64, 255, 255),
		RGB(200, 0, 200),
		RGB(255, 160, 0),
		RGB(80, 80, 88),
		RGB(255, 120, 190),
		RGB(70, 40, 120),
		RGB(0, 160, 190),
		RGB(230, 190, 160),
		RGB(80, 160, 120),
		RGB(100, 88, 70),
		RGB(212, 255, 60),
		RGB(255, 255, 255)
	};

	Check(sifCreateNew(&srFlags));

	for(;;)
	{
		TStackRef<ISimbeyInterchangeFileLayer> srNormal, srWalled;
		WCHAR wzTile[MAX_PATH];
		INT cchTile;
		CITYTILE* pTile;

		Check(Formatting::TPrintF(wzTile, ARRAYSIZE(wzTile), &cchTile, L"size%d.png", nTile));
		if(FAILED(pCityTiles->FindLayer(wzTile, &srNormal, NULL)))
			break;
		cchTile -= 4;
		Check(TStrCchCpy(wzTile + cchTile, ARRAYSIZE(wzTile) - cchTile, L"w.png"));
		Check(pCityTiles->FindLayer(wzTile, &srWalled, NULL));

		Check(m_aCityTiles.AppendSlot(&pTile));

		for(INT n = 0; n < ARRAYSIZE(rgcrFlags); n++)
		{
			TStackRef<ISimbeyInterchangeFileLayer> srColorized;
			Check(ColorizeLayer(srNormal, rgcrFlags[n], srFlags, &srColorized));
			Check(sifCreateStaticSprite(srColorized, -6, -6, pTile->pNormal + n));

			srColorized.Release();
			Check(ColorizeLayer(srWalled, rgcrFlags[n], srFlags, &srColorized));
			Check(sifCreateStaticSprite(srColorized, -6, -6, pTile->pWalled + n));
		}

		nTile++;
	}

Cleanup:
	if(srFlags)
		srFlags->Close();
	return hr;
}

HRESULT CMOMWorldEditor::LoadTerrain (VOID)
{
	HRESULT hr;
	TStackRef<CSIFPackage> srArcanus, srMyrror;

	Check(m_pPackage->OpenDirectory(SLP(L"overland\\terrain\\arcanus"), &srArcanus));
	Check(LoadTileSets(srArcanus, m_mapArcanus));
	Check(CreateGallery(m_mapArcanus, &m_pArcanusTerrain));

	Check(m_pPackage->OpenDirectory(SLP(L"overland\\terrain\\myrror"), &srMyrror));
	Check(LoadTileSets(srMyrror, m_mapMyrror));
	Check(CreateGallery(m_mapMyrror, &m_pMyrrorTerrain));

Cleanup:
	return hr;
}

HRESULT CMOMWorldEditor::CreateGallery (TRStrMap<CTileSet*>& mapTiles, CTerrainGallery** ppGallery)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeFile> srSIF;

	Check(sifCreateNew(&srSIF));
	for(sysint i = 0; i < mapTiles.Length(); i++)
	{
		RSTRING rstrTileSet = mapTiles.GetKey(i);

		if(0 != TStrCmpIAssert(RStrToWide(rstrTileSet), L"shore"))
		{
			CTileSet* pTileSet = *(mapTiles.GetValuePtr(i));
			TArray<CTile*>* pTiles;

			if(SUCCEEDED(pTileSet->FindFromKey(RSTRING_CAST(L"00000000"), &pTiles)))
			{
				TStackRef<ISimbeyInterchangeSprite> srSprite;
				TStackRef<ISimbeyInterchangeFileLayer> srLayer;
				PBYTE pBits;
				INT nWidth, nHeight;

				Check((*pTiles)[0]->CreateSprite(&srSprite));
				Check(srSprite->GetFrameImage(&pBits, &nWidth, &nHeight));
				Check(srSIF->AddLayerFromBits(nWidth, nHeight, pBits, 32, nWidth * 4, &srLayer, NULL));
				Check(srLayer->SetName(RStrToWide(rstrTileSet)));
			}
		}
	}

	*ppGallery = __new CTerrainGallery(m_pRibbon, srSIF);
	CheckAlloc(*ppGallery);

Cleanup:
	if(FAILED(hr) && srSIF)
		srSIF->Close();
	return hr;
}

HRESULT CMOMWorldEditor::SetupMouse (VOID)
{
	HRESULT hr;
	sysint nMouseLayer;
	TStackRef<ISimbeyInterchangeFile> srMouse;
	TStackRef<ISimbeyInterchangeFileLayer> srLayer;

	Check(m_pMain->AddLayer(FALSE, FALSE, RGB(0, 0, 0), &nMouseLayer));
	Check(m_pPackage->OpenSIF(L"mouse\\mouse.sif", &srMouse));
	Check(srMouse->GetLayerByIndex(0, &srLayer));
	Check(sifCreateStaticSprite(srLayer, 0, 0, &m_pMouse));
	Check(m_pMain->AddSprite(nMouseLayer, m_pMouse, NULL));

Cleanup:
	if(srMouse)
		srMouse->Close();
	return hr;
}

HRESULT CMOMWorldEditor::ColorizeLayer (ISimbeyInterchangeFileLayer* pLayer, COLORREF crColorize, ISimbeyInterchangeFile* pStorage, __deref_out ISimbeyInterchangeFileLayer** ppColorized)
{
	HRESULT hr;
	PBYTE pBits;
	DWORD cb;
	USHORT nSquare, nWidth, nHeight;
	LONG x, y;

	Check(pLayer->GetLayerLocation(&nSquare, &x, &y, &nWidth, &nHeight));
	Check(pLayer->GetBitsPtr(&pBits, &cb));

	Check(pStorage->AddLayerFromBits(nWidth, nHeight, pBits, 32, nWidth * 4, ppColorized, NULL));
	Check((*ppColorized)->GetBitsPtr(&pBits, &cb));

	for(USHORT y = 0; y < nHeight; y++)
	{
		for(USHORT x = 0; x < nWidth; x++)
		{
			// If both the red and blue channels are zero,
			// then this is a pixel we want to recolorize.
			if(pBits[0] == 0 && pBits[2] == 0)
			{
				BYTE fAmount = pBits[1];
				pBits[0] = MulDiv(GetRValue(crColorize), fAmount, 255);
				pBits[1] = MulDiv(GetGValue(crColorize), fAmount, 255);
				pBits[2] = MulDiv(GetBValue(crColorize), fAmount, 255);
			}

			pBits += sizeof(DWORD);
		}
	}

Cleanup:
	return hr;
}

HRESULT CMOMWorldEditor::LoadTileSets (CSIFPackage* pPackage, __out TRStrMap<CTileSet*>& mapTiles)
{
	HRESULT hr;
	TStackRef<IJSONArray> srTileSets;
	sysint cTiles;
	RSTRING rstrName = NULL;

	Check(pPackage->GetDirectory(&srTileSets));
	cTiles = srTileSets->Count();

	for(sysint i = 0; i < cTiles; i++)
	{
		TStackRef<IJSONObject> srTileSetDir;
		TStackRef<IJSONValue> srvName;
		TStackRef<CSIFPackage> srTileSet;

		Check(srTileSets->GetObject(i, &srTileSetDir));
		Check(srTileSetDir->FindValueW(L"name", &srvName));
		Check(srvName->GetString(&rstrName));
		Check(pPackage->OpenDirectory(RStrToWide(rstrName), RStrLen(rstrName), &srTileSet));
		Check(LoadTileSet(srTileSet, rstrName, mapTiles));

		RStrRelease(rstrName); rstrName = NULL;
	}

Cleanup:
	RStrRelease(rstrName);
	return hr;
}

HRESULT CMOMWorldEditor::LoadTileSet (CSIFPackage* pPackage, RSTRING rstrName, __out TRStrMap<CTileSet*>& mapTiles)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeFile> srSIF;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONObject> srData;
	TStackRef<IJSONArray> srKeys;
	sysint cKeys;
	CTileSet* pTileSet = NULL;
	RSTRING rstrKey = NULL;

	Check(pPackage->OpenSIF(L"tiles.sif", &srSIF));
	Check(pPackage->GetJSONData(SLP(L"tiles.json"), &srv));
	Check(srv->GetObject(&srData));
	srv.Release();

	Check(srData->FindNonNullValueW(L"keys", &srv));
	Check(srv->GetArray(&srKeys));
	srv.Release();

	pTileSet = __new CTileSet(rstrName);
	CheckAlloc(pTileSet);

	cKeys = srKeys->Count();
	for(sysint i = 0; i < cKeys; i++)
	{
		TStackRef<IJSONObject> srKey;
		TStackRef<IJSONArray> srVariants;

		Check(srKeys->GetObject(i, &srKey));

		srv.Release();
		Check(srKey->FindNonNullValueW(L"key", &srv));
		Check(srv->GetString(&rstrKey));

		srv.Release();
		Check(srKey->FindNonNullValueW(L"variants", &srv));
		Check(srv->GetArray(&srVariants));

		Check(LoadKeyVariants(srSIF, pTileSet, rstrKey, srVariants));

		RStrRelease(rstrKey); rstrKey = NULL;
	}

	Check(mapTiles.Add(rstrName, pTileSet));
	pTileSet = NULL;

Cleanup:
	RStrRelease(rstrKey);
	__delete pTileSet;
	if(srSIF)
		srSIF->Close();
	return hr;
}

HRESULT CMOMWorldEditor::LoadKeyVariants (ISimbeyInterchangeFile* pSIF, CTileSet* pTileSet, RSTRING rstrKey, IJSONArray* pVariants)
{
	HRESULT hr;
	sysint cVariants = pVariants->Count();
	RSTRING rstrFrame = NULL;

	CheckIf(0 == cVariants, E_UNEXPECTED);

	for(sysint i = 0; i < cVariants; i++)
	{
		TStackRef<IJSONValue> srv;
		TStackRef<IJSONArray> srFrames;
		TStackRef<ISimbeyInterchangeFileLayer> srLayer;
		sysint cFrames;

		Check(pVariants->GetValue(i, &srv));
		Check(srv->GetArray(&srFrames));
		srv.Release();

		cFrames = srFrames->Count();
		if(1 == cFrames)
		{
			TStackRef<ISimbeyInterchangeSprite> srSprite;

			Check(srFrames->GetString(0, &rstrFrame));
			Check(pSIF->FindLayer(RStrToWide(rstrFrame), &srLayer, NULL));
			Check(sifCreateStaticSprite(srLayer, 0, 0, &srSprite));
			Check(pTileSet->AddVariant(rstrKey, srSprite));
			srLayer.Release();
			RStrRelease(rstrFrame); rstrFrame = NULL;
		}
		else
		{
			TStackRef<ISimbeyInterchangeAnimator> srAnimator;

			Check(sifCreateAnimator(cFrames, 1, &srAnimator));
			for(sysint n = 0; n < cFrames; n++)
			{
				Check(srAnimator->AddFrame(0, 8, n, 0, 0));
				Check(srFrames->GetString(n, &rstrFrame));
				Check(pSIF->FindLayer(RStrToWide(rstrFrame), &srLayer, NULL));
				Check(srAnimator->SetImage(n, FALSE, srLayer, FALSE));
				srLayer.Release();
				RStrRelease(rstrFrame); rstrFrame = NULL;
			}

			Check(pTileSet->AddVariant(rstrKey, srAnimator));
		}
	}

Cleanup:
	RStrRelease(rstrFrame);
	return hr;
}

HRESULT CMOMWorldEditor::SaveWindowPosition (HWND hwnd, PCWSTR pcwzValueName)
{
	HRESULT hr;

	if(hwnd)
	{
		WINDOWPLACEMENT wp = {0};
		wp.length = sizeof(WINDOWPLACEMENT);

		if(GetWindowPlacement(hwnd, &wp))
		{
			HKEY hKey;

			hr = HRESULT_FROM_WIN32(RegOpenKeyW(HKEY_CURRENT_USER, c_wzRegAppKey, &hKey));
			if(FAILED(hr))
				hr = HRESULT_FROM_WIN32(RegCreateKeyW(HKEY_CURRENT_USER, c_wzRegAppKey, &hKey));

			if(SUCCEEDED(hr))
			{
				// Normalize the scaled window placement to 96 DPI
				DPI::NormalizeScaledPoint(&wp.ptMaxPosition);
				DPI::NormalizeScaledPoint(&wp.ptMinPosition);
				DPI::NormalizeScaledRectSize(&wp.rcNormalPosition);

				hr = HRESULT_FROM_WIN32(RegSetValueExW(hKey, pcwzValueName, NULL, REG_BINARY, (LPBYTE)&wp, sizeof(WINDOWPLACEMENT)));

				RegCloseKey(hKey);
			}
		}
		else
			hr = HRESULT_FROM_WIN32(GetLastError());
	}
	else
		hr = E_INVALIDARG;

	return hr;
}

HRESULT CMOMWorldEditor::LoadWindowPosition (HWND hwnd, PCWSTR pcwzValueName)
{
	HRESULT hr;

	if(hwnd)
	{
		HKEY hKey;

		hr = HRESULT_FROM_WIN32(RegOpenKeyW(HKEY_CURRENT_USER, c_wzRegAppKey, &hKey));
		if(SUCCEEDED(hr))
		{
			WINDOWPLACEMENT wp = {0};
			DWORD cbData = sizeof(wp);

			hr = HRESULT_FROM_WIN32(RegQueryValueExW(hKey, pcwzValueName, NULL, NULL, (LPBYTE)&wp, &cbData));
			if(SUCCEEDED(hr))
			{
				wp.length = sizeof(WINDOWPLACEMENT);

				// Normalize the scaled window placement from 96 DPI
				DPI::ScaleNormalizedPoint(&wp.ptMaxPosition);
				DPI::ScaleNormalizedPoint(&wp.ptMinPosition);
				DPI::ScaleNormalizedRectSize(&wp.rcNormalPosition);

				if(SetWindowPlacement(hwnd, &wp))
					hr = S_OK;
				else
					hr = HRESULT_FROM_WIN32(GetLastError());
			}

			RegCloseKey(hKey);
		}
	}
	else
		hr = E_INVALIDARG;

	return hr;
}
