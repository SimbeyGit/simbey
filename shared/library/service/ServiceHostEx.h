#pragma once

// CServiceHostEx is the service implementation for Windows XP and Windows Server 2003 and beyond

#include "..\Core\Array.h"
#include "ServiceHost.h"

class CServiceHostEx :
	public CServiceHost,
	public IServiceHostEx
{
protected:
	CRITICAL_SECTION m_cs;

	TArray<HDEVNOTIFY> m_aDevNotify;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IServiceHostEx)
		UNK_CHAIN_TO(CServiceHost)
	END_UNK_MAP

public:
	CServiceHostEx ();
	virtual ~CServiceHostEx ();

	// IServiceHostEx
	virtual HRESULT Install (IService* pService, PCTSTR pctzInstallPath) { return CServiceHost::Install(pService, pctzInstallPath); }
	virtual HRESULT Uninstall (IService* pService) { return CServiceHost::Uninstall(pService); }
	virtual HRESULT RunService (IService* pService) { return CServiceHost::RunService(pService); }

	// IServiceHostEx
	virtual HRESULT AddDeviceNotification (PVOID pvNotificationFilter);

protected:
	virtual VOID OnServiceDisconnected (IService* pService);
	virtual HRESULT SetServiceDescription (__inout SC_HANDLE* phService, PCTSTR pctzName, PCTSTR pctzDescription);
	virtual HRESULT RegisterService (PCTSTR pctzSvcName, __out SERVICE_STATUS_HANDLE* phServiceStatus);
};
