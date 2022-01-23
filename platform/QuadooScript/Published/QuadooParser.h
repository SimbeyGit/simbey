#pragma once

#define	FRAME_VARIABLE_OFFSET				2

#define	QUADOO_COMPILE_LINE_NUMBER_MAP		1
#define	QUADOO_PERMISSIVE_STATEMENT_ENDINGS	2
#define	QUADOO_COMPILE_FOR_DEBUGGING		4	// Requires QUADOO_COMPILE_LINE_NUMBER_MAP

interface IQuadooCompilerStatus
{
	virtual VOID STDMETHODCALLTYPE OnCompilerAddFile (PCWSTR pcwzFile, INT cchFile) = 0;
	virtual VOID STDMETHODCALLTYPE OnCompilerStatus (PCWSTR pcwzStatus) = 0;
	virtual VOID STDMETHODCALLTYPE OnCompilerError (HRESULT hrCode, INT nLine, PCWSTR pcwzFile, PCWSTR pcwzError) = 0;
};

HRESULT WINAPI QuadooParseToStream (PCWSTR pcwzFile, DWORD dwFlags, __out ISequentialStream* pstmBinaryScript, __out_opt ISequentialStream* pstmDebug, IQuadooCompilerStatus* pStatus);
HRESULT WINAPI QuadooParseTextToStream (PCWSTR pcwzFileTag, INT cchFileTag, PCWSTR pcwzText, INT cchText, DWORD dwFlags, __out ISequentialStream* pstmBinaryScript, __out_opt ISequentialStream* pstmDebug, IQuadooCompilerStatus* pStatus);
HRESULT WINAPI QuadooAllocStream (__deref_out ISequentialStream** ppStream);
DWORD WINAPI QuadooStreamDataSize (ISequentialStream* pStream);
