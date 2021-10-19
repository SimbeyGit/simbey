#pragma once

#include "Library\Core\BaseUnknown.h"
#include "Library\Core\MemoryStream.h"
#include "Library\Util\FileStream.h"
#include "Published\JSON.h"
#include "Published\SIFPersistedStorage.h"

class CSIFFile :
	public CBaseUnknown,
	public IPersistedFile
{
private:
	ULARGE_INTEGER m_uliPosition;
	ULARGE_INTEGER m_uliSize;
	ISeekableStream* m_pPackage;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(ISequentialStream)
		UNK_INTERFACE(IPersistedFile)
	END_UNK_MAP

public:
	CSIFFile (const ULARGE_INTEGER& uliPosition, const ULARGE_INTEGER& uliSize, ISeekableStream* pPackage);
	~CSIFFile ();

	// ISequentialStream
	virtual HRESULT WINAPI Read (void* pv, ULONG cb, ULONG* pcbRead);
	virtual HRESULT WINAPI Write (const void* pv, ULONG cb, ULONG* pcbWritten);

	// IPersistedFile
	virtual HRESULT WINAPI Seek (DWORD dwPosition);
};

class CSIFPackage :
	public CBaseUnknown,
	public IPersistedStorage
{
private:
	CFileStream* m_pPackage;
	IJSONArray* m_pDirectory;
	ULARGE_INTEGER m_uliSize;

	RSTRING m_rstrThisName;

	RSTRING m_rstrName;
	RSTRING m_rstrDir;
	RSTRING m_rstrData;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IPersistedStorage)
	END_UNK_MAP

public:
	CSIFPackage ();
	CSIFPackage (CFileStream* pPackage, IJSONArray* pDirectory, const ULARGE_INTEGER& uliSize, RSTRING rstrThisName, RSTRING rstrName, RSTRING rstrDir, RSTRING rstrData);
	~CSIFPackage ();

	RSTRING GetName (VOID) { return m_rstrThisName; }
	HRESULT OpenPackage (PCWSTR pcwzPackage);
	HRESULT OpenDirectory (PCWSTR pcwzDirectory, INT cchDirectory, __deref_out CSIFPackage** ppDirectory);
	HRESULT OpenFile (PCWSTR pcwzFilePath, INT cchFilePath, __deref_out IPersistedFile** ppFile, __out_opt ULARGE_INTEGER* puliSize);
	HRESULT ReadFile (PCWSTR pcwzFilePath, INT cchFilePath, __out CMemoryStream* pstmFile);
	HRESULT OpenSIF (PCWSTR pcwzFilePath, __deref_out ISimbeyInterchangeFile** ppSIF);
	HRESULT GetJSONData (PCWSTR pcwzFilePath, INT cchFilePath, __deref_out IJSONValue** ppvData);
	HRESULT GetDirectory (__deref_out IJSONArray** ppDirectory);

	// IPersistedStorage
	virtual HRESULT WINAPI Open (PCWSTR pcwzFileName, DWORD dwDesiredAccess, DWORD dwShareMode, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, __deref_out IPersistedFile** ppFile);
	virtual HRESULT WINAPI Replace (PCWSTR pcwzReplacedFile, PCWSTR pcwzReplacementFile);
	virtual HRESULT WINAPI Move (PCWSTR pcwzExistingFile, PCWSTR pcwzNewFileName);
	virtual HRESULT WINAPI Delete (PCWSTR pcwzFile);

private:
	HRESULT FindDirectory (PCWSTR pcwzDirectory, INT cchDirectory, __deref_out IJSONArray** ppDirectory, __deref_out RSTRING* prstrName);
	HRESULT GetFileRecord (PCWSTR pcwzFilePath, INT cchFilePath, __deref_out IJSONObject** ppRecord);
};
