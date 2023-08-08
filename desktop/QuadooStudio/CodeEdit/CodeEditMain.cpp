#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "TextEditor.h"

HMODULE g_hModule;
DWORD g_dwClassCookie;

BOOL APIENTRY DllMain (HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch(ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		g_hModule = hModule;
		g_dwClassCookie = GetTickCount();
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

HRESULT WINAPI CodeEditRegister (VOID)
{
	return CTextEditor::Register(g_hModule, g_dwClassCookie);
}

HRESULT WINAPI CodeEditUnregister (VOID)
{
	return CTextEditor::Unregister(g_hModule, g_dwClassCookie);
}

HRESULT WINAPI CodeEditCreate (HWND hwndParent, const RECT& rcSite, INT nTabWidth, bool fDarkMode, bool fUseSystemColors, __deref_out ICodeEditor** ppEditor)
{
	HRESULT hr;
	CTextEditor* pEditor = __new CTextEditor(g_hModule, fDarkMode, fUseSystemColors);

	CheckAlloc(pEditor);
	Check(pEditor->Initialize(hwndParent, rcSite, nTabWidth, g_dwClassCookie));
	*ppEditor = pEditor;
	pEditor = NULL;

Cleanup:
	SafeRelease(pEditor);
	return hr;
}
