#pragma once

#include <SIFRibbon.h>
#include <SIFRibbonItem.h>
#include <StringGalleryItem.h>
#include "Library\Core\BaseUnknown.h"
#include "Library\Core\RStrMap.h"
#include "Library\Window\BaseWindow.h"
#include "Library\MIDIPlayer.h"
#include "Package\SIFPackage.h"
#include "..\Shared\InteractiveSurface.h"
#include "..\Shared\Dir.h"
#include "..\Shared\TileSet.h"

interface IJSONValue;
interface IJSONObject;
interface IJSONArray;

interface IRandomNumber;

class CHeightMapGenerator;
class CSmoothingSystem;
class CTileRules;

struct RIVER_DIR
{
	POINT ptStart;
	Dir::Value eDir;
};

class CMapArea
{
private:
	TArray<POINT> m_aPoints;

public:
	inline const POINT& operator[] (sysint n) const
	{
		return m_aPoints[n];
	}

	inline sysint Length (VOID)
	{
		return m_aPoints.Length();
	}

	inline HRESULT Add (const POINT& pt)
	{
		return m_aPoints.Append(pt);
	}

	inline VOID Clear (VOID)
	{
		m_aPoints.Clear();
	}

	BOOL Has (const POINT& pt)
	{
		for(sysint i = 0; i < m_aPoints.Length(); i++)
		{
			POINT& ptItem = m_aPoints[i];
			if(ptItem.x == pt.x && ptItem.y == pt.y)
				return TRUE;
		}
		return FALSE;
	}

	BOOL HasWithin (const POINT& pt, INT nRadius)
	{
		for(sysint i = 0; i < m_aPoints.Length(); i++)
		{
			POINT& ptItem = m_aPoints[i];
			if(abs(ptItem.x - pt.x) <= nRadius && abs(ptItem.y - pt.y) <= nRadius)
				return TRUE;
		}
		return FALSE;
	}
};

class CGeneratorGallery :
	public CBaseUnknown,
	public IUICollection
{
private:
	CSIFRibbon* m_pRibbon;
	UINT m_idxSelection;
	IJSONArray* m_pGenerators;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IUICollection)
	END_UNK_MAP

public:
	CGeneratorGallery (CSIFRibbon* pRibbon, IJSONArray* pGenerators);
	~CGeneratorGallery ();

	UINT GetSelection (VOID);
	VOID SetSelection (UINT idxSelection);

	// IUICollection
	virtual HRESULT STDMETHODCALLTYPE GetCount (__out UINT32* pcItems);
	virtual HRESULT STDMETHODCALLTYPE GetItem (UINT32 index, __deref_out_opt IUnknown** item);
	virtual HRESULT STDMETHODCALLTYPE Add (IUnknown* item);
	virtual HRESULT STDMETHODCALLTYPE Insert (UINT32 index, IUnknown* item);
	virtual HRESULT STDMETHODCALLTYPE RemoveAt (UINT32 index);
	virtual HRESULT STDMETHODCALLTYPE Replace (UINT32 indexReplaced, IUnknown* itemReplaceWith);
	virtual HRESULT STDMETHODCALLTYPE Clear (VOID);
};

class CTerrainGallery :
	public CBaseUnknown,
	public IUICollection
{
private:
	CSIFRibbon* m_pRibbon;
	ISimbeyInterchangeFile* m_pSIF;
	UINT m_idxSelection;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IUICollection)
	END_UNK_MAP

public:
	CTerrainGallery (CSIFRibbon* pRibbon, ISimbeyInterchangeFile* pSIF);
	~CTerrainGallery ();

	HRESULT GetSelectedTile (__out RSTRING* prstrTile);
	UINT GetSelection (VOID);
	VOID SetSelection (UINT idxSelection);

	// IUICollection
	virtual HRESULT STDMETHODCALLTYPE GetCount (__out UINT32* pcItems);
	virtual HRESULT STDMETHODCALLTYPE GetItem (UINT32 index, __deref_out_opt IUnknown** item);
	virtual HRESULT STDMETHODCALLTYPE Add (IUnknown* item);
	virtual HRESULT STDMETHODCALLTYPE Insert (UINT32 index, IUnknown* item);
	virtual HRESULT STDMETHODCALLTYPE RemoveAt (UINT32 index);
	virtual HRESULT STDMETHODCALLTYPE Replace (UINT32 indexReplaced, IUnknown* itemReplaceWith);
	virtual HRESULT STDMETHODCALLTYPE Clear (VOID);
};

