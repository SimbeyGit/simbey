#include <windows.h>
#include "Assert.h"

static CHECK_MSG_CALLBACK g_pfnCheckMsgCallback = NULL;

void WINAPI _SetCheckMsgCallback (CHECK_MSG_CALLBACK pfnCallback)
{
	g_pfnCheckMsgCallback = pfnCallback;
}

void WINAPI _CheckMsg (PCSTR pcszMsg, ...)
{
	if(g_pfnCheckMsgCallback)
	{
		va_list vArgs;
		va_start(vArgs, pcszMsg);
		g_pfnCheckMsgCallback(pcszMsg, vArgs);
		va_end(vArgs);
	}
}

#if	defined(_DEBUG) || defined(_CHECK_REPORT_ERROR)

static ASSERT_FAILURE_CALLBACK g_lpfnFailureCallback = NULL;

void WINAPI _SetAssertCallback (ASSERT_FAILURE_CALLBACK lpfnCallback)
{
	g_lpfnFailureCallback = lpfnCallback;
}

ASSERT_FAILURE_CALLBACK WINAPI _GetAssertCallback (VOID)
{
	return g_lpfnFailureCallback;
}

void WINAPI _ReportAssertFailure (HRESULT hr, LPCSTR lpcszFile, UINT nLine)
{
	if(g_lpfnFailureCallback)
		g_lpfnFailureCallback(hr,lpcszFile,nLine);
}

void WINAPI _AssertFailure (LPCSTR lpcszFile, UINT nLine)
{
	_ReportAssertFailure(S_FALSE, lpcszFile, nLine);

#if defined(_DEBUG)
	DebugBreak();
#endif
}

#endif

HRESULT HrEnsureLastError (VOID)
{
	LONG lError = GetLastError();
	if(NOERROR == lError)
		lError = ERROR_INTERNAL_ERROR;
	return HRESULT_FROM_WIN32(lError);
}
