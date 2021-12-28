#define INITGUID
#include <windows.h>
#include <uiRibbon.h>
#include "Library\Core\CoreDefs.h"
#include "Library\DPI.h"
#include "RibbonWindow.h"

INT WINAPI wWinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, INT nCmdShow)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	HRESULT hr;
	CRibbonWindow* pApp = NULL;
	MSG msg;

	Check(CoInitialize(NULL));

	Check(DPI::SetDPIAware());
	CheckIfGetLastError(!DPI::Initialize());

	Check(CRibbonWindow::Register(hInstance));

	pApp = __new CRibbonWindow(hInstance);
	CheckAlloc(pApp);

	Check(pApp->Initialize((INT)DPI::Scale(640.0f), (INT)DPI::ScaleY(480.0f), nCmdShow));

	while(GetMessage(&msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

Cleanup:
	SafeRelease(pApp);

	CRibbonWindow::Unregister(hInstance);

	CoUninitialize();

	return hr;
}
