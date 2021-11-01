#include <malloc.h>
#include <windows.h>
#include "..\Core\CoreDefs.h"
#include "..\Core\Stack.h"
#include "Formatting.h"
#include "Registry.h"

namespace Registry
{
	const TCHAR c_tzDefaultColors[] = TEXT("Software\\Simbey\\Colors");
	const TCHAR c_tzCustomFmt[] = TEXT("Custom%d");

	HRESULT WINAPI CreateKey (HKEY hOpenKey, LPCTSTR pctzSubKey, REGSAM samDesired, __out HKEY* phKey)
	{
		HRESULT hr;

		if(AdjustSubKeyForHive(pctzSubKey, &pctzSubKey, &hOpenKey) && '\0' == *pctzSubKey)
		{
			*phKey = hOpenKey;
			hr = S_OK;
		}
		else
		{
			hr = HRESULT_FROM_WIN32(RegOpenKeyEx(hOpenKey, pctzSubKey, REG_OPTION_NON_VOLATILE, samDesired, phKey));
			if(FAILED(hr) && E_ACCESSDENIED != hr)
			{
				DWORD dwDisposition = 0;
				hr = HRESULT_FROM_WIN32(RegCreateKeyEx(hOpenKey, pctzSubKey, 0, NULL, REG_OPTION_NON_VOLATILE, samDesired, NULL, phKey, &dwDisposition));
			}
		}

		return hr;
	}

	HRESULT WINAPI WriteRegistryString (HKEY hKey, LPCTSTR pctzValueName, LPCTSTR pctzStringData, INT cchStringData)
	{
		HRESULT hr;
		LPTSTR ptzTemp = (LPTSTR)_malloca((cchStringData + 1) * sizeof(TCHAR));

		CheckAlloc(ptzTemp);
		CopyMemory(ptzTemp, pctzStringData, cchStringData * sizeof(TCHAR));
		ptzTemp[cchStringData] = '\0';
		CheckWin32Error(RegSetValueEx(hKey, pctzValueName, 0, REG_SZ, (LPBYTE)ptzTemp, (cchStringData + 1) * sizeof(TCHAR)));

	Cleanup:
		_freea(ptzTemp);
		return hr;
	}

	HRESULT WINAPI RegisterServer (HINSTANCE hServer, LPCTSTR pctzGuid, LPCTSTR pctzDescription, LPCTSTR pctzProgId)
	{
		HRESULT hr;
		HKEY hKey = NULL;
		HKEY hKeyPart = NULL;
		TCHAR tzClsId[MAX_PATH];
		TCHAR tzPath[MAX_PATH], *ptzPath = tzPath;
		INT cchPath;

		Check(Formatting::TPrintF(tzClsId, ARRAYSIZE(tzClsId), NULL, TEXT("Software\\Classes\\CLSID\\%s"), pctzGuid));
		Check(CreateKey(HKEY_LOCAL_MACHINE, tzClsId, KEY_ALL_ACCESS, &hKey));

		if(pctzDescription)
			Check(WriteRegistryString(hKey, NULL, pctzDescription, TStrLenAssert(pctzDescription)));

		Check(CreateKey(hKey, TEXT("InprocServer32"), KEY_ALL_ACCESS, &hKeyPart));
		cchPath = GetModuleFileName(hServer, tzPath, ARRAYSIZE(tzPath));
		if(tzPath[0] == '\\' && tzPath[1] == '\\' && tzPath[2] == '?' && tzPath[3] == '\\')
		{
			ptzPath += 4;
			cchPath -= 4;
		}

		Check(WriteRegistryString(hKeyPart, NULL, ptzPath, cchPath));
		Check(WriteRegistryString(hKeyPart, TEXT("ThreadingModel"), TEXT("Apartment"), 9));
		RegCloseKey(hKeyPart);
		hKeyPart = NULL;

		Check(CreateKey(hKey, TEXT("ProgID"), KEY_ALL_ACCESS, &hKeyPart));
		Check(WriteRegistryString(hKeyPart, NULL, pctzProgId, TStrLenAssert(pctzProgId)));
		RegCloseKey(hKeyPart);
		hKeyPart = NULL;

		RegCloseKey(hKey);
		hKey = NULL;

		Check(Formatting::TPrintF(tzPath, ARRAYSIZE(tzPath), NULL, TEXT("Software\\Classes\\%s"), pctzProgId));
		Check(CreateKey(HKEY_LOCAL_MACHINE, tzPath, KEY_ALL_ACCESS, &hKey));
		Check(CreateKey(hKey, TEXT("CLSID"), KEY_ALL_ACCESS, &hKeyPart));
		Check(WriteRegistryString(hKeyPart, NULL, pctzGuid, TStrLenAssert(pctzGuid)));

	Cleanup:
		if(hKeyPart)
			RegCloseKey(hKeyPart);
		if(hKey)
			RegCloseKey(hKey);

		return hr;
	}

