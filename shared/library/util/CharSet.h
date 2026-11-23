#pragma once

#include "..\Core\MemoryStream.h"

#define	CP_ISO_8859_1		28591
#define	CP_WINDOWS_1252		1252

namespace CharSet
{
	BOOL WINAPI SetCodePage (__in_ecount(cchCharSet) PCSTR pcszCharSet, INT cchCharSet, __out UINT* pnCodePage);
	HRESULT WINAPI GetCodePageName (UINT nCodePage, __out_ecount(cchMaxName) PSTR pszName, INT cchMaxName, INT* pcchName);

#ifndef	MIN_CHARSET
	HRESULT WINAPI NormalizeToUTF8 (UINT nCodePage, LPCSTR lpcszText, INT cchText, LPSTR* lplpszUTF8, INT* lpcchUTF8);
	HRESULT WINAPI NormalizeToUTF8 (UINT nCodePage, LPCSTR lpcszText, INT cchText, __out CMemoryStream* pstmUTF8);
	HRESULT WINAPI DecodeCharSet (PCSTR pcszEncoded, INT cchEncoded, __out UINT* pnCodePage, __out INT* pcchRemaining = NULL, __out PCSTR* ppcszRemaining = NULL);
	HRESULT WINAPI DecodeCharSetToUTF8 (PCSTR pcszEncoded, INT cchEncoded, BOOL* lpfDecoded, LPSTR* lplpszDecoded, LONG* lpcchDecoded);
	HRESULT WINAPI DecodeMultiSetToUTF8 (PCSTR pcszText, INT cchText, __out CMemoryStream& Stream);
#endif	// MIN_CHARSET

#ifdef	UNICODE_SPLITS
	bool IsHighSurrogate (WCHAR wch);
	bool IsLowSurrogate (WCHAR wch);
	DWORD ReadCodePoint (PCWSTR pcwzText, INT cchText, INT idxText);
	DWORD ReadPreviousCodePoint (PCWSTR pcwzText, INT idxText, __out INT* pidxPrevious);
	bool IsUnicodeExtend (DWORD dwCodePoint);
	bool IsRegionalIndicator (DWORD dwCodePoint);
	INT GetHangulType (DWORD dwCodePoint);
	bool IsUnicodePrepend (DWORD dwCodePoint);
	bool IsVirama (DWORD dwCodePoint);
	bool IsJoinedUnicodePair (DWORD dwLeft, DWORD dwRight);
	INT FindUnicodeBreak (PCWSTR pcwzText, INT cchText, INT idxBreak);
#endif	// UNICODE_SPLITS
}
