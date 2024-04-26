#include <windows.h>
#include "Library\Core\Assert.h"
#include "Library\Core\Check.h"
#include "Library\Core\Pointers.h"
#include "Library\Core\StringCore.h"
#include "ServiceHost.h"

CServiceHost* CServiceHost::m_lpHost = NULL;

CServiceHost::CServiceHost ()
{
	m_lpHost = this;
	m_lpService = NULL;

	m_lpstList = NULL;
	ZeroMemory(&m_status,sizeof(SERVICE_STATUS));
	m_hServiceStatus = NULL;

	m_hStopService = NULL;
	m_hStopWaitObject = NULL;
}

CServiceHost::~CServiceHost ()
{
	Assert(NULL == m_hStopWaitObject);
	Assert(NULL == m_hStopService);

	Assert(m_lpstList == NULL);

	if(m_lpService)
		m_lpService->Release();

	m_lpHost = NULL;
}

// IServiceStatus

BOOL CServiceHost::NotifyStatus (DWORD dwStatus, DWORD dwWaitHint)
{
	m_status.dwCurrentState = dwStatus;
	m_status.dwWaitHint = dwWaitHint;

	switch(dwStatus)
	{
	case SERVICE_STOPPED:
	case SERVICE_START_PENDING:
	case SERVICE_STOP_PENDING:
		m_status.dwControlsAccepted = 0;
		break;
	default:
		m_status.dwControlsAccepted = m_lpService->AcceptServiceRequests(dwStatus);
		break;
	}

	return ::SetServiceStatus(m_hServiceStatus, &m_status);
}

// IServiceHost

HRESULT CServiceHost::Install (IService* pService, PCTSTR pctzInstallPath)
{
	HRESULT hr;
	TCHAR tzName[256];
	TCHAR tzDisplay[512];
	TCHAR tzPath[MAX_PATH];
	union
	{
		TCHAR tzDependencies[1024];
		TCHAR tzDesc[1024];
	};
	LONG cchDependencies;
	PTSTR ptzDependenciesPtr = tzDependencies;
	PTSTR ptzPathPtr = tzPath;
	BOOL fDeletePathPtr = FALSE;
	SC_HANDLE hService = NULL;
	SC_HANDLE hServices;

	hServices = OpenSCManager(NULL,SERVICES_ACTIVE_DATABASE,SC_MANAGER_ALL_ACCESS);
	CheckIfGetLastError(NULL == hServices);

	Check(pService->GetSvcName(tzName, ARRAYSIZE(tzName)));
	Check(pService->GetSvcDisp(tzDisplay, ARRAYSIZE(tzDisplay)));
	Check(pService->GetSvcPath(tzPath, ARRAYSIZE(tzPath)));

	Check(pService->GetDependencies(tzDependencies, ARRAYSIZE(tzDependencies), &cchDependencies));
	if(tzDependencies[0] == '\0' && tzDependencies[1] == '\0')
		ptzDependenciesPtr = NULL;

	// If the path to the service executable contains any spaces, put the path in quotes.
	if(TStrChr(tzPath, static_cast<TCHAR>(' ')))
	{
		INT cch = TStrLenAssert(tzPath);

		ptzPathPtr = __new TCHAR[cch + 3];
		CheckAlloc(ptzPathPtr);

		ptzPathPtr[0] = '"';
		CopyMemory(ptzPathPtr + 1, tzPath, cch * sizeof(TCHAR));
		ptzPathPtr[cch + 1] = '"';
		ptzPathPtr[cch + 2] = 0;
		fDeletePathPtr = TRUE;
	}

	if(NULL == pctzInstallPath || '\0' == pctzInstallPath)
	{
		TCHAR tzCurrentDir[MAX_PATH];
		INT cchCurrentDir = GetCurrentDirectory(ARRAYSIZE(tzCurrentDir), tzCurrentDir);
		CheckIfGetLastError(0 == cchCurrentDir);
		Check(pService->SetInstallPath(tzCurrentDir, cchCurrentDir));
	}
	else
		Check(pService->SetInstallPath(pctzInstallPath, TStrLenAssert(pctzInstallPath)));

	hService = CreateService(hServices,
		tzName,
		tzDisplay,
		SERVICE_ALL_ACCESS,
		SERVICE_WIN32_OWN_PROCESS,
		pService->GetStartType(),
		SERVICE_ERROR_NORMAL,
		ptzPathPtr,
		NULL,
		NULL,
		ptzDependenciesPtr,
		NULL,
		NULL);
	CheckIfGetLastError(NULL == hService);

	Check(pService->GetSvcDesc(tzDesc, ARRAYSIZE(tzDesc)));

	// SetServiceDescription() may close the service handle.
	SetServiceDescription(&hService, tzName, tzDesc);

	SideAssertHr(pService->PostInstall(tzPath, tzDisplay));

Cleanup:
	if(hService)
		CloseServiceHandle(hService);

	if(fDeletePathPtr)
		__delete_array ptzPathPtr;

	if(hServices)
		CloseServiceHandle(hServices);

	return hr;
}

