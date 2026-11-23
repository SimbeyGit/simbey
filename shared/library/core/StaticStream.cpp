#include <windows.h>
#include "CoreDefs.h"
#include "StaticStream.h"

CStaticStream::CStaticStream (PBYTE pBuffer, ULONG cbBuffer)
{
	m_pBuffer = pBuffer;
	m_cbMaxBuffer = cbBuffer;
}

CStaticStream::CStaticStream (const BYTE* pcBuffer, ULONG cbBuffer)
{
	m_pBuffer = const_cast<BYTE*>(pcBuffer);
	m_cbMaxBuffer = cbBuffer;
	m_cbData = m_cbMaxBuffer;
}

CStaticStream::CStaticStream (PSTR pszBuffer, INT cchBuffer)
{
	m_pBuffer = reinterpret_cast<PBYTE>(pszBuffer);
	m_cbMaxBuffer = cchBuffer * sizeof(CHAR);
}

CStaticStream::CStaticStream (PCSTR pcszBuffer, INT cchBuffer)
{
	m_pBuffer = reinterpret_cast<PBYTE>(const_cast<PSTR>(pcszBuffer));
	m_cbMaxBuffer = cchBuffer * sizeof(CHAR);
	m_cbData = m_cbMaxBuffer;
}

CStaticStream::CStaticStream (PWSTR pwzBuffer, INT cchBuffer)
{
	m_pBuffer = reinterpret_cast<PBYTE>(pwzBuffer);
	m_cbMaxBuffer = cchBuffer * sizeof(WCHAR);
}

CStaticStream::CStaticStream (PCWSTR pcwzBuffer, INT cchBuffer)
{
	m_pBuffer = reinterpret_cast<PBYTE>(const_cast<PWSTR>(pcwzBuffer));
	m_cbMaxBuffer = cchBuffer * sizeof(WCHAR);
	m_cbData = m_cbMaxBuffer;
}

CStaticStream::~CStaticStream ()
{
}

HRESULT CStaticStream::GrowAndWrite (VOID const* lpcv, ULONG cb, ULONG* lpcbWritten)
{
	return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
}
