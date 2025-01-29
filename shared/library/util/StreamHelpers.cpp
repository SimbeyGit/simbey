#include <malloc.h>
#include <windows.h>
#include "..\Core\CoreDefs.h"
#include "..\Core\MemoryStream.h"
#ifdef	FORMAT_MONEY
	#include "..\Money.h"
#endif
#ifdef	FORMAT_RSTRING
	#include "RString.h"
#endif
#include "Formatting.h"
#include "StreamHelpers.h"
#ifdef	USE_SIMBEY_CORE_API_REDIRECT
	#include "Published\SimbeyCore.h"
#endif

namespace Stream
{
	HRESULT WINAPI CopyAnsiToStreamW (UINT nCodePage, ISequentialStream* pStream, PCSTR pcszString, INT cchString, INT cchCopy)
	{
#ifndef	USE_SIMBEY_CORE_API_REDIRECT
		HRESULT hr;
		INT cchWide;
		PWSTR pwzWide = NULL;
		ULONG cbWrite;

		if(cchCopy == -1)
			cchCopy = cchString;
		else if(cchString < cchCopy)
			cchCopy = cchString;

		cchWide = MultiByteToWideChar(nCodePage, 0, pcszString, cchCopy, NULL, 0);
		pwzWide = reinterpret_cast<PWSTR>(_malloca(cchWide * sizeof(WCHAR)));
		CheckAlloc(pwzWide);

		CheckIfGetLastError(cchWide != MultiByteToWideChar(nCodePage, 0, pcszString, cchCopy, pwzWide, cchWide));
		Check(Stream::TWrite(pStream, pwzWide, cchWide, &cbWrite));

	Cleanup:
		_freea(pwzWide);
		return hr;
#else
		return ScCopyAnsiToStreamW(nCodePage, pStream, pcszString, cchString, cchCopy);
#endif
	}

	HRESULT WINAPI CopyWideToStreamA (UINT nCodePage, ISequentialStream* pStream, PCWSTR pcwzString, INT cchString, INT cchCopy)
	{
#ifndef	USE_SIMBEY_CORE_API_REDIRECT
		HRESULT hr;
		INT cchMultiByte;
		PSTR pszMultiByte = NULL;
		ULONG cbWrite;

		if(cchCopy == -1)
			cchCopy = cchString;
		else if(cchString < cchCopy)
			cchCopy = cchString;

		cchMultiByte = WideCharToMultiByte(nCodePage, 0, pcwzString, cchCopy, NULL, 0, NULL, NULL);
		pszMultiByte = reinterpret_cast<PSTR>(_malloca(cchMultiByte));
		CheckAlloc(pszMultiByte);

		CheckIfGetLastError(cchMultiByte != WideCharToMultiByte(nCodePage, 0, pcwzString, cchCopy, pszMultiByte, cchMultiByte, NULL, NULL));
		Check(Stream::TWrite(pStream, pszMultiByte, cchMultiByte, &cbWrite));

	Cleanup:
		_freea(pszMultiByte);
		return hr;
#else
		return ScCopyWideToStreamA(nCodePage, pStream, pcwzString, cchString, cchCopy);
#endif
	}

#ifndef	USE_SIMBEY_CORE_API_REDIRECT
	template <typename T, const T* pctzNullFmt, INT cchNullFmt>
	HRESULT TCopyNoConversion (ISequentialStream* pStream, const T* pctzString, INT cchString, INT cchCopy)
	{
		ULONG cbTemp;

		if(NULL == pctzString)
		{
			pctzString = pctzNullFmt;
			cchString = cchNullFmt;
		}

		if(cchCopy == -1)
			cchCopy = cchString;
		else if(cchString < cchCopy)
			cchCopy = cchString;
		return Stream::TWrite(pStream, pctzString, cchCopy, &cbTemp);
	}
#endif

