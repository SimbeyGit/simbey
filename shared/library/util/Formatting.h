#pragma once

#ifndef	NO_FLOATING_POINT
	#include <math.h>
	#include <float.h>
#endif

#include "..\Core\Check.h"
#include "..\Core\StringCore.h"

namespace Formatting
{
	namespace ReadDate
	{
		enum Type
		{
			DateOnly,
			TimeOnly,
			DateAndTime,
			Expiration
		};
	}

	extern const CHAR c_szHexUpper[];
	extern const CHAR c_szHexLower[];

	extern const WCHAR c_wzHexUpper[];
	extern const WCHAR c_wzHexLower[];

	extern const CHAR c_szBase64[];
	extern const CHAR c_szBase64Url[];

	__declspec(selectany) extern const CHAR c_szNullFmt[] = "(null)";
	__declspec(selectany) extern const WCHAR c_wzNullFmt[] = L"(null)";

	///////////////////////////////////////////////////////////////////////////
	// 64-bit Integers to Strings
	///////////////////////////////////////////////////////////////////////////

	template <typename T>
	HRESULT TInt64ToAsc (__int64 iNumber, T* ptzBuffer, INT cchMaxBuffer, UINT iRadix, __out_opt INT* pcchWritten)
	{
		HRESULT hr;

		if(0 > iNumber)
		{
			if(0 < cchMaxBuffer)
			{
				*ptzBuffer = '-';
				hr = TUInt64ToAsc(static_cast<unsigned __int64>(-iNumber), ptzBuffer + 1, cchMaxBuffer - 1, iRadix, pcchWritten);
				if(SUCCEEDED(hr) && pcchWritten)
					(*pcchWritten)++;
			}
			else
				hr = STRSAFE_E_INSUFFICIENT_BUFFER;
		}
		else
			hr = TUInt64ToAsc(static_cast<unsigned __int64>(iNumber), ptzBuffer, cchMaxBuffer, iRadix, pcchWritten);

		return hr;
	}

	template <typename T>
	HRESULT TUInt64ToAsc (unsigned __int64 iNumber, T* ptzBuffer, INT cchMaxBuffer, UINT iRadix, __out_opt INT* pcchWritten)
	{
		HRESULT hr;
		INT c = 0;					// Number of characters written
		T* p;						// pointer to traverse string
		T* firstdig;				// pointer to first digit
		T temp;						// temp char
		UINT digval;				// value of digit
		unsigned __int64 nRadix64 = iRadix;

		p = ptzBuffer;

		firstdig = p;				// save pointer to first digit

		do
		{
			CheckIfIgnore(c >= cchMaxBuffer, STRSAFE_E_INSUFFICIENT_BUFFER);

			digval = (UINT)(iNumber % nRadix64);
			iNumber /= nRadix64;	// get next digit

			// convert to ascii and store
			if(digval > 9)
				*p++ = (T) (digval - 10 + 'a');	// a letter
			else
				*p++ = (T) (digval + '0');		// a digit

			c++;
		} while(iNumber > 0);

		// We now have the digits of the number in the buffer, but in reverse
		// order.  Thus we reverse them now.

		// terminate string; p points to last digit
		Check(TStrTerminateAssert(p--, cchMaxBuffer - c));

		do
		{
			temp = *p;
			*p = *firstdig;
			*firstdig = temp;	// swap *p and *firstdig
			--p;
			++firstdig;			// advance to next two digits
		} while(firstdig < p);	// repeat until halfway

		if(pcchWritten)
			*pcchWritten = c;

	Cleanup:
		return hr;
	}

	///////////////////////////////////////////////////////////////////////////
	// 32-bit Integers to Strings
	///////////////////////////////////////////////////////////////////////////

	template <typename T>
	HRESULT TInt32ToAsc (INT iNumber, T* ptzBuffer, INT cchMaxBuffer, UINT iRadix, __out_opt INT* pcchWritten)
	{
		HRESULT hr;

		if(0 > iNumber)
		{
			if(0 < cchMaxBuffer)
			{
				*ptzBuffer = '-';
				hr = TUInt32ToAsc(static_cast<UINT>(-iNumber), ptzBuffer + 1, cchMaxBuffer - 1, iRadix, pcchWritten);
				if(SUCCEEDED(hr) && pcchWritten)
					(*pcchWritten)++;
			}
			else
				hr = STRSAFE_E_INSUFFICIENT_BUFFER;
		}
		else
			hr = TUInt32ToAsc(static_cast<UINT>(iNumber), ptzBuffer, cchMaxBuffer, iRadix, pcchWritten);

		return hr;
	}

