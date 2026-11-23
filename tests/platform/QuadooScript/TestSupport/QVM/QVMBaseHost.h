#pragma once

#include "Library\Core\BaseUnknown.h"
#include "Library\Core\RStrMap.h"
#include "Library\Core\MemoryStream.h"
#include "Published\QuadooVM.h"
#include "IQVMHost.h"

class CBaseModule
{
public:
	CBaseModule () {}
	virtual ~CBaseModule () {}

	virtual HRESULT Initialize (IQuadooInstanceLoader* pLoader, IQuadooObject* pHost, RSTRING rstrModule) = 0;
	virtual HRESULT Create (QuadooVM::QVARIANT* pqv, __out QuadooVM::QVARIANT* pqvResult) = 0;
	virtual VOID DetachModuleToArray (TArray<HMODULE>& aModules) {}
};

class CQuadooModule : public CBaseModule
{
private:
	IQuadooVM* m_pVM;
	ULONG m_idxCreateObject;

public:
	CQuadooModule ();
	~CQuadooModule ();

	virtual HRESULT Initialize (IQuadooInstanceLoader* pLoader, IQuadooObject* pHost, RSTRING rstrModule);
	virtual HRESULT Create (QuadooVM::QVARIANT* pqv, __out QuadooVM::QVARIANT* pqvResult);

private:
	HRESULT LoadInternal (IQuadooObject* pHost);
};

class CNativeModule : public CBaseModule
{
private:
	HMODULE m_hModule;

public:
	CNativeModule ();
	~CNativeModule ();

	virtual HRESULT Initialize (IQuadooInstanceLoader* pLoader, IQuadooObject* pHost, RSTRING rstrModule);
	virtual HRESULT Create (QuadooVM::QVARIANT* pqv, __out QuadooVM::QVARIANT* pqvResult);
	virtual VOID DetachModuleToArray (TArray<HMODULE>& aModules);
};

class CPrintToHost :
	public CBaseUnknown,
	public IQuadooPrintTarget
{
protected:
	IQVMHost* m_pHost;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooPrintTarget)
	END_UNK_MAP

public:
	CPrintToHost (IQVMHost* pHost);
	virtual ~CPrintToHost ();

	// IQuadooPrintTarget
	virtual HRESULT STDMETHODCALLTYPE Print (RSTRING rstrText);
	virtual HRESULT STDMETHODCALLTYPE PrintLn (RSTRING rstrText);
};

#ifdef	WINCE

class CPrintToSerial :
	public CBaseUnknown,
	public IQuadooPrintTarget
{
public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooPrintTarget)
	END_UNK_MAP

	CPrintToSerial ();
	virtual ~CPrintToSerial ();

	// IQuadooPrintTarget
	virtual HRESULT Print (RSTRING rstrText);
	virtual HRESULT PrintLn (RSTRING rstrText);
};

#endif

class CInputSource :
	public CBaseUnknown,
	public IQuadooInputSource
{
protected:
	IQVMHost* m_pHost;
	CMemoryStream m_stmBuffer;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooInputSource)
	END_UNK_MAP

public:
	CInputSource (IQVMHost* pHost);
	~CInputSource ();

	// IQuadooInputSource
	virtual HRESULT STDMETHODCALLTYPE Read (__inout QuadooVM::QVARIANT* pqvRead);
};

class CQVMBaseHost :
	public CBaseUnknown,
	public CQuadooObjectImpl,
	public IQVMHost
{
private:
	IQuadooInstanceLoader* m_pLoader;
	TRStrMap<CBaseModule*> m_mapModules;
	TRStrMap<sysint> m_mapLinks;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooObject)
		UNK_INTERFACE(IQVMHost)
	END_UNK_MAP

