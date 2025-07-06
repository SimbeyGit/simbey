#pragma once

#include "..\Core\StringCore.h"

#define RSTRING_F_MANAGED		0x1
#define RSTRING_F_WIDE			0x2
#define RSTRING_F_MASK			0x3

typedef const BYTE* RSTRING;

#define RStrIsStatic(r)			(0 == (reinterpret_cast<sysint>(r) & RSTRING_F_MANAGED))
#define RStrIsManaged(r)		(0 != (reinterpret_cast<sysint>(r) & RSTRING_F_MANAGED))

#define RStrIsAnsi(r)			(0 == (reinterpret_cast<sysint>(r) & RSTRING_F_WIDE))
#define RStrIsWide(r)			(0 != (reinterpret_cast<sysint>(r) & RSTRING_F_WIDE))

#define	RStrToVoid(r)			reinterpret_cast<const VOID*>(reinterpret_cast<sysint>(r) & ~RSTRING_F_MASK)
#define RStrToAnsi(r)			reinterpret_cast<PCSTR>(reinterpret_cast<sysint>(r) & ~RSTRING_F_MASK)
#define RStrToWide(r)			reinterpret_cast<PCWSTR>(reinterpret_cast<sysint>(r) & ~RSTRING_F_MASK)
#define RStrExtractLength(r)	reinterpret_cast<const INT*>(RStrToVoid(r))[-1]

template <typename T>
static inline bool TRStrIsType (RSTRING rstrValue)
{
	return false;
}

template <>
static inline bool TRStrIsType<CHAR> (RSTRING rstrValue)
{
	return RStrIsAnsi(rstrValue);
}

template <>
static inline bool TRStrIsType<WCHAR> (RSTRING rstrValue)
{
	return RStrIsWide(rstrValue);
}

template <typename T>
inline const T* TRStrToType (RSTRING rstrText)
{
	return NULL;
}

template <>
inline PCSTR TRStrToType<CHAR> (RSTRING rstrText)
{
	return RStrToAnsi(rstrText);
}

template <>
inline PCWSTR TRStrToType<WCHAR> (RSTRING rstrText)
{
	return RStrToWide(rstrText);
}

inline RSTRING RSTRING_CAST (PCWSTR pcwzText)
{
	return reinterpret_cast<RSTRING>(reinterpret_cast<sysint>(pcwzText) | RSTRING_F_WIDE);
}

inline INT RStrLenInl (RSTRING rstrValue)
{
	if(RStrIsManaged(rstrValue))
		return RStrExtractLength(rstrValue);
	else if(RStrIsWide(rstrValue))
		return TStrLenChecked(RStrToWide(rstrValue));
	else
		return TStrLenChecked(rstrValue);
}

inline LONG RStrAddRef (RSTRING rstrValue)
{
	if(RStrIsManaged(rstrValue))
	{
		const INT* pcRaw = reinterpret_cast<const INT*>(RStrToVoid(rstrValue)) - 2;
		return InterlockedIncrement(reinterpret_cast<LONG*>(const_cast<INT*>(pcRaw)));
	}
	return -1;
}

inline VOID RStrSet (RSTRING& rstrDest, RSTRING rstrSrc)
{
	rstrDest = rstrSrc;
	RStrAddRef(rstrDest);
}

INT WINAPI RStrLen (RSTRING rstrValue);
LONG WINAPI RStrRelease (RSTRING rstrValue);

HRESULT WINAPI RStrAllocA (INT cchElements, __deref_out RSTRING* prstrValue, __deref_out_ecount(cchElements) PSTR* ppszWritePtr);
HRESULT WINAPI RStrAllocW (INT cchElements, __deref_out RSTRING* prstrValue, __deref_out_ecount(cchElements) PWSTR* ppwzWritePtr);
HRESULT WINAPI RStrCreateA (INT cchString, __in_ecount(cchString) PCSTR pcszString, __deref_out RSTRING* prstrValue);
HRESULT WINAPI RStrCreateW (INT cchString, __in_ecount(cchString) PCWSTR pcwzString, __deref_out RSTRING* prstrValue);
HRESULT WINAPI RStrFromStaticA (__in_opt PCSTR pcszString, __deref_out RSTRING* prstrValue);
HRESULT WINAPI RStrFromStaticW (__in_opt PCWSTR pcwzString, __deref_out RSTRING* prstrValue);

