#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Util\FileStream.h"
#include "Library\Util\Formatting.h"
#include "StringEncoding.h"
#include "CharSet.h"

#define	CP_ISO_8859_X		(CP_ISO_8859_1 - 1)

namespace CharSet
{
	BOOL WINAPI SetCodePage (__in_ecount(cchCharSet) PCSTR pcszCharSet, INT cchCharSet, __out UINT* pnCodePage)
	{
		BOOL fSuccess = TRUE;

		if(4 <= cchCharSet && TCompareLeftIAssert(pcszCharSet, "ansi") ||
			8 <= cchCharSet && TCompareLeftIAssert(pcszCharSet, "us-ascii"))
			*pnCodePage = CP_ACP;
		else if(9 < cchCharSet && TCompareLeftIAssert(pcszCharSet, "iso-8859-"))
			*pnCodePage = CP_ISO_8859_X + Formatting::TAscToUInt32(pcszCharSet + 9);
		else if(3 < cchCharSet && TCompareLeftIAssert(pcszCharSet, "utf"))
		{
			pcszCharSet += 3;
			cchCharSet -= 3;

			if('-' == *pcszCharSet)
			{
				pcszCharSet++;
				cchCharSet--;
			}

			if(1 == cchCharSet)
			{
				if('8' == *pcszCharSet)
					*pnCodePage = CP_UTF8;
				else if('7' == *pcszCharSet)
					*pnCodePage = CP_UTF7;
				else
					fSuccess = FALSE;
			}
			else
				fSuccess = FALSE;
		}
		else if(8 < cchCharSet && TCompareLeftIAssert(pcszCharSet, "windows-"))
			*pnCodePage = Formatting::TAscToUInt32(pcszCharSet + 8);
		else if(6 <= cchCharSet && TCompareLeftIAssert(pcszCharSet, "GB2312"))
			*pnCodePage = 936;
		else
			fSuccess = FALSE;

		return fSuccess;
	}

	HRESULT WINAPI GetCodePageName (UINT nCodePage, __out_ecount(cchMaxName) PSTR pszName, INT cchMaxName, INT* pcchName)
	{
		HRESULT hr;

		if(nCodePage >= 28591 && nCodePage <= 28599)
			Check(Formatting::TPrintF(pszName, cchMaxName, pcchName, "iso-8859-%d", nCodePage - 28590));
		else if(CP_UTF8 == nCodePage || CP_UTF7 == nCodePage)
			Check(Formatting::TPrintF(pszName, cchMaxName, pcchName, "utf-%d", nCodePage - (CP_UTF7 - 7)));
		else if((nCodePage >= 1250 && nCodePage <= 1258) || 874 == nCodePage)
			Check(Formatting::TPrintF(pszName, cchMaxName, pcchName, "windows-%d", nCodePage));
		else if(nCodePage == 936)
			Check(TStrCchCpyLen(pszName, cchMaxName, SLP("GB2312"), pcchName));
		else
			Check(E_INVALIDARG);

	Cleanup:
		return hr;
	}

#ifndef	MIN_CHARSET
	HRESULT WINAPI NormalizeToUTF8 (UINT nCodePage, LPCSTR lpcszText, INT cchText, LPSTR* lplpszUTF8, INT* lpcchUTF8)
	{
		HRESULT hr;
		INT cchWide = MultiByteToWideChar(nCodePage,0,lpcszText,cchText,NULL,0);
		if(0 < cchWide)
		{
			LPWSTR lpwzWide = __new WCHAR[cchWide + 1];
			if(lpwzWide)
			{
				SideAssertCompare(MultiByteToWideChar(nCodePage,0,lpcszText,cchText,lpwzWide,cchWide + 1),cchWide);

				*lpcchUTF8 = WideCharToMultiByte(CP_UTF8,0,lpwzWide,cchWide,NULL,0,NULL,NULL);
				if(0 < *lpcchUTF8)
				{
					*lplpszUTF8 = __new CHAR[*lpcchUTF8 + 1];
					if(*lplpszUTF8)
					{
						SideAssertCompare(WideCharToMultiByte(CP_UTF8,0,lpwzWide,cchWide,*lplpszUTF8,*lpcchUTF8 + 1,NULL,NULL),*lpcchUTF8);
						(*lplpszUTF8)[*lpcchUTF8] = '\0';

						hr = S_OK;
					}
					else
						hr = E_OUTOFMEMORY;
				}
				else
					hr = HrEnsureLastError();

				__delete_array lpwzWide;
			}
			else
				hr = E_OUTOFMEMORY;
		}
		else if(0 < cchText)
			hr = HrEnsureLastError();
		else
		{
			// These still have to be set to NULL/0 for the caller!
			*lplpszUTF8 = NULL;
			*lpcchUTF8 = 0;
			hr = S_OK;
		}
		return hr;
	}

