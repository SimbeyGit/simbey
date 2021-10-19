#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "SIFPackage.h"

///////////////////////////////////////////////////////////////////////////////
// CSIFFile
///////////////////////////////////////////////////////////////////////////////

CSIFFile::CSIFFile (const ULARGE_INTEGER& uliPosition, const ULARGE_INTEGER& uliSize, ISeekableStream* pPackage) :
	m_uliPosition(uliPosition),
	m_uliSize(uliSize)
{
	SetInterface(m_pPackage, pPackage);
}

CSIFFile::~CSIFFile ()
{
	SafeRelease(m_pPackage);
}

// ISequentialStream

HRESULT WINAPI CSIFFile::Read (void* pv, ULONG cb, ULONG* pcbRead)
{
	return m_pPackage->Read(pv, cb, pcbRead);
}

HRESULT WINAPI CSIFFile::Write (const void* pv, ULONG cb, ULONG* pcbWritten)
{
	return E_NOTIMPL;
}

// IPersistedFile

HRESULT WINAPI CSIFFile::Seek (DWORD dwPosition)
{
	LARGE_INTEGER liMove;

	liMove.QuadPart = m_uliPosition.QuadPart;
	liMove.QuadPart += static_cast<LONGLONG>(dwPosition);

	return m_pPackage->Seek(liMove, STREAM_SEEK_SET, NULL);
}

///////////////////////////////////////////////////////////////////////////////
// CSIFPackage
///////////////////////////////////////////////////////////////////////////////

CSIFPackage::CSIFPackage () :
	m_pPackage(NULL),
	m_pDirectory(NULL),
	m_rstrThisName(NULL),
	m_rstrName(NULL),
	m_rstrDir(NULL),
	m_rstrData(NULL)
{
}

CSIFPackage::CSIFPackage (CFileStream* pPackage, IJSONArray* pDirectory, const ULARGE_INTEGER& uliSize, RSTRING rstrThisName, RSTRING rstrName, RSTRING rstrDir, RSTRING rstrData) :
	m_uliSize(uliSize)
{
	SetInterface(m_pPackage, pPackage);
	SetInterface(m_pDirectory, pDirectory);

	RStrSet(m_rstrThisName, rstrThisName);

	RStrSet(m_rstrName, rstrName);
	RStrSet(m_rstrDir, rstrDir);
	RStrSet(m_rstrData, rstrData);
}

CSIFPackage::~CSIFPackage ()
{
	RStrRelease(m_rstrThisName);

	RStrRelease(m_rstrData);
	RStrRelease(m_rstrDir);
	RStrRelease(m_rstrName);

	SafeRelease(m_pDirectory);
	SafeRelease(m_pPackage);
}

HRESULT CSIFPackage::OpenPackage (PCWSTR pcwzPackage)
{
	HRESULT hr;
	CHAR szSignature[4], *pszDir = NULL;
	ULONG cb, cbDir;
	FILETIME ft;
	LARGE_INTEGER liDir;
	RSTRING rstrDirW = NULL;
	TStackRef<IJSONValue> srv;

	Check(CFileStream::Open(pcwzPackage, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL, &m_pPackage, &m_uliSize));

	Check(m_pPackage->Read(szSignature, sizeof(szSignature), &cb));
	CheckIf('S' != szSignature[0] || 'P' != szSignature[1] || 'K' != szSignature[2] || 'G' != szSignature[3], E_FAIL);

	Check(m_pPackage->Read(&ft, sizeof(ft), &cb));
	Check(m_pPackage->Read(&liDir, sizeof(liDir), &cb));
	Check(m_pPackage->Read(&cbDir, sizeof(cbDir), &cb));

	Check(m_pPackage->Seek(liDir, STREAM_SEEK_SET, NULL));

	pszDir = __new CHAR[cbDir];
	CheckAlloc(pszDir);

	Check(m_pPackage->Read(pszDir, cbDir, &cb));
	Check(RStrConvertToW(CP_UTF8, cbDir, pszDir, &rstrDirW));

	Check(JSONParseWithDictionary(NULL, NULL, RStrToWide(rstrDirW), RStrLen(rstrDirW), &srv));
	Check(srv->GetArray(&m_pDirectory));

	Check(RStrCreateW(LSP(L"name"), &m_rstrName));
	Check(RStrCreateW(LSP(L"dir"), &m_rstrDir));
	Check(RStrCreateW(LSP(L"data"), &m_rstrData));

Cleanup:
	RStrRelease(rstrDirW);
	SafeDeleteArray(pszDir);
	return hr;
}