HRESULT CServiceHost::Uninstall (IService* pService)
{
	HRESULT hr;
	TCHAR tzName[256];
	SC_HANDLE hService = NULL;
	SC_HANDLE hServices;

	hServices = OpenSCManager(NULL,SERVICES_ACTIVE_DATABASE,SC_MANAGER_CONNECT);
	CheckIfGetLastError(NULL == hServices);

	Check(pService->GetSvcName(tzName, ARRAYSIZE(tzName)));
	hService = OpenService(hServices, tzName, SERVICE_ALL_ACCESS);
	CheckIfGetLastError(NULL == hService);

	CheckIfGetLastError(!DeleteService(hService));

Cleanup:
	if(hService)
		CloseServiceHandle(hService);

	if(hServices)
		CloseServiceHandle(hServices);

	return hr;
}

HRESULT CServiceHost::RunService (IService* lpService)
{
	HRESULT hr;

	CheckIf(NULL != m_lpService, E_UNEXPECTED);

	m_lpstList = __new SERVICE_TABLE_ENTRY[2];
	CheckAlloc(m_lpstList);

	ZeroMemory(m_lpstList, sizeof(SERVICE_TABLE_ENTRY) * 2);

	Check(_GetServiceName(lpService, &m_lpstList[0].lpServiceName));
	m_lpstList[0].lpServiceProc = _ServiceMain;

	m_lpService = lpService;
	m_lpService->AddRef();

	m_hStopService = CreateEvent(NULL, TRUE, FALSE, NULL);
	CheckIfGetLastError(NULL == m_hStopService);

	// StartServiceCtrlDispatcher() doesn't return until the service is stopped.
	CheckIfGetLastError(!StartServiceCtrlDispatcher(m_lpstList));

Cleanup:
	m_hServiceStatus = NULL;

	if(m_hStopWaitObject)
	{
		UnregisterWait(m_hStopWaitObject);
		m_hStopWaitObject = NULL;
	}

	SafeCloseHandle(m_hStopService);

	SafeRelease(m_lpService);

	if(m_lpstList)
	{
		__delete_array m_lpstList[0].lpServiceName;
		__delete_array m_lpstList;
		m_lpstList = NULL;
	}

	return hr;
}

VOID CServiceHost::Start (DWORD cArgs, PTSTR* pptzArgs)
{
	BOOL fStarted = FALSE;

	NotifyStatus(SERVICE_START_PENDING, 0);

	if(RegisterWaitForSingleObject(&m_hStopWaitObject, m_hStopService, _StopService, this, INFINITE, WT_EXECUTELONGFUNCTION | WT_EXECUTEONLYONCE))
	{
		if(m_lpService->Connect(this, cArgs, pptzArgs))
		{
			if(m_lpService->StartService())
			{
				NotifyStatus(SERVICE_RUNNING, 0);
				fStarted = TRUE;
			}
			else
			{
				NotifyStatus(SERVICE_STOP_PENDING, 0);
				m_lpService->Disconnect();
			}
		}
	}

	if(!fStarted)
		NotifyStatus(SERVICE_STOPPED, 0);
}

VOID CServiceHost::Stop (VOID)
{
	// When the service has been fully running, allow time (in ms) for the
	// service to stop/shutdown
	NotifyStatus(SERVICE_STOP_PENDING, 5000);

	m_lpService->StopService();
	m_lpService->Disconnect();

	OnServiceDisconnected(m_lpService);

	// The StartServiceCtrlDispatcher() call will return after SERVICE_STOPPED is sent.
	NotifyStatus(SERVICE_STOPPED, 0);
}

