#include <windows.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Core\MemoryStream.h"
#include "Library\Util\StreamHelpers.h"
#include "Library\Util\Formatting.h"
#include "Library\Window\DialogHost.h"
#include "Library\ChooseFile.h"
#include "ImageProc.h"
#include "NewImageDlg.h"
#include "MDIWindow.h"

const WCHAR c_wzMDIClass[] = L"MDIWindowCls";
const WCHAR c_wzMDITitle[] = L"MDI Window";

const WCHAR c_wzMDIChild[] = L"MDIChildCls";
const WCHAR c_wzImageChild[] = L"ImageChildCls";

const float ZoomValueList[] =
{
	1.0f,
	0.667f,
	0.5f,
	0.25f,
	0.167f,
	0.125f,
	0.0833f,
	0.0625f,
	0.05f,
	0.04f,
	0.03f,
	0.015f,
	0.01f,
	0.007f,
	0.005f,
	0.004f,
	0.003f,
	0.002f,
	0.001f,
};

CMDIChild::CMDIChild (HINSTANCE hInstance, INT nType) :
	m_hInstance(hInstance),
	m_nType(nType)
{
}

CMDIChild::~CMDIChild ()
{
}

HRESULT CMDIChild::Register (HINSTANCE hInstance)
{
	WNDCLASSEX wnd = {0};

	wnd.cbSize = sizeof(WNDCLASSEX);
	wnd.style = CS_HREDRAW | CS_VREDRAW;
	wnd.cbClsExtra = 0;
	wnd.cbWndExtra = 0;
	wnd.hInstance = hInstance;
	wnd.hIcon = LoadIcon(hInstance, MAKEINTRESOURCEW(IDI_MAIN));
	wnd.hCursor = LoadCursor(NULL, IDC_ARROW);
	wnd.hbrBackground = NULL;
	wnd.lpszMenuName = NULL;
	wnd.lpszClassName = c_wzMDIChild;

	return RegisterMDIChild(&wnd, NULL);
}

HRESULT CMDIChild::Unregister (HINSTANCE hInstance)
{
	return UnregisterClass(c_wzMDIChild, hInstance);
}

HRESULT CMDIChild::Initialize (CBaseMDIFrame* pFrame, INT nWidth, INT nHeight)
{
	PCWSTR pcwzName;

	if(0 == m_nType)
		pcwzName = L"Pie Chart";
	else
		pcwzName = L"Square";

	return AttachToFrame(c_wzMDIChild, pcwzName,
		WS_CHILD | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, nWidth, nHeight, pFrame);
}

HINSTANCE CMDIChild::GetInstance (VOID)
{
	return m_hInstance;
}

BOOL CMDIChild::OnPaint (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(m_hwnd, &ps);
	RECT rc;

	GetClientRect(m_hwnd, &rc);
	FillRect(hdc, &ps.rcPaint, (HBRUSH)GetStockObject(WHITE_BRUSH));

	if(0 == m_nType)
	{
		INT nX, nY;
		DWORD dwRadius;
		FLOAT xStartAngle = 30.0f, xSweepAngle = 300.0f;

		nX = rc.right / 2 - 10;
		nY = rc.bottom / 2;

		dwRadius = MulDiv(min(rc.right, rc.bottom), 2, 5);

		HBRUSH hbrPrev = (HBRUSH)SelectObject(hdc, GetStockObject(GRAY_BRUSH));

		BeginPath(hdc);
		MoveToEx(hdc, nX, nY, (LPPOINT) NULL);
		AngleArc(hdc, nX, nY, dwRadius, xStartAngle, xSweepAngle);
		LineTo(hdc, nX, nY);
		EndPath(hdc);
		StrokeAndFillPath(hdc);

		SelectObject(hdc, hbrPrev);

		xStartAngle = 330.0f;
		xSweepAngle = 60.0f;
		nX += 20;

		hbrPrev = (HBRUSH)SelectObject(hdc, CreateSolidBrush(RGB(255, 255, 0)));

		BeginPath(hdc);
		MoveToEx(hdc, nX, nY, (LPPOINT) NULL);
		AngleArc(hdc, nX, nY, dwRadius, xStartAngle, xSweepAngle);
		LineTo(hdc, nX, nY);
		EndPath(hdc);
		StrokeAndFillPath(hdc);

		DeleteObject(SelectObject(hdc, hbrPrev));
	}
	else if(1 == m_nType)
	{
		HBRUSH hbrFill = (HBRUSH)CreateSolidBrush(RGB(255, 255, 0));

		rc.left += 20;
		rc.top += 20;
		rc.right -= 20;
		rc.bottom -= 20;

		FillRect(hdc, &rc, hbrFill);
		FrameRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));

		DeleteObject(hbrFill);
	}

	EndPaint(m_hwnd, &ps);
	return FALSE;
}

CImageChild::CImageChild (HINSTANCE hInstance) :
	m_hInstance(hInstance),
	m_pSIF(NULL),
	m_hbrTransparent(NULL),
	m_fZoom(1.0f),
	m_fZoomStep(0.1f),
	m_fMinZoom(0.001f),
	m_fMaxZoom(16.0f),
	m_xScrollPos(0),
	m_yScrollPos(0),
	m_bLButtonClicked(FALSE),
	m_fSelectionMode(TRUE),
	m_bIsCtrlPressed(FALSE),
	m_bIsAltPressed(FALSE),
	m_nSelectedLayerIndex(-1)
{
}

