#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "..\TestExtension\ExternalControl.h"

static HMODULE g_extension;
static CREATE_EXTERNAL_FACTORY g_createFactory;
static GET_EXTERNAL_COUNTS g_getCounts;
static EXTERNAL_CAN_UNLOAD g_canUnload;

static HRESULT LoadExtension()
{
	HRESULT hr = S_OK;
	WCHAR path[MAX_PATH];
	DWORD length;
	if(g_extension) goto Cleanup;
	length = GetModuleFileNameW(NULL, path, ARRAYSIZE(path));
	CheckIf(!length || length >= ARRAYSIZE(path), HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER));
	while(length && path[length - 1] != L'\\') length--;
	Check(TStrCchCpy(path + length, ARRAYSIZE(path) - length, L"QuadooFormTestExtension.dll"));
	g_extension = LoadLibraryW(path); CheckIf(!g_extension, HrEnsureLastError());
	g_createFactory = reinterpret_cast<CREATE_EXTERNAL_FACTORY>(GetProcAddress(g_extension, "CreateExternalFactory"));
	g_getCounts = reinterpret_cast<GET_EXTERNAL_COUNTS>(GetProcAddress(g_extension, "GetExternalCounts"));
	g_canUnload = reinterpret_cast<EXTERNAL_CAN_UNLOAD>(GetProcAddress(g_extension, "DllCanUnloadNow"));
	CheckIf(!g_createFactory || !g_getCounts || !g_canUnload, E_NOINTERFACE);
Cleanup:
	if(FAILED(hr) && g_extension) { FreeLibrary(g_extension); g_extension = NULL; }
	return hr;
}

HRESULT CreateExternalFactory(IQuadooControlFactory** factory)
{
	HRESULT hr;
	CheckIf(!factory, E_POINTER); *factory = NULL;
	Check(LoadExtension()); Check(g_createFactory(factory));
Cleanup:
	return hr;
}
HRESULT CreateExternalComponentFactory(IQuadooComponentFactory** factory)
{
	HRESULT hr; CREATE_EXTERNAL_COMPONENT_FACTORY create;
	CheckIf(!factory,E_POINTER); *factory = NULL;
	Check(LoadExtension());
	create = reinterpret_cast<CREATE_EXTERNAL_COMPONENT_FACTORY>(GetProcAddress(g_extension,"CreateExternalComponentFactory"));
	CheckIf(!create,E_NOINTERFACE); Check(create(factory));
Cleanup:
	return hr;
}
ExternalCounts ExternalStatistics()
{
	ExternalCounts counts = { 0, 0, 0, 0, 0 };
	if(SUCCEEDED(LoadExtension())) g_getCounts(&counts);
	return counts;
}
HRESULT ConfigureExternalKeyClaims(IQuadooWinControl* control, BOOL enable)
{
	HRESULT hr; CONFIGURE_EXTERNAL_KEY_CLAIMS configure;
	Check(LoadExtension()); configure = reinterpret_cast<CONFIGURE_EXTERNAL_KEY_CLAIMS>(GetProcAddress(g_extension,"ConfigureExternalKeyClaims"));
	CheckIf(!configure,E_NOINTERFACE); Check(configure(control,enable));
Cleanup:
	return hr;
}
HRESULT UnloadExternalExtension()
{
	HRESULT hr = S_OK;
	if(g_extension)
	{
		CheckIf(g_canUnload() != S_OK, HRESULT_FROM_WIN32(ERROR_BUSY));
		CheckIf(!FreeLibrary(g_extension), HrEnsureLastError()); g_extension = NULL;
		g_createFactory = NULL; g_getCounts = NULL; g_canUnload = NULL;
	}
Cleanup:
	return hr;
}
HRESULT ExternalCanUnload()
{
	HRESULT hr;
	Check(LoadExtension()); hr = g_canUnload();
Cleanup:
	return hr;
}

HRESULT CreateExternalScriptFactory(IQuadooControlFactory** factory)
{
	HRESULT hr;
	CREATE_EXTERNAL_FACTORY create;
	CheckIf(!factory,E_POINTER);
	*factory = NULL;
	Check(LoadExtension());
	create = reinterpret_cast<CREATE_EXTERNAL_FACTORY>(GetProcAddress(g_extension,"CreateExternalScriptFactory"));
	CheckIf(!create,E_NOINTERFACE);
	Check(create(factory));
Cleanup:
	return hr;
}
