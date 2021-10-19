#pragma once

#include "Array.h"
#include "..\Util\RString.h"

/////////////////////////////////////////////////////////////////////////////////////
// RSTRING MAP
/////////////////////////////////////////////////////////////////////////////////////

template<typename TValue>
class TRStrMap
{
public:
	typedef struct
	{
		RSTRING rstrName;
		TValue value;
	} NAMED_MAP_ENTRY;

protected:
	TArray<NAMED_MAP_ENTRY> m_Store;
	HRESULT (WINAPI* m_pfnCompare)(RSTRING, RSTRING, INT*);

public:
	TRStrMap (HRESULT(WINAPI* pfnCompare)(RSTRING, RSTRING, INT*)) { m_pfnCompare = pfnCompare; }
	TRStrMap () { m_pfnCompare = RStrCompareIRStr; }
	~TRStrMap () { Clear(); }

	TValue* operator[] (RSTRING rstrKey)
	{
		TValue* p;
		sysint nPosition;
		HRESULT hrFind = BinaryFind(rstrKey, &nPosition);
		if(S_OK == hrFind)
			p = &m_Store[nPosition].value;
		else
		{
			Assert(S_FALSE == hrFind);

			NAMED_MAP_ENTRY Item;
			Item.rstrName = rstrKey;
			ZeroMemory(&Item.value, sizeof(Item.value));

			if(SUCCEEDED(m_Store.InsertAt(&Item, nPosition)))
			{
				RStrAddRef(Item.rstrName);
				p = &m_Store[nPosition].value;
			}
			else
				p = NULL;
		}
		return p;
	}

	TValue* operator[] (sysint n)
	{
		return &m_Store[n].value;
	}

	inline sysint Length (VOID) const
	{
		return m_Store.Length();
	}

	inline RSTRING GetKey (sysint n)
	{
		return m_Store[n].rstrName;
	}

	HRESULT GetKeyChecked (sysint n, RSTRING* prstrKey)
	{
		HRESULT hr = HRESULT_FROM_WIN32(ERROR_INVALID_INDEX);

		Assert(prstrKey);

		if(0 <= n && n < m_Store.Length())
		{
			RStrSet(*prstrKey, m_Store[n].rstrName);
			hr = S_OK;
		}

		return hr;
	}

	HRESULT GetKeyPtrChecked (RSTRING rstrName, __deref_out RSTRING* prstrKey)
	{
		sysint nPosition;
		HRESULT hr = BinaryFind(rstrName, &nPosition);
		if(S_OK == hr)
			RStrSet(*prstrKey, m_Store[nPosition].rstrName);
		else if(S_FALSE == hr)
			hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
		return hr;
	}

	inline TValue* GetValuePtr (sysint n)
	{
		return &m_Store[n].value;
	}

	inline HRESULT GetValuePtrChecked (sysint n, TValue** ppValue)
	{
		HRESULT hr = HRESULT_FROM_WIN32(ERROR_INVALID_INDEX);

		Assert(ppValue);

		if(0 <= n && n < m_Store.Length())
		{
			*ppValue = &m_Store[n].value;
			hr = S_OK;
		}
		return hr;
	}

	inline HRESULT GetValueChecked (sysint n, __out TValue* pValue)
	{
		HRESULT hr = HRESULT_FROM_WIN32(ERROR_INVALID_INDEX);

		Assert(pValue);

		if(0 <= n && n < m_Store.Length())
		{
			*pValue = m_Store[n].value;
			hr = S_OK;
		}
		return hr;
	}

	HRESULT GetKeyAndValue (sysint n, __out RSTRING* prstrName, TValue* pValue)
	{
		HRESULT hr = HRESULT_FROM_WIN32(ERROR_INVALID_INDEX);

		Assert(prstrName && pValue);

		if(0 <= n && n < m_Store.Length())
		{
			// Don't add a reference to the key.
			*prstrName = m_Store[n].rstrName;
			*pValue = m_Store[n].value;
			hr = S_OK;
		}

		return hr;
	}

	HRESULT GetKeyAndValue (sysint n, __out RSTRING* prstrName, TValue* const pValue) const
	{
		HRESULT hr = HRESULT_FROM_WIN32(ERROR_INVALID_INDEX);

		Assert(prstrName && pValue);

		if(0 <= n && n < m_Store.Length())
		{
			// Don't add a reference to the key.
			*prstrName = m_Store[n].rstrName;
			*pValue = m_Store[n].value;
			hr = S_OK;
		}

		return hr;
	}