	template <typename T>
	HRESULT TUInt32ToAsc (UINT iNumber, T* ptzBuffer, INT cchMaxBuffer, UINT iRadix, __out_opt INT* pcchWritten)
	{
		HRESULT hr;
		INT c = 0;					// Number of characters written
		T* p;						// pointer to traverse string
		T* firstdig;				// pointer to first digit
		T temp;						// temp char
		UINT digval;				// value of digit

		p = ptzBuffer;

		firstdig = p;				// save pointer to first digit

		do
		{
			CheckIfIgnore(c >= cchMaxBuffer, STRSAFE_E_INSUFFICIENT_BUFFER);

			digval = (UINT)(iNumber % iRadix);
			iNumber /= iRadix;		// get next digit

			// convert to ascii and store
			if(digval > 9)
				*p++ = (T) (digval - 10 + 'a');	// a letter
			else
				*p++ = (T) (digval + '0');		// a digit

			c++;
		} while(iNumber > 0);

		// We now have the digits of the number in the buffer, but in reverse
		// order.  Thus we reverse them now.

		// terminate string; p points to last digit
		Check(TStrTerminateAssert(p--, cchMaxBuffer - c));

		do
		{
			temp = *p;
			*p = *firstdig;
			*firstdig = temp;	// swap *p and *firstdig
			--p;
			++firstdig;			// advance to next two digits
		} while(firstdig < p);	// repeat until halfway

		if(pcchWritten)
			*pcchWritten = c;

	Cleanup:
		return hr;
	}

	///////////////////////////////////////////////////////////////////////////
	// Strings to 64-bit Integers
	///////////////////////////////////////////////////////////////////////////

	template <typename T>
	__int64 TAscToInt64 (const T* pctzBuffer)
	{
		__int64 total = 0;			// current total
		T c;						// current char
		INT sign;					// if '-', then negative, otherwise positive

		// skip whitespace
		while(IsSpace(*pctzBuffer))
			++pctzBuffer;

		c = *pctzBuffer++;
		sign = c;					// save sign indication
		if(c == (T)'-' || c == (T)'+')
			c = *pctzBuffer++;		// skip sign

		while(c >= (T)'0' && c <= (T)'9')
		{
			total = 10 * total + (c - (T)'0');	// accumulate digit
			c = *pctzBuffer++;		// get next char
		}

		// return result, negated if necessary
		if(sign == (T)'-')
			return -total;
		else
			return total;
	}

	///////////////////////////////////////////////////////////////////////////
	// Strings to 32-bit Integers
	///////////////////////////////////////////////////////////////////////////

	template <typename T>
	inline UINT TAscToUInt32 (const T* pctzBuffer)
	{
		return (UINT)(0xFFFFFFFF & TAscToInt64(pctzBuffer));
	}

	template <typename T>
	INT TAscToInt32 (const T* pctzBuffer)
	{
		INT total = 0;				// current total
		T c;						// current char
		INT sign;					// if '-', then negative, otherwise positive

		// skip whitespace
		while(IsSpace(*pctzBuffer))
			++pctzBuffer;

		c = *pctzBuffer++;
		sign = c;					// save sign indication
		if(c == (T)'-' || c == (T)'+')
			c = *pctzBuffer++;		// skip sign

		while(c >= (T)'0' && c <= (T)'9')
		{
			total = 10 * total + (c - (T)'0');	// accumulate digit
			c = *pctzBuffer++;		// get next char
		}

		// return result, negated if necessary
		if(sign == (T)'-')
			return -total;
		else
			return total;
	}

	template <typename T>
	bool TCharToXValue (T tch, int radix, __out ULONGLONG* pnValue)
	{
		if((T)'0' <= tch && tch <= ((T)'9' - 10) + min(radix, 10))
		{
			*pnValue = tch - (T)'0';
			return true;
		}
		else if((T)'A' <= tch && tch < ((T)'A' - 10) + radix)
		{
			*pnValue = tch - ((T)'A' - 10);
			return true;
		}
		else if((T)'a' <= tch && tch < ((T)'a' - 10) + radix)
		{
			*pnValue = tch - ((T)'a' - 10);
			return true;
		}
		return false;
	}

	template <typename T>
	ULONGLONG TBoundedAscToXUInt64 (const T* pctzBuffer, const T* pctzEnd, int radix, __out_opt const T** ppctzEnd = static_cast<const T**>(NULL))
	{
		ULONGLONG nTotal = 0;
		ULONGLONG nValue;

		while(pctzBuffer < pctzEnd && IsSpace(*pctzBuffer))
			pctzBuffer++;

		while(pctzBuffer < pctzEnd && TCharToXValue(*pctzBuffer, radix, &nValue))
		{
			nTotal = nTotal * radix + nValue;
			pctzBuffer++;
		}

		if(ppctzEnd)
			*ppctzEnd = pctzBuffer;

		return nTotal;
	}

	template <typename T>
	ULONGLONG TAscToXUInt64 (const T* pctzBuffer, int radix, __out_opt const T** ppctzEnd = static_cast<const T**>(NULL))
	{
		ULONGLONG nTotal = 0;
		ULONGLONG nValue;

		while(IsSpace(*pctzBuffer))
			pctzBuffer++;

		while(TCharToXValue(*pctzBuffer, radix, &nValue))
		{
			nTotal = nTotal * radix + nValue;
			pctzBuffer++;
		}

		if(ppctzEnd)
			*ppctzEnd = pctzBuffer;

		return nTotal;
	}