	HRESULT WINAPI NormalizeToUTF8 (UINT nCodePage, LPCSTR lpcszText, INT cchText, __out CMemoryStream* pstmUTF8)
	{
		HRESULT hr;
		INT cchWide = MultiByteToWideChar(nCodePage,0,lpcszText,cchText,NULL,0);
		if(0 < cchWide)
		{
			LPWSTR lpwzWide = __new WCHAR[cchWide];
			if(lpwzWide)
			{
				SideAssertCompare(MultiByteToWideChar(nCodePage,0,lpcszText,cchText,lpwzWide,cchWide), cchWide);

				INT cchUTF8 = WideCharToMultiByte(CP_UTF8,0,lpwzWide,cchWide,NULL,0,NULL,NULL);
				if(0 < cchUTF8)
				{
					PSTR pszWritePtr;

					hr = pstmUTF8->TWriteAdvance(&pszWritePtr, cchUTF8);
					if(SUCCEEDED(hr))
						SideAssertCompare(WideCharToMultiByte(CP_UTF8, 0, lpwzWide, cchWide, pszWritePtr, cchUTF8, NULL, NULL), cchUTF8);
				}
				else
					hr = HrEnsureLastError();

				__delete_array lpwzWide;
			}
			else
				hr = E_OUTOFMEMORY;
		}
		else if(0 < cchText)
			hr = HrEnsureLastError();
		else
			hr = S_OK;
		return hr;
	}

	HRESULT WINAPI MapCharSetToCP (PCSTR pcszEncoded, INT cchEncoded, __out INT* pcchCharSet, __out UINT* pnCodePage)
	{
		HRESULT hr;
		HKEY hKey = NULL;
		INT cchRemaining;
		CHAR szPath[MAX_PATH], *pszPtr;
		PCSTR pcszEnd = TStrCchChr(pcszEncoded, cchEncoded, '?');
		DWORD cbSize = sizeof(UINT), dwType;

		Check(TStrCchCpyEx(szPath, ARRAYSIZE(szPath), "MIME\\Database\\Charset\\", &pszPtr, &cchRemaining));
		if(pcszEnd)
			*pcchCharSet = static_cast<INT>(pcszEnd - pcszEncoded);
		else
			*pcchCharSet = cchEncoded;
		Check(TStrCchCpyN(pszPtr, cchRemaining, pcszEncoded, *pcchCharSet));

		CheckWin32Error(RegOpenKeyExA(HKEY_CLASSES_ROOT, szPath, 0, KEY_READ, &hKey));
		CheckWin32Error(RegQueryValueExA(hKey, "CodePage", NULL, &dwType, (LPBYTE)pnCodePage, &cbSize));

	Cleanup:
		if(hKey)
			RegCloseKey(hKey);
		return hr;
	}

