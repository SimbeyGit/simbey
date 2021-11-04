#include <windows.h>
#include "..\Core\CoreDefs.h"
#include "ClassFactory.h"

CClassFactory::CClassFactory (QUERYCREATEIID pfnQueryCreateIID) :
	m_pfnQueryCreateIID(pfnQueryCreateIID)
{
	// Don't increment the object count for factories.
	// "It is legal to unload a DLL if the only objects that depend on that DLL which are still outstanding are class factories."
}

CClassFactory::~CClassFactory ()
{
	// Don't decrement the object count for factories.
}

// IClassFactory

HRESULT WINAPI CClassFactory::CreateInstance (IUnknown* pUnkOuter, REFIID riid, PVOID* ppvObject)
{
	HRESULT hr;

	CheckIf(NULL != pUnkOuter, CLASS_E_NOAGGREGATION);
	CheckIf(NULL == ppvObject, E_INVALIDARG);

	// No need to check, no need to jump.
	hr = m_pfnQueryCreateIID(riid, ppvObject);

Cleanup:
	return hr;
}

HRESULT WINAPI CClassFactory::LockServer (BOOL fLock)
{
	return CoLockObjectExternal(static_cast<IClassFactory*>(this), fLock, TRUE);
}

#if defined(USE_SIMBEY_CORE_API_REDIRECT) || defined(SIMBEYCORE_EXPORTS)
HRESULT WINAPI ScCreateClassFactory (__in_ecount(cDefs) const CLASS_FACTORY_OBJECT* pcfo, sysint cDefs, REFCLSID rclsid, REFIID riid, __deref_out PVOID* ppvObject)
#else
HRESULT WINAPI CreateClassFactory (__in_ecount(cDefs) const CLASS_FACTORY_OBJECT* pcfo, sysint cDefs, REFCLSID rclsid, REFIID riid, __deref_out PVOID* ppvObject)
#endif
{
	HRESULT hr;

	CheckIf(NULL == ppvObject, E_INVALIDARG);
	*ppvObject = NULL;

	hr = CLASS_E_CLASSNOTAVAILABLE;

	for(sysint i = 0; i < cDefs; i++)
	{
		if(rclsid == *pcfo[i].pclsid)
		{
			TStackRef<CClassFactory> srClassFactory;
			
			srClassFactory.Attach(__new CClassFactory(pcfo[i].pfnQueryCreateIID));
			CheckAlloc(srClassFactory);

			Check(srClassFactory->QueryInterface(riid, ppvObject));
			break;
		}
	}

Cleanup:
	return hr;
}

typedef HRESULT (__stdcall* PFNGETCLASSOBJECT)(REFCLSID, REFIID, LPVOID);

HRESULT WINAPI LoadClassFactory (const CLSID& clsid, const IID& iid, __deref_out PVOID* ppv, PCWSTR pcwzModule, __inout HMODULE* phModule)
{
	HRESULT hr = CoGetClassObject(clsid, CLSCTX_INPROC_SERVER, NULL, iid, ppv);
	if(FAILED(hr))
	{
		PFNGETCLASSOBJECT pfnGetClassObject;

		if(NULL == *phModule)
		{
			*phModule = LoadLibraryW(pcwzModule);
			CheckIfGetLastError(NULL == *phModule);
		}

		Check(TGetFunction(*phModule, "DllGetClassObject", &pfnGetClassObject));
		Check(pfnGetClassObject(clsid, iid, ppv));
	}

Cleanup:
	return hr;
}
