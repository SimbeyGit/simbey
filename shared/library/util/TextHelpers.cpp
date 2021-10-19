#include <windows.h>
#include "..\Core\CoreDefs.h"
#include "..\Core\StringCore.h"
#include "..\Core\Endian.h"
#include "TextHelpers.h"

namespace Text
{
	HRESULT WINAPI AllocateUnicode (const BYTE* pbRawText, ULONG cbRawText, PWSTR* ppwzText, INT* pcchText)
	{
		HRESULT hr;

		if(0 == (cbRawText % sizeof(WCHAR)))
		{
			INT cchText = cbRawText / sizeof(WCHAR);
			hr = TDuplicateCchString(reinterpret_cast<PCWSTR>(pbRawText), cchText, ppwzText);
			*pcchText = cchText;
		}
		else
			hr = HRESULT_FROM_WIN32(ERROR_INVALID_DATA);

		return hr;
	}

	HRESULT WINAPI AllocateUnicodeFromCodePage (const BYTE* pbRawText, ULONG cbRawText, PWSTR* ppwzText, INT* pcchText, UINT nCodePage)
	{
		HRESULT hr;
		DWORD dwFlags = (CP_UTF8 == nCodePage || CP_UTF7 == nCodePage) ? 0 : MB_PRECOMPOSED;
		*pcchText = MultiByteToWideChar(nCodePage, dwFlags, (PCSTR)pbRawText, cbRawText, NULL, 0);
		if(0 < *pcchText)
		{
			*ppwzText = __new WCHAR[*pcchText + 1];
			if(*ppwzText)
			{
				SideAssertCompare(MultiByteToWideChar(nCodePage, dwFlags, (PCSTR)pbRawText, cbRawText, *ppwzText, *pcchText + 1), *pcchText);
				(*ppwzText)[*pcchText] = L'\0';
				hr = S_OK;
			}
			else
				hr = E_OUTOFMEMORY;
		}
		else
			hr = HRESULT_FROM_WIN32(GetLastError());
		return hr;
	}

	HRESULT WINAPI AllocateUnicodeByteOrder (const BYTE* pbRawText, ULONG cbRawText, PWSTR* ppwzText, INT* pcchText, bool fIsBigEndian)
	{
		HRESULT hr = AllocateUnicode(pbRawText, cbRawText, ppwzText, pcchText);

		if(IsBigEndian() != fIsBigEndian)
		{
			// Now swap from big endian to little endian.  This will need to be revisited
			// should this code ever be compiled on a big endian platform.
			if(SUCCEEDED(hr))
			{
				USHORT* pusPtr = (USHORT*)*ppwzText;
				for(INT i = 0; i < *pcchText; i++)
					EndianSwap(pusPtr[i]);
			}
		}

		return hr;
	}

	HRESULT WINAPI ConvertRawTextToUnicode (const BYTE* pbRawText, ULONG cbRawText, PWSTR* ppwzText, INT* pcchText)
	{
		HRESULT hr;
		if(2 <= cbRawText && 0xFF == pbRawText[0] && 0xFE == pbRawText[1])			// UTF-16 LE
			hr = AllocateUnicodeByteOrder(pbRawText + 2, cbRawText - 2, ppwzText, pcchText, false);
		else if(2 <= cbRawText && 0xFE == pbRawText[0] && 0xFF == pbRawText[1])		// UTF-16 BE
			hr = AllocateUnicodeByteOrder(pbRawText + 2, cbRawText - 2, ppwzText, pcchText, true);
		else if(3 <= cbRawText && 0xEF == pbRawText[0] && 0xBB == pbRawText[1] && 0xBF == pbRawText[2])
			hr = AllocateUnicodeFromCodePage(pbRawText + 3, cbRawText - 3, ppwzText, pcchText, CP_UTF8);
		else
			hr = AllocateUnicodeFromCodePage(pbRawText, cbRawText, ppwzText, pcchText, CP_ACP);
		return hr;
	}

	HRESULT WINAPI SaveToHandle (PCWSTR pcwzText, INT cchText, HANDLE hFile)
	{
		static const BYTE bUTF[] = { 0xEF, 0xBB, 0xBF };
		HRESULT hr;
		ULONG cb;
		INT cchUTF8;
		PSTR pszUTF8 = NULL;

		CheckIf(NULL == pcwzText, E_INVALIDARG);

		cchUTF8 = WideCharToMultiByte(CP_UTF8, 0, pcwzText, cchText, NULL, 0, NULL, NULL);
		CheckIfGetLastError(0 == cchUTF8 && 0 != cchText);
		pszUTF8 = __new CHAR[cchUTF8 + 1];
		CheckAlloc(pszUTF8);
		WideCharToMultiByte(CP_UTF8, 0, pcwzText, cchText, pszUTF8, cchUTF8 + 1, NULL, NULL);

		CheckIfGetLastError(!WriteFile(hFile, bUTF, sizeof(bUTF), &cb, NULL));
		CheckIfGetLastError(!WriteFile(hFile, pszUTF8, cchUTF8, &cb, NULL));

		hr = S_OK;

	Cleanup:
		SafeDeleteArray(pszUTF8);
		return hr;
	}