CImageChild::~CImageChild ()
{
	SafeDeleteGdiObject(m_hbrTransparent);

	if(m_pSIF)
	{
		m_pSIF->Close();
		m_pSIF->Release();
	}
}

HRESULT CImageChild::Register (HINSTANCE hInstance)
{
	WNDCLASSEX wnd = {0};

	wnd.cbSize = sizeof(WNDCLASSEX);
	wnd.style = CS_HREDRAW | CS_VREDRAW;
	wnd.cbClsExtra = 0;
	wnd.cbWndExtra = 0;
	wnd.hInstance = hInstance;
	wnd.hIcon = LoadIcon(hInstance, MAKEINTRESOURCEW(IDI_MAIN));
	wnd.hCursor = LoadCursor(NULL, IDC_ARROW);
	wnd.hbrBackground = NULL;
	wnd.lpszMenuName = NULL;
	wnd.lpszClassName = c_wzImageChild;

	return RegisterMDIChild(&wnd, NULL);
}

HRESULT CImageChild::Unregister (HINSTANCE hInstance)
{
	return UnregisterClass(c_wzImageChild, hInstance);
}

HRESULT CImageChild::Initialize (CBaseMDIFrame* pFrame, INT nWidth, INT nHeight)
{
	HRESULT hr;
	HBITMAP hbmPattern = (HBITMAP)LoadImage(m_hInstance, MAKEINTRESOURCE(IDB_TRANS_PATTERN), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);

	CheckIfGetLastError(NULL == hbmPattern);
	m_hbrTransparent = CreatePatternBrush(hbmPattern);
	CheckIfGetLastError(NULL == m_hbrTransparent);

	Check(sifCreateNew(&m_pSIF));
	
	m_sImage.cx = nWidth;
	m_sImage.cy = nHeight;

	INT nScreenWidth = GetSystemMetrics(SM_CXSCREEN);
	INT nScreenHeight = GetSystemMetrics(SM_CYSCREEN);

	if(m_sImage.cx > nScreenWidth * 2 / 3 || m_sImage.cy > nScreenHeight * 2 / 3)
	{
		m_fZoom = (FLOAT)min((FLOAT)nScreenWidth * 2.0 / 3.0 / m_sImage.cx, (FLOAT)nScreenHeight * 2.0 / 3.0 / m_sImage.cy);
		CheckZoomValue();
	}

	PCWSTR pcwzPtr = L"New Image";
	Check(TStrCchCpy(m_wzFileName, ARRAYSIZE(m_wzFileName), pcwzPtr));

	Check(AttachToFrame(
		c_wzImageChild, 
		L"New Image",
		WS_CHILD | WS_VISIBLE | WS_OVERLAPPEDWINDOW, 
		CW_USEDEFAULT, 
		CW_USEDEFAULT,
		(INT)(nWidth * m_fZoom) + 17, 
		(INT)(nHeight * m_fZoom) + 40, 
		pFrame));
	LoadCursors();
	Check(UpdateTitleWithZoom());

Cleanup:
	SafeDeleteGdiObject(hbmPattern);
	return hr;
}

HRESULT CImageChild::AddLayer (PCWSTR pcwzImageFile)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeFileLayer> srLayer;
	DWORD idxLayer;

	Check(sifAddImageFileAsLayer(pcwzImageFile, m_pSIF, &idxLayer));
	if(SUCCEEDED(m_pSIF->GetLayerByIndex(idxLayer, &srLayer)))
	{
		SIZE sLayer;
		LONG x = 0, y = 0;

		srLayer->GetSize(&sLayer);

		if(m_sImage.cx > sLayer.cx)
			x = rand() % (INT)(m_sImage.cx - sLayer.cx);
		if(m_sImage.cy > sLayer.cy)
			y = rand() % (INT)(m_sImage.cy - sLayer.cy);

		srLayer->SetPosition(x, y);
	}
	Invalidate(TRUE);

Cleanup:
	return hr;
}

HRESULT CImageChild::AddSolidColorLayer (COLORREF cr, SIZE size)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeFileLayer> srLayer;
	DWORD idxIndex, cbBits;
	PBYTE pBits;
	BYTE bPixel[4];

	Check(m_pSIF->AddLayer(size.cx, size.cy, &srLayer, &idxIndex));
	Check(srLayer->GetBitsPtr(&pBits, &cbBits));

	bPixel[0] = GetRValue(cr);
	bPixel[1] = GetGValue(cr);
	bPixel[2] = GetBValue(cr);
	bPixel[3] = (cr & 0xFF000000) >> 24;

	for(DWORD i = 0; i < cbBits; i += sizeof(DWORD))
		CopyMemory(pBits + i, bPixel, sizeof(bPixel));

Cleanup:
	return hr;
}

