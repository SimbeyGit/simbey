#include <windows.h>
#include <CommCtrl.h>
#include <gdiplus.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Util\Formatting.h"
#include "GraphWindow.h"

const WCHAR c_wzGraphClass[] = L"GraphWindowCls";
const WCHAR c_wzGraphTitle[] = L"Graph Window";

CGraphWindow::CGraphWindow (HINSTANCE hInstance) :
	m_hInstance(hInstance),
	m_hwndStatus(NULL),
	m_fFullStatus(FALSE)
{
	m_graph.SetGraphType(GRAPH_XY);
	m_graph.SetBGColor(RGB(255,255,255));
	m_graph.SetGraphTarget(this);
}

CGraphWindow::~CGraphWindow ()
{
}

HRESULT CGraphWindow::Register (HINSTANCE hInstance)
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
	wnd.lpszMenuName = NULL;
	wnd.lpszClassName = c_wzGraphClass;

	return RegisterClass(&wnd,NULL);
}

HRESULT CGraphWindow::Unregister (HINSTANCE hInstance)
{
	return UnregisterClass(c_wzGraphClass, hInstance);
}

HRESULT CGraphWindow::Initialize (INT nWidth, INT nHeight, INT nCmdShow)
{
	m_graph.AttachContainer(this);

	return Create(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE, WS_OVERLAPPEDWINDOW, c_wzGraphClass, c_wzGraphTitle, CW_USEDEFAULT, CW_USEDEFAULT, nWidth, nHeight, NULL, nCmdShow);
}

// IGraphClient

VOID CGraphWindow::onGraphPaint (IGrapher* lpGraph)
{
}

VOID CGraphWindow::onGraphMouseMove (KEYS dwKeys, FLOAT x, FLOAT y)
{
	if(m_fFullStatus)
	{
		WCHAR wzCoords[32];
		if(SUCCEEDED(Formatting::TPrintF(wzCoords, ARRAYSIZE(wzCoords), NULL, L"%.1f, %.1f", x, y)))
			SendMessage(m_hwndStatus, SB_SETTEXT, 1, (LPARAM)wzCoords);
	}
}

VOID CGraphWindow::onGraphLBtnDown (KEYS dwKeys, FLOAT x, FLOAT y)
{
}

VOID CGraphWindow::onGraphLBtnUp (KEYS dwKeys, FLOAT x, FLOAT y)
{
}

BOOL CGraphWindow::onGraphRBtnDown (KEYS dwKeys, FLOAT x, FLOAT y)
{
	return FALSE;
}

VOID CGraphWindow::onGraphRBtnUp (KEYS dwKeys, FLOAT x, FLOAT y)
{
}

VOID CGraphWindow::onGraphLBtnDbl (KEYS dwKeys, FLOAT x, FLOAT y)
{
}

VOID CGraphWindow::onGraphRBtnDbl (KEYS dwKeys, FLOAT x, FLOAT y)
{
}

VOID CGraphWindow::onGraphViewChanged (BOOL fZoomChanged)
{
	if(m_fFullStatus)
	{
		WCHAR wzScale[32];
		if(SUCCEEDED(Formatting::TDoubleToString(m_graph.GetScale(), wzScale, ARRAYSIZE(wzScale), 1, NULL)))
			SendMessage(m_hwndStatus, SB_SETTEXT, 3, (LPARAM)wzScale);
	}
}

BOOL CGraphWindow::onGraphKeyDown (WPARAM iKey)
{
	return FALSE;
}

BOOL CGraphWindow::onGraphKeyUp (WPARAM iKey)
{
	return FALSE;
}

BOOL CGraphWindow::onGraphChar (WPARAM iKey)
{
	return FALSE;
}

BOOL CGraphWindow::onGraphWheel (SHORT sDistance, FLOAT x, FLOAT y)
{
	return FALSE;
}

HRESULT CGraphWindow::onGraphGetAcc (IAccessible** ppAccessible)
{
	return E_NOTIMPL;
}

// IGraphContainer

VOID WINAPI CGraphWindow::OnScaleChanged (FLOAT fScale)
{
}

