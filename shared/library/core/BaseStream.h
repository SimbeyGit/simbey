#pragma once

class CBaseStream : public ISequentialStream
{
private:
	ULONG m_cRef;

protected:
	PBYTE m_pBuffer;
	ULONG m_cbMaxBuffer;
	ULONG m_cbData;

	ULONG m_iReadPtr;

public:
	CBaseStream ();
	virtual ~CBaseStream ();

	// IUnknown
	HRESULT WINAPI QueryInterface (REFIID iid, LPVOID* lplpvObject);
	ULONG WINAPI AddRef (VOID);
	ULONG WINAPI Release (VOID);

	// ISequentialStream
	HRESULT WINAPI Read (LPVOID lpv, ULONG cb, ULONG* lpcbRead);
	HRESULT WINAPI Write (VOID const* lpcv, ULONG cb, ULONG* lpcbWritten);

	VOID Reset (VOID);
	ULONG DataRemaining (VOID) const;
	ULONG Capacity (VOID) const;
	HRESULT Seek (LONG lMove, DWORD dwOrigin, __out ULONG* pulNewPosition);

	HRESULT UpdateReadPtr (ULONG cbSkip);
	BYTE const* GetReadPtr (VOID) const;

	HRESULT UpdateWritePtr (ULONG cbData);
	PBYTE GetWritePtr (__out ULONG* pcSlotsAvailable) const;

	HRESULT PopWritePtr (ULONG cbPop, __out_bcount_opt(cbProp) LPBYTE lpCopyWritePtr);
	HRESULT Remove (ULONG nStart, ULONG cbRemove);

	HRESULT Condense (VOID);

	template <typename T>
	ULONG TDataRemaining (VOID) const
	{
		return DataRemaining() / sizeof(T);
	}

	template <typename T>
	inline const T* TGetReadPtr (VOID)
	{
		return reinterpret_cast<const T*>(GetReadPtr());
	}

	template <typename T>
	inline T* TGetWritePtr (__out ULONG* pcSlotsAvailable)
	{
		T* ptPtr = reinterpret_cast<T*>(GetWritePtr(pcSlotsAvailable));
		*pcSlotsAvailable /= sizeof(T);
		return ptPtr;
	}

	template <typename T>
	inline HRESULT TWrite (__in_ecount(cElements) const T* pctv, INT cElements, ULONG* pcbWritten)
	{
		return Write(pctv, cElements * sizeof(T), pcbWritten);
	}

	template <typename T>
	inline HRESULT TRemoveElement (ULONG nElement)
	{
		return Remove(nElement * sizeof(T), sizeof(T));
	}

protected:
	virtual HRESULT GrowAndWrite (VOID const* lpcv, ULONG cb, ULONG* lpcbWritten) = 0;
};