void CImageChild::LoadCursors()
{
	m_hZoomInCursor = LoadCursor(GetModuleHandle(NULL), MAKEINTRESOURCE(IDC_CURSOR_ZOOM_IN));
	if (m_hZoomInCursor == NULL)
	{
		MessageBox(NULL, L"Failed to load zoom in cursor!", L"Error", MB_OK | MB_ICONERROR);
	}

	m_hZoomOutCursor = LoadCursor(GetModuleHandle(NULL), MAKEINTRESOURCE(IDC_CURSOR_ZOOM_OUT));
	if (m_hZoomOutCursor == NULL)
	{
		MessageBox(NULL, L"Failed to load zoom out cursor!", L"Error", MB_OK | MB_ICONERROR);
	}
	m_hDefaultCursor = LoadCursor(NULL, IDC_ARROW);
}

void CImageChild::UpdateCursor()
{
	if (m_bIsAltPressed)
	{
		SetCursor(m_hZoomOutCursor);
		return;
	}
	if (m_bIsCtrlPressed)
	{
		SetCursor(m_hZoomInCursor);
		return;
	}
	SetCursor(m_hDefaultCursor);
}

void CImageChild::_SetScrollSizes()
{
	RECT windowRect ;
	GetWindowRect(m_hwnd, &windowRect);

	SCROLLINFO si;
	si.cbSize = sizeof(si);
	si.fMask = SIF_RANGE | SIF_PAGE;
	si.nMin = 0;
	si.nMax = (INT)(m_sImage.cy * m_fZoom);
	si.nPage = windowRect.bottom - windowRect.top - 38;
	if(si.nPage > si.nMax)
		m_yScrollPos = 0;
	SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);

	si.nMax = m_sImage.cx * m_fZoom;
	si.nPage = windowRect.right - windowRect.left - 15; 
	if(si.nPage > si.nMax)
		m_xScrollPos = 0;
	SetScrollInfo(m_hwnd, SB_HORZ, &si, TRUE);
}

void CImageChild::_SetScrollPos (HWND hWnd, int nBar, int pos)
{
	SCROLLINFO si;
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	GetScrollInfo(hWnd, nBar, &si);
	int tempPos = pos;
	if(tempPos < si.nMin)
		tempPos = si.nMin;
	if(tempPos > si.nMax - si.nPage + 1)
		tempPos = si.nMax - si.nPage + 1;
	if(nBar == SB_VERT)
		m_yScrollPos = tempPos;
	else
		m_xScrollPos = tempPos;
	si.nPos = tempPos;
	SetScrollInfo(hWnd, nBar, &si, TRUE);
}

void CImageChild::ZoomToRectangle ()
{
	float tempfZoom = m_fZoom;
	RECT rc;
	GetClientRect(m_hwnd, &rc);

	int nRectWidth = abs(m_xCurrDrag - m_xStartDrag);
	int nRectHeight = abs(m_yCurrDrag - m_yStartDrag);

	if(nRectWidth < 10 || nRectHeight < 10)
		return;

	const int cx = rc.right - rc.left;
	const int cy = rc.bottom - rc.top;
	const int vx = (int)(m_sImage.cx * m_fZoom);
	const int vy = (int)(m_sImage.cy * m_fZoom);

	float fZoom = min((float)cx / nRectWidth, (float)cy / nRectHeight);
	m_fZoom *= fZoom;
	if(m_fZoom > m_fMaxZoom)
	{
		m_fZoom = m_fMaxZoom;
	}

	UpdateTitleWithZoom();

	if(vx > cx)
	{
		m_xScrollPos = (int)((m_xScrollPos + min(m_xStartDrag, m_xCurrDrag)) * fZoom);
	}
	else
	{
		m_xScrollPos = (int)((min(m_xStartDrag, m_xCurrDrag) - (cx - vx) / 2) * fZoom);
	}

	if(vy > cy)
	{
		m_yScrollPos = (int)((m_yScrollPos + min(m_yCurrDrag, m_yStartDrag)) * fZoom);
	}
	else
	{
		m_yScrollPos = (int)((min(m_yCurrDrag, m_yStartDrag) - (cy - vy) / 2) * fZoom);
	}

	_SetScrollSizes();
	_SetScrollPos(m_hwnd, SB_VERT, m_yScrollPos);
	_SetScrollPos(m_hwnd, SB_HORZ, m_xScrollPos);
}

