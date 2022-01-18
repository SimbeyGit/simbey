#include <windows.h>
#include <CommCtrl.h>
#include <gdiplus.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Util\Formatting.h"
#include "CADWindow.h"

const WCHAR c_wzCADClass[] = L"CADWindowCls";
const WCHAR c_wzCADTitle[] = L"CAD Window";

#define	IDC_STATUS_BAR			1

CCADWindow::CCADWindow (HINSTANCE hInstance) :
	m_hInstance(hInstance),
	m_hwndStatus(NULL),
	m_fFullStatus(FALSE),
	m_pCAD(NULL)
{
	m_graph.SetGraphType(GRAPH_XY);
	m_graph.SetBGColor(RGB(0,0,0));
	m_graph.SetGridColor(RGB(160, 160, 160));
	m_graph.SetGraphTarget(this);
}

CCADWindow::~CCADWindow ()
{
	for(sysint i = 0; i < m_mapLines.Length(); i++)
	{
		CAD_LINE* pLine = *m_mapLines.GetValuePtr(i);
		SafeRelease(pLine->pLineA);
		SafeRelease(pLine->pLineB);
	}
	for(sysint i = 0; i < m_mapPolygons.Length(); i++)
	{
		CAD_POLYGON* pPolygon = *m_mapPolygons.GetValuePtr(i);
		SafeRelease(pPolygon->pPolygon);
	}

	m_mapVertices.DeleteAll();
	m_mapLines.DeleteAll();
	m_mapPolygons.DeleteAll();
}

HRESULT CCADWindow::Register (HINSTANCE hInstance)
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
	wnd.lpszClassName = c_wzCADClass;

	return RegisterClass(&wnd,NULL);
}

HRESULT CCADWindow::Unregister (HINSTANCE hInstance)
{
	return UnregisterClass(c_wzCADClass, hInstance);
}

HRESULT CCADWindow::Initialize (INT nWidth, INT nHeight, INT nCmdShow)
{
	m_graph.AttachContainer(this);

	return Create(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE, WS_OVERLAPPEDWINDOW, c_wzCADClass, c_wzCADTitle, CW_USEDEFAULT, CW_USEDEFAULT, nWidth, nHeight, NULL, nCmdShow);
}

// IGraphClient

VOID CCADWindow::onGraphPaint (IGrapher* lpGraph)
{
	m_pCAD->Paint();
}

VOID CCADWindow::onGraphMouseMove (KEYS dwKeys, FLOAT x, FLOAT y)
{
	m_pCAD->MouseMove(dwKeys, x, y);

	if(m_fFullStatus)
	{
		WCHAR wzCoords[32];
		m_pCAD->SnapCoordinate(x, y);
		if(SUCCEEDED(Formatting::TPrintF(wzCoords, ARRAYSIZE(wzCoords), NULL, L"%.1f, %.1f", x, y)))
			SendMessage(m_hwndStatus, SB_SETTEXT, 2, (LPARAM)wzCoords);
	}
}

VOID CCADWindow::onGraphLBtnDown (KEYS dwKeys, FLOAT x, FLOAT y)
{
	m_pCAD->LBtnDown(dwKeys, x, y);
}

VOID CCADWindow::onGraphLBtnUp (KEYS dwKeys, FLOAT x, FLOAT y)
{
	m_pCAD->LBtnUp(dwKeys, x, y);
}

BOOL CCADWindow::onGraphRBtnDown (KEYS dwKeys, FLOAT x, FLOAT y)
{
	return m_pCAD->RBtnDown(dwKeys, x, y);
}

VOID CCADWindow::onGraphRBtnUp (KEYS dwKeys, FLOAT x, FLOAT y)
{
	m_pCAD->RBtnUp(dwKeys, x, y);
}

VOID CCADWindow::onGraphLBtnDbl (KEYS dwKeys, FLOAT x, FLOAT y)
{
	m_pCAD->LBtnDbl(dwKeys, x, y);
}

VOID CCADWindow::onGraphRBtnDbl (KEYS dwKeys, FLOAT x, FLOAT y)
{
}

