#pragma once

#include "Assert.h"

#define	CheckNoTrace(x) \
	BEGIN_MULTI_LINE_MACRO \
		hr = x; \
		if(FAILED(hr)) \
		{ \
			goto Cleanup; \
		} \
	END_MULTI_LINE_MACRO

#if	defined(_DEBUG) || defined(_CHECK_REPORT_ERROR)

	#define	Check(x) \
		BEGIN_MULTI_LINE_MACRO \
			hr = x; \
			if(FAILED(hr)) \
			{ \
				_ReportAssertFailure(hr,__FILE__,__LINE__); \
				goto Cleanup; \
			} \
		END_MULTI_LINE_MACRO

	#define	CheckIgnore(x,hrIgnore) \
		BEGIN_MULTI_LINE_MACRO \
			hr = x; \
			if(FAILED(hr)) \
			{ \
				if(hr != hrIgnore) \
					_ReportAssertFailure(hr,__FILE__,__LINE__); \
				goto Cleanup; \
			} \
		END_MULTI_LINE_MACRO

	#define	CheckIf(x,y) \
		BEGIN_MULTI_LINE_MACRO \
			if(x) \
			{ \
				__pragma(warning(push)) \
				__pragma(warning(disable:4127)) \
				if(FAILED(y)) \
					_ReportAssertFailure(y,__FILE__,__LINE__); \
				__pragma(warning(pop)) \
				hr = y; \
				goto Cleanup; \
			} \
		END_MULTI_LINE_MACRO

	#define	CheckIfIgnore(x,y) \
		BEGIN_MULTI_LINE_MACRO \
			if(x) \
			{ \
				hr = y; \
				goto Cleanup; \
			} \
		END_MULTI_LINE_MACRO

	#define	CheckIfGetLastErrorCore(f, trace) \
		BEGIN_MULTI_LINE_MACRO \
			if(f) \
			{ \
				hr = HrEnsureLastError(); \
				__pragma(warning(push)) \
				__pragma(warning(disable:4127)) \
				if(trace) \
					_ReportAssertFailure(hr,__FILE__,__LINE__); \
				__pragma(warning(pop)) \
				goto Cleanup; \
			} \
		END_MULTI_LINE_MACRO

	#define	CheckIfGetLastErrorIgnore(f,x) \
		BEGIN_MULTI_LINE_MACRO \
			if(f) \
			{ \
				hr = HrEnsureLastError(); \
				if(HRESULT_FROM_WIN32(x) != hr) \
				{ \
					_ReportAssertFailure(hr,__FILE__,__LINE__); \
				} \
				goto Cleanup; \
			} \
		END_MULTI_LINE_MACRO

	#define	CheckIfGetLastErrorXCore(x) \
		BEGIN_MULTI_LINE_MACRO \
			hr = HrEnsureLastError(); \
			if(HRESULT_FROM_WIN32(x) != hr) \
			{ \
				_ReportAssertFailure(hr,__FILE__,__LINE__); \
				goto Cleanup; \
			} \
		END_MULTI_LINE_MACRO

	#define	CheckTrue(x,y) \
		BEGIN_MULTI_LINE_MACRO \
			if(!(x)) \
			{ \
				__pragma(warning(push)) \
				__pragma(warning(disable:4127)) \
				if(FAILED(y)) \
					_ReportAssertFailure(y,__FILE__,__LINE__); \
				__pragma(warning(pop)) \
				hr = y; \
				goto Cleanup; \
			} \
		END_MULTI_LINE_MACRO

	#define	CheckAlloc(p) \
		BEGIN_MULTI_LINE_MACRO \
			if(NULL == (p)) \
			{ \
				_ReportAssertFailure(E_OUTOFMEMORY,__FILE__,__LINE__); \
				hr = E_OUTOFMEMORY; \
				goto Cleanup; \
			} \
		END_MULTI_LINE_MACRO

	#define	CheckA(x) \
		BEGIN_MULTI_LINE_MACRO \
			hr = x; \
			__pragma(warning(push)) \
			__pragma(warning(disable:4127)) \
			if(FAILED(hr)) \
			{ \
				_ReportAssertFailure(hr,__FILE__,__LINE__); \
				Assert(false); \
				goto Cleanup; \
			} \
			__pragma(warning(pop)) \
		END_MULTI_LINE_MACRO

	#define	CheckIfA(x,y) \
		BEGIN_MULTI_LINE_MACRO \
			if(x) \
			{ \
				if(FAILED(y)) \
					_ReportAssertFailure(y,__FILE__,__LINE__); \
				hr = y; \
				Assert(false); \
				goto Cleanup; \
			} \
		END_MULTI_LINE_MACRO

	#define	CheckTrueA(x,y) \
		BEGIN_MULTI_LINE_MACRO \
			if(!(x)) \
			{ \
				if(FAILED(y)) \
					_ReportAssertFailure(y,__FILE__,__LINE__); \
				hr = y; \
				Assert(false); \
				goto Cleanup; \
			} \
		END_MULTI_LINE_MACRO

	#define	CheckWin32Error(x) \
		BEGIN_MULTI_LINE_MACRO \
			LONG lCheckErrorCode = x; \
			hr = HRESULT_FROM_WIN32(lCheckErrorCode); \
			if(FAILED(hr)) \
			{ \
				_ReportAssertFailure(hr,__FILE__,__LINE__); \
				goto Cleanup; \
			} \
		END_MULTI_LINE_MACRO

	#define	CheckWin32ErrorIgnore(x, hrIgnore) \
		BEGIN_MULTI_LINE_MACRO \
			LONG lCheckErrorCode = x; \
			hr = HRESULT_FROM_WIN32(lCheckErrorCode); \
			if(FAILED(hr)) \
			{ \
				if(hr != hrIgnore) \
					_ReportAssertFailure(hr,__FILE__,__LINE__); \
				goto Cleanup; \
			} \
		END_MULTI_LINE_MACRO

	#define	Verify(f) \
		BEGIN_MULTI_LINE_MACRO \
			if(!(f)) \
			{ \
				_ReportAssertFailure(S_FALSE,__FILE__,__LINE__); \
			} \
		END_MULTI_LINE_MACRO

	#define	VerifyHr(x) \
		BEGIN_MULTI_LINE_MACRO \
			hr = x; \
			if(FAILED(hr)) \
			{ \
				_ReportAssertFailure(hr,__FILE__,__LINE__); \
			} \
		END_MULTI_LINE_MACRO

	#define	CheckMsgCore(x, trace, msg, ...) \
		BEGIN_MULTI_LINE_MACRO \
			hr = x; \
			if(FAILED(hr)) \
			{ \
				_CheckMsg(msg, __VA_ARGS__); \
				__pragma(warning(push)) \
				__pragma(warning(disable:4127)) \
				if(trace) \
					_ReportAssertFailure(hr,__FILE__,__LINE__); \
				__pragma(warning(pop)) \
				goto Cleanup; \
			} \
		END_MULTI_LINE_MACRO

	#define	CheckIfMsgCore(x, y, trace, msg, ...) \
		BEGIN_MULTI_LINE_MACRO \
			if(x) \
			{ \
				_CheckMsg(msg, __VA_ARGS__); \
				__pragma(warning(push)) \
				__pragma(warning(disable:4127)) \
				if(FAILED(y) && trace) \
					_ReportAssertFailure(y,__FILE__,__LINE__); \
				__pragma(warning(pop)) \
				hr = y; \
				goto Cleanup; \
			} \
		END_MULTI_LINE_MACRO

	#define	CheckMsg(x, msg, ...)		CheckMsgCore(x, TRUE, msg, __VA_ARGS__)
	#define	CheckIfMsg(x, y, msg, ...)	CheckIfMsgCore(x, y, TRUE, msg, __VA_ARGS__)

