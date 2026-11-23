#include "AsyncWaits.h"

using namespace AQVM;

namespace
{
	PVOID volatile s_quarantinedWaits = NULL;
#ifdef QUADOO_ASYNC_TESTS
	AsyncWaitTestHooks s_waitHooks = {0};
#endif
}

#ifdef QUADOO_ASYNC_TESTS
AsyncWaitTestHooks& AQVM::AsyncWaitTests()
{
	return s_waitHooks;
}
#endif

BOOL AQVM::PostAsyncDelivery(const AsyncDelivery& delivery)
{
	if(delivery.postError)
	{
		SetLastError(delivery.postError);
		return FALSE;
	}
	return PostMessageW(delivery.window,delivery.message,delivery.ticket,delivery.object);
}

CAsyncWait::CAsyncWait() : m_handle(NULL), m_registration(NULL), m_source(NULL), cleanupNext(NULL)
{
	ZeroMemory(&m_native,sizeof(m_native));
#ifdef QUADOO_ASYNC_TESTS
	s_waitHooks.waits++;
#endif
}

CAsyncWait::~CAsyncWait()
{
	Assert(!m_registration);
	Assert(!m_source);
	if(m_handle)
	{
		CloseHandle(m_handle);
#ifdef QUADOO_ASYNC_TESTS
		s_waitHooks.handles--;
#endif
	}
#ifdef QUADOO_ASYNC_TESTS
	s_waitHooks.waits--;
#endif
}

HRESULT CAsyncWait::Initialize(IQuadooObject* source, const AsyncDelivery& delivery, HWND notificationWindow, UINT notificationMessage)
{
	HRESULT hr;
	TStackRef<IQuadooNativeHandle> native;
	HANDLE handle;
	CheckIf(!source,E_INVALIDARG);
	m_source = source;
	source->AddRef();
	Check(source->QueryInterface(&native));
	CheckIf(!native,E_UNEXPECTED);
	handle = native->GetNativeHandle();
	CheckIf(!handle || handle == INVALID_HANDLE_VALUE,HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE));
#ifdef QUADOO_ASYNC_TESTS
	CheckIf(s_waitHooks.duplicateError,HRESULT_FROM_WIN32(s_waitHooks.duplicateError));
#endif
	CheckIfGetLastError(!DuplicateHandle(GetCurrentProcess(),handle,GetCurrentProcess(),&m_handle,SYNCHRONIZE,FALSE,0));
#ifdef QUADOO_ASYNC_TESTS
	s_waitHooks.handles++;
#endif
	m_native.delivery = delivery;
	m_native.notificationWindow = notificationWindow;
	m_native.notificationMessage = notificationMessage;
#ifdef QUADOO_ASYNC_TESTS
	s_waitHooks.lastTicket = delivery.ticket;
	if(s_waitHooks.postError)
	{
		m_native.delivery.postError = s_waitHooks.postError;
	}
	m_native.posted = s_waitHooks.posted;
	m_native.finish = s_waitHooks.finish;
	m_native.notified = s_waitHooks.notified;
	m_native.finishNotification = s_waitHooks.finishNotification;
	m_native.forcedNotificationError = s_waitHooks.notificationError;
#endif
Cleanup:
	return hr;
}

HRESULT CAsyncWait::Register()
{
	HRESULT hr = S_OK;
	CheckIf(!m_handle || m_registration,QuadooForm::EInvalidState);
#ifdef QUADOO_ASYNC_TESTS
	CheckIf(s_waitHooks.registerError,HRESULT_FROM_WIN32(s_waitHooks.registerError));
#endif
	CheckIfGetLastError(!RegisterWaitForSingleObject(&m_registration,m_handle,WaitCallback,&m_native,INFINITE,WT_EXECUTEONLYONCE));
#ifdef QUADOO_ASYNC_TESTS
	s_waitHooks.registrations++;
	if(s_waitHooks.waitBeforeRegisterReturn)
	{
		CheckIf(WaitForSingleObject(s_waitHooks.posted,5000) != WAIT_OBJECT_0,HRESULT_FROM_WIN32(ERROR_TIMEOUT));
	}
#endif
Cleanup:
	return hr;
}

