#pragma once

#include "ISeekableStream.h"

class CMemFileData
{
private:
	ULONG m_cRef;
	HANDLE m_hHeap;
	BOOL m_fOwnHeap;

public:
	PBYTE m_pBuffer;
	ULARGE_INTEGER m_uliData;

public:
	CMemFileData (HANDLE hHeap, BOOL fOwnHeap);
	~CMemFileData ();

	ULONG AddRef (VOID);
	ULONG Release (VOID);

	HRESULT Realloc (SIZE_T cbAlloc);
};

class CMemoryFile : public ISeekableStream
{
private:
	ULONG m_cRef;

protected:
	CMemFileData* m_pmfd;
	ULARGE_INTEGER m_uliPtr;

public:
	CMemoryFile ();
	CMemoryFile (CMemFileData* pmfd);
	virtual ~CMemoryFile ();

	HRESULT InitializeNew (__in_opt HANDLE hHeap = NULL, BOOL fOwnHeap = FALSE);

	// IUnknown
	HRESULT WINAPI QueryInterface (REFIID iid, LPVOID* lplpvObject);
	ULONG WINAPI AddRef (VOID);
	ULONG WINAPI Release (VOID);

	// ISequentialStream
	HRESULT WINAPI Read (LPVOID lpv, ULONG cb, ULONG* lpcbRead);
	HRESULT WINAPI Write (VOID const* lpcv, ULONG cb, ULONG* lpcbWritten);

	// ISeekableStream
	virtual HRESULT WINAPI Seek (LARGE_INTEGER liDistanceToMove, DWORD dwOrigin, __out_opt ULARGE_INTEGER* puliNewPosition);
	virtual HRESULT WINAPI Stat (__out STATSTG* pStatstg, DWORD grfStatFlag);
	virtual HRESULT WINAPI Duplicate (__deref_out ISeekableStream** ppDupStream);

	ULONGLONG GetSize (VOID) const;
	ULONGLONG GetRemaining (VOID) const;
	HRESULT Truncate (VOID);
	BYTE const* GetReadPtr (VOID) const;
	PBYTE GetWritePtr (VOID) const;

	template <typename T>
	inline HRESULT TWrite (__in_ecount(cElements) const T* pctv, INT cElements, ULONG* pcbWritten)
	{
		return Write(pctv, cElements * sizeof(T), pcbWritten);
	}
};
