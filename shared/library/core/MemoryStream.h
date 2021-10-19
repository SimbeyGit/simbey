#pragma once

#include "BaseStream.h"
#include "ContainerAllocators.h"

#define	MEMSTM_ALLOC_BLOCK				1024 // 1K

struct default_stream_heap
{
	HRESULT allocate_storage (sysint cbMem, PBYTE* ppMem)
	{
		HRESULT hr = S_OK;

		Assert(NULL != ppMem);

		if(0 < cbMem)
		{
			*ppMem = __new BYTE[cbMem];
			if(NULL == *ppMem)
				hr = E_OUTOFMEMORY;
		}
		else
			*ppMem = NULL;

		return hr;
	}

	VOID release_storage (PBYTE pMem)
	{
		__delete_array pMem;
	}
};

template <typename THeap>
class TMemoryStream : public CBaseStream
{
protected:
	THeap m_Heap;

public:
	TMemoryStream ()
	{
		m_pBuffer = NULL;
		m_cbMaxBuffer = 0;
	}

	TMemoryStream (THeap& heap) :
		m_Heap(heap)
	{
		m_pBuffer = NULL;
		m_cbMaxBuffer = 0;
	}

	virtual ~TMemoryStream ()
	{
		m_Heap.release_storage(m_pBuffer);
	}

	LPBYTE Detach (__out ULONG* lpcbData)
	{
		Assert(lpcbData);

		LPBYTE lpPtr = m_pBuffer;
		*lpcbData = m_cbData;

		m_pBuffer = NULL;
		m_cbMaxBuffer = 0;
		m_cbData = 0;
		m_iReadPtr = 0;

		return lpPtr;
	}

	template <typename T, typename C>
	T* TDetach (__out C* pcItems)
	{
		ULONG cbData;
		T* ptItems = reinterpret_cast<T*>(Detach(&cbData));
		*pcItems = static_cast<C>(cbData / sizeof(T));
		return ptItems;
	}

	HRESULT WriteAdvance (__deref_out_bcount(cbAdvance) PBYTE* ppWritePtr, ULONG cbAdvance)
	{
		HRESULT hr = E_INVALIDARG;
		if(ppWritePtr && cbAdvance > 0)
		{
			if(m_cbData + cbAdvance <= m_cbMaxBuffer)
			{
				*ppWritePtr = m_pBuffer + m_cbData;
				m_cbData += cbAdvance;
				hr = S_OK;
			}
			else if(m_iReadPtr > 0)
			{
				MoveMemory(m_pBuffer,m_pBuffer + m_iReadPtr,m_cbData - m_iReadPtr);
				m_cbData -= m_iReadPtr;
				m_iReadPtr = 0;
				hr = WriteAdvance(ppWritePtr, cbAdvance);
			}
			else
			{
				hr = Reserve((0 < m_cbMaxBuffer) ? m_cbMaxBuffer : MEMSTM_ALLOC_BLOCK);
				if(SUCCEEDED(hr))
					hr = WriteAdvance(ppWritePtr,cbAdvance);
			}
		}
		return hr;
	}

	HRESULT Reserve (ULONG cbMemory)
	{
		ULONG cbNewMax;
		HRESULT hr = HrSafeAdd(m_cbMaxBuffer, cbMemory, &cbNewMax);
		if(SUCCEEDED(hr))
		{
			PBYTE pbNew;
			__if_exists(THeap::reallocate_storage)
			{
				hr = m_Heap.reallocate_storage(m_pBuffer, cbNewMax, &pbNew);
				if(SUCCEEDED(hr))
				{
					m_pBuffer = pbNew;
					m_cbMaxBuffer = cbNewMax;
				}
			}
			__if_not_exists(THeap::reallocate_storage)
			{
				hr = m_Heap.allocate_storage(cbNewMax, &pbNew);
				if(SUCCEEDED(hr))
				{
					if(m_pBuffer)
					{
						CopyMemory(pbNew, m_pBuffer, m_cbData);
						m_Heap.release_storage(m_pBuffer);
					}
					m_pBuffer = pbNew;
					m_cbMaxBuffer = cbNewMax;
				}
			}
		}
		return hr;
	}

	HRESULT Compact (VOID)
	{
		HRESULT hr = S_FALSE;
		ULONG cbRemaining = DataRemaining();

		if(cbRemaining < m_cbMaxBuffer)
		{
			PBYTE pbMem;
			__if_exists(THeap::reallocate_storage)
			{
				if(0 < m_iReadPtr)
				{
					MoveMemory(m_pBuffer, m_pBuffer + m_iReadPtr, cbRemaining);
					m_cbData = cbRemaining;
					m_iReadPtr = 0;
				}
				hr = m_Heap.reallocate_storage(m_pBuffer, cbRemaining, &pbMem);
				if(SUCCEEDED(hr))
				{
					m_pBuffer = pbMem;
					m_cbMaxBuffer = cbRemaining;
				}
			}
			__if_not_exists(THeap::reallocate_storage)
			{
				hr = m_Heap.allocate_storage(cbRemaining, &pbMem);
				if(SUCCEEDED(hr))
				{
					CopyMemory(pbMem, m_pBuffer + m_iReadPtr, cbRemaining);
					m_Heap.release_storage(m_pBuffer);
					m_pBuffer = pbMem;
					m_cbMaxBuffer = cbRemaining;
					m_cbData = cbRemaining;
					m_iReadPtr = 0;
				}
			}
		}

		return hr;
	}

	template <typename T>
	inline HRESULT TWriteAdvance (__deref_out_ecount(cAdvanceSlots) T** pptWritePtr, ULONG cAdvanceSlots)
	{
		return WriteAdvance(reinterpret_cast<PBYTE*>(pptWritePtr), cAdvanceSlots * sizeof(T));
	}

protected:
	virtual HRESULT GrowAndWrite (VOID const* lpcv, ULONG cb, ULONG* lpcbWritten)
	{
		HRESULT hr = Reserve(0 == m_cbMaxBuffer ? MEMSTM_ALLOC_BLOCK : m_cbMaxBuffer);
		if(SUCCEEDED(hr))
			hr = Write(lpcv, cb, lpcbWritten);
		return hr;
	}
};

class CMemoryStream :
	public TMemoryStream<default_stream_heap>
{
public:
	template <typename T>
	static HRESULT TCreateAndAllocateStream (__deref_out ISequentialStream** ppStream, __deref_out_ecount(cAdvanceSlots) T** pptWritePtr, ULONG cAdvanceSlots)
	{
		HRESULT hr;
		CMemoryStream* pStream = __new CMemoryStream;
		if(pStream)
		{
			hr = pStream->TWriteAdvance(pptWritePtr, cAdvanceSlots);
			if(SUCCEEDED(hr))
				*ppStream = pStream;
			else
				pStream->Release();
		}
		else
			hr = E_OUTOFMEMORY;
		return hr;
	}

	VOID Swap (CMemoryStream& other)
	{
		SwapData(m_pBuffer, other.m_pBuffer);
		SwapData(m_cbMaxBuffer, other.m_cbMaxBuffer);
		SwapData(m_cbData, other.m_cbData);
		SwapData(m_iReadPtr, other.m_iReadPtr);
	}
};
