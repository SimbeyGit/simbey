#include <windows.h>
#include "..\Core\CoreDefs.h"
#include "FileStream.h"

CFileStream::CFileStream (HANDLE hFile, BOOL fCloseOnDelete)
{
	m_cRef = 1;

	m_hFile = hFile;
	m_fCloseOnDelete = fCloseOnDelete;
}

CFileStream::~CFileStream ()
{
	if(m_fCloseOnDelete)
		CloseHandle(m_hFile);
}

HRESULT WINAPI CFileStream::QueryInterface (REFIID iid, LPVOID* ppvObject)
{
	HRESULT hr;

	CheckIf(NULL == ppvObject, E_INVALIDARG);

	if(IID_ISequentialStream == iid)
		*ppvObject = static_cast<ISequentialStream*>(this);
	else if(__uuidof(ISeekableStream) == iid)
		*ppvObject = static_cast<ISeekableStream*>(this);
	else if(IID_IUnknown == iid)
		*ppvObject = static_cast<IUnknown*>(this);
	else
		Check(E_NOINTERFACE);

	AddRef();
	hr = S_OK;

Cleanup:
	return hr;
}

ULONG WINAPI CFileStream::AddRef (VOID)
{
	return InterlockedIncrement((LONG*)&m_cRef);
}

ULONG WINAPI CFileStream::Release (VOID)
{
	ULONG c = InterlockedDecrement((LONG*)&m_cRef);
	if(c == 0)
		__delete this;
	return c;
}

// ISequentialStream

HRESULT WINAPI CFileStream::Read (LPVOID pv, ULONG cb, ULONG* pcbRead)
{
	HRESULT hr = E_INVALIDARG;
	if(pv && pcbRead)
	{
		if(ReadFile(m_hFile,pv,cb,pcbRead,NULL))
			hr = S_OK;
		else
		{
			DWORD dwError = GetLastError();
			if(dwError == ERROR_HANDLE_EOF)
				hr = S_FALSE;
			else
				hr = HRESULT_FROM_WIN32(dwError);
		}
	}
	return hr;
}

HRESULT WINAPI CFileStream::Write (VOID CONST* pv, ULONG cb, ULONG* pcbWritten)
{
	HRESULT hr = E_INVALIDARG;
	if(pv && pcbWritten)
	{
		if(WriteFile(m_hFile,pv,cb,pcbWritten,NULL))
			hr = S_OK;
		else
		{
			DWORD dwError = GetLastError();
			if(dwError == ERROR_HANDLE_DISK_FULL)
				hr = STG_E_MEDIUMFULL;
			else if(dwError == ERROR_WRITE_FAULT)
				hr = STG_E_WRITEFAULT;
			else
				hr = HRESULT_FROM_WIN32(dwError);
		}
	}
	return hr;
}

// ISeekableStream

HRESULT CFileStream::Seek (LARGE_INTEGER liDistanceToMove, DWORD dwOrigin, __out_opt ULARGE_INTEGER* puliNewPosition)
{
	HRESULT hr;

	// No transformation from dwOrigin is necessary because:
	// STREAM_SEEK_SET == FILE_BEGIN
	// STREAM_SEEK_CUR == FILE_CURRENT
	// STREAM_SEEK_END == FILE_END

#if	_WIN32_WINNT >= 0x0501
	if(SetFilePointerEx(m_hFile, liDistanceToMove, reinterpret_cast<PLARGE_INTEGER>(puliNewPosition), dwOrigin))
		hr = S_OK;
	else
		hr = HRESULT_FROM_WIN32(GetLastError());
#else
	ULARGE_INTEGER uli;
	LONG lHighPart = liDistanceToMove.HighPart;

	uli.LowPart = SetFilePointer(m_hFile, liDistanceToMove.LowPart, &lHighPart, dwOrigin);
	if(INVALID_SET_FILE_POINTER != uli.LowPart)
		hr = S_OK;
	else
		hr = HRESULT_FROM_WIN32(GetLastError());

	if(puliNewPosition && SUCCEEDED(hr))
	{
		uli.HighPart = lHighPart;
		puliNewPosition->QuadPart = uli.QuadPart;
	}
#endif

	return hr;
}

HRESULT WINAPI CFileStream::Stat (__out STATSTG* pStatstg, DWORD grfStatFlag)
{
	HRESULT hr;
	pStatstg->pwcsName = NULL;  // Regardless of grfStatFlag, this implementation does not know the name.

#if	_WIN32_WINNT >= 0x0501
	if(GetFileSizeEx(m_hFile, reinterpret_cast<PLARGE_INTEGER>(&pStatstg->cbSize)))
#else
	pStatstg->cbSize.LowPart = GetFileSize(m_hFile, &pStatstg->cbSize.HighPart);
	if(INVALID_FILE_SIZE != pStatstg->cbSize.LowPart)
#endif
	{
		pStatstg->type = STGTY_STORAGE;
		pStatstg->grfMode = grfStatFlag;
		hr = GetFileTime(&pStatstg->ctime, &pStatstg->atime, &pStatstg->mtime);
	}
	else
		hr = HRESULT_FROM_WIN32(GetLastError());

	return hr;
}

HRESULT WINAPI CFileStream::Duplicate (__deref_out ISeekableStream** ppDupStream)
{
	HRESULT hr;
	HANDLE hProcess = GetCurrentProcess();
	HANDLE hDupFile;

	if(DuplicateHandle(hProcess, m_hFile, hProcess, &hDupFile, 0, FALSE, DUPLICATE_SAME_ACCESS))
	{
		*ppDupStream = Alloc(hDupFile);
		if(*ppDupStream)
			hr = S_OK;
		else
		{
			CloseHandle(hDupFile);
			hr = E_OUTOFMEMORY;
		}
	}
	else
		hr = HRESULT_FROM_WIN32(GetLastError());

	return hr;
}

HRESULT CFileStream::Open (LPCTSTR pctzFile, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile, __deref_out CFileStream** ppstmFile, __out_opt ULARGE_INTEGER* puliSize)
{
	HRESULT hr;
	HANDLE hFile = CreateFile(pctzFile, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);

	CheckIfGetLastErrorNoTrace(INVALID_HANDLE_VALUE == hFile);

	*ppstmFile = __new CFileStream(hFile, TRUE);
	CheckAlloc(*ppstmFile);

	if(puliSize)
		puliSize->LowPart = GetFileSize(hFile, &puliSize->HighPart);

	hFile = INVALID_HANDLE_VALUE;
	hr = S_OK;

Cleanup:
	SafeCloseFileHandle(hFile);
	return hr;
}

HRESULT CFileStream::GetFileTime (__out_opt FILETIME* pftCreation, __out_opt FILETIME* pftLastAccess, __out_opt FILETIME* pftLastWrite)
{
	HRESULT hr;

	CheckIfGetLastError(!::GetFileTime(m_hFile, pftCreation, pftLastAccess, pftLastWrite));
	hr = S_OK;

Cleanup:
	return hr;
}

ISeekableStream* CFileStream::Alloc (HANDLE hFile)
{
	return __new CFileStream(hFile, TRUE);
}
