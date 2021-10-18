#pragma once

namespace Stream
{
	struct CopyBuffer
	{
		PVOID pvBuffer;
		DWORD cbBuffer;
	};

	interface __declspec(uuid("18E84893-6C0A-439f-9C3E-4642FA7D880F")) ICopyCallback : IUnknown
	{
		virtual VOID NotifyCopyStatus (ULONGLONG ullCopied, __out BOOL* pfAbort) = 0;
	};

	HRESULT CopyStream (ISequentialStream* pstmDest, ISequentialStream* pstmSrc, ULONGLONG ullCopy, __out_opt ULONGLONG* pullCopied, __in_opt CopyBuffer* pBuffer, __in_opt ICopyCallback* pCallback);
}
