#pragma once

#include "BaseStream.h"

class CStaticStream : public CBaseStream
{
public:
	CStaticStream (PBYTE pBuffer, ULONG cbBuffer);
	CStaticStream (const BYTE* pcBuffer, ULONG cbBuffer);
	CStaticStream (PSTR pszBuffer, INT cchBuffer);
	CStaticStream (PCSTR pcszBuffer, INT cchBuffer);
	CStaticStream (PWSTR pwzBuffer, INT cchBuffer);
	CStaticStream (PCWSTR pcwzBuffer, INT cchBuffer);
	virtual ~CStaticStream ();

	template <typename T>
	inline T* GetStaticBuffer (VOID)
	{
		return reinterpret_cast<T*>(m_pBuffer);
	}

	inline ULONG GetStaticBufferSize (VOID)
	{
		return m_cbMaxBuffer;
	}

protected:
	virtual HRESULT GrowAndWrite (VOID const* lpcv, ULONG cb, ULONG* lpcbWritten);
};