#else

	#define	Check(x)					CheckNoTrace(x)

	#define	CheckIgnore(x,hrIgnore)		Check(x)

	#define	CheckIf(x,y) \
		BEGIN_MULTI_LINE_MACRO \
			if(x) \
			{ \
				hr = y; \
				goto Cleanup; \
			} \
		END_MULTI_LINE_MACRO

	#define	CheckIfIgnore(x,y)			CheckIf(x,y)

	#define	CheckIfGetLastErrorCore(f, trace) \
		BEGIN_MULTI_LINE_MACRO \
			if(f) \
			{ \
				hr = HrEnsureLastError(); \
				goto Cleanup; \
			} \
		END_MULTI_LINE_MACRO

	#define	CheckIfGetLastErrorIgnore(f,x)		CheckIfGetLastErrorCore(f, FALSE)

	#define	CheckIfGetLastErrorXCore(x) \
		BEGIN_MULTI_LINE_MACRO \
			hr = HrEnsureLastError(); \
			if(HRESULT_FROM_WIN32(x) != hr) \
			{ \
				goto Cleanup; \
			} \
		END_MULTI_LINE_MACRO

	#define	CheckTrue(x,y) \
		BEGIN_MULTI_LINE_MACRO \
			if(!(x)) \
			{ \
				hr = y; \
				goto Cleanup; \
			} \
		END_MULTI_LINE_MACRO

	#define	CheckAlloc(p) \
		BEGIN_MULTI_LINE_MACRO \
			if(NULL == (p)) \
			{ \
				hr = E_OUTOFMEMORY; \
				goto Cleanup; \
			} \
		END_MULTI_LINE_MACRO

	#define	CheckA(hr)					Check(hr)
	#define	CheckIfA(x,y)				CheckIf(x,y)
	#define	CheckTrueA(x,y)				CheckTrue(x,y)

	#define	CheckWin32Error(x) \
		BEGIN_MULTI_LINE_MACRO \
			LONG lCheckErrorCode = x; \
			Check(HRESULT_FROM_WIN32(lCheckErrorCode)); \
		END_MULTI_LINE_MACRO

	#define	CheckWin32ErrorIgnore(x, hrIgnore) \
		BEGIN_MULTI_LINE_MACRO \
			LONG lCheckErrorCode = x; \
			CheckIgnore(HRESULT_FROM_WIN32(lCheckErrorCode), hrIgnore); \
		END_MULTI_LINE_MACRO

	#define	Verify(f) \
		BEGIN_MULTI_LINE_MACRO \
			f; \
		END_MULTI_LINE_MACRO

	#define	VerifyHr(x) \
		BEGIN_MULTI_LINE_MACRO \
			hr = x; \
		END_MULTI_LINE_MACRO

	#define	CheckMsgCore(x, trace, msg, ...) \
		BEGIN_MULTI_LINE_MACRO \
			hr = x; \
			if(FAILED(hr)) \
			{ \
				_CheckMsg(msg, __VA_ARGS__); \
				goto Cleanup; \
			} \
		END_MULTI_LINE_MACRO

	#define	CheckIfMsgCore(x, y, trace, msg, ...) \
		BEGIN_MULTI_LINE_MACRO \
			if(x) \
			{ \
				_CheckMsg(msg, __VA_ARGS__); \
				hr = y; \
				goto Cleanup; \
			} \
		END_MULTI_LINE_MACRO

	#define	CheckMsg(x, msg, ...)		CheckMsgCore(x, FALSE, msg, __VA_ARGS__)
	#define	CheckIfMsg(x, y, msg, ...)	CheckIfMsgCore(x, y, FALSE, msg, __VA_ARGS__)

#endif

#define	CheckIfGetLastError(f)				CheckIfGetLastErrorCore(f, TRUE)
#define	CheckIfGetLastErrorNoTrace(f)		CheckIfGetLastErrorCore(f, FALSE)

#define	CheckIfGetLastErrorX(f,x) \
	BEGIN_MULTI_LINE_MACRO \
		if(f) \
		{ \
			CheckIfGetLastErrorXCore(x); \
		} \
	END_MULTI_LINE_MACRO

#define	CheckMsgNoTrace(x, msg, ...)		CheckMsgCore(x, FALSE, msg, __VA_ARGS__)
#define	CheckIfMsgNoTrace(x, y, msg, ...)	CheckIfMsgCore(x, y, FALSE, msg, __VA_ARGS__)
