#include <windows.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Core\MemoryStream.h"
#include "Library\Util\Formatting.h"
#include "Library\Util\FileStream.h"
#include "Library\Util\StreamHelpers.h"
#include "Library\Util\TextHelpers.h"
#include "Library\Util\Registry.h"
#include "Library\Window\DialogHost.h"
#include "Library\DPI.h"
#include "Library\ChooseFile.h"
#include "Published\JSON.h"
#include "Ribbon.h"
#include "RibbonMappings.h"
#include "TileRules.h"
#include "NewWorldDlg.h"
#include "CityDlg.h"
#include "HeightMapGenerator.h"
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
// CSimpleRNG
///////////////////////////////////////////////////////////////////////////////

class CSimpleRNG : public IRandomNumber
{
public:
	CSimpleRNG (DWORD dwSeed)
	{
		srand(dwSeed);
	}

	// IRandomNumber
	virtual INT Next (VOID)
	{
		return rand();
	}

	virtual INT Next (INT nMax)
	{
		return rand() % nMax;
	}
};

///////////////////////////////////////////////////////////////////////////////
// CGeneratorGallery
///////////////////////////////////////////////////////////////////////////////

CGeneratorGallery::CGeneratorGallery (CSIFRibbon* pRibbon, IJSONArray* pGenerators) :
	m_pRibbon(pRibbon),
	m_pGenerators(pGenerators),
	m_idxSelection(0)
{
	m_pRibbon->AddRef();
	m_pGenerators->AddRef();
}

CGeneratorGallery::~CGeneratorGallery ()
{
	m_pGenerators->Release();
	m_pRibbon->Release();
}

UINT CGeneratorGallery::GetSelection (VOID)
{
	return m_idxSelection;
}

VOID CGeneratorGallery::SetSelection (UINT idxSelection)
{
	m_idxSelection = idxSelection;
}

// IUICollection

