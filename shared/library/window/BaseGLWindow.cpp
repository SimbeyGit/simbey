#include <windows.h>
#include <gl\gl.h>
#include <gl\glu.h>
#include "Library\Core\Check.h"
#include "BaseGLWindow.h"

CBaseGLWindow::CBaseGLWindow ()
{
	m_fFullScreen = FALSE;
	m_cBitsPerPixel = 32;
	m_nZBufferSize = 16;

	m_dblVerticalFOV = 45.0;
	m_dblAspect = 0.0;
	m_rMinVisualDepth = 0.1;
	m_rMaxVisualDepth = 1000.0;

	m_hdc = NULL;
	m_hrc = NULL;
}

CBaseGLWindow::~CBaseGLWindow ()
{
	Assert(NULL == m_hrc);
	Assert(NULL == m_hdc);
}

VOID CBaseGLWindow::GetPerspective (__out DOUBLE& dblVerticalFOV, __out DOUBLE& dblAspect, __out DOUBLE& dblMinVisualDepth, __out DOUBLE& dblMaxVisualDepth)
{
	dblVerticalFOV = m_dblVerticalFOV;
	dblAspect = m_dblAspect;
	dblMinVisualDepth = m_rMinVisualDepth;
	dblMaxVisualDepth = m_rMaxVisualDepth;
}

VOID CBaseGLWindow::SetPerspective (DOUBLE dblVerticalFOV, DOUBLE dblMinVisualDepth, DOUBLE dblMaxVisualDepth)
{
	m_dblVerticalFOV = dblVerticalFOV;
	m_rMinVisualDepth = dblMinVisualDepth;
	m_rMaxVisualDepth = dblMaxVisualDepth;

	if(m_hrc)
	{
		RECT rc;
		GetClientRect(m_hwnd, &rc);
		ReSizeViewPort(rc.right - rc.left, rc.bottom - rc.top);
	}
}

VOID CBaseGLWindow::Redraw (VOID)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();
	Draw();
	SwapBuffers(m_hdc);
}

HRESULT CBaseGLWindow::CreateGL (DWORD dwStyle, BOOL fFullScreen, LPCTSTR lpcszClass, LPCTSTR lpcszTitle, INT x, INT y, INT nWidth, INT nHeight, HWND hwndParent, INT nCmdShow)
{
	HRESULT hr = E_UNEXPECTED;
	if(NULL == m_hwnd)
	{
		DWORD dwExStyle = 0;

		hr = S_OK;

		if(fFullScreen)
		{
			DEVMODE dmScreenSettings = {0};

			dmScreenSettings.dmSize = sizeof(dmScreenSettings);
			dmScreenSettings.dmPelsWidth = nWidth;
			dmScreenSettings.dmPelsHeight = nHeight;
			dmScreenSettings.dmBitsPerPel = m_cBitsPerPixel;
			dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

			if(0 != (WS_CHILD & dwStyle))
				hr = E_INVALIDARG;
			else if(DISP_CHANGE_SUCCESSFUL != ChangeDisplaySettings(&dmScreenSettings,CDS_FULLSCREEN))
				hr = E_FAIL;
			else
			{
				x = 0;
				y = 0;
				dwExStyle = WS_EX_APPWINDOW;
				dwStyle = WS_POPUP;
				ShowCursor(FALSE);
			}
		}
		else if(0 == (WS_CHILD & dwStyle))
		{
			dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
			dwStyle = WS_OVERLAPPEDWINDOW;
		}

		if(SUCCEEDED(hr) && !fFullScreen)
			hr = CBaseWindow::AdjustWindowSize(dwStyle, dwExStyle, FALSE, x, y, nWidth, nHeight);

		if(SUCCEEDED(hr))
		{
			dwStyle |= (WS_CLIPSIBLINGS | WS_CLIPCHILDREN);

			hr = Create(dwExStyle, dwStyle, lpcszClass, lpcszTitle, x, y, nWidth, nHeight, hwndParent, nCmdShow);
		}

		if(SUCCEEDED(hr))
			hr = CreateTextures();

		if(SUCCEEDED(hr))
			m_fFullScreen = fFullScreen;
	}
	return hr;
}

HRESULT CBaseGLWindow::SetFullScreen (BOOL fFullScreen, INT nWidth, INT nHeight)
{
	HRESULT hr;
	DWORD dwStyle;
	TCHAR tzClass[256], tzTitle[256];
	INT x = 0, y = 0;

	CheckIf(fFullScreen == m_fFullScreen, S_FALSE);
	CheckIf(NULL == m_hwnd, E_FAIL);

	dwStyle = GetWindowLong(m_hwnd, GWL_STYLE);
	CheckIf(0 != (WS_CHILD & dwStyle), E_FAIL);

	CheckIfGetLastError(0 == GetClassName(m_hwnd, tzClass, ARRAYSIZE(tzClass)));
	CheckIfGetLastError(0 == GetWindowText(m_hwnd, tzTitle, ARRAYSIZE(tzTitle)));

	Check(Destroy());

	if(!fFullScreen)
	{
		INT xScreen = GetSystemMetrics(SM_CXFULLSCREEN);
		INT yScreen = GetSystemMetrics(SM_CYFULLSCREEN);

		CheckIf(nWidth > xScreen || nHeight > yScreen, E_INVALIDARG);

		x = xScreen / 2 - nWidth / 2;
		y = yScreen / 2 - nHeight / 2;
	}

	Check(CreateGL(dwStyle, fFullScreen, tzClass, tzTitle, x, y, nWidth, nHeight, NULL, SW_SHOW));

	Assert(fFullScreen == m_fFullScreen);

Cleanup:
	return hr;
}

