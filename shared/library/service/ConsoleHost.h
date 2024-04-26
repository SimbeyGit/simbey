#pragma once

#include "..\Core\BaseUnknown.h"
#include "ServiceHostEx.h"

class CConsoleHost :
	public CBaseUnknown,
	public IServiceHostEx
{
protected:
	int m_cArgs;
	PTSTR* m_pptzArgs;

	IService* m_pService;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IServiceHost)
		UNK_INTERFACE(IServiceHostEx)
	END_UNK_MAP

public:
	CConsoleHost ();
	virtual ~CConsoleHost ();

	VOID AttachCommandLineArgs (int cArgs, PTSTR* pptzArgs);

	// IServiceStatus
	virtual BOOL NotifyStatus (DWORD dwStatus, DWORD dwWaitHint);

	// IServiceHost
	virtual HRESULT Install (IService* pService, PCTSTR pctzInstallPath);
	virtual HRESULT Uninstall (IService* pService);
	virtual HRESULT RunService (IService* pService);

	// IServiceHostEx
	virtual HRESULT AddDeviceNotification (PVOID pvNotificationFilter);
};
