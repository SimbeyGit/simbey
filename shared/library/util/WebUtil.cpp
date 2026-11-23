#include <windows.h>
#include "..\Core\CoreDefs.h"
#include "Formatting.h"
#include "CharSet.h"
#include "WebUtil.h"

namespace WebUtil
{
	HRESULT FindHeader (PCSTR pcszHeaders, INT cchHeaders, PCSTR pcszName, INT cchName, __out INT* pnStart, __out INT* pnEnd)
	{
		for(INT i = 0; i < cchHeaders; i++)
		{
			if(TCompareLeftIAssert(pcszHeaders + i, pcszName))
			{
				PCSTR pcszStart;

				i += cchName;
				while(i < cchHeaders && Formatting::IsSpace(pcszHeaders[i]))
					i++;

				pcszStart = pcszHeaders + i;
				*pnStart = i;
				cchHeaders -= *pnStart;

				for(INT n = 0; n < cchHeaders; n++)
				{
					if('\r' == pcszStart[n] && '\n' == pcszStart[n + 1])
					{
						*pnEnd = *pnStart + n;
						return S_OK;
					}
				}

				return E_UNEXPECTED;
			}
		}

		return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	}

	HRESULT ReadSubPartA (PCSTR pcszHeader, INT cchHeader, PCSTR pcszName, INT cchName, __out PCSTR* ppcszSubPart, __out INT* pcchSubPart)
	{
		for(INT i = 0; i < cchHeader; i++)
		{
			if(TCompareLeftIAssert(pcszHeader + i, pcszName))
			{
				i += cchName;

				if('"' == pcszHeader[i])
				{
					i++;
					for(INT n = i; n < cchHeader; n++)
					{
						if('"' == pcszHeader[n])
						{
							*ppcszSubPart = pcszHeader + i;
							*pcchSubPart = n - i;
							return S_OK;
						}
					}
				}
				else
				{
					for(INT n = i; n <= cchHeader; n++)
					{
						if(n == cchHeader || ';' == pcszHeader[n])
						{
							*ppcszSubPart = pcszHeader + i;
							*pcchSubPart = n - i;
							return S_OK;
						}
					}
				}

				return E_UNEXPECTED;
			}
		}
		return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	}

	HRESULT ReadSubPartW (PCSTR pcszHeader, INT cchHeader, PCSTR pcszName, INT cchName, __out RSTRING* prstrSubPartW)
	{
		HRESULT hr;
		PCSTR pcszSubPart;
		INT cchSubPart;

		CheckNoTrace(ReadSubPartA(pcszHeader, cchHeader, pcszName, cchName, &pcszSubPart, &cchSubPart));

		// Convert the value using ISO-8859-1, per the web standards.
		Check(RStrConvertToW(CP_ISO_8859_1, cchSubPart, pcszSubPart, prstrSubPartW));

	Cleanup:
		return hr;
	}

	INT UrlUnescapePart (PSTR pszPart, INT cchPart)
	{
		INT iWrite = 0;
		for(INT iRead = 0; iRead < cchPart; iRead++)
		{
			CHAR ch = pszPart[iRead];
			if(ch == '%')
			{
				CHAR chA, chB;
				if(++iRead == cchPart)
					break;
				chA = pszPart[iRead];
				if(++iRead == cchPart)
					break;
				chB = pszPart[iRead];
				ch = (CHAR)((Formatting::THexValue(static_cast<BYTE>(chA)) << 4) | Formatting::THexValue(static_cast<BYTE>(chB)));
				pszPart[iWrite++] = ch;
			}
			else if(ch == '+')
				pszPart[iWrite++] = ' ';
			else
				pszPart[iWrite++] = ch;
		}
		return iWrite;
	}

	HRESULT UrlEscapePart (PCWSTR pcwzPart, INT cchPart, __out CMemoryStream& stmEscapedW)
	{
		HRESULT hr;
		RSTRING rstrUTF8 = NULL;
		CMemoryStream stmEncodedA;
		INT cchData;

		// Convert the string to UTF-8, then encode the UTF-8 string.
		Check(RStrConvertToA(cchPart, pcwzPart, &rstrUTF8));
		Check(URLEncodeA(RStrToAnsi(rstrUTF8), RStrLen(rstrUTF8), &stmEncodedA));

		cchData = stmEncodedA.DataRemaining();
		if(0 < cchData)
		{
			PCSTR pcszRead = stmEncodedA.TGetReadPtr<CHAR>();
			PWSTR pwzWrite;

			// There are no extended characters in the data, so the conversion is 1-to-1.
			Check(stmEscapedW.TWriteAdvance(&pwzWrite, cchData));

			// Copy the encoded text directly to Unicode.  No conversion necessary.
			for(INT i = 0; i < cchData; i++)
				pwzWrite[i] = pcszRead[i];
		}

	Cleanup:
		RStrRelease(rstrUTF8);
		return hr;
	}