void CImageChild::ZoomIn (int nStep, POINT center)
{
	float tempfZoom = m_fZoom;
	RECT rc;

	GetClientRect(m_hwnd, &rc);
	int cx = rc.right - rc.left;
	int cy = rc.bottom - rc.top;

	if(nStep)
	{
		if(m_fZoom >= 1.0)
		{
			int tempZoom = (int)(m_fZoom + 1.0);
			m_fZoom = tempZoom * 1.0f;
		}
		else
		{
			for(int i = 0; i < ARRAYSIZE(ZoomValueList) - 1; i++)
			{
				if(m_fZoom < ZoomValueList[i] && m_fZoom >= ZoomValueList[i + 1])
				{
					m_fZoom = ZoomValueList[i];
					break;
				}
			}
		}
	}
	else
	{
		m_fZoom += m_fZoomStep;
	}
	
	if(m_fZoom > m_fMaxZoom)
	{
		m_fZoom = m_fMaxZoom;
	}

	UpdateTitleWithZoom();
	_SetScrollSizes();

	if(m_sImage.cx * m_fZoom > cx)
	{
		int rw = cx / m_fZoom * tempfZoom;
		int lx = center.x - rw / 2;
		if(lx < 0)
			lx = 0;
		int rx = center.x + rw / 2;
		if(rx > cx)
			lx -= rx - cx;
		
		int vx = m_sImage.cx * tempfZoom;
		if(vx > cx)
			m_xScrollPos = (int)((m_xScrollPos + lx) * m_fZoom / tempfZoom);
		else
			m_xScrollPos = (int)((lx - (cx - vx) / 2) * m_fZoom / tempfZoom);
		_SetScrollPos(m_hwnd, SB_HORZ, m_xScrollPos);
	}
	else
		m_xScrollPos = 0;
	if(m_sImage.cy * m_fZoom > cy)
	{
		int vh = cy / m_fZoom * tempfZoom;
		int ty = center.y - vh / 2;
		if(ty < 0)
			ty = 0;
		int by = center.y + vh / 2;
		if(by > cy)
			ty -= by - cy;

		int vy = m_sImage.cy * tempfZoom;
		if(vy > cy)
			m_yScrollPos = (int)((m_yScrollPos + ty) * m_fZoom / tempfZoom);
		else
			m_yScrollPos = (int)((ty - (cy - vy) / 2) * m_fZoom / tempfZoom);
		_SetScrollPos(m_hwnd, SB_VERT, m_yScrollPos);
	}
	else
		m_yScrollPos = 0;
	Invalidate(FALSE);
}

void CImageChild::ZoomOut (int nStep, POINT center)
{
	float tempfZoom = m_fZoom;
	RECT rc;

	GetClientRect(m_hwnd, &rc);
	int cx = rc.right - rc.left;
	int cy = rc.bottom - rc.top;

	if(nStep)
	{
		if(m_fZoom >= 2.0)
		{
			int tempZoom = (int)(m_fZoom - 1.0);
			m_fZoom = tempZoom * 1.0f;
		}
		else if(1.0 < m_fZoom && m_fZoom < 2.0)
		{
			m_fZoom = 1.0f;
		}
		else
		{
			for(int i = 0; i < 18; i++)
			{
				if(m_fZoom <= ZoomValueList[i] && m_fZoom > ZoomValueList[i + 1])
				{
					m_fZoom = ZoomValueList[i + 1];
					break;
				}
			}
		}
	}
	else
	{
		m_fZoom -= m_fZoomStep;
	}
	
	if(m_fZoom < m_fMinZoom)
	{
		m_fZoom = m_fMinZoom;
	}

	UpdateTitleWithZoom();
	_SetScrollSizes();

	if(m_sImage.cx * m_fZoom > cx)
	{
		int vx = cx / m_fZoom * tempfZoom;
		int lx = center.x - vx / 2;
		if(lx < 0)
			lx = 0;
		int rx = center.x + vx / 2;
		if(rx > cx)
			lx -= rx - cx;
		m_xScrollPos = (m_xScrollPos + lx) * m_fZoom / tempfZoom;
		_SetScrollPos(m_hwnd, SB_HORZ, m_xScrollPos);
	}
	else
		m_xScrollPos = 0;

	if(m_sImage.cy * m_fZoom > cy)
	{
		int vy = cy / m_fZoom * tempfZoom;
		int ty = center.y - vy / 2;
		if(ty < 0)
			ty = 0;
		int by = center.y + vy / 2;
		if(by > cy)
			ty -= by - cy;
		m_yScrollPos = (m_yScrollPos + ty) * m_fZoom / tempfZoom;
		_SetScrollPos(m_hwnd, SB_VERT, m_yScrollPos);
	}
	else
		m_yScrollPos = 0;
	Invalidate(FALSE);
}

void CImageChild::CheckZoomValue ()
{
	if(m_fZoom > 1.0f)
	{
		int tempZoom = (int)m_fZoom;
		m_fZoom = tempZoom * 1.0f;
	}
	else
	{
		for(int i = 0; i < 18; i++)
		{
			if(m_fZoom < ZoomValueList[i] && m_fZoom >= ZoomValueList[i + 1])
			{
				m_fZoom = ZoomValueList[i + 1];
				break;
			}
		}
	}
}

void CImageChild::SetClientSize (HWND hwnd, int clientWidth, int clientHeight)
{
	// Define the desired client area size
	RECT clientRect = {0, 0, clientWidth, clientHeight};

	// Retrieve the current window style and extended style
	DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);
	DWORD dwExStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

	// Adjust the window size to accommodate the desired client area size
	if(AdjustWindowRectEx(&clientRect, dwStyle, FALSE, dwExStyle))
	{
		int windowWidth = clientRect.right - clientRect.left;
		int windowHeight = clientRect.bottom - clientRect.top;

		// Retrieve the current window position
		RECT windowRect;
		GetWindowRect(hwnd, &windowRect);
		int x = windowRect.left;
		int y = windowRect.top;

		// Set the new window size
		SetWindowPos(hwnd, HWND_TOP, x, y, windowWidth, windowHeight, SWP_NOZORDER);
	}
}

