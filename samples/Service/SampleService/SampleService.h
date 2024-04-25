#pragma once

#include "Library\Util\RefString.h"
#include "Library\Service\ServiceHostEx.h"

class CSampleService :
	public CBaseUnknown,
	public IService
{
protected:
	StringW m_strBasePath;
	IServiceHost* m_lpHost;
	HINSTANCE m_hInstance;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IService)
	END_UNK_MAP

public:
	CSampleService (HINSTANCE hInstance);
	~CSampleService ();

	static PCWSTR GetServiceRegKey (VOID);

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
	virtual DWORD AcceptServiceRequests (VOID);
	virtual DWORD HandleServiceRequest (DWORD dwControl, DWORD dwEventType, PVOID pvEventData);
	virtual HRESULT PostInstall (PCWSTR pcwzServiceFilePath, PCWSTR pcwzDisplayName);
	virtual BOOL QueryStopRequest (BOOL fShutdown, __inout DWORD* pdwResult);

	// To be effective, call SetBasePath() before initializing.
	HRESULT SetBasePath (PCWSTR pcwzBasePath);

	BOOL Initialize (VOID);
	VOID Uninitialize (VOID);
};