	template <typename T>
	UINT TAscToXUInt32 (const T* pctzBuffer, int radix, __out_opt const T** ppctzEnd = static_cast<const T**>(NULL))
	{
		return (UINT)(0xFFFFFFFF & TAscToXUInt64(pctzBuffer, radix, ppctzEnd));
	}

	///////////////////////////////////////////////////////////////////////////
	// Doubles to Strings
	///////////////////////////////////////////////////////////////////////////

#ifndef	NO_FLOATING_POINT
	template <typename T>
	HRESULT TFormatRawDoubleString (T* ptzDest, INT cchMaxDest, __out INT* pcchWritten, const T* pctzDouble, INT cchDouble, INT decpt, INT cMaxPlaces, INT sign)
	{
		HRESULT hr;
		T* ptzWrite = ptzDest;

		CheckIf(4 > cchMaxDest, STRSAFE_E_INSUFFICIENT_BUFFER);

		if(sign)
		{
			*ptzWrite = '-';
			ptzWrite++;
			cchMaxDest--;
		}

		if(0 >= decpt)
		{
			ptzWrite[0] = '0';
			ptzWrite[1] = '.';
			ptzWrite += 2;
			cchMaxDest -= 2;

			while(0 > decpt++)
			{
				*ptzWrite = '0';
				ptzWrite++;
				cchMaxDest--;
			}

			decpt = 0;
		}
		else
		{
			INT cchCopy = min(decpt, cchDouble);

			Check(TStrCchCpyNEx(ptzWrite, cchMaxDest, pctzDouble, cchCopy, &ptzWrite, &cchMaxDest));
			CheckIf(0 == cchMaxDest, STRSAFE_E_INSUFFICIENT_BUFFER);

			while(decpt > cchCopy)
			{
				*ptzWrite = '0';
				ptzWrite++;
				cchMaxDest--;
				CheckIf(0 == cchMaxDest, STRSAFE_E_INSUFFICIENT_BUFFER);

				cchCopy++;
			}

			*ptzWrite = '.';
			ptzWrite++;
			cchMaxDest--;
		}

		if(cchDouble > decpt)
		{
			INT cchCopy = cchDouble - decpt;
			const T* pctzCopy = pctzDouble + decpt;
			if(cchCopy > cMaxPlaces)
				cchCopy = cMaxPlaces;
			while(1 < cchCopy && '0' == pctzCopy[cchCopy - 1])
				cchCopy--;
			Check(TStrCchCpyNEx(ptzWrite, cchMaxDest, pctzCopy, cchCopy, &ptzWrite, &cchMaxDest));
		}
		else
		{
			CheckIf(0 == cchMaxDest, STRSAFE_E_INSUFFICIENT_BUFFER);

			*ptzWrite = '0';
			ptzWrite++;
			cchMaxDest--;

			Check(TStrTerminateAssert(ptzWrite, cchMaxDest));
		}

		if(pcchWritten)
			*pcchWritten = static_cast<INT>(ptzWrite - ptzDest);

	Cleanup:
		return hr;
	}

	template <typename D, typename T>
	HRESULT TCopyInfinity (D dValue, T* ptzBuffer, INT cchMaxBuffer, __out_opt INT* pcchWritten)
	{
		static const T c_tzInfinity[] = { '1', '.', '#', 'I', 'N', 'F', 'I', 'N', 'I', 'T', 'Y', '\0' };

		if(dValue < DBL_MIN)
		{
			*ptzBuffer = '-';
			ptzBuffer++;
			cchMaxBuffer--;

			if(pcchWritten)
				*pcchWritten = StaticLength(c_tzInfinity) + 1;
		}
		else if(dValue > DBL_MAX)
		{
			*ptzBuffer = '+';
			ptzBuffer++;
			cchMaxBuffer--;

			if(pcchWritten)
				*pcchWritten = StaticLength(c_tzInfinity) + 1;
		}
		else if(pcchWritten)
			*pcchWritten = StaticLength(c_tzInfinity);

		return TStrCchCpy(ptzBuffer, cchMaxBuffer, c_tzInfinity);
	}

	template <typename D, typename T>
	inline HRESULT TFloatingPointToString (D dValue, T* ptzBuffer, INT cchMaxBuffer, INT cPrecision, INT cMaxPlaces, __out_opt INT* pcchWritten)
	{
		if(_finite(dValue))
		{
			T tzBuffer[40];
			INT decpt, sign, cch;

			cch = TConvertDouble(static_cast<D>(dValue), tzBuffer, ARRAYSIZE(tzBuffer), cPrecision, &decpt, &sign, 1);

			return TFormatRawDoubleString(ptzBuffer, cchMaxBuffer, pcchWritten, tzBuffer, cch, decpt, cMaxPlaces, sign);
		}
		else
			return TCopyInfinity(dValue, ptzBuffer, cchMaxBuffer, pcchWritten);
	}

