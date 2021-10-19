#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\DPI.h"
#include "MOMCombatDemo.h"

INT WINAPI wWinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, INT nCmdShow)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	HRESULT hr;
	CMOMCombatDemo* pApp = NULL;

	Check(DPI::SetDPIAware());
	CheckIfGetLastError(!DPI::Initialize());

	Check(CMOMCombatDemo::Register(hInstance));

	pApp = __new CMOMCombatDemo(hInstance);
	CheckAlloc(pApp);

	Check(pApp->Initialize((INT)DPI::Scale(512.0f), (INT)DPI::ScaleY(320.0f), nCmdShow));
	pApp->Run();

Cleanup:
	SafeRelease(pApp);

	CMOMCombatDemo::Unregister(hInstance);

	return hr;
}
