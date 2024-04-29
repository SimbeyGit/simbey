#pragma once

#include "Library\Service\IServiceHost.h"

namespace SvcHost
{
	enum Property
	{
		Name,
		Description,
		Display
	};
}

interface __declspec(uuid("76CBA51D-9CC3-4b2d-AAB3-F94209411EE5")) IHostedService : IUnknown
{
	// Optional methods that can be handled by CHostedServiceImpl
	virtual HRESULT WINAPI RegisterInstallation (HKEY hKey, PCWSTR pcwzInstallPath, INT cchInstallPath) = 0;
	virtual HRESULT WINAPI ExecuteCommand (INT cArgs, __in_ecount(cArgs) PWSTR* ppwzArgs, INT idxArg, __out INT* pnSkip, __out BOOL* pfCanStartService) = 0;
	virtual HMODULE WINAPI GetServiceModule (VOID) = 0;
	virtual DWORD WINAPI GetStartType (VOID) = 0;
	virtual HRESULT WINAPI GetDependencies (__out ISequentialStream* pstmDependencies) = 0;
	virtual HRESULT WINAPI PostInstall (PCWSTR pcwzServiceFilePath, PCWSTR pcwzDisplayName) = 0;
	virtual DWORD WINAPI AcceptServiceRequests (DWORD dwStatus) = 0;
	virtual BOOL WINAPI HandleServiceRequest (DWORD dwControl, DWORD dwEventType, PVOID pvEventData, __out DWORD* pdwResult) = 0;
	virtual BOOL WINAPI QueryStopRequest (BOOL fShutdown, __inout DWORD* pdwResult) = 0;

	// Required methods that must be implemented by the user's class
	virtual PCWSTR WINAPI GetServiceRegKey (VOID) = 0;
	virtual HRESULT WINAPI GetServiceProperty (SvcHost::Property eProperty, __out_ecount(cchMaxBuffer) PWSTR pwzBuffer, INT cchMaxBuffer) = 0;
	virtual HRESULT WINAPI SetBasePath (PCWSTR pcwzBasePath) = 0;
	virtual BOOL WINAPI Initialize (VOID) = 0;
	virtual BOOL WINAPI Start (VOID) = 0;
	virtual VOID WINAPI Stop (VOID) = 0;
	virtual VOID WINAPI Uninitialize (VOID) = 0;
};

class CHostedServiceImpl : public IHostedService
{
	virtual HRESULT WINAPI RegisterInstallation (HKEY hKey, PCWSTR pcwzInstallPath, INT cchInstallPath) { return S_OK; }
	virtual HRESULT WINAPI ExecuteCommand (INT cArgs, __in_ecount(cArgs) PWSTR* ppwzArgs, INT idxArg, __out INT* pnSkip, __out BOOL* pfCanStartService) { return S_OK; }
	virtual HMODULE WINAPI GetServiceModule (VOID) { return GetModuleHandle(NULL); }
	virtual DWORD WINAPI GetStartType (VOID) { return SERVICE_AUTO_START; }
	virtual HRESULT WINAPI GetDependencies (__out ISequentialStream* pstmDependencies) { return S_OK; }
	virtual HRESULT WINAPI PostInstall (PCWSTR pcwzServiceFilePath, PCWSTR pcwzDisplayName) { return S_OK; }
	virtual DWORD WINAPI AcceptServiceRequests (DWORD dwStatus) { return SERVICE_ACCEPT_STOP; }
	virtual BOOL WINAPI HandleServiceRequest (DWORD dwControl, DWORD dwEventType, PVOID pvEventData, __out DWORD* pdwResult) { return FALSE; }
	virtual BOOL WINAPI QueryStopRequest (BOOL fShutdown, __inout DWORD* pdwResult) { return TRUE; }
};

VOID WINAPI SvcInitGlobalCrashHandler (PCWSTR (WINAPI* pfnGetServiceRegKey)());
HRESULT WINAPI SvcRunHostedService (HRESULT (WINAPI* pfnCreateService)(IServiceStatus*, IHostedService**), INT cArgs, __in_ecount(cArgs) PWSTR* ppwzArgs);