VOID CCADWindow::onGraphViewChanged (BOOL fZoomChanged)
{
	if(m_fFullStatus)
	{
		WCHAR wzScale[32];
		if(SUCCEEDED(Formatting::TDoubleToString(m_graph.GetScale(), wzScale, ARRAYSIZE(wzScale), 1, NULL)))
			SendMessage(m_hwndStatus, SB_SETTEXT, 4, (LPARAM)wzScale);
	}
}

BOOL CCADWindow::onGraphKeyDown (WPARAM iKey)
{
	return FALSE;
}

BOOL CCADWindow::onGraphKeyUp (WPARAM iKey)
{
	return m_pCAD->KeyUp(iKey);
}

BOOL CCADWindow::onGraphChar (WPARAM iKey)
{
	iKey = TUpperCase(iKey);

	if(iKey == 'V')
	{
		m_pCAD->SetMode(CAD::Vertex);
		UpdateStatusBarMode();
		Invalidate(FALSE);
	}
	else if(iKey == 'L')
	{
		m_pCAD->SetMode(CAD::Line);
		UpdateStatusBarMode();
		Invalidate(FALSE);
	}
	else if(iKey == 'P')
	{
		m_pCAD->SetMode(CAD::Polygon);
		UpdateStatusBarMode();
		Invalidate(FALSE);
	}
	else if(iKey == 'D')
	{
		m_pCAD->SetMode(CAD::Drawing);
		UpdateStatusBarMode();
		Invalidate(FALSE);
	}

	return FALSE;
}

BOOL CCADWindow::onGraphWheel (SHORT sDistance, FLOAT x, FLOAT y)
{
	return FALSE;
}

HRESULT CCADWindow::onGraphGetAcc (IAccessible** ppAccessible)
{
	return E_NOTIMPL;
}

// IGraphContainer

VOID WINAPI CCADWindow::OnScaleChanged (FLOAT fScale)
{
}

VOID WINAPI CCADWindow::OnGridSpacingChanged (INT iSpacing)
{
	if(m_fFullStatus)
	{
		WCHAR wzSpacing[32];
		if(SUCCEEDED(Formatting::TUInt32ToAsc(iSpacing, wzSpacing, ARRAYSIZE(wzSpacing), 10, NULL)))
			SendMessage(m_hwndStatus, SB_SETTEXT, 3, (LPARAM)wzSpacing);
	}
}

HRESULT WINAPI CCADWindow::SetFocus (__in IGrapher* pGraphCtrl)
{
	::SetFocus(m_hwnd);
	return S_OK;
}

HRESULT WINAPI CCADWindow::InvalidateContainer (__in IGrapher* pGraphCtrl)
{
	InvalidateRect(m_hwnd, NULL, FALSE);
	return S_OK;
}

HRESULT WINAPI CCADWindow::ScreenToWindowless (__in IGrapher* pGraphCtrl, __inout PPOINT ppt)
{
	if(ScreenToClient(m_hwnd, ppt))
		return S_OK;
	return E_FAIL;
}

BOOL WINAPI CCADWindow::CaptureMouse (__in IGrapher* pGraphCtrl, BOOL fCapture)
{
	if(fCapture)
	{
		SetCapture(m_hwnd);
		return TRUE;
	}
	else
		return ReleaseCapture();
}

VOID WINAPI CCADWindow::DrawDib24 (HDC hdcDest, LONG x, LONG y, HDC hdcSrc, const BYTE* pcDIB24, LONG xSize, LONG ySize, LONG lPitch)
{
	BitBlt(hdcDest, x, y, xSize, ySize, hdcSrc, 0, 0, SRCCOPY);
}

// ICADHost

BOOL CCADWindow::OnSplitLine (const CAD_LINE* pcLine, const FPOINT& fpSplit, ICADLine* pSplit, __deref_out ICADLine** ppNewSplit)
{
	*ppNewSplit = NULL;
	return TRUE;
}

BOOL CCADWindow::OnCreateLine (DWORD idPolygon, DWORD idNewLine, __deref_out ICADLine** ppLine)
{
	*ppLine = NULL;
	return TRUE;
}

