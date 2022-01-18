#include <windows.h>
#include <gdiplus.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Window\DialogHost.h"
#include "AboutDialog.h"
#include "SimpleWindow.h"

const WCHAR c_wzSimpleClass[] = L"SimpleWindowCls";
const WCHAR c_wzSimpleTitle[] = L"Simple Window";

CSimpleWindow::CSimpleWindow (HINSTANCE hInstance) :
	m_hInstance(hInstance)
{
}

CSimpleWindow::~CSimpleWindow ()
{
}

HRESULT CSimpleWindow::Register (HINSTANCE hInstance)
{
	WNDCLASSEX wnd = {0};

	wnd.cbSize = sizeof(WNDCLASSEX);
	wnd.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	wnd.cbClsExtra = 0;
	wnd.cbWndExtra = 0;
	wnd.hInstance = hInstance;
	wnd.hIcon = LoadIcon(hInstance, MAKEINTRESOURCEW(IDI_MAIN));
	wnd.hCursor = LoadCursor(NULL, IDC_ARROW);
	wnd.hbrBackground = NULL;
	wnd.lpszMenuName = MAKEINTRESOURCEW(IDR_MENU);
	wnd.lpszClassName = c_wzSimpleClass;

	return RegisterClass(&wnd,NULL);
}

HRESULT CSimpleWindow::Unregister (HINSTANCE hInstance)
{
	return UnregisterClass(c_wzSimpleClass, hInstance);
}

HRESULT CSimpleWindow::Initialize (INT nWidth, INT nHeight, INT nCmdShow)
{
	return Create(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE, WS_OVERLAPPEDWINDOW, c_wzSimpleClass, c_wzSimpleTitle, CW_USEDEFAULT, CW_USEDEFAULT, nWidth, nHeight, NULL, nCmdShow);
}

HINSTANCE CSimpleWindow::GetInstance (VOID)
{
	return m_hInstance;
}

VOID CSimpleWindow::OnFinalDestroy (HWND hwnd)
{
	PostQuitMessage(0);
}

HRESULT CSimpleWindow::FinalizeAndShow (DWORD dwStyle, INT nCmdShow)
{
	return __super::FinalizeAndShow(dwStyle, nCmdShow);
}

BOOL CSimpleWindow::OnCreate (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return FALSE;
}

BOOL CSimpleWindow::OnPaint (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(m_hwnd, &ps);

	FillRect(hdc, &ps.rcPaint, (HBRUSH)GetStockObject(WHITE_BRUSH));

	EndPaint(m_hwnd, &ps);
	return FALSE;
}

BOOL CSimpleWindow::OnEraseBackground (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	lResult = TRUE;
	return TRUE;
}

BOOL CSimpleWindow::OnCommand (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	BOOL fHandled = FALSE;

	switch(LOWORD(wParam))
	{
	case ID_FILE_EXIT:
		PostMessage(m_hwnd, WM_CLOSE, 0, 0);
		lResult = 0;
		break;
	case ID_HELP_ABOUT:
		{
			CDialogHost Host(m_hInstance);
			CAboutDialog dlgAbout;

			Host.Display(m_hwnd, &dlgAbout);
		}
		break;
	}

	return fHandled;
}

BOOL CSimpleWindow::OnClose (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return FALSE;
}
