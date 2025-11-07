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

interface __declspec(uuid("8385B200-0BB2-4f27-A749-8BDE81807BBA")) IQuadooDefinitions : IUnknown
{
	virtual STDMETHODIMP_(DWORD) Count (VOID) = 0;
	virtual STDMETHODIMP GetDefinition (DWORD idxDef, __out RSTRING* prstrFile, __out INT* pnLine, __out PCWSTR* ppcwzToken) = 0;
};

interface __declspec(uuid("8CABAB92-4464-45e5-85C7-83C69B647B98")) IQuadooCompiler : IUnknown
{
	virtual STDMETHODIMP Initialize (BOOL fDebugging) = 0;
	virtual STDMETHODIMP AddMacro (PCWSTR pcwzMacro, PCWSTR* ppcwzArgs, INT cArgs, __in_ecount_opt(cchDef) PCWSTR pcwzDef, INT cchDef) = 0;
	virtual STDMETHODIMP CompileFile (PCWSTR pcwzFile, __out ISequentialStream* pstmBinaryScript, __out_opt ISequentialStream* pstmDebug, __deref_opt_out IQuadooDebugTree** ppDebugTree) = 0;
	virtual STDMETHODIMP CompileText (PCWSTR pcwzFileTag, INT cchFileTag, PCWSTR pcwzText, INT cchText, __out ISequentialStream* pstmBinaryScript, __out_opt ISequentialStream* pstmDebug, __deref_opt_out IQuadooDebugTree** ppDebugTree) = 0;
};

HRESULT WINAPI QuadooParseToStream (PCWSTR pcwzFile, DWORD dwFlags, __out ISequentialStream* pstmBinaryScript, __out_opt ISequentialStream* pstmDebug, IQuadooCompilerStatus* pStatus, __deref_opt_out IQuadooDebugTree** ppDebugTree = NULL);
HRESULT WINAPI QuadooParseTextToStream (PCWSTR pcwzFileTag, INT cchFileTag, PCWSTR pcwzText, INT cchText, DWORD dwFlags, __out ISequentialStream* pstmBinaryScript, __out_opt ISequentialStream* pstmDebug, IQuadooCompilerStatus* pStatus, __deref_opt_out IQuadooDebugTree** ppDebugTree = NULL);
HRESULT WINAPI QuadooAllocStream (__deref_out ISequentialStream** ppStream);
DWORD WINAPI QuadooStreamDataSize (ISequentialStream* pStream);

HRESULT WINAPI QuadooFindDefinitions (PCWSTR pcwzFile, PCWSTR pcwzDef, __deref_out IQuadooDefinitions** ppDefs);

HRESULT WINAPI QuadooCreateCompiler (DWORD dwFlags, IQuadooCompilerStatus* pStatus, __deref_out IQuadooCompiler** ppCompiler);