public:
	CQVMBaseHost ();
	virtual ~CQVMBaseHost ();

	VOID AttachLoader (IQuadooInstanceLoader* pLoader);
	VOID UnloadModules (VOID);

	// IQuadooObject
	virtual HRESULT STDMETHODCALLTYPE Invoke (__in_opt IQuadooVM* pVM, RSTRING rstrMethod, QuadooVM::QVPARAMS* pqvParams, __out QuadooVM::QVARIANT* pqvResult);
	virtual HRESULT STDMETHODCALLTYPE GetProperty (__in_opt IQuadooVM* pVM, RSTRING rstrProperty, __out QuadooVM::QVARIANT* pqvResult);

	// Must be implemented by subclass
	virtual HRESULT MapPath (RSTRING rstrPath, __out RSTRING* prstrAbsolute) = 0;
	virtual HMODULE GetHostModule (VOID) = 0;

	static HRESULT ReadBinaryFile (RSTRING rstrFile, __out QuadooVM::QVARIANT* pqvFile);
	static HRESULT WriteBinaryFile (IQuadooObject* pFile, RSTRING rstrFile);
	static HRESULT GetEnvVariable (PCWSTR pcwzName, __out RSTRING* prstrValue);
	static HRESULT FileFromDecoder (QuadooVM::QVARIANT* pqvFileName, RSTRING rstrEncoded,
		HRESULT (WINAPI* pfnDecoderA)(PCSTR, INT, ISequentialStream*, DWORD*),
		__out QuadooVM::QVARIANT* pqvFile);
	static HRESULT FileFromByteArray (RSTRING rstrFile, IQuadooArray* pByteArray, __out QuadooVM::QVARIANT* pqvFile);
	static HRESULT ResolvePath (RSTRING rstrPath, RSTRING rstrRelative, __out RSTRING* prstrAbsolute);

	static HRESULT SetPrintTarget (IQVMHost* pHost, IQuadooVM* pVM);
	static HRESULT SetInputSource (IQVMHost* pHost, IQuadooVM* pVM);

protected:
	static HRESULT BinaryClone (QuadooVM::QVARIANT* pqvBinary, __out QuadooVM::QVARIANT* pqvResult);

	HRESULT BinaryWrite (QuadooVM::QVARIANT* pqvBinary);
	HRESULT WriteLink (IQuadooObject* pResponse, RSTRING rstrWebPath, __out QuadooVM::QVARIANT* pqvResult);
	HRESULT LoadQuadoo (QuadooVM::QVPARAMS* pqvParams, __out QuadooVM::QVARIANT* pqvResult);
	HRESULT OpenFolder (QuadooVM::QVPARAMS* pqvParams, __out QuadooVM::QVARIANT* pqvResult);
	HRESULT RegisterMemoryModule (IQuadooObject* pObject, __deref_out RSTRING* prstrModule);

	static HRESULT FileSearch (QuadooVM::QVPARAMS* pqvParams, __out QuadooVM::QVARIANT* pqvResult);

	static HRESULT FileAppend (QuadooVM::QVPARAMS* pqvParams, __out QuadooVM::QVARIANT* pqvResult);
	static HRESULT FileAppendString (RSTRING rstrValue, HANDLE hFile, UINT nCodePage, __out QuadooVM::QVARIANT* pqvResult);
	static HRESULT FileAppendBinary (IQuadooObject* pObject, HANDLE hFile, __out QuadooVM::QVARIANT* pqvResult);

	static HRESULT OpenResources (QuadooVM::QVPARAMS* pqvParams, __out QuadooVM::QVARIANT* pqvResult);
	static HRESULT OpenResourceEditor (QuadooVM::QVPARAMS* pqvParams, __out QuadooVM::QVARIANT* pqvResult);
	static HRESULT OpenLineReader (QuadooVM::QVPARAMS* pqvParams, __out QuadooVM::QVARIANT* pqvResult);
	static HRESULT OpenProcessObject (QuadooVM::QVPARAMS* pqvParams, __out QuadooVM::QVARIANT* pqvResult);
	static HRESULT OpenProcessList (__out QuadooVM::QVARIANT* pqvResult);
	static HRESULT Compile (QuadooVM::QVARIANT* pqvFileRef, QuadooVM::QVARIANT* pqvScript, QuadooVM::QVARIANT* pqvPermissive, QuadooVM::QVARIANT* pqvObject, QuadooVM::QVARIANT* pqvResult);

public:
#ifdef	WINCE
	static HRESULT CreateMessageQueue (QuadooVM::QVPARAMS* pqvParams, BOOL fReadMode, __out QuadooVM::QVARIANT* pqvResult);
#endif
	static HRESULT OpenFileFromParams (QuadooVM::QVPARAMS* pqvParams, DWORD dwDesiredAccess, DWORD dwShareMode, DWORD dwCreationDisposition, __out HANDLE* phFile);
	static HRESULT GetWindowsVersion (__out QuadooVM::QVARIANT* pqvResult);
	static HRESULT FetchDirectory (UINT (WINAPI* pfnGetDir)(PWSTR, UINT), __out QuadooVM::QVARIANT* pqvResult);
};
