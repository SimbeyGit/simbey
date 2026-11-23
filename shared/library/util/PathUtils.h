#pragma once

#include <shlobj.h>
#include "..\Core\StringCore.h"

template <typename T>
DWORD WINAPI TGetFileAttributes (const T* pctzFileName)
{
	// This template must not be used.  Either TGetFileAttributes<CHAR> or TGetFileAttributes<WCHAR> should be selected.
	Assert(false);
	return INVALID_FILE_ATTRIBUTES;
}

template <>
DWORD WINAPI TGetFileAttributes<CHAR> (PCSTR pctzFileName);

template <>
DWORD WINAPI TGetFileAttributes<WCHAR> (PCWSTR pctzFileName);

template <typename T>
HRESULT WINAPI THrGetFileAttributes (const T* pctzFileName, __out DWORD* pdwAttribs)
{
	HRESULT hr = S_OK;

	Assert(NULL != pdwAttribs);

	*pdwAttribs = TGetFileAttributes(pctzFileName);
	if(INVALID_FILE_ATTRIBUTES == *pdwAttribs)
	{
		const T* pctzFilePtr = TStrRChr(pctzFileName, static_cast<T>('\\'));
		if(pctzFilePtr)
		{
			T tzPath[MAX_PATH];
			INT cchPath = static_cast<INT>(pctzFilePtr - pctzFileName);
			CopyMemory(tzPath, pctzFileName, cchPath * sizeof(T));
			tzPath[cchPath] = '\0';

			DWORD dwSubFolderAttribs = TGetFileAttributes(tzPath);
			if(INVALID_FILE_ATTRIBUTES == dwSubFolderAttribs || 0 == (dwSubFolderAttribs & FILE_ATTRIBUTE_DIRECTORY))
				hr = HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
			else
				hr = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
		}
		else
			hr = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
	}
	return hr;
}

template <typename T>
BOOL WINAPI TSHGetPathFromIDList (PCIDLIST_ABSOLUTE pidl, __out_ecount(MAX_PATH) T* ptzPath)
{
	// This template must not be used.  Either TSHGetPathFromIDList<CHAR> or TSHGetPathFromIDList<WCHAR> should be selected.
	Assert(false);
	return FALSE;
}

template <>
BOOL WINAPI TSHGetPathFromIDList<CHAR> (PCIDLIST_ABSOLUTE pidl, __out_ecount(MAX_PATH) PSTR ptzPath);

template <>
BOOL WINAPI TSHGetPathFromIDList<WCHAR> (PCIDLIST_ABSOLUTE pidl, __out_ecount(MAX_PATH) PWSTR ptzPath);

template <typename T>
HRESULT WINAPI TGetShellFolderLocation (INT nFolder, __out_ecount(MAX_PATH) T* ptzFolder)
{
	HRESULT hr;
	LPMALLOC pMalloc = NULL;
	LPITEMIDLIST pPidl = NULL;

	Check(SHGetMalloc(&pMalloc));
	Check(SHGetSpecialFolderLocation(NULL, nFolder, &pPidl));
	CheckIf(!TSHGetPathFromIDList(pPidl, ptzFolder), E_FAIL);

Cleanup:
	if(pMalloc)
	{
		if(pPidl)
			pMalloc->Free(pPidl);
		pMalloc->Release();
	}
	return hr;
}

namespace Path
{
	HRESULT PrepareFilePath (__in_ecount(cchFileTarget) PCWSTR pcwzFileTarget, INT cchFileTarget, __out_ecount(cchBase) PWSTR pwzBase, INT cchBase, PWSTR* ppwzBasePathPtr, INT* pcchBasePath);
	HRESULT PreparePathTarget (__in_ecount(cchPathTarget) PCWSTR pcwzPathTarget, INT cchPathTarget, __out_ecount(cchBase) PWSTR pwzBase, INT cchBase, PWSTR* ppwzBasePathPtr, INT* pcchBasePath);
	HRESULT BuildAbsolutePath (__in_ecount(cchFileTarget) PCWSTR pcwzFileTarget, INT cchFileTarget, __deref_out PWSTR* ppwzAbsolutePath, __out INT* pcchAbsolutePath);
	HRESULT CreateFolderPath (PCWSTR pcwzPath);
	HRESULT CopyFolderTree (PCWSTR pcwzSource, PCWSTR pcwzDestination, BOOL fFailIfExists);
	HRESULT DeleteFolderPathEx (__in_ecount(cchPath) PCWSTR pcwzPath, INT cchPath, BOOL fDeleteRoot, BOOL fClearROFlags, __inout_opt ULONG* pcDeleted /* Deleted Files */, __inout_opt ULONG* pcErrors /* Files or Folders */);
	HRESULT DeleteFolderPath (PCWSTR pcwzPath);
	BOOL DeleteFileEx (PCWSTR pcwzFile, BOOL fClearROFlag);
	BOOL RemoveDirectoryEx (PCWSTR pcwzDir, BOOL fClearROFlag);
	HRESULT DeleteFiles (PCWSTR pcwzPath, INT cchPath, BOOL fClearROFlag, BOOL fCritical, BOOL (WINAPI* pfnCallback)(PCWSTR, PVOID), PVOID pvParam);
}
