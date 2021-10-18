#pragma once

#include <sal.h>
#include "CoreDefs.h"

// CVirtualUnknown serves as the first common base class for implementing IUnknown.
// CBaseUnknown and CAggregable inherit from CVirtualUnknown.
class CVirtualUnknown : public IUnknown
{
public:
	virtual HRESULT VirtualQueryInterface (REFIID riid, __deref_out PVOID* ppvObj) = 0;
	virtual HRESULT FinalConstruct (VOID) { return S_OK; }
};

// CBaseUnknown is the common base class for implementing IUnknown.  Instead of directly
// inheriting from IUnknown, inherit from CBaseUnknown (if aggregation is not needed).
class CBaseUnknown : public CVirtualUnknown
{
private:
	volatile ULONG m_cRef;

protected:
	CBaseUnknown ();
	virtual ~CBaseUnknown ();

public:
	IUnknown* GetControllingUnknown (VOID) { return this; }

	STDMETHOD(QueryInterface)(REFIID riid, PVOID* ppvObj);
	STDMETHOD_(ULONG, AddRef)();
	STDMETHOD_(ULONG, Release)();
};

// Like CBaseUnknown, CAggregable simplifies IUnknown reference counting details but also
// supports object aggregation.
// For more information about aggregation, please read:
// http://msdn.microsoft.com/en-us/library/ms686558(VS.85).aspx
class CAggregable : public CVirtualUnknown
{
private:
	class CDefaultController : public IUnknown
	{
	private:
		volatile ULONG m_cRef;

	public:
		CDefaultController ();
		virtual ~CDefaultController ();

		// IUnknown Members
		STDMETHOD(QueryInterface)(REFIID riid, __deref_out PVOID* ppvObj);
		STDMETHOD_(ULONG, AddRef)();
		STDMETHOD_(ULONG, Release)();
	};

	friend CDefaultController;

	CDefaultController m_cUnkOuter;
	IUnknown* m_pUnkOuter;

protected:
	CAggregable (__in_opt IUnknown* pUnkOuter);
	virtual ~CAggregable ();

public:
	// Even if the object is aggregated, m_cUnkOuter always
	// manages its reference count.  This allows InnerAddRef()
	// and InnerRelease() to function as expected.
	IUnknown* GetInner (VOID) { return &m_cUnkOuter; }

	// m_pUnkOuter is always the controlling IUnknown,
	// regardless of whether the object is aggregated.
	IUnknown* GetControllingUnknown (VOID) { return m_pUnkOuter; }

	STDMETHOD(QueryInterface)(REFIID riid, PVOID* ppvObj);
	STDMETHOD_(ULONG, AddRef)();
	STDMETHOD_(ULONG, Release)();

	// When managing aggregated objects, use InnerAddRef() and InnerRelease().
	ULONG InnerAddRef (VOID);
	ULONG InnerRelease (VOID);
};

template <typename TFinalClass>
class TBaseUnknown : public CBaseUnknown
{
public:
	static HRESULT CreateInstance (__deref_out TFinalClass** ppObj)
	{
		HRESULT hr;
		Assert(ppObj);
		*ppObj = __new TFinalClass;
		if(*ppObj)
		{
			hr = (*ppObj)->FinalConstruct();
			if(FAILED(hr))
				(*ppObj)->Release();
		}
		else
			hr = E_OUTOFMEMORY;
		return hr;
	}

	static HRESULT WINAPI QueryCreateIID (REFIID riid, __deref_out PVOID* ppvObject)
	{
		TFinalClass* pObject;
		HRESULT hr = CreateInstance(&pObject);
		if(SUCCEEDED(hr))
		{
			hr = pObject->QueryInterface(riid, ppvObject);
			pObject->Release();
		}
		return hr;
	}
};

template <typename TFinalClass>
class TAggregable : public CAggregable
{
public:
	TAggregable (__in_opt IUnknown* pOuter) :
		CAggregable(pOuter)
	{
	}

	static HRESULT CreateInstance (__deref_out TFinalClass** ppObj, __in_opt IUnknown* pOuter)
	{
		HRESULT hr;
		Assert(ppObj);
		*ppObj = __new TFinalClass(pOuter);
		if(*ppObj)
		{
			hr = (*ppObj)->FinalConstruct();
			if(FAILED(hr))
				(*ppObj)->Release();
		}
		else
			hr = E_OUTOFMEMORY;
		return hr;
	}

