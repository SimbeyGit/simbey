#include <winsock2.h>
#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "Library\Util\Registry.h"
#include "SampleService.h"

CSampleService::CSampleService (HINSTANCE hInstance) :
	m_hInstance(hInstance)
{
	m_lpHost = NULL;
}

CSampleService::~CSampleService ()
{
	Assert(m_lpHost == NULL);
}

PCWSTR CSampleService::GetServiceRegKey (VOID)
{
	return L"Software\\Simbey\\SampleService";
}

HRESULT CSampleService::SetInstallPath (PCTSTR pctzInstallPath, INT cchInstallPath)
{
	HKEY hKey;
	HRESULT hr = Registry::CreateKey(HKEY_LOCAL_MACHINE, GetServiceRegKey(), KEY_ALL_ACCESS, &hKey);
	if(SUCCEEDED(hr))
	{
		if(pctzInstallPath && *pctzInstallPath)
			hr = HRESULT_FROM_WIN32(RegSetValueEx(hKey, L"BasePath", 0, REG_SZ, (LPBYTE)pctzInstallPath, (cchInstallPath + 1) * sizeof(WCHAR)));
		else
			RegDeleteValue(hKey, L"BasePath");
		RegCloseKey(hKey);
	}
	return hr;
}

HRESULT CSampleService::GetSvcName (PWSTR pwzBuffer, LONG Length)
{
	return TStrCchCpy(pwzBuffer, Length, L"SampleService");
}

HRESULT CSampleService::GetSvcPath (PWSTR pwzBuffer, LONG Length)
{
	HRESULT hr;

	if(GetModuleFileName(NULL, pwzBuffer, Length))
		hr = S_OK;
	else
		hr = HRESULT_FROM_WIN32(GetLastError());

	return hr;
}

HRESULT CSampleService::GetSvcDesc (PWSTR pwzBuffer, LONG Length)
{
	return TStrCchCpy(pwzBuffer, Length, L"Demonstrates how to create a basic Win32 service");
}

HRESULT CSampleService::GetSvcDisp (PWSTR pwzBuffer, LONG Length)
{
	return TStrCchCpy(pwzBuffer, Length, L"Sample Service");
}

DWORD CSampleService::GetStartType (VOID)
{
	return SERVICE_AUTO_START;
}

HRESULT CSampleService::GetDependencies (__out_ecount(cchMaxBuffer) PWSTR pwzBuffer, LONG cchMaxBuffer, __out LONG* pcchBuffer)
{
	HRESULT hr;

	CheckIf(cchMaxBuffer < 2, STRSAFE_E_INSUFFICIENT_BUFFER);

	// The buffer must be double-zero terminated.
	pwzBuffer[0] = '\0';
	pwzBuffer[1] = '\0';

	if(pcchBuffer)
		*pcchBuffer = 0;

	hr = S_OK;

Cleanup:
	return hr;
}

BOOL CSampleService::Connect (IServiceHost* lpHost, DWORD cArgs, PWSTR* ppwzArgs)
{
	BOOL fSuccess = FALSE;
	if(m_lpHost == NULL && lpHost)
	{
		m_lpHost = lpHost;
		m_lpHost->AddRef();

		if(Initialize())
			fSuccess = TRUE;
		else
			Disconnect();
	}
	return fSuccess;
}

BOOL CSampleService::StartService (VOID)
{
	// Add service starting requirements here
	return TRUE;
}

VOID CSampleService::StopService (VOID)
{
	// Add service stopping requirements here
}

VOID CSampleService::Disconnect (VOID)
{
	// Perform any memory deletions/cleanup here

	SafeRelease(m_lpHost);
}

DWORD CSampleService::AcceptServiceRequests (VOID)
{
	return 0;
}

DWORD CSampleService::HandleServiceRequest (DWORD dwControl, DWORD dwEventType, PVOID pvEventData)
{
	return (SERVICE_CONTROL_INTERROGATE == dwControl) ? NO_ERROR : ERROR_CALL_NOT_IMPLEMENTED;
}

HRESULT CSampleService::PostInstall (PCWSTR pcwzServiceFilePath, PCWSTR pcwzDisplayName)
{
	return S_OK;
}

BOOL CSampleService::QueryStopRequest (BOOL fShutdown, __inout DWORD* pdwResult)
{
	return TRUE;
}

HRESULT CSampleService::SetBasePath (PCWSTR pcwzBasePath)
{
	return m_strBasePath.Assign(TStrLenChecked(pcwzBasePath), pcwzBasePath);
}

BOOL CSampleService::Initialize (VOID)
{
	// Perform service initializations here

	return TRUE;
}

VOID CSampleService::Uninitialize (VOID)
{
	// Perform service uninitializations here
}
