#pragma once

#include "Array.h"
#include "StringCore.h"

/////////////////////////////////////////////////////////////////////////////////////
// NAMED MAP
/////////////////////////////////////////////////////////////////////////////////////

template<typename TChar, typename TValue, typename TTraits = DefaultTraits>
class TNamedMap
{
public:
	typedef struct
	{
		TChar* ptzName;
		TValue value;
	} NAMED_MAP_ENTRY;

	typedef TArray<NAMED_MAP_ENTRY, TTraits> ArrayType;
	typedef typename TTraits::THeap THeap;

protected:
	THeap m_Heap;
	ArrayType m_Store;
	INT (__cdecl* m_pfnCompare)(const TChar*, const TChar*);

public:
	TNamedMap (INT(__cdecl* pfnCompare)(const TChar*, const TChar*)) { m_pfnCompare = pfnCompare; }
	TNamedMap (INT(__cdecl* pfnCompare)(const TChar*, const TChar*), THeap& heap) : m_Heap(heap), m_Store(heap) { m_pfnCompare = pfnCompare; }
	~TNamedMap () { Clear(); }

	TValue* operator[](const TChar* ptzKey)
	{
		TValue* p;
		sysint nPosition;
		if(BinaryFind(ptzKey, &nPosition))
			p = &m_Store[nPosition].value;
		else
		{
			NAMED_MAP_ENTRY Item;
			ZeroMemory(&Item, sizeof(Item));

			if(SUCCEEDED(Add(ptzKey, Item.value)))
				p = &m_Store[nPosition].value;
			else
				p = NULL;
		}
		return p;
	}

	TValue* operator[](sysint n)
	{
		return &m_Store[n].value;
	}

	inline sysint Length (VOID) const
	{
		return m_Store.Length();
	}

	inline const TChar* GetKey (sysint n) const
	{
		return m_Store[n].ptzName;
	}

	HRESULT GetKeyChecked (sysint n, const TChar** pptzKey) const
	{
		HRESULT hr = HRESULT_FROM_WIN32(ERROR_INVALID_INDEX);

		Assert(pptzKey);

		if(0 <= n && n < m_Store.Length())
		{
			*pptzKey = m_Store[n].ptzName;
			hr = S_OK;
		}

		return hr;
	}

