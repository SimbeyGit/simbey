#include <windows.h>
#include "CoreDefs.h"
#include "MemoryFile.h"

CMemFileData::CMemFileData (HANDLE hHeap, BOOL fOwnHeap)
{
	m_cRef = 1;
	m_hHeap = hHeap;
	m_fOwnHeap = fOwnHeap;
	m_pBuffer = NULL;
	m_uliData.QuadPart = 0;
}

CMemFileData::~CMemFileData ()
{
	HeapFree(m_hHeap, 0, m_pBuffer);
	if(m_fOwnHeap)
		HeapDestroy(m_hHeap);
}

ULONG CMemFileData::AddRef (VOID)
{
	return (ULONG)InterlockedIncrement((LONG*)&m_cRef);
}

ULONG CMemFileData::Release (VOID)
{
	ULONG c = (ULONG)InterlockedDecrement((LONG*)&m_cRef);
	if(c == 0)
		__delete this;
	return c;
}

HRESULT CMemFileData::Realloc (SIZE_T cbAlloc)
{
	HRESULT hr;
	PBYTE pBuffer = m_pBuffer ?
		(PBYTE)HeapReAlloc(m_hHeap, 0, m_pBuffer, cbAlloc) :
		(PBYTE)HeapAlloc(m_hHeap, 0, cbAlloc);
	if(pBuffer)
	{
		m_uliData.QuadPart = cbAlloc;
		m_pBuffer = pBuffer;
		hr = S_OK;
	}
	else
		hr = HrEnsureLastError();
	return hr;
}

CMemoryFile::CMemoryFile () :
	m_cRef(1),
	m_pmfd(NULL)
{
	m_uliPtr.QuadPart = 0;
}

CMemoryFile::CMemoryFile (CMemFileData* pmfd) :
	m_cRef(1)
{
	SetInterface(m_pmfd, pmfd);
	m_uliPtr.QuadPart = 0;
}

CMemoryFile::~CMemoryFile ()
{
	SafeRelease(m_pmfd);
}

HRESULT CMemoryFile::InitializeNew (__in_opt HANDLE hHeap, BOOL fOwnHeap)
{
	HRESULT hr;

	if(NULL == m_pmfd)
	{
		if(NULL == hHeap)
			hHeap = GetProcessHeap();

		m_pmfd = __new CMemFileData(hHeap, fOwnHeap);
		hr = m_pmfd ? S_OK : E_OUTOFMEMORY;
	}
	else
		hr = S_FALSE;

	return hr;
}

// IUnknown