	HRESULT URLEncode (RSTRING rstrText, __out ISequentialStream* pstmEncoded)
	{
		HRESULT hr;
		RSTRING rstrUTF8 = NULL;

		Check(RStrConvertToA(RStrLen(rstrText), RStrToWide(rstrText), &rstrUTF8));
		Check(URLEncodeA(RStrToAnsi(rstrUTF8), RStrLen(rstrUTF8), pstmEncoded));

	Cleanup:
		RStrRelease(rstrUTF8);
		return hr;
	}

	HRESULT URLDecode (RSTRING rstrText, __out ISequentialStream* pstmDecoded)
	{
		HRESULT hr;
		RSTRING rstrUTF8 = NULL;

		Check(RStrConvertToA(RStrLen(rstrText), RStrToWide(rstrText), &rstrUTF8));
		Check(URLDecodeA(RStrToAnsi(rstrUTF8), RStrLen(rstrUTF8), pstmDecoded));

	Cleanup:
		RStrRelease(rstrUTF8);
		return hr;
	}

	HRESULT URLEncodeA (PCSTR pcszText, INT cchText, __out ISequentialStream* pstmEncoded)
	{
		HRESULT hr = S_FALSE;
		ULONG cb;
		CHAR szEncoded[3]; szEncoded[0] = L'%';
		INT idxWriteFrom = -1;

		for(INT i = 0; i < cchText; i++)
		{
			union
			{
				CHAR ch;
				BYTE b;
			};

			ch = pcszText[i];
			if(('a' <= ch && ch <= 'z') ||
				('A' <= ch && ch <= 'Z') ||
				('0' <= ch && ch <= '9') ||
				'-' == ch || '_' == ch || '.' == ch || '~' == ch)
			{
				if(-1 == idxWriteFrom)
					idxWriteFrom = i;
			}
			else
			{
				if(-1 != idxWriteFrom)
				{
					Check(pstmEncoded->Write(pcszText + idxWriteFrom, i - idxWriteFrom, &cb));
					idxWriteFrom = -1;
				}

				if(L' ' == ch)
					Check(pstmEncoded->Write(SLP("+"), &cb));
				else
				{
					szEncoded[1] = Formatting::c_szHexUpper[b / 16];
					szEncoded[2] = Formatting::c_szHexUpper[b % 16];
					Check(pstmEncoded->Write(szEncoded, sizeof(szEncoded), &cb));
				}
			}
		}

		if(-1 != idxWriteFrom)
			Check(pstmEncoded->Write(pcszText + idxWriteFrom, cchText - idxWriteFrom, &cb));

	Cleanup:
		return hr;
	}

	HRESULT URLDecodeA (PCSTR pcszText, INT cchText, __out ISequentialStream* pstmDecoded)
	{
		HRESULT hr = S_FALSE;

		for(INT i = 0; i < cchText; i++)
		{
			BYTE b = pcszText[i];
			ULONG cb;

			if('%' == b)
			{
				CheckIf(i + 2 >= cchText, E_INVALIDARG);
				b = Formatting::THexValue(static_cast<BYTE>(pcszText[i + 1])) << 4 | Formatting::THexValue(static_cast<BYTE>(pcszText[i + 2]));
				i += 2;
			}
			else if('+' == b)
				b = ' ';

			Check(pstmDecoded->Write(&b, sizeof(b), &cb));
		}

	Cleanup:
		return hr;
	}

	HRESULT ResolveInlineLinks (PCSTR pcszText, INT cchText, PCSTR pcszLinkPrefix, INT cchLinkPrefix, __out CMemoryStream& stmResolved)
	{
		HRESULT hr;
		INT nScan = cchText - 9, nStart = 0;
		DWORD cbWritten;
		BOOL fQuoted = FALSE;

		for(INT i = 0; i < nScan; i++)
		{
			if('"' == pcszText[i])
				fQuoted = !fQuoted;
			else if(!fQuoted)
			{
				INT cchAttrib;
				CHAR chTerm;

				if(IsUrlAttribute(pcszText + i, cchText - i, &cchAttrib, &chTerm))
				{
					PCSTR pcszEnd;

					if('"' == chTerm)
					{
						i += cchAttrib + 1;
						pcszEnd = TStrCchChr(pcszText + i, cchText - i, '"');
						fQuoted = TRUE;
					}
					else
					{
						i += cchAttrib;
						pcszEnd = NULL;
						for(INT n = i; n < cchText; n++)
						{
							if(chTerm == pcszText[n] || Formatting::IsSpace(pcszText[n]))
							{
								pcszEnd = pcszText + n;
								break;
							}
						}
					}

					CheckIf(NULL == pcszEnd, E_FAIL);

					if(TCompareLeftIAssert(pcszText + i, "cid:") && '/' != pcszText[i + 4])
					{
						Check(stmResolved.TWrite(pcszText + nStart, i - nStart, &cbWritten));
						i += 4;

						Check(stmResolved.TWrite(pcszLinkPrefix, cchLinkPrefix, &cbWritten));

						nStart = i;
					}
				}
			}
		}

		CheckIf(0 == nStart, S_FALSE);
		Check(stmResolved.TWrite(pcszText + nStart, cchText - nStart, &cbWritten));

	Cleanup:
		return hr;
	}
}
