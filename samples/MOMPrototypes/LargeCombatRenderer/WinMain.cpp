#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\DPI.h"
#include "LargeCombatRenderer.h"

INT WINAPI wWinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, INT nCmdShow)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	HRESULT hr;
	CLargeCombatRenderer* pApp = NULL;

	Check(DPI::SetDPIAware());
	CheckIfGetLastError(!DPI::Initialize());

	Check(CLargeCombatRenderer::Register(hInstance));

	pApp = __new CLargeCombatRenderer(hInstance);
	CheckAlloc(pApp);

	Check(pApp->Initialize((INT)DPI::Scale(640.0f), (INT)DPI::ScaleY(400.0f), nCmdShow));
	pApp->Run();

Cleanup:
	SafeRelease(pApp);

	CLargeCombatRenderer::Unregister(hInstance);

	return hr;
}
