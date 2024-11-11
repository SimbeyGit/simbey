#include <windows.h>
#include "CoreDefs.h"
#include "BaseStream.h"

CBaseStream::CBaseStream ()
{
	m_cRef = 1;

	// It's up to subclasses to initialize m_pBuffer and m_cbMaxBuffer

	m_cbData = 0;
	m_iReadPtr = 0;
}

CBaseStream::~CBaseStream ()
{
}

// IUnknown

HRESULT WINAPI CBaseStream::QueryInterface (REFIID iid, LPVOID* lplpvObject)
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

ULONG WINAPI CBaseStream::AddRef (VOID)
{
	return (ULONG)InterlockedIncrement((LONG*)&m_cRef);
}

ULONG WINAPI CBaseStream::Release (VOID)
{
	ULONG c = (ULONG)InterlockedDecrement((LONG*)&m_cRef);
	if(c == 0)
		__delete this;
	return c;
}

// ISequentialStream

HRESULT WINAPI CBaseStream::Read (LPVOID lpv, ULONG cb, ULONG* lpcbRead)
{
	HRESULT hr = E_INVALIDARG;
	if(lpv && lpcbRead)
	{
		ULONG cbAvailable = m_cbData - m_iReadPtr;
		if(cb > cbAvailable)
			cb = cbAvailable;
		if(cb > 0)
		{
			CopyMemory(lpv,m_pBuffer + m_iReadPtr,cb);
			m_iReadPtr += cb;
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

HRESULT WINAPI CBaseStream::Write (VOID const* lpcv, ULONG cb, ULONG* lpcbWritten)
{
	HRESULT hr = E_INVALIDARG;
	if(lpcv && lpcbWritten)
	{
		if(m_cbData + cb <= m_cbMaxBuffer)
		{
			CopyMemory(m_pBuffer + m_cbData,lpcv,cb);
			m_cbData += cb;
			*lpcbWritten = cb;
			hr = S_OK;
		}
		else if(m_iReadPtr > 0)
		{
			MoveMemory(m_pBuffer,m_pBuffer + m_iReadPtr,m_cbData - m_iReadPtr);
			m_cbData -= m_iReadPtr;
			m_iReadPtr = 0;
			hr = Write(lpcv,cb,lpcbWritten);
		}
		else
			hr = GrowAndWrite(lpcv, cb, lpcbWritten);
	}
	return hr;
}

// ISeekableStream

HRESULT WINAPI CBaseStream::Seek (LARGE_INTEGER liDistanceToMove, DWORD dwOrigin, __out_opt ULARGE_INTEGER* puliNewPosition)
{
	HRESULT hr;
	ULONG ulPosition;

	if(liDistanceToMove.QuadPart < 0)
	{
		CheckIf(liDistanceToMove.QuadPart < INT_MIN, HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE));
		Check(Seek(static_cast<LONG>(liDistanceToMove.QuadPart), dwOrigin, &ulPosition));
	}
	else
	{
		CheckIf(liDistanceToMove.HighPart != 0, HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE));
		Check(Seek(liDistanceToMove.LowPart, dwOrigin, &ulPosition));
	}

	if(puliNewPosition)
	{
		puliNewPosition->LowPart = ulPosition;
		puliNewPosition->HighPart = 0;
	}

Cleanup:
	return hr;
}

HRESULT WINAPI CBaseStream::Stat (__out STATSTG* pStatstg, DWORD grfStatFlag)
{
	pStatstg->pwcsName = NULL;  // Regardless of grfStatFlag, this implementation does not know the name.
	pStatstg->cbSize.HighPart = 0;
	pStatstg->cbSize.LowPart = DataRemaining();
	pStatstg->type = STGTY_STORAGE;
	pStatstg->grfMode = grfStatFlag;
	ZeroMemory(&pStatstg->ctime, sizeof(pStatstg->ctime));
	ZeroMemory(&pStatstg->atime, sizeof(pStatstg->atime));
	ZeroMemory(&pStatstg->mtime, sizeof(pStatstg->mtime));
	return S_OK;
}

HRESULT WINAPI CBaseStream::Duplicate (__deref_out ISeekableStream** ppDupStream)
{
	return E_NOTIMPL;
}

VOID CBaseStream::Reset (VOID)
{
	m_cbData = 0;
	m_iReadPtr = 0;
}

ULONG CBaseStream::DataRemaining (VOID) const
{
	return m_cbData - m_iReadPtr;
}

ULONG CBaseStream::Capacity (VOID) const
{
	return m_cbMaxBuffer - DataRemaining();
}

HRESULT CBaseStream::Seek (LONG lMove, DWORD dwOrigin, __out ULONG* pulNewPosition)
{
	HRESULT hr = STG_E_INVALIDFUNCTION;

	switch(dwOrigin)
	{
	case STREAM_SEEK_SET:
		if(0 <= lMove && (ULONG)lMove <= m_cbData)
		{
			m_iReadPtr = lMove;
			*pulNewPosition = m_iReadPtr;
			hr = S_OK;
		}
		break;
	case STREAM_SEEK_CUR:
		*pulNewPosition = m_iReadPtr + lMove;
		if(*pulNewPosition <= m_cbData)
		{
			m_iReadPtr = *pulNewPosition;
			hr = S_OK;
		}
		break;
	case STREAM_SEEK_END:
		*pulNewPosition = m_cbData + lMove;
		if(*pulNewPosition <= m_cbData)
		{
			m_iReadPtr = *pulNewPosition;
			hr = S_OK;
		}
		break;
	}

	return hr;
}

HRESULT CBaseStream::CopyTo (PVOID pvDest, ULONG idxOffset, ULONG cbCopy)
{
	idxOffset += m_iReadPtr;
	if(idxOffset <= m_cbData)
	{
		ULONG cbAvailable = m_cbData - idxOffset;
		if(cbCopy <= cbAvailable)
		{
			CopyMemory(pvDest, m_pBuffer + idxOffset, cbCopy);
			return S_OK;
		}
	}
	return E_FAIL;
}

HRESULT CBaseStream::UpdateReadPtr (ULONG cbSkip)
{
	HRESULT hr;

	ULONG cbAvailable = m_cbData - m_iReadPtr;
	if(cbSkip <= cbAvailable)
	{
		m_iReadPtr += cbSkip;
		hr = S_OK;
	}
	else
		hr = E_FAIL;

	return hr;
}

BYTE const* CBaseStream::GetReadPtr (VOID) const
{
	return m_pBuffer + m_iReadPtr;
}

HRESULT CBaseStream::UpdateWritePtr (ULONG cbData)
{
	HRESULT hr;

	if(m_cbData + cbData <= m_cbMaxBuffer)
	{
		m_cbData += cbData;
		hr = S_OK;
	}
	else
		hr = E_FAIL;

	return hr;
}

PBYTE CBaseStream::GetWritePtr (__out ULONG* pcSlotsAvailable) const
{
	*pcSlotsAvailable = m_cbMaxBuffer - m_cbData;
	return m_pBuffer + m_cbData;
}

HRESULT CBaseStream::PopWritePtr (ULONG cbPop, __out_bcount_opt(cbProp) LPBYTE lpCopyWritePtr)
{
	HRESULT hr;

	if(m_cbData >= cbPop)
	{
		m_cbData -= cbPop;
		if(lpCopyWritePtr)
			CopyMemory(lpCopyWritePtr,m_pBuffer + m_cbData,cbPop);
		hr = S_OK;
	}
	else
		hr = E_FAIL;

	return hr;
}

HRESULT CBaseStream::Remove (ULONG nStart, ULONG cbRemove)
{
	HRESULT hr;
	ULONG nEnd = nStart + cbRemove;

	if(nEnd <= m_cbData)
	{
		MoveMemory(m_pBuffer + nStart, m_pBuffer + nEnd, m_cbData - nEnd);
		m_cbData -= cbRemove;
		if(nEnd <= m_iReadPtr)
			m_iReadPtr -= cbRemove;
		else if(nStart < m_iReadPtr)
			m_iReadPtr = nStart;
		hr = S_OK;
	}
	else
		hr = E_FAIL;

	return hr;
}

HRESULT CBaseStream::Condense (VOID)
{
	HRESULT hr = S_FALSE;
	if(0 < m_iReadPtr)
	{
		ULONG cbRemaining = DataRemaining();
		MoveMemory(m_pBuffer, m_pBuffer + m_iReadPtr, cbRemaining);
		m_cbData = cbRemaining;
		m_iReadPtr = 0;
		hr = S_OK;
	}
	return hr;
}
