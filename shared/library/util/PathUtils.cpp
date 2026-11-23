#include <windows.h>
#include "..\Core\CoreDefs.h"
#include "..\Util\Formatting.h"
#include "PathUtils.h"

template <>
DWORD WINAPI TGetFileAttributes<CHAR> (PCSTR pctzFileName)
{
	return GetFileAttributesA(pctzFileName);
}

template <>
DWORD WINAPI TGetFileAttributes<WCHAR> (PCWSTR pctzFileName)
{
	return GetFileAttributesW(pctzFileName);
}

template <>
BOOL WINAPI TSHGetPathFromIDList (PCIDLIST_ABSOLUTE pidl, __out_ecount(MAX_PATH) PSTR ptzPath)
{
	return SHGetPathFromIDListA(pidl, ptzPath);
}

template <>
BOOL WINAPI TSHGetPathFromIDList (PCIDLIST_ABSOLUTE pidl, __out_ecount(MAX_PATH) PWSTR ptzPath)
{
	return SHGetPathFromIDListW(pidl, ptzPath);
}

namespace Path
{
	HRESULT PrepareFilePath (__in_ecount(cchFileTarget) PCWSTR pcwzFileTarget, INT cchFileTarget, __out_ecount(cchBase) PWSTR pwzBase, INT cchBase, PWSTR* ppwzBasePathPtr, INT* pcchBasePath)
	{
		HRESULT hr;
		PCWSTR pcwzFileTargetPathPtr = TStrCchRChr(pcwzFileTarget, cchFileTarget, L'\\');

		CheckIf(NULL == pcwzFileTargetPathPtr, E_INVALIDARG);
		pcwzFileTargetPathPtr++;

		Check(TStrCchCpyNEx(pwzBase, cchBase, pcwzFileTarget, static_cast<INT>(pcwzFileTargetPathPtr - pcwzFileTarget), ppwzBasePathPtr, pcchBasePath));

	Cleanup:
		return hr;
	}

	HRESULT PreparePathTarget (__in_ecount(cchPathTarget) PCWSTR pcwzPathTarget, INT cchPathTarget, __out_ecount(cchBase) PWSTR pwzBase, INT cchBase, PWSTR* ppwzBasePathPtr, INT* pcchBasePath)
	{
		HRESULT hr;

		CheckIf(0 == cchPathTarget, E_INVALIDARG);

		Check(TStrCchCpy(pwzBase, cchBase, pcwzPathTarget));

		if(L'\\' != pcwzPathTarget[cchPathTarget - 1])
		{
			Check(TStrCchCpy(pwzBase + cchPathTarget, cchBase - cchPathTarget, L"\\"));
			cchPathTarget++;
		}

		*ppwzBasePathPtr = pwzBase + cchPathTarget;
		*pcchBasePath = cchBase - cchPathTarget;

	Cleanup:
		return hr;
	}

	HRESULT BuildAbsolutePath (__in_ecount(cchFileTarget) PCWSTR pcwzFileTarget, INT cchFileTarget, __deref_out PWSTR* ppwzAbsolutePath, __out INT* pcchAbsolutePath)
	{
		HRESULT hr;

		PCWSTR pcwzPtr = TStrCchRChr(pcwzFileTarget, cchFileTarget, L'\\');
		WCHAR wzCurrentDir[MAX_PATH];
		INT cchCurrentDir;

		cchCurrentDir = GetCurrentDirectoryW(ARRAYSIZE(wzCurrentDir), wzCurrentDir);
		CheckIfGetLastError(0 == cchCurrentDir);

		if(NULL == pcwzPtr)
		{
			if(L'\\' != wzCurrentDir[cchCurrentDir - 1])
				wzCurrentDir[cchCurrentDir++] = L'\\';
			Check(TDuplicateCchString(wzCurrentDir, cchCurrentDir, ppwzAbsolutePath));
			*pcchAbsolutePath = cchCurrentDir;
		}
		else
		{
			Check(Formatting::TBuildDirectory(wzCurrentDir, cchCurrentDir, pcwzFileTarget, static_cast<INT>(pcwzPtr - pcwzFileTarget) + 1, ppwzAbsolutePath, pcchAbsolutePath));
		}

	Cleanup:
		return hr;
	}

	HRESULT CreateFolderPath (PCWSTR pcwzPath)
	{
		HRESULT hr;
		PCWSTR pcwzPtr = NULL;
		DWORD dwError;
		INT fCreatedAnyPart = FALSE;

		if(L'\\' == *pcwzPath)
			pcwzPtr = pcwzPath;

		for(;;)
		{
			WCHAR wzPart[MAX_PATH];
			INT cchPart;

			pcwzPtr = TStrChr(pcwzPtr ? pcwzPtr + 1 : pcwzPath, L'\\');
			if(NULL == pcwzPtr)
				break;
			cchPart = static_cast<INT>(pcwzPtr - pcwzPath);
			Check(TStrCchCpyN(wzPart, ARRAYSIZE(wzPart), pcwzPath, cchPart));
			if(CreateDirectoryW(wzPart, NULL))
				fCreatedAnyPart = TRUE;
			else
			{
				dwError = GetLastError();
				if(ERROR_ALREADY_EXISTS != dwError)
				{
					if(ERROR_ACCESS_DENIED != dwError || 2 < cchPart)
						Check(HRESULT_FROM_WIN32(dwError));
				}
			}
		}

		if(CreateDirectoryW(pcwzPath, NULL))
			fCreatedAnyPart = TRUE;
		else
		{
			dwError = GetLastError();
			if(ERROR_ALREADY_EXISTS != dwError)
				Check(HRESULT_FROM_WIN32(dwError));
		}

		hr = fCreatedAnyPart ? S_OK : S_FALSE;

	Cleanup:
		return hr;
	}

