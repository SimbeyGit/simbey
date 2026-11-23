#pragma once

#include "ILockableStream.h"

class CLockableStream : public ILockableStream
{
private:
	CRITICAL_SECTION m_cs;

	ULONG m_cRef;
	ULONG m_cLocks;

	ISequentialStream* m_pstmSource;
	DWORD m_cbData;
	PBYTE m_pbData;

public:
	CLockableStream (ISequentialStream* pstmSource, DWORD cbData);
	virtual ~CLockableStream ();

	// IUnknown
	HRESULT WINAPI QueryInterface (REFIID iid, LPVOID* lplpvObject);
	ULONG WINAPI AddRef (VOID);
	ULONG WINAPI Release (VOID);

	// ISequentialStream
	HRESULT WINAPI Read (LPVOID lpv, ULONG cb, ULONG* lpcbRead);
	HRESULT WINAPI Write (VOID const* lpcv, ULONG cb, ULONG* lpcbWritten);

	// ILockableStream
	virtual HRESULT WINAPI Lock (__deref_out_bcount(*pcbData) PBYTE* ppbData, __out DWORD* pcbData);
	virtual VOID WINAPI Unlock (VOID);
};
