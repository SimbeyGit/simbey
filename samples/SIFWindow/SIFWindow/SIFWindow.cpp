#include <windows.h>
#include <mmsystem.h>
#include "Library\Core\CoreDefs.h"
#include "SIFWindow.h"

#define	GAME_TICK_MS		33

const WCHAR c_wzSIFClass[] = L"SIFWindowCls";
const WCHAR c_wzSIFTitle[] = L"SIF Window";

CSIFWindow::CSIFWindow (HINSTANCE hInstance) :
	m_hInstance(hInstance),
	m_Surface(512, 320),
	m_pMain(NULL),
	m_fActive(FALSE)
{
	m_Surface.EnableClear(RGB(255, 255, 255));
}

CSIFWindow::~CSIFWindow ()
{
}

HRESULT CSIFWindow::Register (HINSTANCE hInstance)
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
	wnd.lpszClassName = c_wzSIFClass;

	return RegisterClass(&wnd,NULL);
}

HRESULT CSIFWindow::Unregister (HINSTANCE hInstance)
{
	return UnregisterClass(c_wzSIFClass, hInstance);
}

HRESULT CSIFWindow::Initialize (INT nWidth, INT nHeight, INT nCmdShow)
{
	HRESULT hr;
	RECT rect = { 0, 0, nWidth, nHeight };

	CheckIfGetLastError(!AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE));
	Check(Create(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE, WS_OVERLAPPEDWINDOW,
		c_wzSIFClass, c_wzSIFTitle, CW_USEDEFAULT, CW_USEDEFAULT,
		rect.right - rect.left, rect.bottom - rect.top, NULL, nCmdShow));

	{
		INT xSize, ySize;
		RECT rc;

		m_Surface.GetViewSize(&xSize, &ySize);
		rc.left = 0;
		rc.top = 0;
		rc.right = xSize;
		rc.bottom = ySize;

		Check(m_Surface.AddCanvas(&rc, TRUE, &m_pMain));
	}

Cleanup:
	return hr;
}

VOID CSIFWindow::Run (VOID)
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
				UpdateScene();
				m_Surface.Redraw(m_hwnd, NULL);
			}
		}
		else
			WaitMessage();
	}
}

HINSTANCE CSIFWindow::GetInstance (VOID)
{
	return m_hInstance;
}

VOID CSIFWindow::OnFinalDestroy (HWND hwnd)
{
	m_Surface.Destroy();

	PostQuitMessage(0);
}

HRESULT CSIFWindow::FinalizeAndShow (DWORD dwStyle, INT nCmdShow)
{
	return __super::FinalizeAndShow(dwStyle, nCmdShow);
}

BOOL CSIFWindow::OnCreate (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return FALSE;
}

BOOL CSIFWindow::OnPaint (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
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

BOOL CSIFWindow::OnEraseBackground (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	lResult = TRUE;
	return TRUE;
}

BOOL CSIFWindow::OnSize (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	m_Surface.Position(LOWORD(lParam), HIWORD(lParam));
	return FALSE;
}

BOOL CSIFWindow::OnClose (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return FALSE;
}

BOOL CSIFWindow::OnActivate (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	m_fActive = (WA_INACTIVE != LOWORD(wParam));
	return FALSE;
}

BOOL CSIFWindow::OnMouseMove (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	INT xView, yView;
	CSIFCanvas* pCanvas;

	m_Surface.TranslateClientPointToView(LOWORD(lParam), HIWORD(lParam), &pCanvas, &xView, &yView);
	if(pCanvas == m_pMain)
	{
	}

	return TRUE;
}

BOOL CSIFWindow::OnLButtonDown (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	INT xView, yView;
	CSIFCanvas* pCanvas;

	m_Surface.TranslateClientPointToView(LOWORD(lParam), HIWORD(lParam), &pCanvas, &xView, &yView);
	if(pCanvas == m_pMain)
	{
	}

	return TRUE;
}

BOOL CSIFWindow::OnLButtonUp (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return TRUE;
}

BOOL CSIFWindow::OnRButtonDown (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return TRUE;
}

BOOL CSIFWindow::OnKeyDown (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return FALSE;
}

BOOL CSIFWindow::OnKeyUp (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return FALSE;
}

BOOL CSIFWindow::OnSetCursor (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
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

VOID CSIFWindow::UpdateScene (VOID)
{
}
