#pragma once

#include "Array.h"

/////////////////////////////////////////////////////////////////////////////////////
// SORTED ARRAY
/////////////////////////////////////////////////////////////////////////////////////

template<typename TItem>
class TSortedArray
{
protected:
	TArray<TItem> m_aItems;

public:
	TSortedArray ()
	{
	}

	virtual ~TSortedArray ()
	{
	}

	TItem& operator[](sysint n)
	{
		return m_aItems[n];
	}

	sysint Length (VOID) const
	{
		return m_aItems.Length();
	}

	VOID GetData (TItem** ppvItems, sysint* pcItems)
	{
		m_aItems.GetData(ppvItems, pcItems);
	}

	HRESULT InsertSorted (const TItem* pvItem)
	{
		HRESULT hr;
		sysint nInsert;

		if(Find(*pvItem, &nInsert))
			hr = E_FAIL;
		else
			hr = InsertAt(pvItem, nInsert);

		return hr;
	}

	HRESULT InsertSorted (const TItem& vItem)
	{
		HRESULT hr;
		sysint nInsert;

		if(Find(vItem, &nInsert))
			hr = E_FAIL;
		else
			hr = InsertAt(vItem, nInsert);

		return hr;
	}

	HRESULT InsertAt (const TItem* pvItem, sysint nInsert)
	{
		return m_aItems.InsertAt(pvItem, nInsert);
	}

	HRESULT InsertAt (const TItem& vItem, sysint nInsert)
	{
		return m_aItems.InsertAt(&vItem, nInsert);
	}

	HRESULT Remove (const TItem& vItem)
	{
		HRESULT hr;
		sysint nPosition;
		if(Find(vItem, &nPosition))
		{
			Delete(nPosition);
			hr = S_OK;
		}
		else
			hr = E_FAIL;
		return hr;
	}

	HRESULT RemoveByIndex (sysint nPosition, __out_opt TItem* pValue)
	{
		HRESULT hr;
		if(nPosition < m_aItems.Length())
		{
			if(pValue)
				*pValue = m_aItems[nPosition];
			Delete(nPosition);
			hr = S_OK;
		}
		else
			hr = E_INVALIDARG;
		return hr;
	}

	VOID Delete (sysint nPosition)
	{
		m_aItems.Remove(nPosition, NULL);
	}

	inline BOOL HasItem (const TItem& vItem)
	{
		sysint nPosition;
		return Find(vItem, &nPosition);
	}

	BOOL Find (const TItem& vItem, sysint* pnPosition)
	{
		TItem* pvItems;
		sysint iLeft = 0, iRight, iMiddle;

		m_aItems.GetData(&pvItems, &iRight);
		iRight--;

		while(iLeft <= iRight)
		{
			iMiddle = ((unsigned)(iLeft + iRight)) >> 1;
			if(pvItems[iMiddle] < vItem)
				iLeft = iMiddle + 1;
			else if(pvItems[iMiddle] > vItem)
				iRight = iMiddle - 1;
			else
			{
				*pnPosition = iMiddle;	// The item has been found at this position
				return TRUE;
			}
		}
		*pnPosition = iLeft;			// This is the point where the item should be inserted
		return FALSE;
	}

	inline VOID Clear (VOID)
	{
		m_aItems.Clear();
	}
};