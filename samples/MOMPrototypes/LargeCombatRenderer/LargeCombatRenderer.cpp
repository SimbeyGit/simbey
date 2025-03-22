#include <windows.h>
#include <mmsystem.h>
#include "Library\Core\CoreDefs.h"
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

CLargeCombatRenderer::CLargeCombatRenderer (HINSTANCE hInstance) :
	m_hInstance(hInstance),
	m_Surface(640, 400),
	m_pMain(NULL),
	m_Isometric(TILE_WIDTH, TILE_HEIGHT),
	m_fActive(FALSE),
	m_pSIF(NULL),
	m_pTile(NULL)
{
	m_Surface.EnableClear(RGB(255, 255, 255));

	ZeroMemory(m_fKeys, sizeof(m_fKeys));
}

CLargeCombatRenderer::~CLargeCombatRenderer ()
{
	if(m_pSIF)
	{
		SafeRelease(m_pTile);

		m_pSIF->Close();
		m_pSIF->Release();
	}
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

HRESULT CLargeCombatRenderer::Initialize (INT nWidth, INT nHeight, INT nCmdShow)
{
	HRESULT hr;
	RECT rect = { 0, 0, nWidth, nHeight };

	DWORD dwIndex;
	Check(sifCreateNew(&m_pSIF));
	Check(sifAddImageFileAsLayer(L"Tile_62x32.png", m_pSIF, &dwIndex));
	Check(m_pSIF->GetLayerByIndex(dwIndex, &m_pTile));

	CheckIfGetLastError(!AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE));
	Check(Create(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE, WS_OVERLAPPEDWINDOW,
		c_wzLCRClass, c_wzLCRTitle, CW_USEDEFAULT, CW_USEDEFAULT,
		rect.right - rect.left, rect.bottom - rect.top, NULL, nCmdShow));

	{
		INT xSize, ySize;
		RECT rc;

		m_Surface.GetViewSize(&xSize, &ySize);
		rc.left = 0;
		rc.top = 0;
		rc.right = xSize;
		rc.bottom = ySize;

		INT xScroll, yScroll;
		INT xTileStart, yTileStart, xTileEnd, yTileEnd, yMid, nAdjust = 0;

		m_Isometric.TileToView(MAP_WIDTH / 2, MAP_HEIGHT / 2, &xScroll, &yScroll);
		Check(m_Surface.AddCanvas(&rc, TRUE, &m_pMain));
		m_pMain->SetScroll(xScroll + TILE_WIDTH / 2, yScroll);

		m_Isometric.GetTileRange(m_pMain, &xTileStart, &yTileStart, &xTileEnd, &yTileEnd);
		yMid = (yTileStart + yTileEnd) / 2;

		sysint nLayer;
		m_pMain->AddTileLayer(FALSE, c_slTileOffsets, 0, &nLayer);

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
					INT xIso, yIso, xTile, yTile;
					TStackRef<ISimbeyInterchangeSprite> srSprite;

					m_Isometric.TileToView(x, y, &xIso, &yIso);
					m_Isometric.IsometricToView(m_pMain, xIso, yIso, &xTile, &yTile);

					Check(sifCreateStaticSprite(m_pTile, 0, 0, &srSprite));
					srSprite->SetPosition(xTile, yTile);
					Check(m_pMain->AddSprite(nLayer, srSprite, NULL));
				}
			}

			if(y < yMid)
				nAdjust++;
			else
				nAdjust--;
		}
	}

Cleanup:
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