	template <>
	HRESULT TPrintVFEx<CHAR> (UINT nCodePage, ISequentialStream* pStream, PCSTR pctzFormat, va_list vArgs)
	{
#ifndef	USE_SIMBEY_CORE_API_REDIRECT
		HRESULT hr;

		if(pStream && pctzFormat)
		{
			CHAR tch;
			ULONG cbTemp;

			hr = S_OK;

			do
			{
				tch = *pctzFormat;
				if(tch == '%')
				{
					pctzFormat++;
					tch = *pctzFormat;
					if(tch == '%')
					{
						hr = pStream->Write(&tch, sizeof(tch), &cbTemp);
						pctzFormat++;
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
						CHAR tzTemp[64];

						if(tch == '.')
						{
							pctzFormat++;
							cchCopy = Formatting::TAscToXUInt32(pctzFormat, 10, &pctzFormat);
						}

						tch = *pctzFormat;
						pctzFormat++;

						switch(tch)
						{
						case 'c':
							vChar = va_arg(vArgs, CHAR);
							hr = pStream->Write(&vChar, sizeof(vChar), &cbTemp);
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
									vChar = '-';
									hr = pStream->Write(&vChar, sizeof(vChar), &cbTemp);
									vLong *= -1;
								}
								if(SUCCEEDED(hr))
								{
									vULong = (ULONG)vLong;
									hr = Formatting::TCopyULong(tzTemp, ARRAYSIZE(tzTemp), vULong, cchCopy, 10, &cchCopy);
									if(SUCCEEDED(hr))
									{
										hr = Stream::TWrite(pStream, tzTemp, cchCopy, &cbTemp);
									}
								}
							}
							break;
						case 'f':
							if(cchCopy == -1)
								cchCopy = 8;
							vDouble = (DOUBLE)va_arg(vArgs,DOUBLE);
							hr = Formatting::TDoubleToString(vDouble, tzTemp, ARRAYSIZE(tzTemp), cchCopy, &cchCopy);
							if(SUCCEEDED(hr))
								hr = Stream::TWrite(pStream, tzTemp, cchCopy, &cbTemp);
							break;
						case 's':
	Ansi_String_Copy:
							if(cchCopy != 0)
							{
								vString = va_arg(vArgs, PSTR);
								hr = TCopyNoConversion<CHAR, SLP(Formatting::c_szNullFmt)>(pStream, vString, TStrLenChecked(vString), cchCopy);
							}
							break;
						case 'S':
	Unicode_String_Copy:
							if(cchCopy != 0)
							{
								vWide = va_arg(vArgs, PCWSTR);
								if(NULL == vWide)
									vWide = Formatting::c_wzNullFmt;
								hr = CopyWideToStreamA(nCodePage, pStream, vWide, TStrLenAssert(vWide), cchCopy);
							}
							break;
						case 'u':
#ifndef	_WIN64
	UInt32_Copy:
#endif
							if(cchCopy != 0)
							{
								vULong = va_arg(vArgs,ULONG);
								hr = Formatting::TCopyULong(tzTemp, ARRAYSIZE(tzTemp), vULong, cchCopy, 10, &cchCopy);
								if(SUCCEEDED(hr))
								{
									hr = Stream::TWrite(pStream, tzTemp, cchCopy, &cbTemp);
								}
							}
							break;
						case 'x':
						case 'X':
							if(cchCopy != 0)
							{
								vULong = va_arg(vArgs,ULONG);
								hr = Formatting::TCopyULong(tzTemp, ARRAYSIZE(tzTemp), vULong, cchCopy, 16, &cchCopy);
								if(SUCCEEDED(hr))
								{
									if(tch == 'X')
									{
										Formatting::TStrUpr(tzTemp);
									}
									hr = Stream::TWrite(pStream, tzTemp, cchCopy, &cbTemp);
								}
							}
							break;
						case 'o':
						case 'O':
							if(cchCopy != 0)
							{
								vULong = va_arg(vArgs,ULONG);
								hr = Formatting::TCopyULong(tzTemp, ARRAYSIZE(tzTemp), vULong, cchCopy, 8, &cchCopy);
								if(SUCCEEDED(hr))
								{
									hr = Stream::TWrite(pStream, tzTemp, cchCopy, &cbTemp);
								}
							}
							break;
						case 'p':
						case 'P':
#if defined(_WIN64)
							{
								ULARGE_INTEGER uliPtr;
								uliPtr.QuadPart = va_arg(vArgs, unsigned __int64);
								hr = Formatting::TCopyULong(tzTemp, ARRAYSIZE(tzTemp), uliPtr.HighPart, 8, 16, &cchCopy);
								if(SUCCEEDED(hr))
								{
									tzTemp[8] = ':';
									hr = Formatting::TCopyULong(tzTemp + 9, ARRAYSIZE(tzTemp) - 9, uliPtr.LowPart, 8, 16, &cchCopy);
								}
							}
#else
							vULong = va_arg(vArgs,ULONG);
							hr = Formatting::TCopyULong(tzTemp, ARRAYSIZE(tzTemp), HIWORD(vULong), 4, 16, &cchCopy);
							if(SUCCEEDED(hr))
							{
								tzTemp[4] = ':';
								hr = Formatting::TCopyULong(tzTemp + 5, ARRAYSIZE(tzTemp) - 5, LOWORD(vULong), 4, 16, &cchCopy);
							}
#endif
							if(SUCCEEDED(hr))
							{
								if(tch == 'P')
								{
									Formatting::TStrUpr(tzTemp);
								}
								hr = Stream::TWrite(pStream, tzTemp, sizeof(size_t) * 2 + 1, &cbTemp);
							}
							break;
						case 'h':
							tch = *pctzFormat;
							if(tch == 's' || tch == 'S')
							{
								pctzFormat++;
								goto Ansi_String_Copy;
							}
							break;
						case 'l':
							tch = *pctzFormat;
							if(tch == 's' || tch == 'S')
							{
								pctzFormat++;
								goto Unicode_String_Copy;
							}
							break;
						case 'e':
						case 'E':
						case 'g':
						case 'G':
							vDouble = (DOUBLE)va_arg(vArgs, DOUBLE);
							hr = Formatting::TCopyDouble(tzTemp, ARRAYSIZE(tzTemp), vDouble, cchCopy, tch, &cchCopy);
							if(SUCCEEDED(hr))
								hr = Stream::TWrite(pStream, tzTemp, cchCopy, &cbTemp);
							break;
						case 'z':
						case 'Z':
							if(cchCopy != 0)
							{
								PCSTR pcszHex = (tch == 'z') ? Formatting::c_szHexLower : Formatting::c_szHexUpper;
								LPBYTE vBytes = va_arg(vArgs,LPBYTE);
								LONG cSize = va_arg(vArgs,LONG);
								INT i;
								PSTR ptzPtr = tzTemp;

								if(cchCopy == -1 || cchCopy > cSize)
									cchCopy = cSize;

								for(i = 0; i < cchCopy; i++)
								{
									ptzPtr[0] = pcszHex[vBytes[i] >> 4];
									ptzPtr[1] = pcszHex[vBytes[i] & 0x0F];
									ptzPtr += 2;

									if((ULONG)(ptzPtr - tzTemp) == ARRAYSIZE(tzTemp))
									{
										hr = Stream::TWrite(pStream, tzTemp, ARRAYSIZE(tzTemp), &cbTemp);
										if(FAILED(hr))
											break;
										ptzPtr = tzTemp;
									}
								}

								if(ptzPtr != tzTemp && SUCCEEDED(hr))
									hr = Stream::TWrite(pStream, tzTemp, (ULONG)(ptzPtr - tzTemp), &cbTemp);
							}
							break;
						case 'q':		// Quad Word
#ifdef	_WIN64
	UInt64_Copy:
#endif
							{
								ULONGLONG vULongLong = va_arg(vArgs, ULONGLONG);
								hr = Formatting::TCopyULongLong(tzTemp, ARRAYSIZE(tzTemp), vULongLong, cchCopy, 10, &cchCopy);
								if(SUCCEEDED(hr))
								{
									hr = Stream::TWrite(pStream, tzTemp, cchCopy, &cbTemp);
								}
							}
							break;
#ifdef	FORMAT_MONEY
						case 'm':
							if(-1 == cchCopy)
							{
								CMoney* pMoney = va_arg(vArgs, CMoney*);
								hr = pMoney->GetDollarAmount(tzTemp, ARRAYSIZE(tzTemp), &cchCopy);
								if(SUCCEEDED(hr))
									hr = Stream::TWrite(pStream, tzTemp, cchCopy, &cbTemp);
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
								hr = Formatting::TCopyLongLong(tzTemp, ARRAYSIZE(tzTemp), vLongLong, cchCopy, 10, &cchCopy);
								if(SUCCEEDED(hr))
									hr = Stream::TWrite(pStream, tzTemp, cchCopy, &cbTemp);
							}
							break;
#ifdef	FORMAT_RSTRING
						case 'r':
							{
								RSTRING rstrValue = va_arg(vArgs, RSTRING);
								if(RStrIsWide(rstrValue))
								{
									vWide = RStrToWide(rstrValue);
									if(NULL == vWide)
										hr = CopyWideToStreamA(nCodePage, pStream, SLP(Formatting::c_wzNullFmt), cchCopy);
									else
										hr = CopyWideToStreamA(nCodePage, pStream, vWide, RStrLenInl(rstrValue), cchCopy);
								}
								else
									hr = TCopyNoConversion<CHAR, SLP(Formatting::c_szNullFmt)>(pStream, RStrToAnsi(rstrValue), RStrLenInl(rstrValue), cchCopy);
							}
							break;
#endif
						case 'I':
							tch = *pctzFormat;
							if(tch == 'd')
							{
								pctzFormat++;
#ifdef	_WIN64
								goto Int64_Copy;
#else
								goto Int32_Copy;
#endif
							}
							else if(tch == 'u')
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
					if(tch == '\0')
						break;
					hr = pStream->Write(&tch, sizeof(tch), &cbTemp);
					pctzFormat++;
				}
			} while(SUCCEEDED(hr));
		}
		else
			hr = E_INVALIDARG;

		return hr;
#else
		return ScStreamVFExA(nCodePage, pStream, pctzFormat, vArgs);
#endif
	}

