#pragma once

#include "CoreDefs.h"
#include "ContainerTraits.h"

/////////////////////////////////////////////////////////////////////////////////////
// GENERIC ARRAY
/////////////////////////////////////////////////////////////////////////////////////

template<typename TItem, typename TTraits = DefaultTraits>
class TArray
{
	typedef struct {} HasDirectAccess;

protected:
	typedef typename TTraits::THeap THeap;
	typedef typename TTraits::TUsage TUsage;

	THeap m_Heap;
	TItem* m_pvItems;
	sysint m_cItems;

	__if_not_exists(TTraits::UseReallocate)
	{
		sysint m_cMaxItems;
	}

public:
	TArray () :
		m_pvItems(NULL),
		m_cItems(0)
	{
		__if_exists(m_cMaxItems)
		{
			m_cMaxItems = 0;
		}
	}

	TArray (THeap& heap) :
		m_Heap(heap),
		m_pvItems(NULL),
		m_cItems(0)
	{
		__if_exists(m_cMaxItems)
		{
			m_cMaxItems = 0;
		}
	}

	virtual ~TArray ()
	{
		if(m_pvItems)
		{
			m_Heap.release_storage(m_pvItems);
			m_cItems = 0;
		}
	}

	inline TItem& operator[] (sysint n)
	{
		Assert(0 <= n && n < m_cItems);
		return m_pvItems[n];
	}

	inline const TItem& operator[] (sysint n) const
	{
		Assert(0 <= n && n < m_cItems);
		return m_pvItems[n];
	}

	inline sysint Length (VOID) const
	{
		return m_cItems;
	}

	inline VOID GetData (TItem** ppvItems, sysint* pcItems)
	{
		*ppvItems = m_pvItems;
		*pcItems = m_cItems;
	}

	inline VOID GetData (const TItem** ppcvItems, sysint* pcItems) const
	{
		*ppcvItems = m_pvItems;
		*pcItems = m_cItems;
	}

	inline TItem* GetItemPtr (sysint nPosition)
	{
		Assert(0 <= nPosition && nPosition < m_cItems);
		return m_pvItems + nPosition;
	}

	inline const TItem* GetItemPtr (sysint nPosition) const
	{
		Assert(0 <= nPosition && nPosition < m_cItems);
		return m_pvItems + nPosition;
	}

	inline TItem* End (VOID)
	{
		return m_pvItems + m_cItems;
	}

	inline const TItem* End (VOID) const
	{
		return m_pvItems + m_cItems;
	}

	// Allocates 0 to nAllocItems elements in the array so that nAllocItems calls to
	// InsertAt() and Append() are guaranteed to complete without reallocating the array.
	HRESULT Reserve (sysint nAllocItems)
	{
		Assert(0 < nAllocItems);

		__if_not_exists(m_cMaxItems)
		{
			// The renew pattern doesn't support reservations.
			return S_FALSE;
		}
		__if_exists(m_cMaxItems)
		{
			HRESULT hr;
			sysint cFreeElements = m_cMaxItems - m_cItems;

			if(cFreeElements >= nAllocItems)
			{
				// The space is already available.
				hr = S_OK;
			}
			else
			{
				sysint cNewMax = m_cMaxItems + (nAllocItems - cFreeElements);
				TItem* pvNew;

				hr = m_Heap.allocate_storage(cNewMax, &pvNew);
				if(SUCCEEDED(hr))
				{
					CopyMemory(pvNew, m_pvItems, sizeof(TItem) * m_cItems);
					m_Heap.release_storage(m_pvItems);

					m_pvItems = pvNew;
					m_cMaxItems = cNewMax;
				}
			}

			return hr;
		}
	}

	// Reserves and commits the number of items specified by cCommitItems.  After this call,
	// these items have been allocated and filled with zeroes.
	HRESULT Commit (sysint cCommitItems)
	{
		HRESULT hr;

		__if_not_exists(m_cMaxItems)
		{
			TItem* pvNew;
			hr = m_Heap.reallocate_storage(m_pvItems, m_cItems + cCommitItems, &pvNew);
			if(SUCCEEDED(hr))
			{
				m_pvItems = pvNew;

				ZeroMemory(m_pvItems + m_cItems, sizeof(TItem) * cCommitItems);
				m_cItems += cCommitItems;
			}
		}
		__if_exists(m_cMaxItems)
		{
			hr = Reserve(cCommitItems);
			if(SUCCEEDED(hr))
			{
				ZeroMemory(m_pvItems + m_cItems, sizeof(TItem) * cCommitItems);
				m_cItems += cCommitItems;

				Assert(m_cItems <= m_cMaxItems);
			}
		}

		return hr;
	}