VOID CBaseGLWindow::OnFinalDestroy (HWND hwnd)
{
	if(m_fFullScreen)
	{
		ChangeDisplaySettings(NULL, 0);
		ShowCursor(TRUE);
		m_fFullScreen = FALSE;
	}

	if(m_hrc)
	{
		wglMakeCurrent(NULL, NULL);
		wglDeleteContext(m_hrc);
		m_hrc = NULL;
	}

	if(m_hdc)
	{
		ReleaseDC(hwnd, m_hdc);
		m_hdc = NULL;
	}
}

HRESULT CBaseGLWindow::FinalizeAndShow (DWORD dwStyle, INT nCmdShow)
{
	::ShowWindow(m_hwnd, nCmdShow);

	if(m_fFullScreen)
	{
		SetForegroundWindow(m_hwnd);
		SetFocus(m_hwnd);
	}

	return ConfigureOpenGL();
}

BOOL CBaseGLWindow::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	BOOL fHandled = FALSE;

	switch(message)
	{
	case WM_CREATE:
		if(FAILED(SetupRenderingPixelFormat()))
		{
			lResult = -1;
			fHandled = TRUE;
		}
		break;
	case WM_SIZE:
		ReSizeViewPort(LOWORD(lParam), HIWORD(lParam));
		break;
	case WM_PAINT:
		if(ValidateRect(m_hwnd, NULL))
		{
			Redraw();
			fHandled = TRUE;
		}
		break;
	case WM_SYSKEYDOWN:
		if(wParam == VK_RETURN && 0 == (WS_CHILD & GetWindowLong(m_hwnd, GWL_STYLE)))
		{
			RequestModeChange(!m_fFullScreen);
			fHandled = TRUE;
		}
		break;
	}

	if(!fHandled)
		fHandled = __super::DefWindowProc(message, wParam, lParam, lResult);

	return fHandled;
}

VOID CBaseGLWindow::Unproject (LONG cx, LONG cy, DOUBLE& wx, DOUBLE& wy, DOUBLE& wz)
{
	GLdouble model[4*4];
	GLdouble proj[4*4];
	GLint view[4];
	FLOAT cz;

	glGetDoublev(GL_MODELVIEW_MATRIX, model);
	glGetDoublev(GL_PROJECTION_MATRIX, proj);
	glGetIntegerv(GL_VIEWPORT, view);

	cy = view[3] - cy;

	glReadPixels(cx, cy, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT /* GL_DOUBLE is not supported by OpenGL for this parameter */, &cz);

	gluUnProject((GLdouble)cx, (GLdouble)cy, (GLdouble)cz,
		model, proj, view,
		&wx, &wy, &wz);
}

HRESULT CBaseGLWindow::ConfigureOpenGL (VOID)
{
	glEnable(GL_TEXTURE_2D);
	glShadeModel(GL_SMOOTH);
	glClearColor(0.2f, 0.2f, 0.2f, 0.5f);
	glClearDepth(1.0f);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glDepthFunc(GL_LEQUAL);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glBindTexture(GL_TEXTURE_2D,0);
	return S_OK;
}

HRESULT CBaseGLWindow::SetupRenderingPixelFormat (VOID)
{
	HRESULT hr;
	PIXELFORMATDESCRIPTOR pfd =
	{
		sizeof(PIXELFORMATDESCRIPTOR),				// Size Of This Pixel Format Descriptor
		1,											// Version Number
		PFD_DRAW_TO_WINDOW |						// Format Must Support Window
		PFD_SUPPORT_OPENGL |						// Format Must Support OpenGL
		PFD_DOUBLEBUFFER,							// Must Support Double Buffering
		PFD_TYPE_RGBA,								// Request An RGBA Format
		m_cBitsPerPixel,							// Select Our Color Depth
		0, 0, 0, 0, 0, 0,							// Color Bits Ignored
		0,											// No Alpha Buffer
		0,											// Shift Bit Ignored
		0,											// No Accumulation Buffer
		0, 0, 0, 0,									// Accumulation Bits Ignored
		m_nZBufferSize,								// Z-Buffer (Depth Buffer) size
		0,											// No Stencil Buffer
		0,											// No Auxiliary Buffer
		PFD_MAIN_PLANE,								// Main Drawing Layer
		0,											// Reserved
		0, 0, 0										// Layer Masks Ignored
	};
	UINT PixelFormat;

	m_hdc = GetDC(m_hwnd);
	CheckIfGetLastError(NULL == m_hdc);

	PixelFormat = ChoosePixelFormat(m_hdc, &pfd);
	CheckIfGetLastError(0 == PixelFormat);

	CheckIfGetLastError(!SetPixelFormat(m_hdc, PixelFormat, &pfd));

	m_hrc = wglCreateContext(m_hdc);
	CheckIfGetLastError(NULL == m_hrc);

	__try
	{
		CheckIfGetLastError(!wglMakeCurrent(m_hdc, m_hrc));
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		// Why does wglMakeCurrent() fail this way?
		Check(HRESULT_FROM_WIN32(ERROR_FAILED_DRIVER_ENTRY));
	}

	hr = S_OK;

Cleanup:
	return hr;
}

VOID CBaseGLWindow::ReSizeViewPort (INT nWidth, INT nHeight)
{
	if(nHeight == 0)
		nHeight = 1;

	m_dblAspect = (DOUBLE)nWidth / (DOUBLE)nHeight;

	glViewport(0, 0, nWidth, nHeight);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(m_dblVerticalFOV, m_dblAspect, m_rMinVisualDepth, m_rMaxVisualDepth);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}
