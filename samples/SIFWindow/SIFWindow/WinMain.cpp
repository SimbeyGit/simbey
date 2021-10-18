#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\DPI.h"
#include "SIFWindow.h"

INT WINAPI wWinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, INT nCmdShow)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	HRESULT hr;
	CSIFWindow* pApp = NULL;

	Check(DPI::SetDPIAware());
	CheckIfGetLastError(!DPI::Initialize());

	Check(CSIFWindow::Register(hInstance));

	pApp = __new CSIFWindow(hInstance);
	CheckAlloc(pApp);

	Check(pApp->Initialize((INT)DPI::Scale(512.0f), (INT)DPI::ScaleY(320.0f), nCmdShow));
	pApp->Run();

Cleanup:
	SafeRelease(pApp);

	CSIFWindow::Unregister(hInstance);

	return hr;
}
