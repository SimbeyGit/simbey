#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Util\CrashHandler.h"
#include "Library\Service\ServiceHost.h"
#include "Library\Service\ConsoleHost.h"
#include "SampleService.h"

BOOL IsCommandFlag (PCWSTR pcwzArg, PCWSTR pcwzFlag)
{
	return ('-' == *pcwzArg || '/' == *pcwzArg) && 0 == lstrcmpi(pcwzArg + 1, pcwzFlag);
}

HRESULT ExecuteModuleCommands (INT cArgs, __in_ecount(cArgs) PWSTR* ppwzArgs, IServiceHost* lpHost, CSampleService* lpService, __out BOOL* pfRunService)
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
	}

Cleanup:
	*pfRunService = SUCCEEDED(hr) && fRunService;

	return hr;
}

BOOL GetCrashDir (PTSTR ptzCrashDir, INT cchMaxDir, __out INT* pcchCrashDir)
{
	BOOL fSuccess = FALSE;
	HKEY hKey;
	if(ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, CSampleService::GetServiceRegKey(), 0, KEY_READ, &hKey))
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

INT wmain (INT cArgs, __in_ecount(cArgs) PWSTR* ppwzArgs)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	HRESULT hr;

	SetGlobalCrashHandler();

	IServiceHost* pHost;
	hr = CreateServiceHost(cArgs, ppwzArgs, &pHost);
	if(SUCCEEDED(hr))
	{
		CSampleService* pService = __new CSampleService(GetModuleHandle(NULL));
		if(pService)
		{
			BOOL fRunService;
			hr = ExecuteModuleCommands(cArgs, ppwzArgs, pHost, pService, &fRunService);
			if(SUCCEEDED(hr) && fRunService)
				hr = pHost->RunService(pService);
			pService->Release();
		}
		pHost->Release();
	}

	return (INT)hr;
}