	const TChar* GetKeyPtr (const TChar* ptzName) const
	{
		const TChar* ptzKey = NULL;
		sysint nPosition;
		if(BinaryFind(ptzName, &nPosition))
			ptzKey = m_Store[nPosition].ptzName;
		return ptzKey;
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

	HRESULT GetKeyAndValue (sysint n, __out const TChar** ppctzName, TValue* pValue)
	{
		HRESULT hr = HRESULT_FROM_WIN32(ERROR_INVALID_INDEX);

		Assert(ppctzName && pValue);

		if(0 <= n && n < m_Store.Length())
		{
			*ppctzName = m_Store[n].ptzName;
			*pValue = m_Store[n].value;
			hr = S_OK;
		}

		return hr;
	}

	HRESULT GetKeyAndValuePtr (sysint n, __out const TChar** ppctzName, __out TValue** ppValue)
	{
		HRESULT hr = HRESULT_FROM_WIN32(ERROR_INVALID_INDEX);

		Assert(ppctzName && ppValue);

		if(0 <= n && n < m_Store.Length())
		{
			*ppctzName = m_Store[n].ptzName;
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

	HRESULT Add (const TChar* ptzName, const TValue& value)
	{
		HRESULT hr;
		sysint nPosition;

		Assert(NULL != ptzName);	// ptzName must not be NULL

		if(BinaryFind(ptzName, &nPosition))
			hr = E_FAIL;
		else
			hr = AddByIndex(nPosition, ptzName, value);

		return hr;
	}

	HRESULT AddAndReturnIndex (const TChar* ptzName, const TValue& value, __out sysint* pnPosition)
	{
		HRESULT hr;

		Assert(NULL != ptzName);	// ptzName must not be NULL

		if(BinaryFind(ptzName, pnPosition))
			hr = E_FAIL;
		else
			hr = AddByIndex(*pnPosition, ptzName, value);

		return hr;
	}

	HRESULT AddSlot (const TChar* ptzName, TValue** ppValue)
	{
		HRESULT hr = HRESULT_FROM_WIN32(ERROR_ALREADY_ASSIGNED);
		sysint nPosition;

		Assert(NULL != ptzName);	// ptzName must not be NULL

		if(!BinaryFind(ptzName, &nPosition))
		{
			hr = AddSlotByIndex(nPosition, ptzName, ppValue);
		}

		return hr;
	}

	HRESULT Remove (const TChar* ptzName, __out_opt TValue* pValue)
	{
		HRESULT hr = E_FAIL;
		sysint nPosition;

		if(BinaryFind(ptzName, &nPosition))
		{
			NAMED_MAP_ENTRY Entry;
			m_Store.Remove(nPosition, &Entry);
			m_Heap.release_storage(Entry.ptzName);
			if(pValue)
				*pValue = Entry.value;
			hr = S_OK;
		}

		return hr;
	}

	HRESULT RemoveByIndex (sysint nPosition, __out_opt TValue* pValue)
	{
		HRESULT hr = HRESULT_FROM_WIN32(ERROR_INVALID_INDEX);

		if(nPosition < m_Store.Length())
		{
			NAMED_MAP_ENTRY Entry;
			m_Store.Remove(nPosition, &Entry);
			m_Heap.release_storage(Entry.ptzName);
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
			m_Heap.release_storage(pData[i].ptzName);

		m_Store.Clear();
	}

	HRESULT Find (const TChar* ptzName, TValue* pValue) const
	{
		HRESULT hr = E_FAIL;
		sysint nPosition;

		if(BinaryFind(ptzName, &nPosition))
		{
			const NAMED_MAP_ENTRY* pcData;
			sysint cList;

			m_Store.GetData(&pcData, &cList);
			*pValue = pcData[nPosition].value;

			hr = S_OK;
		}

		return hr;
	}

	HRESULT FindPtr (const TChar* ptzName, __deref_out TValue** ppValue)
	{
		HRESULT hr = E_FAIL;
		sysint nPosition;

		Assert(ppValue);	 // It doesn't make sense to call Find() without ppValue.

		if(BinaryFind(ptzName, &nPosition))
		{
			NAMED_MAP_ENTRY* pData;
			sysint cList;

			m_Store.GetData(&pData, &cList);
			*ppValue = &pData[nPosition].value;

			hr = S_OK;
		}

		return hr;
	}

	inline BOOL HasItem (const TChar* ptzName) const
	{
		sysint nPosition;
		return BinaryFind(ptzName, &nPosition);
	}

	inline BOOL IndexOf (const TChar* ptzName, sysint* pnPosition) const
	{
		return BinaryFind(ptzName, pnPosition);
	}

	HRESULT Update (const TChar* pctzName, const TValue& value, __out_opt TValue* pOldValue)
	{
		HRESULT hr;
		TValue* pSlot;

		Check(FindPtr(pctzName, &pSlot));
		if(pOldValue)
			*pOldValue = *pSlot;
		*pSlot = value;

	Cleanup:
		return hr;
	}

	HRESULT UpdateOrAdd (const TChar* pctzName, const TValue& value, __out_opt TValue* pOldValue)
	{
		HRESULT hr;
		sysint nPosition;

		Assert(NULL != pctzName);	// ptzName must not be NULL

		if(BinaryFind(pctzName, &nPosition))
		{
			if(pOldValue)
				*pOldValue = m_Store[nPosition].value;
			m_Store[nPosition].value = value;
			hr = S_OK;
		}
		else
		{
			hr = AddByIndex(nPosition, pctzName, value);

			if(pOldValue)
				ZeroMemory(pOldValue, sizeof(TValue));
		}

		return hr;
	}

	HRESULT UpdateByIndex (sysint nPosition, const TValue& value, __out_opt TValue* pOldValue)
	{
		HRESULT hr;

		if(0 <= nPosition && nPosition < m_Store.Length())
		{
			if(pOldValue)
			{
				*pOldValue = m_Store[nPosition].value;
			}
			m_Store[nPosition].value = value;
			hr = S_OK;
		}
		else
			hr = E_FAIL;

		return hr;
	}

	VOID Swap (TNamedMap<TChar, TValue, TTraits>& mapOther)
	{
		m_Store.Swap(mapOther.m_Store);
	}

	VOID DeleteAll (VOID)
	{
		NAMED_MAP_ENTRY* pData;
		sysint cList;

		m_Store.GetData(&pData, &cList);
		for(sysint i = 0; i < cList; i++)
		{
			m_Heap.release_storage(pData[i].ptzName);
			__delete pData[i].value;
		}
		m_Store.Clear();
	}

protected:
	BOOL BinaryFind (const TChar* ptzName, sysint* pnPosition) const
	{
		const NAMED_MAP_ENTRY* pcData;
		sysint iRight, iLeft = 0, iMiddle, nCompare;

		m_Store.GetData(&pcData, &iRight);
		iRight--;

		while(iLeft <= iRight)
		{
			iMiddle = ((unsigned)(iLeft + iRight)) >> 1;
			nCompare = m_pfnCompare(pcData[iMiddle].ptzName, ptzName);
			if(0 > nCompare)
				iLeft = iMiddle + 1;
			else if(0 < nCompare)
				iRight = iMiddle - 1;
			else
			{
				*pnPosition = iMiddle;  // The item has been found at this position
				return TRUE;
			}
		}
		*pnPosition = iLeft;			// This is the point where the item should be inserted
		return FALSE;
	}

	HRESULT AddByIndex (sysint nPosition, const TChar* ptzName, const TValue value)
	{
		HRESULT hr;
		NAMED_MAP_ENTRY Entry;

		// TDuplicateStringAssert() won't recheck whether ptzName is NULL
		hr = TDuplicateStringAssert(m_Heap, ptzName, &Entry.ptzName);
		if(SUCCEEDED(hr))
		{
			Entry.value = value;
			hr = m_Store.InsertAt(&Entry, nPosition);

			if(FAILED(hr))
				m_Heap.release_storage(Entry.ptzName);
		}

		return hr;
	}

	HRESULT AddSlotByIndex (sysint nPosition, const TChar* ptzName, TValue** ppValue)
	{
		HRESULT hr;
		NAMED_MAP_ENTRY* pEntry;

		hr = m_Store.InsertSlot(&pEntry, nPosition);
		if(SUCCEEDED(hr))
		{
			*ppValue = &pEntry->value;

			// TDuplicateStringAssert() won't recheck whether ptzName is NULL
			hr = TDuplicateStringAssert(m_Heap, ptzName, &pEntry->ptzName);

			if(FAILED(hr))
				m_Store.Remove(nPosition, NULL);
		}

		return hr;
	}
};

template<typename TValue>
class TNamedMapW : public TNamedMap<WCHAR, TValue>
{
public:
	TNamedMapW() : TNamedMap(TStrCmpAssert<WCHAR>) {}
	~TNamedMapW() {}
};

template<typename TValue>
class TNamedMapIW : public TNamedMap<WCHAR, TValue>
{
public:
	TNamedMapIW() : TNamedMap(TStrCmpIAssert<WCHAR>) {}
	~TNamedMapIW() {}
};

template<typename TValue>
class TNamedMapA : public TNamedMap<CHAR, TValue>
{
public:
	TNamedMapA() : TNamedMap(TStrCmpAssert<CHAR>) {}
	~TNamedMapA() {}
};

template<typename TValue>
class TNamedMapIA : public TNamedMap<CHAR, TValue>
{
public:
	TNamedMapIA() : TNamedMap(TStrCmpIAssert<CHAR>) {}
	~TNamedMapIA() {}
};

/////////////////////////////////////////////////////////////////////////////////////
// MAP
/////////////////////////////////////////////////////////////////////////////////////

template<typename TKey, typename TValue, typename TTraits = DefaultTraits>
class TMap
{
public:
	typedef struct
	{
		TKey key;
		TValue value;
	} KEY_MAP_ENTRY;

	typedef TArray<KEY_MAP_ENTRY, TTraits> ArrayType;
	typedef typename TTraits::THeap THeap;

protected:
	ArrayType m_Store;

public:
	TMap () {}
	TMap (THeap& heap) : m_Store(heap) {}
	~TMap () {}

	TValue* operator[](const TKey key)
	{
		TValue* p;
		sysint nPosition;
		if(BinaryFind(key, &nPosition))
			p = &m_Store[nPosition].value;
		else
		{
			KEY_MAP_ENTRY Item;
			ZeroMemory(&Item, sizeof(Item));
			Item.key = key;

			if(SUCCEEDED(m_Store.InsertAt(&Item, nPosition)))
				p = &m_Store[nPosition].value;
			else
				p = NULL;
		}
		return p;
	}

	inline sysint Length (VOID) const
	{
		return m_Store.Length();
	}

	inline TKey GetKey (sysint n) const
	{
		return m_Store[n].key;
	}

	HRESULT GetKeyChecked (sysint n, TKey* pKey) const
	{
		HRESULT hr = HRESULT_FROM_WIN32(ERROR_INVALID_INDEX);

		Assert(pKey);

		if(0 <= n && n < m_Store.Length())
		{
			*pKey = m_Store[n].key;
			hr = S_OK;
		}

		return hr;
	}

	inline TValue* GetValuePtr (sysint n)
	{
		return &m_Store[n].value;
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

	HRESULT GetKeyAndValue (sysint n, __out TKey* pKey, __out TValue* pValue)
	{
		HRESULT hr = HRESULT_FROM_WIN32(ERROR_INVALID_INDEX);

		Assert(pKey && pValue);

		if(0 <= n && n < m_Store.Length())
		{
			*pKey   = m_Store[n].key;
			*pValue = m_Store[n].value;
			hr = S_OK;
		}

		return hr;
	}

	HRESULT GetKeyAndValuePtr (sysint n, __out TKey* pKey, __out TValue** ppValue)
	{
		HRESULT hr = HRESULT_FROM_WIN32(ERROR_INVALID_INDEX);

		Assert(pKey && ppValue);

		if(0 <= n && n < m_Store.Length())
		{
			*pKey = m_Store[n].key;
			*ppValue = &m_Store[n].value;
			hr = S_OK;
		}

		return hr;
	}

	HRESULT GetValuePtrChecked (sysint n, TValue** ppValue)
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

	inline HRESULT Reserve (sysint nAllocItems)
	{
		return m_Store.Reserve(nAllocItems);
	}

	HRESULT Compact (VOID)
	{
		return m_Store.Compact();
	}

	HRESULT Add (const TKey key, const TValue& value)
	{
		HRESULT hr = E_FAIL;
		sysint n;

		if(!BinaryFind(key, &n))
		{
			hr = AddByIndex(n, key, value);
		}

		return hr;
	}

	HRESULT AddAndReturnIndex (const TKey key, const TValue& value, __out sysint* pnPosition)
	{
		HRESULT hr;

		if(BinaryFind(key, pnPosition))
			hr = E_FAIL;
		else
			hr = AddByIndex(*pnPosition, key, value);

		return hr;
	}

	HRESULT AddSlot (const TKey key, TValue** ppValue)
	{
		HRESULT hr = HRESULT_FROM_WIN32(ERROR_ALREADY_ASSIGNED);
		sysint nPosition;

		if(!BinaryFind(key, &nPosition))
		{
			hr = AddSlotByIndex(nPosition, key, ppValue);
		}

		return hr;
	}

	HRESULT Remove (const TKey key, __out_opt TValue* pValue)
	{
		HRESULT hr = E_FAIL;
		sysint n;

		if(BinaryFind(key, &n))
		{
			if(pValue)
			{
				KEY_MAP_ENTRY Item;
				m_Store.Remove(n, &Item);
				*pValue = Item.value;
			}
			else
				m_Store.Remove(n, NULL);
			hr = S_OK;
		}

		return hr;
	}

	HRESULT RemoveByIndex (sysint nPosition, __out_opt TValue* pValue)
	{
		HRESULT hr = E_FAIL;

		if(nPosition < m_Store.Length())
		{
			if(pValue)
			{
				KEY_MAP_ENTRY Entry;
				m_Store.Remove(nPosition, &Entry);
				*pValue = Entry.value;
			}
			else
				m_Store.Remove(nPosition, NULL);
			hr = S_OK;
		}

		return hr;
	}

	inline VOID Clear (VOID)
	{
		m_Store.Clear();
	}

	HRESULT Find (const TKey key, TValue* pValue) const
	{
		HRESULT hr = E_FAIL;
		sysint n;

		if(BinaryFind(key, &n))
		{
			*pValue = m_Store[n].value;
			hr = S_OK;
		}

		return hr;
	}

	HRESULT FindPtr (const TKey key, __deref_out TValue** ppValue)
	{
		HRESULT hr = E_FAIL;
		sysint n;

		Assert(ppValue);	// It doesn't make sense to call Find() without ppValue.

		if(BinaryFind(key, &n))
		{
			*ppValue = &m_Store[n].value;
			hr = S_OK;
		}

		return hr;
	}

	inline BOOL HasItem (const TKey key) const
	{
		sysint nPosition;
		return BinaryFind(key, &nPosition);
	}

	inline BOOL IndexOf (const TKey key, sysint* pnPosition) const
	{
		return BinaryFind(key, pnPosition);
	}

	HRESULT Update (const TKey key, const TValue& value, __out_opt TValue* pOldValue)
	{
		HRESULT hr;
		TValue* pSlot;

		Check(FindPtr(key, &pSlot));
		if(pOldValue)
		{
			*pOldValue = *pSlot;
		}
		*pSlot = value;

	Cleanup:
		return hr;
	}

	HRESULT UpdateOrAdd (const TKey key, const TValue& value, __out_opt TValue* pOldValue)
	{
		HRESULT hr;
		sysint nPosition;

		if(BinaryFind(key, &nPosition))
		{
			if(pOldValue)
			{
				*pOldValue = m_Store[nPosition].value;
			}
			m_Store[nPosition].value = value;
			hr = S_OK;
		}
		else
		{
			hr = AddByIndex(nPosition, key, value);

			if(pOldValue)
			{
				ZeroMemory(pOldValue, sizeof(TValue));
			}
		}

		return hr;
	}

	HRESULT UpdateByIndex (sysint nPosition, const TValue& value, __out_opt TValue* pOldValue)
	{
		HRESULT hr;

		if(0 <= nPosition && nPosition < m_Store.Length())
		{
			if(pOldValue)
			{
				*pOldValue = m_Store[nPosition].value;
			}
			m_Store[nPosition].value = value;
			hr = S_OK;
		}
		else
			hr = E_FAIL;

		return hr;
	}

	VOID Swap (TMap<TKey, TValue, TTraits>& mapOther)
	{
		m_Store.Swap(mapOther.m_Store);
	}

	VOID DeleteAll (VOID)
	{
		KEY_MAP_ENTRY* pData;
		sysint cList;

		m_Store.GetData(&pData, &cList);
		for(sysint i = 0; i < cList; i++)
			__delete pData[i].value;
		m_Store.Clear();
	}

protected:
	BOOL BinaryFind (const TKey key, sysint* pnPosition) const
	{
		const KEY_MAP_ENTRY* pcData;
		sysint iRight, iLeft = 0, iMiddle;

		m_Store.GetData(&pcData, &iRight);
		iRight--;

		while(iLeft <= iRight)
		{
			iMiddle = ((unsigned)(iLeft + iRight)) >> 1;
			if(pcData[iMiddle].key < key)
				iLeft = iMiddle + 1;
			else if(pcData[iMiddle].key > key)
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

	HRESULT AddByIndex (sysint nPosition, const TKey key, const TValue& value)
	{
		KEY_MAP_ENTRY Item;
		Item.key = key;
		Item.value = value;

		return m_Store.InsertAt(&Item, nPosition);
	}

	HRESULT AddSlotByIndex (sysint nPosition, const TKey key, TValue** ppValue)
	{
		HRESULT hr;
		KEY_MAP_ENTRY* pEntry;

		hr = m_Store.InsertSlot(&pEntry, nPosition);
		if(SUCCEEDED(hr))
		{
			pEntry->key = key;
			*ppValue = &pEntry->value;
		}

		return hr;
	}
};

/////////////////////////////////////////////////////////////////////////////////////
// MULTI MAP
/////////////////////////////////////////////////////////////////////////////////////

template <typename TCompatibleMap, typename TKey, typename TValue, typename TTraits = DefaultTraits>
class TMultiCompatibleMap
{
public:
	typedef typename TTraits::THeap THeap;
	typedef TArray<TValue> ArrayType;

protected:
	THeap m_Heap;
	TCompatibleMap m_Map;

public:
	TMultiCompatibleMap () {}
	TMultiCompatibleMap (THeap& heap) : m_Heap(heap), m_Map(heap) {}
	~TMultiCompatibleMap () { Clear(); }

	ArrayType** operator[] (const TKey key)
	{
		ArrayType** ppvMapArray = m_Map[key];
		if(ppvMapArray && NULL == *ppvMapArray)
		{
			if(SUCCEEDED(m_Heap.allocate_storage(1, ppvMapArray)))
				__new_placement(*ppvMapArray) ArrayType(m_Heap);
		}
		return ppvMapArray;
	}

	inline sysint Length (VOID) const
	{
		return m_Map.Length();
	}

	sysint ArrayLength (const TKey key) const
	{
		sysint cArrayLength = -1;
		ArrayType* pvMapArray;
		if(SUCCEEDED(m_Map.Find(key, &pvMapArray)) && pvMapArray)
			cArrayLength = pvMapArray->Length();
		return cArrayLength;
	}

	inline TKey GetKey (sysint n) const
	{
		return m_Map.GetKey(n);
	}

	inline const TKey GetKeyConst (sysint n) const
	{
		return const_cast<const TKey>(m_Map.GetKey(n));
	}

	inline ArrayType** GetValuePtr (sysint n)
	{
		return m_Map.GetValuePtr(n);
	}

	HRESULT GetContainer (const TKey key, ArrayType** ppContainer)
	{
		HRESULT hr;
		ArrayType* pContainer;

		Assert(NULL != ppContainer);

		hr = m_Map.Find(key, &pContainer);
		if(FAILED(hr))
		{
			hr = m_Heap.allocate_storage(1, &pContainer);
			if(SUCCEEDED(hr))
			{
				__new_placement(pContainer) ArrayType(m_Heap);
				hr = m_Map.Add(key, pContainer);
				if(FAILED(hr))
					m_Heap.release_storage(pContainer);
			}
		}

		*ppContainer = pContainer;

		return hr;
	}

	inline HRESULT Reserve (sysint nAllocItems)
	{
		return m_Map.Reserve(nAllocItems);
	}

	HRESULT Add (const TKey key, const TValue& value)
	{
		HRESULT hr;

		ArrayType** ppvMapArray = m_Map[key];
		if(ppvMapArray)
		{
			if(NULL == *ppvMapArray)
			{
				if(SUCCEEDED(m_Heap.allocate_storage(1, ppvMapArray)))
					__new_placement(*ppvMapArray) ArrayType(m_Heap);
			}
			if(*ppvMapArray)
				hr = (*ppvMapArray)->Append(value);
			else
				hr = E_OUTOFMEMORY;
		}
		else
			hr = E_OUTOFMEMORY;

		return hr;
	}

	HRESULT AddSlot (const TKey key, TValue** ppValue)
	{
		HRESULT hr;

		ArrayType** ppvMapArray = m_Map[key];
		if(ppvMapArray)
		{
			if(NULL == *ppvMapArray)
			{
				if(SUCCEEDED(m_Heap.allocate_storage(1, ppvMapArray)))
					__new_placement(*ppvMapArray) ArrayType(m_Heap);
			}
			if(*ppvMapArray)
				hr = (*ppvMapArray)->AppendSlot(ppValue);
			else
				hr = E_OUTOFMEMORY;
		}
		else
			hr = E_OUTOFMEMORY;

		return hr;
	}

	HRESULT Remove (const TKey key, ArrayType** ppMapArray)
	{
		ArrayType* pMapArray;
		HRESULT hr = m_Map.Remove(key, &pMapArray);

		if(SUCCEEDED(hr))
		{
			if(ppMapArray)
				*ppMapArray = pMapArray;
			else
			{
				pMapArray->~TArray();
				m_Heap.release_storage(pMapArray);
			}
		}

		return hr;
	}

	VOID Clear (VOID)
	{
		sysint c = m_Map.Length();
		if(0 < c)
		{
			for(sysint i = 0; i < c; i++)
			{
				ArrayType** ppvMapArray = m_Map.GetValuePtr(i);
				Assert(ppvMapArray);
				if(*ppvMapArray)
				{
					(*ppvMapArray)->~TArray();
					m_Heap.release_storage(*ppvMapArray);
				}
			}
			m_Map.Clear();
		}
	}

	HRESULT Find (const TKey key, TValue** ppvArray, sysint* pArray)
	{
		ArrayType* pMapArray;
		HRESULT hr = m_Map.Find(key, &pMapArray);
		if(SUCCEEDED(hr))
			pMapArray->GetData(ppvArray, pArray);
		return hr;
	}

	HRESULT Find (const TKey key, const TValue** ppcvArray, sysint* pArray) const
	{
		ArrayType* pMapArray;
		HRESULT hr = m_Map.Find(key, &pMapArray);
		if(SUCCEEDED(hr))
			pMapArray->GetData(ppcvArray, pArray);
		return hr;
	}
};

template<typename TKey, typename TValue>
class TMultiMap : public TMultiCompatibleMap<TMap<TKey, TArray<TValue>*>, TKey, TValue>
{
};
