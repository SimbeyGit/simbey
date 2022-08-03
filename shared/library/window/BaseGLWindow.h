#pragma once

#include "BaseWindow.h"

class CBaseGLWindow : public CBaseWindow
{
protected:
	BOOL m_fFullScreen;
	INT m_cBitsPerPixel;
	INT m_nZBufferSize;

	DOUBLE m_dblVerticalFOV;
	DOUBLE m_dblAspect;
	DOUBLE m_rMinVisualDepth;
	DOUBLE m_rMaxVisualDepth;

	HDC m_hdc;
	HGLRC m_hrc;

public:
	CBaseGLWindow ();
	virtual ~CBaseGLWindow ();

	VOID GetPerspective (__out DOUBLE& dblVerticalFOV, __out DOUBLE& dblAspect, __out DOUBLE& dblMinVisualDepth, __out DOUBLE& dblMaxVisualDepth);
	VOID SetPerspective (DOUBLE dblVerticalFOV, DOUBLE dblMinVisualDepth, DOUBLE dblMaxVisualDepth);

	VOID Redraw (VOID);

protected:
	HRESULT CreateGL (DWORD dwStyle, BOOL fFullScreen, LPCTSTR lpcszClass, LPCTSTR lpcszTitle, INT x, INT y, INT nWidth, INT nHeight, HWND hwndParent, INT nCmdShow);
	HRESULT SetFullScreen (BOOL fFullScreen, INT nWidth, INT nHeight);

	// Subclasses must implement these methods.
	virtual HRESULT CreateTextures (VOID) = 0;
	virtual VOID Draw (VOID) = 0;
	virtual VOID RequestModeChange (BOOL fFullScreen) = 0;

	// CBaseWindow behaviors.
	virtual VOID OnFinalDestroy (HWND hwnd);
	virtual HRESULT FinalizeAndShow (DWORD dwStyle, INT nCmdShow);
	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

	static VOID Unproject (LONG cx, LONG cy, DOUBLE& wx, DOUBLE& wy, DOUBLE& wz);

	virtual HRESULT ConfigureOpenGL (VOID);

private:
	HRESULT SetupRenderingPixelFormat (VOID);
	VOID ReSizeViewPort (INT nWidth, INT nHeight);
};