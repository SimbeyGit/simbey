#ifndef	_H_ASSERT
#define	_H_ASSERT

#include "MultiLineMacros.h"

typedef void (WINAPI* CHECK_MSG_CALLBACK)(PCSTR, va_list);
void WINAPI _SetCheckMsgCallback (CHECK_MSG_CALLBACK pfnCallback);
void WINAPI _CheckMsg (PCSTR pcszMsg, ...);

typedef void (WINAPI* ASSERT_FAILURE_CALLBACK)(HRESULT,LPCSTR,UINT);
typedef void (WINAPI* SET_ERROR_TRACER)(ASSERT_FAILURE_CALLBACK);

#if	defined(_DEBUG) || defined(_CHECK_REPORT_ERROR)

	void WINAPI _SetAssertCallback (ASSERT_FAILURE_CALLBACK lpfnCallback);
	ASSERT_FAILURE_CALLBACK WINAPI _GetAssertCallback (VOID);
	void WINAPI _ReportAssertFailure (HRESULT hr, LPCSTR lpcszFile, UINT nLine);
	void WINAPI _AssertFailure (LPCSTR lpcszFile, UINT nLine);

	// Assert() includes __assume(x) to suppress static code analysis
	// warnings that result from the check within Assert() itself.
	#define	Assert(x) \
		BEGIN_MULTI_LINE_MACRO \
			if(!(x)) \
			{ \
				_AssertFailure(__FILE__, __LINE__); \
			} \
			__assume(x); \
		END_MULTI_LINE_MACRO

#if _MSC_VER <= 1200

	#if !defined(DWORD_PTR)
		#if defined(_WIN64)
			typedef unsigned __int64 ULONG_PTR, *PULONG_PTR;
		#else
			typedef unsigned __int32 ULONG_PTR, *PULONG_PTR;
		#endif

		typedef ULONG_PTR DWORD_PTR, *PDWORD_PTR;
	#endif

#endif

	#define	SideAssert(x)			Assert(x)
	#define	SideAssertHr(x)			Assert(SUCCEEDED(x))
	#define	SideAssertCompare(x,y)	Assert(x == y)

	// These definitions work on Windows XP and Windows 2000
	//#define	AssertNoLockOnCS(cs)	Assert(-1 == cs.LockCount || (DWORD_PTR)cs.OwningThread != GetCurrentThreadId())
	//#define	AssertOwnership(cs)		Assert(0 <= cs.LockCount && (DWORD_PTR)cs.OwningThread == GetCurrentThreadId())

	// These definitions work on Windows Server 2003 SP1 and Windows Vista and later.
	#define	AssertNoLockOnCS(cs)	Assert(0x1 == (0x1 & cs.LockCount) || (DWORD_PTR)cs.OwningThread != GetCurrentThreadId())
	#define	AssertOwnership(cs)		Assert(0x0 == (0x1 & cs.LockCount) && (DWORD_PTR)cs.OwningThread == GetCurrentThreadId())

	#define	SET_ASSERT_CALLBACK(x)	_SetAssertCallback(x)

#else

	#define	Assert(x)
	#define	SideAssert(x)			x
	#define	SideAssertHr(x)			x
	#define	SideAssertCompare(x,y)	x

	#define	AssertNoLockOnCS(cs)
	#define	AssertOwnership(cs)

	#define	SET_ASSERT_CALLBACK(x)

#endif

#define	AssertMsg(x, message)		Assert(x)

#endif

HRESULT HrEnsureLastError (VOID);