BOOL CCADWindow::OnCreatePolygon (DWORD idPolygon, __deref_out ICADPolygon** ppPolygon)
{
	*ppPolygon = NULL;
	return TRUE;
}

VOID CCADWindow::OnBeginDrawing (VOID)
{
}

VOID CCADWindow::OnDrawLineStatus (FLOAT rLength)
{
	WCHAR wzStatus[64];
	if(SUCCEEDED(Formatting::TPrintF(wzStatus, ARRAYSIZE(wzStatus), NULL, L"Length: %.1f", rLength)))
		SendMessage(m_hwndStatus, SB_SETTEXT, 0, (LPARAM)wzStatus);
}

VOID CCADWindow::OnEndDrawing (VOID)
{
	SendMessage(m_hwndStatus, SB_SETTEXT, 0, (LPARAM)L"Ready");
}

HINSTANCE CCADWindow::GetInstance (VOID)
{
	return m_hInstance;
}

VOID CCADWindow::OnFinalDestroy (HWND hwnd)
{
	SafeDelete(m_pCAD);
	m_hwndStatus = NULL;
	m_graph.AttachContainer(NULL);
	PostQuitMessage(0);
}

HRESULT CCADWindow::FinalizeAndShow (DWORD dwStyle, INT nCmdShow)
{
	return __super::FinalizeAndShow(dwStyle, nCmdShow);
}

VOID CCADWindow::ConfigureStatusBar (VOID)
{
	INT rgParts[5], cParts = ARRAYSIZE(rgParts);

	if(m_rcStatus.right > 300)
	{
		rgParts[0] = m_rcStatus.right - 375;
		rgParts[1] = rgParts[0] + 75;
		rgParts[2] = rgParts[1] + 160;
		rgParts[3] = rgParts[2] + 55;
		rgParts[4] = -1;
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

VOID CCADWindow::UpdateStatusBarMode (VOID)
{
	switch(m_pCAD->GetMode())
	{
	case CAD::Vertex:
		SendMessage(m_hwndStatus, SB_SETTEXT, 1, (LPARAM)L"Vertex");
		break;
	case CAD::Line:
		SendMessage(m_hwndStatus, SB_SETTEXT, 1, (LPARAM)L"Line");
		break;
	case CAD::Polygon:
		SendMessage(m_hwndStatus, SB_SETTEXT, 1, (LPARAM)L"Polygon");
		break;
	case CAD::Drawing:
		SendMessage(m_hwndStatus, SB_SETTEXT, 1, (LPARAM)L"Drawing");
		break;
	}
}

BOOL CCADWindow::OnCreate (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	m_hwndStatus = CreateStatusWindowW(WS_VISIBLE | WS_CHILD, L"Ready", m_hwnd, IDC_STATUS_BAR);
	if(NULL == m_hwndStatus)
	{
		lResult = 0;
		return TRUE;
	}

	GetClientRect(m_hwndStatus, &m_rcStatus);
	ConfigureStatusBar();

	m_pCAD = __new CCADCtrl(this, &m_mapVertices, &m_mapLines, &m_mapPolygons, &m_graph, CAD::Vertex);
	if(NULL == m_pCAD)
	{
		lResult = 0;
		return TRUE;
	}

	UpdateStatusBarMode();

	return FALSE;
}

BOOL CCADWindow::OnPaint (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(m_hwnd, &ps);

	m_graph.Paint(hdc);

	EndPaint(m_hwnd, &ps);
	return FALSE;
}

BOOL CCADWindow::OnEraseBackground (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	lResult = TRUE;
	return TRUE;
}

BOOL CCADWindow::OnSize (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	m_graph.SizeObject(LOWORD(lParam), HIWORD(lParam) - m_rcStatus.bottom);

	m_rcStatus.right = LOWORD(lParam);
	MoveWindow(m_hwndStatus, 0, HIWORD(lParam) - m_rcStatus.bottom, m_rcStatus.right, m_rcStatus.bottom, TRUE);

	ConfigureStatusBar();

	return FALSE;
}

BOOL CCADWindow::OnClose (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return FALSE;
}