HINSTANCE CImageChild::GetInstance (VOID)
{
	return m_hInstance;
}

HRESULT CImageChild::UpdateTitleWithZoom (VOID)
{
	HRESULT hr;
	WCHAR wzTitle[MAX_PATH + 30];

	Check(Formatting::TPrintF(wzTitle, ARRAYSIZE(wzTitle), NULL, L"%ls [%ls] @ %.1f%%", m_wzFileName, m_fSelectionMode ? L"SELECT" : L"MOVE", m_fZoom * 100.0f));
	SetWindowText(m_hwnd, wzTitle);

Cleanup:
	return hr;
}

int CImageChild::OnSize (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	//if(0 < m_pSIF->GetLayerCount())
	{
		_SetScrollSizes();
		_SetScrollPos(m_hwnd, SB_VERT, m_yScrollPos);
		_SetScrollPos(m_hwnd, SB_HORZ, m_xScrollPos);
		m_bWindowsSizeChanged = TRUE;
	}
	return 0;
}

#define SCROLL_LINE 10
BOOL CImageChild::OnVScroll (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	static int yPos;
	switch (LOWORD(wParam))
	{
	case SB_LINEUP:
		yPos -= SCROLL_LINE;
		break;
	case SB_LINEDOWN:
		yPos += SCROLL_LINE;
		break;
	case SB_PAGEUP:
		yPos -= SCROLL_LINE * 10;
		break;
	case SB_PAGEDOWN:
		yPos += SCROLL_LINE * 10;
		break;
	case SB_THUMBTRACK:
		yPos = HIWORD(wParam);
		break;
	default:
		break;
	}
	_SetScrollPos(m_hwnd, SB_VERT, yPos);
	Invalidate(FALSE);
	return 0;
}

BOOL CImageChild::OnHScroll (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	static int xPos;
	switch (LOWORD(wParam))
	{
	case SB_LINEUP:
		xPos -= SCROLL_LINE;
		break;
	case SB_LINEDOWN:
		xPos += SCROLL_LINE;
		break;
	case SB_PAGEUP:
		xPos -= SCROLL_LINE * 10;
		break;
	case SB_PAGEDOWN:
		xPos += SCROLL_LINE * 10;
		break;
	case SB_THUMBTRACK:
		xPos = HIWORD(wParam);
		break;
	default:
		break;
	}
	_SetScrollPos(m_hwnd, SB_HORZ, xPos);
	Invalidate(FALSE);
	return 0;
}

BOOL CImageChild::OnMouseWheel (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	RECT rc;
	GetClientRect(m_hwnd, &rc);
	int cx = rc.right - rc.left;
	int cy = rc.bottom - rc.top;
	int delta = GET_WHEEL_DELTA_WPARAM(wParam);

	if(cx > m_sImage.cx * m_fZoom && cy > m_sImage.cy * m_fZoom)
	{
		POINT center;
		center.x = cx / 2;
		center.y = cy / 2;
		if(delta > 0)
			ZoomOut(0, center);
		else
			ZoomIn(0, center);
		return TRUE;
	}

	if(GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL)
	{
		m_xScrollPos -= delta / 10;
		_SetScrollPos(m_hwnd, SB_HORZ, m_xScrollPos);
		Invalidate(FALSE);
	}
	else
	{
		m_yScrollPos -= delta / 10;
		_SetScrollPos(m_hwnd, SB_VERT, m_yScrollPos);
		Invalidate(FALSE);
	}
	return TRUE;
}

BOOL CImageChild::OnKeyDown (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	RECT rc;
	GetClientRect(m_hwnd, &rc);
	int cx = rc.right - rc.left;
	int cy = rc.bottom - rc.top;
	POINT center;
	center.x = cx / 2;
	center.y = cy / 2;

	int key = static_cast<int>(wParam);

	if(wParam == VK_MENU)
	{
		m_bIsAltPressed = TRUE;
		UpdateCursor();
	}
	else if(wParam == 'M')
	{
		m_fSelectionMode = !m_fSelectionMode;
		UpdateTitleWithZoom();
	}

	if(!m_fSelectionMode)
		return FALSE;

	bool ctrlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
	if (ctrlPressed)
	{
		m_bIsCtrlPressed = TRUE;
		UpdateCursor();
		if (key == VK_OEM_MINUS)
			ZoomOut(1, center);
		else if (key == VK_OEM_PLUS)
			ZoomIn(1, center);
	}

	return FALSE;
}

BOOL CImageChild::OnKeyUp (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	if(!m_fSelectionMode)
		return FALSE;
	if (wParam == VK_MENU) // VK_MENU is the virtual-key code for the Alt key
	{
		m_bIsAltPressed = FALSE;
		UpdateCursor();
	}
	if(wParam == VK_CONTROL)
	{
		m_bIsCtrlPressed = FALSE;
		UpdateCursor();
	}
	return FALSE;
}

BOOL CImageChild::OnSysKeyDown (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	if(!m_fSelectionMode)
		return FALSE;
	if(wParam == VK_MENU)
	{
		m_bIsAltPressed = TRUE;
		UpdateCursor();
	}
	return FALSE;
}