	template <typename T>
	HRESULT TFloatToString (FLOAT fValue, T* ptzBuffer, INT cchMaxBuffer, INT cMaxPlaces, __out_opt INT* pcchWritten)
	{
		return TFloatingPointToString(fValue, ptzBuffer, cchMaxBuffer, 7, cMaxPlaces, pcchWritten);
	}

	template <typename T>
	HRESULT TDoubleToString (DOUBLE fValue, T* ptzBuffer, INT cchMaxBuffer, INT cMaxPlaces, __out_opt INT* pcchWritten)
	{
		return TFloatingPointToString(fValue, ptzBuffer, cchMaxBuffer, 15, cMaxPlaces, pcchWritten);
	}
#endif

	///////////////////////////////////////////////////////////////////////////
	// Formatting Helper Functions
	///////////////////////////////////////////////////////////////////////////

	template <typename T>
	T* TStrLwr (T* ptzText)
	{
		for(T* cp = ptzText; *cp; ++cp)
		{
			if('A' <= *cp && *cp <= 'Z')
				*cp += 32;
		}
		return ptzText;
	}

	template <typename T>
	T* TStrUpr (T* ptzText)
	{
		for(T* cp = ptzText; *cp; ++cp)
		{
			if('a' <= *cp && *cp <= 'z')
				*cp -= 32;
		}
		return ptzText;
	}

	///////////////////////////////////////////////////////////////////////////
	// Formatting Helper Functions
	///////////////////////////////////////////////////////////////////////////

#ifndef	NO_FLOATING_POINT
	template <typename T>
	INT TConvertDouble (DOUBLE fValue, T* ptzBuffer, INT cMaxBuffer, INT cPrecision, INT* decpt, INT* sign, INT eFlag)
	{
		INT r2;
		DOUBLE fi, fj;
		T* p, *p1;

		if(cPrecision < 0) cPrecision = 0;
		if(cPrecision >= cMaxBuffer - 1) cPrecision = cMaxBuffer - 2;

		r2 = 0;
		*sign = 0;
		p = &ptzBuffer[0];
		if(fValue < 0.0)
		{
			*sign = 1;
			fValue = -fValue;
		}
		fValue = modf(fValue, &fi);
		p1 = &ptzBuffer[cMaxBuffer];

		if(fi != 0.0)
		{
			do
			{
				fj = modf(fi / 10.0, &fi);
				*--p1 = (T)((fj + 0.03) * 10.0) + '0';
				r2++;
			} while(fi != 0.0 && p1 > ptzBuffer);
			while(p1 < &ptzBuffer[cMaxBuffer])
				*p++ = *p1++;
		}
		else if(fValue > 0.0)
		{
			while((fj = fValue * 10.0) < 1.0)
			{
				fValue = fj;
				r2--;
			}
		}
		p1 = &ptzBuffer[cPrecision];
		if(eFlag == 0)
			p1 += r2;
		*decpt = r2;
		while(p <= p1 && p < &ptzBuffer[cMaxBuffer])
		{
			fValue *= 10.0;
			fValue = modf(fValue, &fj);
			*p++ = (T)fj + '0';
		}
		if(p1 >= &ptzBuffer[cMaxBuffer])
		{
			ptzBuffer[cMaxBuffer - 1] = '\0';
			return cMaxBuffer - 1;
		}
		if('0' == *p1)
			p1--;
		p = p1;
		*p1 += 5;
		if(*p1 > '9')
		{
			do
			{
				*p1 = '0';
				if(p1 > ptzBuffer)
					++*--p1;
				else
				{
					*p1 = '1';
					(*decpt)++;
					if(eFlag == 0)
					{
						if(p > ptzBuffer)
							*p = '0';
						p++;
					}
				}
			} while(*p1 > '9');
			p = p1 + 1;
		}
		*p = '\0';
		return static_cast<INT>(p - ptzBuffer);
	}
#endif

	template <typename T>
	HRESULT TCopyULong (T* ptzOutput, INT cchMaxOutput, ULONG iValue, INT cCopy, INT iRadix, INT* pcchCopied)
	{
		HRESULT hr;
		INT cchTemp;

		Check(TUInt32ToAsc(iValue, ptzOutput, cchMaxOutput, iRadix, &cchTemp));
		if(cchTemp < cCopy)
		{
			INT i, c = cCopy - cchTemp;

			CheckIf(cchTemp + c >= cchMaxOutput, STRSAFE_E_INSUFFICIENT_BUFFER);

			// Shift the digits to the right by c for the '0' padding.
			MoveMemory(ptzOutput + c, ptzOutput, (cchTemp + 1) * sizeof(T));

			for(i = 0; i < c; i++)
			{
				*ptzOutput = '0';
				ptzOutput++;
			}

			cchTemp += c;
		}

		*pcchCopied = cchTemp;

	Cleanup:
		return hr;
	}