template <typename T>
static inline HRESULT TRStrCreate (const T* pctzValue, INT cchValue, __deref_out RSTRING* prstrValue)
{
	return E_NOTIMPL;
}

template <>
static inline HRESULT TRStrCreate<CHAR> (PCSTR pctzValue, INT cchValue, __deref_out RSTRING* prstrValue)
{
	return RStrCreateA(cchValue, pctzValue, prstrValue);
}

template <>
static inline HRESULT TRStrCreate<WCHAR> (PCWSTR pctzValue, INT cchValue, __deref_out RSTRING* prstrValue)
{
	return RStrCreateW(cchValue, pctzValue, prstrValue);
}

HRESULT WINAPI RStrFormatVA (__deref_out RSTRING* prstrValue, PCSTR pcszFormat, va_list vArgs);
HRESULT WINAPI RStrFormatVW (__deref_out RSTRING* prstrValue, PCWSTR pcwzFormat, va_list vArgs);

HRESULT RStrFormatA (__deref_out RSTRING* prstrValue, PCSTR pcszFormat, ...);
HRESULT RStrFormatW (__deref_out RSTRING* prstrValue, PCWSTR pcwzFormat, ...);

HRESULT WINAPI RStrAppendA (RSTRING rstrBase, INT cchString, __in_ecount(cchString) PCSTR pcszString, __deref_out RSTRING* prstrValue);
HRESULT WINAPI RStrAppendW (RSTRING rstrBase, INT cchString, __in_ecount(cchString) PCWSTR pcwzString, __deref_out RSTRING* prstrValue);
HRESULT WINAPI RStrAppendRStr (RSTRING rstrBase, RSTRING rstrValue, __deref_out RSTRING* prstrValue);

HRESULT WINAPI RStrCompareA (RSTRING rstrBase, PCSTR pcszString, __out INT* pnResult);
HRESULT WINAPI RStrCompareW (RSTRING rstrBase, PCWSTR pcwzString, __out INT* pnResult);
HRESULT WINAPI RStrCompareRStr (RSTRING rstrLeft, RSTRING rstrRight, __out INT* pnResult);

HRESULT WINAPI RStrCompareIA (RSTRING rstrBase, PCSTR pcszString, __out INT* pnResult);
HRESULT WINAPI RStrCompareIW (RSTRING rstrBase, PCWSTR pcwzString, __out INT* pnResult);
HRESULT WINAPI RStrCompareIRStr (RSTRING rstrLeft, RSTRING rstrRight, __out INT* pnResult);

HRESULT WINAPI RStrSubString (RSTRING rstrValue, INT cchStart, INT cchRun, __deref_out RSTRING* prstrValue);
HRESULT WINAPI RStrTrim (RSTRING rstrValue, __deref_out RSTRING* prstrValue);

HRESULT WINAPI RStrCopyToA (RSTRING rstrValue, INT cchMaxDest, __out_ecount(cchMaxDest) PSTR pszDest, __out_opt INT* pcchDest);
HRESULT WINAPI RStrCopyToW (RSTRING rstrValue, INT cchMaxDest, __out_ecount(cchMaxDest) PWSTR pwzDest, __out_opt INT* pcchDest);

HRESULT WINAPI RStrReplaceA (INT cchString, PCSTR pcszString, __deref_inout RSTRING* prstrValue);
HRESULT WINAPI RStrReplaceW (INT cchString, PCWSTR pcwzString, __deref_inout RSTRING* prstrValue);
VOID WINAPI RStrReplace (__inout RSTRING* prstrDest, RSTRING rstrSrc);

