#include <windows.h>
#include "..\Core\CoreDefs.h"
#include "..\Core\StringCore.h"
#include "ModuleCookie.h"

#ifndef	LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR
	#define	LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR	0x00000100
#endif
#ifndef LOAD_LIBRARY_SEARCH_USER_DIRS
	#define	LOAD_LIBRARY_SEARCH_USER_DIRS		0x00000400
#endif
#ifndef	LOAD_LIBRARY_SEARCH_SYSTEM32
	#define	LOAD_LIBRARY_SEARCH_SYSTEM32		0x00000800
#endif

CModuleCookie::CModuleCookie () :
	m_hModule(NULL),
	m_pvCookie(NULL)
{
}

CModuleCookie::~CModuleCookie ()
{
	Free();
}

HRESULT CModuleCookie::Load (PCWSTR pcwzModule)
{
	HRESULT hr;

	CheckIf(NULL != m_hModule || NULL != m_pvCookie, E_UNEXPECTED);
	CheckIfGetLastError(INVALID_FILE_ATTRIBUTES == GetFileAttributes(pcwzModule));

	m_hModule = LoadLibrary(pcwzModule);
	if(NULL == m_hModule)
	{
		PVOID (WINAPI* pfnAddDllDirectory)(PCWSTR NewDirectory);
		WCHAR wzDir[MAX_PATH];
		PCWSTR pcwzPtr = TStrRChr(pcwzModule, L'\\');
		HMODULE hKernel = GetModuleHandle(L"Kernel32.dll");
		CheckIfGetLastError(NULL == hKernel);
		Check(TGetFunction(hKernel, "AddDllDirectory", &pfnAddDllDirectory));

		CheckIf(NULL == pcwzPtr, E_INVALIDARG);
		Check(TStrCchCpyN(wzDir, ARRAYSIZE(wzDir), pcwzModule, static_cast<INT>(pcwzPtr - pcwzModule)));
		m_pvCookie = pfnAddDllDirectory(wzDir);
		CheckIfGetLastError(NULL == m_pvCookie);

		m_hModule = LoadLibraryEx(pcwzModule, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);
		CheckIfGetLastError(NULL == m_hModule);
	}

Cleanup:
	return hr;
}

VOID CModuleCookie::Free (VOID)
{
	if(m_hModule)
	{
		FreeLibrary(m_hModule);
		m_hModule = NULL;
	}
	if(m_pvCookie)
	{
		HMODULE hKernel = GetModuleHandle(L"Kernel32.dll");
		if(hKernel)
		{
			BOOL (WINAPI* pfnRemoveDllDirectory)(PVOID pvCookie);
			if(SUCCEEDED(TGetFunction(hKernel, "RemoveDllDirectory", &pfnRemoveDllDirectory)))
				pfnRemoveDllDirectory(m_pvCookie);
			m_pvCookie = NULL;
		}
	}
}
