#include <windows.h>
#include "CoreDefs.h"
#include "LockableStream.h"

CLockableStream::CLockableStream (ISequentialStream* pstmSource, DWORD cbData)
{
	InitializeCriticalSection(&m_cs);

	m_cRef = 1;
	m_cLocks = 0;

	SetInterface(m_pstmSource, pstmSource);
	m_cbData = cbData;
	m_pbData = NULL;
}

CLockableStream::~CLockableStream ()
{
	Assert(0 == m_cLocks);

	SafeDeleteArray(m_pbData);
	SafeRelease(m_pstmSource);

	DeleteCriticalSection(&m_cs);
}

// IUnknown

HRESULT WINAPI CLockableStream::QueryInterface (REFIID iid, LPVOID* lplpvObject)
{
	HRESULT hr = E_POINTER;
	if(lplpvObject)
	{
		if(iid == __uuidof(ILockableStream))
			*lplpvObject = static_cast<ILockableStream*>(this);
		else if(iid == IID_ISequentialStream)
			*lplpvObject = static_cast<ISequentialStream*>(this);
		else if(iid == IID_IUnknown)
			*lplpvObject = static_cast<IUnknown*>(this);
		else
		{
			hr = E_NOINTERFACE;
			goto exit;
		}
		AddRef();
		hr = S_OK;
	}

exit:
	return hr;
}

ULONG WINAPI CLockableStream::AddRef (VOID)
{
	return (ULONG)InterlockedIncrement((LONG*)&m_cRef);
}

ULONG WINAPI CLockableStream::Release (VOID)
{
	ULONG c = (ULONG)InterlockedDecrement((LONG*)&m_cRef);
	if(c == 0)
		__delete this;
	return c;
}

// ISequentialStream

HRESULT WINAPI CLockableStream::Read (LPVOID lpv, ULONG cb, ULONG* lpcbRead)
{
	HRESULT hr;

	EnterCriticalSection(&m_cs);

	CheckIf(0 != m_cLocks, HRESULT_FROM_WIN32(ERROR_LOCKED));
	if(m_cbData < cb)
		cb = m_cbData;
	Check(m_pstmSource->Read(lpv, cb, lpcbRead));
	m_cbData -= *lpcbRead;
	SafeDeleteArray(m_pbData);

Cleanup:
	LeaveCriticalSection(&m_cs);
	return hr;
}

HRESULT WINAPI CLockableStream::Write (VOID const* lpcv, ULONG cb, ULONG* lpcbWritten)
{
	HRESULT hr;

	EnterCriticalSection(&m_cs);

	CheckIf(0 != m_cLocks, HRESULT_FROM_WIN32(ERROR_LOCKED));
	Check(m_pstmSource->Write(lpcv, cb, lpcbWritten));
	m_cbData += *lpcbWritten;
	SafeDeleteArray(m_pbData);

Cleanup:
	LeaveCriticalSection(&m_cs);
	return hr;
}

// ILockableStream

HRESULT WINAPI CLockableStream::Lock (__deref_out_bcount(*pcbData) PBYTE* ppbData, __out DWORD* pcbData)
{
	HRESULT hr;

	EnterCriticalSection(&m_cs);

	if(0 == m_cLocks && NULL == m_pbData)
	{
		DWORD cbRemaining = m_cbData;
		m_pbData = __new BYTE[m_cbData];
		CheckAlloc(m_pbData);

		// Depending on the source, the data may only be available in chunks.
		// Read the data until m_cbData bytes have been read.
		do
		{
			DWORD cbRead;
			Check(m_pstmSource->Read(m_pbData + (m_cbData - cbRemaining), cbRemaining, &cbRead));
			cbRemaining -= cbRead;
		} while(0 < cbRemaining);
	}

	m_cLocks++;
	*ppbData = m_pbData;
	*pcbData = m_cbData;
	hr = S_OK;

Cleanup:
	LeaveCriticalSection(&m_cs);
	return hr;
}

VOID WINAPI CLockableStream::Unlock (VOID)
{
	EnterCriticalSection(&m_cs);
	m_cLocks--;
	LeaveCriticalSection(&m_cs);
}
