#include <windows.h>
#include <commctrl.h>
#include <gdiplus.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\DPI.h"
#include "CADWindow.h"

INT WINAPI wWinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, INT nCmdShow)
{
#if defined(_DEBUG) && !defined(__VIRTUAL_DBGMEM)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	HRESULT hr;
	CCADWindow* pApp = NULL;
	MSG msg;
	INITCOMMONCONTROLSEX iccex;
	ULONG_PTR gdiPlusToken = 0;
	Gdiplus::GdiplusStartupInput gdiPlusStartupInput;

	Check(DPI::SetDPIAware());
	CheckIfGetLastError(!DPI::Initialize());

	iccex.dwSize = sizeof(iccex);
	iccex.dwICC = ICC_LISTVIEW_CLASSES | ICC_DATE_CLASSES;
	CheckIfGetLastError(!InitCommonControlsEx(&iccex));

	Gdiplus::GdiplusStartup(&gdiPlusToken, &gdiPlusStartupInput, NULL);

	Check(CCADWindow::Register(hInstance));

	pApp = __new CCADWindow(hInstance);
	CheckAlloc(pApp);

	Check(pApp->Initialize((INT)DPI::Scale(600.0f), (INT)DPI::ScaleY(500.0f), nCmdShow));

	while(GetMessage(&msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

Cleanup:
	SafeRelease(pApp);

	CCADWindow::Unregister(hInstance);

	if(0 != gdiPlusToken)
		Gdiplus::GdiplusShutdown(gdiPlusToken);

	return hr;
}
