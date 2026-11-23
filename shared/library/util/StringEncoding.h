#pragma once

#define	CCHMAX_QPLINE		72
#define	CCHMAX_BASE64_LINE	72

namespace StringEncoding
{
	inline LONG Base64Size (LONG cbInputData)
	{
		return ((4 * cbInputData / 3) + 3) & ~3;
	}

	inline LONG Base64LinesSize (LONG cbInputData)
	{
		LONG cbBase64 = Base64Size(cbInputData);
		INT cLines = (cbBase64 + (CCHMAX_BASE64_LINE - 1)) / CCHMAX_BASE64_LINE;
		return cbBase64 + cLines * 2;	// <CR> + <LF>
	}

	INT WINAPI EncodeBase64Fast (const VOID* pcvInput, LONG Length, LPSTR lpszOutput);
	INT WINAPI EncodeBase64 (const VOID* pcvInput, LONG Length, LPSTR lpszOutput);
	BOOL WINAPI DecodeBase64 (LPCSTR lpcszInput, LPSTR lpszOutput, LONG* lpcOutput);
	BOOL WINAPI DecodeCchBase64 (LPCSTR lpcszInput, INT cchInput, LPSTR lpszOutput, LONG* lpcOutput);

	INT WINAPI EncodeHexW (PVOID pvInput, LONG cbLength, PWSTR pwzOutput, LONG cColumns);

	INT WINAPI EncodeHex (LPVOID lpInput, LONG cLength, LPSTR lpszOutput, LONG cColumns);
	BOOL WINAPI DecodeHex (LPCSTR lpcszInput, LPSTR lpszOutput, LONG* lpcOutput);
	BOOL WINAPI DecodeHex (LPCSTR lpcszInput, LONG cLength, LPSTR lpszOutput, LONG* lpcOutput);

	INT WINAPI EncodeOctal (LPVOID lpInput, LONG cLength, LPSTR lpszOutput, LONG cColumns);
	BOOL WINAPI DecodeOctal (LPCSTR lpcszInput, LPSTR lpszOutput, LONG* lpcOutput);

	HRESULT WINAPI EncodeQuoted (PCSTR pcszText, LPSTR lpszOutput, ULONG cMaxOutput, ULONG* lpcOutput);
	UINT WINAPI DecodeQuoted (LPCSTR lpcszEncoded, LPSTR lpszDecoded, UINT cbDecoded);
	UINT WINAPI DecodeCchQuoted (LPCSTR lpcszEncoded, INT cchEncoded, LPSTR lpszDecoded, UINT cbDecoded);
}
