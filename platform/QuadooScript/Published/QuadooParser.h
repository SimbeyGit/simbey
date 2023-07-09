#pragma once

#define	FRAME_VARIABLE_OFFSET				2

#define	QUADOO_COMPILE_LINE_NUMBER_MAP		1
#define	QUADOO_PERMISSIVE_STATEMENT_ENDINGS	2
#define	QUADOO_COMPILE_FOR_DEBUGGING		4	// Requires QUADOO_COMPILE_LINE_NUMBER_MAP

typedef const BYTE* RSTRING;

interface IQuadooCompilerStatus
{
	virtual VOID STDMETHODCALLTYPE OnCompilerAddFile (PCWSTR pcwzFile, INT cchFile) = 0;
	virtual VOID STDMETHODCALLTYPE OnCompilerStatus (PCWSTR pcwzStatus) = 0;
	virtual VOID STDMETHODCALLTYPE OnCompilerError (HRESULT hrCode, INT nLine, PCWSTR pcwzFile, PCWSTR pcwzError) = 0;
	virtual STDMETHODIMP OnCompilerResolvePath (PCWSTR pcwzPath, __out_ecount(cchMaxAbsolutePath) PWSTR pwzAbsolutePath, INT cchMaxAbsolutePath) = 0;
};

struct SCOPE_PARENT
{
	PCWSTR pcwzType;
	RSTRING rstrNameW;
	DWORD idxCodePtr;
};

interface __declspec(uuid("8D833697-09F8-4daa-9DAD-0B90313FCDD5")) IQuadooEnumVariables : IUnknown
{
	virtual STDMETHODIMP EnumCallback (const SCOPE_PARENT& scope, RSTRING rstrVariableW, PCWSTR pcwzDeclaredType, BOOL fStatic, LONG idxOffset, __out BOOL* pfContinue) = 0;
};

interface __declspec(uuid("BE60854D-DAE9-4d51-BB6C-392FE4A15C41")) IQuadooDebugTree : IUnknown
{
	virtual STDMETHODIMP_(DWORD) GetFiles (VOID) = 0;
	virtual STDMETHODIMP GetFileString (DWORD idxFile, __out DWORD* pidFile) = 0;
	virtual STDMETHODIMP GetFilePath (DWORD idxFile, __deref_out RSTRING* prstrFileW) = 0;
	virtual STDMETHODIMP EnumVariables (RSTRING rstrFileW, INT nLineNumber, IQuadooEnumVariables* pEnum) = 0;
};

HRESULT WINAPI QuadooParseToStream (PCWSTR pcwzFile, DWORD dwFlags, __out ISequentialStream* pstmBinaryScript, __out_opt ISequentialStream* pstmDebug, IQuadooCompilerStatus* pStatus, __deref_opt_out IQuadooDebugTree** ppDebugTree = NULL);
HRESULT WINAPI QuadooParseTextToStream (PCWSTR pcwzFileTag, INT cchFileTag, PCWSTR pcwzText, INT cchText, DWORD dwFlags, __out ISequentialStream* pstmBinaryScript, __out_opt ISequentialStream* pstmDebug, IQuadooCompilerStatus* pStatus, __deref_opt_out IQuadooDebugTree** ppDebugTree = NULL);
HRESULT WINAPI QuadooAllocStream (__deref_out ISequentialStream** ppStream);
DWORD WINAPI QuadooStreamDataSize (ISequentialStream* pStream);
