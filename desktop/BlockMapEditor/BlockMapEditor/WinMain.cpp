#define INITGUID
#include <windows.h>
#include <commctrl.h>
#include <uiRibbon.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Window\AdapterWindow.h"
#include "Library\Spatial\Geometry.h"
#include "Library\DPI.h"
#include "BlockMapEditorApp.h"

INT WINAPI wWinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, INT nCmdShow)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	HRESULT hr;

	Geometry::InitializeTables();

	hr = CoInitialize(NULL);
	if(SUCCEEDED(hr))
	{
		CBlockMapEditorApp* pApp = NULL;
		MSG msg;

		CheckIf(!DPI::Initialize(), E_FAIL);

		Check(CBlockMapEditorApp::Register(hInstance));
		Check(CDialogControlAdapter::Register(L"DlgAdapter", hInstance, NULL));

		pApp = __new CBlockMapEditorApp(hInstance);
		CheckAlloc(pApp);

		Check(pApp->Initialize(lpCmdLine, nCmdShow));

		while(GetMessage(&msg,NULL,0,0) > 0)
		{
			if(!pApp->TranslateMsg(&msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

	Cleanup:
		SafeRelease(pApp);

		CoUninitialize();
	}

	return hr;
}