	template <>
	HRESULT TPrintVFEx<WCHAR> (UINT nCodePage, ISequentialStream* pStream, PCWSTR pctzFormat, va_list vArgs)
	{
#ifndef	USE_SIMBEY_CORE_API_REDIRECT
		HRESULT hr;

		if(pStream && pctzFormat)
		{
			WCHAR tch;
			ULONG cbTemp;

			hr = S_OK;

			do
			{
				tch = *pctzFormat;
				if(tch == L'%')
				{
					pctzFormat++;
					tch = *pctzFormat;
					if(tch == L'%')
					{
						hr = pStream->Write(&tch, sizeof(WCHAR), &cbTemp);
						pctzFormat++;
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
						WCHAR tzTemp[64];

						if(tch == L'.')
						{
							pctzFormat++;
							cchCopy = Formatting::TAscToXUInt32(pctzFormat, 10, &pctzFormat);
						}

						tch = *pctzFormat;
						pctzFormat++;

						switch(tch)
						{
						case L'c':
							vChar = va_arg(vArgs,CHAR);
							hr = pStream->Write(&vChar, sizeof(vChar), &cbTemp);
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
									vChar = L'-';
									hr = pStream->Write(&vChar, sizeof(vChar), &cbTemp);
									vLong *= -1;
								}
								if(SUCCEEDED(hr))
								{
									vULong = (ULONG)vLong;
									hr = Formatting::TCopyULong(tzTemp, ARRAYSIZE(tzTemp), vULong, cchCopy, 10, &cchCopy);
									if(SUCCEEDED(hr))
									{
										hr = Stream::TWrite(pStream, tzTemp, cchCopy, &cbTemp);
									}
								}
							}
							break;
						case L'f':
							if(cchCopy == -1)
								cchCopy = 8;
							vDouble = (DOUBLE)va_arg(vArgs,DOUBLE);
							hr = Formatting::TDoubleToString(vDouble, tzTemp, ARRAYSIZE(tzTemp), cchCopy, &cchCopy);
							if(SUCCEEDED(hr))
								hr = Stream::TWrite(pStream, tzTemp, cchCopy, &cbTemp);
							break;
						case L's':
	Unicode_String_Copy:
							if(cchCopy != 0)
							{
								vString = va_arg(vArgs, PCWSTR);
								hr = TCopyNoConversion<WCHAR, SLP(Formatting::c_wzNullFmt)>(pStream, vString, TStrLenChecked(vString), cchCopy);
							}
							break;
						case L'S':
	Ansi_String_Copy:
							if(cchCopy != 0)
							{
								vAnsi = va_arg(vArgs, PCSTR);
								if(NULL == vAnsi)
									vAnsi = Formatting::c_szNullFmt;
								hr = CopyAnsiToStreamW(nCodePage, pStream, vAnsi, TStrLenAssert(vAnsi), cchCopy);
							}
							break;
						case L'u':
#ifndef	_WIN64
	UInt32_Copy:
#endif
							if(cchCopy != 0)
							{
								vULong = va_arg(vArgs, ULONG);
								hr = Formatting::TCopyULong(tzTemp, ARRAYSIZE(tzTemp), vULong, cchCopy, 10, &cchCopy);
								if(SUCCEEDED(hr))
								{
									hr = Stream::TWrite(pStream, tzTemp, cchCopy, &cbTemp);
								}
							}
							break;
						case L'x':
						case L'X':
							if(cchCopy != 0)
							{
								vULong = va_arg(vArgs, ULONG);
								hr = Formatting::TCopyULong(tzTemp, ARRAYSIZE(tzTemp), vULong, cchCopy, 16, &cchCopy);
								if(SUCCEEDED(hr))
								{
									if(tch == L'X')
									{
										Formatting::TStrUpr(tzTemp);
									}
									hr = Stream::TWrite(pStream, tzTemp, cchCopy, &cbTemp);
								}
							}
							break;
						case L'o':
						case L'O':
							if(cchCopy != 0)
							{
								vULong = va_arg(vArgs, ULONG);
								hr = Formatting::TCopyULong(tzTemp, ARRAYSIZE(tzTemp), vULong, cchCopy, 8, &cchCopy);
								if(SUCCEEDED(hr))
								{
									hr = Stream::TWrite(pStream, tzTemp, cchCopy, &cbTemp);
								}
							}
							break;
						case L'p':
						case L'P':
#if defined(_WIN64)
							{
								ULARGE_INTEGER uliPtr;
								uliPtr.QuadPart = va_arg(vArgs, unsigned __int64);
								hr = Formatting::TCopyULong(tzTemp, ARRAYSIZE(tzTemp), uliPtr.HighPart, 8, 16, &cchCopy);
								if(SUCCEEDED(hr))
								{
									tzTemp[8] = L':';
									hr = Formatting::TCopyULong(tzTemp + 9, ARRAYSIZE(tzTemp) - 9, uliPtr.LowPart, 8, 16, &cchCopy);
								}
							}
#else
							vULong = va_arg(vArgs, ULONG);
							hr = Formatting::TCopyULong(tzTemp, ARRAYSIZE(tzTemp), HIWORD(vULong), 4, 16, &cchCopy);
							if(SUCCEEDED(hr))
							{
								tzTemp[4] = L':';
								hr = Formatting::TCopyULong(tzTemp + 5, ARRAYSIZE(tzTemp) - 5, LOWORD(vULong), 4, 16, &cchCopy);
							}
#endif
							if(SUCCEEDED(hr))
							{
								if(tch == L'P')
								{
									Formatting::TStrUpr(tzTemp);
								}
								hr = Stream::TWrite(pStream, tzTemp, sizeof(size_t) * 2 + 1, &cbTemp);
							}
							break;
						case L'h':
							tch = *pctzFormat;
							if(tch == L's' || tch == L'S')
							{
								pctzFormat++;
								goto Ansi_String_Copy;
							}
							break;
						case L'l':
							tch = *pctzFormat;
							if(tch == L's' || tch == L'S')
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
							hr = Formatting::TCopyDouble(tzTemp, ARRAYSIZE(tzTemp), vDouble, cchCopy, tch, &cchCopy);
							if(SUCCEEDED(hr))
								hr = Stream::TWrite(pStream, tzTemp, cchCopy, &cbTemp);
							break;
						case L'z':
						case L'Z':
							if(cchCopy != 0)
							{
								PCWSTR pcwzHex = (tch == L'z') ? Formatting::c_wzHexLower : Formatting::c_wzHexUpper;
								LPBYTE vBytes = va_arg(vArgs,LPBYTE);
								LONG cSize = va_arg(vArgs,LONG);
								INT i;
								PWSTR pszPtr = tzTemp;

								if(cchCopy == -1 || cchCopy > cSize)
									cchCopy = cSize;

								for(i = 0; i < cchCopy; i++)
								{
									pszPtr[0] = pcwzHex[vBytes[i] >> 4];
									pszPtr[1] = pcwzHex[vBytes[i] & 0x0F];
									pszPtr += 2;

									if((ULONG)(pszPtr - tzTemp) == ARRAYSIZE(tzTemp))
									{
										hr = Stream::TWrite(pStream, tzTemp, ARRAYSIZE(tzTemp), &cbTemp);
										if(FAILED(hr))
											break;
										pszPtr = tzTemp;
									}
								}

								if(pszPtr != tzTemp && SUCCEEDED(hr))
									hr = Stream::TWrite(pStream, tzTemp, (ULONG)(pszPtr - tzTemp), &cbTemp);
							}
							break;
						case 'q':		// Quad Word
#ifdef	_WIN64
	UInt64_Copy:
#endif
							{
								ULONGLONG vULongLong = va_arg(vArgs, ULONGLONG);
								hr = Formatting::TCopyULongLong(tzTemp, ARRAYSIZE(tzTemp), vULongLong, cchCopy, 10, &cchCopy);
								if(SUCCEEDED(hr))
								{
									hr = Stream::TWrite(pStream, tzTemp, cchCopy, &cbTemp);
								}
							}
							break;
#ifdef	FORMAT_MONEY
						case 'm':
							if(-1 == cchCopy)
							{
								CMoney* pMoney = va_arg(vArgs, CMoney*);
								hr = pMoney->GetDollarAmountW(tzTemp, ARRAYSIZE(tzTemp), &cchCopy);
								if(SUCCEEDED(hr))
									hr = Stream::TWrite(pStream, tzTemp, cchCopy, &cbTemp);
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
								hr = Formatting::TCopyLongLong(tzTemp, ARRAYSIZE(tzTemp), vLongLong, cchCopy, 10, &cchCopy);
								if(SUCCEEDED(hr))
									hr = Stream::TWrite(pStream, tzTemp, cchCopy, &cbTemp);
							}
							break;
#ifdef	FORMAT_RSTRING
						case L'r':
							{
								RSTRING rstrValue = va_arg(vArgs, RSTRING);
								if(RStrIsWide(rstrValue))
									hr = TCopyNoConversion<WCHAR, SLP(Formatting::c_wzNullFmt)>(pStream, RStrToWide(rstrValue), RStrLenInl(rstrValue), cchCopy);
								else
								{
									vAnsi = RStrToAnsi(rstrValue);
									if(NULL == vAnsi)
										hr = CopyAnsiToStreamW(nCodePage, pStream, SLP(Formatting::c_szNullFmt), cchCopy);
									else
										hr = CopyAnsiToStreamW(nCodePage, pStream, vAnsi, RStrLenInl(rstrValue), cchCopy);
 								}
							}
							break;
#endif
						case L'I':
							tch = *pctzFormat;
							if(tch == L'd')
							{
								pctzFormat++;
#ifdef	_WIN64
								goto Int64_Copy;
#else
								goto Int32_Copy;
#endif
							}
							else if(tch == L'u')
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
					if(tch == L'\0')
						break;
					hr = pStream->Write(&tch, sizeof(tch), &cbTemp);
					pctzFormat++;
				}
			} while(SUCCEEDED(hr));
		}
		else
			hr = E_INVALIDARG;

		return hr;
#else
		return ScStreamVFExW(nCodePage, pStream, pctzFormat, vArgs);
#endif
	}

