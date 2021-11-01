#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "Library\DPI.h"
#include "NeuralAPI.h"
#include "Extensibility.h"
#include "NeuralNetApp.h"

BOOL ProcessCommandLine (CNeuralNetApp* lpApp, LPSTR lpCmdLine, HRESULT* lphr, LPCSTR* lplpcszAppCmd)
{
	BOOL fProcessed = FALSE;
	if(lpCmdLine && *lpCmdLine)
	{
		if(TStrIStr(lpCmdLine,"/register") || TStrIStr(lpCmdLine,"-register"))
		{
			*lphr = lpApp->RegisterFileType();
			fProcessed = TRUE;
		}
		else if(TStrIStr(lpCmdLine,"/unregister") || TStrIStr(lpCmdLine,"-unregister"))
		{
			*lphr = lpApp->UnregisterFileType();
			fProcessed = TRUE;
		}
		else
		{
			LPCSTR lpcszPtr = TStrIStr(lpCmdLine,"/open");
			if(NULL == lpcszPtr)
				lpcszPtr = TStrIStr(lpCmdLine,"-open");
			if(lpcszPtr)
				*lplpcszAppCmd = lpcszPtr + 1;
		}
	}
	return fProcessed;
}

BOOL InitializeApplication (CNeuralNetApp** lplpApp, HINSTANCE hInstance, LPSTR lpCmdLine, INT nCmdShow, HRESULT* lphr)
{
	BOOL fStartApp = FALSE;
	LPCSTR lpcszAppCmd = NULL;
	*lplpApp = __new CNeuralNetApp(hInstance);
	if(*lplpApp)
	{
		if(!ProcessCommandLine(*lplpApp, lpCmdLine, lphr, &lpcszAppCmd))
		{
			*lphr = (*lplpApp)->Initialize((INT)DPI::Scale(480.0f), (INT)DPI::Scale(400.0f), nCmdShow);
			if(SUCCEEDED(*lphr))
			{
				if(lpcszAppCmd)
					(*lplpApp)->ExecCommand(lpcszAppCmd);
				fStartApp = TRUE;
			}
			else
				SafeRelease(*lplpApp);
		}
	}
	else
		*lphr = E_OUTOFMEMORY;
	return fStartApp;
}

INT WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	UNREFERENCED_PARAMETER(hPrevInstance);

	HRESULT hr;
	CNeuralNetApp* lpApp;

	DPI::SetDPIAware();
	DPI::Initialize();

	Check(CoInitialize(NULL));

	Check(CNeuralNetApp::Register(hInstance));
	if(InitializeApplication(&lpApp, hInstance, lpCmdLine, nCmdShow, &hr))
	{
		MSG msg;

		ExtLoad(lpApp);

		while(GetMessage(&msg,NULL,0,0) > 0)
		{
			if(lpApp->PreTranslateMessage(&msg) == FALSE)
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		ExtUnload();

		lpApp->Uninitialize();
	}

	SafeRelease(lpApp);

Cleanup:
	CoUninitialize();
	return hr;
}