BOOL CImageChild::OnSysKeyUp (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	if(!m_fSelectionMode)
		return FALSE;
	if(wParam == VK_MENU) // VK_MENU is the virtual-key code for the Alt key
	{
		m_bIsAltPressed = FALSE;
		UpdateCursor();
	}
	return FALSE;
}

BOOL CImageChild::OnLButtonDown (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	if(m_bLButtonClicked)
		return FALSE;

	RECT rc;
	GetClientRect(m_hwnd, &rc);
	int cx = rc.right - rc.left;
	int cy = rc.bottom - rc.top;
	POINT center;
	center.x = LOWORD(lParam);
	center.y = HIWORD(lParam);

	if(m_bIsCtrlPressed && m_fSelectionMode)
	{
		ZoomIn(1, center);
		return TRUE;
	}

	if(m_bIsAltPressed && m_fSelectionMode)
	{
		ZoomOut(1, center);
		return TRUE;
	}

	m_xStartDrag = m_xCurrDrag = LOWORD(lParam); // Horizontal position of cursor
	m_yStartDrag = m_yCurrDrag = HIWORD(lParam); // Vertical position of cursor
	if(m_xStartDrag >= rc.right - 20 || m_yStartDrag >= rc.bottom - 20)
		return FALSE;

	m_bLButtonClicked = TRUE;

	m_nSelectedLayerIndex = -1;
	INT xDest = 0, yDest = 0;
	int newW = (INT)(m_sImage.cx * m_fZoom);
	int newH = (INT)(m_sImage.cy * m_fZoom);

	if(newW < cx)
		xDest = (cx - newW) / 2;

	if(newH < cy)
		yDest = (cy - newH) / 2;

	for(DWORD i = m_pSIF->GetLayerCount(); i > 0; i--)
	{
		TStackRef<ISimbeyInterchangeFileLayer> srLayer;
		RECT rcLayer;

		SideAssertHr(m_pSIF->GetLayerByIndex(i - 1, &srLayer));
		SideAssertHr(srLayer->GetPosition(&rcLayer));

		if(xDest + rcLayer.left * m_fZoom - m_xScrollPos <= m_xCurrDrag 
			&& m_xCurrDrag <= xDest + (rcLayer.right) * m_fZoom - m_xScrollPos
			&& yDest + rcLayer.top * m_fZoom - m_yScrollPos <= m_yCurrDrag 
			&& m_yCurrDrag <= yDest + (rcLayer.bottom) * m_fZoom - m_yScrollPos)
		{
			m_xDragLayer = rcLayer.left;
			m_yDragLayer = rcLayer.top;

			m_xOriginalDrag = m_xStartDrag;
			m_yOriginalDrag = m_yStartDrag;

			m_nSelectedLayerIndex = i - 1;
			break;
		}
	}
	return TRUE;
}

BOOL CImageChild::OnMouseMove (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	if(!m_bLButtonClicked)
		return FALSE;
	m_xCurrDrag = LOWORD(lParam); // Horizontal position of cursor
	m_yCurrDrag = HIWORD(lParam); // Vertical position of cursor
	if(!m_fSelectionMode && m_nSelectedLayerIndex >= 0)
	{
		TStackRef<ISimbeyInterchangeFileLayer> srLayer;

		SideAssertHr(m_pSIF->GetLayerByIndex(m_nSelectedLayerIndex, &srLayer));

		FLOAT rx = (FLOAT)(m_xCurrDrag - m_xOriginalDrag);
		FLOAT ry = (FLOAT)(m_yCurrDrag - m_yOriginalDrag);
		srLayer->SetPosition(m_xDragLayer + (LONG)(rx / m_fZoom), m_yDragLayer + (LONG)(ry / m_fZoom));

		m_xStartDrag = m_xCurrDrag;
		m_yStartDrag = m_yCurrDrag;
	}
	Invalidate(FALSE);
	return TRUE;
}

BOOL CImageChild::OnLButtonUp (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	if(!m_bLButtonClicked)
		return FALSE;

	m_bLButtonClicked = FALSE;
	m_xCurrDrag = LOWORD(lParam); // Horizontal position of cursor
	m_yCurrDrag = HIWORD(lParam); // Vertical position of cursor
	if(m_fSelectionMode)
	{
		int nRectWidth = abs(m_xCurrDrag - m_xStartDrag);
		int nRectHeight = abs(m_yCurrDrag - m_yStartDrag);

		if(nRectWidth >= 10 && nRectHeight >= 10)
			ZoomToRectangle();
	}	
	else
	{
		RECT rc;
		GetClientRect(m_hwnd, &rc);
		int cx = rc.right - rc.left;
		int cy = rc.bottom - rc.top;
		m_nSelectedLayerIndex = -1;
		INT xDest = 0, yDest = 0;
		int newW = (INT)(m_sImage.cx * m_fZoom);
		int newH = (INT)(m_sImage.cy * m_fZoom);

		if(newW < cx)
			xDest = (cx - newW) / 2;

		if(newH < cy)
			yDest = (cy - newH) / 2;

		for(DWORD i = m_pSIF->GetLayerCount(); i >= 0; i--)
		{
			TStackRef<ISimbeyInterchangeFileLayer> srLayer;
			RECT rcLayer;
			SIZE sLayer;

			SideAssertHr(m_pSIF->GetLayerByIndex(i - 1, &srLayer));
			SideAssertHr(srLayer->GetPosition(&rcLayer));

			if(xDest + rcLayer.left * m_fZoom - m_xScrollPos <= m_xCurrDrag 
				&& m_xCurrDrag <= xDest + (rcLayer.right) * m_fZoom - m_xScrollPos
				&& yDest + rcLayer.top * m_fZoom - m_yScrollPos <= m_yCurrDrag 
				&& m_yCurrDrag <= yDest + (rcLayer.bottom) * m_fZoom - m_yScrollPos)
			{
				m_nSelectedLayerIndex = i - 1;
				break;
			}
		}
	}
	Invalidate(FALSE);
	return TRUE;
}

