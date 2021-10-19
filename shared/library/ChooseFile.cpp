#include <windows.h>
#include "Core\CoreDefs.h"
#include "Core\StringCore.h"
#include "ChooseFile.h"

CChooseFile::CChooseFile ()
{
	ZeroMemory(&m_ofnOpen,sizeof(OPENFILENAME));
	m_ofnOpen.lStructSize = sizeof(OPENFILENAME);
	m_ofnOpen.hInstance = NULL;
	m_ofnOpen.lpstrCustomFilter = NULL;
	m_ofnOpen.nMaxCustFilter = 0;
	m_ofnOpen.nFilterIndex = 0;
	m_ofnOpen.nMaxFile = 65535;
	m_ofnOpen.lpstrFileTitle = NULL;
	m_ofnOpen.nMaxFileTitle = 65535;
	m_ofnOpen.lpstrInitialDir = NULL;
	m_ofnOpen.lpstrTitle = NULL;
	m_ofnOpen.nFileOffset = 0;
	m_ofnOpen.nFileExtension = 0;
	m_ofnOpen.lpstrDefExt = TEXT("txt");
	m_ofnOpen.lCustData = 0;
	m_ofnOpen.lpfnHook = NULL;
	m_ofnOpen.lpTemplateName = NULL;

	m_lpszName = NULL;
	m_lpszTitle = NULL;
	m_lpszDir = NULL;
	m_cchDir = 0;
}

CChooseFile::~CChooseFile ()
{
	FreeFileList();
	__delete_array m_ofnOpen.lpstrInitialDir;
	__delete_array m_lpszTitle;
	__delete_array m_lpszName;
}

HRESULT CChooseFile::Initialize (VOID)
{
	Assert(NULL == m_lpszName && NULL == m_lpszTitle);

	m_lpszName = __new TCHAR[m_ofnOpen.nMaxFile + 1];
	m_lpszTitle = __new TCHAR[m_ofnOpen.nMaxFileTitle + 1];

	return (m_lpszName && m_lpszTitle) ? S_OK : E_OUTOFMEMORY;
}

HRESULT CChooseFile::SetInitialDirectory (LPCTSTR pctzInitialDir)
{
	HRESULT hr = S_OK;
	SafeDeleteArray(m_ofnOpen.lpstrInitialDir);
	if(pctzInitialDir)
	{
		hr = TDuplicateStringAssert(pctzInitialDir, const_cast<LPTSTR*>(&m_ofnOpen.lpstrInitialDir));
	}
	return hr;
}

BOOL CChooseFile::OpenSingleFile (HWND hwnd)
{
	return OpenSingleFile(hwnd,TEXT("All Files\0*.*\0\0"));
}

BOOL CChooseFile::OpenSingleFile (HWND hwnd, LPCTSTR lpctzFilter)
{
	return OpenSingleFile(hwnd, NULL, lpctzFilter);
}

BOOL CChooseFile::OpenSingleFile (HWND hwnd, LPCTSTR lpctzDialogTitle, LPCTSTR lpctzFilter)
{
	return SUCCEEDED(Display(hwnd,TRUE,lpctzDialogTitle,NULL,lpctzFilter,OFN_HIDEREADONLY | OFN_CREATEPROMPT | OFN_EXPLORER)) && 1 == m_aFiles.Length();
}

INT CChooseFile::OpenMultipleFiles (HWND hwnd)
{
	return OpenMultipleFiles(hwnd,TEXT("All Files\0*.*\0\0"));
}

INT CChooseFile::OpenMultipleFiles (HWND hwnd, LPCTSTR lpctzFilter)
{
	INT cFiles = 0;
	if(SUCCEEDED(Display(hwnd,TRUE,NULL,NULL,lpctzFilter,OFN_ALLOWMULTISELECT | OFN_HIDEREADONLY | OFN_CREATEPROMPT | OFN_EXPLORER)))
	{
		cFiles = static_cast<INT>(m_aFiles.Length());
	}
	return cFiles;
}

BOOL CChooseFile::SaveFile (HWND hwnd)
{
	return SaveFile(hwnd,TEXT("All Files\0*.*\0\0"));
}

