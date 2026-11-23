#include "AsyncCalls.h"
#include "Library\Util\Formatting.h"

using namespace AQVM;

namespace
{
	volatile LONG s_ticket = 0;
	// Existing VM exception identity. Only its IUnknown prefix is used; no VM
	// implementation header, class layout or object-pointer cast is required.
	const IID ExceptionIdentity = {0x5660ed64,0x49bb,0x48b3,{0x90,0x8d,0x5d,0x5b,0x25,0x50,0x12,0xca}};

#ifdef QUADOO_ASYNC_TESTS
	AsyncTestHooks s_hooks = {-1,0,FALSE,0,0};
#endif
	BOOL FailAllocation()
	{
#ifdef QUADOO_ASYNC_TESTS
		if(s_hooks.failAllocation == 0)
		{
			s_hooks.failAllocation = -1;
			return TRUE;
		}
		if(s_hooks.failAllocation > 0)
		{
			s_hooks.failAllocation--;
		}
#endif
		return FALSE;
	}
	HRESULT NextTicket(UINT_PTR* ticket)
	{
		HRESULT hr = S_OK;
#ifdef QUADOO_ASYNC_TESTS
		CheckIf(s_hooks.failTicket,HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW));
#endif
		for(;;)
		{
			LONG current = InterlockedCompareExchange(&s_ticket,0,0);
			DWORD value = static_cast<DWORD>(current);
			CheckIf(value == MAXDWORD,HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW));
			if(InterlockedCompareExchange(&s_ticket,static_cast<LONG>(value + 1),current) == current)
			{
				*ticket = static_cast<UINT_PTR>(value + 1);
				break;
			}
		}
	Cleanup:
		return hr;
	}

}

#ifdef QUADOO_ASYNC_TESTS
AsyncTestHooks& AQVM::AsyncTests()
{
	return s_hooks;
}
#endif

namespace AQVM
{
	class CAsyncTarget : public CBaseUnknown, public IQuadooComponentDestroyedObserver
	{
	public:
		CAsyncCalls* owner;
		TStackRef<CInstanceLink> link;
		TStackRef<IQuadooControlContainer> container;
		TStackRef<IUnknown> identity;
		TStackRef<IQuadooComponentLifecycle> lifecycle;
		HWND window;
		BOOL closed;
		BOOL subscribed;
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooComponentDestroyedObserver)
		END_UNK_MAP
		CAsyncTarget(CAsyncCalls* calls, CInstanceLink* binding, IQuadooControlContainer* control, HWND hwnd) :
			owner(calls), link(binding), container(control), window(hwnd), closed(FALSE), subscribed(FALSE)
		{
#ifdef QUADOO_ASYNC_TESTS
			s_hooks.targets++;
#endif
		}
		~CAsyncTarget()
		{
			Assert(!subscribed);
#ifdef QUADOO_ASYNC_TESTS
			s_hooks.targets--;
#endif
		}
		VOID Unsubscribe()
		{
			owner = NULL;
			if(subscribed)
			{
				subscribed = FALSE;
				TStackRef<IQuadooComponentLifecycle> source(lifecycle);
				source->RemoveDestroyedObserver(this);
			}
		}
		STDMETHOD_(VOID,OnComponentDestroyed)(IQuadooComponent*)
		{
			TStackRef<CAsyncTarget> keepAlive(this);
			TStackRef<CAsyncCalls> calls(owner);
			if(calls)
			{
				calls->CloseTarget(this,TRUE);
			}
		}
	};
	struct AsyncRequest : public CBaseUnknown
	{
		BEGIN_UNK_MAP
			UNK_INTERFACE_PATH(IUnknown,CBaseUnknown)
		END_UNK_MAP
		TStackRef<CAsyncTarget> target;
		TStackRef<IUnknown> lease;
		IQuadooObject* callback;
		TStackRef<CAsyncWait> wait;
		BOOL waiting;
		AsyncRequest() : callback(NULL), waiting(FALSE)
		{
#ifdef QUADOO_ASYNC_TESTS
			s_hooks.requests++;
#endif
		}
		~AsyncRequest()
		{
			Assert(!wait);
			IQuadooObject* object = callback;
			callback = NULL;
			if(object)
			{
				QVMReleaseObject(object);
			}
			lease.Release();
#ifdef QUADOO_ASYNC_TESTS
			s_hooks.requests--;
#endif
		}
	};
}