	HRESULT WINAPI SaveToFile (PCWSTR pcwzText, INT cchText, PCWSTR pcwzFile)
	{
		HRESULT hr;
		HANDLE hFile = CreateFileW(pcwzFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		CheckIfGetLastError(INVALID_HANDLE_VALUE == hFile);

		Check(SaveToHandle(pcwzText, cchText, hFile));

	Cleanup:
		SafeCloseFileHandle(hFile);
		return hr;
	}

	HRESULT WINAPI SaveToFileA (PCWSTR pcwzText, INT cchText, PCSTR pcszFile)
	{
		HRESULT hr;
		HANDLE hFile = CreateFileA(pcszFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		CheckIfGetLastError(INVALID_HANDLE_VALUE == hFile);

		Check(SaveToHandle(pcwzText, cchText, hFile));

	Cleanup:
		SafeCloseFileHandle(hFile);
		return hr;
	}

	HRESULT WINAPI LoadFromHandle (HANDLE hFile, PWSTR* ppwzText, INT* pcchText)
	{
		HRESULT hr;
		ULONG cb;
		DWORD dwSize = GetFileSize(hFile, NULL);
		PBYTE pbData = __new BYTE[dwSize];

		CheckAlloc(pbData);

		if(0 < dwSize)
		{
			CheckIfGetLastError(!ReadFile(hFile, pbData, dwSize, &cb, NULL));
			Check(ConvertRawTextToUnicode(pbData, dwSize, ppwzText, pcchText));
		}
		else
		{
			// Reuse the zero-byte buffer.
			*ppwzText = reinterpret_cast<PWSTR>(pbData);
			pbData = NULL;
			*pcchText = 0;
			hr = S_OK;
		}

	Cleanup:
		SafeDeleteArray(pbData);
		return hr;
	}

	HRESULT WINAPI LoadFromFile (PCWSTR pcwzFile, PWSTR* ppwzText, INT* pcchText)
	{
		HRESULT hr;
		HANDLE hFile = CreateFileW(pcwzFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		CheckIfGetLastErrorIgnore(INVALID_HANDLE_VALUE == hFile, ERROR_FILE_NOT_FOUND);
		Check(LoadFromHandle(hFile, ppwzText, pcchText));

	Cleanup:
		SafeCloseFileHandle(hFile);
		return hr;
	}

	HRESULT WINAPI LoadFromFileA (PCSTR pcszFile, PWSTR* ppwzText, INT* pcchText)
	{
		HRESULT hr;
		HANDLE hFile = CreateFileA(pcszFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		CheckIfGetLastErrorIgnore(INVALID_HANDLE_VALUE == hFile, ERROR_FILE_NOT_FOUND);
		Check(LoadFromHandle(hFile, ppwzText, pcchText));

	Cleanup:
		SafeCloseFileHandle(hFile);
		return hr;
	}

	HRESULT WINAPI SaveToFileNoBOM (PCWSTR pcwzText, INT cchText, UINT nCodePage, PCWSTR pcwzFile)
	{
		HRESULT hr;
		ULONG cb;
		INT cchConverted;
		PSTR pszConverted = NULL;
		HANDLE hFile = INVALID_HANDLE_VALUE;

		CheckIf(NULL == pcwzText, E_INVALIDARG);

		hFile = CreateFileW(pcwzFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		CheckIfGetLastError(INVALID_HANDLE_VALUE == hFile);

		cchConverted = WideCharToMultiByte(nCodePage, 0, pcwzText, cchText, NULL, 0, NULL, NULL);
		CheckIfGetLastError(0 == cchConverted && 0 != cchText);
		pszConverted = __new CHAR[cchConverted + 1];
		CheckAlloc(pszConverted);
		WideCharToMultiByte(nCodePage, 0, pcwzText, cchText, pszConverted, cchConverted + 1, NULL, NULL);

		CheckIfGetLastError(!WriteFile(hFile, pszConverted, cchConverted, &cb, NULL));
		hr = S_OK;

	Cleanup:
		SafeDeleteArray(pszConverted);
		SafeCloseFileHandle(hFile);
		return hr;
	}
}
