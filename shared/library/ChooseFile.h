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
	// S_OK = accepted, S_FALSE = cancelled, failure = common-dialog/system error.
	// Filter is the usual double-NUL Win32 list. No file contents are written.
	HRESULT Choose (HWND hwnd, BOOL fOpen, LPCTSTR pctzTitle, LPCTSTR pctzName,
		LPCTSTR pctzFilter, LPCTSTR pctzDefaultExt, UINT nFilterIndex, DWORD dwFlags,
		__out UINT* pnSelectedFilter);
	LPCTSTR GetInitialDirectory (VOID);

	BOOL OpenSingleFile (HWND hwnd);
	BOOL OpenSingleFile (HWND hwnd, LPCTSTR lpctzFilter);
	BOOL OpenSingleFile (HWND hwnd, LPCTSTR lpctzDialogTitle, LPCTSTR lpctzFilter);

	INT OpenMultipleFiles (HWND hwnd);
	INT OpenMultipleFiles (HWND hwnd, LPCTSTR lpctzFilter);
	INT OpenMultipleFiles (HWND hwnd, LPCTSTR lpctzDialogTitle, LPCTSTR lpctzFilter);

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