CAsyncCalls::CAsyncCalls(CApplicationRuntime* runtime) : m_runtime(runtime), m_closed(FALSE), m_reapingWaits(FALSE), m_reapPending(FALSE), m_cleanupWaits(NULL)
{
}

CAsyncCalls::~CAsyncCalls()
{
	Assert(!m_requests.Length() && !m_targets.Length() && !m_cleanupWaits);
}

VOID CAsyncCalls::Retire(CAsyncTarget* target)
{
	TStackRef<CAsyncTarget> keepAlive(target);
	for(sysint n = 0; n < m_targets.Length(); n++)
	{
		if(m_targets[n] == target)
		{
			m_targets.Remove(n,NULL);
			target->Release();
			break;
		}
	}
	target->Unsubscribe();
}

VOID CAsyncCalls::RetireIdle(CAsyncTarget* target)
{
	if(!target->closed)
	{
		for(sysint n = 0; n < m_requests.Length(); n++)
		{
			if((*m_requests.GetValuePtr(n))->target == target)
			{
				return;
			}
		}
		Retire(target);
	}
}

VOID CAsyncCalls::CancelRequests(CAsyncTarget* target)
{
	for(;;)
	{
		AsyncRequest* request = NULL;
		for(sysint n = 0; n < m_requests.Length(); n++)
		{
			if((*m_requests.GetValuePtr(n))->target == target)
			{
				m_requests.RemoveByIndex(n,&request);
				break;
			}
		}
		if(!request)
		{
			break;
		}
		FinishWait(request);
		request->Release();
	}
}

VOID CAsyncCalls::CloseTarget(CAsyncTarget* target, BOOL destroyed)
{
	TStackRef<CAsyncCalls> keepAlive(this);
	TStackRef<CAsyncTarget> held(target);
	TStackRef<CApplicationRuntime> runtime(m_runtime);
	BOOL entered = runtime && SUCCEEDED(runtime->EnterCallback());
	target->closed = TRUE;
	CancelRequests(target);
	if(destroyed)
	{
		Retire(target);
	}
	if(entered)
	{
		runtime->LeaveCallback();
	}
}

VOID CAsyncCalls::CancelBinding(CInstanceLink* link)
{
	TStackRef<CAsyncCalls> keepAlive(this);
	for(;;)
	{
		TStackRef<CAsyncTarget> target;
		for(sysint n = 0; n < m_targets.Length(); n++)
		{
			if(m_targets[n]->link == link)
			{
				target = m_targets[n];
				break;
			}
		}
		if(!target)
		{
			break;
		}
		CloseTarget(target,TRUE);
	}
}

VOID CAsyncCalls::ReapClosed()
{
	TStackRef<CAsyncCalls> keepAlive(this);
	for(sysint n = m_targets.Length(); n > 0; n--)
	{
		if(n > m_targets.Length())
		{
			continue;
		}
		TStackRef<CAsyncTarget> target(m_targets[n - 1]);
		if(target->closed)
		{
			HWND current = NULL;
			QuadooForm::ComponentState state = QuadooForm::Destroyed;
			if(!IsWindow(target->window) || FAILED(target->container->GetState(&state)) ||
				state >= QuadooForm::Destroyed || FAILED(target->container->GetHandle(&current)) || current != target->window)
			{
				Retire(target);
			}
		}
	}
}

