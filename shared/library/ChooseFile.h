#pragma once

#include "Core\Array.h"

class CChooseFile
{
protected:
	OPENFILENAME m_ofnOpen;
	TArray<LPTSTR> m_aFiles;
	LPTSTR m_lpszName;
	LPTSTR m_lpszTitle;
	LPTSTR m_lpszDir;		// Set when using OFN_ALLOWMULTISELECT
	INT m_cchDir;			// Set when using OFN_ALLOWMULTISELECT

public:
	CChooseFile ();
	~CChooseFile ();

	HRESULT Initialize (VOID);
	HRESULT SetInitialDirectory (LPCTSTR pctzInitialDir);

	BOOL OpenSingleFile (HWND hwnd);
	BOOL OpenSingleFile (HWND hwnd, LPCTSTR lpctzFilter);
	BOOL OpenSingleFile (HWND hwnd, LPCTSTR lpctzDialogTitle, LPCTSTR lpctzFilter);

	INT OpenMultipleFiles (HWND hwnd);
	INT OpenMultipleFiles (HWND hwnd, LPCTSTR lpctzFilter);

	BOOL SaveFile (HWND hwnd);
	BOOL SaveFile (HWND hwnd, LPCTSTR pctzFilter);
	BOOL SaveFile (HWND hwnd, LPCTSTR pctzDialogTitle, LPCTSTR pctzFilter);
	BOOL SaveFile (HWND hwnd, LPCTSTR pctzDialogTitle, LPCTSTR pctzFileName, LPCTSTR pctzFilter);

	LPCTSTR GetFile (INT index);
	LPCTSTR GetDir (__out_opt INT* pcchDir = NULL);

	INT GetFileCount (VOID);
	VOID FreeFileList (VOID);

protected:
	HRESULT Display (HWND hwnd, BOOL bOpen, LPCTSTR pctzDialogTitle, LPCTSTR pctzFileName, LPCTSTR pctzFilter, DWORD dwFlags);
};