HRESULT CSIFPackage::OpenDirectory (PCWSTR pcwzDirectory, INT cchDirectory, __deref_out CSIFPackage** ppDirectory)
{
	HRESULT hr;
	TStackRef<IJSONArray> srDirectory;
	RSTRING rstrName = NULL;

	Check(FindDirectory(pcwzDirectory, cchDirectory, &srDirectory, &rstrName));

	*ppDirectory = __new CSIFPackage(m_pPackage, srDirectory, m_uliSize, rstrName, m_rstrName, m_rstrDir, m_rstrData);
	CheckAlloc(*ppDirectory);

Cleanup:
	RStrRelease(rstrName);
	return hr;
}

HRESULT CSIFPackage::OpenFile (PCWSTR pcwzFilePath, INT cchFilePath, __deref_out IPersistedFile** ppFile, __out_opt ULARGE_INTEGER* puliSize)
{
	HRESULT hr;
	TStackRef<IJSONObject> srFile;
	TStackRef<IJSONValue> srv;
	TStackRef<ISeekableStream> srClone;
	ULARGE_INTEGER uliPosition, uliSize;

	Check(GetFileRecord(pcwzFilePath, cchFilePath, &srFile));

	Check(srFile->FindNonNullValueW(L"file", &srv));
	srFile.Release();
	Check(srv->GetObject(&srFile));
	srv.Release();

	Check(srFile->FindNonNullValueW(L"pos", &srv));
	Check(srv->GetLongInteger(reinterpret_cast<LONGLONG*>(&uliPosition.QuadPart)));
	srv.Release();

	Check(srFile->FindNonNullValueW(L"size", &srv));
	Check(srv->GetLongInteger(reinterpret_cast<LONGLONG*>(&uliSize.QuadPart)));
	srv.Release();

	Check(m_pPackage->Duplicate(&srClone));

	*ppFile = __new CSIFFile(uliPosition, uliSize, srClone);
	CheckAlloc(*ppFile);

	hr = (*ppFile)->Seek(0);
	if(SUCCEEDED(hr))
	{
		if(puliSize)
			*puliSize = uliSize;
	}
	else
		SafeRelease(*ppFile);

Cleanup:
	return hr;
}

HRESULT CSIFPackage::ReadFile (PCWSTR pcwzFilePath, INT cchFilePath, __out CMemoryStream* pstmFile)
{
	HRESULT hr;
	TStackRef<IPersistedFile> srFile;
	ULARGE_INTEGER uliSize;
	PBYTE pbWrite;
	ULONG cbRead;

	Check(OpenFile(pcwzFilePath, cchFilePath, &srFile, &uliSize));
	CheckIf(0 != uliSize.HighPart, E_FAIL);
	Check(pstmFile->WriteAdvance(&pbWrite, uliSize.LowPart));
	Check(srFile->Read(pbWrite, uliSize.LowPart, &cbRead));

Cleanup:
	return hr;
}

HRESULT CSIFPackage::OpenSIF (PCWSTR pcwzFilePath, __deref_out ISimbeyInterchangeFile** ppSIF)
{
	return sifLoadFromStorage(this, pcwzFilePath, ppSIF);
}

HRESULT CSIFPackage::GetJSONData (PCWSTR pcwzFilePath, INT cchFilePath, __deref_out IJSONValue** ppvData)
{
	HRESULT hr;
	TStackRef<IJSONObject> srRecord;

	CheckNoTrace(GetFileRecord(pcwzFilePath, cchFilePath, &srRecord));
	Check(srRecord->FindNonNullValue(m_rstrData, ppvData));

Cleanup:
	return hr;
}