	HRESULT WINAPI DecodeCharSet (PCSTR pcszEncoded, INT cchEncoded, __out UINT* pnCodePage, __out INT* pcchRemaining, __out PCSTR* ppcszRemaining)
	{
		HRESULT hr = E_FAIL;
		PCSTR pcszPtr = pcszEncoded;
		INT cchPtr = cchEncoded;

		// Is it "ISO-" ?
		if((pcszPtr[0] == 'i' || pcszPtr[0] == 'I') && (pcszPtr[1] == 's' || pcszPtr[1] == 'S') && (pcszPtr[2] == 'o' || pcszPtr[2] == 'O') && pcszPtr[3] == '-')
		{
			pcszPtr += 4;
			cchPtr -= 4;

			// Is it "8859-" ?
			if(pcszPtr[0] == '8' && pcszPtr[1] == '8' && pcszPtr[2] == '5' && pcszPtr[3] == '9' && pcszPtr[4] == '-')
			{
				pcszPtr += 5;
				cchPtr -= 5;

				// Is it '1' through '9'?
				if(pcszPtr[0] >= '1' && pcszPtr[0] <= '9')
				{
					*pnCodePage = 28591 + (pcszPtr[0] - '1');

					pcszPtr++;
					cchPtr--;

					hr = S_OK;
				}
			}
			// Is it "2022-jp" ?
			else if(pcszPtr[0] == '2' && pcszPtr[1] == '0' && pcszPtr[2] == '2' && pcszPtr[3] == '2' && pcszPtr[4] == '-' &&
				(pcszPtr[5] == 'j' || pcszPtr[5] == 'J') && (pcszPtr[6] == 'p' || pcszPtr[6] == 'P'))
			{
				*pnCodePage = 50220;

				pcszPtr += 7;
				cchPtr -= 7;

				hr = S_OK;
			}
		}
		// Is it "UTF-" ?
		else if((pcszPtr[0] == 'u' || pcszPtr[0] == 'U') && (pcszPtr[1] == 't' || pcszPtr[1] == 'T') && (pcszPtr[2] == 'f' || pcszPtr[2] == 'F') && pcszPtr[3] == '-')
		{
			pcszPtr += 4;
			cchPtr -= 4;

			// Is it '7' or '8'?
			if(pcszPtr[0] == '7' || pcszPtr[0] == '8')
			{
				*pnCodePage = 65000 + (pcszPtr[0] - '7');

				pcszPtr++;
				cchPtr--;

				hr = S_OK;
			}
			// Is it "32" ?
			else if(pcszPtr[0] == '3' && pcszPtr[1] == '2')
			{
				pcszPtr += 2;
				cchPtr -= 2;

				// Is it "BE" ?
				if((pcszPtr[0] == 'b' || pcszPtr[0] == 'B') && (pcszPtr[1] == 'e' || pcszPtr[1] == 'E'))
				{
					*pnCodePage = 12000;

					pcszPtr += 2;
					cchPtr -= 2;
				}
				else
					*pnCodePage = 12001;

				hr = S_OK;
			}
		}
		// Is it "Windows-" ?
		else if((pcszPtr[0] == 'w' || pcszPtr[0] == 'W') && (pcszPtr[1] == 'i' || pcszPtr[1] == 'I') && (pcszPtr[2] == 'n' || pcszPtr[2] == 'N') && (pcszPtr[3] == 'd' || pcszPtr[3] == 'D') && (pcszPtr[4] == 'o' || pcszPtr[4] == 'O') && (pcszPtr[5] == 'w' || pcszPtr[5] == 'W') && (pcszPtr[6] == 's' || pcszPtr[6] == 'S') && pcszPtr[7] == '-')
		{
			pcszPtr += 8;
			cchPtr -= 8;

			for(INT i = 0; i <= cchPtr; i++)
			{
				if(i == cchPtr || '?' == pcszPtr[i])
				{
					if(0 < i)
					{
						*pnCodePage = Formatting::TAscToUInt32(pcszPtr);
						hr = S_OK;
					}

					pcszPtr += i;
					cchPtr -= i;
					break;
				}
			}
		}
		// Is it "cp#" ?
		else if((pcszPtr[0] == 'C' || pcszPtr[0] == 'c') && (pcszPtr[1] == 'P' || pcszPtr[1] == 'p') &&
			pcszPtr[2] >= '0' && pcszPtr[2] <= '9')
		{
			pcszPtr += 2;
			cchPtr -= 2;

			for(INT i = 1; i <= cchPtr; i++)
			{
				if(i == cchPtr || '?' == pcszPtr[i])
				{
					if(0 < i)
					{
						*pnCodePage = Formatting::TAscToUInt32(pcszPtr);
						hr = S_OK;
					}

					pcszPtr += i;
					cchPtr -= i;
					break;
				}
			}
		}
		// Is it "us-ascii" ?
		else if((pcszPtr[0] == 'U' || pcszPtr[0] == 'u') && (pcszPtr[1] == 'S' || pcszPtr[1] == 's') && pcszPtr[2] == '-' &&
			(pcszPtr[3] == 'A' || pcszPtr[3] == 'a') && (pcszPtr[4] == 'S' || pcszPtr[4] == 's') && (pcszPtr[5] == 'C' || pcszPtr[5] == 'c') && (pcszPtr[6] == 'I' || pcszPtr[6] == 'i') && (pcszPtr[7] == 'I' || pcszPtr[7] == 'i'))
		{
			*pnCodePage = CP_ACP;

			pcszPtr += 8;
			cchPtr -= 8;

			hr = S_OK;
		}
		// Is it "GB2312" ?
		else if((pcszPtr[0] == 'g' || pcszPtr[0] == 'G') && (pcszPtr[1] == 'b' || pcszPtr[1] == 'B') && pcszPtr[2] == '2' && pcszPtr[3] == '3' && pcszPtr[4] == '1' && pcszPtr[5] == '2')
		{
			*pnCodePage = 936;

			pcszPtr += 6;
			cchPtr -= 6;

			hr = S_OK;
		}

		if(SUCCEEDED(hr))
		{
			if(pcchRemaining)
				*pcchRemaining = cchPtr;
			if(ppcszRemaining)
				*ppcszRemaining = pcszPtr;
		}
		else
		{
			INT cchCharSet;

			if(SUCCEEDED(MapCharSetToCP(pcszEncoded, cchEncoded, &cchCharSet, pnCodePage)))
			{
				if(pcchRemaining)
					*pcchRemaining = cchEncoded - cchCharSet;
				if(ppcszRemaining)
					*ppcszRemaining = pcszEncoded + cchCharSet;
				hr = S_OK;
			}
		}

		return hr;
	}