	template <>
	HRESULT TPrintVF<CHAR> (ISequentialStream* pStream, PCSTR pctzFormat, va_list vArgs)
	{
#ifndef	USE_SIMBEY_CORE_API_REDIRECT
		return TPrintVFEx(CP_UTF8, pStream, pctzFormat, vArgs);
#else
		return ScStreamVFA(pStream, pctzFormat, vArgs);
#endif
	}

	template <>
	HRESULT TPrintVF<WCHAR> (ISequentialStream* pStream, PCWSTR pctzFormat, va_list vArgs)
	{
#ifndef	USE_SIMBEY_CORE_API_REDIRECT
		return TPrintVFEx(CP_UTF8, pStream, pctzFormat, vArgs);
#else
		return ScStreamVFW(pStream, pctzFormat, vArgs);
#endif
	}

	HRESULT WriteWideToCodePage (CMemoryStream* pstmDest, UINT nCodePage, DWORD dwFlags, INT cchWide, PCWSTR pcwzWide)
	{
		HRESULT hr;
		INT cchMultiByte;
		PSTR pszWritePtr;

		CheckIfIgnore(0 == cchWide, S_OK);

		cchMultiByte = WideCharToMultiByte(nCodePage, dwFlags, pcwzWide, cchWide, NULL, 0, NULL, NULL);
		CheckIfGetLastError(0 == cchMultiByte);

		Check(pstmDest->TWriteAdvance(&pszWritePtr, cchMultiByte));
		SideAssertCompare(WideCharToMultiByte(nCodePage, dwFlags, pcwzWide, cchWide, pszWritePtr, cchMultiByte, NULL, NULL), cchMultiByte);

	Cleanup:
		return hr;
	}
}
