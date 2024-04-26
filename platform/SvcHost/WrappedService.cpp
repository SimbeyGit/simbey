#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "Library\Core\StaticStream.h"
#include "Library\Util\Registry.h"
#include "WrappedService.h"

CWrappedService::CWrappedService (IHostedService* pService) :
	m_pService(pService),
	m_lpHost(NULL)
{
	m_pService->AddRef();
	m_lpHost = NULL;
}

CWrappedService::~CWrappedService ()
{
	Assert(m_lpHost == NULL);
	m_pService->Release();
}

HRESULT CWrappedService::SetInstallPath (PCTSTR pctzInstallPath, INT cchInstallPath)
{
	HKEY hKey;
	HRESULT hr = Registry::CreateKey(HKEY_LOCAL_MACHINE, m_pService->GetServiceRegKey(), KEY_ALL_ACCESS, &hKey);
	if(SUCCEEDED(hr))
	{
		if(pctzInstallPath && *pctzInstallPath)
			hr = HRESULT_FROM_WIN32(RegSetValueEx(hKey, L"BasePath", 0, REG_SZ, (LPBYTE)pctzInstallPath, (cchInstallPath + 1) * sizeof(WCHAR)));
		else
			RegDeleteValue(hKey, L"BasePath");

		if(SUCCEEDED(hr))
			hr = m_pService->RegisterInstallation(hKey, pctzInstallPath, cchInstallPath);

		RegCloseKey(hKey);
	}
	return hr;
}

HRESULT CWrappedService::GetSvcName (PWSTR pwzBuffer, LONG Length)
{
	return m_pService->GetServiceProperty(SvcHost::Name, pwzBuffer, Length);
}

HRESULT CWrappedService::GetSvcPath (PWSTR pwzBuffer, LONG Length)
{
	HRESULT hr;

	if(GetModuleFileName(m_pService->GetServiceModule(), pwzBuffer, Length))
		hr = S_OK;
	else
		hr = HRESULT_FROM_WIN32(GetLastError());

	return hr;
}

HRESULT CWrappedService::GetSvcDesc (PWSTR pwzBuffer, LONG Length)
{
	return m_pService->GetServiceProperty(SvcHost::Description, pwzBuffer, Length);
}

HRESULT CWrappedService::GetSvcDisp (PWSTR pwzBuffer, LONG Length)
{
	return m_pService->GetServiceProperty(SvcHost::Display, pwzBuffer, Length);
}

DWORD CWrappedService::GetStartType (VOID)
{
	return m_pService->GetStartType();
}

HRESULT CWrappedService::GetDependencies (__out_ecount(cchMaxBuffer) PWSTR pwzBuffer, LONG cchMaxBuffer, __out LONG* pcchBuffer)
{
	HRESULT hr;
	CStaticStream stmBuffer(pwzBuffer, cchMaxBuffer);

	CheckIf(cchMaxBuffer < 2, STRSAFE_E_INSUFFICIENT_BUFFER);
	Check(m_pService->GetDependencies(&stmBuffer));

	if(stmBuffer.DataRemaining() == 0)
	{
		// The buffer must be double-zero terminated.
		pwzBuffer[0] = '\0';
		pwzBuffer[1] = '\0';
	}

	if(pcchBuffer)
		*pcchBuffer = stmBuffer.DataRemaining();

Cleanup:
	return hr;
}

BOOL CWrappedService::Connect (IServiceHost* lpHost, DWORD cArgs, PWSTR* ppwzArgs)
{
	BOOL fSuccess = FALSE;
	if(m_lpHost == NULL && lpHost)
	{
		m_lpHost = lpHost;
		m_lpHost->AddRef();

		if(m_pService->Initialize())
			fSuccess = TRUE;
		else
			Disconnect();
	}
	return fSuccess;
}

BOOL CWrappedService::StartService (VOID)
{
	return m_pService->Start();
}

VOID CWrappedService::StopService (VOID)
{
	m_pService->Stop();
}

VOID CWrappedService::Disconnect (VOID)
{
	m_pService->Uninitialize();
	SafeRelease(m_lpHost);
}

DWORD CWrappedService::AcceptServiceRequests (DWORD dwStatus)
{
	return m_pService->AcceptServiceRequests(dwStatus);
}

DWORD CWrappedService::HandleServiceRequest (DWORD dwControl, DWORD dwEventType, PVOID pvEventData)
{
	DWORD dwResult;
	if(!m_pService->HandleServiceRequest(dwControl, dwEventType, pvEventData, &dwResult))
		dwResult = (SERVICE_CONTROL_INTERROGATE == dwControl) ? NO_ERROR : ERROR_CALL_NOT_IMPLEMENTED;
	return dwResult;
}

HRESULT CWrappedService::PostInstall (PCWSTR pcwzServiceFilePath, PCWSTR pcwzDisplayName)
{
	return m_pService->PostInstall(pcwzServiceFilePath, pcwzDisplayName);
}

BOOL CWrappedService::QueryStopRequest (BOOL fShutdown, __inout DWORD* pdwResult)
{
	return m_pService->QueryStopRequest(fShutdown, pdwResult);
}

HRESULT CWrappedService::SetBasePath (PCWSTR pcwzBasePath)
{
	return m_pService->SetBasePath(pcwzBasePath);
}

HRESULT CWrappedService::ExecuteCommand (INT cArgs, __in_ecount(cArgs) PWSTR* ppwzArgs, INT idxArg, __out INT* pnSkip, __out BOOL* pfCanStartService)
{
	return m_pService->ExecuteCommand(cArgs, ppwzArgs, idxArg, pnSkip, pfCanStartService);
}
