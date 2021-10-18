#pragma once

#include <limits.h>
#include "SDKExtras.h"
#include "Assert.h"

#ifndef	INTSAFE_E_ARITHMETIC_OVERFLOW
	#define	INTSAFE_E_ARITHMETIC_OVERFLOW	((HRESULT)0x80070216L)  // 0x216 = 534 = ERROR_ARITHMETIC_OVERFLOW
#endif

//
// Int32x32To64 macro
//
#ifndef	Int32x32To64
	#define Int32x32To64(a, b)  ((__int64)(((__int64)((long)(a))) * ((long)(b))))
#endif

//
// UInt32x32To64 macro
//
#ifndef	UInt32x32To64
	#if defined(MIDL_PASS) || defined(RC_INVOKED) || defined(_M_CEE_PURE) || defined(_68K_) || defined(_MPPC_) || defined(_M_IA64) || defined(_M_AMD64)
		#define	UInt32x32To64(a, b) (((unsigned __int64)((unsigned int)(a))) * ((unsigned __int64)((unsigned int)(b))))
	#elif defined(_M_IX86)
		#define	UInt32x32To64(a, b) ((unsigned __int64)(((unsigned __int64)((unsigned int)(a))) * ((unsigned int)(b))))
	#else
		#error Must define a target architecture.
	#endif
#endif

template <typename T>
inline HRESULT HrSafeAdd (T tAugend, T tAddend, __out __deref_out_range(==, tAugend + tAddend) T* ptResult)
{
	HRESULT hr;

	if((tAugend + tAddend) >= tAugend)
	{
		*ptResult = (tAugend + tAddend);
		hr = S_OK;
	}
	else
		hr = INTSAFE_E_ARITHMETIC_OVERFLOW;

	return hr;
}

inline HRESULT HrULongLongToULong (ULONGLONG ullOperand, __out __deref_out_range(==, ullOperand) ULONG* pulResult)
{
	HRESULT hr;

	if(ullOperand <= ULONG_MAX)
	{
		*pulResult = (ULONG)ullOperand;
		hr = S_OK;
	}
	else
		hr = INTSAFE_E_ARITHMETIC_OVERFLOW;

	return hr;
}

inline HRESULT HrLongLongToInt (LONGLONG llOperand, __out __deref_out_range(==, llOperand) INT* plResult)
{
	HRESULT hr;

	if(INT_MIN <= llOperand && llOperand <= INT_MAX)
	{
		*plResult = (INT)llOperand;
		hr = S_OK;
	}
	else
		hr = INTSAFE_E_ARITHMETIC_OVERFLOW;

	return hr;
}

template <typename T>
inline HRESULT HrSafeMult (T tMultiplicand, T tMultiplier, __out __deref_out_range(==, tMultiplier * tMultiplicand) T* ptResult)
{
	Assert(false);	// This template must not be used.  Use one of the specializations.
	return E_NOTIMPL;
}

template <>
inline HRESULT HrSafeMult<ULONG> (ULONG tMultiplicand, ULONG tMultiplier, __out __deref_out_range(==, tMultiplier * tMultiplicand) ULONG* ptResult)
{
	ULONGLONG ull64Result = UInt32x32To64(tMultiplicand, tMultiplier);

	return HrULongLongToULong(ull64Result, ptResult);
}

template <>
inline HRESULT HrSafeMult<INT> (INT tMultiplicand, INT tMultiplier, __out __deref_out_range(==, tMultiplier * tMultiplicand) INT* ptResult)
{
	LONGLONG ll64Result = Int32x32To64(tMultiplicand, tMultiplier);

	return HrLongLongToInt(ll64Result, ptResult);
}

template <>
inline HRESULT HrSafeMult<unsigned __int64> (unsigned __int64 tMultiplicand, unsigned __int64 tMultiplier, __out __deref_out_range(==, tMultiplier * tMultiplicand) unsigned __int64* ptResult)
{
	unsigned __int32 aHigh, aLow, bHigh, bLow;

	aHigh = (unsigned __int32)(tMultiplicand >> 32);
	aLow  = (unsigned __int32)tMultiplicand;
	bHigh = (unsigned __int32)(tMultiplier >> 32);
	bLow  = (unsigned __int32)tMultiplier;

	*ptResult = 0;

	if(aHigh == 0)
	{
		if(bHigh != 0)
		{
			*ptResult = (unsigned __int64)aLow * (unsigned __int64)bHigh;
		}
	}
	else if(bHigh == 0)
	{
		if(aHigh != 0)
		{        
			*ptResult = (unsigned __int64)aHigh * (unsigned __int64)bLow;
		}
	}
	else
	{
		return INTSAFE_E_ARITHMETIC_OVERFLOW;
	}

	if(*ptResult != 0)
	{
		unsigned __int64 tmp;

		if((unsigned __int32)(*ptResult >> 32) != 0)
			return INTSAFE_E_ARITHMETIC_OVERFLOW;

		*ptResult <<= 32;
		tmp = (unsigned __int64)aLow * (unsigned __int64)bLow;
		*ptResult += tmp;

		if(*ptResult < tmp)
			return INTSAFE_E_ARITHMETIC_OVERFLOW;
	}
	else
	{
		*ptResult = (unsigned __int64)aLow * (unsigned __int64)bLow;
	}

	return S_OK;
}

template <>
inline HRESULT HrSafeMult<__int64> (__int64 tMultiplicand, __int64 tMultiplier, __out __deref_out_range(==, tMultiplier * tMultiplicand) __int64* ptResult)
{
	HRESULT hr;
	bool aNegative = false;
	bool bNegative = false;

	unsigned __int64 tmp;
	__int64 a1 = tMultiplicand;
	__int64 b1 = tMultiplier;

	if( a1 < 0 )
	{
		aNegative = true;
		a1 = -a1;
	}

	if( b1 < 0 )
	{
		bNegative = true;
		b1 = -b1;
	}

	hr = HrSafeMult<unsigned __int64>( (unsigned __int64)a1, (unsigned __int64)b1, &tmp);
	if(SUCCEEDED(hr))
	{
		// The unsigned multiplication didn't overflow
		if( aNegative ^ bNegative )
		{
			// Result must be negative
			if( tmp <= (unsigned __int64)LLONG_MIN)
				*ptResult = -(signed __int64)tmp;
			else
				hr = INTSAFE_E_ARITHMETIC_OVERFLOW;
		}
		else
		{
			// Result must be positive
			if( tmp <= (unsigned __int64)LLONG_MAX)
				*ptResult = (signed __int64)tmp;
			else
				hr = INTSAFE_E_ARITHMETIC_OVERFLOW;
		}
	}

	return hr;
}

#ifdef	_WIN64

#define	HrSafeMultSysInt(multiplicand, multiplier, result)	HrSafeMult(multiplicand, multiplier, result)

#else

inline HRESULT HrSafeMultSysInt (sysint tMultiplicand, sysint tMultiplier, __out __deref_out_range(==, tMultiplier * tMultiplicand) sysint* ptResult)
{
	return HrSafeMult<INT>(static_cast<INT>(tMultiplicand), static_cast<INT>(tMultiplier), reinterpret_cast<INT*>(ptResult));
}

#endif

// More variations of HrSafeMult() can be added as needed.