	HRESULT WINAPI DecodeMultiSetToUTF8 (PCSTR pcszText, INT cchText, __out CMemoryStream& Stream)
	{
		HRESULT hr = S_FALSE;
		ULONG cb;
		LONG nStart = cchText;

		for(LONG i = 0; i < cchText; i++)
		{
			CHAR ch = pcszText[i];

			if('=' == ch && (i < cchText - 1 && '?' == pcszText[i + 1]))
			{
				LONG nEnd = i;
				LONG nCheckEnd = cchText - 3;
				LONG nEncoded = i + 2;
				bool fValid = false;

				for(LONG n = nEncoded; n < nCheckEnd; n++)
				{
					if('?' == pcszText[n] && '?' == pcszText[n + 2])
					{
						// Skip ahead to ignore any possibility of "?=" occurring at the beginning of a quoted printable encoding.
						i = n + 2;
						fValid = true;
						break;
					}
				}

				if(fValid)
				{
					fValid = false;
					nCheckEnd = cchText - 1;

					for(LONG n = i + 1; n < nCheckEnd; n++)
					{
						if('?' == pcszText[n] && '=' == pcszText[n + 1])
						{
							i = n;
							fValid = true;
							break;
						}
					}
				}

				if(fValid)
				{
					BOOL fDecoded = FALSE;
					LPSTR lpszDecoded;
					LONG cchDecoded;

					if(cchText != nStart)
					{
						Check(Stream.Write(pcszText + nStart, nEnd - nStart, &cb));
						nStart = cchText;
					}

					hr = DecodeCharSetToUTF8(pcszText + nEncoded, i - nEncoded, &fDecoded, &lpszDecoded, &cchDecoded);
					CheckIf(FAILED(hr) && E_FAIL != hr, hr);

					if(fDecoded)
					{
						hr = Stream.Write(lpszDecoded,cchDecoded,&cb);
						__delete_array lpszDecoded;
					}
					else
					{
						// If the encoding wasn't understood, just write the original text.
						hr = Stream.Write(pcszText + nEnd, (i - nEnd) + 2, &cb);
					}
					Check(hr);

					i++;
				}
			}
			else if(cchText == nStart)
				nStart = i;
		}

		if(cchText != nStart)
			Check(Stream.Write(pcszText + nStart, cchText - nStart, &cb));

	Cleanup:
		return hr;
	}