namespace World
{
	enum Type
	{
		Arcanus,
		Myrror
	};
}

struct CITYTILE
{
	ISimbeyInterchangeSprite* pNormal[17];
	ISimbeyInterchangeSprite* pWalled[17];
};

class CMapData
{
public:
	RSTRING m_rstrFeature;
	IJSONObject* m_pData;
	IJSONObject* m_pCity;
	IJSONObject* m_pStack;

public:
	CMapData () :
		m_rstrFeature(NULL),
		m_pData(NULL),
		m_pCity(NULL),
		m_pStack(NULL)
	{
	}

	~CMapData ()
	{
		Clear();
	}

	VOID Clear (VOID)
	{
		if(m_rstrFeature)
		{
			RStrRelease(m_rstrFeature);
			m_rstrFeature = NULL;
		}

		SafeRelease(m_pData);
		SafeRelease(m_pCity);
		SafeRelease(m_pStack);
	}
};

class CBaseGalleryCommand
{
public:
	CTerrainGallery* m_pGallery;
	MAPTILE* m_pWorld;
	INT m_xWorld, m_yWorld;

public:
	CBaseGalleryCommand (CTerrainGallery* pGallery, MAPTILE* pWorld, INT xWorld, INT yWorld) :
		m_pGallery(pGallery),
		m_pWorld(pWorld),
		m_xWorld(xWorld), m_yWorld(yWorld)
	{
		m_pGallery->AddRef();
	}

	virtual ~CBaseGalleryCommand ()
	{
		m_pGallery->Release();
	}

	virtual HRESULT Execute (class CMOMWorldEditor* pEditor, INT xTile, INT yTile) = 0;
	virtual BOOL ContinuePainting (VOID) { return TRUE; }
};

class CTerrainCommand : public CBaseGalleryCommand
{
public:
	TRStrMap<CTileSet*>* m_pmapTileSets;

public:
	CTerrainCommand (CTerrainGallery* pGallery, MAPTILE* pWorld, INT xWorld, INT yWorld, TRStrMap<CTileSet*>* pmapTileSets) :
		CBaseGalleryCommand(pGallery, pWorld, xWorld, yWorld),
		m_pmapTileSets(pmapTileSets)
	{
	}

	virtual HRESULT Execute (class CMOMWorldEditor* pEditor, INT xTile, INT yTile);
};

class CFeaturesCommand : public CBaseGalleryCommand
{
public:
	CFeaturesCommand (CTerrainGallery* pGallery, MAPTILE* pWorld, INT xWorld, INT yWorld) :
		CBaseGalleryCommand(pGallery, pWorld, xWorld, yWorld)
	{
	}

	virtual HRESULT Execute (class CMOMWorldEditor* pEditor, INT xTile, INT yTile);
};

class CClearFeatureCommand : public CBaseGalleryCommand
{
public:
	CClearFeatureCommand (CTerrainGallery* pGallery, MAPTILE* pWorld, INT xWorld, INT yWorld) :
		CBaseGalleryCommand(pGallery, pWorld, xWorld, yWorld)
	{
	}

	virtual HRESULT Execute (class CMOMWorldEditor* pEditor, INT xTile, INT yTile);
};

class CPlaceCityCommand : public CBaseGalleryCommand
{
public:
	CPlaceCityCommand (CTerrainGallery* pGallery, MAPTILE* pWorld, INT xWorld, INT yWorld) :
		CBaseGalleryCommand(pGallery, pWorld, xWorld, yWorld)
	{
	}

	virtual HRESULT Execute (class CMOMWorldEditor* pEditor, INT xTile, INT yTile);
	virtual BOOL ContinuePainting (VOID) { return FALSE; }
};

class COverlandTerrain
{
private:
	CSIFPackage* m_pPackage;

public:
	COverlandTerrain ();
	~COverlandTerrain ();

