#include <windows.h>
#include "Core\CoreDefs.h"
#include "EnumVariant.h"

CEnumVariant::CEnumVariant ()
{
	m_cRef = 1;

	m_lpvList = NULL;
	m_cList = 0;
}

CEnumVariant::~CEnumVariant ()
{
	Clear();
}

VOID CEnumVariant::AttachList (VARIANT* lpvList, ULONG cList)
{
	Clear();

	m_lpvList = lpvList;
	m_cList = cList;
}

// IUnknown

HRESULT WINAPI CEnumVariant::QueryInterface (REFIID iid, LPVOID* lplpvObject)
{
	HRESULT hr = E_INVALIDARG;
	if(lplpvObject)
	{
		if(iid == IID_IEnumVARIANT)
			*lplpvObject = (IEnumVARIANT*)this;
		else if(iid == IID_IUnknown)
			*lplpvObject = (IUnknown*)this;
		else
		{
			hr = E_NOINTERFACE;
			goto exit;
		}
		AddRef();
		hr = S_OK;
	}
exit:
	return hr;
}

ULONG WINAPI CEnumVariant::AddRef (VOID)
{
	return (ULONG)InterlockedIncrement((LONG*)&m_cRef);
}

ULONG WINAPI CEnumVariant::Release (VOID)
{
	ULONG c = (ULONG)InterlockedDecrement((LONG*)&m_cRef);
	if(c == 0)
		__delete this;
	return c;
}

// IEnumVARIANT

HRESULT WINAPI CEnumVariant::Next (ULONG celt, VARIANT* rgVar, ULONG* lpCeltFetched)
{
	HRESULT hr = S_OK;
	ULONG cFetched = 0;

	for(ULONG i = 0; i < celt; i++)
		VariantInit(rgVar + i);

	for(ULONG i = 0; i < celt; i++)
	{
		if(i >= m_iReadPtr)
		{
			hr = S_FALSE;
			break;
		}

		hr = VariantCopy(rgVar + i,m_lpvList + m_iReadPtr + i);
		if(FAILED(hr))
			break;

		cFetched++;
		m_iReadPtr++;
	}

	if(lpCeltFetched)
		*lpCeltFetched = cFetched;

	return hr;
}

HRESULT WINAPI CEnumVariant::Skip (ULONG celt)
{
	HRESULT hr = S_OK;
	m_iReadPtr += celt;
	if(m_iReadPtr > m_cList)
	{
		m_iReadPtr = m_cList;
		hr = S_FALSE;
	}
	return hr;
}

HRESULT WINAPI CEnumVariant::Reset (VOID)
{
	m_iReadPtr = 0;
	return S_OK;
}

HRESULT WINAPI CEnumVariant::Clone (IEnumVARIANT** lplpEnum)
{
	UNREFERENCED_PARAMETER(lplpEnum);

	HRESULT hr = S_OK;
	CEnumVariant* lpClone = __new CEnumVariant;
	if(lpClone)
	{
		VARIANT* lpvList = NULL;
		if(m_cList > 0)
		{
			lpvList = __new VARIANT[m_cList];
			if(lpvList)
			{
				ULONG i;
				for(i = 0; i < m_cList; i++)
				{
					VariantInit(lpvList + i);
					hr = VariantCopy(lpvList + i,m_lpvList + i);
					if(FAILED(hr))
					{
						ULONG n;
						for(n = 0; n < i; n++)
							VariantClear(lpvList + n);
						__delete_array lpvList;
						lpvList = NULL;
						break;
					}
				}
			}
			else
				hr = E_OUTOFMEMORY;
		}
		if(SUCCEEDED(hr))
		{
			lpClone->m_lpvList = lpvList;
			lpClone->m_cList = m_cList;
			lpClone->m_iReadPtr = m_iReadPtr;
			*lplpEnum = lpClone;
		}
		else
			lpClone->Release();
	}
	else
		hr = E_OUTOFMEMORY;

	return hr;
}

VOID CEnumVariant::Clear (VOID)
{
	if(m_lpvList)
	{
		for(ULONG i = 0; i < m_cList; i++)
			VariantClear(m_lpvList + i);
		__delete_array m_lpvList;
		m_lpvList = NULL;
		m_cList = 0;
		m_iReadPtr = 0;
	}
}