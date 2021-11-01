#pragma once

namespace Registry
{
	HRESULT WINAPI CreateKey (HKEY hOpenKey, LPCTSTR pctzSubKey, REGSAM samDesired, __out HKEY* phKey);
	HRESULT WINAPI WriteRegistryString (HKEY hKey, LPCTSTR pctzValueName, LPCTSTR pctzStringData, INT cchStringData);
	HRESULT WINAPI RegisterServer (HINSTANCE hServer, LPCTSTR pctzGuid, LPCTSTR pctzDescription, LPCTSTR pctzProgId);
	HRESULT WINAPI ApproveShellExt (LPCTSTR pctzGuid, LPCTSTR pctzDescription, BOOL fApprove);
	LONG WINAPI RegDeleteKeyRecursive (HKEY hKeyBase, LPCTSTR pctzKey);

	HRESULT WINAPI LoadCustomColors (__in_opt HKEY hKey, COLORREF* pcr, __inout INT* pcColors);
	HRESULT WINAPI SaveCustomColors (__in_opt HKEY hKey, COLORREF* pcr, INT cColors);

	HRESULT WINAPI RegisterFileType (LPCTSTR pctzExt, LPCTSTR pctzLabel, INT cchLabel, LPCTSTR pctzType, INT cchType, LPCTSTR pctzDesc, INT cchDesc, INT idxIcon, LPCTSTR pctzOpenCmd);
	HRESULT WINAPI UnregisterFileType (LPCTSTR pctzExt, LPCTSTR pctzLabel);

	HRESULT WINAPI GetRegistryHiveByName (LPCTSTR pctzHive, __out HKEY* phKey);
	BOOL WINAPI AdjustSubKeyForHive (LPCTSTR pctzSubKey, LPCTSTR* ppctzSubKey, __out HKEY* phKey);

	HRESULT WINAPI Install (LPCTSTR pctzScript, HKEY hKey = NULL);
}