	template <typename T>
	HRESULT TCopyLongLong (T* ptzOutput, INT cchMaxOutput, LONGLONG iValue, INT cCopy, INT iRadix, INT* pcchCopied)
	{
		HRESULT hr;
		INT cchTemp;

		Check(TInt64ToAsc(iValue, ptzOutput, cchMaxOutput, iRadix, &cchTemp));
		if(cchTemp < cCopy)
		{
			INT i, c = cCopy - cchTemp;

			CheckIf(cchTemp + c >= cchMaxOutput, STRSAFE_E_INSUFFICIENT_BUFFER);

			// Shift the digits to the right by c for the '0' padding.
			MoveMemory(ptzOutput + c, ptzOutput, (cchTemp + 1) * sizeof(T));

			for(i = 0; i < c; i++)
			{
				*ptzOutput = '0';
				ptzOutput++;
			}

			cchTemp += c;
		}

		*pcchCopied = cchTemp;

	Cleanup:
		return hr;
	}

	template <typename T>
	HRESULT TCopyULongLong (T* ptzOutput, INT cchMaxOutput, ULONGLONG iValue, INT cCopy, INT iRadix, INT* pcchCopied)
	{
		HRESULT hr;
		INT cchTemp;

		Check(TUInt64ToAsc(iValue, ptzOutput, cchMaxOutput, iRadix, &cchTemp));
		if(cchTemp < cCopy)
		{
			INT i, c = cCopy - cchTemp;

			CheckIf(cchTemp + c >= cchMaxOutput, STRSAFE_E_INSUFFICIENT_BUFFER);

			// Shift the digits to the right by c for the '0' padding.
			MoveMemory(ptzOutput + c, ptzOutput, (cchTemp + 1) * sizeof(T));

			for(i = 0; i < c; i++)
			{
				*ptzOutput = '0';
				ptzOutput++;
			}

			cchTemp += c;
		}

		*pcchCopied = cchTemp;

	Cleanup:
		return hr;
	}

#ifndef	NO_FLOATING_POINT
	template <typename T>
	HRESULT TCopyDouble (T* ptzOutput, INT cchMaxOutput, DOUBLE fValue, INT cCopy, T chFormat, __out INT* pcchCopied)
	{
		HRESULT hr;

		if(_finite(fValue))
		{
			INT decpt, sign, exp, cchDouble;
			T szTemp[40];

			if(cCopy == -1)
				cCopy = 8;

			cchDouble = TConvertDouble(fValue, szTemp, ARRAYSIZE(szTemp), 15, &decpt, &sign, 1);

			if(chFormat == 'G' || chFormat == 'g')
			{
				INT magnitude = decpt - 1;
				if(magnitude < -4 || magnitude > cCopy - 1)
					chFormat -= 2;				// 'e' or 'E'
				else
					chFormat = 'f';
			}

			if(chFormat == 'f')
			{
				Check(TFormatRawDoubleString(ptzOutput, cchMaxOutput, pcchCopied, szTemp, cchDouble, decpt, cCopy, sign));
			}
			else
			{
				T* ptzStart = ptzOutput;
				INT nMinTest;

				if(decpt < 0)
				{
					// Test all the digits.
					nMinTest = 0;
				}
				else
				{
					// Only test back to the decimal point.
					nMinTest = decpt;

					// cCopy initially indicates how many decimal places to copy,
					// but now it needs to be updated to include the left side.
					cCopy += decpt;
				}

				while(cchDouble > nMinTest && szTemp[cchDouble - 1] == '0')
					cchDouble--;

				if(cchDouble < cCopy) cCopy = cchDouble;
				CheckIf(cchMaxOutput <= cCopy + 16, STRSAFE_E_INSUFFICIENT_BUFFER);

				if(sign) *ptzOutput++ = '-';
				*ptzOutput++ = szTemp[0];
				if(cCopy > 0)
				{
					*ptzOutput++ = '.';
					cCopy--;
				}
				CopyMemory(ptzOutput, szTemp + 1, cCopy * sizeof(T));
				ptzOutput += cCopy;
				*ptzOutput++ = chFormat;	// 'e' or 'E'

				if(decpt == 0)
				{
					if(fValue == 0.0)
						exp = 0;
					else
						exp = -1;
				}
				else
					exp = decpt - 1;

				if(exp < 0)
				{
					*ptzOutput++ = '-';
					exp = -exp;
				}
				else
					*ptzOutput++ = '+';

				Check(TUInt32ToAsc(static_cast<UINT>(exp), ptzOutput, cchMaxOutput - static_cast<INT>(ptzOutput - ptzStart), 10, pcchCopied));
				ptzOutput += *pcchCopied;

				*pcchCopied = static_cast<INT>(ptzOutput - ptzStart);
			}
		}
		else
			Check(TCopyInfinity(fValue, ptzOutput, cchMaxOutput, pcchCopied));

	Cleanup:
		return hr;
	}
#endif