HRESULT WINAPI CMemoryFile::QueryInterface (REFIID iid, LPVOID* lplpvObject)
{
	HRESULT hr = E_POINTER;
	if(lplpvObject)
	{
		if(iid == IID_ISequentialStream)
			*lplpvObject = static_cast<ISequentialStream*>(this);
		else if(iid == __uuidof(ISeekableStream))
			*lplpvObject = static_cast<ISeekableStream*>(this);
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

ULONG WINAPI CMemoryFile::AddRef (VOID)
{
	return (ULONG)InterlockedIncrement((LONG*)&m_cRef);
}

ULONG WINAPI CMemoryFile::Release (VOID)
{
	ULONG c = (ULONG)InterlockedDecrement((LONG*)&m_cRef);
	if(c == 0)
		__delete this;
	return c;
}

// ISequentialStream

HRESULT WINAPI CMemoryFile::Read (LPVOID lpv, ULONG cb, ULONG* lpcbRead)
{
	HRESULT hr = E_INVALIDARG;
	if(lpv && lpcbRead)
	{
		ULARGE_INTEGER uliRemaining;
		uliRemaining.QuadPart = GetRemaining();

		if((ULONGLONG)cb > uliRemaining.QuadPart)
			cb = uliRemaining.LowPart;
		if(cb > 0)
		{
			CopyMemory(lpv, m_pmfd->m_pBuffer + (size_t)m_uliPtr.QuadPart, cb);
			m_uliPtr.QuadPart += cb;
			*lpcbRead = cb;
			hr = S_OK;
		}
		else
		{
			*lpcbRead = 0;
			hr = S_FALSE;
		}
	}
	return hr;
}

HRESULT WINAPI CMemoryFile::Write (VOID const* lpcv, ULONG cb, ULONG* lpcbWritten)
{
	HRESULT hr = E_INVALIDARG;
	if(lpcv && lpcbWritten)
	{
		ULARGE_INTEGER uliRemaining;
		uliRemaining.QuadPart = GetRemaining();

		if(0 < uliRemaining.QuadPart)
		{
			ULONG cbCopy = cb;

			if((ULONGLONG)cb > uliRemaining.QuadPart)
				cbCopy = uliRemaining.LowPart;

			CopyMemory(m_pmfd->m_pBuffer + (size_t)m_uliPtr.QuadPart, lpcv, cbCopy);
			m_uliPtr.QuadPart += (size_t)cbCopy;
			cb -= cbCopy;
			lpcv = (const BYTE*)lpcv + cbCopy;
			*lpcbWritten = cbCopy;
			hr = S_OK;
		}
		else
		{
			*lpcbWritten = 0;
			hr = S_FALSE;
		}

		if(0 < cb)
		{
			hr = m_pmfd->Realloc((SIZE_T)(m_pmfd->m_uliData.QuadPart + (ULONGLONG)cb));
			if(SUCCEEDED(hr))
			{
				CopyMemory(m_pmfd->m_pBuffer + (size_t)m_uliPtr.QuadPart, lpcv, cb);
				m_uliPtr.QuadPart += (size_t)cb;
				*lpcbWritten += cb;
			}
		}
	}
	return hr;
}

// ISeekableStream

HRESULT WINAPI CMemoryFile::Seek (LARGE_INTEGER liDistanceToMove, DWORD dwOrigin, __out_opt ULARGE_INTEGER* puliNewPosition)
{
	HRESULT hr = STG_E_INVALIDFUNCTION;
	ULARGE_INTEGER uliSeek;

	switch(dwOrigin)
	{
	case STREAM_SEEK_SET:
		uliSeek.QuadPart = (ULONGLONG)liDistanceToMove.QuadPart;
		break;
	case STREAM_SEEK_CUR:
		uliSeek.QuadPart = m_uliPtr.QuadPart + liDistanceToMove.QuadPart;
		break;
	case STREAM_SEEK_END:
		uliSeek.QuadPart = m_pmfd->m_uliData.QuadPart + liDistanceToMove.QuadPart;
		break;
	default:
		goto Cleanup;
	}

	if(uliSeek.QuadPart > m_pmfd->m_uliData.QuadPart)
		Check(m_pmfd->Realloc((SIZE_T)uliSeek.QuadPart));
	else
		hr = S_OK;

	m_uliPtr.QuadPart = uliSeek.QuadPart;
	if(puliNewPosition)
		puliNewPosition->QuadPart = uliSeek.QuadPart;

Cleanup:
	return hr;
}

HRESULT WINAPI CMemoryFile::Stat (__out STATSTG* pStatstg, DWORD grfStatFlag)
{
	pStatstg->pwcsName = NULL;  // Regardless of grfStatFlag, this implementation does not know the name.
	pStatstg->cbSize.QuadPart = GetSize();
	pStatstg->type = STGTY_STORAGE;
	pStatstg->grfMode = grfStatFlag;
	ZeroMemory(&pStatstg->ctime, sizeof(pStatstg->ctime));
	ZeroMemory(&pStatstg->atime, sizeof(pStatstg->atime));
	ZeroMemory(&pStatstg->mtime, sizeof(pStatstg->mtime));
	return S_OK;
}

HRESULT WINAPI CMemoryFile::Duplicate (__deref_out ISeekableStream** ppDupStream)
{
	HRESULT hr;

	CheckIf(NULL == ppDupStream, E_POINTER);
	*ppDupStream = __new CMemoryFile(m_pmfd);
	CheckAlloc(*ppDupStream);
	hr = S_OK;

Cleanup:
	return hr;
}

CMemFileData* CMemoryFile::GetFileData (VOID) const
{
	return m_pmfd;
}

VOID CMemoryFile::ReplaceData (CMemFileData* pmfd)
{
	ReplaceInterface(m_pmfd, pmfd);
	m_uliPtr.QuadPart = 0;
}

ULONGLONG CMemoryFile::GetOffset (VOID) const
{
	return m_uliPtr.QuadPart;
}

ULONGLONG CMemoryFile::GetSize (VOID) const
{
	return m_pmfd->m_uliData.QuadPart;
}

ULONGLONG CMemoryFile::GetRemaining (VOID) const
{
	return m_uliPtr.QuadPart > m_pmfd->m_uliData.QuadPart ?
		0 : m_pmfd->m_uliData.QuadPart - m_uliPtr.QuadPart;
}

HRESULT CMemoryFile::Truncate (VOID)
{
	HRESULT hr;
	if(m_pmfd->m_uliData.QuadPart != m_uliPtr.QuadPart)
		hr = m_pmfd->Realloc((SIZE_T)m_uliPtr.QuadPart);
	else
		hr = S_FALSE;
	return hr;
}

BYTE const* CMemoryFile::GetReadPtr (VOID) const
{
	return m_pmfd->m_pBuffer + m_uliPtr.QuadPart;
}

PBYTE CMemoryFile::GetWritePtr (VOID)
{
	return m_pmfd->m_pBuffer + m_uliPtr.QuadPart;
}
