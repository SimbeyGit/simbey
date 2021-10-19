#define INITGUID
#include <windows.h>
#include <uiRibbon.h>
#include "Library\Core\CoreDefs.h"
#include "Library\DPI.h"
#include "MOMWorldEditor.h"

INT WINAPI wWinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, INT nCmdShow)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	HRESULT hr;
	CMOMWorldEditor* pApp = NULL;

	Check(CoInitialize(NULL));

	Check(DPI::SetDPIAware());
	CheckIfGetLastError(!DPI::Initialize());

	Check(CMOMWorldEditor::Register(hInstance));

	pApp = __new CMOMWorldEditor(hInstance);
	CheckAlloc(pApp);

	Check(pApp->Initialize((INT)DPI::Scale(768.0f), (INT)DPI::ScaleY(436.0f), nCmdShow));
	pApp->Run();

Cleanup:
	SafeRelease(pApp);

	CMOMWorldEditor::Unregister(hInstance);

	CoUninitialize();

	return hr;
}
