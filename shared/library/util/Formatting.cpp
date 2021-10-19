#include <windows.h>
#ifdef	FORMAT_MONEY
	#include "..\Money.h"
#endif
#ifdef	FORMAT_RSTRING
	#include "RString.h"
#endif
#include "Formatting.h"
#ifdef	USE_SIMBEY_CORE_API_REDIRECT
	#include "Published\SimbeyCore.h"
#endif

namespace Formatting
{
	const CHAR c_szHexUpper[] = "0123456789ABCDEF";
	const CHAR c_szHexLower[] = "0123456789abcdef";

	const WCHAR c_wzHexUpper[] = L"0123456789ABCDEF";
	const WCHAR c_wzHexLower[] = L"0123456789abcdef";

	const CHAR c_szBase64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	const CHAR c_szBase64Url[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

	BOOL IsSpace (INT ch)
	{
		BOOL bSpace = FALSE;
		switch(ch)
		{
		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
		case 32:
			bSpace = TRUE;
		}
		return bSpace;
	}

	template <>
	HRESULT TPrintVFEx<CHAR> (UINT nCodePage, __out_ecount(cchMaxOutput) PSTR ptzOutput, INT cchMaxOutput, INT* pcchOutput, PCSTR pctzFormat, va_list vArgs)
	{
#ifndef	USE_SIMBEY_CORE_API_REDIRECT
		HRESULT hr;
		UINT cchStartLength = cchMaxOutput;
		CHAR ch;

		CheckIf(NULL == pctzFormat, E_INVALIDARG);

		Assert(ptzOutput || 0 == cchMaxOutput);

		while(0 < cchMaxOutput)
		{
			ch = *pctzFormat;
			if(ch == '%')
			{
				pctzFormat++;
				ch = *pctzFormat;
				if(ch == '%')
				{
					*ptzOutput = '%';
					ptzOutput++;
					pctzFormat++;
					cchMaxOutput--;
				}
				else
				{
					CHAR vChar;
					LONG vLong;
					ULONG vULong;
					DOUBLE vDouble;
					union
					{
						PCSTR vString;
						PCWSTR vWide;
					};
					INT cchCopy = -1;

					if(ch == '.')
					{
						pctzFormat++;
						cchCopy = TAscToInt32(pctzFormat);
						while(*pctzFormat >= '0' && *pctzFormat <= '9')
							pctzFormat++;
					}

					ch = *pctzFormat;
					pctzFormat++;

					switch(ch)
					{
					case 'c':
						vChar = va_arg(vArgs, CHAR);
						*ptzOutput = vChar;
						ptzOutput++;
						cchMaxOutput--;
						break;
					case 'd':
					case 'i':
#ifndef	_WIN64
	Int32_Copy:
#endif
						if(cchCopy != 0)
						{
							vLong = va_arg(vArgs,LONG);
							if(vLong < 0)
							{
								*ptzOutput = '-';
								ptzOutput++;
								cchMaxOutput--;
								vLong *= -1;
							}
							vULong = (ULONG)vLong;
							Check(TCopyULong(ptzOutput, cchMaxOutput, vULong, cchCopy, 10, &cchCopy));
							cchMaxOutput -= cchCopy;
							ptzOutput += cchCopy;
						}
						break;
					case 'f':
						if(cchCopy == -1)
							cchCopy = 8;
						vDouble = (DOUBLE)va_arg(vArgs,DOUBLE);
						Check(TDoubleToString(vDouble, ptzOutput, cchMaxOutput, cchCopy, &cchCopy));
						cchMaxOutput -= cchCopy;
						ptzOutput += cchCopy;
						break;
					case 's':
	Ansi_String_Copy:
						if(cchCopy != 0)
						{
							vString = va_arg(vArgs, PCSTR);
#ifdef	FORMAT_RSTRING
	Ansi_String_Copy_Inner:
#endif
							if(NULL == vString)
								vString = c_szNullFmt;
							if(-1 == cchCopy)
								Check(TStrCchCpyEx(ptzOutput, cchMaxOutput, vString, &ptzOutput, &cchMaxOutput));
							else
								Check(TStrCchCpyNEx(ptzOutput, cchMaxOutput, vString, cchCopy, &ptzOutput, &cchMaxOutput));
						}
						break;
					case 'S':
	Unicode_String_Copy:
						if(cchCopy != 0)
						{
							vWide = va_arg(vArgs, PCWSTR);
#ifdef	FORMAT_RSTRING
	Unicode_String_Copy_Inner:
#endif
							if(NULL == vWide)
								vWide = c_wzNullFmt;
							if(-1 == cchCopy)
								cchCopy = TStrLenAssert(vWide);
							if(0 < cchCopy)
							{
								cchCopy = WideCharToMultiByte(nCodePage, 0, vWide, cchCopy, ptzOutput, cchMaxOutput, NULL, NULL);
								CheckIfGetLastError(0 == cchCopy);
								ptzOutput += cchCopy;
								cchMaxOutput -= cchCopy;
							}
						}
						break;
					case 'u':
#ifndef	_WIN64
	UInt32_Copy:
#endif
						if(0 != cchCopy)
						{
							vULong = va_arg(vArgs,ULONG);
							Check(TCopyULong(ptzOutput, cchMaxOutput, vULong, cchCopy, 10, &cchCopy));
							cchMaxOutput -= cchCopy;
							ptzOutput += cchCopy;
						}
						break;
					case 'x':
					case 'X':
						if(0 != cchCopy)
						{
							vULong = va_arg(vArgs,ULONG);
							Check(TCopyULong(ptzOutput, cchMaxOutput, vULong, cchCopy, 16, &cchCopy));
							cchMaxOutput -= cchCopy;
							if(ch == 'X')
							{
								ptzOutput[cchCopy] = '\0';
								TStrUpr(ptzOutput);
							}
							ptzOutput += cchCopy;
						}
						break;
					case 'o':
					case 'O':
						if(0 != cchCopy)
						{
							vULong = va_arg(vArgs,ULONG);
							Check(TCopyULong(ptzOutput, cchMaxOutput, vULong, cchCopy, 8, &cchCopy));
							cchMaxOutput -= cchCopy;
							ptzOutput += cchCopy;
						}
						break;
					case 'p':
					case 'P':
						CheckIf(cchMaxOutput <= 9, STRSAFE_E_INSUFFICIENT_BUFFER);

						vULong = va_arg(vArgs,ULONG);
						Check(TCopyULong(ptzOutput, cchMaxOutput, HIWORD(vULong), 4, 16, &cchCopy));
						ptzOutput[4] = ':';
						Check(TCopyULong(ptzOutput + 5, cchMaxOutput - 5, LOWORD(vULong), 4, 16, &cchCopy));

						if(ch == 'P')
						{
							ptzOutput[9] = '\0';
							TStrUpr(ptzOutput);
						}

						cchMaxOutput -= 9;
						ptzOutput += 9;
						break;
					case 'h':
						ch = *pctzFormat;
						if(ch == 's' || ch == 'S')
						{
							pctzFormat++;
							goto Ansi_String_Copy;
						}
						break;
					case 'l':
						ch = *pctzFormat;
						if(ch == 's' || ch == 'S')
						{
							pctzFormat++;
							goto Unicode_String_Copy;
						}
						break;
					case 'e':
					case 'E':
					case 'g':
					case 'G':
						vDouble = (DOUBLE)va_arg(vArgs,DOUBLE);
						Check(TCopyDouble(ptzOutput, cchMaxOutput, vDouble, cchCopy, ch, &cchCopy));
						cchMaxOutput -= cchCopy;
						ptzOutput += cchCopy;
						break;
					case 'z':
					case 'Z':
						if(0 != cchCopy)
						{
							PCSTR pcszHex = (ch == 'z') ? c_szHexLower : c_szHexUpper;
							LPBYTE vBytes = va_arg(vArgs,LPBYTE);
							LONG cSize = va_arg(vArgs,LONG);
							INT i;

							if(cchCopy == -1 || cchCopy > cSize)
								cchCopy = cSize;

							CheckIf(cchMaxOutput <= cchCopy << 1, STRSAFE_E_INSUFFICIENT_BUFFER);

							for(i = 0; i < cchCopy; i++)
							{
								ptzOutput[0] = pcszHex[vBytes[i] >> 4];
								ptzOutput[1] = pcszHex[vBytes[i] & 0x0F];
								ptzOutput += 2;
							}
							cchMaxOutput -= cchCopy << 1;
						}
						break;
					case 'q':
#ifdef	_WIN64
	UInt64_Copy:
#endif
						{
							ULONGLONG vULongLong = va_arg(vArgs, ULONGLONG);
							Check(TCopyULongLong(ptzOutput, cchMaxOutput, vULongLong, cchCopy, 10, &cchCopy));
							cchMaxOutput -= cchCopy;
							ptzOutput += cchCopy;
						}
						break;
#ifdef	FORMAT_MONEY
					case 'm':
						if(-1 == cchCopy)
						{
							CMoney* pMoney = va_arg(vArgs, CMoney*);
							Check(pMoney->GetDollarAmount(ptzOutput, cchMaxOutput, &cchCopy));
							ptzOutput += cchCopy;
							cchMaxOutput -= cchCopy;
						}
						break;
#endif
					case 'n':
#ifdef	_WIN64
	Int64_Copy:
#endif
						if(0 != cchCopy)
						{
							LONGLONG vLongLong = va_arg(vArgs, LONGLONG);
							Check(TCopyLongLong(ptzOutput, cchMaxOutput, vLongLong, cchCopy, 10, &cchCopy));
							cchMaxOutput -= cchCopy;
							ptzOutput += cchCopy;
						}
						break;
#ifdef	FORMAT_RSTRING
					case 'r':
						{
							RSTRING rstrValue = va_arg(vArgs, RSTRING);
							if(-1 == cchCopy)
								cchCopy = RStrLenInl(rstrValue);
							if(RStrIsWide(rstrValue))
							{
								vWide = RStrToWide(rstrValue);
								goto Unicode_String_Copy_Inner;
							}
							else
							{
								vString = RStrToAnsi(rstrValue);
								goto Ansi_String_Copy_Inner;
							}
						}
						break;
#endif
					case 'I':
						ch = *pctzFormat;
						if(ch == 'd')
						{
							pctzFormat++;
#ifdef	_WIN64
							goto Int64_Copy;
#else
							goto Int32_Copy;
#endif
						}
						else if(ch == 'u')
						{
							pctzFormat++;
#ifdef	_WIN64
							goto UInt64_Copy;
#else
							goto UInt32_Copy;
#endif
						}
						break;
					}
				}
			}
			else
			{
				if(ch == 0)
					break;

				*ptzOutput = ch;
				ptzOutput++;
				pctzFormat++;
				cchMaxOutput--;
			}
		}

		Check(TStrTerminateAssert(ptzOutput, cchMaxOutput));

		if(pcchOutput)
			*pcchOutput = cchStartLength - cchMaxOutput;

	Cleanup:
		return hr;
#else
		return ScPrintVFA(ptzOutput, cchMaxOutput, pcchOutput, pctzFormat, vArgs);
#endif
	}

	template <>
	HRESULT TPrintVFEx<WCHAR> (UINT nCodePage, __out_ecount(cchMaxOutput) PWSTR ptzOutput, INT cchMaxOutput, INT* pcchOutput, PCWSTR pctzFormat, va_list vArgs)
	{
#ifndef	USE_SIMBEY_CORE_API_REDIRECT
		HRESULT hr;
		UINT cchStartLength = cchMaxOutput;
		WCHAR ch;

		CheckIf(NULL == pctzFormat, E_INVALIDARG);

		Assert(ptzOutput || 0 == cchMaxOutput);

		while(0 < cchMaxOutput)
		{
			ch = *pctzFormat;
			if(ch == L'%')
			{
				pctzFormat++;
				ch = *pctzFormat;
				if(ch == L'%')
				{
					*ptzOutput = L'%';
					ptzOutput++;
					pctzFormat++;
					cchMaxOutput--;
				}
				else
				{
					WCHAR vChar;
					LONG vLong;
					ULONG vULong;
					DOUBLE vDouble;
					union
					{
						PCWSTR vString;
						PCSTR vAnsi;
					};
					INT cchCopy = -1;

					if(ch == L'.')
					{
						pctzFormat++;
						cchCopy = TAscToInt32(pctzFormat);
						while(*pctzFormat >= L'0' && *pctzFormat <= L'9')
							pctzFormat++;
					}

					ch = *pctzFormat;
					pctzFormat++;

					switch(ch)
					{
					case L'c':
						vChar = va_arg(vArgs, WCHAR);
						*ptzOutput = vChar;
						ptzOutput++;
						cchMaxOutput--;
						break;
					case L'd':
					case L'i':
#ifndef	_WIN64
	Int32_Copy:
#endif
						if(cchCopy != 0)
						{
							vLong = va_arg(vArgs,LONG);
							if(vLong < 0)
							{
								*ptzOutput = L'-';
								ptzOutput++;
								cchMaxOutput--;
								vLong *= -1;
							}
							vULong = (ULONG)vLong;
							Check(TCopyULong(ptzOutput, cchMaxOutput, vULong, cchCopy, 10, &cchCopy));
							cchMaxOutput -= cchCopy;
							ptzOutput += cchCopy;
						}
						break;
					case L'f':
						if(cchCopy == -1)
							cchCopy = 8;
						vDouble = (DOUBLE)va_arg(vArgs,DOUBLE);
						Check(TDoubleToString(vDouble, ptzOutput, cchMaxOutput, cchCopy, &cchCopy));
						cchMaxOutput -= cchCopy;
						ptzOutput += cchCopy;
						break;
					case L's':
	Unicode_String_Copy:
						if(cchCopy != 0)
						{
							vString = va_arg(vArgs, PCWSTR);
#ifdef	FORMAT_RSTRING
	Unicode_String_Copy_Inner:
#endif
							if(NULL == vString)
								vString = c_wzNullFmt;
							if(-1 == cchCopy)
								Check(TStrCchCpyEx(ptzOutput, cchMaxOutput, vString, &ptzOutput, &cchMaxOutput));
							else
								Check(TStrCchCpyNEx(ptzOutput, cchMaxOutput, vString, cchCopy, &ptzOutput, &cchMaxOutput));
						}
						break;
					case L'S':
	Ansi_String_Copy:
						if(cchCopy != 0)
						{
							vAnsi = va_arg(vArgs, PCSTR);
#ifdef	FORMAT_RSTRING
	Ansi_String_Copy_Inner:
#endif
							if(NULL == vAnsi)
								vAnsi = c_szNullFmt;
							if(-1 == cchCopy)
								cchCopy = TStrLenAssert(vAnsi);
							if(0 < cchCopy)
							{
								cchCopy = MultiByteToWideChar(nCodePage, 0, vAnsi, cchCopy, ptzOutput, cchMaxOutput);
								CheckIfGetLastError(0 == cchCopy);
								ptzOutput += cchCopy;
								cchMaxOutput -= cchCopy;
							}
						}
						break;
					case L'u':
#ifndef	_WIN64
	UInt32_Copy:
#endif
						if(0 != cchCopy)
						{
							vULong = va_arg(vArgs,ULONG);
							Check(TCopyULong(ptzOutput, cchMaxOutput, vULong, cchCopy, 10, &cchCopy));
							cchMaxOutput -= cchCopy;
							ptzOutput += cchCopy;
						}
						break;
					case L'x':
					case L'X':
						if(0 != cchCopy)
						{
							vULong = va_arg(vArgs,ULONG);
							Check(TCopyULong(ptzOutput, cchMaxOutput, vULong, cchCopy, 16, &cchCopy));
							cchMaxOutput -= cchCopy;
							if(ch == L'X')
							{
								ptzOutput[cchCopy] = L'\0';
								TStrUpr(ptzOutput);
							}
							ptzOutput += cchCopy;
						}
						break;
					case L'o':
					case L'O':
						if(0 != cchCopy)
						{
							vULong = va_arg(vArgs,ULONG);
							Check(TCopyULong(ptzOutput, cchMaxOutput, vULong, cchCopy, 8, &cchCopy));
							cchMaxOutput -= cchCopy;
							ptzOutput += cchCopy;
						}
						break;
					case L'p':
					case L'P':
						CheckIf(cchMaxOutput <= 9, STRSAFE_E_INSUFFICIENT_BUFFER);

						vULong = va_arg(vArgs,ULONG);
						Check(TCopyULong(ptzOutput, cchMaxOutput, HIWORD(vULong), 4, 16, &cchCopy));
						ptzOutput[4] = L':';
						Check(TCopyULong(ptzOutput + 5, cchMaxOutput - 5, LOWORD(vULong), 4, 16, &cchCopy));

						if(ch == L'P')
						{
							ptzOutput[9] = L'\0';
							TStrUpr(ptzOutput);
						}

						cchMaxOutput -= 9;
						ptzOutput += 9;
						break;
					case L'h':
						ch = *pctzFormat;
						if(ch == L's' || ch == L'S')
						{
							pctzFormat++;
							goto Ansi_String_Copy;
						}
						break;
					case L'l':
						ch = *pctzFormat;
						if(ch == L's' || ch == L'S')
						{
							pctzFormat++;
							goto Unicode_String_Copy;
						}
						break;
					case L'e':
					case L'E':
					case L'g':
					case L'G':
						vDouble = (DOUBLE)va_arg(vArgs,DOUBLE);
						Check(TCopyDouble(ptzOutput, cchMaxOutput, vDouble, cchCopy, ch, &cchCopy));
						cchMaxOutput -= cchCopy;
						ptzOutput += cchCopy;
						break;
					case L'z':
					case L'Z':
						if(0 != cchCopy)
						{
							PCWSTR pcwzHex = (ch == L'z') ? c_wzHexLower : c_wzHexUpper;
							LPBYTE vBytes = va_arg(vArgs,LPBYTE);
							LONG cSize = va_arg(vArgs,LONG);
							INT i;

							if(cchCopy == -1 || cchCopy > cSize)
								cchCopy = cSize;

							CheckIf(cchMaxOutput <= cchCopy << 1, STRSAFE_E_INSUFFICIENT_BUFFER);

							for(i = 0; i < cchCopy; i++)
							{
								ptzOutput[0] = pcwzHex[vBytes[i] >> 4];
								ptzOutput[1] = pcwzHex[vBytes[i] & 0x0F];
								ptzOutput += 2;
							}
							cchMaxOutput -= cchCopy << 1;
						}
						break;
					case L'q':
						{
#ifdef	_WIN64
	UInt64_Copy:
#endif
							ULONGLONG vULongLong = va_arg(vArgs, ULONGLONG);
							Check(TCopyULongLong(ptzOutput, cchMaxOutput, vULongLong, cchCopy, 10, &cchCopy));
							cchMaxOutput -= cchCopy;
							ptzOutput += cchCopy;
						}
						break;
#ifdef	FORMAT_MONEY
					case L'm':
						if(-1 == cchCopy)
						{
							CMoney* pMoney = va_arg(vArgs, CMoney*);
							Check(pMoney->GetDollarAmountW(ptzOutput, cchMaxOutput, &cchCopy));
							ptzOutput += cchCopy;
							cchMaxOutput -= cchCopy;
						}
						break;
#endif
					case 'n':
#ifdef	_WIN64
	Int64_Copy:
#endif
						if(0 != cchCopy)
						{
							LONGLONG vLongLong = va_arg(vArgs, LONGLONG);
							Check(TCopyLongLong(ptzOutput, cchMaxOutput, vLongLong, cchCopy, 10, &cchCopy));
							cchMaxOutput -= cchCopy;
							ptzOutput += cchCopy;
						}
						break;
#ifdef	FORMAT_RSTRING
					case L'r':
						{
							RSTRING rstrValue = va_arg(vArgs, RSTRING);
							if(-1 == cchCopy)
								cchCopy = RStrLenInl(rstrValue);
							if(RStrIsWide(rstrValue))
							{
								vString = RStrToWide(rstrValue);
								goto Unicode_String_Copy_Inner;
							}
							else
							{
								vAnsi = RStrToAnsi(rstrValue);
								goto Ansi_String_Copy_Inner;
							}
						}
						break;
#endif
					case L'I':
						ch = *pctzFormat;
						if(ch == L'd')
						{
							pctzFormat++;
#ifdef	_WIN64
							goto Int64_Copy;
#else
							goto Int32_Copy;
#endif
						}
						else if(ch == L'u')
						{
							pctzFormat++;
#ifdef	_WIN64
							goto UInt64_Copy;
#else
							goto UInt32_Copy;
#endif
						}
						break;
					}
				}
			}
			else
			{
				if(ch == 0)
					break;

				*ptzOutput = ch;
				ptzOutput++;
				pctzFormat++;
				cchMaxOutput--;
			}
		}

		Check(TStrTerminateAssert(ptzOutput, cchMaxOutput));

		if(pcchOutput)
			*pcchOutput = cchStartLength - cchMaxOutput;

	Cleanup:
		return hr;
#else
		return ScPrintVFW(ptzOutput, cchMaxOutput, pcchOutput, pctzFormat, vArgs);
#endif
	}

	template <>
	HRESULT TPrintVF<CHAR> (__out_ecount(cchMaxOutput) PSTR ptzOutput, INT cchMaxOutput, INT* pcchOutput, PCSTR pctzFormat, va_list vArgs)
	{
#ifndef	USE_SIMBEY_CORE_API_REDIRECT
		return TPrintVFEx(CP_UTF8, ptzOutput, cchMaxOutput, pcchOutput, pctzFormat, vArgs);
#else
		return ScPrintVFA(ptzOutput, cchMaxOutput, pcchOutput, pctzFormat, vArgs);
#endif
	}

	template <>
	HRESULT TPrintVF<WCHAR> (__out_ecount(cchMaxOutput) PWSTR ptzOutput, INT cchMaxOutput, INT* pcchOutput, PCWSTR pctzFormat, va_list vArgs)
	{
#ifndef	USE_SIMBEY_CORE_API_REDIRECT
		return TPrintVFEx(CP_UTF8, ptzOutput, cchMaxOutput, pcchOutput, pctzFormat, vArgs);
#else
		return ScPrintVFW(ptzOutput, cchMaxOutput, pcchOutput, pctzFormat, vArgs);
#endif
	}
}