HRESULT CAsyncCalls::FindTarget(CInstanceLink* link, IQuadooControlContainer* container, HWND window, CAsyncTarget** result)
{
	HRESULT hr;
	TStackRef<IUnknown> identity;
	TStackRef<CAsyncTarget> target;
	BOOL created = FALSE;
	Check(container->QueryInterface(IID_IUnknown,reinterpret_cast<PVOID*>(&identity)));
	CheckIf(!identity,E_UNEXPECTED);
	for(sysint n = 0; n < m_targets.Length(); n++)
	{
		if(m_targets[n]->identity == identity && m_targets[n]->link == link)
		{
			target = m_targets[n];
			if(target->window != window)
			{
				CloseTarget(target,TRUE);
				target.Release();
				break;
			}
			CheckIf(target->closed,QuadooForm::EDestroyed);
			*result = target.Detach();
			goto Cleanup;
		}
	}
	CheckIf(FailAllocation(),E_OUTOFMEMORY);
	target.Attach(__new CAsyncTarget(this,link,container,window));
	CheckAlloc(target);
	created = TRUE;
	target->identity = identity;
	CheckIf(FailAllocation(),E_OUTOFMEMORY);
	Check(m_targets.Append(target));
	target->AddRef();
	hr = container->QueryInterface(&target->lifecycle);
	CheckIf(FAILED(hr) && hr != E_NOINTERFACE,hr);
	CheckIf(hr == S_OK && !target->lifecycle,E_UNEXPECTED);
	if(target->lifecycle)
	{
		target->subscribed = TRUE;
		Check(target->lifecycle->AddDestroyedObserver(target));
	}
	CheckIf(target->closed,QuadooForm::EDestroyed);
	*result = target.Detach();
	hr = S_OK;
Cleanup:
	if(FAILED(hr) && target && created)
	{
		Retire(target);
	}
	return hr;
}

HRESULT CAsyncCalls::Enqueue(CInstanceLink* link, IQuadooComponent* component, IQuadooObject* callback)
{
	return Prepare(link,component,NULL,callback);
}

HRESULT CAsyncCalls::EnqueueWait(CInstanceLink* link, IQuadooComponent* component, IQuadooObject* source, IQuadooObject* callback)
{
	HRESULT hr;
	CheckIf(!source,E_INVALIDARG);
	Check(Prepare(link,component,source,callback));
Cleanup:
	return hr;
}

HRESULT CAsyncCalls::Prepare(CInstanceLink* link, IQuadooComponent* component, IQuadooObject* source, IQuadooObject* callback)
{
	HRESULT hr;
	TStackRef<CAsyncCalls> keepAlive(this);
	TStackRef<CApplicationRuntime> runtime(m_runtime);
	TStackRef<IQuadooControlContainer> container;
	TStackRef<IQuadooContainerMessageTarget> supported;
	TStackRef<CAsyncTarget> target;
	TStackRef<AsyncRequest> request;
	UINT_PTR ticket = 0;
	BOOL published = FALSE;
	BOOL entered = FALSE;
	HWND window = NULL;
	DWORD process = 0;
	AsyncDelivery delivery = {0};
	QuadooForm::ComponentState state = QuadooForm::Destroyed;

	CheckIf(!runtime || m_closed,QuadooForm::EInvalidState);
	Check(runtime->CheckAsync());
	CheckIf(!link || !link->instance || !link->asyncOpen || !component || !callback,E_INVALIDARG);
	CheckIf(link->instance->Runtime() != runtime,E_INVALIDARG);
	Check(runtime->EnterCallback());
	entered = TRUE;
	Check(component->GetState(&state));
	CheckIf(state >= QuadooForm::Destroying,QuadooForm::EDestroyed);
	Check(component->QueryInterface(&container));
	Check(component->QueryInterface(&supported));
	CheckIf(!container || !supported,E_NOINTERFACE);
	Check(container->GetHandle(&window));
	CheckIf(!window || !IsWindow(window),QuadooForm::EDestroyed);
	CheckIf(GetWindowThreadProcessId(window,&process) != GetCurrentThreadId() || process != GetCurrentProcessId(),QuadooForm::EWrongThread);
	UINT message = QuadooForm::ContainerCallbackMessage();
	CheckIf(!message,HrEnsureLastError());
	ReapClosed();
	Check(FindTarget(link,container,window,&target));
	CheckIf(FailAllocation(),E_OUTOFMEMORY);
	request.Attach(__new AsyncRequest);
	CheckAlloc(request);
	request->target = target;
	request->callback = callback;
	callback->AddRef();
	Check(runtime->AcquireLifetimeLease(&request->lease));
	Check(NextTicket(&ticket));
	delivery.window = window;
	delivery.message = message;
	delivery.ticket = ticket;
	delivery.object = reinterpret_cast<LPARAM>(callback);
#ifdef QUADOO_ASYNC_TESTS
	delivery.postError = s_hooks.postError;
	s_hooks.postError = 0;
#endif
	if(source)
	{
		CheckIf(FailAllocation(),E_OUTOFMEMORY);
		request->wait.Attach(__new CAsyncWait);
		CheckAlloc(request->wait);
		request->waiting = TRUE;
		HWND notificationWindow;
		UINT notificationMessage;
		Check(runtime->GetAsyncWaitNotification(&notificationWindow,&notificationMessage));
		Check(request->wait->Initialize(source,delivery,notificationWindow,notificationMessage));
	}
	CheckIf(FailAllocation(),E_OUTOFMEMORY);
	Check(m_requests.Add(ticket,request));
	request->AddRef();
	published = TRUE;
	Check(runtime->CheckAsync());
	CheckIf(!link->instance || !link->asyncOpen || target->closed,QuadooForm::EDestroyed);
	Check(container->GetState(&state));
	CheckIf(state >= QuadooForm::Destroying,QuadooForm::EDestroyed);
	HWND current = NULL;
	Check(container->GetHandle(&current));
	CheckIf(current != window,QuadooForm::EDestroyed);
	Check(runtime->CheckAsync());
	CheckIf(!link->instance || !link->asyncOpen || target->closed,QuadooForm::EDestroyed);
	if(request->wait)
	{
		Check(request->wait->Register());
	}
	else
	{
		CheckIfGetLastError(!PostAsyncDelivery(delivery));
	}
Cleanup:
	if(request && FAILED(hr) && published)
	{
		AsyncRequest* removed = NULL;
		if(SUCCEEDED(m_requests.Remove(ticket,&removed)))
		{
			FinishWait(removed);
			removed->Release();
		}
	}
	if(request && FAILED(hr))
	{
		FinishWait(request);
	}
	request.Release();
	if(target)
	{
		RetireIdle(target);
		target.Release();
	}
	if(entered)
	{
		runtime->LeaveCallback();
	}
	return hr;
}