BOOL CImageChild::OnPaint (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(m_hwnd, &ps);

	RECT rc;
	GetClientRect(m_hwnd, &rc);
	INT cx = rc.right - rc.left;
	INT cy = rc.bottom - rc.top;

	if(m_bWindowsSizeChanged)
	{
		m_xDIB = cx;
		m_yDIB = cy;
		if(m_hDIB != NULL)
			DeleteObject(m_hDIB);
		sifCreateBlankDIB(hdc, cx, cy, 24, reinterpret_cast<PVOID*>(&m_pDIB), &m_hDIB);
		m_bWindowsSizeChanged = FALSE;
	}

	HDC hdcDIB = CreateCompatibleDC(hdc);
	HBITMAP hbmPrev = (HBITMAP)SelectObject(hdcDIB, m_hDIB);

	INT xDest = 0, yDest = 0;
	int newW = (INT)(m_sImage.cx * m_fZoom);
	int newH = (INT)(m_sImage.cy * m_fZoom);

	if(newW < cx)
		xDest = (cx - newW) / 2;

	if(newH < cy)
		yDest = (cy - newH) / 2;

	if(newW < cx || newH < cy)
	{
		RECT rc = { 0, 0, cx, cy };
		HBRUSH hBrush = CreateSolidBrush(RGB(192, 192, 192));
		FillRect(hdcDIB, &rc, hBrush);
		DeleteObject(hBrush);
	}

	{
		RECT rc = { xDest, yDest, xDest + newW, yDest + newH };
		FillRect(hdcDIB, &rc, m_hbrTransparent);
	}

	for(DWORD i = 0; i < m_pSIF->GetLayerCount(); i++)
	{
		TStackRef<ISimbeyInterchangeFileLayer> srLayer;
		SIZE sLayer;
		RECT rcLayer;
		PBYTE pRGBA;
		DWORD cbBits;

		SideAssertHr(m_pSIF->GetLayerByIndex(i, &srLayer));
		srLayer->GetSize(&sLayer);
		srLayer->GetPosition(&rcLayer);

		if(SUCCEEDED(srLayer->GetBitsPtr(&pRGBA, &cbBits)))
			CopyBits(pRGBA, sLayer.cx, sLayer.cy, rcLayer.left, rcLayer.top, m_pDIB, m_xDIB, m_yDIB, xDest, yDest, m_xScrollPos, m_yScrollPos, m_fZoom, newW, newH);

		if(i == m_nSelectedLayerIndex)
		{
			SetBkMode(hdcDIB, TRANSPARENT);
			HBRUSH hNullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
			HBRUSH hOldBrush = (HBRUSH)SelectObject(hdcDIB, hNullBrush);

			HPEN hPen = CreatePen(PS_DASH, 1, RGB(100, 100, 100));
			HPEN hOldPen = (HPEN)SelectObject(hdcDIB, hPen);

			Rectangle(hdcDIB, 
				xDest + rcLayer.left * m_fZoom - m_xScrollPos - 2, 
				yDest + rcLayer.top * m_fZoom - m_yScrollPos - 2, 
				xDest + rcLayer.right * m_fZoom - m_xScrollPos + 2, 
				yDest + rcLayer.bottom * m_fZoom - m_yScrollPos + 2);

			SelectObject(hdcDIB, hOldPen);
			SelectObject(hdcDIB, hOldBrush);
			DeleteObject(hPen);
		}
	}

	if(m_bLButtonClicked && m_fSelectionMode)
	{
		SetBkMode(hdcDIB, TRANSPARENT);
		HBRUSH hNullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
		HBRUSH hOldBrush = (HBRUSH)SelectObject(hdcDIB, hNullBrush);

		HPEN hPen = CreatePen(PS_DASH, 1, RGB(100, 100, 100));
		HPEN hOldPen = (HPEN)SelectObject(hdcDIB, hPen);

		Rectangle(hdcDIB, m_xStartDrag, m_yStartDrag, m_xCurrDrag, m_yCurrDrag);
		SelectObject(hdcDIB, hOldPen);
		SelectObject(hdcDIB, hOldBrush);
		DeleteObject(hPen);
	}

	BitBlt(hdc, 0, 0, cx, cy, hdcDIB, 0, 0, SRCCOPY);
	if(hdcDIB)
	{
		SelectObject(hdcDIB, hbmPrev);
		DeleteDC(hdcDIB);
	}
	EndPaint(m_hwnd, &ps);
	return TRUE;
}