VOID WINAPI CGraphWindow::OnGridSpacingChanged (INT iSpacing)
{
	if(m_fFullStatus)
	{
		WCHAR wzSpacing[32];
		if(SUCCEEDED(Formatting::TUInt32ToAsc(iSpacing, wzSpacing, ARRAYSIZE(wzSpacing), 10, NULL)))
			SendMessage(m_hwndStatus, SB_SETTEXT, 2, (LPARAM)wzSpacing);
	}
}

HRESULT WINAPI CGraphWindow::SetFocus (__in IGrapher* pGraphCtrl)
{
	::SetFocus(m_hwnd);
	return S_OK;
}

HRESULT WINAPI CGraphWindow::InvalidateContainer (__in IGrapher* pGraphCtrl)
{
	InvalidateRect(m_hwnd, NULL, FALSE);
	return S_OK;
}

HRESULT WINAPI CGraphWindow::ScreenToWindowless (__in IGrapher* pGraphCtrl, __inout PPOINT ppt)
{
	if(ScreenToClient(m_hwnd, ppt))
		return S_OK;
	return E_FAIL;
}

BOOL WINAPI CGraphWindow::CaptureMouse (__in IGrapher* pGraphCtrl, BOOL fCapture)
{
	if(fCapture)
	{
		SetCapture(m_hwnd);
		return TRUE;
	}
	else
		return ReleaseCapture();
}

VOID WINAPI CGraphWindow::DrawDib24 (HDC hdcDest, LONG x, LONG y, HDC hdcSrc, const BYTE* pcDIB24, LONG xSize, LONG ySize, LONG lPitch)
{
	BitBlt(hdcDest, x, y, xSize, ySize, hdcSrc, 0, 0, SRCCOPY);
}

HINSTANCE CGraphWindow::GetInstance (VOID)
{
	return m_hInstance;
}

VOID CGraphWindow::OnFinalDestroy (HWND hwnd)
{
	m_hwndStatus = NULL;
	m_graph.AttachContainer(NULL);
	PostQuitMessage(0);
}

HRESULT CGraphWindow::FinalizeAndShow (DWORD dwStyle, INT nCmdShow)
{
	return __super::FinalizeAndShow(dwStyle, nCmdShow);
}

VOID CGraphWindow::ConfigureStatusBar (VOID)
{
	INT rgParts[4], cParts = ARRAYSIZE(rgParts);

	if(m_rcStatus.right > 300)
	{
		rgParts[0] = m_rcStatus.right - 300;
		rgParts[1] = rgParts[0] + 160;
		rgParts[2] = rgParts[1] + 55;
		rgParts[3] = -1;
		m_fFullStatus = TRUE;

		// Update the grid spacing and zoom labels.
		OnGridSpacingChanged(m_graph.GetGridSpacing());
		onGraphViewChanged(FALSE);
	}
	else
	{
		rgParts[0] = -1;
		cParts = 1;
		m_fFullStatus = FALSE;
	}
	SendMessage(m_hwndStatus, SB_SETPARTS, cParts, (LPARAM)rgParts);
}

BOOL CGraphWindow::OnCreate (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	m_hwndStatus = CreateStatusWindowW(WS_VISIBLE | WS_CHILD, L"Ready", m_hwnd, 1);
	if(NULL == m_hwndStatus)
	{
		lResult = 0;
		return TRUE;
	}

	GetClientRect(m_hwndStatus, &m_rcStatus);
	ConfigureStatusBar();

	return FALSE;
}

BOOL CGraphWindow::OnPaint (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(m_hwnd, &ps);

	m_graph.Paint(hdc);

	EndPaint(m_hwnd, &ps);
	return FALSE;
}

BOOL CGraphWindow::OnEraseBackground (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	lResult = TRUE;
	return TRUE;
}

BOOL CGraphWindow::OnSize (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	m_graph.SizeObject(LOWORD(lParam), HIWORD(lParam) - m_rcStatus.bottom);

	m_rcStatus.right = LOWORD(lParam);
	MoveWindow(m_hwndStatus, 0, HIWORD(lParam) - m_rcStatus.bottom, m_rcStatus.right, m_rcStatus.bottom, TRUE);

	ConfigureStatusBar();

	return FALSE;
}

BOOL CGraphWindow::OnClose (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return FALSE;
}
