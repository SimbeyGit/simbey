#pragma once

#include "CoreDefs.h"
#include "ContainerTraits.h"

#ifndef	STRSAFE_E_INSUFFICIENT_BUFFER
	#define	STRSAFE_E_INSUFFICIENT_BUFFER	((HRESULT)0x8007007AL)  // 0x7A = 122L = ERROR_INSUFFICIENT_BUFFER
#endif

#ifndef	STRSAFE_E_INVALID_PARAMETER
	#define STRSAFE_E_INVALID_PARAMETER		((HRESULT)0x80070057L)  // 0x57 =  87L = ERROR_INVALID_PARAMETER
#endif

#ifndef	STRSAFE_MAX_CCH
	#define	STRSAFE_MAX_CCH					0x7FFFFFFF
#endif

template <typename T>
inline T TUpperCase (T tch)
{
	if(tch >= 'a' && tch <= 'z')
		tch -= ('a' - 'A');
	return tch;
}

template <typename T>
inline T TLowerCase (T tch)
{
	if(tch >= 'A' && tch <= 'Z')
		tch += ('a' - 'A');
	return tch;
}

struct AnsiDaysAndMonths
{
	static PCSTR pctzShortDays[7];
	static PCSTR pctzLongDays[7];
	static PCSTR pctzShortMonths[12];
	static PCSTR pctzLongMonths[12];
};

