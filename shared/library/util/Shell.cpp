#include <windows.h>
#include <Shlobj.h>
#include "..\Core\CoreDefs.h"
#include "Shell.h"

HRESULT HrShellExecute (HWND hwnd, PCWSTR pcwzOperation, PCWSTR pcwzFile, PCWSTR pcwzParameters, PCWSTR pcwzDirectory, INT nShowCmd, __out LONG_PTR& nResult)
{
	HRESULT hr = S_OK;
	nResult = (LONG_PTR)ShellExecuteW(hwnd, pcwzOperation, pcwzFile, pcwzParameters, pcwzDirectory, nShowCmd);
	if(32 >= nResult)
	{
		switch(nResult)
		{
		case 0:
			hr = E_OUTOFMEMORY;
			break;
		case SE_ERR_ASSOCINCOMPLETE:
		case SE_ERR_DDEBUSY:
		case SE_ERR_DDEFAIL:
		case SE_ERR_DDETIMEOUT:
		case SE_ERR_DLLNOTFOUND:
			hr = E_FAIL;
			break;
		case SE_ERR_SHARE:
			hr = HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION);
			break;
		default:
			// SE_ERR_FNF, SE_ERR_OOM, SE_ERR_PNF, and SE_ERR_ACCESSDENIED map to Win32 errors.
			hr = HRESULT_FROM_WIN32((LONG)nResult);
			break;
		}
	}
	return hr;
}

HRESULT HrShellExecuteEx (LPSHELLEXECUTEINFOW psei)
{
	return ShellExecuteExW(psei) ? S_OK : HrEnsureLastError();
}

HRESULT HrCreateLink (PCWSTR pcwzPathObj, PCWSTR pcwzPathLink, PCWSTR pcwzDesc)
{
	HRESULT hr;
	IShellLink* psl = NULL;
	IPersistFile* ppf = NULL;

	// Get a pointer to the IShellLink interface. It is assumed that CoInitialize
	// has already been called.
	Check(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&psl)));

	// Set the path to the shortcut target and add the description.
	Check(psl->SetPath(pcwzPathObj));

	if(pcwzDesc)
		Check(psl->SetDescription(pcwzDesc));

	// Query IShellLink for the IPersistFile interface, used for saving the
	// shortcut in persistent storage.
	Check(psl->QueryInterface(IID_PPV_ARGS(&ppf)));

	// Save the link by calling IPersistFile::Save.
	Check(ppf->Save(pcwzPathLink, TRUE));

Cleanup:
	SafeRelease(ppf);
	SafeRelease(psl);
	return hr;
}