	HRESULT WINAPI DecodeCharSetToUTF8 (PCSTR pcszEncoded, INT cchEncoded, BOOL* lpfDecoded, LPSTR* lplpszDecoded, LONG* lpcchDecoded)
	{
		// This protected method assumes that the text surrounding pcszEncoded
		// begins with "=?" and ends with "?="
		UINT nCodePage = 0;
		HRESULT hr = DecodeCharSet(pcszEncoded, cchEncoded, &nCodePage, &cchEncoded, &pcszEncoded);
		if(SUCCEEDED(hr) && 3 <= cchEncoded && pcszEncoded[0] == '?' && pcszEncoded[2] == '?')
		{
			LPSTR lpszDecoded;
			LONG cchDecoded;
			CHAR chType = TUpperCase(pcszEncoded[1]);

			pcszEncoded += 3;
			cchEncoded -= 3;

			cchDecoded = (cchEncoded << 1) + 1;
			lpszDecoded = __new CHAR[cchDecoded];
			if(lpszDecoded)
			{
				BOOL fDecoded = FALSE;

				// Is it Base64 encoded?
				if('B' == chType)
					fDecoded = StringEncoding::DecodeCchBase64(pcszEncoded,cchEncoded,lpszDecoded,&cchDecoded);
				// Is it Quoted-Printable encoded?
				else if('Q' == chType)
				{
					cchDecoded = StringEncoding::DecodeCchQuoted(pcszEncoded,cchEncoded,lpszDecoded,cchDecoded);
					fDecoded = (0 < cchDecoded);
				}

				// If it's not UTF8, then it must be normalized to UTF8.
				if(fDecoded)
				{
					lpszDecoded[cchDecoded] = '\0';

					for(INT i = 0; i < cchDecoded; i++)
					{
						// Replace any NIL characters with spaces.
						if('\0' == lpszDecoded[i])
							lpszDecoded[i] = ' ';
					}

					if(CP_UTF8 != nCodePage)
					{
						LPSTR lpszUTF8;
						INT cchUTF8;

						hr = NormalizeToUTF8(nCodePage,lpszDecoded,cchDecoded,&lpszUTF8,&cchUTF8);
						__delete_array lpszDecoded;	// Either way, it's no longer needed.
						if(SUCCEEDED(hr))
						{
							lpszDecoded = lpszUTF8;
							cchDecoded = cchUTF8;
						}
						else
						{
							lpszDecoded = NULL;
							fDecoded = FALSE;
						}
					}
					else
						hr = S_OK;
				}
				else
				{
					__delete_array lpszDecoded;
					lpszDecoded = NULL;
					cchDecoded = 0;
				}

				*lpfDecoded = fDecoded;
				*lplpszDecoded = lpszDecoded;
				*lpcchDecoded = cchDecoded;
			}
			else
				hr = E_OUTOFMEMORY;
		}

		return hr;
	}
#endif	// MIN_CHARSET

#ifdef	UNICODE_SPLITS
	bool IsHighSurrogate (WCHAR wch)
	{
		return 0xD800 <= wch && wch <= 0xDBFF;
	}

	bool IsLowSurrogate (WCHAR wch)
	{
		return 0xDC00 <= wch && wch <= 0xDFFF;
	}

