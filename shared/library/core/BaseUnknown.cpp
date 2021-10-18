#include <windows.h>
#include "BaseUnknown.h"

///////////////////////////////////////////////////////////////////////////////
// CBaseUnknown
///////////////////////////////////////////////////////////////////////////////

CBaseUnknown::CBaseUnknown () :
	m_cRef(1)
{
}

CBaseUnknown::~CBaseUnknown ()
{
	Assert(0 == m_cRef);
}

STDMETHODIMP CBaseUnknown::QueryInterface (REFIID riid, __deref_out PVOID* ppvObj)
{
	HRESULT hr = E_POINTER;
	if(ppvObj)
	{
		if(IsEqualIID(riid, IID_IUnknown))
		{
			*ppvObj = static_cast<IUnknown*>(this);
			InterlockedIncrement((LONG*)&m_cRef);
			hr = S_OK;
		}
		else
			hr = VirtualQueryInterface(riid, ppvObj);
	}
	return hr;
}

STDMETHODIMP_(ULONG) CBaseUnknown::AddRef ()
{
	Assert(0 < m_cRef);	// The object already had at least one reference.

	return (ULONG)InterlockedIncrement((volatile LONG*)&m_cRef);
}

STDMETHODIMP_(ULONG) CBaseUnknown::Release ()
{
	Assert(0 < m_cRef);	// The object already had at least one reference.

	ULONG c = (ULONG)InterlockedDecrement((volatile LONG*)&m_cRef);
	if(0 == c)
		__delete this;

	return c;
}

///////////////////////////////////////////////////////////////////////////////
// CAggregable
///////////////////////////////////////////////////////////////////////////////

#define NestedClassToParentOffset(parentClass, member)	((UINT_PTR)&(((parentClass*)0)->member))
#define GetParentClass(parentClass, member, nestedPtr)	((parentClass*)(((PBYTE)nestedPtr) - NestedClassToParentOffset(parentClass, member)))

CAggregable::CDefaultController::CDefaultController()
{
	m_cRef = 1;
}

CAggregable::CDefaultController::~CDefaultController()
{
	Assert(0 == m_cRef);
}

STDMETHODIMP CAggregable::CDefaultController::QueryInterface (REFIID riid, __deref_out PVOID* ppvObj)
{
	HRESULT hr = E_POINTER;
	if(ppvObj)
	{
		// The default handler handles IUnknown, but all other interface
		// requests are forwarded to the CAggregable derivation.

		if(IsEqualIID(riid, IID_IUnknown))
		{
			*ppvObj = static_cast<IUnknown*>(this);
			InterlockedIncrement((LONG*)&m_cRef);
			hr = S_OK;
		}
		else
		{
			CAggregable* pParent = GetParentClass(CAggregable, m_cUnkOuter, this);
			hr = pParent->VirtualQueryInterface(riid, ppvObj);
		}
	}
	return hr;
}

STDMETHODIMP_(ULONG) CAggregable::CDefaultController::AddRef()
{
	Assert(0 < m_cRef);	// The object already had at least one reference.

	return (ULONG)InterlockedIncrement((volatile LONG*)&m_cRef);
}

STDMETHODIMP_(ULONG) CAggregable::CDefaultController::Release()
{
	Assert(0 < m_cRef);	// The object already had at least one reference.

	ULONG c = (ULONG)InterlockedDecrement((volatile LONG*)&m_cRef);
	if(0 == c)
	{
		CAggregable* pParent = GetParentClass(CAggregable, m_cUnkOuter, this);
		__delete pParent;
	}

	return c;
}

CAggregable::CAggregable (__in_opt IUnknown* pUnkOuter)
{
	if(pUnkOuter)
		m_pUnkOuter = pUnkOuter;
	else
		m_pUnkOuter = &m_cUnkOuter;
}

CAggregable::~CAggregable ()
{
}

STDMETHODIMP CAggregable::QueryInterface (REFIID riid, __deref_out PVOID* ppvObj)
{
	// Forward top-level IUnknown::QueryInterface() calls to the controlling IUnknown.
	return m_pUnkOuter->QueryInterface(riid, ppvObj);
}

STDMETHODIMP_(ULONG) CAggregable::AddRef ()
{
	// Forward top-level IUnknown::AddRef() calls to the controlling IUnknown.
	return m_pUnkOuter->AddRef();
}

STDMETHODIMP_(ULONG) CAggregable::Release ()
{
	// Forward top-level IUnknown::Release() calls to the controlling IUnknown.
	return m_pUnkOuter->Release();
}

ULONG CAggregable::InnerAddRef()
{
	return GetInner()->AddRef();
}

ULONG CAggregable::InnerRelease()
{
	return GetInner()->Release();
}