	HRESULT WINAPI ApproveShellExt (LPCTSTR pctzGuid, LPCTSTR pctzDescription, BOOL fApprove)
	{
		HRESULT hr;
		HKEY hApproved = NULL;

		CheckWin32Error(RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved"), REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, &hApproved));
		if(fApprove)
			CheckWin32Error(RegSetValueEx(hApproved,pctzGuid,0,REG_SZ,(LPBYTE)pctzDescription,(TStrLenAssert(pctzDescription) + 1) * sizeof(TCHAR)));
		else
			CheckWin32Error(RegDeleteValue(hApproved, pctzGuid));

	Cleanup:
		RegCloseKey(hApproved);
		return hr;
	}

	LONG WINAPI RegDeleteKeyRecursive (HKEY hKeyBase, LPCTSTR pctzKey)
	{
		HKEY hKey;
		LONG lResult = RegOpenKeyEx(hKeyBase, pctzKey, 0, KEY_ALL_ACCESS, &hKey);
		if(ERROR_SUCCESS == lResult)
		{
			FILETIME ft;
			TCHAR tzBuffer[MAX_PATH];
			DWORD dwSize = ARRAYSIZE(tzBuffer);
			while(RegEnumKeyEx(hKey, 0, tzBuffer, &dwSize, NULL, NULL, NULL, &ft) == ERROR_SUCCESS)
			{
				lResult = RegDeleteKeyRecursive(hKey, tzBuffer);
				if(lResult != ERROR_SUCCESS)
					break;
				dwSize = ARRAYSIZE(tzBuffer);
			}
			RegCloseKey(hKey);
			lResult = RegDeleteKey(hKeyBase, pctzKey);
		}
		return lResult;
	}

	HRESULT WINAPI LoadCustomColors (__in_opt HKEY hKey, COLORREF* pcr, __inout INT* pcColors)
	{
		HRESULT hr;
		bool fOpenedKey = false;
		INT cColors = *pcColors;

		ZeroMemory(pcr, sizeof(COLORREF) * cColors);
		*pcColors = 0;

		if(NULL == hKey)
		{
			CheckWin32ErrorIgnore(RegOpenKeyEx(HKEY_CURRENT_USER, c_tzDefaultColors, REG_OPTION_NON_VOLATILE, KEY_READ, &hKey), HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
			fOpenedKey = true;
		}

		for(INT i = 0; i < cColors; i++)
		{
			DWORD cb = sizeof(COLORREF);
			TCHAR tzName[20];

			Check(Formatting::TPrintF(tzName, ARRAYSIZE(tzName), NULL, c_tzCustomFmt, i));
			if(ERROR_SUCCESS == RegQueryValueEx(hKey, tzName, NULL, NULL, reinterpret_cast<PBYTE>(pcr + i), &cb))
				(*pcColors)++;
		}

		hr = S_OK;

	Cleanup:
		if(fOpenedKey)
			RegCloseKey(hKey);
		return hr;
	}

	HRESULT WINAPI SaveCustomColors (__in_opt HKEY hKey, COLORREF* pcr, INT cColors)
	{
		HRESULT hr = S_FALSE;
		bool fOpenedKey = false;

		if(NULL == hKey)
		{
			Check(CreateKey(HKEY_CURRENT_USER, c_tzDefaultColors, KEY_ALL_ACCESS, &hKey));
			fOpenedKey = true;
		}

		for(INT i = 0; i < cColors; i++)
		{
			TCHAR tzName[20];

			Check(Formatting::TPrintF(tzName, ARRAYSIZE(tzName), NULL, c_tzCustomFmt, i));
			CheckWin32Error(RegSetValueEx(hKey, tzName, 0, REG_DWORD, reinterpret_cast<PBYTE>(pcr + i), sizeof(COLORREF)));
		}

	Cleanup:
		if(fOpenedKey)
			RegCloseKey(hKey);
		return hr;
	}

	HRESULT WINAPI RegisterFileType (LPCTSTR pctzExt, LPCTSTR pctzLabel, INT cchLabel, LPCTSTR pctzType, INT cchType, LPCTSTR pctzDesc, INT cchDesc, INT idxIcon, LPCTSTR pctzOpenCmd)
	{
		HRESULT hr;
		HKEY hKey = NULL;
		HKEY hKeyShell = NULL;
		HKEY hKeyCommand = NULL, hKeyAction = NULL;
		TCHAR tzModulePath[MAX_PATH], *ptzPath;
		TCHAR tzIcon[MAX_PATH];
		TCHAR tzCommand[MAX_PATH];
		INT cchPath, cchIcon, cchCommand;

		cchPath = GetModuleFileName(NULL, tzModulePath, ARRAYSIZE(tzModulePath));

		if(tzModulePath[0] == '\\' && tzModulePath[1] == '\\' && tzModulePath[2] == '?' && tzModulePath[3] == '\\')
		{
			ptzPath = tzModulePath + 4;
			cchPath -= 4;
		}
		else
			ptzPath = tzModulePath;

		Check(Registry::CreateKey(HKEY_CLASSES_ROOT, pctzExt, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &hKey));
		Check(Registry::WriteRegistryString(hKey, NULL, pctzLabel, cchLabel));
		Check(Registry::WriteRegistryString(hKey, TEXT("Content Type"), pctzType, cchType));
		RegCloseKey(hKey);
		hKey = NULL;

		Check(Registry::CreateKey(HKEY_CLASSES_ROOT, pctzLabel, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &hKey));
		Check(Registry::WriteRegistryString(hKey, NULL, pctzDesc, cchDesc));

		Check(Registry::CreateKey(hKey, TEXT("DefaultIcon"), KEY_ALL_ACCESS | KEY_WOW64_64KEY, &hKeyShell));
		Check(Formatting::TPrintF(tzIcon, ARRAYSIZE(tzIcon), &cchIcon, TEXT("%s,%d"), ptzPath, idxIcon));
		Check(Registry::WriteRegistryString(hKeyShell, NULL, tzIcon, cchIcon));
		RegCloseKey(hKeyShell);
		hKeyShell = NULL;

		Check(Registry::CreateKey(hKey, TEXT("shell"), KEY_ALL_ACCESS | KEY_WOW64_64KEY, &hKeyShell));
		Check(Registry::CreateKey(hKeyShell, TEXT("Open"), KEY_ALL_ACCESS | KEY_WOW64_64KEY, &hKeyAction));
		Check(Registry::WriteRegistryString(hKeyAction, NULL, SLP(TEXT("&Open"))));

		Check(Registry::CreateKey(hKeyAction, TEXT("command"), KEY_ALL_ACCESS | KEY_WOW64_64KEY, &hKeyCommand));
		Check(Formatting::TPrintF(tzCommand, ARRAYSIZE(tzCommand), &cchCommand, TEXT("\"%s\" %s \"%%L\""), ptzPath, pctzOpenCmd));
		Check(Registry::WriteRegistryString(hKeyCommand, NULL, tzCommand, cchCommand));

	Cleanup:
		if(hKeyCommand)
			RegCloseKey(hKeyCommand);
		if(hKeyAction)
			RegCloseKey(hKeyAction);
		if(hKeyShell)
			RegCloseKey(hKeyShell);
		if(hKey)
			RegCloseKey(hKey);
		return hr;
	}

	HRESULT WINAPI UnregisterFileType (LPCTSTR pctzExt, LPCTSTR pctzLabel)
	{
		HRESULT hr;

		CheckWin32Error(Registry::RegDeleteKeyRecursive(HKEY_CLASSES_ROOT, pctzExt));
		CheckWin32Error(Registry::RegDeleteKeyRecursive(HKEY_CLASSES_ROOT, pctzLabel));

	Cleanup:
		return hr;
	}

	HRESULT WINAPI GetRegistryHiveByName (LPCTSTR pctzHive, __out HKEY* phKey)
	{
		HRESULT hr = S_OK;

		if(0 == TStrCmpIAssert(pctzHive, TEXT("HKEY_LOCAL_MACHINE")) || 0 == TStrCmpIAssert(pctzHive, TEXT("HKLM")))
		{
			*phKey = HKEY_LOCAL_MACHINE;
		}
		else if(0 == TStrCmpIAssert(pctzHive, TEXT("HKEY_CURRENT_USER")) || 0 == TStrCmpIAssert(pctzHive, TEXT("HKCU")))
		{
			*phKey = HKEY_CURRENT_USER;
		}
		else if(0 == TStrCmpIAssert(pctzHive, TEXT("HKEY_CLASSES_ROOT")) || 0 == TStrCmpIAssert(pctzHive, TEXT("HKCR")))
		{
			*phKey = HKEY_CLASSES_ROOT;
		}
		else if(0 == TStrCmpIAssert(pctzHive, TEXT("HKEY_USERS")))
		{
			*phKey = HKEY_USERS;
		}
		else
		{
			hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
		}

		return hr;
	}

	HRESULT WINAPI FindEndAndCopy (LPCTSTR pctzText, TCHAR tchEnd, LPTSTR ptzPath, INT cchMaxPath, LPCTSTR* ppctzNext)
	{
		HRESULT hr;
		LPCTSTR pctzEnd;

		if('"' == tchEnd)
		{
			while('"' != *pctzText)
			{
				TCHAR tch = *pctzText;
				pctzText++;

				CheckIf('\0' == tch, E_INVALIDARG);
				if('\\' == tch)
				{
					tch = *pctzText;
					pctzText++;

					CheckIf('\0' == tch, E_INVALIDARG);

					CheckIf(0 == cchMaxPath, STRSAFE_E_INSUFFICIENT_BUFFER);
					*ptzPath = tch;
					ptzPath++;
					cchMaxPath--;
				}
				else
				{
					CheckIf(0 == cchMaxPath, STRSAFE_E_INSUFFICIENT_BUFFER);
					*ptzPath = tch;
					ptzPath++;
					cchMaxPath--;
				}
			}
			Check(TStrTerminateAssert(ptzPath, cchMaxPath));
			pctzEnd = pctzText;
		}
		else
		{
			pctzEnd = TStrChr(pctzText, tchEnd);
			CheckIf(NULL == pctzEnd, E_INVALIDARG);
			Check(TStrCchCpyN(ptzPath, cchMaxPath, pctzText, static_cast<INT>(pctzEnd - pctzText)));
		}
		*ppctzNext = pctzEnd + 1;

	Cleanup:
		return hr;
	}

	HRESULT WINAPI CopyAndResolve (LPCTSTR pctzText, TCHAR tchEnd, LPTSTR ptzValue, INT cchMaxValue, LPCTSTR* ppctzNext)
	{
		HRESULT hr;

		while(tchEnd != *pctzText)
		{
			TCHAR tch = *pctzText;
			CheckIf('\0' == tch, E_INVALIDARG);
			pctzText++;

			CheckIf(0 == cchMaxValue, STRSAFE_E_INSUFFICIENT_BUFFER);

			if('@' == tch)
			{
				TCHAR tzName[MAX_PATH];

				Check(FindEndAndCopy(pctzText, '@', tzName, ARRAYSIZE(tzName), &pctzText));
				if('\0' == *tzName)
				{
					*ptzValue = '@';
					ptzValue++;
					cchMaxValue--;
				}
				else
				{
					CheckIf(0 != TStrCmpIAssert(tzName, TEXT("MODULE")), E_INVALIDARG);
					*ptzValue = '"';
					DWORD cch = GetModuleFileName(NULL, ptzValue + 1, cchMaxValue - 1);

					ptzValue += cch + 1;
					cchMaxValue -= (cch + 1);

					CheckIf(0 == cchMaxValue, STRSAFE_E_INSUFFICIENT_BUFFER);
					*ptzValue = '"';
					ptzValue++;
					cchMaxValue--;
				}
			}
			else if('\\' == tch)
			{
				tch = *pctzText;
				CheckIf('\0' == tch, E_INVALIDARG);
				pctzText++;

				*ptzValue = tch;
				ptzValue++;
				cchMaxValue--;
			}
			else
			{
				*ptzValue = tch;
				ptzValue++;
				cchMaxValue--;
			}
		}

		Check(TStrTerminateAssert(ptzValue, cchMaxValue));
		*ppctzNext = pctzText + 1;

	Cleanup:
		return hr;
	}

	BOOL WINAPI AdjustSubKeyForHive (LPCTSTR pctzSubKey, LPCTSTR* ppctzSubKey, __out HKEY* phKey)
	{
		BOOL fAdjusted = FALSE;

		if('H' == TUpperCase(pctzSubKey[0]))
		{
			LPCTSTR pctzSlash = TStrChr(pctzSubKey, static_cast<TCHAR>('\\'));
			if(pctzSlash)
			{
				TCHAR tzHive[32];
				if(SUCCEEDED(TStrCchCpyN(tzHive, ARRAYSIZE(tzHive), pctzSubKey, static_cast<INT>(pctzSlash - pctzSubKey))) &&
					SUCCEEDED(GetRegistryHiveByName(tzHive, phKey)))
				{
					*ppctzSubKey = pctzSlash + 1;
					fAdjusted = TRUE;
				}
			}
			else if(SUCCEEDED(GetRegistryHiveByName(pctzSubKey, phKey)))
			{
				*ppctzSubKey = pctzSubKey + TStrLenAssert(pctzSubKey);
				fAdjusted = TRUE;
			}
		}

		return fAdjusted;
	}

	HRESULT WINAPI Install (LPCTSTR pctzScript, HKEY hKey)
	{
		HRESULT hr = S_FALSE;
		TStack<HKEY> aKeys;
		TCHAR tzPath[MAX_PATH]; tzPath[0] = '\0';

		while(*pctzScript)
		{
			TCHAR tch = *pctzScript;
			pctzScript++;

			if('-' == tch)
			{
				if('"' == *pctzScript)
				{
					Check(FindEndAndCopy(pctzScript, '"', tzPath, ARRAYSIZE(tzPath), &pctzScript));
					RegDeleteValue(hKey, tzPath);
					tzPath[0] = '\0';
				}
				else if('[' == *pctzScript)
				{
					Check(FindEndAndCopy(pctzScript, ']', tzPath, ARRAYSIZE(tzPath), &pctzScript));
					RegDeleteKeyRecursive(hKey, tzPath);
					tzPath[0] = '\0';
				}
				else
					Check(E_INVALIDARG);
			}
			else if('[' == tch)
				Check(FindEndAndCopy(pctzScript, ']', tzPath, ARRAYSIZE(tzPath), &pctzScript));
			else if('"' == tch || '@' == tch)
			{
				LPCTSTR pctzName;

				if('@' == tch)
					pctzName = NULL;
				else
				{
					Check(FindEndAndCopy(pctzScript, '"', tzPath, ARRAYSIZE(tzPath), &pctzScript));
					pctzName = tzPath;
				}

				while(Formatting::IsSpace(*pctzScript))
					pctzScript++;
				CheckIf('=' != *pctzScript, E_INVALIDARG);
				pctzScript++;
				while(Formatting::IsSpace(*pctzScript))
					pctzScript++;

				if('"' == *pctzScript)
				{
					TCHAR tzValue[MAX_PATH];
					Check(CopyAndResolve(pctzScript + 1, '"', tzValue, ARRAYSIZE(tzValue), &pctzScript));
					Check(WriteRegistryString(hKey, pctzName, tzValue, TStrLenAssert(tzValue)));
				}
				else if('0' == *pctzScript && 'x' == TLowerCase(pctzScript[1]))
				{
					DWORD dwValue = Formatting::TAscToXUInt32(pctzScript + 2, 16, &pctzScript);
					CheckWin32Error(RegSetValueEx(hKey, pctzName, 0, REG_DWORD, (PBYTE)&dwValue, sizeof(dwValue)));
				}
				else
					Check(E_INVALIDARG);
			}
			else if('{' == tch)
			{
				CheckIf('\0' == tzPath[0], E_INVALIDARG);
				Check(aKeys.Push(hKey));
				Check(CreateKey(hKey, tzPath, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &hKey));
				tzPath[0] = '\0';
			}
			else if('}' == tch)
			{
				RegCloseKey(hKey);
				Check(aKeys.Pop(&hKey));
			}
		}

		CheckIf(0 != aKeys.Length(), E_INVALIDARG);

	Cleanup:
		return hr;
	}
}
