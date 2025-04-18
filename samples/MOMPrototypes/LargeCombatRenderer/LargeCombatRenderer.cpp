#include <windows.h>
#include <mmsystem.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Util\Formatting.h"
#include "Library\Util\Options.h"
#include "..\Shared\HeightMapGenerator.h"
#include "..\Shared\TileRules.h"
#include "..\Shared\TileSetLoader.h"
#include "LargeCombatRenderer.h"

#define	GAME_TICK_MS		33

#define	TILE_WIDTH			64
#define	TILE_HEIGHT			32

#define	MAP_WIDTH			32
#define	MAP_HEIGHT			32

const WCHAR c_wzLCRClass[] = L"LargeCombatRendererCls";
const WCHAR c_wzLCRTitle[] = L"Large Combat Renderer";

static SIF_LINE_OFFSET c_slTileOffsets[] =
{
	{ 30, 2 },
	{ 28, 6 },
	{ 26, 10 },
	{ 24, 14 },
	{ 22, 18 },
	{ 20, 22 },
	{ 18, 26 },
	{ 16, 30 },
	{ 14, 34 },
	{ 12, 38 },
	{ 10, 42 },
	{ 8, 46 },
	{ 6, 50 },
	{ 4, 54 },
	{ 2, 58 },
	{ 0, 62 },
	{ 0, 62 },
	{ 2, 58 },
	{ 4, 54 },
	{ 6, 50 },
	{ 8, 46 },
	{ 10, 42 },
	{ 12, 38 },
	{ 14, 34 },
	{ 16, 30 },
	{ 18, 26 },
	{ 20, 22 },
	{ 22, 18 },
	{ 24, 14 },
	{ 26, 10 },
	{ 28, 6 },
	{ 30, 2 }
};

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
// CCombatTiles
///////////////////////////////////////////////////////////////////////////////

CCombatTiles::CCombatTiles () :
	m_cRef(1),
	m_pWorld(NULL)
{
}

CCombatTiles::~CCombatTiles ()
{
	__delete_array m_pWorld;
}

HRESULT CCombatTiles::Initialize (VOID)
{
	HRESULT hr;

	m_pWorld = __new MAPTILE[MAP_WIDTH * MAP_HEIGHT];
	CheckAlloc(m_pWorld);
	ZeroMemory(m_pWorld, sizeof(MAPTILE) * MAP_WIDTH * MAP_HEIGHT);
	hr = S_OK;

Cleanup:
	return hr;
}

IFACEMETHODIMP_(ULONG) CCombatTiles::AddRef ()
{
	return ++m_cRef;
}

IFACEMETHODIMP_(ULONG) CCombatTiles::Release ()
{
	ULONG cRef = --m_cRef;
	if(0 == cRef)
		__delete this;
	return cRef;
}

// ITileMap

VOID CCombatTiles::GetSize (__out INT* pxTiles, __out INT* pyTiles)
{
	*pxTiles = MAP_WIDTH;
	*pyTiles = MAP_HEIGHT;
}

CTile* CCombatTiles::GetTile (INT idxTile)
{
	return m_pWorld[idxTile].pTile;
}

VOID CCombatTiles::SetTile (INT idxTile, CTile* pTile)
{
	m_pWorld[idxTile].pTile = pTile;
}

///////////////////////////////////////////////////////////////////////////////
// CLargeCombatRenderer
///////////////////////////////////////////////////////////////////////////////

CLargeCombatRenderer::CLargeCombatRenderer (HINSTANCE hInstance) :
	m_hInstance(hInstance),
	m_Surface(640, 400),
	m_pMain(NULL),
	m_Isometric(TILE_WIDTH, TILE_HEIGHT),
	m_fActive(FALSE),
	m_pPackage(NULL),
	m_pTileRules(NULL),
	m_pGenerators(NULL)
{
	m_Surface.EnableClear(RGB(255, 255, 255));

	ZeroMemory(m_fKeys, sizeof(m_fKeys));
}

CLargeCombatRenderer::~CLargeCombatRenderer ()
{
	m_mapCombatTiles.DeleteAll();
	SafeRelease(m_pGenerators);
	SafeDelete(m_pTileRules);
	m_mapSmoothingSystems.DeleteAll();

	SafeRelease(m_pPackage);
}