VOID CAsyncCalls::Diagnose(CApplicationRuntime* runtime, CAsyncTarget* target, HRESULT error, QuadooVM::QVARIANT* result, BOOL waiting, DWORD notificationError)
{
	TStackRef<IUnknown> exception;
	QuadooVM::QVARIANT code = {QuadooVM::Null};
	QuadooVM::QVARIANT description = {QuadooVM::Null};
	QuadooVM::QVPARAMS empty = {0};
	CRString name, path, notificationDetail;

	if(SUCCEEDED(error) && result->eType == QuadooVM::Object && result->pObject &&
		SUCCEEDED(result->pObject->QueryInterface(ExceptionIdentity,reinterpret_cast<PVOID*>(&exception))))
	{
		error = DISP_E_EXCEPTION;
		if(SUCCEEDED(result->pObject->GetProperty(NULL,RSTRING_CAST(L"Code"),&code)) &&
			code.eType == QuadooVM::I4 && FAILED(code.lVal))
		{
			error = code.lVal;
		}
		result->pObject->Invoke(NULL,RSTRING_CAST(L"ToString"),&empty,&description);
	}
	if(FAILED(error))
	{
		target->container->GetName(&name);
		if(SUCCEEDED(RStrFormatW(&path,waiting ? L"AsyncWait(%r)" : L"Async(%r)",static_cast<RSTRING>(name))))
		{
			RSTRING detail = description.eType == QuadooVM::String ?
				description.rstrVal : RSTRING_CAST(L"Deferred callback invocation failed.");
			if(notificationError && notificationError != ERROR_IO_PENDING && SUCCEEDED(RStrFormatW(&notificationDetail,
				L"Deferred callback posting failed (0x%.8x); maintenance notification also failed (0x%.8x).",
				error,HRESULT_FROM_WIN32(notificationError))))
			{
				detail = notificationDetail;
			}
			runtime->ReportDiagnostic(error,path,detail);
		}
	}
	QVMClearVariant(&description);
	QVMClearVariant(&code);
}