VOID CServiceHost::ServiceMain (DWORD cArgs, PTSTR* pptzArgs)
{
	m_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	m_status.dwCurrentState = SERVICE_STOPPED;
	m_status.dwControlsAccepted = 0;
	m_status.dwWin32ExitCode = 0;
	m_status.dwServiceSpecificExitCode = 0;
	m_status.dwCheckPoint = 0;
	m_status.dwWaitHint = 0;

	if(SUCCEEDED(RegisterService(m_lpstList[0].lpServiceName, &m_hServiceStatus)))
		Start(cArgs, pptzArgs);
}

DWORD CServiceHost::ServiceHandler (DWORD dwControl, DWORD dwEventType, PVOID pvEventData)
{
	DWORD dwResult = NO_ERROR;

	switch(dwControl)
	{
	case SERVICE_CONTROL_STOP:
	case SERVICE_CONTROL_SHUTDOWN:
		if(m_lpService->QueryStopRequest(SERVICE_CONTROL_SHUTDOWN == dwControl, &dwResult))
			SetEvent(m_hStopService);
		break;

	default:
		dwResult = m_lpService->HandleServiceRequest(dwControl, dwEventType, pvEventData);
		break;
	}

	return dwResult;
}

VOID CServiceHost::OnServiceDisconnected (IService* /*pService*/)
{
}

HRESULT CServiceHost::SetServiceDescription (__inout SC_HANDLE* phService, PCTSTR pctzName, PCTSTR pctzDescription)
{
	HRESULT hr;
	HKEY hKey = NULL;
	HKEY hServiceKey = NULL;

	CheckIf(NULL == pctzName || NULL == pctzDescription, E_INVALIDARG);

	// Immediately close the service handle to force all registry updates to be flushed.
	if(phService && *phService)
	{
		CloseServiceHandle(*phService);
		*phService = NULL;
	}

	CheckWin32Error(RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("SYSTEM\\CurrentControlSet\\Services"), 0, KEY_ALL_ACCESS, &hKey));
	CheckWin32Error(RegOpenKeyEx(hKey, pctzName, 0, KEY_ALL_ACCESS, &hServiceKey));
	CheckWin32Error(RegSetValueEx(hServiceKey, TEXT("Description"), 0, REG_SZ, (LPCBYTE)pctzDescription, (TStrLenAssert(pctzDescription) + 1) * sizeof(TCHAR)));

Cleanup:
	if(hServiceKey)
		RegCloseKey(hServiceKey);
	if(hKey)
		RegCloseKey(hKey);

	return hr;
}

HRESULT CServiceHost::RegisterService (PCTSTR pctzSvcName, __out SERVICE_STATUS_HANDLE* phServiceStatus)
{
	HRESULT hr;

	*phServiceStatus = ::RegisterServiceCtrlHandler(pctzSvcName, _ServiceHandler);
	CheckIfGetLastError(NULL == *phServiceStatus);

	hr = S_OK;

Cleanup:
	return hr;
}

DWORD WINAPI CServiceHost::_ServiceHandlerEx (DWORD dwControl, DWORD dwEventType, PVOID pvEventData, PVOID pvContext)
{
	CServiceHost* pHost = reinterpret_cast<CServiceHost*>(pvContext);
	return pHost->ServiceHandler(dwControl, dwEventType, pvEventData);
}

VOID WINAPI CServiceHost::_StopService (PVOID pParam, BOOLEAN /*fTimeout*/)
{
	CServiceHost* pHost = reinterpret_cast<CServiceHost*>(pParam);
	pHost->Stop();
}

VOID WINAPI CServiceHost::_ServiceMain (DWORD cArgs, PTSTR* pptzArgs)
{
	if(CServiceHost::m_lpHost)
		CServiceHost::m_lpHost->ServiceMain(cArgs, pptzArgs);
}

VOID WINAPI CServiceHost::_ServiceHandler (DWORD dwOpCode)
{
	if(CServiceHost::m_lpHost)
		CServiceHost::m_lpHost->ServiceHandler(dwOpCode, 0, NULL);
}

HRESULT CServiceHost::_GetServiceName (IService* pService, __deref_out PTSTR* pptzName)
{
	HRESULT hr;
	TCHAR tzName[256];

	Check(pService->GetSvcName(tzName, ARRAYSIZE(tzName)));
	Check(TDuplicateStringAssert(tzName, pptzName, NULL));

Cleanup:
	return hr;
}