HRESULT CLargeCombatRenderer::Register (HINSTANCE hInstance)
{
	WNDCLASSEX wnd = {0};

	wnd.cbSize = sizeof(WNDCLASSEX);
	wnd.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	wnd.cbClsExtra = 0;
	wnd.cbWndExtra = 0;
	wnd.hInstance = hInstance;
	wnd.hIcon = NULL; //LoadIcon(hInstance, MAKEINTRESOURCEW(IDI_MAIN));
	wnd.hCursor = NULL; //LoadCursor(NULL, IDC_ARROW);
	wnd.hbrBackground = NULL;
	wnd.lpszMenuName = NULL;
	wnd.lpszClassName = c_wzLCRClass;

	return RegisterClass(&wnd,NULL);
}

HRESULT CLargeCombatRenderer::Unregister (HINSTANCE hInstance)
{
	return UnregisterClass(c_wzLCRClass, hInstance);
}

HRESULT CLargeCombatRenderer::Initialize (INT nWidth, INT nHeight, PCWSTR pcwzCmdLine, INT nCmdShow)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONObject> srSmoothing;
	RECT rect = { 0, 0, nWidth, nHeight }, rc;
	INT xSize, ySize;
	INT xScroll, yScroll;
	RSTRING rstrWorld = NULL, rstrGenerator = NULL, rstrSeed = NULL;
	DWORD dwSeed;
	COptions options;

	Check(options.Parse(pcwzCmdLine));

	m_pPackage = __new CSIFPackage;
	CheckAlloc(m_pPackage);
	Check(m_pPackage->OpenPackage(L"Assets.pkg"));

	Check(m_pPackage->GetJSONData(SLP(L"combat_large\\terrain\\smoothing.json"), &srv));
	Check(srv->GetObject(&srSmoothing));
	srv.Release();
	Check(CSmoothingSystem::LoadFromJSON(srSmoothing, m_mapSmoothingSystems));

	Check(m_pPackage->GetJSONData(SLP(L"combat_large\\terrain\\rules.json"), &srv));

	m_pTileRules = __new CTileRules;
	CheckAlloc(m_pTileRules);
	Check(m_pTileRules->Initialize(m_mapSmoothingSystems, srv));
	srv.Release();

	Check(m_pPackage->GetJSONData(SLP(L"combat_large\\terrain\\generators.json"), &srv));
	Check(srv->GetArray(&m_pGenerators));

	CheckIfGetLastError(!AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE));
	Check(Create(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE, WS_OVERLAPPEDWINDOW,
		c_wzLCRClass, c_wzLCRTitle, CW_USEDEFAULT, CW_USEDEFAULT,
		rect.right - rect.left, rect.bottom - rect.top, NULL, nCmdShow));

	m_Surface.GetViewSize(&xSize, &ySize);
	rc.left = 0;
	rc.top = 0;
	rc.right = xSize;
	rc.bottom = ySize;

	m_Isometric.TileToView(MAP_WIDTH / 2, MAP_HEIGHT / 2, &xScroll, &yScroll);
	Check(m_Surface.AddCanvas(&rc, TRUE, &m_pMain));
	m_pMain->SetScroll(xScroll + TILE_WIDTH / 2, yScroll);

	if(!options.GetParamValue(L"world", &rstrWorld))
		Check(RStrCreateW(LSP(L"arcanus"), &rstrWorld));
	if(!options.GetParamValue(L"generator", &rstrGenerator))
		Check(RStrCreateW(LSP(L"grasslands"), &rstrGenerator));

	if(options.GetParamValue(L"seed", &rstrSeed))
		dwSeed = Formatting::TAscToUInt32(RStrToWide(rstrSeed));
	else
		dwSeed = GetTickCount();

	Check(GenerateMap(dwSeed, rstrWorld, rstrGenerator));

Cleanup:
	RStrRelease(rstrGenerator);
	RStrRelease(rstrWorld);
	RStrRelease(rstrSeed);
	return hr;
}

VOID CLargeCombatRenderer::Run (VOID)
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

				m_Surface.Tick();
				m_Surface.Redraw(m_hwnd, NULL);
			}
		}
		else
			WaitMessage();
	}
}

HINSTANCE CLargeCombatRenderer::GetInstance (VOID)
{
	return m_hInstance;
}

