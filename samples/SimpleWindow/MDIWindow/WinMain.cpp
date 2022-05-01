#include <windows.h>
#include <commctrl.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\DPI.h"
#include "MDIWindow.h"

INT WINAPI wWinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, INT nCmdShow)
{
	HRESULT hr;
	CMDIWindow* pApp = NULL;
	MSG msg;
	INITCOMMONCONTROLSEX iccex;

	Check(DPI::SetDPIAware());
	CheckIfGetLastError(!DPI::Initialize());

	iccex.dwSize = sizeof(iccex);
	iccex.dwICC = ICC_LISTVIEW_CLASSES | ICC_DATE_CLASSES;
	CheckIfGetLastError(!InitCommonControlsEx(&iccex));

	Check(CMDIWindow::Register(hInstance));
	Check(CMDIChild::Register(hInstance));

	pApp = __new CMDIWindow(hInstance);
	CheckAlloc(pApp);

	Check(pApp->Initialize((INT)DPI::Scale(600.0f), (INT)DPI::ScaleY(500.0f), nCmdShow));

	while(GetMessage(&msg, NULL, 0, 0) > 0)
	{
		if(!pApp->ProcessMDIMessage(&msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

Cleanup:
	SafeRelease(pApp);

	CMDIChild::Unregister(hInstance);
	CMDIWindow::Unregister(hInstance);

	return hr;
}
