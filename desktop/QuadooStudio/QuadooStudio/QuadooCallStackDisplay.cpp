#include <windows.h>
#include <commctrl.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Util\Formatting.h"
#include "Library\DPI.h"
#include "DebugSession.h"
#include "Tabs.h"
#include "QuadooCallStackDisplay.h"

const WCHAR c_wzCallStackDisplayClass[] = L"CallStackDisplayCls";

CQuadooCallStackDisplay::CQuadooCallStackDisplay (HINSTANCE hInstance, CTabs* pTabs) :
	m_hInstance(hInstance),
	m_pTabs(pTabs),
	m_hwndList(NULL)
{
}

CQuadooCallStackDisplay::~CQuadooCallStackDisplay ()
{
}

HRESULT CQuadooCallStackDisplay::Register (HINSTANCE hInstance)
{
	WNDCLASSEX wnd = {0};

	wnd.cbSize = sizeof(WNDCLASSEX);
	wnd.style = CS_HREDRAW | CS_VREDRAW;
	wnd.cbClsExtra = 0;
	wnd.cbWndExtra = 0;
	wnd.hInstance = hInstance;
	wnd.hIcon = NULL;
	wnd.hCursor = LoadCursor(NULL, IDC_ARROW);
	wnd.hbrBackground = NULL;
	wnd.lpszMenuName = NULL;
	wnd.lpszClassName = c_wzCallStackDisplayClass;

	return RegisterClass(&wnd,NULL);
}

HRESULT CQuadooCallStackDisplay::Unregister (HINSTANCE hInstance)
{
	return UnregisterClass(c_wzCallStackDisplayClass, hInstance);
}

INT CQuadooCallStackDisplay::GetDefaultHeight (VOID)
{
	return (INT)DPI::ScaleY(120.0f);
}

HRESULT CQuadooCallStackDisplay::Initialize (HWND hwndParent)
{
	INT nHeight = GetDefaultHeight();
	RECT rcParent;

	GetClientRect(hwndParent, &rcParent);

	return Create(0, WS_CHILD | WS_VISIBLE, c_wzCallStackDisplayClass, NULL, 0, rcParent.bottom - nHeight, rcParent.right, nHeight, hwndParent, SW_SHOW);
}

VOID CQuadooCallStackDisplay::Clear (VOID)
{
	if(m_hwndList)
		ListView_DeleteAllItems(m_hwndList);
}

VOID CQuadooCallStackDisplay::UpdateColorScheme (bool fDarkMode)
{
	InvalidateRect(m_hwnd, NULL, FALSE);
}

HRESULT CQuadooCallStackDisplay::SetFrames (CQuadooDebugSession* pSession)
{
	HRESULT hr = S_OK;
	DWORD cFrames = pSession ? pSession->GetStackFrameCount() : 0;

	Clear();
	for(DWORD i = 0; i < cFrames; i++)
	{
		PCWSTR pcwzName;
		DWORD nIP, nSP, nFP, nExecutionDepth;
		LVITEM item = {0};
		WCHAR wzSP[16], wzFP[16], wzIP[16];

		Check(pSession->GetStackFrame(i, &pcwzName, &nIP, &nSP, &nFP, &nExecutionDepth));
		UNREFERENCED_PARAMETER(nExecutionDepth);
		Check(Formatting::TUInt32ToAsc(nSP, wzSP, ARRAYSIZE(wzSP), 10, NULL));
		Check(Formatting::TUInt32ToAsc(nFP, wzFP, ARRAYSIZE(wzFP), 10, NULL));
		Check(Formatting::TPrintF(wzIP, ARRAYSIZE(wzIP), NULL, L"0x%.8X", nIP));

		item.mask = LVIF_TEXT;
		item.iItem = static_cast<INT>(i);
		item.pszText = const_cast<PWSTR>(pcwzName);
		CheckIf(-1 == ListView_InsertItem(m_hwndList, &item), HRESULT_FROM_WIN32(ERROR_INVALID_DATA));

		item.iSubItem = 1;
		item.pszText = wzSP;
		ListView_SetItem(m_hwndList, &item);
		item.iSubItem = 2;
		item.pszText = wzFP;
		ListView_SetItem(m_hwndList, &item);
		item.iSubItem = 3;
		item.pszText = wzIP;
		ListView_SetItem(m_hwndList, &item);
	}

Cleanup:
	return hr;
}