	template <typename T>
	HRESULT TPrintVFEx (UINT nCodePage, T* ptzOutput, INT cchMaxOutput, INT* pcchOutput, const T* pctzFormat, va_list vArgs)
	{
		// This template must not be used.  Either TPrintVFEx<CHAR> or TPrintVFEx<WCHAR> should be selected.
		Assert(false);
		return E_NOTIMPL;
	}

	template <>
	HRESULT TPrintVFEx<CHAR> (UINT nCodePage, __out_ecount(cchMaxOutput) PSTR ptzOutput, INT cchMaxOutput, INT* pcchOutput, PCSTR pctzFormat, va_list vArgs);

	template <>
	HRESULT TPrintVFEx<WCHAR> (UINT nCodePage, __out_ecount(cchMaxOutput) PWSTR ptzOutput, INT cchMaxOutput, INT* pcchOutput, PCWSTR pctzFormat, va_list vArgs);

	template <typename T>
	HRESULT TPrintFEx (UINT nCodePage, __out_ecount(cchMaxOutput) T* ptzOutput, INT cchMaxOutput, INT* pcchOutput, const T* pctzFormat, ...)
	{
		HRESULT hr;

		va_list vArgs;
		va_start(vArgs, pctzFormat);
		hr = TPrintVFEx(nCodePage, ptzOutput, cchMaxOutput, pcchOutput, pctzFormat, vArgs);
		va_end(vArgs);

		return hr;
	}

	template <typename T>
	HRESULT TPrintVF (T* ptzOutput, INT cchMaxOutput, INT* pcchOutput, const T* pctzFormat, va_list vArgs)
	{
		// This template must not be used.  Either TPrintVF<CHAR> or TPrintVF<WCHAR> should be selected.
		Assert(false);
		return E_NOTIMPL;
	}

	template <>
	HRESULT TPrintVF<CHAR> (__out_ecount(cchMaxOutput) PSTR ptzOutput, INT cchMaxOutput, INT* pcchOutput, PCSTR pctzFormat, va_list vArgs);

	template <>
	HRESULT TPrintVF<WCHAR> (__out_ecount(cchMaxOutput) PWSTR ptzOutput, INT cchMaxOutput, INT* pcchOutput, PCWSTR pctzFormat, va_list vArgs);

	template <typename T>
	HRESULT TPrintF (__out_ecount(cchMaxOutput) T* ptzOutput, INT cchMaxOutput, INT* pcchOutput, const T* pctzFormat, ...)
	{
		HRESULT hr;

		va_list vArgs;
		va_start(vArgs, pctzFormat);
		hr = TPrintVF(ptzOutput, cchMaxOutput, pcchOutput, pctzFormat, vArgs);
		va_end(vArgs);

		return hr;
	}