__declspec(selectany) PCSTR AnsiDaysAndMonths::pctzShortDays[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
__declspec(selectany) PCSTR AnsiDaysAndMonths::pctzLongDays[7] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
__declspec(selectany) PCSTR AnsiDaysAndMonths::pctzShortMonths[12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
__declspec(selectany) PCSTR AnsiDaysAndMonths::pctzLongMonths[12] = {"January","February","March","April","May","June","July","August","September","October","November","December"};

struct WideDaysAndMonths
{
	static PCWSTR pctzShortDays[7];
	static PCWSTR pctzLongDays[7];
	static PCWSTR pctzShortMonths[12];
	static PCWSTR pctzLongMonths[12];
};

__declspec(selectany) PCWSTR WideDaysAndMonths::pctzShortDays[7] = {L"Sun",L"Mon",L"Tue",L"Wed",L"Thu",L"Fri",L"Sat"};
__declspec(selectany) PCWSTR WideDaysAndMonths::pctzLongDays[7] = {L"Sunday",L"Monday",L"Tuesday",L"Wednesday",L"Thursday",L"Friday",L"Saturday"};
__declspec(selectany) PCWSTR WideDaysAndMonths::pctzShortMonths[12] = {L"Jan",L"Feb",L"Mar",L"Apr",L"May",L"Jun",L"Jul",L"Aug",L"Sep",L"Oct",L"Nov",L"Dec"};
__declspec(selectany) PCWSTR WideDaysAndMonths::pctzLongMonths[12] = {L"January",L"February",L"March",L"April",L"May",L"June",L"July",L"August",L"September",L"October",L"November",L"December"};

///////////////////////////////////////////////////////////////////////////////
// Individual Traits
///////////////////////////////////////////////////////////////////////////////

struct AssertValidate
{
	template <typename TParam>
	static inline bool ValidateParam (const TParam* pctParam)
	{
		Assert(NULL != pctParam);
		return true;
	}
};

struct CheckedValidate
{
	template <typename TParam>
	static inline bool ValidateParam (const TParam* pctParam)
	{
		return NULL != pctParam;
	}
};

struct CharacterRead
{
	template <typename TChar>
	static inline TChar ReadCharacter (TChar tch)
	{
		return tch;
	}
};

struct CharacterReadI
{
	template <typename TChar>
	static inline TChar ReadCharacter (TChar tch)
	{
		return TUpperCase(tch);
	}
};

///////////////////////////////////////////////////////////////////////////////
// Compound Traits
///////////////////////////////////////////////////////////////////////////////

struct SensitiveAssert
{
	typedef AssertValidate TValidator;
	typedef CharacterRead TReadChar;
};

struct InsensitiveAssert
{
	typedef AssertValidate TValidator;
	typedef CharacterReadI TReadChar;
};

struct SensitiveChecked
{
	typedef CheckedValidate TValidator;
	typedef CharacterRead TReadChar;
};

struct InsensitiveChecked
{
	typedef CheckedValidate TValidator;
	typedef CharacterReadI TReadChar;
};

///////////////////////////////////////////////////////////////////////////////
// TStringCore
///////////////////////////////////////////////////////////////////////////////

template <typename TTraits>
class TStringCore
{
public:
	template <typename T>
	static inline INT TStrLen (const T* pctzString)
	{
		INT cch = 0;

		if(TTraits::TValidator::ValidateParam(pctzString))
		{
			while(*pctzString)
			{
				pctzString++;
				cch++;
			}
		}

		return cch;
	}

	template <typename T>
	static inline HRESULT TSafeStrLen (const T* pctzString, INT cchMax, __out INT* pcch)
	{
		HRESULT hr;
		INT cch = 0;

		if(TTraits::TValidator::ValidateParam(pctzString))
		{
			hr = S_OK;

			while(*pctzString)
			{
				pctzString++;
				cch++;

				if(cch == cchMax)
				{
					hr = STRSAFE_E_INVALID_PARAMETER;
					break;
				}
			}
		}
		else
			hr = E_INVALIDARG;

		*pcch = cch;

		return hr;
	}

	template <typename T>
	static inline INT TStrCchLen (__in_ecount(cchMax) const T* pctzString, INT cchMax)
	{
		const T* pctzStart = pctzString;
		const T* pctzEnd = pctzString + cchMax;

		if(TTraits::TValidator::ValidateParam(pctzString))
		{
			while(pctzString < pctzEnd && *pctzString)
				pctzString++;
		}

		return static_cast<INT>(pctzString - pctzStart);
	}

	template <typename T>
	static inline HRESULT TStrTerminate (__out_ecount(cchString) T* ptzString, INT cchString)
	{
		HRESULT hr;
		if(TTraits::TValidator::ValidateParam(ptzString))
		{
			if(0 < cchString)
			{
				hr = S_OK;
				*ptzString = '\0';
			}
			else
				hr = STRSAFE_E_INSUFFICIENT_BUFFER;
		}
		else
			hr = E_INVALIDARG;
		return hr;
	}

	template <typename T>
	static inline HRESULT TStrCchCpy (T* ptzDest, INT cchMaxDest, const T* pctzSrc)
	{
		HRESULT hr;

		if(TTraits::TValidator::ValidateParam(ptzDest) &&
			TTraits::TValidator::ValidateParam(pctzSrc))
		{
			while(0 < cchMaxDest && '\0' != *pctzSrc)
			{
				*ptzDest++ = *pctzSrc++;
				cchMaxDest--;
			}

			hr = TStringCore<SensitiveAssert>::TStrTerminate(ptzDest, cchMaxDest);
		}
		else
			hr = E_INVALIDARG;

		return hr;
	}

	template <typename T>
	static inline HRESULT TStrCchCpyEx (T* ptzDest, INT cchMaxDest, const T* pctzSrc, __out_opt T** pptzEnd, __out_opt INT* pcchRemaining)
	{
		HRESULT hr;

		if(TTraits::TValidator::ValidateParam(ptzDest) &&
			TTraits::TValidator::ValidateParam(pctzSrc))
		{
			while(0 < cchMaxDest && '\0' != *pctzSrc)
			{
				*ptzDest++ = *pctzSrc++;
				cchMaxDest--;
			}

			hr = TStringCore<SensitiveAssert>::TStrTerminate(ptzDest, cchMaxDest);

			if(pptzEnd)
				*pptzEnd = ptzDest;
			if(pcchRemaining)
				*pcchRemaining = cchMaxDest;
		}
		else
			hr = E_INVALIDARG;

		return hr;
	}

	template <typename T>
	static inline HRESULT TStrCchCpyNEx (T* ptzDest, INT cchMaxDest, const T* pctzSrc, INT cchSrc, __out_opt T** pptzEnd, __out_opt INT* pcchRemaining)
	{
		HRESULT hr;

		if(TTraits::TValidator::ValidateParam(ptzDest) &&
			TTraits::TValidator::ValidateParam(pctzSrc))
		{
			hr = S_OK;

			while(0 < cchMaxDest && 0 < cchSrc)
			{
				if('\0' == *pctzSrc)
				{
					hr = STRSAFE_E_INVALID_PARAMETER;
					break;
				}
				cchSrc--;

				*ptzDest++ = *pctzSrc++;
				cchMaxDest--;
			}

			if(SUCCEEDED(hr))
				hr = TStringCore<SensitiveAssert>::TStrTerminate(ptzDest, cchMaxDest);

			if(pptzEnd)
				*pptzEnd = ptzDest;
			if(pcchRemaining)
				*pcchRemaining = cchMaxDest;
		}
		else
			hr = E_INVALIDARG;

		return hr;
	}

	template <typename T>
	static inline HRESULT TStrCchCpyN (T* ptzDest, INT cchMaxDest, const T* pctzSrc, INT cchSrc)
	{
		return TStrCchCpyNEx(ptzDest, cchMaxDest, pctzSrc, cchSrc, reinterpret_cast<T**>(NULL), NULL);
	}

	template <typename T>
	static inline HRESULT TStrCat (T* ptzDest, INT cchMaxDest, const T* pctzSrc)
	{
		HRESULT hr;

		if(TTraits::TValidator::ValidateParam(ptzDest) &&
			TTraits::TValidator::ValidateParam(pctzSrc))
		{
			INT cchDest = TStringCore<SensitiveAssert>::TStrLen(ptzDest);
			hr = TStringCore<SensitiveAssert>::TStrCchCpy(ptzDest + cchDest, cchMaxDest - cchDest, pctzSrc);
		}
		else
			hr = E_INVALIDARG;

		return hr;
	}

	template <typename T>
	static inline INT TStrCmp (const T* pctzStringA, const T* pctzStringB)
	{
		INT nResult = 0;

		if(TTraits::TValidator::ValidateParam(pctzStringA))
		{
			if(TTraits::TValidator::ValidateParam(pctzStringB))
			{
				for(;;)
				{
					T tchA = TTraits::TReadChar::ReadCharacter(*pctzStringA);
					T tchB = TTraits::TReadChar::ReadCharacter(*pctzStringB);

					if(tchA < tchB)
					{
						nResult = -1;
						break;
					}
					if(tchA > tchB)
					{
						nResult = 1;
						break;
					}
					if(tchA == '\0')
						break;

					pctzStringA++;
					pctzStringB++;
				}
			}
			else
				nResult = 1;
		}
		else if(TTraits::TValidator::ValidateParam(pctzStringB))
			nResult = -1;

		return nResult;
	}

	// With TStrCchCmp(), pctzStringA is bound to cchStringA.  Once the end of pctzStringA
	// is reached, the strings are only considered equal if pctzStringB[cchStringA] is nill.
	template <typename T>
	static inline INT TStrCchCmp (__in_ecount(cchStringA) const T* pctzStringA, INT cchStringA, const T* pctzStringB)
	{
		INT nResult = 0;

		if(TTraits::TValidator::ValidateParam(pctzStringA))
		{
			if(TTraits::TValidator::ValidateParam(pctzStringB))
			{
				INT i = 0;

				for(;;)
				{
					if(i == cchStringA)
					{
						// If pctzStringB is not also at the end, then the
						// pctzStringA fragment must be considered less.
						nResult = '\0' == pctzStringB[i] ? 0 : -1;
						break;
					}

					T tchB = TTraits::TReadChar::ReadCharacter(pctzStringB[i]);

					if('\0' == tchB)
					{
						nResult = 1;
						break;
					}

					T tchA = TTraits::TReadChar::ReadCharacter(pctzStringA[i]);

					if(tchA < tchB)
					{
						nResult = -1;
						break;
					}
					if(tchA > tchB)
					{
						nResult = 1;
						break;
					}

					i++;
				}
			}
			else
				nResult = 1;
		}
		else if(TTraits::TValidator::ValidateParam(pctzStringB))
			nResult = -1;

		return nResult;
	}

	// With TStrCmpN(), neither string is bound to cchCompare.  cchCompare is simply
	// the number of characters to compare.  Either string may be shorter.  The
	// TStrCmpN() function is functionally equivalent to the CRT's strncmp function.
	template <typename T>
	static inline INT TStrCmpN (const T* pctzStringA, const T* pctzStringB, INT cchCompare)
	{
		INT nCompare = 0;

		if(TTraits::TValidator::ValidateParam(pctzStringA))
		{
			if(TTraits::TValidator::ValidateParam(pctzStringB))
			{
				for(INT i = 0; i < cchCompare; i++)
				{
					T tchA = TTraits::TReadChar::ReadCharacter(pctzStringA[i]);
					T tchB = TTraits::TReadChar::ReadCharacter(pctzStringB[i]);

					if(tchA < tchB)
					{
						nCompare = -1;
						break;
					}
					if(tchA > tchB)
					{
						nCompare = 1;
						break;
					}

					if('\0' == tchA)
					{
						Assert('\0' == tchB);
						break;
					}
				}
			}
			else
				nCompare = 1;
		}
		else if(TTraits::TValidator::ValidateParam(pctzStringB))
			nCompare = -1;

		return nCompare;
	}

	template <typename T>
	static inline BOOL TStrMatchLeft (const T* pctzStringA, __in_ecount(cchCompare) const T* pctzStringB, INT cchCompare)
	{
		BOOL fMatch;

		if(TTraits::TValidator::ValidateParam(pctzStringA) && (0 == cchCompare || TTraits::TValidator::ValidateParam(pctzStringB)))
		{
			fMatch = TRUE;

			for(INT i = 0; i < cchCompare; i++)
			{
				if(TTraits::TReadChar::ReadCharacter(*pctzStringA) !=
					TTraits::TReadChar::ReadCharacter(*pctzStringB))
				{
					fMatch = FALSE;
					break;
				}
				pctzStringB++;
				pctzStringA++;
			}
		}
		else
			fMatch = FALSE;

		return fMatch;
	}

	template <typename T>
	static inline BOOL TCompareLeft (const T* pctzText, const T* pctzFrag)
	{
		BOOL fMatch = FALSE;
		if(TTraits::TValidator::ValidateParam(pctzFrag))
		{
			if(TTraits::TValidator::ValidateParam(pctzText))
			{
				T tchF, tchT;
				for(;;)
				{
					tchF = TTraits::TReadChar::ReadCharacter(*pctzFrag);
					if('\0' == tchF)
					{
						fMatch = TRUE;
						break;
					}
					tchT = TTraits::TReadChar::ReadCharacter(*pctzText);
					if(tchF != tchT)
						break;
					pctzFrag++;
					pctzText++;
				}
			}
		}
		else
			fMatch = TRUE;
		return fMatch;
	}

	template <typename T, typename THeap>
	static inline HRESULT TDuplicateString (THeap& heap, const T* pctzString, __deref_out T** pptzNew, __out_opt INT* pcchLength = NULL)
	{
		HRESULT hr;

		if(TTraits::TValidator::ValidateParam(pctzString))
		{
			INT cchString = TStringCore<SensitiveAssert>::TStrLen(pctzString);
			hr = heap.allocate_storage(cchString + 1, pptzNew);
			if(SUCCEEDED(hr))
			{
				if(pcchLength)
					*pcchLength = cchString;

				CopyMemory(*pptzNew, pctzString, (cchString + 1) * sizeof(T));
			}
		}
		else
		{
			if(pcchLength)
				*pcchLength = 0;

			*pptzNew = NULL;
			hr = S_OK;
		}

		return hr;
	}

	template <typename T, typename THeap>
	static inline HRESULT TReplaceString (THeap& heap, const T* pctzString, T** pptzReplace, INT* pcchLength = NULL)
	{
		HRESULT hr;

		if(TTraits::TValidator::ValidateParam(pctzString))
		{
			T* ptzDup;
			hr = TStringCore<SensitiveAssert>::TDuplicateString(heap, pctzString, &ptzDup, pcchLength);
			if(SUCCEEDED(hr))
			{
				heap.release_storage(*pptzReplace);
				*pptzReplace = ptzDup;
			}
		}
		else
		{
			if(pcchLength)
				*pcchLength = 0;

			if(*pptzReplace)
			{
				heap.release_storage(*pptzReplace);
				*pptzReplace = NULL;
				hr = S_OK;
			}
			else
				hr = S_FALSE;
		}

		return hr;
	}

	template <typename T>
	static inline const T* TStrStr (const T* pctzText, const T* pctzFrag)
	{
		if(TTraits::TValidator::ValidateParam(pctzText) &&
			TTraits::TValidator::ValidateParam(pctzFrag))
		{
			T chT, chF, chFirst = TTraits::TReadChar::ReadCharacter(*pctzFrag);
			do
			{
				chT = TTraits::TReadChar::ReadCharacter(*pctzText);
				if(chT == chFirst)
				{
					INT iText = 1;
					INT iFrag = 1;
					for(;;)
					{
						chF = TTraits::TReadChar::ReadCharacter(pctzFrag[iFrag++]);
						if(chF == 0)
							return pctzText;
						chT = TTraits::TReadChar::ReadCharacter(pctzText[iText++]);
						if(chT != chF)
							break;
					}
				}
				pctzText++;
			} while(chT != 0);
		}
		return NULL;
	}

	template <typename T>
	static inline const T* TStrCchStr (const T* pctzText, INT cchText, const T* pctzFrag)
	{
		if(TTraits::TValidator::ValidateParam(pctzText) &&
			TTraits::TValidator::ValidateParam(pctzFrag))
		{
			T chFirst = TTraits::TReadChar::ReadCharacter(*pctzFrag);
			do
			{
				T chT = TTraits::TReadChar::ReadCharacter(*pctzText);
				if(chT == chFirst)
				{
					INT iFrag = 1;

					for(INT i = 1; i < cchText; i++)
					{
						T chF = TTraits::TReadChar::ReadCharacter(pctzFrag[iFrag++]);
						chT = TTraits::TReadChar::ReadCharacter(pctzText[i]);
						if(chT != chF)
							goto FindNext;
					}

					return pctzText;
				}
	FindNext:
				pctzText++;
			} while(0 < --cchText);
		}
		return NULL;
	}

	template <typename T>
	static inline T* TStrLwr (T* ptzString)
	{
		if(TTraits::TValidator::ValidateParam(ptzString))
		{
			for(T* cp = ptzString; *cp; ++cp)
			{
				if('A' <= *cp && *cp <= 'Z')
					*cp += 32;
			}
		}
		return ptzString;
	}

	template <typename T>
	static inline T* TStrUpr (T* ptzString)
	{
		if(TTraits::TValidator::ValidateParam(ptzString))
		{
			for(T* cp = ptzString; *cp; ++cp)
			{
				if('a' <= *cp && *cp <= 'z')
					*cp -= 32;
			}
		}
		return ptzString;
	}

	template <typename T>
	static inline HRESULT TStrCchLwr (const T* pctzString, __out_ecount(cchMaxDest) T* ptzDest, INT cchMaxDest)
	{
		HRESULT hr;

		if(TTraits::TValidator::ValidateParam(pctzString))
		{
			hr = STRSAFE_E_INSUFFICIENT_BUFFER;

			for(INT i = 0; i < cchMaxDest; i++)
			{
				T tch = pctzString[i];
				if('A' <= tch && tch <= 'Z')
					tch += 32;
				ptzDest[i] = tch;
				if('\0' == tch)
				{
					hr = S_OK;
					break;
				}
			}
		}
		else
			hr = E_INVALIDARG;

		return hr;
	}

	template <typename T>
	static inline HRESULT TStrCchUpr (const T* pctzString, __out_ecount(cchMaxDest) T* ptzDest, INT cchMaxDest)
	{
		HRESULT hr;

		if(TTraits::TValidator::ValidateParam(pctzString))
		{
			hr = STRSAFE_E_INSUFFICIENT_BUFFER;

			for(INT i = 0; i < cchMaxDest; i++)
			{
				T tch = pctzString[i];
				if('a' <= tch && tch <= 'z')
					tch -= 32;
				ptzDest[i] = tch;
				if('\0' == tch)
				{
					hr = S_OK;
					break;
				}
			}
		}
		else
			hr = E_INVALIDARG;

		return hr;
	}

	template <typename T>
	static inline const T* TStrCchStrR (const T* pctzText, INT cchText, const T* pctzFrag, INT cchFrag)
	{
		const T* pctzPtr = (pctzText + cchText) - cchFrag;
		while(pctzPtr >= pctzText)
		{
			bool fMatch = true;
			for(INT i = 0; i < cchFrag; i++)
			{
				if(TTraits::TReadChar::ReadCharacter(pctzPtr[i]) != TTraits::TReadChar::ReadCharacter(pctzFrag[i]))
				{
					fMatch = false;
					break;
				}
			}
			if(fMatch)
				return pctzPtr;
			pctzPtr--;
		}
		return NULL;
	}
};

///////////////////////////////////////////////////////////////////////////////
// Template instantiations of TStringCore (or just templatized string functions)
///////////////////////////////////////////////////////////////////////////////

template <typename T>
INT TStrLenAssert (const T* pctzString)
{
	return TStringCore<SensitiveAssert>::TStrLen(pctzString);
}

template <typename T>
INT TStrLenChecked (__in_opt const T* pctzString)
{
	return TStringCore<SensitiveChecked>::TStrLen(pctzString);
}

template <typename T>
HRESULT TSafeStrLenAssert (const T* pctzString, INT cchMax, __out INT* pcch)
{
	return TStringCore<SensitiveAssert>::TSafeStrLen(pctzString, cchMax, pcch);
}

template <typename T>
HRESULT TSafeStrLenChecked (const T* pctzString, INT cchMax, __out INT* pcch)
{
	return TStringCore<SensitiveChecked>::TSafeStrLen(pctzString, cchMax, pcch);
}

template <typename T>
INT TStrCchLenAssert (__in_ecount(cchMax) const T* pctzString, INT cchMax)
{
	return TStringCore<SensitiveAssert>::TStrCchLen(pctzString, cchMax);
}

template <typename T>
INT TStrCchLenChecked (__in_ecount(cchMax) const T* pctzString, INT cchMax)
{
	return TStringCore<SensitiveChecked>::TStrCchLen(pctzString, cchMax);
}

template <typename T>
HRESULT TStrTerminateAssert (__out_ecount(cchString) T* ptzString, INT cchString)
{
	return TStringCore<SensitiveAssert>::TStrTerminate(ptzString, cchString);
}

template <typename T>
const T* TStrChr (const T* pctzString, T tchFind)
{
	Assert(NULL != pctzString);

	while(*pctzString != tchFind)
	{
		if(0 == *pctzString)
			return NULL;
		pctzString++;
	}
	return pctzString;
}

template <typename T>
const T* TStrCchChr (const T* pctzString, INT cchString, T tchFind)
{
	Assert(NULL != pctzString || 0 == cchString);

	for(INT i = 0; i < cchString; i++)
	{
		if(pctzString[i] == tchFind)
			return pctzString + i;
	}
	return NULL;
}

template <typename T>
const T* TStrRChr (const T* pctzString, T tchFind)
{
	Assert(NULL != pctzString);

	const T* pctzLast = NULL;
	while(*pctzString)
	{
		if(*pctzString == tchFind)
			pctzLast = pctzString;
		pctzString++;
	}
	return pctzLast;
}

template <typename T>
const T* TStrCchRChr (const T* pctzString, INT cchString, T tchFind)
{
	Assert(NULL != pctzString || 0 == cchString);

	for(INT i = cchString - 1; i >= 0; i--)
	{
		if(pctzString[i] == tchFind)
			return pctzString + i;
	}
	return NULL;
}

template <typename T>
HRESULT TStrCchCpy (T* ptzDest, INT cchMaxDest, const T* pctzSrc)
{
	return TStringCore<SensitiveAssert>::TStrCchCpy(ptzDest, cchMaxDest, pctzSrc);
}

template <typename T>
HRESULT TStrCchCpyEx (T* ptzDest, INT cchMaxDest, const T* pctzSrc, __out_opt T** pptzEnd, __out_opt INT* pcchRemaining)
{
	return TStringCore<SensitiveAssert>::TStrCchCpyEx(ptzDest, cchMaxDest, pctzSrc, pptzEnd, pcchRemaining);
}

template <typename T>
HRESULT TStrCchCpyLen (T* ptzDest, INT cchMaxDest, const T* pctzSrc, INT cchSrc, __out_opt INT* pcchCopied)
{
	HRESULT hr;
	if(cchMaxDest > cchSrc)
	{
		CopyMemory(ptzDest, pctzSrc, cchSrc * sizeof(T));
		ptzDest[cchSrc] = '\0';
		if(pcchCopied)
			*pcchCopied = cchSrc;
		hr = S_OK;
	}
	else
		hr = STRSAFE_E_INSUFFICIENT_BUFFER;
	return hr;
}

template <typename T>
HRESULT TStrCchCpyNEx (T* ptzDest, INT cchMaxDest, const T* pctzSrc, INT cchSrc, __out_opt T** pptzEnd, __out_opt INT* pcchRemaining)
{
	return TStringCore<SensitiveAssert>::TStrCchCpyNEx(ptzDest, cchMaxDest, pctzSrc, cchSrc, pptzEnd, pcchRemaining);
}

template <typename T>
HRESULT TStrCchCpyN (T* ptzDest, INT cchMaxDest, const T* pctzSrc, INT cchSrc)
{
	return TStringCore<SensitiveAssert>::TStrCchCpyN(ptzDest, cchMaxDest, pctzSrc, cchSrc);
}

template <typename T>
HRESULT TStrCatAssert (T* ptzDest, INT cchMaxDest, const T* pctzSrc)
{
	return TStringCore<SensitiveAssert>::TStrCat(ptzDest, cchMaxDest, pctzSrc);
}

template <typename T>
INT TStrCmpAssert (const T* pctzStringA, const T* pctzStringB)
{
	return TStringCore<SensitiveAssert>::TStrCmp(pctzStringA, pctzStringB);
}

template <typename T>
INT TStrCmpChecked (const T* pctzStringA, const T* pctzStringB)
{
	return TStringCore<SensitiveChecked>::TStrCmp(pctzStringA, pctzStringB);
}

template <typename T>
INT TStrCmpIAssert (const T* pctzStringA, const T* pctzStringB)
{
	return TStringCore<InsensitiveAssert>::TStrCmp(pctzStringA, pctzStringB);
}

template <typename T>
INT TStrCmpIChecked (const T* pctzStringA, const T* pctzStringB)
{
	return TStringCore<InsensitiveChecked>::TStrCmp(pctzStringA, pctzStringB);
}

template <typename T>
INT TStrCchCmpAssert (__in_ecount(cchStringA) const T* pctzStringA, INT cchStringA, const T* pctzStringB)
{
	return TStringCore<SensitiveAssert>::TStrCchCmp(pctzStringA, cchStringA, pctzStringB);
}

template <typename T>
INT TStrCchCmpChecked (__in_ecount(cchStringA) const T* pctzStringA, INT cchStringA, const T* pctzStringB)
{
	return TStringCore<SensitiveChecked>::TStrCchCmp(pctzStringA, cchStringA, pctzStringB);
}

template <typename T>
INT TStrCchCmpIAssert (__in_ecount(cchStringA) const T* pctzStringA, INT cchStringA, const T* pctzStringB)
{
	return TStringCore<InsensitiveAssert>::TStrCchCmp(pctzStringA, cchStringA, pctzStringB);
}

template <typename T>
INT TStrCchCmpIChecked (__in_ecount(cchStringA) const T* pctzStringA, INT cchStringA, const T* pctzStringB)
{
	return TStringCore<InsensitiveChecked>::TStrCchCmp(pctzStringA, cchStringA, pctzStringB);
}

template <typename T>
INT TStrCmpNAssert (const T* pctzStringA, const T* pctzStringB, INT cchCompare)
{
	return TStringCore<SensitiveAssert>::TStrCmpN(pctzStringA, pctzStringB, cchCompare);
}

template <typename T>
INT TStrICmpNAssert (const T* pctzStringA, const T* pctzStringB, INT cchCompare)
{
	return TStringCore<InsensitiveAssert>::TStrCmpN(pctzStringA, pctzStringB, cchCompare);
}

template <typename T>
INT TStrCmpNChecked (const T* pctzStringA, const T* pctzStringB, INT cchCompare)
{
	return TStringCore<SensitiveChecked>::TStrCmpN(pctzStringA, pctzStringB, cchCompare);
}

template <typename T>
INT TStrICmpNChecked (const T* pctzStringA, const T* pctzStringB, INT cchCompare)
{
	return TStringCore<InsensitiveChecked>::TStrCmpN(pctzStringA, pctzStringB, cchCompare);
}

template <typename T>
BOOL TStrMatchLeftAssert (const T* pctzStringA, __in_ecount(cchCompare) const T* pctzStringB, INT cchCompare)
{
	return TStringCore<SensitiveAssert>::TStrMatchLeft(pctzStringA, pctzStringB, cchCompare);
}

template <typename T>
BOOL TStrMatchLeftIAssert (const T* pctzStringA, __in_ecount(cchCompare) const T* pctzStringB, INT cchCompare)
{
	return TStringCore<InsensitiveAssert>::TStrMatchLeft(pctzStringA, pctzStringB, cchCompare);
}

template <typename T>
BOOL TStrMatchLeftChecked (const T* pctzStringA, __in_ecount(cchCompare) const T* pctzStringB, INT cchCompare)
{
	return TStringCore<SensitiveChecked>::TStrMatchLeft(pctzStringA, pctzStringB, cchCompare);
}

template <typename T>
BOOL TStrMatchLeftIChecked (const T* pctzStringA, __in_ecount(cchCompare) const T* pctzStringB, INT cchCompare)
{
	return TStringCore<InsensitiveChecked>::TStrMatchLeft(pctzStringA, pctzStringB, cchCompare);
}

template <typename T>
BOOL TCompareLeftAssert (const T* pctzString, const T* pctzFrag)
{
	return TStringCore<SensitiveAssert>::TCompareLeft(pctzString, pctzFrag);
}

template <typename T>
BOOL TCompareLeftIAssert (const T* pctzString, const T* pctzFrag)
{
	return TStringCore<InsensitiveAssert>::TCompareLeft(pctzString, pctzFrag);
}

template <typename T>
BOOL TCompareLeftChecked (const T* pctzString, const T* pctzFrag)
{
	return TStringCore<SensitiveChecked>::TCompareLeft(pctzString, pctzFrag);
}

template <typename T>
BOOL TCompareLeftIChecked (const T* pctzString, const T* pctzFrag)
{
	return TStringCore<InsensitiveChecked>::TCompareLeft(pctzString, pctzFrag);
}

template <typename T>
const T* TStrStr (const T* pctzText, const T* pctzFrag)
{
	return TStringCore<SensitiveChecked>::TStrStr(pctzText, pctzFrag);
}

template <typename T>
const T* TStrIStr (const T* pctzText, const T* pctzFrag)
{
	return TStringCore<InsensitiveChecked>::TStrStr(pctzText, pctzFrag);
}

template <typename T>
const T* TStrCchStr (const T* pctzText, INT cchText, const T* pctzFrag)
{
	return TStringCore<SensitiveChecked>::TStrCchStr(pctzText, cchText, pctzFrag);
}

template <typename T>
const T* TStrCchIStr (const T* pctzText, INT cchText, const T* pctzFrag)
{
	return TStringCore<InsensitiveChecked>::TStrCchStr(pctzText, cchText, pctzFrag);
}

template <typename T>
const T* TStrStrR (const T* pctzText, const T* pctzFrag)
{
	const T* pctzLast = NULL, *pctzFind;
	for(;;)
	{
		pctzFind = TStrStr(pctzText, pctzFrag);
		if(pctzFind == NULL)
			break;
		pctzLast = pctzFind;
		pctzText = pctzLast + 1;
	}
	return pctzLast;
}

template <typename T>
const T* TStrIStrR (const T* pctzText, const T* pctzFrag)
{
	const T* pctzLast = NULL, *pctzFind;
	for(;;)
	{
		pctzFind = TStrIStr(pctzText, pctzFrag);
		if(pctzFind == NULL)
			break;
		pctzLast = pctzFind;
		pctzText = pctzLast + 1;
	}
	return pctzLast;
}

template <typename T>
const T* TStrCchStrR (const T* pctzText, INT cchText, const T* pctzFrag, INT cchFrag)
{
	return TStringCore<SensitiveAssert>::TStrCchStrR(pctzText, cchText, pctzFrag, cchFrag);
}

template <typename T>
const T* TStrCchIStrR (const T* pctzText, INT cchText, const T* pctzFrag, INT cchFrag)
{
	return TStringCore<InsensitiveAssert>::TStrCchStrR(pctzText, cchText, pctzFrag, cchFrag);
}

template <typename T>
HRESULT TDuplicateStringAssert (const T* pctzString, __deref_out T** pptzNew, __out_opt INT* pcchLength = NULL)
{
	crt_heap heap;
	return TStringCore<SensitiveAssert>::TDuplicateString(heap, pctzString, pptzNew, pcchLength);
}

template <typename T, typename THeap>
HRESULT TDuplicateStringAssert (THeap& heap, const T* pctzString, __deref_out T** pptzNew, __out_opt INT* pcchLength = NULL)
{
	return TStringCore<SensitiveAssert>::TDuplicateString(heap, pctzString, pptzNew, pcchLength);
}

template <typename T>
HRESULT TDuplicateStringChecked (const T* pctzString, __deref_out T** pptzNew, __out_opt INT* pcchLength = NULL)
{
	crt_heap heap;
	return TStringCore<SensitiveChecked>::TDuplicateString(heap, pctzString, pptzNew, pcchLength);
}

template <typename T, typename THeap>
HRESULT TDuplicateStringChecked (THeap& heap, const T* pctzString, __deref_out T** pptzNew, __out_opt INT* pcchLength = NULL)
{
	return TStringCore<SensitiveChecked>::TDuplicateString(heap, pctzString, pptzNew, pcchLength);
}

template <typename T>
HRESULT TReplaceStringAssert (const T* pctzString, T** pptzReplace, INT* pcchLength = NULL)
{
	crt_heap heap;
	return TStringCore<SensitiveAssert>::TReplaceString(heap, pctzString, pptzReplace, pcchLength);
}

template <typename T, typename THeap>
HRESULT TReplaceStringAssert (THeap& heap, const T* pctzString, T** pptzReplace, INT* pcchLength = NULL)
{
	return TStringCore<SensitiveAssert>::TReplaceString(heap, pctzString, pptzReplace, pcchLength);
}

template <typename T>
HRESULT TReplaceStringChecked (const T* pctzString, T** pptzReplace, INT* pcchLength = NULL)
{
	crt_heap heap;
	return TStringCore<SensitiveChecked>::TReplaceString(heap, pctzString, pptzReplace, pcchLength);
}

template <typename T, typename THeap>
HRESULT TReplaceStringChecked (THeap& heap, const T* pctzString, T** pptzReplace, INT* pcchLength = NULL)
{
	return TStringCore<SensitiveChecked>::TReplaceString(heap, pctzString, pptzReplace, pcchLength);
}

template <typename T>
HRESULT TDuplicateCchString (const T* pctzString, INT cchString, __deref_out T** pptzNew)
{
	HRESULT hr = S_OK;
	*pptzNew = __new T[cchString + 1];
	if(*pptzNew)
	{
		CopyMemory(*pptzNew, pctzString, cchString * sizeof(T));
		(*pptzNew)[cchString] = L'\0';
	}
	else
		hr = E_OUTOFMEMORY;
	return hr;
}

template <typename T>
T* TStrLwrAssert (T* ptzString)
{
	return TStringCore<SensitiveAssert>::TStrLwr(ptzString);
}

template <typename T>
T* TStrUprAssert (T* ptzString)
{
	return TStringCore<SensitiveAssert>::TStrLwr(ptzString);
}

template <typename T>
HRESULT TStrCchLwrAssert (const T* pctzString, __out_ecount(cchMaxDest) T* ptzDest, INT cchMaxDest)
{
	return TStringCore<SensitiveAssert>::TStrCchLwr(pctzString, ptzDest, cchMaxDest);
}

template <typename T>
HRESULT TStrCchUprAssert (const T* pctzString, __out_ecount(cchMaxDest) T* ptzDest, INT cchMaxDest)
{
	return TStringCore<SensitiveAssert>::TStrCchUpr(pctzString, ptzDest, cchMaxDest);
}

template <typename T>
const T* TFindEnd (const T* pctzText, const T* pctzList)
{
	const T* pctzFind = NULL;
	T ch;

	if(pctzList[0] != 0 && pctzList[1] != 0 && pctzList[2] == 0)
	{
		// The optimized case for two list entries is common with E-Mail header parsing.
		T ch1 = pctzList[0];
		T ch2 = pctzList[1];

		ch = *pctzText;
		while(ch)
		{
			if(ch == ch1 || ch == ch2)
			{
				pctzFind = pctzText;
				break;
			}
			ch = *(++pctzText);
		}
	}
	else
	{
		const T* pctzPtr;

		ch = *pctzList;
		while(ch)
		{
			pctzPtr = TStrChr(pctzText, ch);
			if(pctzPtr)
			{
				if(pctzFind)
				{
					if(pctzPtr < pctzFind)
						pctzFind = pctzPtr;
				}
				else
					pctzFind = pctzPtr;
			}
			ch = *(++pctzList);
		}
	}
	return pctzFind;
}