VOID CLargeCombatRenderer::OnFinalDestroy (HWND hwnd)
{
	m_Surface.Destroy();

	PostQuitMessage(0);
}

HRESULT CLargeCombatRenderer::FinalizeAndShow (DWORD dwStyle, INT nCmdShow)
{
	return __super::FinalizeAndShow(dwStyle, nCmdShow);
}

BOOL CLargeCombatRenderer::OnCreate (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return FALSE;
}

BOOL CLargeCombatRenderer::OnPaint (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(m_hwnd, &ps);

	const RECT* prcUnpainted;
	INT cUnpainted;
	m_Surface.GetUnpaintedRects(&prcUnpainted, &cUnpainted);

	for(INT i = 0; i < cUnpainted; i++)
		FillRect(hdc, prcUnpainted + i, reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

	m_Surface.Redraw(m_hwnd, hdc);
	EndPaint(m_hwnd, &ps);
	lResult = TRUE;
	return TRUE;
}

BOOL CLargeCombatRenderer::OnEraseBackground (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	lResult = TRUE;
	return TRUE;
}

BOOL CLargeCombatRenderer::OnSize (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	m_Surface.Position(LOWORD(lParam), HIWORD(lParam));
	return FALSE;
}

BOOL CLargeCombatRenderer::OnClose (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return FALSE;
}

BOOL CLargeCombatRenderer::OnActivate (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	m_fActive = (WA_INACTIVE != LOWORD(wParam));
	return FALSE;
}

BOOL CLargeCombatRenderer::OnMouseMove (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	INT xView, yView;
	CSIFCanvas* pCanvas;

	m_Surface.TranslateClientPointToView(LOWORD(lParam), HIWORD(lParam), &pCanvas, &xView, &yView);
	if(pCanvas == m_pMain)
	{
	}

	return TRUE;
}

BOOL CLargeCombatRenderer::OnLButtonDown (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	INT xView, yView;
	CSIFCanvas* pCanvas;

	m_Surface.TranslateClientPointToView(LOWORD(lParam), HIWORD(lParam), &pCanvas, &xView, &yView);
	if(pCanvas == m_pMain)
	{
	}

	return TRUE;
}

BOOL CLargeCombatRenderer::OnLButtonUp (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return TRUE;
}

BOOL CLargeCombatRenderer::OnRButtonDown (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return TRUE;
}

BOOL CLargeCombatRenderer::OnKeyDown (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	m_fKeys[static_cast<BYTE>(wParam)] = true;
	return FALSE;
}

BOOL CLargeCombatRenderer::OnKeyUp (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	m_fKeys[static_cast<BYTE>(wParam)] = false;
	return FALSE;
}

BOOL CLargeCombatRenderer::OnSetCursor (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	if(HTCLIENT == LOWORD(lParam))
	{
		const RECT* prcUnpainted;
		INT cUnpainted;

		m_Surface.GetUnpaintedRects(&prcUnpainted, &cUnpainted);
		if(0 < cUnpainted)
		{
			POINT pt;

			if(GetCursorPos(&pt))
			{
				ScreenToClient(m_hwnd, &pt);
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

HRESULT CLargeCombatRenderer::PlaceTile (CSIFCanvas* pCanvas, INT xTile, INT yTile, sysint nLayer, CTile* pTile, __deref_out_opt ISimbeyInterchangeSprite** ppSprite)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeSprite> srSprite;
	INT xIso, yIso, x, y;

	m_Isometric.TileToView(xTile, yTile, &xIso, &yIso);
	m_Isometric.IsometricToView(m_pMain, xIso, yIso, &x, &y);

	Check(pTile->CreateSprite(&srSprite));
	srSprite->SetPosition(x, y);
	Check(pCanvas->AddSprite(nLayer, srSprite, NULL));

	if(ppSprite)
		*ppSprite = srSprite.Detach();

Cleanup:
	return hr;
}

HRESULT CLargeCombatRenderer::AllocateCombatWorld (__deref_out CCombatTiles** ppTiles)
{
	HRESULT hr;
	CTileSet* pStandardSet;
	TArray<CTile*>* paTiles;
	TStackRef<CCombatTiles> srTiles;

	Check(m_mapCombatTiles.Find(RSTRING_CAST(L"standard"), &pStandardSet));
	Check(pStandardSet->FindFromKey(RSTRING_CAST(L"00000000"), &paTiles));

	srTiles.Attach(__new CCombatTiles);
	CheckAlloc(srTiles);
	Check(srTiles->Initialize());

	for(INT i = 0; i < MAP_WIDTH * MAP_HEIGHT; i++)
		srTiles->m_pWorld[i].pTile = (*paTiles)[rand() % paTiles->Length()];

	*ppTiles = srTiles.Detach();

Cleanup:
	return hr;
}

HRESULT CLargeCombatRenderer::GenerateCombatWorld (CCombatTiles* pTiles, IJSONObject* pGenerator, DWORD dwSeed)
{
	HRESULT hr;
	CMapPainter painter(m_pTileRules, pTiles);
	COORD_SYSTEM coords;
	CSimpleRNG rng(dwSeed);
	CHeightMapGenerator HeightMap(&rng, rng.Next(5) + 2, rng.Next(5) + 2, 0);
	TStackRef<IJSONValue> srv;
	INT cTiles;
	TArray<POINT> aTiles;
	RSTRING rstrDarkened = NULL, rstrRidge = NULL;
	CTileSet* pFeatures;
	TArray<CTile*>* paFeatures;

	coords.nWidth = MAP_WIDTH;
	coords.nHeight = MAP_HEIGHT;
	coords.fWrapsLeftToRight = FALSE;
	coords.fWrapsTopToBottom = FALSE;

	Check(HeightMap.Initialize(coords));
	Check(HeightMap.GenerateHeightMap());

	Check(RStrCreateW(LSP(L"darkened"), &rstrDarkened));
	Check(RStrCreateW(LSP(L"ridge"), &rstrRidge));

	Check(pGenerator->FindNonNullValueW(L"dark", &srv));
	Check(srv->GetInteger(&cTiles));
	srv.Release();

	if(0 < cTiles)
	{
		Check(HeightMap.SetLowestTiles(cTiles, &aTiles));
		for(sysint i = 0; i < aTiles.Length(); i++)
		{
			POINT& pt = aTiles[i];
			Check(painter.PaintTile(pt.x, pt.y, rstrDarkened));
			Check(painter.CheckTransitions(pt.x, pt.y, rstrDarkened));
			Check(painter.Commit(&m_mapCombatTiles, NULL));
		}
		aTiles.Clear();
	}

	Check(pGenerator->FindNonNullValueW(L"ridge", &srv));
	Check(srv->GetInteger(&cTiles));
	srv.Release();

	if(0 < cTiles)
	{
		Check(HeightMap.SetHighestTiles(cTiles, &aTiles));
		for(sysint i = 0; i < aTiles.Length(); i++)
		{
			POINT& pt = aTiles[i];
			Check(painter.PaintTile(pt.x, pt.y, rstrRidge));
			Check(painter.CheckTransitions(pt.x, pt.y, rstrRidge));
			Check(painter.Commit(&m_mapCombatTiles, NULL));
		}
		aTiles.Clear();
	}

	Check(pGenerator->FindNonNullValueW(L"features", &srv));
	Check(srv->GetInteger(&cTiles));

	// Half the tiles won't be included anyway, so let's multiply by 2.5 to increase the visible distribution.
	cTiles = MulDiv(cTiles, 5, 2);

	for(INT y = 0; y < MAP_HEIGHT; y++)
	{
		for(INT x = 0; x < MAP_WIDTH; x++)
		{
			if(((x >= 7 && x <= 13) || (x >= 19 && x <= 25)) && (y >= 15 && y <= 21))
			{
				// No terrain features are placed on these tiles.
			}
			else
			{
				POINT pt = { x, y };
				Check(aTiles.Append(pt));
			}
		}
	}

	Check(m_mapCombatTiles.Find(RSTRING_CAST(L"feature"), &pFeatures));
	Check(pFeatures->FindFromKey(RSTRING_CAST(L"00000000"), &paFeatures));

	for(INT i = 0; i < cTiles; i++)
	{
		INT idxTile = rng.Next(aTiles.Length());
		POINT pt;

		Check(aTiles.RemoveChecked(idxTile, &pt));
		pTiles->m_pWorld[pt.y * MAP_WIDTH + pt.x].pFeature = (*paFeatures)[rng.Next(paFeatures->Length())];
	}

Cleanup:
	RStrRelease(rstrDarkened);
	RStrRelease(rstrRidge);
	return hr;
}

HRESULT CLargeCombatRenderer::GenerateMap (DWORD dwSeed, RSTRING rstrWorld, RSTRING rstrGenerator)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONObject> srGenerator;
	TStackRef<CSIFPackage> srTileSets;
	RSTRING rstrTiles = NULL;
	WCHAR wzTileSets[MAX_PATH];
	INT xTileStart, yTileStart, xTileEnd, yTileEnd, cchTileSets, yMid, nAdjust = 0;
	sysint nLayer, nUnitLayer;
	TStackRef<CCombatTiles> srTiles;

	Check(JSONFindArrayObject(m_pGenerators, RSTRING_CAST(L"name"), rstrGenerator, &srGenerator, NULL));
	Check(srGenerator->FindNonNullValueW(L"tiles", &srv));
	Check(srv->GetString(&rstrTiles));

	// Load the tile-specific graphics for the combat terrain tiles.
	Check(Formatting::TPrintF(wzTileSets, ARRAYSIZE(wzTileSets), &cchTileSets, L"combat_large\\terrain\\%r\\%r", rstrWorld, rstrTiles));
	Check(m_pPackage->OpenDirectory(wzTileSets, cchTileSets, &srTileSets));
	Check(TileSetLoader::LoadTileSets(srTileSets, m_mapCombatTiles));
	srTileSets.Release();

	// Load the default graphics for any missing combat terrain tiles.
	Check(Formatting::TPrintF(wzTileSets, ARRAYSIZE(wzTileSets), &cchTileSets, L"combat_large\\terrain\\%r\\default", rstrWorld));
	Check(m_pPackage->OpenDirectory(wzTileSets, cchTileSets, &srTileSets));
	Check(TileSetLoader::LoadTileSets(srTileSets, m_mapCombatTiles));

	Check(AllocateCombatWorld(&srTiles));
	Check(GenerateCombatWorld(srTiles, srGenerator, dwSeed));

	Check(m_pMain->AddTileLayer(FALSE, c_slTileOffsets, 0, &nLayer));
	//Check(m_pMain->AddLayer(TRUE, LayerRender::Masked, 0, &m_nTileEffectsLayer));
	Check(m_pMain->AddLayer(TRUE, LayerRender::Blended, 0, &nUnitLayer));

	m_Isometric.GetTileRange(m_pMain, &xTileStart, &yTileStart, &xTileEnd, &yTileEnd);
	yMid = (yTileStart + yTileEnd) / 2;

	for(INT y = yTileStart; y < yTileEnd; y++)
	{
		if(0 <= y && y < MAP_HEIGHT)
		{
			INT xStart = xTileStart - nAdjust;
			INT xEnd = xTileEnd + nAdjust;
			if(0 > xStart)
				xStart = 0;
			if(MAP_WIDTH < xEnd)
				xEnd = MAP_WIDTH;
			for(INT x = xStart; x < xEnd; x++)
			{
				MAPTILE* pMapPtr = srTiles->m_pWorld + (y * MAP_WIDTH + x);
				PlaceTile(m_pMain, x, y, nLayer, pMapPtr->pTile, NULL);
				if(pMapPtr->pFeature)
				{
					TStackRef<ISimbeyInterchangeSprite> srSprite;
					INT xIso, yIso, xPlace, yPlace;

					Check(pMapPtr->pFeature->CreateSprite(&srSprite));

					m_Isometric.TileToView(x, y, &xIso, &yIso);
					m_Isometric.IsometricToView(m_pMain, xIso, yIso, &xPlace, &yPlace);

					// The terrain features are supposed to be positioned using the
					// bottom center, not counting rows only containing shadow pixels.
					srSprite->SetPosition(xPlace + (TILE_WIDTH / 2), yPlace + TILE_HEIGHT / 2);
					Check(m_pMain->AddSprite(nUnitLayer, srSprite, NULL));
				}
			}
		}

		if(y < yMid)
			nAdjust++;
		else
			nAdjust--;
	}

Cleanup:
	RStrRelease(rstrTiles);
	return hr;
}