	HRESULT Compact (VOID)
	{
		HRESULT hr = S_FALSE;
		__if_exists(m_cMaxItems)
		{
			if(m_cItems < m_cMaxItems)
			{
				TItem* pvNew;
				hr = m_Heap.allocate_storage(m_cItems, &pvNew);
				if(SUCCEEDED(hr))
				{
					CopyMemory(pvNew, m_pvItems, sizeof(TItem) * m_cItems);
					m_Heap.release_storage(m_pvItems);

					m_pvItems = pvNew;
					m_cMaxItems = m_cItems;
				}
			}
		}
		return hr;
	}

	HRESULT InsertAt (const TItem* pvItem, sysint nInsert)
	{
		HRESULT hr;
		TItem* pvNew;

		Assert(0 <= nInsert && nInsert <= m_cItems);

		__if_not_exists(m_cMaxItems)
		{
			hr = m_Heap.reallocate_storage(m_pvItems, m_cItems + 1, &pvNew);
			if(SUCCEEDED(hr))
			{
				m_pvItems = pvNew;
				MoveMemory(m_pvItems + nInsert + 1,m_pvItems + nInsert,sizeof(TItem) * (m_cItems - nInsert));
				m_pvItems[nInsert] = *pvItem;
				m_cItems++;
			}
		}
		__if_exists(m_cMaxItems)
		{
			if(m_cItems < m_cMaxItems)
			{
				if(nInsert > m_cItems)
					nInsert = m_cItems;
				MoveMemory(m_pvItems + nInsert + 1, m_pvItems + nInsert, sizeof(TItem) * (m_cItems - nInsert));
				m_pvItems[nInsert] = *pvItem;
				m_cItems++;
				hr = S_OK;
			}
			else
			{
				sysint cNewMax;
				hr = TUsage::Grow(m_cMaxItems, cNewMax);
				if(SUCCEEDED(hr))
				{
					hr = m_Heap.allocate_storage(cNewMax, &pvNew);
					if(SUCCEEDED(hr))
					{
						CopyMemory(pvNew, m_pvItems, sizeof(TItem) * nInsert);
						CopyMemory(pvNew + nInsert + 1, m_pvItems + nInsert, sizeof(TItem) * (m_cItems - nInsert));

						m_Heap.release_storage(m_pvItems);
						m_pvItems = pvNew;
						m_cMaxItems = cNewMax;

						m_pvItems[nInsert] = *pvItem;
						m_cItems++;
					}
				}
			}
		}
		return hr;
	}

	inline HRESULT InsertAt (const TItem& vItem, sysint nInsert)
	{
		return InsertAt(&vItem, nInsert);
	}

	inline HRESULT Append (const TItem* pvItem)
	{
		return InsertAt(pvItem, m_cItems);
	}

	inline HRESULT Append (const TItem& vItem)
	{
		return InsertAt(&vItem, m_cItems);
	}

	HRESULT AppendNew (TItem* pvItem)
	{
		// This template method only works when TItem is a pointer type.
		HRESULT hr;
		*pvItem = __new TRemovePtr<TItem>::type;
		if(*pvItem)
		{
			hr = InsertAt(*pvItem, m_cItems);
			if(FAILED(hr))
			{
				__delete *pvItem;
				*pvItem = NULL;
			}
		}
		else
			hr = E_OUTOFMEMORY;
		return hr;
	}

	HRESULT InsertSlot (TItem** ppvItem, sysint nInsert)
	{
		// This template method reserves a slot of size sizeof(TItem).
		HRESULT hr;
		TItem vSlot;
		ZeroMemory(&vSlot, sizeof(vSlot));
		hr = InsertAt(&vSlot, nInsert);
		if(SUCCEEDED(hr))
			*ppvItem = m_pvItems + nInsert;
		return hr;
	}

	HRESULT AppendSlot (TItem** ppvItem)
	{
		return InsertSlot(ppvItem, m_cItems);
	}

	VOID MoveItem (sysint nDest, sysint nSrc)
	{
		Assert(nDest != nSrc && nDest < m_cItems && nSrc < m_cItems);

		TItem vItem = m_pvItems[nSrc];
		if(nDest > nSrc)
			MoveMemory(m_pvItems + nSrc, m_pvItems + nSrc + 1, (nDest - nSrc) * sizeof(TItem));
		else
			MoveMemory(m_pvItems + nDest + 1, m_pvItems + nDest, (nSrc - nDest) * sizeof(TItem));
		m_pvItems[nDest] = vItem;
	}