	template <typename T>
	HRESULT TBuildDirectory (const T* pctzCurrentDirectory, INT cchCurrentDirectory, const T* pctzRelativePath, INT cchRelativePath, T** pptzAbsolutePath, __out_opt INT* pcchAbsolutePath)
	{
		HRESULT hr;
		T* ptzAbsolutePath = NULL;

		Assert(pctzCurrentDirectory && (0 == pctzRelativePath || pctzRelativePath) && pptzAbsolutePath);

#ifdef	WINCE
		// The shortest valid directory is a single backslash.
		CheckIf(1 > cchCurrentDirectory, E_INVALIDARG);
#else
		// The shortest current directory could be a drive letter and a colon.
		CheckIf(2 > cchCurrentDirectory, E_INVALIDARG);
#endif

		if(2 <= cchRelativePath && pctzRelativePath[1] == ':')
		{
			ptzAbsolutePath = __new T[cchRelativePath + 1];
			CheckAlloc(ptzAbsolutePath);
			CopyMemory(ptzAbsolutePath, pctzRelativePath, sizeof(T) * cchRelativePath);
			ptzAbsolutePath[cchRelativePath] = '\0';
			if(pcchAbsolutePath)
				*pcchAbsolutePath = cchRelativePath;
		}
		else
		{
			INT cchMaxAbsolutePath = cchCurrentDirectory + cchRelativePath + 2;

			ptzAbsolutePath = __new T[cchMaxAbsolutePath];
			CheckAlloc(ptzAbsolutePath);

			if(1 <= cchRelativePath && '\\' == pctzRelativePath[0])
			{
				bool fNetworkPath = 2 <= cchRelativePath && '\\' == pctzRelativePath[1];
				T* ptzDest = ptzAbsolutePath;

				if(!fNetworkPath && ':' == pctzCurrentDirectory[1])
				{
					const T ctzPathFormat[] = { '%', 'c', ':', '\0' };
					Check(Formatting::TPrintF(ptzAbsolutePath, cchMaxAbsolutePath, NULL, ctzPathFormat, pctzCurrentDirectory[0]));
					ptzDest += 2;

					if(pcchAbsolutePath)
						*pcchAbsolutePath = cchRelativePath + 2;
				}
				else if(pcchAbsolutePath)
					*pcchAbsolutePath = cchRelativePath;

				CopyMemory(ptzDest, pctzRelativePath, sizeof(T) * cchRelativePath);
				ptzDest[cchRelativePath] = '\0';
			}
			else
			{
				T chC = 0, chLastC = 0;
				INT nWriteAt, cPeriods = 0;
				BOOL fCountingPeriods = FALSE;

				Check(TStrCchCpyLen(ptzAbsolutePath, cchMaxAbsolutePath, pctzCurrentDirectory, cchCurrentDirectory, NULL));

				nWriteAt = cchCurrentDirectory;
				if(ptzAbsolutePath[nWriteAt - 1] != '\\' && 0 < cchRelativePath)
					ptzAbsolutePath[nWriteAt++] = '\\';
				for(INT n = 0; n < cchRelativePath; n++)
				{
					chC = pctzRelativePath[n];
					switch(chC)
					{
					case '.':
						if(fCountingPeriods)
							cPeriods++;
						else if(chLastC == '\\' || chLastC == 0)
						{
							cPeriods = 0;
							fCountingPeriods = TRUE;
						}
						else
							ptzAbsolutePath[nWriteAt++] = '.';
						break;
					default:
						if(fCountingPeriods)
						{
							fCountingPeriods = FALSE;
							if(cPeriods > 0)
							{
								INT count = 0;
								INT iTemp = nWriteAt;

								for(INT i = nWriteAt-1; i > 1; i--)
								{
									if(ptzAbsolutePath[i] == '\\')
									{
										if(count++ == cPeriods)
										{
											nWriteAt = i+1;
											chLastC = ptzAbsolutePath[i];
											break;
										}
									}
								}

								if(iTemp == nWriteAt)
								{
									if(chC == '\\' && ptzAbsolutePath[nWriteAt - 1] == '\\')
										nWriteAt--;
								}
							}
							else
							{
								Assert('.' == chLastC);

								if('\\' == chC)
								{
									// If there was only one period, treat it like a backslash
									// so that two backslashes won't be written consecutively.
									chLastC = '\\';
								}
								else
									ptzAbsolutePath[nWriteAt++] = L'.';
							}
						}

						// Intent:  Don't write consecutive backslashes.
						if(!(chC == '\\' && chLastC == '\\'))
							ptzAbsolutePath[nWriteAt++] = chC;
					}
					chLastC = chC;
				}

				// Check if had been counting
				if(fCountingPeriods)
				{
					fCountingPeriods = FALSE;
					if(cPeriods > 0)
					{
						INT count = 0;
						INT iTemp = nWriteAt;

						for(INT i = nWriteAt - 1; i >= 0; i--)
						{
							if(ptzAbsolutePath[i] == '\\')
							{
								if(count++ == cPeriods)
								{
									nWriteAt = i+1;
									chLastC = ptzAbsolutePath[i];
									break;
								}
							}
						}

						if(iTemp == nWriteAt)
						{
							if(chC == '\\' && ptzAbsolutePath[nWriteAt - 1] == '\\')
								nWriteAt--;
						}
					}
					if(ptzAbsolutePath[nWriteAt-1] == '\\')
					{
						if(nWriteAt > 1)
							nWriteAt--;
					}
				}

				// Set End Zero
				Check(TStrTerminateAssert(ptzAbsolutePath + nWriteAt, cchMaxAbsolutePath - nWriteAt));

				if(pcchAbsolutePath)
					*pcchAbsolutePath = nWriteAt;
			}
		}

		*pptzAbsolutePath = ptzAbsolutePath;
		ptzAbsolutePath = NULL;

		hr = S_OK;

	Cleanup:
		__delete_array ptzAbsolutePath;
		return hr;
	}

	template <typename TChar>
	inline UINT THexValue (TChar tch)
	{
		UINT n = 0;

		if(tch >= '0' && tch <= '9')
			n = tch - '0';
		else if(tch >= 'A' && tch <= 'F')
			n = tch - 'A' + 10;
		else if(tch >= 'a' && tch <= 'f')
			n = tch - 'a' + 10;

		return n;
	}

	template <typename TChar>
	LONG THexToLong (const TChar* pctzHex, INT cDigits)
	{
		LONG iValue = 0;
		TChar tch;

		for(INT i = 0; i < cDigits; i++)
		{
			tch = *pctzHex;
			if(tch >= '0' && tch <= '9')
				iValue = iValue * 16 + (tch - '0');
			else if(tch >= 'A' && tch <= 'F')
				iValue = iValue * 16 + (tch - 'A' + 10);
			else if(tch >= 'a' && tch <= 'f')
				iValue = iValue * 16 + (tch - 'a' + 10);
			else
				break;
			pctzHex++;
		}

		return iValue;
	}

	template <typename TChar>
	TChar* Trim (TChar* ptzString)
	{
		TChar* ptzSpace = NULL, *ptzIter;
		while(IsSpace(*ptzString))
			ptzString++;
		ptzIter = ptzString;
		while(*ptzIter)
		{
			if(!IsSpace(*ptzIter))
				ptzSpace = NULL;
			else if(NULL == ptzSpace)
				ptzSpace = ptzIter;
			ptzIter++;
		}
		if(ptzSpace)
			*ptzSpace = '\0';
		return ptzString;
	}