	HRESULT CopyFolderTree (PCWSTR pcwzSource, PCWSTR pcwzDestination, BOOL fFailIfExists)
	{
		HRESULT hr;
		DWORD dwSourceAttributes, dwDestinationAttributes;
		WCHAR wzSearch[MAX_PATH], wzSource[MAX_PATH], wzDestination[MAX_PATH];
		WIN32_FIND_DATAW FindData;
		HANDLE hFind = INVALID_HANDLE_VALUE;

		dwSourceAttributes = GetFileAttributesW(pcwzSource);
		CheckIfGetLastError(INVALID_FILE_ATTRIBUTES == dwSourceAttributes);
		CheckIf(0 == (dwSourceAttributes & FILE_ATTRIBUTE_DIRECTORY), HRESULT_FROM_WIN32(ERROR_DIRECTORY));
		CheckIf(dwSourceAttributes & FILE_ATTRIBUTE_REPARSE_POINT, HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED));

		if(!CreateDirectoryW(pcwzDestination, NULL))
		{
			CheckIfGetLastError(ERROR_ALREADY_EXISTS != GetLastError());
			CheckIf(fFailIfExists, HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS));
			dwDestinationAttributes = GetFileAttributesW(pcwzDestination);
			CheckIfGetLastError(INVALID_FILE_ATTRIBUTES == dwDestinationAttributes);
			CheckIf(0 == (dwDestinationAttributes & FILE_ATTRIBUTE_DIRECTORY), HRESULT_FROM_WIN32(ERROR_DIRECTORY));
		}

		Check(Formatting::TPrintF(wzSearch, ARRAYSIZE(wzSearch), NULL, L"%ls\\*", pcwzSource));
		hFind = FindFirstFileW(wzSearch, &FindData);
		CheckIfGetLastError(INVALID_HANDLE_VALUE == hFind);

		do
		{
			if(0 == TStrCmpAssert(FindData.cFileName, L".") || 0 == TStrCmpAssert(FindData.cFileName, L".."))
				continue;

			Check(Formatting::TPrintF(wzSource, ARRAYSIZE(wzSource), NULL, L"%ls\\%ls", pcwzSource, FindData.cFileName));
			Check(Formatting::TPrintF(wzDestination, ARRAYSIZE(wzDestination), NULL, L"%ls\\%ls", pcwzDestination, FindData.cFileName));

			if(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				Check(CopyFolderTree(wzSource, wzDestination, fFailIfExists));
			else
				CheckIfGetLastError(!CopyFileW(wzSource, wzDestination, fFailIfExists));
		} while(FindNextFileW(hFind, &FindData));

		CheckIfGetLastError(ERROR_NO_MORE_FILES != GetLastError());
		hr = S_OK;

	Cleanup:
		if(INVALID_HANDLE_VALUE != hFind)
			FindClose(hFind);
		return hr;
	}

	HRESULT DeleteFolderPathEx (__in_ecount(cchPath) PCWSTR pcwzPath, INT cchPath, BOOL fDeleteRoot, BOOL fClearROFlags, __inout_opt ULONG* pcDeleted, __inout_opt ULONG* pcErrors)
	{
		HRESULT hr;
		PWSTR pwzQuery = NULL;
		HANDLE hQuery = INVALID_HANDLE_VALUE;
		WIN32_FIND_DATAW FindData;

		CheckIf(0 == cchPath, E_INVALIDARG);

		pwzQuery = __new WCHAR[cchPath + MAX_PATH + 3];
		CheckAlloc(pwzQuery);

		// Copy the path into the query
		CopyMemory(pwzQuery, pcwzPath, cchPath * sizeof(WCHAR));

		// Add the back slash
		if(pwzQuery[cchPath - 1] != L'\\')
			pwzQuery[cchPath++] = L'\\';

		// Setup the query
		pwzQuery[cchPath] = L'*';
		pwzQuery[cchPath + 1] = L'\0';

		// Delete the folders first
		hQuery = FindFirstFileW(pwzQuery, &FindData);
		if(hQuery != INVALID_HANDLE_VALUE)
		{
			do
			{
				if(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					if(FindData.cFileName[0] != L'.' || (FindData.cFileName[1] != '\0' && (FindData.cFileName[1] != L'.' || FindData.cFileName[2] != L'\0')))
					{
						INT cchRemaining;
						Check(TStrCchCpyEx<WCHAR>(pwzQuery + cchPath, MAX_PATH, FindData.cFileName, NULL, &cchRemaining));
						Check(DeleteFolderPathEx(pwzQuery, cchPath + (MAX_PATH - cchRemaining), TRUE, fClearROFlags, pcDeleted, pcErrors));
					}
				}
			} while(FindNextFileW(hQuery,&FindData));
			FindClose(hQuery);
			hQuery = INVALID_HANDLE_VALUE;
		}

		// If the folders were deleted, now delete the files

		// Reset the query
		pwzQuery[cchPath] = L'*';
		pwzQuery[cchPath + 1] = L'\0';

		hQuery = FindFirstFileW(pwzQuery, &FindData);
		if(hQuery != INVALID_HANDLE_VALUE)
		{
			do
			{
				if((FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
				{
					Check(TStrCchCpy(pwzQuery + cchPath, MAX_PATH, FindData.cFileName));
					if(DeleteFileEx(pwzQuery, fClearROFlags))
					{
						if(pcDeleted)
							(*pcDeleted)++;
					}
					else if(pcErrors)
					{
						(*pcErrors)++;
					}
					else
					{
						Check(HRESULT_FROM_WIN32(GetLastError()));
					}
				}
			} while(FindNextFileW(hQuery,&FindData));
			FindClose(hQuery);
			hQuery = INVALID_HANDLE_VALUE;
		}

		if(fDeleteRoot)
		{
			// Finally, delete the original path
			pwzQuery[cchPath - 1] = L'\0';
			if(!RemoveDirectoryEx(pwzQuery, fClearROFlags))
			{
				CheckIfGetLastErrorIgnore(NULL == pcErrors, HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND));
				(*pcErrors)++;
			}
		}

		hr = S_OK;

	Cleanup:
		if(INVALID_HANDLE_VALUE != hQuery)
			FindClose(hQuery);
		__delete_array pwzQuery;
		return hr;
	}

	HRESULT DeleteFolderPath (PCWSTR pcwzPath)
	{
		return DeleteFolderPathEx(pcwzPath, TStrLenChecked(pcwzPath), TRUE, TRUE, NULL, NULL);
	}

	BOOL DeleteFileEx (PCWSTR pcwzFile, BOOL fClearROFlag)
	{
		BOOL fSuccess = DeleteFileW(pcwzFile);
		if(!fSuccess && fClearROFlag)
		{
			LONG nError = GetLastError();
			if(ERROR_ACCESS_DENIED == nError || ERROR_WRITE_PROTECT == nError)
			{
				if(SetFileAttributesW(pcwzFile, GetFileAttributesW(pcwzFile) & ~FILE_ATTRIBUTE_READONLY))
					fSuccess = DeleteFileW(pcwzFile);
			}
		}
		return fSuccess;
	}

	BOOL RemoveDirectoryEx (PCWSTR pcwzDir, BOOL fClearROFlag)
	{
		BOOL fSuccess = RemoveDirectoryW(pcwzDir);
		if(!fSuccess && fClearROFlag)
		{
			LONG nError = GetLastError();
			if(ERROR_ACCESS_DENIED == nError || ERROR_WRITE_PROTECT == nError)
			{
				if(SetFileAttributesW(pcwzDir, GetFileAttributesW(pcwzDir) & ~FILE_ATTRIBUTE_READONLY))
					fSuccess = RemoveDirectoryW(pcwzDir);
			}
		}
		return fSuccess;
	}

	HRESULT DeleteFiles (PCWSTR pcwzPath, INT cchPath, BOOL fClearROFlag, BOOL fCritical, BOOL (WINAPI* pfnCallback)(PCWSTR, PVOID), PVOID pvParam)
	{
		HRESULT hr;
		WIN32_FIND_DATAW FindData;
		HANDLE hFind = INVALID_HANDLE_VALUE;
		WCHAR wzBasePath[MAX_PATH], *pwzBasePtr;
		INT cchBasePtr;

		Check(PrepareFilePath(pcwzPath, cchPath, wzBasePath, ARRAYSIZE(wzBasePath), &pwzBasePtr, &cchBasePtr));

		hFind = FindFirstFileW(pcwzPath, &FindData);
		if(INVALID_HANDLE_VALUE != hFind)
		{
			do
			{
				Check(TStrCchCpy(pwzBasePtr, cchBasePtr, FindData.cFileName));

				if(NULL == pfnCallback || pfnCallback(wzBasePath, pvParam))
				{
					if(!DeleteFileEx(wzBasePath, fClearROFlag) && fCritical)
						Check(HRESULT_FROM_WIN32(GetLastError()));
				}
			} while(FindNextFileW(hFind, &FindData));
		}
		else if(fCritical)
		{
			DWORD dwError = GetLastError();
			if(ERROR_NO_MORE_FILES != dwError)
				Check(HRESULT_FROM_WIN32(dwError));
		}

	Cleanup:
		if(INVALID_HANDLE_VALUE != hFind)
			FindClose(hFind);
		return hr;
	}
}
