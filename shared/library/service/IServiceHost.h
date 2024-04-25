#pragma once

interface IServiceHost;

interface __declspec(uuid("84F9C6AE-7CDB-4b9c-91B1-46108BF68DE5")) IService : public IUnknown
{
	virtual HRESULT SetInstallPath (PCTSTR pctzInstallPath, INT cchInstallPath) = 0;
	virtual HRESULT GetSvcName (PTSTR ptzBuffer, LONG Length) = 0;
	virtual HRESULT GetSvcPath (PTSTR ptzBuffer, LONG Length) = 0;
	virtual HRESULT GetSvcDesc (PTSTR ptzBuffer, LONG Length) = 0;
	virtual HRESULT GetSvcDisp (PTSTR ptzBuffer, LONG Length) = 0;
	virtual DWORD GetStartType (VOID) = 0;
	virtual HRESULT GetDependencies (__out_ecount(cchMaxBuffer) PTSTR ptzBuffer, LONG cchMaxBuffer, __out LONG* pcchBuffer) = 0;
	virtual BOOL Connect (IServiceHost* lpHost, DWORD cArgs, PTSTR* pptzArgs) = 0;
	virtual BOOL StartService (VOID) = 0;
	virtual VOID StopService (VOID) = 0;
	virtual VOID Disconnect (VOID) = 0;
	virtual DWORD AcceptServiceRequests (VOID) = 0;
	virtual DWORD HandleServiceRequest (DWORD dwControl, DWORD dwEventType, PVOID pvEventData) = 0;
	virtual HRESULT PostInstall (PCTSTR pctzServiceFilePath, PCTSTR pctzDisplayName) = 0;
	virtual BOOL QueryStopRequest (BOOL fShutdown, __inout DWORD* pdwResult) = 0;
};

interface __declspec(uuid("ED299507-7406-4a0f-8D6F-2D845B3FEDFF")) IServiceHost : public IUnknown
{
	virtual HRESULT Install (IService* pService, PCTSTR pctzInstallPath) = 0;
	virtual HRESULT Uninstall (IService* pService) = 0;
	virtual HRESULT RunService (IService* pService) = 0;
};

interface __declspec(uuid("8C6A38BA-120A-4d10-A846-76AD45040C7B")) IServiceHostEx : public IServiceHost
{
	virtual HRESULT AddDeviceNotification (PVOID pvNotificationFilter) = 0;
};