	VOID SetRootPackage (CSIFPackage* pPackage);
	HRESULT GetJSONData (PCWSTR pcwzSubPath, INT cchSubPath, __deref_out IJSONValue** ppv);
	HRESULT GetJSONArray (PCWSTR pcwzSubPath, INT cchSubPath, __deref_out IJSONArray** ppArray);
	HRESULT GetJSONObject (PCWSTR pcwzSubPath, INT cchSubPath, __deref_out IJSONObject** ppObject);
	HRESULT OpenSIF (PCWSTR pcwzSubPath, __deref_out ISimbeyInterchangeFile** ppSIF);
	HRESULT OpenDirectory (PCWSTR pcwzSubPath, INT cchSubPath, __deref_out CSIFPackage** ppSubPackage);
};

class CMOMWorldEditor :
	public CBaseUnknown,
	public CBaseWindow,
	public IRibbonHost,
	public IUICommandHandler,
	public MIDI::INotifyFinished,
	public ILayerInputHandler
{
protected:
	HINSTANCE m_hInstance;

	CSIFRibbon* m_pRibbon;
	CSIFPackage* m_pPackage;
	CInteractiveSurface* m_pSurface;

	MIDI::CPlayer m_player;
	BOOL m_fActive;

	bool m_fKeys[256];

	TRStrMap<IJSONObject*> m_mapFeatureChances;
	TRStrMap<CSmoothingSystem*> m_mapSmoothingSystems;
	CTileRules* m_pTileRules;

	IJSONArray* m_pGenerators;
	IJSONArray* m_pProportions;
	CGeneratorGallery* m_pGeneratorGallery;

	TRStrMap<CTileSet*> m_mapArcanus, m_mapMyrror;
	CTerrainGallery* m_pArcanusTerrain;
	CTerrainGallery* m_pMyrrorTerrain;

	TRStrMap<ISimbeyInterchangeSprite*> m_mapFeatures;
	CTerrainGallery* m_pFeatures;
	TArray<CITYTILE> m_aCityTiles;

	SIZE m_sizeTiles;
	INT m_nCityOffset;

	INT m_xWorld, m_yWorld;
	MAPTILE* m_pArcanusWorld;
	MAPTILE* m_pMyrrorWorld;
	World::Type m_eType;

	CSIFCanvas* m_pMain;
	CInteractiveLayer* m_pMapTileLayer;
	sysint m_nFeaturesLayer;
	sysint m_nCitiesLayer;
	ISimbeyInterchangeSprite* m_pMouse;

	INT m_xDrag, m_yDrag;
	BOOL m_fDragging;
	BOOL m_fPainting;

	CBaseGalleryCommand* m_pCommand;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IOleWindow)
		UNK_INTERFACE(IBaseWindow)
		UNK_INTERFACE(IRibbonHost)
		UNK_INTERFACE(IUIApplication)
		UNK_INTERFACE(IUICommandHandler)
		UNK_INTERFACE(ILayerInputHandler)
	END_UNK_MAP

	BEGIN_WM_MAP
		HANDLE_WM(WM_CREATE, OnCreate)
		HANDLE_WM(WM_PAINT, OnPaint)
		HANDLE_WM(WM_ERASEBKGND, OnEraseBackground)
		HANDLE_WM(WM_SIZE, OnSize)
		HANDLE_WM(WM_CLOSE, OnClose)
		HANDLE_WM(WM_ACTIVATE, OnActivate)
		HANDLE_WM(WM_MOUSEMOVE, m_pSurface->ProcessLayerInput)
		HANDLE_WM(WM_LBUTTONDOWN, m_pSurface->ProcessLayerInput)
		HANDLE_WM(WM_LBUTTONUP, m_pSurface->ProcessLayerInput)
		HANDLE_WM(WM_RBUTTONDOWN, m_pSurface->ProcessLayerInput)
		HANDLE_WM(WM_RBUTTONUP, m_pSurface->ProcessLayerInput)
		HANDLE_WM(WM_KEYDOWN, OnKeyDown)
		HANDLE_WM(WM_KEYUP, OnKeyUp)
		HANDLE_WM(WM_SETCURSOR, OnSetCursor)
		HANDLE_WM(WM_DESTROY, OnDestroy)
	END_WM_MAP

	CMOMWorldEditor (HINSTANCE hInstance);
	virtual ~CMOMWorldEditor ();

	static HRESULT Register (HINSTANCE hInstance);
	static HRESULT Unregister (HINSTANCE hInstance);

	HRESULT Initialize (INT nWidth, INT nHeight, INT nCmdShow);
	VOID Run (VOID);

	// IRibbonHost
	virtual HRESULT WINAPI GetRibbonSettingsKey (__out_ecount(cchMaxKey) PSTR pszKeyName, INT cchMaxKey);
	virtual HRESULT WINAPI GetRibbonSettingsValue (__out_ecount(cchMaxValue) PSTR pszValueName, INT cchMaxValue);
	virtual HRESULT WINAPI GetRibbonResource (__out HMODULE* phModule, __out_ecount(cchMaxResource) PWSTR pwzResource, INT cchMaxResource);
	virtual UINT32 WINAPI TranslateGroupToImage (UINT32 nID);
	virtual UINT32 WINAPI TranslateImageToLargeImage (UINT32 nID);

	// IUIApplication
	virtual HRESULT STDMETHODCALLTYPE OnViewChanged (UINT32 viewId, UI_VIEWTYPE typeID, IUnknown* view, UI_VIEWVERB verb, INT32 uReasonCode);
	virtual HRESULT STDMETHODCALLTYPE OnCreateUICommand (UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler** commandHandler);
	virtual HRESULT STDMETHODCALLTYPE OnDestroyUICommand (UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler* commandHandler);

	// IUICommandHandler
	virtual HRESULT STDMETHODCALLTYPE Execute (UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* currentValue, IUISimplePropertySet* commandExecutionProperties);
	virtual HRESULT STDMETHODCALLTYPE UpdateProperty (UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT* currentValue, PROPVARIANT* newValue);