	template <typename TValidator, typename TChar>
	BOOL IsWhiteSpaceOrEmpty (const TChar* pctzString)
	{
		BOOL fIsWhiteSpaceOrEmpty = TRUE;
		if(TValidator::ValidateParam(pctzString))
		{
			while(*pctzString)
			{
				if(!IsSpace(*pctzString))
				{
					fIsWhiteSpaceOrEmpty = FALSE;
					break;
				}
				pctzString++;
			}
		}
		return fIsWhiteSpaceOrEmpty;
	}

	template <typename T>
	HRESULT TReadDateFromString (const T* pctzDate, __out SYSTEMTIME* pst, __out ReadDate::Type* peType)
	{
		HRESULT hr = S_OK;
		static const T c_tzEnd1[] = { '/', '-', '\0' };
		static const T c_tzEnd2[] = { ' ', '\t', '\0' };
		static const T c_tzEnd3[] = { 'a', 'A', 'p', 'P', '\0' };
		const T* pctzSep = TFindEnd(pctzDate, c_tzEnd1);

		pst->wDayOfWeek = 0;
		pst->wMilliseconds = 0;

		if(pctzSep)
		{
			pst->wMonth = (WORD)Formatting::TAscToUInt32(pctzDate);
			CheckIfIgnore(0 == pst->wMonth, HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));

			pctzDate = pctzSep + 1;
			CheckIf('\0' == *pctzDate, HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
			pctzSep = TFindEnd(pctzDate, c_tzEnd1);

			if(pctzSep)
			{
				// It's a regular mm/dd/yy date
				pst->wDay = (WORD)Formatting::TAscToUInt32(pctzDate);
				pctzDate = pctzSep + 1;
				CheckIf('\0' == *pctzDate, HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));

				pst->wYear = (WORD)Formatting::TAscToUInt32(pctzDate);

				pctzSep = TFindEnd(pctzDate, c_tzEnd2);
				if(pctzSep)
				{
					do
					{
						pctzSep++;
					} while(IsSpace(*pctzSep));
					pctzDate = pctzSep;

					if(*pctzDate < '0' || '9' < *pctzDate)
						pctzDate = NULL;
				}
				else
					pctzDate = NULL;

				if(pctzDate)
					*peType = ReadDate::DateAndTime;
				else
					*peType = ReadDate::DateOnly;
			}
			else
			{
				// It's an expiration mm/yy date
				pst->wDay = 1;
				pst->wYear = (WORD)Formatting::TAscToUInt32(pctzDate);

				pctzDate = NULL;
				*peType = ReadDate::Expiration;
			}

			// Check for yyyy/mm/dd format
			if(pst->wMonth > 12)
			{
				WORD wYear = pst->wMonth;
				pst->wMonth = pst->wDay;
				pst->wDay = pst->wYear;
				pst->wYear = wYear;
			}

			if(pst->wYear < 100)
			{
				SYSTEMTIME stNow;
				GetLocalTime(&stNow);
				pst->wYear += 1000 * (stNow.wYear / 1000);
			}
		}
		else
		{
			*peType = ReadDate::TimeOnly;

			pst->wMonth = 0;
			pst->wDay = 0;
			pst->wYear = 0;
		}

		if(pctzDate)
		{
			CheckIfIgnore(*pctzDate < '0' || *pctzDate > '9', HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));

			pst->wHour = Formatting::TAscToUInt32(pctzDate);
			pctzSep = TStrChr(pctzDate, static_cast<T>(':'));
			CheckIf(NULL == pctzSep, HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
			pctzDate = pctzSep + 1;
			CheckIf('\0' == *pctzDate, HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));

			pst->wMinute = Formatting::TAscToUInt32(pctzDate);
			pctzSep = TStrChr(pctzDate, static_cast<T>(':'));

			if(pctzSep)
			{
				pctzDate = pctzSep + 1;
				CheckIf('\0' == *pctzDate, HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));

				pst->wSecond = static_cast<WORD>(Formatting::TAscToUInt32(pctzDate));
			}
			else
				pst->wSecond = 0;

			pctzDate = TFindEnd(pctzDate, c_tzEnd3);
			if(pctzDate && 'M' == TUpperCase(pctzDate[1]))
			{
				// Only adjust for AM/PM if 0 < wHour <= 12
				if('P' == TUpperCase(pctzDate[0]))
				{
					if(pst->wHour < 12)
						pst->wHour += 12;
				}
				else
				{
					if(pst->wHour == 12)
						pst->wHour = 0;
				}
			}
		}
		else
		{
			pst->wHour = 0;
			pst->wMinute = 0;
			pst->wSecond = 0;
		}

	Cleanup:
		return hr;
	}

	BOOL IsSpace (INT ch);
}