HRESULT CAsyncCalls::HandleMessage(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result)
{
	HRESULT hr = S_OK;
	TStackRef<CAsyncCalls> keepAlive(this);
	TStackRef<CApplicationRuntime> runtime(m_runtime);
	AsyncRequest* request = NULL;
	BOOL entered = FALSE;
	QuadooVM::QVARIANT value = {QuadooVM::Null};
	CheckIf(!handled || !result,E_POINTER);
	*handled = FALSE;
	*result = 0;
	CheckIf(!runtime || m_closed,QuadooForm::EInvalidState);
	Check(runtime->CheckThread());
	if(message == WM_NCDESTROY)
	{
		for(;;)
		{
			TStackRef<CAsyncTarget> target;
			for(sysint n = 0; n < m_targets.Length(); n++)
			{
				if(!m_targets[n]->closed && m_targets[n]->window == window)
				{
					target = m_targets[n];
					break;
				}
			}
			if(!target)
			{
				break;
			}
			CloseTarget(target,FALSE);
		}
		goto Cleanup;
	}
	if(message != QuadooForm::ContainerCallbackMessage() || FAILED(m_requests.Find(static_cast<UINT_PTR>(wp),&request)))
	{
		request = NULL;
		goto Cleanup;
	}
	if(request->target->window != window || request->callback != reinterpret_cast<IQuadooObject*>(lp))
	{
		request = NULL;
		goto Cleanup;
	}
	if(request->wait && !request->wait->Fired())
	{
		request = NULL;
		goto Cleanup;
	}
	m_requests.Remove(static_cast<UINT_PTR>(wp),NULL);
	*handled = TRUE;
	Check(runtime->EnterCallback());
	entered = TRUE;
	if(request->wait)
	{
		HRESULT drained = request->wait->Drain();
		HRESULT posted = SUCCEEDED(drained) ? request->wait->PostResult() : drained;
		FinishWait(request);
		if(FAILED(posted))
		{
			Diagnose(runtime,request->target,posted,&value,TRUE);
			RetireIdle(request->target);
			goto Cleanup;
		}
	}
	RetireIdle(request->target);
	if(SUCCEEDED(runtime->CheckAsync()) && !request->target->closed && request->target->link->instance && request->target->link->asyncOpen)
	{
		HWND current = NULL;
		QuadooForm::ComponentState state = QuadooForm::Destroyed;
		if(SUCCEEDED(request->target->container->GetState(&state)) && state < QuadooForm::Destroying &&
			SUCCEEDED(request->target->container->GetHandle(&current)) && current == window)
		{
			QuadooVM::QVPARAMS empty = {0};
			HRESULT invoked = request->callback->Invoke(NULL,NULL,&empty,&value);
			Diagnose(runtime,request->target,invoked,&value,request->waiting);
		}
	}
Cleanup:
	QVMClearVariant(&value);
	if(request)
	{
		FinishWait(request);
		request->Release();
	}
	if(entered)
	{
		runtime->LeaveCallback();
	}
	return hr;
}

VOID CAsyncCalls::FinishWait(AsyncRequest* request)
{
	TStackRef<CAsyncWait> wait;
	TStackRef<CApplicationRuntime> runtime(m_runtime);
	wait.Attach(request->wait.Detach());
	if(wait)
	{
		wait->Cancel();
		HRESULT first = wait->Drain();
		HRESULT hr = first;
		// A rejected busy operation gets one immediate retry. Further
		// attempts depend on real UI sweeps or shutdown, never a timer.
		if(hr == HRESULT_FROM_WIN32(ERROR_BUSY))
		{
			hr = wait->Drain();
		}
		if(FAILED(hr))
		{
			// Publish ownership before diagnostics or script releases can
			// reenter shutdown. Workers need no script ownership to finish.
			wait->cleanupNext = m_cleanupWaits;
			m_cleanupWaits = wait;
			wait->AddRef();
		}
		if(FAILED(first) && runtime)
		{
			QuadooVM::QVARIANT empty = {QuadooVM::Null};
			Diagnose(runtime,request->target,first,&empty,TRUE);
		}
		if(SUCCEEDED(hr) && wait->Failed() && runtime)
		{
			QuadooVM::QVARIANT empty = {QuadooVM::Null};
			Diagnose(runtime,request->target,wait->PostResult(),&empty,TRUE,wait->NotificationError());
		}
		wait->ReleaseSource();
	}
}