HRESULT STDMETHODCALLTYPE CGeneratorGallery::GetCount (__out UINT32* pcItems)
{
	*pcItems = m_pGenerators->Count();
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CGeneratorGallery::GetItem (UINT32 index, __deref_out_opt IUnknown** item)
{
	HRESULT hr;
	TStackRef<IJSONObject> srGenerator;
	TStackRef<IJSONValue> srv;
	TStackRef<CStringGalleryItem> srItem;
	RSTRING rstrName = NULL;

	Check(m_pGenerators->GetObject(index, &srGenerator));
	Check(srGenerator->FindNonNullValueW(L"name", &srv));
	Check(srv->GetString(&rstrName));
	Check(CStringGalleryItem::Create(m_pRibbon, &srItem));
	srItem->SetObject(srGenerator);
	Check(srItem->SetItemText(RStrToWide(rstrName)));
	srItem->SetItemIcon(ID_RANDOM);
	*item = static_cast<IUISimplePropertySet*>(srItem.Detach());

Cleanup:
	RStrRelease(rstrName);
	return hr;
}

HRESULT STDMETHODCALLTYPE CGeneratorGallery::Add (IUnknown* item)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CGeneratorGallery::Insert (UINT32 index, IUnknown* item)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CGeneratorGallery::RemoveAt (UINT32 index)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CGeneratorGallery::Replace (UINT32 indexReplaced, IUnknown* itemReplaceWith)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CGeneratorGallery::Clear (VOID)
{
	return E_NOTIMPL;
}

///////////////////////////////////////////////////////////////////////////////
// CTerrainGallery
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
	Check(RStrCompareRStr(m_pWorld[yTile * m_xWorld + xTile].pTile->GetTileSet()->GetName(), rstrTile, &nResult));
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
	m_pGenerators(NULL),
	m_pProportions(NULL),
	m_pGeneratorGallery(NULL),
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
	SafeRelease(m_pGeneratorGallery);
	SafeRelease(m_pProportions);
	SafeRelease(m_pGenerators);
	SafeDelete(m_pTileRules);

	m_mapSmoothingSystems.DeleteAll();

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
	TStackRef<IJSONObject> srData, srSmoothing;
	TStackRef<IJSONValue> srvRules, srvSmoothing, srvGenerators, srvProportions;
	ISimbeyInterchangeFile* pSIF = NULL;

	Check(CSIFRibbon::Create(DPI::Scale, &m_pRibbon));

	m_pSurface = __new CInteractiveSurface(SURFACE_WIDTH, SURFACE_HEIGHT);
	CheckAlloc(m_pSurface);
	m_pSurface->EnableClear(RGB(255, 255, 255));

	Check(LoadPackage());

	Check(m_pPackage->GetJSONData(SLP(L"overland\\terrain\\smoothing.json"), &srvSmoothing));
	Check(srvSmoothing->GetObject(&srSmoothing));
	Check(LoadSmoothing(srSmoothing));

	Check(m_pPackage->GetJSONData(SLP(L"overland\\terrain\\rules.json"), &srvRules));

	m_pTileRules = __new CTileRules;
	CheckAlloc(m_pTileRules);
	Check(m_pTileRules->Initialize(m_mapSmoothingSystems, srvRules));

	Check(m_pPackage->GetJSONData(SLP(L"overland\\terrain\\generators.json"), &srvGenerators));
	Check(srvGenerators->GetArray(&m_pGenerators));

	Check(m_pPackage->GetJSONData(SLP(L"overland\\terrain\\proportions.json"), &srvProportions));
	Check(srvProportions->GetArray(&m_pProportions));

	m_pGeneratorGallery = __new CGeneratorGallery(m_pRibbon, m_pGenerators);
	CheckAlloc(m_pGeneratorGallery);

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
	Check(static_cast<CInteractiveCanvas*>(m_pMain)->AddInteractiveLayer(TRUE, LayerRender::Masked, RGB(0, 0, 0), this, &m_pMapTileLayer));
	Check(m_pMain->AddLayer(FALSE, LayerRender::Masked, RGB(0, 0, 0), &m_nFeaturesLayer));
	Check(m_pMain->AddLayer(FALSE, LayerRender::Masked, RGB(0, 0, 0), &m_nCitiesLayer));

	Check(SetupMouse());

	Check(SetupMap(64, 48, TRUE));

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
		while(PeekMessage(&msg,NULL,0,0,PM_REMOVE))
		{
			if(msg.message == WM_QUIT)
				return;
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
		CheckIf(ID_TERRAIN != commandId && ID_FEATURES != commandId && ID_RANDOM != commandId, E_NOTIMPL);
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
		case ID_RANDOM:
			m_pGeneratorGallery->SetSelection(currentValue->uintVal);
			Check(GenerateRandomWorlds());
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
		else if(ID_RANDOM == commandId)
		{
			newValue->uintVal = m_pGeneratorGallery->GetSelection();
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
		else if(ID_RANDOM == commandId)
			srSource = m_pGeneratorGallery;
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
	Registry::LoadWindowPosition(m_hwnd, c_wzRegAppKey, c_wzAppWindowPos, &nCmdShow);
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
	Registry::SaveWindowPosition(m_hwnd, c_wzRegAppKey, c_wzAppWindowPos);
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

	Check(SetupMap(dlgNewWorld.m_nWidth, dlgNewWorld.m_nHeight, TRUE));

	m_pMain->SetScroll((m_xWorld * TILE_WIDTH) / 2 - SURFACE_WIDTH / 2,
		(m_yWorld * TILE_HEIGHT) / 2 - SURFACE_HEIGHT / 2);
	UpdateVisibleTiles();

Cleanup:
	return hr;
}

HRESULT CMOMWorldEditor::GenerateRandomWorlds (VOID)
{
	HRESULT hr;
	TRStrMap<CTileSet*>* prgmapTileSets[2] = { &m_mapArcanus, &m_mapMyrror };
	COORD_SYSTEM coords;
	CSimpleRNG rng(GetTickCount());
	TStackRef<IJSONObject> srGenerator, srProportion, srZone, srTowers;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONArray> srBlobs;
	INT nDepth, nZoneWidth, nZoneHeight, nTundraRowCount;
	INT nPercentageOfMapIsLand, nPercentageOfLandIsHills, nPercentageOfHillsAreMountains;
	RSTRING rstrTile = NULL;

	Check(m_pGenerators->GetObject(m_pGeneratorGallery->GetSelection(), &srGenerator));
	Check(m_pProportions->GetObject(rng.Next(m_pProportions->Count()), &srProportion));

	Check(srGenerator->FindNonNullValueW(L"width", &srv));
	Check(srv->GetInteger(&coords.nWidth));
	srv.Release();

	Check(srGenerator->FindNonNullValueW(L"height", &srv));
	Check(srv->GetInteger(&coords.nHeight));
	srv.Release();

	Check(srGenerator->FindNonNullValueW(L"depth", &srv));
	Check(srv->GetInteger(&nDepth));
	srv.Release();

	Check(srGenerator->FindNonNullValueW(L"zone", &srv));
	Check(srv->GetObject(&srZone));
	srv.Release();

	Check(srZone->FindNonNullValueW(L"width", &srv));
	Check(srv->GetInteger(&nZoneWidth));
	srv.Release();

	Check(srZone->FindNonNullValueW(L"height", &srv));
	Check(srv->GetInteger(&nZoneHeight));
	srv.Release();

	Check(srGenerator->FindNonNullValueW(L"towers", &srv));
	Check(srv->GetObject(&srTowers));
	srv.Release();

	Check(srProportion->FindNonNullValueW(L"tundraRowCount", &srv));
	Check(srv->GetInteger(&nTundraRowCount));
	srv.Release();

	Check(srProportion->FindNonNullValueW(L"percentageOfMapIsLand", &srv));
	Check(srv->GetInteger(&nPercentageOfMapIsLand));
	srv.Release();

	Check(srProportion->FindNonNullValueW(L"percentageOfLandIsHills", &srv));
	Check(srv->GetInteger(&nPercentageOfLandIsHills));
	srv.Release();

	Check(srProportion->FindNonNullValueW(L"percentageOfHillsAreMountains", &srv));
	Check(srv->GetInteger(&nPercentageOfHillsAreMountains));
	srv.Release();

	Check(srProportion->FindNonNullValueW(L"blobs", &srv));
	Check(srv->GetArray(&srBlobs));
	srv.Release();

	DeleteWorld(m_pArcanusWorld);
	DeleteWorld(m_pMyrrorWorld);

	coords.fWrapsLeftToRight = TRUE;
	coords.fWrapsTopToBottom = FALSE;
	Check(SetupMap(coords.nWidth, coords.nHeight, FALSE));

	m_pMain->SetScroll((m_xWorld * TILE_WIDTH) / 2 - SURFACE_WIDTH / 2,
		(m_yWorld * TILE_HEIGHT) / 2 - SURFACE_HEIGHT / 2);

	{
		MAPTILE* prgWorlds[2] = { m_pArcanusWorld, m_pMyrrorWorld };
		CHeightMapGenerator HeightMap(&rng, nZoneWidth, nZoneHeight, nTundraRowCount);

		Check(HeightMap.Initialize(coords));

		m_pMapTileLayer->Clear();
		m_pMain->ClearLayer(m_nFeaturesLayer);
		m_pMain->ClearLayer(m_nCitiesLayer);

		DOUBLE dblLandTileCountTimes100 = (DOUBLE)(coords.nWidth * coords.nHeight * nPercentageOfMapIsLand);

		for(INT nPlane = 0; nPlane < ARRAYSIZE(prgWorlds); nPlane++)
		{
			TRStrMap<CTileSet*>* pmapTileSet = prgmapTileSets[nPlane];
			BOOL fActiveWorld = static_cast<World::Type>(nPlane) == m_eType;
			MAPTILE* pWorld = prgWorlds[nPlane];

			Check(HeightMap.GenerateHeightMap());

			Check(SetHighestTiles(pmapTileSet, pWorld, HeightMap, SLP(L"grasslands"), (INT)(dblLandTileCountTimes100 / 100.0 + 0.5), fActiveWorld));
			Check(SetHighestTiles(pmapTileSet, pWorld, HeightMap, SLP(L"hills"), (INT)(dblLandTileCountTimes100 * (DOUBLE)nPercentageOfLandIsHills / 10000.0 + 0.5), fActiveWorld));
			Check(SetHighestTiles(pmapTileSet, pWorld, HeightMap, SLP(L"mountains"), (INT)(dblLandTileCountTimes100 * (DOUBLE)nPercentageOfLandIsHills * (DOUBLE)nPercentageOfHillsAreMountains / 1000000.0 + 0.5), fActiveWorld));

			Check(MakeTundra(pmapTileSet, pWorld, m_xWorld, m_yWorld, fActiveWorld));

			for(sysint i = 0; i < srBlobs->Count(); i++)
			{
				TStackRef<IJSONObject> srBlob;
				INT nPercentage, nEachAreaTileCount;

				Check(srBlobs->GetObject(i, &srBlob));

				Check(srBlob->FindNonNullValueW(L"percentage", &srv));
				Check(srv->GetInteger(&nPercentage));
				srv.Release();

				Check(srBlob->FindNonNullValueW(L"eachAreaTileCount", &srv));
				Check(srv->GetInteger(&nEachAreaTileCount));
				srv.Release();

				Check(srBlob->FindNonNullValueW(L"tile", &srv));
				Check(srv->GetString(&rstrTile));
				srv.Release();

				Check(PlaceBlob(&rng, pmapTileSet, pWorld, rstrTile, (INT)(dblLandTileCountTimes100 * (DOUBLE)nPercentage / 10000.0 + 0.5), nEachAreaTileCount));
				RStrRelease(rstrTile); rstrTile = NULL;
			}
		}

		Check(PlaceTowersOfWizardry(&rng, prgmapTileSets, prgWorlds, srTowers));
	}

	Check(UpdateVisibleTiles());

Cleanup:
	RStrRelease(rstrTile);
	return hr;
}

HRESULT CMOMWorldEditor::SetHighestTiles (TRStrMap<CTileSet*>* pmapTileSets, MAPTILE* pWorld, CHeightMapGenerator& heightMap, PCWSTR pcwzTile, INT cchTile, INT nDesiredTileCount, BOOL fActiveWorld)
{
	HRESULT hr;
	TArray<POINT> aTiles;
	RSTRING rstrTile = NULL;

	Check(heightMap.SetHighestTiles(nDesiredTileCount, &aTiles));
	Check(RStrCreateW(cchTile, pcwzTile, &rstrTile));

	for(sysint i = 0; i < aTiles.Length(); i++)
	{
		POINT& pt = aTiles[i];
		Check(PlaceTile(pWorld, pt.x, pt.y, pmapTileSets, rstrTile, fActiveWorld));
	}

Cleanup:
	RStrRelease(rstrTile);
	return hr;
}

HRESULT CMOMWorldEditor::PlaceBlob (IRandomNumber* pRand, TRStrMap<CTileSet*>* pmapTileSets, MAPTILE* pWorld, RSTRING rstrTile, INT nDesiredTileCount, INT nEachAreaTileCount)
{
	HRESULT hr;
	INT cTotalTilesPlaced = 0;
	CTileSet* pGrass, *pPlace;

	Check(pmapTileSets->Find(RSTRING_CAST(L"grasslands"), &pGrass));
	Check(pmapTileSets->Find(rstrTile, &pPlace));

	while(cTotalTilesPlaced < nDesiredTileCount)
	{
		INT cTilesPlaced;

		Check(PlaceSingleBlob(pRand, pmapTileSets, pGrass, pPlace, pWorld, rstrTile, nEachAreaTileCount, &cTilesPlaced));
		if(0 == cTilesPlaced)
			break;

		cTotalTilesPlaced += cTilesPlaced;
	}

Cleanup:
	return hr;
}

HRESULT CMOMWorldEditor::PlaceSingleBlob (IRandomNumber* pRand, TRStrMap<CTileSet*>* pmapTileSets, CTileSet* pReplace, CTileSet* pPlace, MAPTILE* pWorld, RSTRING rstrTile, INT nEachAreaTileCount, __out INT* pcTilesPlaced)
{
	HRESULT hr;
	CMapArea mapStarting;
	MAPTILE* pWorldPtr = pWorld;
	POINT pt;
	INT cTilesPlaced = 0;

	// Make a list of all the possible start locations
	for(pt.y = 0; pt.y < m_yWorld; pt.y++)
	{
		for(pt.x = 0; pt.x < m_xWorld; pt.x++)
		{
			if(pReplace == pWorldPtr[pt.x].pTile->GetTileSet())
				Check(mapStarting.Add(pt));
		}

		pWorldPtr += m_xWorld;
	}

	// Pick a random start location
	CheckIf(0 == mapStarting.Length(), S_FALSE);
	pt = mapStarting[pRand->Next(mapStarting.Length())];

	// Work out how big we'll aim to make this blob, this will be +/- 50% of the average size
	INT nBlobSize = pRand->Next(nEachAreaTileCount) + (nEachAreaTileCount / 2);

	{
		// Create an area to keep track of the tiles in this blob
		CMapArea thisBlob, mapEnlarged;

		// Always set one tile (regardless of nBlobSize)
		for(;;)
		{
			// Set this tile
			Check(PlaceTile(pWorld, pt.x, pt.y, pmapTileSets, pPlace->GetName(), FALSE));
			Check(thisBlob.Add(pt));
			cTilesPlaced++;
			nBlobSize--;

			// Look for a neighboring tile
			if(0 == nBlobSize)
				break;

			// Create a ring around the current blob, i.e. this will tell us all the possible cells we could expand the blob into
			for(sysint i = 0; i < thisBlob.Length(); i++)
			{
				POINT ptItem = thisBlob[i];

				for(INT n = 0; n < ARRAYSIZE(c_rgDirections); n++)
				{
					pt.y = ptItem.y + c_rgDirections[n].y;
					if(pt.y >= 0 && pt.y < m_yWorld)
					{
						pt.x = ptItem.x + c_rgDirections[n].x;
						if(pt.x < 0)
							pt.x += m_xWorld;
						else if(pt.x >= m_xWorld)
							pt.x -= m_xWorld;

						// Not already in the blob, not already in the ring, and is a valid candidate for expansion.
						if(!thisBlob.Has(pt) && !mapEnlarged.Has(pt) && mapStarting.Has(pt))
							Check(mapEnlarged.Add(pt));
					}
				}
			}

			if(0 == mapEnlarged.Length())
				break;
			pt = mapEnlarged[pRand->Next(mapEnlarged.Length())];
			mapEnlarged.Clear();
		}
	}

Cleanup:
	*pcTilesPlaced = cTilesPlaced;
	return hr;
}

HRESULT CMOMWorldEditor::PlaceTowersOfWizardry (IRandomNumber* pRand, TRStrMap<CTileSet*>** prgmapTileSets, MAPTILE** prgWorlds, IJSONObject* pTowers)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	TArray<POINT> aTowers;
	INT nSeparation, nCount;
	CMapArea mapArea;
	RSTRING rstrTower = NULL;
	RSTRING rstrGrass = NULL;

	Check(RStrCreateW(LSP(L"towerOfWizardry-uncleared"), &rstrTower));
	Check(RStrCreateW(LSP(L"grasslands"), &rstrGrass));

	Check(pTowers->FindNonNullValueW(L"separation", &srv));
	Check(srv->GetInteger(&nSeparation));
	srv.Release();

	Check(pTowers->FindNonNullValueW(L"count", &srv));
	Check(srv->GetInteger(&nCount));

	for(INT nTower = 0; nTower < nCount; nTower++)
	{
		for(;;)
		{
			INT nWorldPtr = 0;
			POINT pt;

			// Build initial list of suitable locations - we do this on each loop because we may reduce the separation
			// Avoid map cells which are tundra on either plane - this avoids putting towers too close to the top or bottom
			mapArea.Clear();
			for(pt.y = 0; pt.y < m_yWorld; pt.y++)
			{
				for(pt.x = 0; pt.x < m_xWorld; pt.x++)
				{
					BOOL fPossible = TRUE;

					for(INT nPlane = 0; nPlane < 2; nPlane++)
					{
						MAPTILE* pWorld = prgWorlds[nPlane];

						if(pWorld[nWorldPtr + pt.x].pTile->GetTileSet()->IsTileSet(L"tundra"))
						{
							fPossible = FALSE;
							break;
						}
					}

					if(fPossible)
					{
						for(sysint i = 0; i < aTowers.Length(); i++)
						{
							POINT& ptTower = aTowers[i];

							if(abs(pt.y - ptTower.y) <= nSeparation)
							{
								if(abs(pt.x - ptTower.x) <= nSeparation ||
									abs(pt.x - (ptTower.x + m_xWorld)) <= nSeparation ||
									abs(pt.x - (ptTower.x - m_xWorld)) <= nSeparation)
								{
									fPossible = FALSE;
									break;
								}
							}
						}

						if(fPossible)
							Check(mapArea.Add(pt));
					}
				}

				nWorldPtr += m_xWorld;
			}

			if(0 == mapArea.Length())
			{
				// Reduce the separation distance.
				CheckIf(0 == --nSeparation, E_FAIL);
			}
			else
			{
				pt = mapArea[pRand->Next(mapArea.Length())];
				Check(aTowers.Append(pt));

				for(INT nPlane = 0; nPlane < 2; nPlane++)
				{
					MAPTILE* pWorld = prgWorlds[nPlane];
					MAPTILE* pCell = pWorld + pt.y * m_xWorld + pt.x;
					CTileSet* pTileSet = pCell->pTile->GetTileSet();

					if(pTileSet->IsTileSet(L"ocean") || pTileSet->IsTileSet(L"shore"))
						Check(PlaceTile(pWorld, pt.x, pt.y, prgmapTileSets[nPlane], rstrGrass, FALSE));
					RStrReplace(&pCell->rstrFeature, rstrTower);
				}
				break;
			}
		}
	}

Cleanup:
	RStrRelease(rstrGrass);
	RStrRelease(rstrTower);
	return hr;
}

HRESULT CMOMWorldEditor::MakeTundra (TRStrMap<CTileSet*>* pmapTileSets, MAPTILE* pWorld, INT xWorld, INT yWorld, BOOL fActiveWorld)
{
	HRESULT hr;
	CTileSet* pTundra;

	Check(pmapTileSets->Find(RSTRING_CAST(L"tundra"), &pTundra));

	for(INT x = 0; x < xWorld; x++)
	{
		Check(PlaceTile(pWorld, x, 0, pmapTileSets, pTundra->m_rstrName, fActiveWorld));
		Check(PlaceTile(pWorld, x, yWorld - 1, pmapTileSets, pTundra->m_rstrName, fActiveWorld));

		if(rand() % 10 >= 4)
		{
			Check(PlaceTile(pWorld, x, 1, pmapTileSets, pTundra->m_rstrName, fActiveWorld));
			Check(PlaceTile(pWorld, x, yWorld - 2, pmapTileSets, pTundra->m_rstrName, fActiveWorld));
		}
	}

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

HRESULT CMOMWorldEditor::ResetWorldTiles (MAPTILE* pWorld, INT xWorld, INT yWorld, TRStrMap<CTileSet*>& mapTiles, BOOL fAddRandomTundra)
{
	HRESULT hr;
	MAPTILE* pWorldPtr = pWorld;
	CTileSet* pOcean;
	TArray<CTile*>* pTiles;
	sysint cVariants;
	INT nPrev = -1;

	Check(mapTiles.Find(RSTRING_CAST(L"ocean"), &pOcean));
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
			ClearTileData(pWorldPtr);
			pWorldPtr->pTile = (*pTiles)[nNext];
			pWorldPtr++;
			nPrev = nNext;
		}
	}

	if(fAddRandomTundra)
		Check(MakeTundra(&mapTiles, pWorld, xWorld, yWorld, FALSE));

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

HRESULT CMOMWorldEditor::SetupMap (INT xWorld, INT yWorld, BOOL fAddRandomTundra)
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

	Check(ResetWorldTiles(m_pArcanusWorld, m_xWorld, m_yWorld, m_mapArcanus, fAddRandomTundra));
	Check(ResetWorldTiles(m_pMyrrorWorld, m_xWorld, m_yWorld, m_mapMyrror, fAddRandomTundra));

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

		Check(JSONCreateString(pCell->pTile->GetTileSet()->GetName(), &srv));
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

VOID CMOMWorldEditor::ClearTileData (MAPTILE* pTile)
{
	if(pTile->rstrFeature)
	{
		RStrRelease(pTile->rstrFeature);
		pTile->rstrFeature = NULL;
	}

	SafeRelease(pTile->pCity);
	SafeRelease(pTile->pData);
	SafeRelease(pTile->pStack);
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
	if(S_OK == hr)
	{
		m_fPainting = m_pCommand->ContinuePainting();
		if(!m_fPainting)
			SafeDelete(m_pCommand);

		Check(UpdateVisibleTiles());
	}

Cleanup:
	return hr;
}

HRESULT CMOMWorldEditor::PlaceTile (MAPTILE* pWorld, INT xTile, INT yTile, TRStrMap<CTileSet*>* pmapTileSets, RSTRING rstrTile, BOOL fActiveWorld)
{
	HRESULT hr;
	CMapPainter painter(m_pTileRules, pWorld, m_xWorld, m_yWorld);
	TArray<POINT> aTiles;

	Check(painter.PaintTile(xTile, yTile, rstrTile));
	Check(painter.CheckTransitions(xTile, yTile, rstrTile));
	Check(painter.Commit(pmapTileSets, &aTiles));

	for(sysint i = 0; i < aTiles.Length(); i++)
	{
		ClearTileData(pWorld + (aTiles[i].y * m_xWorld + aTiles[i].x));
		ClearTile(aTiles[i].x, aTiles[i].y, fActiveWorld);
	}

Cleanup:
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

HRESULT CMOMWorldEditor::LoadPackage (VOID)
{
	HRESULT hr;
	bool fMissingPackage = false;

	m_pPackage = __new CSIFPackage;
	CheckAlloc(m_pPackage);

	// This file must be built by SIFPackage.exe
	if(GetFileAttributes(L"Assets.pkg") != INVALID_FILE_ATTRIBUTES)
		Check(m_pPackage->OpenPackage(L"Assets.pkg"));
	else if(GetFileAttributes(L"..\\Assets.pkg") != INVALID_FILE_ATTRIBUTES)
		Check(m_pPackage->OpenPackage(L"..\\Assets.pkg"));
	else if(GetFileAttributes(L"Assets\\Assets.pkg") != INVALID_FILE_ATTRIBUTES)
		Check(m_pPackage->OpenPackage(L"Assets\\Assets.pkg"));
	else
	{
		fMissingPackage = true;
		MessageBox(GetDesktopWindow(), L"Could not find Assets.pkg!", L"Missing Data Package", MB_ICONERROR | MB_OK);
		Check(HRESULT_FROM_WIN32(ERROR_MISSING_SYSTEMFILE));
	}

Cleanup:
	if(FAILED(hr) && !fMissingPackage)
	{
		WCHAR wzError[100];
		if(SUCCEEDED(Formatting::TPrintF(wzError, ARRAYSIZE(wzError), NULL, L"Could not load Assets.pkg due to error: 0x%.8X!", hr)))
			MessageBox(GetDesktopWindow(), wzError, L"Data Package Error", MB_ICONERROR | MB_OK);
	}
	return hr;
}

HRESULT CMOMWorldEditor::LoadSmoothing (IJSONObject* pSmoothing)
{
	HRESULT hr;
	CSmoothingSystem* pSystem = NULL;
	RSTRING rstrSystem = NULL;

	for(sysint i = 0; i < pSmoothing->Count(); i++)
	{
		TStackRef<IJSONValue> srv;
		TStackRef<IJSONObject> srSystem;

		Check(pSmoothing->GetValueByIndex(i, &srv));
		Check(srv->GetObject(&srSystem));

		pSystem = __new CSmoothingSystem;
		CheckAlloc(pSystem);
		Check(pSystem->Initialize(srSystem));

		Check(pSmoothing->GetValueName(i, &rstrSystem));
		Check(m_mapSmoothingSystems.Add(rstrSystem, pSystem));
		pSystem = NULL;

		RStrRelease(rstrSystem); rstrSystem = NULL;
	}

Cleanup:
	RStrRelease(rstrSystem);
	__delete pSystem;
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

	Check(m_pMain->AddLayer(FALSE, LayerRender::Masked, RGB(0, 0, 0), &nMouseLayer));
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