BOOL CChooseFile::SaveFile (HWND hwnd, PCTSTR pctzFilter)
{
	return SaveFile(hwnd, NULL, pctzFilter);
}

BOOL CChooseFile::SaveFile (HWND hwnd, LPCTSTR pctzDialogTitle, PCTSTR pctzFilter)
{
	return SUCCEEDED(Display(hwnd,FALSE,pctzDialogTitle,NULL,pctzFilter,OFN_HIDEREADONLY | OFN_CREATEPROMPT | OFN_EXPLORER | OFN_OVERWRITEPROMPT)) && 1 == m_aFiles.Length();
}

BOOL CChooseFile::SaveFile (HWND hwnd, LPCTSTR pctzDialogTitle, LPCTSTR pctzFileName, LPCTSTR pctzFilter)
{
	return SUCCEEDED(Display(hwnd,FALSE,pctzDialogTitle,pctzFileName,pctzFilter,OFN_HIDEREADONLY | OFN_CREATEPROMPT | OFN_EXPLORER | OFN_OVERWRITEPROMPT)) && 1 == m_aFiles.Length();
}

LPCTSTR CChooseFile::GetFile (INT index)
{
	return m_aFiles[index];
}

LPCTSTR CChooseFile::GetDir (__out_opt INT* pcchDir)
{
	if(pcchDir)
		*pcchDir = m_cchDir;
	return m_lpszDir;
}

INT CChooseFile::GetFileCount (VOID)
{
	return static_cast<INT>(m_aFiles.Length());
}

VOID CChooseFile::FreeFileList (VOID)
{
	for(INT i = 0; i < m_aFiles.Length(); i++)
	{
		__delete_array m_aFiles[i];
	}
	m_aFiles.Clear();

	SafeDeleteArrayCount(m_lpszDir, m_cchDir);
}

//

HRESULT CChooseFile::Display (HWND hwnd, BOOL bOpen, LPCTSTR pctzDialogTitle, LPCTSTR pctzFileName, LPCTSTR pctzFilter, DWORD dwFlags)
{
	HRESULT hr;
	LPTSTR ptzNew = NULL;
	INT cchName;

	Assert(m_lpszName && m_lpszTitle);

	ZeroMemory(m_lpszName, m_ofnOpen.nMaxFile + 1);
	ZeroMemory(m_lpszTitle, m_ofnOpen.nMaxFileTitle + 1);

	if(pctzFileName)
	{
		// This is the name that will show up in the dialog.
		Check(TStrCchCpy(m_lpszName, m_ofnOpen.nMaxFile + 1, pctzFileName));
	}

	m_ofnOpen.lpstrFilter = pctzFilter;
	m_ofnOpen.hwndOwner = hwnd;
	m_ofnOpen.lpstrFile = m_lpszName;
	m_ofnOpen.lpstrFileTitle = m_lpszTitle;
	m_ofnOpen.lpstrTitle = pctzDialogTitle;
	m_ofnOpen.Flags = dwFlags;

	FreeFileList();

	if(bOpen)
	{
		CheckIfGetLastError(!GetOpenFileName(&m_ofnOpen));
	}
	else
	{
		CheckIfGetLastError(!GetSaveFileName(&m_ofnOpen));
	}

	cchName = TStrLenAssert(m_lpszName);

	if((dwFlags & OFN_ALLOWMULTISELECT) && m_lpszName[cchName + 1] != 0)
	{
		LPTSTR lpszPtr;

		m_lpszDir = __new TCHAR[cchName + 1];
		CheckAlloc(m_lpszDir);

		Check(TStrCchCpy(m_lpszDir, cchName + 1, m_lpszName));
		m_cchDir = cchName;

		lpszPtr = m_lpszName + m_cchDir + 1;
		while(*lpszPtr)
		{
			Check(TDuplicateStringAssert(lpszPtr, &ptzNew, &cchName));
			Check(m_aFiles.Append(ptzNew));
			ptzNew = NULL;

			lpszPtr += cchName + 1;
		}
	}
	else
	{
		Check(TDuplicateStringAssert(m_lpszName, &ptzNew));
		Check(m_aFiles.Append(ptzNew));
		ptzNew = NULL;
	}

Cleanup:
	__delete_array ptzNew;
	return hr;
}