protected:
	virtual HINSTANCE GetInstance (VOID);
	virtual VOID OnFinalDestroy (HWND hwnd);

	virtual HRESULT FinalizeAndShow (DWORD dwStyle, INT nCmdShow);

	DECL_WM_HANDLER(OnCreate);
	DECL_WM_HANDLER(OnPaint);
	DECL_WM_HANDLER(OnEraseBackground);
	DECL_WM_HANDLER(OnSize);
	DECL_WM_HANDLER(OnClose);
	DECL_WM_HANDLER(OnActivate);
	DECL_WM_HANDLER(OnKeyDown);
	DECL_WM_HANDLER(OnKeyUp);
	DECL_WM_HANDLER(OnSetCursor);
	DECL_WM_HANDLER(OnDestroy);

	// INotifyFinished
	virtual VOID OnNotifyFinished (MIDI::CPlayer* pPlayer, BOOL fCompleted);

	// ILayerInputHandler
	virtual BOOL ProcessMouseInput (LayerInput::Mouse eType, WPARAM wParam, LPARAM lParam, INT xView, INT yView, LRESULT& lResult);
	virtual BOOL ProcessKeyboardInput (LayerInput::Keyboard eType, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

	VOID OnUpdateFrame (VOID);

	HRESULT OpenSaveMap (UINT32 commandId);
	HRESULT LoadFrom (PCWSTR pcwzFile);
	HRESULT SaveTo (PCWSTR pcwzFile);
	HRESULT PromptForNewMap (VOID);
	HRESULT GenerateRandomWorlds (VOID);

	HRESULT SetHighestTiles (TRStrMap<CTileSet*>* pmapTileSets, MAPTILE* pWorld, CHeightMapGenerator& heightMap, PCWSTR pcwzTile, INT cchTile, INT nDesiredTileCount, BOOL fActiveWorld);
	HRESULT PlaceBlob (IRandomNumber* pRand, TRStrMap<CTileSet*>* pmapTileSets, MAPTILE* pWorld, IJSONObject* pData, INT nDesiredTileCount, INT nEachAreaTileCount);
	HRESULT PlaceSingleBlob (IRandomNumber* pRand, TRStrMap<CTileSet*>* pmapTileSets, CTileSet* pReplace, CTileSet* pPlace, MAPTILE* pWorld, RSTRING rstrTile, INT nEachAreaTileCount, __out INT* pcTilesPlaced);
	HRESULT PlaceTowersOfWizardry (IRandomNumber* pRand, TRStrMap<CTileSet*>** prgmapTileSets, MAPTILE** prgWorlds, IJSONObject* pTowers);
	HRESULT MakeRivers (IRandomNumber* pRand, TRStrMap<CTileSet*>* pmapTileSets, MAPTILE* pWorld, INT cRivers);
	HRESULT AddRiverStarts (MAPTILE* pWorld, const POINT& ptShore, CTile* pShoreTile, CTileSet* pGrass, TArray<RIVER_DIR>& aRivers);
	BOOL RiverStartNotAdjacentToOthers (TArray<RIVER_DIR>& aRivers, const POINT& pt);
	HRESULT ProcessRiver (IRandomNumber* pRand, __inout RIVER_DIR& river, TRStrMap<CTileSet*>* pmapTileSets, MAPTILE* pWorld, CTileSet* pRiver, CTileSet* pGrass);
	HRESULT MakeTundra (TRStrMap<CTileSet*>* pmapTileSets, MAPTILE* pWorld, INT xWorld, INT yWorld, BOOL fActiveWorld);
	INT CountAdjacentTiles (MAPTILE* pWorld, const POINT& ptAround, CTileSet* pTileSet);
	HRESULT PlaceMapFeatures (IRandomNumber* pRand, MAPTILE* pWorld, INT nPlane, INT nChance);
	HRESULT PlaceNodes (IRandomNumber* pRand, TRStrMap<CTileSet*>* pmapTileSets, MAPTILE* pWorld, INT cNodes);
	HRESULT PlaceLairs (IRandomNumber* pRand, TRStrMap<CTileSet*>* pmapTileSets, MAPTILE* pWorld, IJSONObject* pLairs, INT nPlane);

	VOID DeleteWorld (MAPTILE*& pWorld);
	HRESULT ResetWorldTiles (MAPTILE* pWorld, INT xWorld, INT yWorld, TRStrMap<CTileSet*>& mapTiles, BOOL fAddRandomTundra);

	HRESULT UpdateVisibleTiles (VOID);
	VOID Scroll (INT x, INT y);

	HRESULT SetupMap (INT xWorld, INT yWorld, BOOL fAddRandomTundra);
	HRESULT AllocateWorld (INT nWorldCells, __deref_out MAPTILE** ppWorld);
	HRESULT LoadWorldFromJSON (TRStrMap<CTileSet*>& mapTileSet, IJSONObject* pMap, PCWSTR pcwzWorld, INT xWorld, INT yWorld, MAPTILE** ppWorld);
	HRESULT SaveWorldToJSON (IJSONObject* pMap, PCWSTR pcwzWorld, INT xWorld, INT yWorld, MAPTILE* pWorld);

	HRESULT ReplaceCommand (CBaseGalleryCommand* pCommand);

public:
	HRESULT ClearTile (INT x, INT y, BOOL fActiveWorld);
	HRESULT PlaceSelectedTile (INT x, INT y);
	HRESULT PlaceTile (MAPTILE* pWorld, INT xTile, INT yTile, TRStrMap<CTileSet*>* pmapTileSets, RSTRING rstrTile, BOOL fActiveWorld);

	HRESULT PlaceOrModifyCity (MAPTILE* pWorld, INT xTile, INT yTile);

protected:
	HRESULT LoadPackage (VOID);
	HRESULT LoadMapFeatureChances (IJSONObject* pFeatures);

	HRESULT LoadFeatures (ISimbeyInterchangeFile* pFeatures);
	HRESULT LoadCityTiles (ISimbeyInterchangeFile* pCityTiles);

	HRESULT LoadTerrain (COverlandTerrain& overland);
	HRESULT CreateGallery (TRStrMap<CTileSet*>& mapTiles, CTerrainGallery** ppGallery);
	HRESULT SetupMouse (VOID);

	static HRESULT ColorizeLayer (ISimbeyInterchangeFileLayer* pLayer, COLORREF crColorize, ISimbeyInterchangeFile* pStorage, __deref_out ISimbeyInterchangeFileLayer** ppColorized);
};