	HRESULT GetKeyAndValuePtr (sysint n, __out RSTRING* prstrName, __out TValue** ppValue)
	{
		HRESULT hr = HRESULT_FROM_WIN32(ERROR_INVALID_INDEX);

		Assert(prstrName && ppValue);

		if(0 <= n && n < m_Store.Length())
		{
			// Don't add a reference to the key.
			*prstrName = m_Store[n].rstrName;
			*ppValue = &m_Store[n].value;
			hr = S_OK;
		}

		return hr;
	}

	inline HRESULT Reserve (sysint nAllocItems)
	{
		return m_Store.Reserve(nAllocItems);
	}

	inline HRESULT Compact (VOID)
	{
		return m_Store.Compact();
	}

	HRESULT Add (RSTRING rstrName, const TValue& value)
	{
		HRESULT hr;
		sysint nPosition;

		Assert(NULL != rstrName);	// rstrName must not be NULL

		hr = BinaryFind(rstrName, &nPosition);
		if(S_FALSE == hr)
			hr = AddByIndex(nPosition, rstrName, value);
		else if(S_OK == hr)
			hr = HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS);

		return hr;
	}

	HRESULT Remove (RSTRING rstrName, __out_opt TValue* pValue)
	{
		HRESULT hr;
		sysint nPosition;

		hr = BinaryFind(rstrName, &nPosition);
		if(S_OK == hr)
		{
			NAMED_MAP_ENTRY Entry;
			m_Store.Remove(nPosition, &Entry);
			RStrRelease(Entry.rstrName);
			if(pValue)
				*pValue = Entry.value;
		}
		else if(S_FALSE == hr)
			hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

		return hr;
	}

	HRESULT RemoveByIndex (sysint nPosition, __out_opt TValue* pValue)
	{
		HRESULT hr = HRESULT_FROM_WIN32(ERROR_INVALID_INDEX);

		if(nPosition < m_Store.Length())
		{
			NAMED_MAP_ENTRY Entry;
			m_Store.Remove(nPosition, &Entry);
			RStrRelease(Entry.rstrName);
			if(pValue)
				*pValue = Entry.value;
			hr = S_OK;
		}

		return hr;
	}

	VOID Clear (VOID)
	{
		NAMED_MAP_ENTRY* pData;
		sysint cList;

		m_Store.GetData(&pData, &cList);
		for(sysint i = 0; i < cList; i++)
			RStrRelease(pData[i].rstrName);

		m_Store.Clear();
	}

	HRESULT Find (RSTRING rstrName, TValue* pValue)
	{
		HRESULT hr;
		sysint nPosition;

		hr = BinaryFind(rstrName, &nPosition);
		if(S_OK == hr)
			*pValue = m_Store[nPosition].value;
		else if(S_FALSE == hr)
			hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

		return hr;
	}

	HRESULT Find (RSTRING rstrName, TValue* const pcValue) const
	{
		HRESULT hr;
		sysint nPosition;

		hr = BinaryFind(rstrName, &nPosition);
		if(S_OK == hr)
			*pcValue = m_Store[nPosition].value;
		else if(S_FALSE == hr)
			hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

		return hr;
	}

	HRESULT FindPtr (RSTRING rstrName, __deref_out TValue** ppValue)
	{
		HRESULT hr;
		sysint nPosition;

		Assert(ppValue);	 // It doesn't make sense to call FindPtr() without ppValue.

		hr = BinaryFind(rstrName, &nPosition);
		if(S_OK == hr)
			*ppValue = &m_Store[nPosition].value;
		else if(S_FALSE == hr)
			hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

		return hr;
	}

	HRESULT FindPtrAdd (RSTRING rstrName, __deref_out TValue** ppValue, bool* pfAdded)
	{
		HRESULT hr;
		sysint nPosition;

		Assert(ppValue);	 // It doesn't make sense to call FindPtrAdd() without ppValue.

		hr = BinaryFind(rstrName, &nPosition);
		if(S_FALSE == hr)
		{
			NAMED_MAP_ENTRY Entry;

			Entry.rstrName = rstrName;
			hr = m_Store.InsertAt(&Entry, nPosition);

			if(SUCCEEDED(hr))
			{
				RStrAddRef(rstrName);
				*pfAdded = true;
			}
		}
		else
			*pfAdded = false;

		if(S_OK == hr)
			*ppValue = &m_Store[nPosition].value;

		return hr;
	}

	inline bool HasItem (RSTRING rstrName) const
	{
		sysint nPosition;
		return S_OK == BinaryFind(rstrName, &nPosition);
	}

	inline bool IndexOf (RSTRING rstrName, sysint* pnPosition)
	{
		return S_OK == BinaryFind(rstrName, pnPosition);
	}

	HRESULT Update (RSTRING rstrName, const TValue& value, __out_opt TValue* pOldValue)
	{
		HRESULT hr;
		TValue* pSlot;

		Check(FindPtr(rstrName, &pSlot));
		if(pOldValue)
			*pOldValue = *pSlot;
		*pSlot = value;

	Cleanup:
		return hr;
	}

	HRESULT UpdateOrAdd (RSTRING rstrName, const TValue& value, __out_opt TValue* pOldValue)
	{
		HRESULT hr;
		sysint nPosition;

		Assert(NULL != rstrName);	// rstrName must not be NULL

		hr = BinaryFind(rstrName, &nPosition);
		if(S_OK == hr)
		{
			if(pOldValue)
				*pOldValue = m_Store[nPosition].value;
			m_Store[nPosition].value = value;
		}
		else if(S_FALSE == hr)
		{
			hr = AddByIndex(nPosition, rstrName, value);

			if(pOldValue)
				ZeroMemory(pOldValue, sizeof(TValue));
		}
		else
			hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

		return hr;
	}

	HRESULT UpdateByIndex (sysint nPosition, const TValue& value, __out_opt TValue* pOldValue)
	{
		HRESULT hr;

		if(0 <= nPosition && nPosition < m_Store.Length())
		{
			if(pOldValue)
				*pOldValue = m_Store[nPosition].value;
			m_Store[nPosition].value = value;
			hr = S_OK;
		}
		else
			hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

		return hr;
	}

	HRESULT Move (RSTRING rstrExisting, RSTRING rstrNew)
	{
		sysint nExisting, nNew;
		HRESULT hr = BinaryFind(rstrNew, &nNew);
		if(S_FALSE == hr)
		{
			hr = BinaryFind(rstrExisting, &nExisting);
			if(S_OK == hr)
			{
				hr = AddByIndex(nNew, rstrNew, m_Store[nExisting].value);
				if(SUCCEEDED(hr))
					SideAssertHr(RemoveByIndex(nExisting >= nNew ? nExisting + 1 : nExisting, NULL));
			}
			else if(S_FALSE == hr)
				hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
		}
		else if(S_OK == hr)
			hr = HRESULT_FROM_WIN32(ERROR_ALREADY_ASSIGNED);
		return hr;
	}

