#pragma once

#include "..\Core\BaseUnknown.h"

typedef HRESULT (WINAPI* QUERYCREATEIID)(REFIID, PVOID*);

struct CLASS_FACTORY_OBJECT
{
	const CLSID* pclsid;
	QUERYCREATEIID pfnQueryCreateIID;
};

class CClassFactory :
	public CBaseUnknown,
	public IClassFactory
{
private:
	QUERYCREATEIID m_pfnQueryCreateIID;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IClassFactory)
	END_UNK_MAP

	CClassFactory (QUERYCREATEIID pfnQueryCreateIID);
	~CClassFactory ();

public:
	// IClassFactory
	virtual HRESULT WINAPI CreateInstance (IUnknown* pUnkOuter, REFIID riid, PVOID* ppvObject);
	virtual HRESULT WINAPI LockServer (BOOL fLock);
};

#define	BEGIN_GET_CLASS_OBJECT \
	HRESULT WINAPI DllGetClassObject (REFCLSID rclsid, REFIID riid, __deref_out PVOID* ppvObject) \
	{ \
		static const CLASS_FACTORY_OBJECT cfo[] = \
		{ \

#define	EXPORT_FACTORY(clsid, class) \
			{ &clsid, class::QueryCreateIID },

#if defined(USE_SIMBEY_CORE_API_REDIRECT) || defined(SIMBEYCORE_EXPORTS)
#define	END_GET_CLASS_OBJECT \
		}; \
		return ScCreateClassFactory(cfo, ARRAYSIZE(cfo), rclsid, riid, ppvObject); \
	}

HRESULT WINAPI ScCreateClassFactory (__in_ecount(cDefs) const CLASS_FACTORY_OBJECT* pcfo, sysint cDefs, REFCLSID rclsid, REFIID riid, __deref_out PVOID* ppvObject);
#else
#define	END_GET_CLASS_OBJECT \
		}; \
		return CreateClassFactory(cfo, ARRAYSIZE(cfo), rclsid, riid, ppvObject); \
	}

HRESULT WINAPI CreateClassFactory (__in_ecount(cDefs) const CLASS_FACTORY_OBJECT* pcfo, sysint cDefs, REFCLSID rclsid, REFIID riid, __deref_out PVOID* ppvObject);
#endif

HRESULT WINAPI LoadClassFactory (const CLSID& clsid, const IID& iid, __deref_out PVOID* ppv, PCWSTR pcwzModule, __inout HMODULE* phModule);
