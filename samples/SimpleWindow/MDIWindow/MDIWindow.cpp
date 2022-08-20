#include <windows.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Core\MemoryStream.h"
#include "Library\Util\StreamHelpers.h"
#include "Library\Util\Formatting.h"
#include "Library\Window\DialogHost.h"
#include "Library\ChooseFile.h"
#include "MDIWindow.h"

const WCHAR c_wzMDIClass[] = L"MDIWindowCls";
const WCHAR c_wzMDITitle[] = L"MDI Window";

const WCHAR c_wzMDIChild[] = L"MDIChildCls";
const WCHAR c_wzImageChild[] = L"ImageChildCls";

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
	m_pSIF(NULL)
{
}

CImageChild::~CImageChild ()
{
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

	Check(sifCreateNew(&m_pSIF));
	Check(AttachToFrame(c_wzImageChild, L"Image",
		WS_CHILD | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, nWidth, nHeight, pFrame));

Cleanup:
	return hr;
}

HRESULT CImageChild::AddLayer (PCWSTR pcwzImageFile)
{
	HRESULT hr;

	Check(sifAddImageFileAsLayer(pcwzImageFile, m_pSIF, NULL));
	InvalidateRect(m_hwnd, NULL, FALSE);

Cleanup:
	return hr;
}

HINSTANCE CImageChild::GetInstance (VOID)
{
	return m_hInstance;
}

BOOL CImageChild::OnPaint (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(m_hwnd, &ps);
	RECT rc;
	HBITMAP hDIB;
	SIF_SURFACE sifSurface;

	GetClientRect(m_hwnd, &rc);
	sifSurface.xSize = rc.right - rc.left;
	sifSurface.ySize = rc.bottom - rc.top;
	sifSurface.cBitsPerPixel = 24;
	sifSurface.lPitch = ((sifSurface.xSize * 3) + 3) & ~3;

	if(SUCCEEDED(sifCreateBlankDIB(hdc, sifSurface.xSize, sifSurface.ySize, sifSurface.cBitsPerPixel, reinterpret_cast<PVOID*>(&sifSurface.pbSurface), &hDIB)))
	{
		HDC hdcDIB = CreateCompatibleDC(hdc);
		HBITMAP hbmPrev = (HBITMAP)SelectObject(hdcDIB, hDIB);
		TStackRef<ISimbeyInterchangeFileLayer> srLayer;

		if(SUCCEEDED(m_pSIF->GetLayerByIndex(0, &srLayer)))
		{
			INT xDest, yDest;
			RECT rcLayer;
			SIZE sLayer;

			srLayer->GetPosition(&rcLayer);
			sLayer.cx = rcLayer.right - rcLayer.left;
			sLayer.cy = rcLayer.bottom - rcLayer.top;

			if(sLayer.cx < sifSurface.xSize)
				xDest = sifSurface.xSize / 2 - sLayer.cx / 2;
			else
				xDest = 0;

			if(sLayer.cy < sifSurface.ySize)
				yDest = sifSurface.ySize / 2 - sLayer.cy / 2;
			else
				yDest = 0;

			srLayer->DrawToDIB24(&sifSurface, xDest, yDest);

			BitBlt(hdc, 0, 0, sifSurface.xSize, sifSurface.ySize, hdcDIB, 0, 0, SRCCOPY);
		}

		if(hdcDIB)
		{
			SelectObject(hdcDIB, hbmPrev);
			DeleteDC(hdcDIB);
		}

		DeleteObject(hDIB);
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
	CChooseFile dlgChooseImage;

	Check(dlgChooseImage.Initialize());
	CheckIfIgnore(!dlgChooseImage.OpenSingleFile(m_hwnd, L"Open Image", L"Image File (*.png, *.bmp, *.jpg)\0*.png;*.bmp;*.jpg\0"), E_ABORT);

	srChild.Attach(__new CImageChild(m_hInstance));
	CheckAlloc(srChild);
	Check(srChild->Initialize(this, 400, 350));
	Check(srChild->AddLayer(dlgChooseImage.GetFile(0)));

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