HRESULT CSIFPackage::GetDirectory (__deref_out IJSONArray** ppDirectory)
{
	SetInterface(*ppDirectory, m_pDirectory);
	return m_pDirectory ? S_OK : E_FAIL;
}

// IPersistedStorage

HRESULT WINAPI CSIFPackage::Open (PCWSTR pcwzFileName, DWORD dwDesiredAccess, DWORD dwShareMode, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, __deref_out IPersistedFile** ppFile)
{
	HRESULT hr;

	CheckIf(GENERIC_READ != dwDesiredAccess, E_ACCESSDENIED);
	Check(OpenFile(pcwzFileName, TStrLenAssert(pcwzFileName), ppFile, NULL));

Cleanup:
	return hr;
}

HRESULT WINAPI CSIFPackage::Replace (PCWSTR pcwzReplacedFile, PCWSTR pcwzReplacementFile)
{
	return E_NOTIMPL;
}

HRESULT WINAPI CSIFPackage::Move (PCWSTR pcwzExistingFile, PCWSTR pcwzNewFileName)
{
	return E_NOTIMPL;
}

HRESULT WINAPI CSIFPackage::Delete (PCWSTR pcwzFile)
{
	return E_NOTIMPL;
}

HRESULT CSIFPackage::FindDirectory (PCWSTR pcwzDirectory, INT cchDirectory, __deref_out IJSONArray** ppDirectory, __deref_out RSTRING* prstrName)
{
	HRESULT hr = S_OK;
	TStackRef<IJSONArray> srDirectory(m_pDirectory);
	PCWSTR pcwzStart = NULL;
	RSTRING rstrDir = NULL;

	for(INT i = 0; i <= cchDirectory; i++)
	{
		PCWSTR pcwzPtr = pcwzDirectory + i;

		if(i == cchDirectory || L'\\' == *pcwzPtr)
		{
			if(pcwzStart)
			{
				TStackRef<IJSONObject> srRecord;
				TStackRef<IJSONValue> srv;

				Check(RStrCreateW(static_cast<INT>(pcwzPtr - pcwzStart), pcwzStart, &rstrDir));
				Check(JSONFindArrayObject(srDirectory, m_rstrName, rstrDir, &srRecord, NULL));
				Check(srRecord->FindNonNullValue(m_rstrDir, &srv));

				srDirectory.Release();
				Check(srv->GetArray(&srDirectory));

				pcwzStart = NULL;
				RStrRelease(rstrDir); rstrDir = NULL;
			}
		}
		else if(NULL == pcwzStart)
			pcwzStart = pcwzPtr;
	}

	if(prstrName)
		*prstrName = rstrDir; rstrDir = NULL;
	*ppDirectory = srDirectory.Detach();

Cleanup:
	RStrRelease(rstrDir);
	return hr;
}

HRESULT CSIFPackage::GetFileRecord (PCWSTR pcwzFilePath, INT cchFilePath, __deref_out IJSONObject** ppRecord)
{
	HRESULT hr;
	TStackRef<IJSONArray> srDirectory;
	PCWSTR pcwzFileName = TStrCchRChr(pcwzFilePath, cchFilePath, L'\\');
	RSTRING rstrFileName = NULL;

	if(pcwzFileName)
	{
		Check(FindDirectory(pcwzFilePath, static_cast<INT>(pcwzFileName - pcwzFilePath), &srDirectory, NULL));
		pcwzFileName++;
	}
	else
	{
		srDirectory = m_pDirectory;
		pcwzFileName = pcwzFilePath;
	}

	Check(RStrCreateW(cchFilePath - static_cast<INT>(pcwzFileName - pcwzFilePath), pcwzFileName, &rstrFileName));
	CheckNoTrace(JSONFindArrayObject(srDirectory, m_rstrName, rstrFileName, ppRecord, NULL));

Cleanup:
	RStrRelease(rstrFileName);
	return hr;
}