	DWORD ReadCodePoint (PCWSTR pcwzText, INT cchText, INT idxText)
	{
		DWORD dwCodePoint = pcwzText[idxText];

		if(IsHighSurrogate(pcwzText[idxText]) && idxText + 1 < cchText &&
			IsLowSurrogate(pcwzText[idxText + 1]))
		{
			dwCodePoint = 0x10000 +
				((pcwzText[idxText] - 0xD800) << 10) +
				(pcwzText[idxText + 1] - 0xDC00);
		}

		return dwCodePoint;
	}

	DWORD ReadPreviousCodePoint (PCWSTR pcwzText, INT idxText, __out INT* pidxPrevious)
	{
		INT idxPrevious = idxText - 1;

		if(0 < idxPrevious && IsLowSurrogate(pcwzText[idxPrevious]) &&
			IsHighSurrogate(pcwzText[idxPrevious - 1]))
		{
			idxPrevious--;
		}

		*pidxPrevious = idxPrevious;
		return ReadCodePoint(pcwzText, idxText, idxPrevious);
	}

	bool IsUnicodeExtend (DWORD dwCodePoint)
	{
		// These supplementary characters are not reliably classified by the
		// older GetStringTypeW() implementations supported by this project.
		if((0x1F3FB <= dwCodePoint && dwCodePoint <= 0x1F3FF) ||	// Emoji modifiers
			(0xE0020 <= dwCodePoint && dwCodePoint <= 0xE007F) ||	// Emoji tags
			(0xE0100 <= dwCodePoint && dwCodePoint <= 0xE01EF))		// Variation selectors
		{
			return true;
		}

		if(0x200C == dwCodePoint ||
			(0xFE00 <= dwCodePoint && dwCodePoint <= 0xFE0F))
		{
			return true;
		}

		if(dwCodePoint <= 0xFFFF)
		{
			WCHAR wch = static_cast<WCHAR>(dwCodePoint);
			WORD wType;

			if(GetStringTypeW(CT_CTYPE3, &wch, 1, &wType))
			{
				return 0 != (wType & (C3_NONSPACING | C3_DIACRITIC | C3_VOWELMARK));
			}
		}

		return false;
	}

	bool IsRegionalIndicator (DWORD dwCodePoint)
	{
		return 0x1F1E6 <= dwCodePoint && dwCodePoint <= 0x1F1FF;
	}

	INT GetHangulType (DWORD dwCodePoint)
	{
		if((0x1100 <= dwCodePoint && dwCodePoint <= 0x115F) ||
			(0xA960 <= dwCodePoint && dwCodePoint <= 0xA97C))
			return 1;	// L
		if((0x1160 <= dwCodePoint && dwCodePoint <= 0x11A7) ||
			(0xD7B0 <= dwCodePoint && dwCodePoint <= 0xD7C6))
			return 2;	// V
		if((0x11A8 <= dwCodePoint && dwCodePoint <= 0x11FF) ||
			(0xD7CB <= dwCodePoint && dwCodePoint <= 0xD7FB))
			return 3;	// T
		if(0xAC00 <= dwCodePoint && dwCodePoint <= 0xD7A3)
			return 0 == (dwCodePoint - 0xAC00) % 28 ? 4 : 5;	// LV or LVT
		return 0;
	}

	bool IsUnicodePrepend (DWORD dwCodePoint)
	{
		return (0x0600 <= dwCodePoint && dwCodePoint <= 0x0605) ||
			0x06DD == dwCodePoint || 0x070F == dwCodePoint ||
			(0x0890 <= dwCodePoint && dwCodePoint <= 0x0891) ||
			0x08E2 == dwCodePoint || 0x0D4E == dwCodePoint ||
			0x110BD == dwCodePoint || 0x110CD == dwCodePoint ||
			(0x111C2 <= dwCodePoint && dwCodePoint <= 0x111C3) ||
			0x1193F == dwCodePoint || 0x11941 == dwCodePoint ||
			0x11A3A == dwCodePoint ||
			(0x11A84 <= dwCodePoint && dwCodePoint <= 0x11A89) ||
			0x11D46 == dwCodePoint;
	}