protected:
	HRESULT BinaryFind (RSTRING rstrName, sysint* pnPosition) const
	{
		HRESULT hrCompare;
		const NAMED_MAP_ENTRY* pData;
		sysint iRight, iLeft = 0, iMiddle;

		m_Store.GetData(&pData, &iRight);
		iRight--;

		while(iLeft <= iRight)
		{
			INT nCompare;

			iMiddle = ((unsigned)(iLeft + iRight)) >> 1;
			hrCompare = m_pfnCompare(pData[iMiddle].rstrName, rstrName, &nCompare);
			if(FAILED(hrCompare))
				return hrCompare;
			if(0 > nCompare)
				iLeft = iMiddle + 1;
			else if(0 < nCompare)
				iRight = iMiddle - 1;
			else
			{
				*pnPosition = iMiddle;  // The item has been found at this position
				return S_OK;
			}
		}
		*pnPosition = iLeft;			// This is the point where the item should be inserted
		return S_FALSE;
	}

	HRESULT AddByIndex (sysint nPosition, RSTRING rstrName, const TValue value)
	{
		HRESULT hr;
		NAMED_MAP_ENTRY Entry;

		Entry.rstrName = rstrName;
		Entry.value = value;
		hr = m_Store.InsertAt(&Entry, nPosition);

		if(SUCCEEDED(hr))
			RStrAddRef(rstrName);

		return hr;
	}
};
