#pragma once

#include "Library\Core\BaseUnknown.h"
#include "Published\SvcHost.h"

class CSampleHostedSvc :
	public CBaseUnknown,
	public CHostedServiceImpl
{
protected:
	IServiceStatus* m_pStatus;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IHostedService)
	END_UNK_MAP

public:
	static HRESULT WINAPI CreateService (IServiceStatus* pStatus, __deref_out IHostedService** ppService);

public:
	CSampleHostedSvc (IServiceStatus* pStatus);
	~CSampleHostedSvc ();

	static PCWSTR WINAPI _GetServiceRegKey (VOID);

	// IHostedService
	virtual PCWSTR WINAPI GetServiceRegKey (VOID);
	virtual HRESULT WINAPI GetServiceProperty (SvcHost::Property eProperty, __out_ecount(cchMaxBuffer) PWSTR pwzBuffer, INT cchMaxBuffer);
	virtual HRESULT WINAPI SetBasePath (PCWSTR pcwzBasePath);
	virtual BOOL WINAPI Initialize (VOID);
	virtual BOOL WINAPI Start (VOID);
	virtual VOID WINAPI Stop (VOID);
	virtual VOID WINAPI Uninitialize (VOID);
};
