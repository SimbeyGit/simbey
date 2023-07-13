#pragma once

namespace Text
{
	HRESULT WINAPI AllocateUnicode (const BYTE* pbRawText, ULONG cbRawText, PWSTR* ppwzText, INT* pcchText);
	HRESULT WINAPI AllocateUnicodeFromCodePage (const BYTE* pbRawText, ULONG cbRawText, PWSTR* ppwzText, INT* pcchText, UINT nCodePage);
	HRESULT WINAPI AllocateUnicodeByteOrder (const BYTE* pbRawText, ULONG cbRawText, PWSTR* ppwzText, INT* pcchText, bool fIsBigEndian);
	HRESULT WINAPI ConvertRawTextToUnicode (const BYTE* pbRawText, ULONG cbRawText, PWSTR* ppwzText, INT* pcchText, UINT nCodePage = CP_ACP);

	HRESULT WINAPI SaveToHandle (PCWSTR pcwzText, INT cchText, HANDLE hFile);
	HRESULT WINAPI SaveToFile (PCWSTR pcwzText, INT cchText, PCWSTR pcwzFile);
	HRESULT WINAPI SaveToFileA (PCWSTR pcwzText, INT cchText, PCSTR pcszFile);
	HRESULT WINAPI LoadFromHandle (HANDLE hFile, PWSTR* ppwzText, INT* pcchText, UINT nCodePage = CP_ACP);
	HRESULT WINAPI LoadFromFile (PCWSTR pcwzFile, PWSTR* ppwzText, INT* pcchText, UINT nCodePage = CP_ACP);
	HRESULT WINAPI LoadFromFileA (PCSTR pcszFile, PWSTR* ppwzText, INT* pcchText, UINT nCodePage = CP_ACP);

	HRESULT WINAPI SaveToFileNoBOM (PCWSTR pcwzText, INT cchText, UINT nCodePage, PCWSTR pcwzFile);
}