	bool IsVirama (DWORD dwCodePoint)
	{
		switch(dwCodePoint)
		{
		case 0x094D: case 0x09CD: case 0x0A4D: case 0x0ACD:
		case 0x0B4D: case 0x0BCD: case 0x0C4D: case 0x0CCD:
		case 0x0D4D: case 0x0DCA: case 0x0E3A: case 0x0F84:
		case 0x1039: case 0x103A: case 0x1714: case 0x1734:
		case 0x17D2: case 0x1A60: case 0x1B44: case 0x1BAA:
		case 0x1BAB: case 0xA806: case 0xA8C4: case 0xA953:
		case 0xA9C0: case 0xAAF6: case 0xABED: case 0x11046:
		case 0x1107F: case 0x110B9: case 0x11133: case 0x111C0:
		case 0x11235: case 0x112EA: case 0x1134D: case 0x11442:
		case 0x114C2: case 0x115BF: case 0x1163F: case 0x116B6:
		case 0x1172B: case 0x11839: case 0x1193D: case 0x1193E:
		case 0x119E0: case 0x11A34: case 0x11A47: case 0x11A99:
		case 0x11C3F: case 0x11D44: case 0x11D45: case 0x11D97:
			return true;
		}

		return false;
	}

	bool IsJoinedUnicodePair (DWORD dwLeft, DWORD dwRight)
	{
		INT nLeftHangul, nRightHangul;

		if((0x000D == dwLeft && 0x000A == dwRight) ||
			IsUnicodeExtend(dwRight) || 0x200D == dwRight ||
			0x200D == dwLeft || IsVirama(dwLeft) || IsUnicodePrepend(dwLeft))
		{
			return true;
		}

		nLeftHangul = GetHangulType(dwLeft);
		nRightHangul = GetHangulType(dwRight);
		return (1 == nLeftHangul && (1 == nRightHangul || 2 == nRightHangul || 4 == nRightHangul || 5 == nRightHangul)) ||
			((4 == nLeftHangul || 2 == nLeftHangul) && (2 == nRightHangul || 3 == nRightHangul)) ||
			((5 == nLeftHangul || 3 == nLeftHangul) && 3 == nRightHangul);
	}

	INT FindUnicodeBreak (PCWSTR pcwzText, INT cchText, INT idxBreak)
	{
		INT idxSafe = idxBreak;
		DWORD dwRight;

		if(idxSafe <= 0 || cchText <= idxSafe)
			return idxSafe;

		// Never place the break between the halves of a UTF-16 surrogate pair.
		if(IsLowSurrogate(pcwzText[idxSafe]) && IsHighSurrogate(pcwzText[idxSafe - 1]))
			idxSafe--;

		dwRight = ReadCodePoint(pcwzText, cchText, idxSafe);

		while(0 < idxSafe)
		{
			INT idxPrevious;
			DWORD dwLeft = ReadPreviousCodePoint(pcwzText, idxSafe, &idxPrevious);

			if(!IsJoinedUnicodePair(dwLeft, dwRight))
				break;

			idxSafe = idxPrevious;
			dwRight = dwLeft;
		}

		// Regional indicators form pairs.  Check this after consuming any
		// trailing combining characters so those remain attached to the flag.
		if(IsRegionalIndicator(dwRight))
		{
			INT idxScan = idxSafe, idxPairStart = idxSafe, cIndicators = 0, idxPrevious;

			while(0 < idxScan && IsRegionalIndicator(ReadPreviousCodePoint(pcwzText, idxScan, &idxPrevious)))
			{
				cIndicators++;
				if(1 == cIndicators)
					idxPairStart = idxPrevious;
				idxScan = idxPrevious;
			}

			if(0 != cIndicators % 2)
				idxSafe = idxPairStart;
		}

		return idxSafe;
	}
#endif	// UNICODE_SPLITS
}