VOID CAsyncCalls::ReapWaits()
{
	TStackRef<CAsyncCalls> keepAlive(this);
	TStackRef<CApplicationRuntime> runtime(m_runtime);
	if(!runtime)
	{
		return;
	}
	if(m_reapingWaits)
	{
		m_reapPending = TRUE;
		return;
	}
	m_reapingWaits = TRUE;
	runtime->EnterCallback();
	// Retry each existing native cleanup record once per actual reap. A new
	// nested notification must not create an unbounded unregister retry loop.
	CAsyncWait** previous = &m_cleanupWaits;
	while(*previous)
	{
		CAsyncWait* wait = *previous;
		if(SUCCEEDED(wait->Drain()))
		{
			*previous = wait->cleanupNext;
			wait->Release();
		}
		else
		{
			previous = &wait->cleanupNext;
		}
	}
	do
	{
		m_reapPending = FALSE;
		for(;;)
		{
			AsyncRequest* request = NULL;
			for(sysint n = 0; n < m_requests.Length(); n++)
			{
				AsyncRequest* candidate = *m_requests.GetValuePtr(n);
				if(candidate->wait && candidate->wait->Failed())
				{
					m_requests.RemoveByIndex(n,&request);
					break;
				}
			}
			if(!request)
			{
				break;
			}
			FinishWait(request);
			RetireIdle(request->target);
			request->Release();
		}
	}
	while(m_reapPending);
	m_reapingWaits = FALSE;
	runtime->LeaveCallback();
}

VOID CAsyncCalls::Shutdown()
{
	TStackRef<CAsyncCalls> keepAlive(this);
	m_closed = TRUE;
	while(m_targets.Length())
	{
		TStackRef<CAsyncTarget> target(m_targets[m_targets.Length() - 1]);
		CloseTarget(target,TRUE);
	}
	ReapWaits();
	while(m_cleanupWaits)
	{
		CAsyncWait* wait = m_cleanupWaits;
		m_cleanupWaits = wait->cleanupNext;
		if(FAILED(wait->Drain()))
		{
			if(m_runtime)
			{
				m_runtime->ReportDiagnostic(E_FAIL,RSTRING_CAST(L"AsyncWait cleanup"),
					RSTRING_CAST(L"Native wait could not be drained; native-only resources retained until process exit."));
			}
			wait->Quarantine();
		}
		else
		{
			wait->Release();
		}
	}
	m_runtime = NULL;
}

HRESULT CApplicationRuntime::CheckAsync()
{
	HRESULT hr;
	Check(CheckThread());
	CheckIf(!m_catalog || m_stopping || m_shuttingDown,QuadooForm::EInvalidState);
Cleanup:
	return hr;
}
HRESULT CApplicationRuntime::QueueAsync(CInstanceLink* link, IQuadooComponent* component, IQuadooObject* callback)
{
	HRESULT hr;
	Check(CheckAsync());
	CheckIf(!m_async,QuadooForm::EInvalidState);
	Check(m_async->Enqueue(link,component,callback));
Cleanup:
	return hr;
}
HRESULT CApplicationRuntime::QueueAsyncWait(CInstanceLink* link, IQuadooComponent* component, IQuadooObject* source, IQuadooObject* callback)
{
	HRESULT hr;
	Check(CheckAsync());
	CheckIf(!m_async,QuadooForm::EInvalidState);
	Check(m_async->EnqueueWait(link,component,source,callback));
Cleanup:
	return hr;
}
VOID CApplicationRuntime::CancelAsync(CInstanceLink* link)
{
	if(m_async)
	{
		m_async->CancelBinding(link);
	}
}
HRESULT CApplicationRuntime::HandleContainerMessage(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result)
{
	HRESULT hr;
	TStackRef<CApplicationRuntime> keepAlive(this);
	Check(CheckThread());
	CheckIf(!handled || !result,E_POINTER);
	*handled = FALSE;
	*result = 0;
	if(m_async)
	{
		Check(m_async->HandleMessage(window,message,wp,lp,handled,result));
	}
Cleanup:
	return hr;
}
