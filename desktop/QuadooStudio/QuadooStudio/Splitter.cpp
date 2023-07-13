#include <windows.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Splitter.h"

const WCHAR c_wzSplitterClass[] = L"SplitterCls";

CSplitter::CSplitter (HINSTANCE hInstance) :
	m_hInstance(hInstance),
	m_hbrPattern(NULL),
	m_hwndLeft(NULL),
	m_fDragging(FALSE)
{
}

CSplitter::~CSplitter ()
{
}

HRESULT CSplitter::Register (HINSTANCE hInstance)
{
	WNDCLASSEX wnd = {0};

	wnd.cbSize = sizeof(WNDCLASSEX);
	wnd.style = CS_HREDRAW | CS_VREDRAW;
	wnd.cbClsExtra = 0;
	wnd.cbWndExtra = 0;
	wnd.hInstance = hInstance;
	wnd.hIcon = NULL;
	wnd.hCursor = LoadCursor(NULL, IDC_SIZEWE);
	wnd.hbrBackground = NULL;
	wnd.lpszMenuName = NULL;
	wnd.lpszClassName = c_wzSplitterClass;

	return RegisterClass(&wnd,NULL);
}

HRESULT CSplitter::Unregister (HINSTANCE hInstance)
{
	return UnregisterClass(c_wzSplitterClass, hInstance);
}

HRESULT CSplitter::Initialize (HWND hwndParent, HWND hwndLeft)
{
	m_hwndLeft = hwndLeft;
	return Create(0, WS_CHILD | WS_VISIBLE, c_wzSplitterClass, NULL, 0, 0, 3, 10, hwndParent, SW_SHOW);
}

// CBaseWindow

HINSTANCE CSplitter::GetInstance (VOID)
{
	return m_hInstance;
}

VOID CSplitter::OnFinalDestroy (HWND hwnd)
{
	SafeDeleteGdiObject(m_hbrPattern);
}

HRESULT CSplitter::FinalizeAndShow (DWORD dwStyle, INT nCmdShow)
{
	return __super::FinalizeAndShow(dwStyle, nCmdShow);
}

BOOL CSplitter::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	switch(message)
	{
	case WM_CREATE:
		{
			HBITMAP hbmSplitter = LoadBitmap(m_hInstance, MAKEINTRESOURCE(IDB_SPLITTER));
			if(NULL == hbmSplitter)
			{
				lResult = -1;
				return TRUE;
			}
			m_hbrPattern = CreatePatternBrush(hbmSplitter);
			DeleteObject(hbmSplitter);
		}
		break;

	case WM_ERASEBKGND:
		{
			RECT rc;
			HDC hdc = (HDC)wParam;

			GetClientRect(m_hwnd, &rc);
			FillRect(hdc, &rc, m_hbrPattern);
		}
		break;

	case WM_LBUTTONDOWN:
		m_xDragStart = GET_X_LPARAM(lParam);
		SetCapture(m_hwnd);
		m_fDragging = TRUE;
		break;

	case WM_MOUSEMOVE:
		if(m_fDragging)
		{
			HWND hwndParent = GetParent(m_hwnd);
			RECT rcLeft, rcParent;
			INT xDelta = GET_X_LPARAM(lParam) - m_xDragStart;

			GetWindowRect(m_hwndLeft, &rcLeft);
			rcLeft.right += xDelta;
			ScreenToClient(hwndParent, (POINT*)&rcLeft);
			ScreenToClient(hwndParent, (POINT*)&rcLeft + 1);
			MoveWindow(m_hwndLeft, rcLeft.left, rcLeft.top, rcLeft.right - rcLeft.left, rcLeft.bottom - rcLeft.top, TRUE);

			GetClientRect(hwndParent, &rcParent);
			SendMessage(hwndParent, WM_SIZE, 0, MAKELPARAM(rcParent.right - rcParent.left, rcParent.bottom - rcParent.top));
		}
		break;

	case WM_LBUTTONUP:
		if(m_fDragging)
		{
			m_fDragging = FALSE;
			ReleaseCapture();
		}
		break;
	}

	return __super::DefWindowProc(message, wParam, lParam, lResult);
}
