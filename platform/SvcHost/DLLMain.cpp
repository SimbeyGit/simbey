#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Util\CrashHandler.h"
#include "Library\Service\ServiceHost.h"
#include "Library\Service\ConsoleHost.h"
#include "Published\SvcHost.h"
#include "WrappedService.h"

PCWSTR (WINAPI* g_pfnGetServiceRegKey)() = NULL;

BOOL IsCommandFlag (PCWSTR pcwzArg, PCWSTR pcwzFlag)
{
	return ('-' == *pcwzArg || '/' == *pcwzArg) && 0 == lstrcmpi(pcwzArg + 1, pcwzFlag);
}

HRESULT ExecuteModuleCommands (INT cArgs, __in_ecount(cArgs) PWSTR* ppwzArgs, IServiceHost* lpHost, CWrappedService* lpService, __out BOOL* pfRunService)
{
	HRESULT hr = S_FALSE;
	BOOL fRunService = TRUE;
	HANDLE hOutput = GetStdHandle(STD_OUTPUT_HANDLE);

	for(INT i = 1; i < cArgs; i++)
	{
		if(IsCommandFlag(ppwzArgs[i], L"basepath") && i < cArgs - 1)
		{
			Check(lpService->SetBasePath(ppwzArgs[i + 1]));
			i++;
		}
		else if(IsCommandFlag(ppwzArgs[i], L"i") || IsCommandFlag(ppwzArgs[i], L"install"))
		{
			Check(lpHost->Install(lpService, i < cArgs - 1 ? ppwzArgs[i + 1] : NULL));
			i++;
			fRunService = FALSE;
		}
		else if(IsCommandFlag(ppwzArgs[i], L"r") || IsCommandFlag(ppwzArgs[i], L"register"))
		{
			Check(lpHost->Install(lpService, NULL));
			fRunService = FALSE;
		}
		else if(IsCommandFlag(ppwzArgs[i], L"u") || IsCommandFlag(ppwzArgs[i], L"uninstall"))
		{
			Check(lpHost->Uninstall(lpService));
			fRunService = FALSE;
		}
		else
		{
			BOOL fCanStartService = fRunService;
			INT nSkip = 0;
			Check(lpService->ExecuteCommand(cArgs, ppwzArgs, i, &nSkip, &fCanStartService));
			i += nSkip;
			if(!fCanStartService)
				fRunService = FALSE;
		}
	}

Cleanup:
	*pfRunService = SUCCEEDED(hr) && fRunService;
	return hr;
}

BOOL GetCrashDir (PTSTR ptzCrashDir, INT cchMaxDir, __out INT* pcchCrashDir)
{
	BOOL fSuccess = FALSE;
	HKEY hKey;
	if(g_pfnGetServiceRegKey && ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, g_pfnGetServiceRegKey(), 0, KEY_READ, &hKey))
	{
		DWORD dwType, dwSize = cchMaxDir * sizeof(TCHAR);
		if(ERROR_SUCCESS == RegQueryValueEx(hKey, TEXT("CrashDir"), NULL, &dwType, (LPBYTE)ptzCrashDir, &dwSize))
		{
			if(0 < dwSize && '\0' == ptzCrashDir[dwSize - 1])
				dwSize--;

			if(0 < dwSize)
			{
				if(ptzCrashDir[dwSize - 1] != '\\' && dwSize < (DWORD)cchMaxDir)
				{
					ptzCrashDir[dwSize++] = '\\';
					fSuccess = TRUE;
				}
				else
					fSuccess = TRUE;
			}

			*pcchCrashDir = dwSize;
		}
		RegCloseKey(hKey);
	}
	return fSuccess;
}

HRESULT CreateServiceHost (INT cArgs, __in_ecount(cArgs) PWSTR* ppwzArgs, __deref_out IServiceHost** ppHost)
{
	HRESULT hr = S_FALSE;

	for(INT i = 1; i < cArgs; i++)
	{
		if(IsCommandFlag(ppwzArgs[i], L"console"))
		{
			CConsoleHost* pHost = __new CConsoleHost;
			CheckAlloc(pHost);
			hr = S_OK;
			*ppHost = pHost;
			break;
		}
	}

	if(S_FALSE == hr)
	{
		CServiceHost* pHost = __new CServiceHostEx;
		CheckAlloc(pHost);
		hr = S_OK;
		*ppHost = pHost;
	}

Cleanup:
	return hr;
}

VOID WINAPI SvcInitGlobalCrashHandler (PCWSTR (WINAPI* pfnGetServiceRegKey)())
{
	g_pfnGetServiceRegKey = pfnGetServiceRegKey;
	SetGlobalCrashHandler();
}

HRESULT WINAPI SvcRunHostedService (HRESULT (WINAPI* pfnCreateService)(IServiceStatus*, IHostedService**), INT cArgs, __in_ecount(cArgs) PWSTR* ppwzArgs)
{
	HRESULT hr;

	if(pfnCreateService)
	{
		IServiceHost* pHost;

		hr = CreateServiceHost(cArgs, ppwzArgs, &pHost);
		if(SUCCEEDED(hr))
		{
			IHostedService* pHostedService;

			hr = pfnCreateService(pHost, &pHostedService);
			if(SUCCEEDED(hr))
			{
				CWrappedService* pService = __new CWrappedService(pHostedService);
				if(pService)
				{
					BOOL fRunService;
					hr = ExecuteModuleCommands(cArgs, ppwzArgs, pHost, pService, &fRunService);
					if(SUCCEEDED(hr) && fRunService)
						hr = pHost->RunService(pService);
					pService->Release();
				}
				else
					hr = E_OUTOFMEMORY;

				pHostedService->Release();
			}

			pHost->Release();
		}
	}
	else
		hr = E_POINTER;

	return hr;
}

BOOL APIENTRY DllMain (HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch(ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
