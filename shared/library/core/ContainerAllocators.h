#pragma once

#include "Assert.h"
#include "SafeMath.h"

struct crt_heap
{
	template <typename T>
	HRESULT allocate_storage (sysint cItems, __deref_out_opt T** ppMem)
	{
		HRESULT hr;
		sysint cb;

		Assert(NULL != ppMem);

		hr = HrSafeMultSysInt(cItems, static_cast<sysint>(sizeof(T)), &cb);
		if(SUCCEEDED(hr))
		{
			if(0 < cItems)
			{
				*ppMem = reinterpret_cast<T*>(__malloc(cb));
				if(NULL == *ppMem)
					hr = E_OUTOFMEMORY;
			}
			else
				*ppMem = NULL;
		}

		return hr;
	}

	template <typename T>
	HRESULT reallocate_storage (T* pMem, sysint cItems, __deref_out_opt T** ppMem)
	{
		HRESULT hr;
		sysint cb;

		Assert(NULL != ppMem);

		hr = HrSafeMultSysInt(cItems, static_cast<sysint>(sizeof(T)), &cb);
		if(SUCCEEDED(hr))
		{
			*ppMem = reinterpret_cast<T*>(__realloc(pMem, cb));
			if(0 < cItems && NULL == *ppMem)
				hr = E_OUTOFMEMORY;
		}

		return hr;
	}

	template<typename T>
	VOID release_storage (T* pMem)
	{
		__free(pMem);
	}
};

struct crt_byte_heap
{
	HRESULT allocate_storage (sysint cbMem, __deref_out_opt PBYTE* ppMem)
	{
		HRESULT hr = S_OK;

		Assert(NULL != ppMem);

		if(0 < cbMem)
		{
			*ppMem = reinterpret_cast<PBYTE>(__malloc(cbMem));
			if(NULL == *ppMem)
				hr = E_OUTOFMEMORY;
		}
		else
			*ppMem = NULL;

		return hr;
	}

	HRESULT reallocate_storage (PBYTE pMem, sysint cbMem, __deref_out_opt PBYTE* ppMem)
	{
		HRESULT hr = S_OK;

		Assert(NULL != ppMem);

		*ppMem = reinterpret_cast<PBYTE>(__realloc(pMem, cbMem));
		if(0 < cbMem && NULL == *ppMem)
			hr = E_OUTOFMEMORY;

		return hr;
	}

	VOID release_storage (PBYTE pMem)
	{
		__free(pMem);
	}
};

class win_heap
{
private:
	HANDLE m_hHeap;

public:
	win_heap (const win_heap& from)
	{
		m_hHeap = from.m_hHeap;
	}

	win_heap (HANDLE hHeap)
	{
		m_hHeap = hHeap;
	}

	template<typename T>
	HRESULT allocate_storage (sysint cItems, __deref_out_opt T** ppMem)
	{
		HRESULT hr;
		sysint cb;

		Assert(NULL != ppMem);

		hr = HrSafeMultSysInt(cItems, static_cast<sysint>(sizeof(T)), &cb);
		if(SUCCEEDED(hr))
		{
			if(0 < cItems)
			{
				*ppMem = reinterpret_cast<T*>(HeapAlloc(m_hHeap, 0, cb));
				if(NULL == *ppMem)
					hr = E_OUTOFMEMORY;
			}
			else
				*ppMem = NULL;
		}

		return hr;
	}

	template <typename T>
	HRESULT reallocate_storage (T* pMem, sysint cItems, __deref_out_opt T** ppMem)
	{
		HRESULT hr;
		sysint cb;

		Assert(NULL != ppMem);

		hr = HrSafeMultSysInt(cItems, static_cast<sysint>(sizeof(T)), &cb);
		if(SUCCEEDED(hr))
		{
			if(NULL == pMem)
				*ppMem = reinterpret_cast<T*>(HeapAlloc(m_hHeap, 0, cb));
			else
				*ppMem = reinterpret_cast<T*>(HeapReAlloc(m_hHeap, 0, pMem, cb));
			if(0 < cItems && NULL == *ppMem)
				hr = E_OUTOFMEMORY;
		}

		return hr;
	}

	template<typename T>
	VOID release_storage (T* pMem)
	{
		if(pMem)
			HeapFree(m_hHeap, 0, pMem);
	}

	operator HANDLE()
	{   
		return m_hHeap;
	}

	void operator= (HANDLE h)
	{
		m_hHeap = h;
	}
};
