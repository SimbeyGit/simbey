#pragma once

#include "Library\Service\ServiceHostEx.h"
#include "Published\SvcHost.h"

class CWrappedService :
	public CBaseUnknown,
	public IService
{
protected:
	IHostedService* m_pService;
	IServiceHost* m_lpHost;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IService)
	END_UNK_MAP

public:
	CWrappedService (IHostedService* pService);
	~CWrappedService ();

	virtual HRESULT SetInstallPath (PCWSTR pcwzInstallPath, INT cchInstallPath);
	virtual HRESULT GetSvcName (PWSTR pwzBuffer, LONG Length);
	virtual HRESULT GetSvcPath (PWSTR pwzBuffer, LONG Length);
	virtual HRESULT GetSvcDesc (PWSTR pwzBuffer, LONG Length);
	virtual HRESULT GetSvcDisp (PWSTR pwzBuffer, LONG Length);
	virtual DWORD GetStartType (VOID);
	virtual HRESULT GetDependencies (__out_ecount(cchMaxBuffer) PWSTR pwzBuffer, LONG cchMaxBuffer, __out LONG* pcchBuffer);
	virtual BOOL Connect (IServiceHost* lpHost, DWORD cArgs, PWSTR* ppwzArgs);
	virtual BOOL StartService (VOID);
	virtual VOID StopService (VOID);
	virtual VOID Disconnect (VOID);
	virtual DWORD AcceptServiceRequests (DWORD dwStatus);
	virtual DWORD HandleServiceRequest (DWORD dwControl, DWORD dwEventType, PVOID pvEventData);
	virtual HRESULT PostInstall (PCWSTR pcwzServiceFilePath, PCWSTR pcwzDisplayName);
	virtual BOOL QueryStopRequest (BOOL fShutdown, __inout DWORD* pdwResult);

	// To be effective, call SetBasePath() before initializing.
	HRESULT SetBasePath (PCWSTR pcwzBasePath);

	HRESULT ExecuteCommand (INT cArgs, __in_ecount(cArgs) PWSTR* ppwzArgs, INT idxArg, __out INT* pnSkip, __out BOOL* pfCanStartService);
};
