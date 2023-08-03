// Enable tooltip windows and Windows Vista visual themes
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define	INITGUID
#include <windows.h>
#include <commctrl.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\DPI.h"
#include "Splitter.h"
#include "TextEditor.h"
#include "QuadooProject.h"
#include "QuadooStudio.h"

HRESULT StartCommonControls (DWORD flags)
{
	INITCOMMONCONTROLSEX iccex;
	iccex.dwSize = sizeof(iccex);
	iccex.dwICC = flags;
	return InitCommonControlsEx(&iccex) ? S_OK : HrEnsureLastError();
}

INT WINAPI wWinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, INT nCmdShow)
{
#if defined(_DEBUG) && !defined(__VIRTUAL_DBGMEM)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	HRESULT hr;
	MSG msg;
	CQuadooStudio* pQuadooStudio = NULL;

	Check(DPI::SetDPIAware());
	CheckIfGetLastError(!DPI::Initialize());

	Check(OleInitialize(NULL));
	Check(StartCommonControls(ICC_TAB_CLASSES | ICC_TREEVIEW_CLASSES));

	Check(CSplitter::Register(hInstance));
	Check(CTextEditor::Register(hInstance));
	Check(CQuadooProject::Register(hInstance));
	Check(CQuadooStudio::Register(hInstance));

	pQuadooStudio = __new CQuadooStudio(hInstance);
	CheckAlloc(pQuadooStudio);

	Check(pQuadooStudio->Initialize(lpCmdLine, (INT)DPI::Scale(600.0f), (INT)DPI::Scale(460.0f), nCmdShow));

	while(GetMessage(&msg,NULL,0,0) > 0)
	{
		if(!pQuadooStudio->PreTranslate(&msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

Cleanup:
	SafeRelease(pQuadooStudio);

	CQuadooStudio::Unregister(hInstance);
	CQuadooProject::Unregister(hInstance);
	CTextEditor::Unregister(hInstance);
	CSplitter::Unregister(hInstance);

	OleUninitialize();

	return hr;
}