HRESULT WINAPI RStrToBStr (RSTRING rstrValue, __out BSTR* pbstrValue);
HRESULT WINAPI RStrCchToBStr (RSTRING rstrValue, INT cchValue, __out BSTR* pbstrValue);

HRESULT WINAPI RStrConvertToA (INT cchString, PCWSTR pcwzString, __deref_out RSTRING* prstrValueA);
HRESULT WINAPI RStrConvertToW (UINT nCodePage, INT cchString, PCSTR pcszString, __deref_out RSTRING* prstrValueW);

HRESULT RStrFormatMessageA (__deref_out RSTRING* prstrValue, INT cchFormat, __in_ecount(cchFormat) PCSTR pcszFormat, ...);
HRESULT RStrFormatMessageW (__deref_out RSTRING* prstrValue, INT cchFormat, __in_ecount(cchFormat) PCWSTR pcwzFormat, ...);

HRESULT WINAPI RStrReplaceSubStringA (RSTRING rstrSource, PCSTR pcszFind, INT cchFind, PCSTR pcszReplace, INT cchReplace, __deref_out RSTRING* prstrResult);
HRESULT WINAPI RStrReplaceSubStringW (RSTRING rstrSource, PCWSTR pcwzFind, INT cchFind, PCWSTR pcwzReplace, INT cchReplace, __deref_out RSTRING* prstrResult);
HRESULT WINAPI RStrReplaceSubStringIA (RSTRING rstrSource, PCSTR pcszFind, INT cchFind, PCSTR pcszReplace, INT cchReplace, __deref_out RSTRING* prstrResult);
HRESULT WINAPI RStrReplaceSubStringIW (RSTRING rstrSource, PCWSTR pcwzFind, INT cchFind, PCWSTR pcwzReplace, INT cchReplace, __deref_out RSTRING* prstrResult);

HRESULT WINAPI RStrFromGUID (REFGUID rguid, __deref_out RSTRING* prstrGUIDW);

class CRString
{
private:
	RSTRING m_rstrValue;

public:
	CRString () :
		m_rstrValue(NULL)
	{
	}

	CRString (RSTRING rstrValue)
	{
		RStrSet(m_rstrValue, rstrValue);
	}

	CRString (const CRString& rsOther)
	{
		RStrSet(m_rstrValue, rsOther.m_rstrValue);
	}

	~CRString ()
	{
		RStrRelease(m_rstrValue);
	}

#if _MSC_VER >= 1700  // Visual Studio 2012 or newer supports move semantics
    CRString (CRString&& other) noexcept
    {
        m_rstrValue = other.m_rstrValue;
        other.m_rstrValue = NULL;
    }

    CRString& operator= (CRString&& other) noexcept
    {
        if(this != &other)
        {
            RStrRelease(m_rstrValue);
            m_rstrValue = other.m_rstrValue;
            other.m_rstrValue = NULL;
        }
        return *this;
    }
#endif

	CRString& operator= (const CRString& rsOther)
	{
		RStrReplace(&m_rstrValue, rsOther.m_rstrValue);
		return *this;
	}

	operator RSTRING () const
	{
		return m_rstrValue;
	}

	RSTRING operator* ()
	{
		return m_rstrValue;
	}

	RSTRING* operator& ()
	{
		Assert(NULL == m_rstrValue);
		return &m_rstrValue;
	}

	operator bool () const
	{
		return (NULL != m_rstrValue);
	}

	bool operator! () const
	{
		return (NULL == m_rstrValue);
	}

	bool operator== (RSTRING rstrOther) const
	{
		INT nResult;
		return SUCCEEDED(RStrCompareRStr(m_rstrValue, rstrOther, &nResult)) && 0 == nResult;
	}

	CRString& operator= (__in_opt RSTRING rstrOther)
	{
		RStrReplace(&m_rstrValue, rstrOther);
		return *this;
	}

	inline RSTRING Detach (VOID)
	{
		RSTRING rstrDetached = m_rstrValue;
		m_rstrValue = NULL;
		return rstrDetached;
	}

	inline INT Length (VOID)
	{
		return RStrLen(m_rstrValue);
	}
};
