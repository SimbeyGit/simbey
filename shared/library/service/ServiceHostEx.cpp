#include <windows.h>
#include "Library\Core\Assert.h"
#include "Library\Core\Check.h"
#include "Library\Core\Pointers.h"
#include "Library\Core\StringCore.h"
#include "ServiceHostEx.h"

CServiceHostEx::CServiceHostEx ()
{
	InitializeCriticalSection(&m_cs);
}

CServiceHostEx::~CServiceHostEx ()
{
	DeleteCriticalSection(&m_cs);
}

// IServiceHostEx

HRESULT CServiceHostEx::AddDeviceNotification (PVOID pvNotificationFilter)
{
	HRESULT hr;
	HDEVNOTIFY hDevNotify = NULL;

	EnterCriticalSection(&m_cs);

	CheckIf(NULL == m_hServiceStatus, E_UNEXPECTED);

	hDevNotify = RegisterDeviceNotification(m_hServiceStatus, pvNotificationFilter, DEVICE_NOTIFY_SERVICE_HANDLE);
	CheckIfGetLastError(NULL == hDevNotify);

	Check(m_aDevNotify.Append(hDevNotify));
	hDevNotify = NULL;

Cleanup:
	LeaveCriticalSection(&m_cs);

	if(hDevNotify)
		UnregisterDeviceNotification(hDevNotify);

	return hr;
}

VOID CServiceHostEx::OnServiceDisconnected (IService* /*pService*/)
{
	EnterCriticalSection(&m_cs);

	while(0 < m_aDevNotify.Length())
	{
		HDEVNOTIFY hDevNotify;
		m_aDevNotify.Remove(m_aDevNotify.Length() - 1, &hDevNotify);
		UnregisterDeviceNotification(hDevNotify);
	}

	LeaveCriticalSection(&m_cs);
}

HRESULT CServiceHostEx::SetServiceDescription (__inout SC_HANDLE* phService, PCTSTR /*pctzName*/, PCTSTR pctzDescription)
{
	// This version of SetServiceDescription() uses ChangeServiceConfig2(), which is only available beginning with Windows XP and Windows Server 2003.

	HRESULT hr;
	SERVICE_DESCRIPTION sd;

	CheckIf(NULL == phService || NULL == *phService, E_INVALIDARG);

	// Attempt to set the service description.
	sd.lpDescription = const_cast<PTSTR>(pctzDescription);
	CheckIfGetLastError(!ChangeServiceConfig2(*phService, SERVICE_CONFIG_DESCRIPTION, &sd));

	hr = S_OK;

Cleanup:
	return hr;
}

HRESULT CServiceHostEx::RegisterService (PCTSTR pctzSvcName, __out SERVICE_STATUS_HANDLE* phServiceStatus)
{
	// This version of RegisterService() uses RegisterServiceCtrlHandlerEx(), which is only available beginning with Windows XP and Windows Server 2003.

	HRESULT hr;

	*phServiceStatus = ::RegisterServiceCtrlHandlerEx(pctzSvcName, CServiceHost::_ServiceHandlerEx, static_cast<CServiceHost*>(this));
	CheckIfGetLastError(NULL == *phServiceStatus);

	hr = S_OK;

Cleanup:
	return hr;
}