	static HRESULT WINAPI QueryCreateIID (REFIID riid, __deref_out PVOID* ppvObject, __in_opt IUnknown* pOuter)
	{
		HRESULT hr;
		if(NULL == pOuter || IsEqualIID(riid, IID_IUnknown))
		{
			TFinalClass* pObject;
			hr = CreateInstance(&pObject, pOuter);
			if(SUCCEEDED(hr))
			{
				hr = pObject->QueryInterface(riid, ppvObject);
				pObject->Release();
			}
		}
		else
			hr = CLASS_E_NOAGGREGATION;
		return hr;
	}
};

// An object inheriting from multiple IUnknown sources (CBaseUnknown plus
// any IUnknown derived interfaces) must implement the IUnknown methods.  The
// compiler cannot otherwise disambiguate between inheritances.  To hide this
// detail, specify IMP_UNKNOWN(base) on your object.
#define	IMP_UNKNOWN(base)		STDMETHODIMP QueryInterface (REFIID riid, __deref_out PVOID* ppvObj) { return base::QueryInterface(riid, ppvObj); } \
								STDMETHODIMP_(ULONG) AddRef () { return base::AddRef(); } \
								STDMETHODIMP_(ULONG) Release () { return base::Release(); }

#define	IMP_BASE_UNKNOWN		IMP_UNKNOWN(CBaseUnknown)

// Use the EMPTY_UNK_MAP macro when the class does not expose any interfaces.
#define	EMPTY_UNK_MAP				virtual HRESULT VirtualQueryInterface (REFIID riid, __deref_out PVOID* ppvObj) \
									{ \
										return E_NOINTERFACE; \
									}

// Use the BEGIN_UNK_MAP and END_UNK_MAP macros to define VirtualQueryInterface().
// This is the internal method used by CAggregable to properly aggregate interfaces
// implemented by your object.
#define	BEGIN_UNK_MAP				virtual HRESULT VirtualQueryInterface (REFIID riid, __deref_out PVOID* ppvObj) \
									{ \
										HRESULT hr = S_OK; \

// Use UNK_INTERFACE to expose an interface your object implements.
#define	UNK_INTERFACE(x)				if(__uuidof(x) == riid) \
										{ \
											*ppvObj = static_cast<x*>(this); \
											static_cast<CVirtualUnknown*>(this)->AddRef(); \
											goto exit; \
										}

#define	UNK_INTERFACE_IID(x)			if(IID_##x == riid) \
										{ \
											*ppvObj = static_cast<x*>(this); \
											static_cast<CVirtualUnknown*>(this)->AddRef(); \
											goto exit; \
										}

#define	UNK_INTERFACE_PATH(x, y)		if(__uuidof(x) == riid) \
										{ \
											*ppvObj = static_cast<x*>(static_cast<y*>(this)); \
											static_cast<CVirtualUnknown*>(this)->AddRef(); \
											goto exit; \
										}

// If your object aggregates contained objects, use UNK_AGGREGATE to expose the
// interfaces implemented by your object's contained objects.
#define	UNK_AGGREGATE(x)				if(SUCCEEDED(x->GetInner()->QueryInterface(riid, ppvObj))) \
										{ \
											goto exit; \
										}

// If your object aggregates contained objects, use UNK_AGGREGATE_IID to expose
// specific interfaces.  Internally, UNK_AGGREGATE is used after checking riid.
#define	UNK_AGGREGATE_IID(iid, x)		if(iid == riid) \
										{ \
											UNK_AGGREGATE(x) \
										}

// If you need a non-standard way to expose reference counted objects that
// aren't or can't be aggregated, then use UNK_EXPOSE_OBJECT() to create a
// one-way binding.
#define	UNK_EXPOSE_OBJECT(iid, obj)		if(iid == riid) \
										{ \
											*ppvObj = obj; \
											obj->AddRef(); \
											goto exit; \
										}

// UNK_CHAIN_TO must be used last.  There can be multiple chains, but they must
// all be grouped together and last in the map.
#define	UNK_CHAIN_TO(parent)			if(SUCCEEDED(parent::VirtualQueryInterface(riid, ppvObj))) \
										{ \
											goto exit; \
										}

// END_UNK_MAP closes the VirtualQueryInterface() definition.
#define	END_UNK_MAP						hr = E_NOINTERFACE; \
									exit: \
										return hr; \
									}

#define	BEGIN_SERVICE_MAP		virtual HRESULT WINAPI QueryService (REFGUID guidService, REFIID riid, PVOID* ppvObj) \
								{ \
									HRESULT hr = S_OK; \
									if(ppvObj) \
									{

#define	PROVIDE_SERVICE(obj)			if(__uuidof(*(obj)) == riid) \
										{ \
											*ppvObj = obj; \
											obj->AddRef(); \
											goto exit; \
										}

#define	END_SERVICE_MAP					hr = E_NOINTERFACE; \
									} \
									else \
										hr = E_POINTER; \
								exit: \
									return hr; \
								}