CMDIWindow::CMDIWindow (HINSTANCE hInstance) :
	m_hInstance(hInstance)
{
}

CMDIWindow::~CMDIWindow ()
{
}

HRESULT CMDIWindow::Register (HINSTANCE hInstance)
{
	WNDCLASSEX wnd = {0};

	wnd.cbSize = sizeof(WNDCLASSEX);
	wnd.style = 0;
	wnd.cbClsExtra = 0;
	wnd.cbWndExtra = 0;
	wnd.hInstance = hInstance;
	wnd.hIcon = LoadIcon(hInstance, MAKEINTRESOURCEW(IDI_MAIN));
	wnd.hCursor = LoadCursor(NULL, IDC_ARROW);
	wnd.hbrBackground = NULL;
	wnd.lpszMenuName = MAKEINTRESOURCEW(IDR_MENU);
	wnd.lpszClassName = c_wzMDIClass;

	return RegisterMDIFrame(&wnd, NULL);
}

HRESULT CMDIWindow::Unregister (HINSTANCE hInstance)
{
	return UnregisterClass(c_wzMDIClass, hInstance);
}

HRESULT CMDIWindow::Initialize (INT nWidth, INT nHeight, INT nCmdShow)
{
	return Create(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE, WS_OVERLAPPEDWINDOW, c_wzMDIClass, c_wzMDITitle, CW_USEDEFAULT, CW_USEDEFAULT, nWidth, nHeight, NULL, nCmdShow);
}

HINSTANCE CMDIWindow::GetInstance (VOID)
{
	return m_hInstance;
}

VOID CMDIWindow::OnFinalDestroy (HWND hwnd)
{
	PostQuitMessage(0);
	__super::OnFinalDestroy(hwnd);
}

HRESULT CMDIWindow::FinalizeAndShow (DWORD dwStyle, INT nCmdShow)
{
	return __super::FinalizeAndShow(dwStyle, nCmdShow);
}

BOOL CMDIWindow::GetMDIMenuData (__out HMENU* phWindowMenu, __out UINT* pidFirstChild)
{
	if(FindSubMenu(::GetMenu(m_hwnd), L"WINDOW", phWindowMenu))
	{
		*pidFirstChild = 5000;
		return TRUE;
	}
	return FALSE;
}

HRESULT CMDIWindow::OpenImageWindow (VOID)
{
	HRESULT hr;
	TStackRef<CImageChild> srChild;
	CDialogHost dlgHost(m_hInstance);
	CNewImageDlg dlgNewImage;

	Check(dlgHost.Display(m_hwnd, &dlgNewImage));
	CheckIfIgnore(IDOK != dlgHost.GetReturnValue(), E_ABORT);

	srChild.Attach(__new CImageChild(m_hInstance));
	CheckAlloc(srChild);
	Check(srChild->Initialize(this, dlgNewImage.m_size.cx, dlgNewImage.m_size.cy));

	if(dlgNewImage.m_cr != 0)
		Check(srChild->AddSolidColorLayer(dlgNewImage.m_cr, dlgNewImage.m_size));

	// Add multiple image files to the SIF (we'll draw the layers from the SIF)
	for(;;)
	{
		CChooseFile dlgChooseImage;

		Check(dlgChooseImage.Initialize());
		if(!dlgChooseImage.OpenSingleFile(m_hwnd, L"Add Image", L"Image File (*.png, *.bmp, *.jpg)\0*.png;*.bmp;*.jpg\0"))
			break;

		Check(srChild->AddLayer(dlgChooseImage.GetFile(0)));
	}

Cleanup:
	if(FAILED(hr) && srChild)
		srChild->Destroy();
	return hr;
}

BOOL CMDIWindow::OnCreate (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return FALSE;
}

BOOL CMDIWindow::OnPaint (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(m_hwnd, &ps);

	FillRect(hdc, &ps.rcPaint, (HBRUSH)GetStockObject(WHITE_BRUSH));

	EndPaint(m_hwnd, &ps);
	return FALSE;
}

BOOL CMDIWindow::OnEraseBackground (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	lResult = TRUE;
	return TRUE;
}

BOOL CMDIWindow::OnCommand (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	BOOL fHandled = FALSE;

	switch(LOWORD(wParam))
	{
	case ID_FILE_EXIT:
		PostMessage(m_hwnd, WM_CLOSE, 0, 0);
		lResult = 0;
		break;
	case ID_WINDOW_NEWPIE:
		{
			TStackRef<CMDIChild> srChild;
			srChild.Attach(__new CMDIChild(m_hInstance, 0));
			if(srChild)
				srChild->Initialize(this, 400, 350);
			fHandled = TRUE;
		}
		break;
	case ID_WINDOW_NEWSQUARE:
		{
			TStackRef<CMDIChild> srChild;
			srChild.Attach(__new CMDIChild(m_hInstance, 1));
			if(srChild)
				srChild->Initialize(this, 400, 350);
			fHandled = TRUE;
		}
		break;
	case ID_WINDOW_NEWIMAGE:
		OpenImageWindow();
		fHandled = TRUE;
		break;
	}

	return fHandled;
}

BOOL CMDIWindow::OnClose (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return FALSE;
}
