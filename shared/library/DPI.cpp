#include <windows.h>
#include "Core\CoreDefs.h"
#include "DPI.h"

namespace DPI
{
	LONG xDPI = 0;
	LONG yDPI = 0;
	FLOAT xScale = 0.0;
	FLOAT yScale = 0.0;

	typedef enum _PROCESS_DPI_AWARENESS
	{
		PROCESS_DPI_UNAWARE            = 0,
		PROCESS_SYSTEM_DPI_AWARE       = 1,
		PROCESS_PER_MONITOR_DPI_AWARE  = 2
	} PROCESS_DPI_AWARENESS;

	HRESULT SetDPIAware (VOID)
	{
		HRESULT hr;
		HMODULE hModule = LoadLibrary(TEXT("SHCore.DLL"));

		if(hModule)
		{
			HRESULT (WINAPI *pfnSetProcessDpiAwareness)(PROCESS_DPI_AWARENESS);
			Check(TGetFunction(hModule, "SetProcessDpiAwareness", &pfnSetProcessDpiAwareness));
			hr = pfnSetProcessDpiAwareness(PROCESS_SYSTEM_DPI_AWARE);

			if(FAILED(hr))
			{
				FreeLibrary(hModule);
				hModule = NULL;
			}
		}
		else
			hr = S_FALSE;

		if(S_OK != hr)
		{
			BOOL (WINAPI* pfnSetProcessDPIAware)();

			hModule = LoadLibrary(TEXT("USER32.DLL"));

			CheckIfGetLastError(NULL == hModule);
			if(SUCCEEDED(TGetFunction(hModule, "SetProcessDPIAware", &pfnSetProcessDPIAware)))
			{
				CheckIfGetLastError(!pfnSetProcessDPIAware());
			}

			hr = S_OK;
		}

	Cleanup:
		if(hModule)
			FreeLibrary(hModule);
		return hr;
	}

	BOOL Initialize (VOID)
	{
		BOOL fSuccess = FALSE;
		HWND hwndDesktop = ::GetDesktopWindow();
		HDC hdc = ::GetDC(hwndDesktop);
		if(hdc)
		{
			xDPI = GetDeviceCaps(hdc, LOGPIXELSX);
			yDPI = GetDeviceCaps(hdc, LOGPIXELSY);
			xScale = (FLOAT)xDPI / 96.0f;
			yScale = (FLOAT)yDPI / 96.0f;
			::ReleaseDC(hwndDesktop, hdc);
			fSuccess = TRUE;
		}
		return fSuccess;
	}

	VOID SetDPI (LONG xCustomDPI, LONG yCustomDPI)
	{
		xDPI = xCustomDPI;
		yDPI = yCustomDPI;
		xScale = (FLOAT)xDPI / 96.0f;
		yScale = (FLOAT)yDPI / 96.0f;
	}

	VOID NormalizeScaledPoint (LPPOINT lpPoint)
	{
		lpPoint->x = lpPoint->x * 96 / xDPI;
		lpPoint->y = lpPoint->y * 96 / yDPI;
	}

	VOID ScaleNormalizedPoint (LPPOINT lpPoint)
	{
		lpPoint->x = lpPoint->x * xDPI / 96;
		lpPoint->y = lpPoint->y * yDPI / 96;
	}

	VOID NormalizeScaledRectSize (LPRECT lpRect)
	{
		lpRect->right = lpRect->left + (lpRect->right - lpRect->left) * 96 / xDPI;
		lpRect->bottom = lpRect->top + (lpRect->bottom - lpRect->top) * 96 / yDPI;
	}

	VOID ScaleNormalizedRectSize (LPRECT lpRect)
	{
		lpRect->right = lpRect->left + (lpRect->right - lpRect->left) * xDPI / 96;
		lpRect->bottom = lpRect->top + (lpRect->bottom - lpRect->top) * yDPI / 96;
	}

	VOID ScaleSize (SIZE* pSize)
	{
		pSize->cx = MulDiv(pSize->cx, xDPI, 96);
		pSize->cy = MulDiv(pSize->cy, yDPI, 96);
	}

	INT MapPointSize (INT nPointSize)
	{
		return -MulDiv(nPointSize, yDPI, 72);
	}
};