VOID CALLBACK CAsyncWait::WaitCallback(PVOID context, BOOLEAN timeout)
{
	NativeContext* native = static_cast<NativeContext*>(context);
	if(InterlockedCompareExchange(&native->fired,1,0))
	{
		return;
	}
	DWORD error = ERROR_SUCCESS;
	if(timeout)
	{
		error = ERROR_TIMEOUT;
	}
	else if(!InterlockedCompareExchange(&native->canceled,0,0) && !PostAsyncDelivery(native->delivery))
	{
		error = GetLastError();
		if(!error)
		{
			error = ERROR_GEN_FAILURE;
		}
	}
#ifdef QUADOO_ASYNC_TESTS
	if(native->posted)
	{
		SetEvent(native->posted);
	}
	if(native->finish)
	{
		WaitForSingleObject(native->finish,5000);
	}
#endif
	InterlockedExchange(&native->error,static_cast<LONG>(error));
	BOOL notify = error && native->notificationWindow && native->notificationMessage &&
		!InterlockedCompareExchange(&native->canceled,0,0);
	InterlockedExchange(&native->notificationError,notify ? ERROR_IO_PENDING : ERROR_SUCCESS);
	InterlockedExchange(&native->finished,1);
	if(notify)
	{
		DWORD notificationError = native->forcedNotificationError;
#ifdef QUADOO_ASYNC_TESTS
		InterlockedIncrement(&s_waitHooks.notifications);
#endif
		// Cross-thread SendNotifyMessage does not wait for the UI procedure.
		// The UI may already be draining this callback when this call returns.
		if(!notificationError && !SendNotifyMessageW(native->notificationWindow,native->notificationMessage,0,0))
		{
			notificationError = GetLastError();
			if(!notificationError)
			{
				notificationError = ERROR_GEN_FAILURE;
			}
		}
		InterlockedExchange(&native->notificationError,static_cast<LONG>(notificationError));
	}
#ifdef QUADOO_ASYNC_TESTS
	if(native->notified)
	{
		SetEvent(native->notified);
	}
	if(native->finishNotification)
	{
		WaitForSingleObject(native->finishNotification,5000);
	}
#endif
}

VOID CAsyncWait::Cancel()
{
	InterlockedExchange(&m_native.canceled,1);
}

HRESULT CAsyncWait::Drain()
{
	HRESULT hr = S_OK;
	if(m_registration)
	{
#ifdef QUADOO_ASYNC_TESTS
		if(s_waitHooks.drainEntered)
		{
			SetEvent(s_waitHooks.drainEntered);
		}
		if(s_waitHooks.unregisterFailures)
		{
			s_waitHooks.unregisterFailures--;
			Check(HRESULT_FROM_WIN32(ERROR_BUSY));
		}
#endif
		CheckIfGetLastError(!UnregisterWaitEx(m_registration,INVALID_HANDLE_VALUE));
		m_registration = NULL;
#ifdef QUADOO_ASYNC_TESTS
		s_waitHooks.registrations--;
#endif
	}
	if(m_handle)
	{
		CloseHandle(m_handle);
		m_handle = NULL;
#ifdef QUADOO_ASYNC_TESTS
		s_waitHooks.handles--;
#endif
	}
Cleanup:
	return hr;
}

VOID CAsyncWait::ReleaseSource()
{
	IQuadooObject* source = m_source;
	m_source = NULL;
	if(source)
	{
		QVMReleaseObject(source);
	}
}

BOOL CAsyncWait::Fired()
{
	return InterlockedCompareExchange(&m_native.fired,0,0) != 0;
}

BOOL CAsyncWait::Failed()
{
	return InterlockedCompareExchange(&m_native.finished,0,0) != 0 &&
		InterlockedCompareExchange(&m_native.error,0,0) != ERROR_SUCCESS;
}

HRESULT CAsyncWait::PostResult()
{
	HRESULT hr = E_PENDING;
	if(InterlockedCompareExchange(&m_native.finished,0,0))
	{
		hr = HRESULT_FROM_WIN32(static_cast<DWORD>(InterlockedCompareExchange(&m_native.error,0,0)));
	}
	return hr;
}

DWORD CAsyncWait::NotificationError()
{
	return static_cast<DWORD>(InterlockedCompareExchange(&m_native.notificationError,0,0));
}

VOID CAsyncWait::Quarantine()
{
	Cancel();
	ReleaseSource();
	// Ownership of this native-only record transfers to the process-lifetime
	// list. The callback uses no VM code and never dereferences its object value.
	PVOID previous;
	do
	{
		previous = InterlockedCompareExchangePointer(&s_quarantinedWaits,NULL,NULL);
		cleanupNext = static_cast<CAsyncWait*>(previous);
	}
	while(InterlockedCompareExchangePointer(&s_quarantinedWaits,this,previous) != previous);
}
