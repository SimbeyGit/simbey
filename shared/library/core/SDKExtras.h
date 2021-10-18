// There are things that should be available in the Windows SDK but aren't.
// This header contains those missing or incomplete features.

#pragma once

// IID_PPV_ARGS
//
// Depending on the SDK installed, IID_PPV_ARGS may not be available.
#ifndef	IID_PPV_ARGS
	extern "C++"
	{
		template<typename T> PVOID* IID_PPV_ARGS_Helper (T** ppObject) 
		{
			// make sure everyone derives from IUnknown
			static_cast<IUnknown*>(*ppObject);

			return reinterpret_cast<void**>(ppObject);
		}
	}

	#define	IID_PPV_ARGS(ppObject) __uuidof(**(ppObject)), IID_PPV_ARGS_Helper(ppObject)
#endif

#ifndef	WM_MOUSEWHEEL
	#define	WM_MOUSEWHEEL		0x020A
#endif

#ifndef	WM_INPUT
	#define	WM_INPUT			0x00FF
#endif

#ifndef	GET_X_LPARAM
	#define	GET_X_LPARAM(lp)	((INT)(SHORT)LOWORD(lp))
#endif

#ifndef	GET_Y_LPARAM
	#define	GET_Y_LPARAM(lp)	((INT)(SHORT)HIWORD(lp))
#endif

// InterlockedOr()
//
// This is ridiculous!  According to MSDN, it should always be available, but
// WinBase.h screwed it up.  It appears to only be available for 64-bit builds,
// but clearly there's no reason it shouldn't be available for 32-bit.
#if	!defined(InterlockedOr)

#define	InterlockedOr			InterlockedOr_Inline

LONG __forceinline InterlockedOr_Inline (__inout volatile LONG* pnTarget, __in LONG nSetBits)
{
	LONG i, j;

	j = *pnTarget;
	do
	{
		i = j;
		j = InterlockedCompareExchange(pnTarget, i | nSetBits, i);
	} while(i != j);

	return j;
}

#endif

// InterlockedAnd()
//
// Same comments apply to InterlockedAnd() as to InterlockedOr().
#if	!defined(InterlockedAnd)

#define	InterlockedAnd			InterlockedAnd_Inline

LONG __forceinline InterlockedAnd_Inline (__inout volatile LONG* pnTarget, __in LONG nSetBits)
{
	LONG i, j;

	j = *pnTarget;
	do
	{
		i = j;
		j = InterlockedCompareExchange(pnTarget, i & nSetBits, i);
	} while(i != j);

	return j;
}

#endif

__if_not_exists(GUID_WICPixelFormat32bppRGBA)
{
	DEFINE_GUID(GUID_WICPixelFormat32bppRGBA, 0xf5c7ad2d, 0x6a8d, 0x43dd, 0xa7, 0xa8, 0xa2, 0x99, 0x35, 0x26, 0x1a, 0xe9);
}

#ifndef	__in_xcount
	#define	__in_xcount(size)				// Ignore this if the installed SDK doesn't contain this macro.
#endif

#ifndef	__deref_out_range
	#define	__deref_out_range(lb,ub)		// Ignore this if the installed SDK doesn't contain this macro.
#endif

#define	VT_RSTRING				VT_USERDEFINED