	VOID Remove (sysint nPosition, TItem* pvItem)
	{
		Assert(0 <= nPosition && nPosition < m_cItems);

		if(pvItem)
			*pvItem = m_pvItems[nPosition];

		MoveMemory(m_pvItems + nPosition,m_pvItems + nPosition + 1,(m_cItems - (nPosition + 1)) * sizeof(TItem));
		m_cItems--;

		__if_not_exists(m_cMaxItems)
		{
			SideAssertHr(m_Heap.reallocate_storage(m_pvItems, m_cItems, &m_pvItems));
		}
		__if_exists(m_cMaxItems)
		{
			sysint cNewMax;
			if(S_OK == TUsage::Shrink(m_cItems, m_cMaxItems, cNewMax))
			{
				TItem* pvNew;
				if(SUCCEEDED(m_Heap.allocate_storage(cNewMax, &pvNew)))
				{
					CopyMemory(pvNew, m_pvItems, m_cItems * sizeof(TItem));
					m_Heap.release_storage(m_pvItems);
					m_pvItems = pvNew;
					m_cMaxItems = cNewMax;
				}
			}
		}
	}

	HRESULT RemoveChecked (sysint nPosition, TItem* pvItem)
	{
		HRESULT hr = E_FAIL;
		if(nPosition >= 0 && nPosition < m_cItems)
		{
			Remove(nPosition, pvItem);
			hr = S_OK;
		}
		return hr;
	}

	HRESULT Truncate (sysint nPosition)
	{
		HRESULT hr;
		if(nPosition >= 0 && nPosition < m_cItems)
		{
			__if_not_exists(m_cMaxItems)
			{
				SideAssertHr(m_Heap.reallocate_storage(m_pvItems, nPosition, &m_pvItems));
			}
			__if_exists(m_cMaxItems)
			{
				m_cMaxItems = nPosition;
			}
			m_cItems = nPosition;
			hr = S_OK;
		}
		else
			hr = E_FAIL;
		return hr;
	}

	TItem* Detach (sysint* pcItems)
	{
		TItem* pvItems = m_pvItems;
		m_pvItems = NULL;

		Assert(pcItems);
		*pcItems = m_cItems;
		m_cItems = 0;
		__if_exists(m_cMaxItems)
		{
			m_cMaxItems = 0;
		}
		return pvItems;
	}

	inline VOID Clear (VOID)
	{
		m_cItems = 0;
	}

	HRESULT Resize (sysint nItems)
	{
		HRESULT hr;
		if(m_cItems > nItems)
			hr = Truncate(nItems);
		else if(m_cItems < nItems)
			hr = Commit(nItems - m_cItems);
		else
			hr = S_FALSE;
		return hr;
	}

	VOID Swap (TArray<TItem, TTraits>& aOther)
	{
		SwapData(m_pvItems, aOther.m_pvItems);
		SwapData(m_cItems, aOther.m_cItems);

		__if_not_exists(TTraits::UseReallocate)
		{
			SwapData(m_cMaxItems, aOther.m_cMaxItems);
		}
	}

	HRESULT Swap (sysint nItemA, sysint nItemB)
	{
		if(nItemA < m_cItems && nItemB < m_cItems)
		{
			SwapData(m_pvItems[nItemA], m_pvItems[nItemB]);
			return S_OK;
		}
		return HRESULT_FROM_WIN32(ERROR_INVALID_INDEX);
	}

	VOID DeleteAll (VOID)
	{
		while(0 < m_cItems)
			__delete m_pvItems[--m_cItems];
		Assert(0 == m_cItems);
	}

	bool IndexOf (const TItem& item, __out sysint& nPosition) const
	{
		for(sysint i = 0; i < m_cItems; i++)
		{
			if(m_pvItems[i] == item)
			{
				nPosition = i;
				return true;
			}
		}
		return false;
	}

	bool LastIndexOf (const TItem& item, __out sysint& nPosition) const
	{
		for(sysint i = m_cItems - 1; i >= 0; i--)
		{
			if(m_pvItems[i] == item)
			{
				nPosition = i;
				return true;
			}
		}
		return false;
	}
};

/////////////////////////////////////////////////////////////////////////////////////
// STATIC ARRAY
/////////////////////////////////////////////////////////////////////////////////////

template<typename TItem, typename TTraits = DefaultTraits>
class TStaticArray : public TArray<TItem, TTraits>
{
public:
	TStaticArray (TItem* pvItems, sysint cItems)
	{
		m_pvItems = pvItems;
		m_cItems = cItems;

		__if_exists(m_cMaxItems)
		{
			m_cMaxItems = cItems;
		}
	}

	~TStaticArray ()
	{
		m_pvItems = NULL;
		m_cItems = 0;
	}
};

/////////////////////////////////////////////////////////////////////////////////////
// ARRAY TRAITS
/////////////////////////////////////////////////////////////////////////////////////

template<typename TItem, typename TTraits = DefaultTraits>
struct TArrayTraits : public TTraits
{
	typedef TArray<TItem, TTraits> ArrayType;
};

template<typename TItem, typename TTraits = DefaultTraits>
struct TStaticTraits : public TTraits
{
	typedef TStaticArray<TItem, TTraits> ArrayType;
};
