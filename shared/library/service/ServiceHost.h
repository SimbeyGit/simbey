#ifndef	_H_SERVICEHOST
#define	_H_SERVICEHOST

// CServiceHost acts as a base service class when used with CServiceHostEx or as a stand-alone
// service class for services targeting Windows 2000.

#include "IServiceHost.h"
#include "..\Core\BaseUnknown.h"

class CServiceHost :
	public CBaseUnknown,
	public IServiceHost
{
protected:
	static CServiceHost* m_lpHost;

	IService* m_lpService;

	SERVICE_TABLE_ENTRY* m_lpstList;		// Only one entry currently supported
	SERVICE_STATUS_HANDLE m_hServiceStatus;
	SERVICE_STATUS m_status;

	HANDLE m_hStopService;
	HANDLE m_hStopWaitObject;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IServiceStatus)
		UNK_INTERFACE(IServiceHost)
	END_UNK_MAP

public:
	CServiceHost ();
	virtual ~CServiceHost ();

	// IServiceStatus
	virtual BOOL NotifyStatus (DWORD dwStatus, DWORD dwWaitHint);

	// IServiceHost
	virtual HRESULT Install (IService* pService, PCTSTR pctzInstallPath);
	virtual HRESULT Uninstall (IService* pService);
	virtual HRESULT RunService (IService* pService);

protected:
	VOID Start (DWORD cArgs, PTSTR* pptzArgs);
	VOID Stop (VOID);

	VOID ServiceMain (DWORD cArgs, PTSTR* pptzArgs);
	DWORD ServiceHandler (DWORD dwControl, DWORD dwEventType, PVOID pvEventData);

	virtual VOID OnServiceDisconnected (IService* pService);
	virtual HRESULT SetServiceDescription (__inout SC_HANDLE* phService, PCTSTR pctzName, PCTSTR pctzDescription);
	virtual HRESULT RegisterService (PCTSTR pctzSvcName, __out SERVICE_STATUS_HANDLE* phServiceStatus);

protected:
	static DWORD WINAPI _ServiceHandlerEx (DWORD dwControl, DWORD dwEventType, PVOID pvEventData, PVOID pvContext);

private:
	static VOID WINAPI _StopService (PVOID pParam, BOOLEAN fTimeout);
	static VOID WINAPI _ServiceMain (DWORD cArgs, PTSTR* pptzArgs);
	static VOID WINAPI _ServiceHandler (DWORD dwOpCode);

	static HRESULT _GetServiceName (IService* pService, __deref_out PTSTR* pptzName);
};

#endif