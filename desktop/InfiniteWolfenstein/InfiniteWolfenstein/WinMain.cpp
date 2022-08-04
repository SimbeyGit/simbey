#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Spatial\Geometry.h"
#include "InfiniteWolfenstein.h"

INT WINAPI wWinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, INT nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);

#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	HRESULT hr;
	CInfiniteWolfenstein* pApp = NULL;

	Geometry::InitializeTables();

	Check(CInfiniteWolfenstein::Register(hInstance));

	pApp = __new CInfiniteWolfenstein(hInstance);
	CheckAlloc(pApp);

	Check(pApp->Initialize(lpCmdLine, nCmdShow));
	pApp->Run();

Cleanup:
	SafeRelease(pApp);
	return (INT)hr;
}
