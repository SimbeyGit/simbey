#pragma once

#include "Array.h"

/////////////////////////////////////////////////////////////////////////////////////
// REF ARRAY
/////////////////////////////////////////////////////////////////////////////////////

template<typename TRefItem, typename TTraits = DefaultTraits>
class TRefArray
{
	__if_not_exists(TRefItem::AddRef)
	{
		static_assert(false, "TRefItem::AddRef() MUST exist!  TRefArray only works with reference counted objects.");
	}
	__if_not_exists(TRefItem::Release)
	{
		static_assert(false, "TRefItem::Release() MUST exist!  TRefArray only works with reference counted objects.");
	}

	typedef TRefItem* TItem;
	typedef TArray<TItem> ArrayType;
	typedef typename TTraits::THeap THeap;

protected:
	ArrayType m_aItems;

public:
	TRefArray () {}
	TRefArray (THeap& heap) : m_aItems(heap) {}
	~TRefArray () { Clear(); }

	inline TItem& operator[] (sysint n)
	{
		return m_aItems[n];
	}

	HRESULT Get (sysint nPosition, TItem* pvItem)
	{
		HRESULT hr = E_FAIL;
		if(0 <= nPosition && nPosition < m_aItems.Length())
		{
			*pvItem = m_aItems[nPosition];
			if(*pvItem)
			{
				(*pvItem)->AddRef();
			}
			hr = S_OK;
		}
		return hr;
	}

	HRESULT Set (sysint nPosition, const TItem& vItem)
	{
		HRESULT hr = E_FAIL;
		if(0 <= nPosition && nPosition < m_aItems.Length())
		{
			ReplaceInterface(m_aItems[nPosition], vItem);
			hr = S_OK;
		}
		return hr;
	}

	inline sysint Length (VOID)
	{
		return m_aItems.Length();
	}

	inline void GetData (TItem** ppvItems, sysint* pcItems)
	{
		m_aItems.GetData(ppvItems, pcItems);
	}

	HRESULT Reserve (sysint nAllocItems)
	{
		return m_aItems.Reserve(nAllocItems);
	}

	HRESULT Commit (sysint cCommitItems)
	{
		return m_aItems.Commit(cCommitItems);
	}

	HRESULT Compact (VOID)
	{
		return m_aItems.Compact();
	}

	HRESULT InsertAt (const TItem* pvItem, sysint nInsert)
	{
		HRESULT hr = m_aItems.InsertAt(pvItem, nInsert);
		if(SUCCEEDED(hr) && *pvItem)
		{
			(*pvItem)->AddRef();
		}
		return hr;
	}

	inline HRESULT InsertAt (const TItem& vItem, sysint nInsert)
	{
		HRESULT hr = m_aItems.InsertAt(vItem, nInsert);
		if(SUCCEEDED(hr) && vItem)
		{
			vItem->AddRef();
		}
		return hr;
	}

	inline HRESULT Append (const TItem* pvItem)
	{
		HRESULT hr = m_aItems.Append(pvItem);
		if(SUCCEEDED(hr) && *pvItem)
		{
			(*pvItem)->AddRef();
		}
		return hr;
	}

	inline HRESULT Append (const TItem& vItem)
	{
		HRESULT hr = m_aItems.Append(vItem);
		if(SUCCEEDED(hr) && vItem)
		{
			vItem->AddRef();
		}
		return hr;
	}

	void MoveItem (sysint nDest, sysint nSrc)
	{
		m_aItems.MoveItem(nDest, nSrc);
	}

	VOID Remove (sysint nPosition, TItem* pvItem)
	{
		if(pvItem)
		{
			// Ownership of the reference is being transferred to the caller.
			m_aItems.Remove(nPosition, pvItem);
		}
		else
		{
			TItem vItem;
			m_aItems.Remove(nPosition, &vItem);
			SafeRelease(vItem);
		}
	}

	TItem* Detach (sysint* pcItems)
	{
		return m_aItems.Detach(pcItems);
	}

	inline VOID Clear (VOID)
	{
		for(sysint i = 0; i < m_aItems.Length(); i++)
		{
			TItem vItem = m_aItems[i];
			if(vItem)
			{
				vItem->Release();
			}
		}
		m_aItems.Clear();
	}
};