// CBaseWindow

HINSTANCE CQuadooCallStackDisplay::GetInstance (VOID)
{
	return m_hInstance;
}

VOID CQuadooCallStackDisplay::OnFinalDestroy (HWND hwnd)
{
	m_hwndList = NULL;
}

HRESULT CQuadooCallStackDisplay::FinalizeAndShow (DWORD dwStyle, INT nCmdShow)
{
	return __super::FinalizeAndShow(dwStyle, nCmdShow);
}

BOOL CQuadooCallStackDisplay::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	switch(message)
	{
	case WM_CREATE:
		{
			LVCOLUMN column = {0};

			m_hwndList = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL,
				WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
				0, 24, 0, 0, m_hwnd, NULL, m_hInstance, NULL);
			if(NULL == m_hwndList)
			{
				lResult = -1;
				return TRUE;
			}
			SendMessage(m_hwndList, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
			ListView_SetExtendedListViewStyle(m_hwndList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

			column.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
			column.fmt = LVCFMT_LEFT;
			column.cx = 300;
			column.pszText = L"Function/Method";
			ListView_InsertColumn(m_hwndList, 0, &column);
			column.cx = 70;
			column.pszText = L"SP";
			column.iSubItem = 1;
			ListView_InsertColumn(m_hwndList, 1, &column);
			column.pszText = L"FP";
			column.iSubItem = 2;
			ListView_InsertColumn(m_hwndList, 2, &column);
			column.cx = 92;
			column.pszText = L"IP";
			column.iSubItem = 3;
			ListView_InsertColumn(m_hwndList, 3, &column);
		}
		break;

	case WM_PAINT:
		{
			const TAB_COLORS& tabColors = m_pTabs->GetTabColors();
			INT cyHeader = (INT)DPI::ScaleY(25.0f);
			PAINTSTRUCT ps;
			RECT rc;
			HDC hdc = BeginPaint(m_hwnd, &ps);
			HBRUSH hbr = CreateSolidBrush(tabColors.crNormalOutline.ToCOLORREF());
			HFONT hFont = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
			HGDIOBJ hOldFont = SelectObject(hdc, hFont);

			GetClientRect(m_hwnd, &rc);
			INT yBottom = rc.bottom;

			rc.top = 0;
			rc.bottom = 1;
			FillRect(hdc, &rc, hbr);
			DeleteObject(hbr);

			hbr = CreateSolidBrush(tabColors.crBackground.ToCOLORREF());
			rc.top = 1;
			rc.bottom = min(yBottom, cyHeader);
			FillRect(hdc, &rc, hbr);
			SetBkMode(hdc, TRANSPARENT);
			SetTextColor(hdc, tabColors.crLabel.ToCOLORREF());
			rc.left += 6;
			DrawText(hdc, L"Call Stack", -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

			SelectObject(hdc, hOldFont);
			DeleteObject(hbr);
			EndPaint(m_hwnd, &ps);
		}
		lResult = 0;
		return TRUE;

	case WM_SIZE:
		if(m_hwndList)
		{
			INT nWidth = LOWORD(lParam);
			INT nHeight = HIWORD(lParam);
			INT nNameWidth = max(100, nWidth - 70 - 70 - 92 - GetSystemMetrics(SM_CXVSCROLL) - 8);
			INT cyHeader = (INT)DPI::ScaleY(25.0f);

			MoveWindow(m_hwndList, 0, cyHeader, nWidth, max(0, nHeight - cyHeader), TRUE);
			ListView_SetColumnWidth(m_hwndList, 0, nNameWidth);
		}
		lResult = 0;
		return TRUE;
	}

	return __super::DefWindowProc(message, wParam, lParam, lResult);
}
