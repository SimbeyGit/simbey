#include "RuntimeObjects.h"
#include "AsyncCalls.h"

using namespace AQVM;

namespace AQVM
{
	class CApplicationLink : public CBaseUnknown
	{
	public:
		CApplicationRuntime* runtime;
		BEGIN_UNK_MAP
			UNK_INTERFACE_PATH(IUnknown, CBaseUnknown)
		END_UNK_MAP

		CApplicationLink(CApplicationRuntime* owner) : runtime(owner)
		{
		}
	};

	class CRootObserver : public CBaseUnknown, public IQuadooComponentDestroyedObserver
	{
		TStackRef<CApplicationLink> m_link;

	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooComponentDestroyedObserver)
		END_UNK_MAP

		CRootObserver(CApplicationLink* link) : m_link(link)
		{
		}

		STDMETHOD_(VOID,OnComponentDestroyed)(IQuadooComponent* component)
		{
			TStackRef<CApplicationRuntime> runtime(m_link->runtime);
			if(runtime)
			{
				runtime->RootDestroyed(component);
			}
		}
	};

	struct ApplicationRoot
	{
		TStackRef<IUnknown> identity;
		TStackRef<IQuadooComponent> component;
		TStackRef<IQuadooForm> form;
		TStackRef<IQuadooComponentLifecycle> lifecycle;
		TStackRef<CRootObserver> observer;
		BOOL committed;
		BOOL completed;
		BOOL registered;

		ApplicationRoot() : committed(FALSE), completed(FALSE), registered(FALSE)
		{
		}

		~ApplicationRoot()
		{
			if(lifecycle && observer)
			{
				lifecycle->RemoveDestroyedObserver(observer);
			}
		}
	};

	// Internal native seam for future application-owned services. Like component
	// methods, acquisition and final Release are confined to the UI thread.
	class CApplicationLease : public CBaseUnknown
	{
		TStackRef<CApplicationLink> m_link;

	public:
		BEGIN_UNK_MAP
			UNK_INTERFACE_PATH(IUnknown, CBaseUnknown)
		END_UNK_MAP

		CApplicationLease(CApplicationLink* link) : m_link(link)
		{
		}

		~CApplicationLease()
		{
			if(m_link->runtime)
			{
				m_link->runtime->ReleaseLifetimeLease();
			}
		}
	};
}

static const WCHAR LifetimeWindowClass[] = L"Quadoo.ApplicationLifetime";
static const UINT LifetimeCheckMessage = WM_APP + 0x56A;

static UINT AsyncWaitFailureMessage()
{
	static volatile LONG registered = 0;
	UINT message = static_cast<UINT>(InterlockedCompareExchange(&registered,0,0));
	if(!message)
	{
		message = RegisterWindowMessageW(L"QuadooForm.AsyncWaitFailure.{DC9D1DF8-13F6-40F2-96B0-7C1B12EB61C7}");
		if(message)
		{
			InterlockedCompareExchange(&registered,static_cast<LONG>(message),0);
		}
	}
	return message;
}

HRESULT CApplicationRuntime::InitializeLifetime()
{
	HRESULT hr = S_OK;
	WNDCLASSW type = {0};
	HINSTANCE module = GetModuleHandleW(NULL);
	CheckIf(m_dispatch || m_lifetimeLink, QuadooForm::EInvalidState);
	CheckIf(!AsyncWaitFailureMessage(),HrEnsureLastError());
	m_lifetimeLink = __new CApplicationLink(this);
	CheckAlloc(m_lifetimeLink);
	type.lpfnWndProc = LifetimeWindowProc;
	type.hInstance = module;
	type.lpszClassName = LifetimeWindowClass;
	if(!RegisterClassW(&type))
	{
		CheckIf(GetLastError() != ERROR_CLASS_ALREADY_EXISTS, HrEnsureLastError());
		CheckIf(!GetClassInfoW(module, LifetimeWindowClass, &type), HrEnsureLastError());
		CheckIf(type.lpfnWndProc != LifetimeWindowProc, E_UNEXPECTED);
	}
	m_dispatch = CreateWindowExW(0, LifetimeWindowClass, L"", 0, 0, 0, 0, 0,
		HWND_MESSAGE, NULL, module, this);
	CheckIf(!m_dispatch, HrEnsureLastError());
Cleanup:
	return hr;
}

LRESULT CALLBACK CApplicationRuntime::LifetimeWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	CApplicationRuntime* runtime = reinterpret_cast<CApplicationRuntime*>(GetWindowLongPtrW(window, GWLP_USERDATA));
	if(message == WM_NCCREATE)
	{
		CREATESTRUCTW* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
		runtime = static_cast<CApplicationRuntime*>(create->lpCreateParams);
		SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(runtime));
	}
	else if(message == LifetimeCheckMessage && runtime)
	{
		TStackRef<CApplicationRuntime> keepAlive(runtime);
		runtime->m_checkPending = FALSE;
		runtime->CheckLifetime();
		return 0;
	}
	else if(message == AsyncWaitFailureMessage() && !wParam && !lParam && runtime)
	{
		TStackRef<CApplicationRuntime> keepAlive(runtime);
		TStackRef<CAsyncCalls> calls(runtime->m_async);
		if(calls)
		{
#ifdef QUADOO_ASYNC_TESTS
			AsyncWaitTests().maintenanceCalls++;
#endif
			calls->ReapWaits();
		}
		return 0;
	}
	else if(message == WM_NCDESTROY)
	{
		SetWindowLongPtrW(window, GWLP_USERDATA, 0);
	}
	return DefWindowProcW(window, message, wParam, lParam);
}

VOID CApplicationRuntime::QueueLifetimeCheck()
{
	if(m_dispatch && !m_shuttingDown && !m_exitCommitted && !m_checkPending)
	{
		m_checkPending = TRUE;
		m_checkRetry = FALSE;
		if(!PostMessageW(m_dispatch, LifetimeCheckMessage, 0, 0))
		{
			DWORD error = GetLastError();
			m_checkPending = FALSE;
			if(error == ERROR_NOT_ENOUGH_QUOTA)
			{
				// Existing posted messages will make progress without another
				// wake. Retry after the pump removes one; quota is not fatal.
				m_checkRetry = TRUE;
				return;
			}
			// A live dispatch window on this thread should accept this message.
			// Preserve a real failure instead of silently losing the final wake.
			if(SUCCEEDED(m_hrError))
			{
				m_hrError = error ? HRESULT_FROM_WIN32(error) : E_FAIL;
				m_stopping = TRUE;
			}
		}
	}
}

HRESULT CApplicationRuntime::RegisterRoot(IQuadooComponent* component, BOOL committed)
{
	HRESULT hr;
	ApplicationRoot* record = NULL;
	TStackRef<IUnknown> identity;
	QuadooForm::ComponentState state;
	BOOL appended = FALSE;
	BOOL entered = FALSE;
	Check(CheckThread());
	CheckIf(!component, E_INVALIDARG);
	CheckIf(!m_lifetimeLink || m_stopping || (!committed && !m_creationDepth), QuadooForm::EInvalidState);
	Check(EnterCallback());
	entered = TRUE;
	Check(component->QueryInterface(IID_IUnknown, reinterpret_cast<PVOID*>(&identity)));
	for(sysint n = 0; n < m_roots.Length(); n++)
	{
		if(m_roots[n]->identity == identity)
		{
			hr = m_roots[n]->registered ? S_FALSE : QuadooForm::EInvalidState;
			goto Cleanup;
		}
	}
	record = __new ApplicationRoot;
	CheckAlloc(record);
	record->identity = identity;
	record->component = component;
	Check(component->GetState(&state));
	CheckIf(state >= QuadooForm::Destroying, QuadooForm::EDestroyed);
	CheckIf(committed && state != QuadooForm::Ready, QuadooForm::EInvalidState);
	hr = component->QueryInterface(&record->lifecycle);
	if(hr == E_NOINTERFACE)
	{
		ReportDiagnostic(hr, RSTRING_CAST(L"lifecycle"),
			RSTRING_CAST(L"Form/frame roots require IQuadooComponentLifecycle. Update QuadooForm.dll or provide final destruction notifications."));
	}
	Check(hr);
	hr = component->QueryInterface(&record->form);
	CheckIf(FAILED(hr) && hr != E_NOINTERFACE, hr);
	record->observer.Attach(__new CRootObserver(m_lifetimeLink));
	CheckAlloc(record->observer);
	record->committed = committed;
	Check(m_roots.Append(record));
	appended = TRUE;
	Check(record->lifecycle->AddDestroyedObserver(record->observer));
	record->registered = TRUE;
	// Registration can call external code; a completed callback must never be
	// overwritten by the successful return from AddDestroyedObserver.
	m_generation++;
	record = NULL;
	QueueLifetimeCheck();
Cleanup:
	if(record)
	{
		if(appended)
		{
			for(sysint n = 0; n < m_roots.Length(); n++)
			{
				if(m_roots[n] == record)
				{
					m_roots.Remove(n, NULL);
					break;
				}
			}
		}
		__delete record;
	}
	if(entered)
	{
		LeaveCallback();
	}
	return hr;
}

HRESULT CApplicationRuntime::RegisterForm(IQuadooForm* form)
{
	HRESULT hr;
	TStackRef<IQuadooComponent> component;
	BOOL entered = FALSE;
	Check(CheckThread());
	CheckIf(!form, E_INVALIDARG);
	Check(EnterCallback());
	entered = TRUE;
	Check(form->QueryInterface(&component));
	Check(RegisterRoot(component, TRUE));
Cleanup:
	if(entered)
	{
		LeaveCallback();
	}
	return hr;
}

HRESULT CApplicationRuntime::CommitRoot(IQuadooComponent* component)
{
	HRESULT hr;
	TStackRef<IUnknown> identity;
	Check(CheckThread());
	Check(component->QueryInterface(IID_IUnknown, reinterpret_cast<PVOID*>(&identity)));
	hr = QuadooForm::EInvalidState;
	for(sysint n = 0; n < m_roots.Length(); n++)
	{
		if(m_roots[n]->identity == identity)
		{
			m_roots[n]->committed = TRUE;
			m_generation++;
			QueueLifetimeCheck();
			hr = S_OK;
			break;
		}
	}
Cleanup:
	return hr;
}

VOID CApplicationRuntime::UnregisterRoot(IQuadooComponent* component)
{
	TStackRef<IUnknown> identity;
	if(component && SUCCEEDED(component->QueryInterface(IID_IUnknown, reinterpret_cast<PVOID*>(&identity))))
	{
		for(sysint n = 0; n < m_roots.Length(); n++)
		{
			if(m_roots[n]->identity == identity)
			{
				ApplicationRoot* record = m_roots[n];
				m_roots.Remove(n, NULL);
				m_generation++;
				__delete record;
				QueueLifetimeCheck();
				break;
			}
		}
	}
}

VOID CApplicationRuntime::RootDestroyed(IQuadooComponent* component)
{
	QuadooForm::ComponentState state;
	TStackRef<IUnknown> identity;
	if(FAILED(CheckThread()) || !component || m_shuttingDown)
	{
		return;
	}
	m_callbackDepth++;
	HRESULT hr = component->GetState(&state);
	if(SUCCEEDED(hr) && state != QuadooForm::Destroyed)
	{
		hr = QuadooForm::EInvalidState;
	}
	if(SUCCEEDED(hr))
	{
		hr = component->QueryInterface(IID_IUnknown, reinterpret_cast<PVOID*>(&identity));
	}
	if(FAILED(hr))
	{
		OnError(hr, RSTRING_CAST(L"lifecycle"), RSTRING_CAST(L"Invalid final destruction notification."));
		LeaveCallback();
		return;
	}
	for(sysint n = 0; n < m_roots.Length(); n++)
	{
		if(m_roots[n]->identity == identity)
		{
			m_roots[n]->completed = TRUE;
			m_generation++;
			QueueLifetimeCheck();
			break;
		}
	}
	LeaveCallback();
}

HRESULT CApplicationRuntime::BeginScriptEntry()
{
	HRESULT hr;
	Check(CheckThread());
	CheckIf(!m_catalog || m_stopping || m_entryActive || m_started || m_creationDepth || m_callbackDepth,
		QuadooForm::EInvalidState);
	m_entryActive = TRUE;
Cleanup:
	return hr;
}

HRESULT CApplicationRuntime::SetLifetimeCallbacks(IQuadooObject* callbacks)
{
	HRESULT hr;
	TStackRef<IQuadooInterface> methods;
	ULONG ordinal;
	Check(CheckThread());
	CheckIf(!callbacks, E_INVALIDARG);
	CheckIf(!m_entryActive || m_started || m_stopping || m_lifetimeCallbacks || m_creationDepth || m_callbackDepth,
		QuadooForm::EInvalidState);
	Check(callbacks->GetInterface(RSTRING_CAST(L"I__QuadooApplicationLifetime"), &methods));
	Check(methods->ResolveMethod(RSTRING_CAST(L"CollectReleasedObjects"), &ordinal));
	m_collectOrdinal = ordinal;
	m_lifetimeCallbacks = methods;
Cleanup:
	return hr;
}

VOID CApplicationRuntime::CollectReleasedObjects()
{
	if(m_lifetimeCallbacks)
	{
		QuadooVM::QVARIANT result = {QuadooVM::Null};
		QuadooVM::QVPARAMS empty = {0};
		m_callbackDepth++;
		HRESULT hr = m_lifetimeCallbacks->Invoke(m_collectOrdinal, &empty, &result);
		if(SUCCEEDED(hr) && result.eType != QuadooVM::Null && result.eType != QuadooVM::Bool)
		{
			hr = DISP_E_EXCEPTION;
			if(result.eType == QuadooVM::Object)
			{
				QuadooVM::QVARIANT description = {QuadooVM::Null};
				QuadooVM::QVARIANT code = {QuadooVM::Null};
				if(SUCCEEDED(result.pObject->GetProperty(NULL, RSTRING_CAST(L"Code"), &code)) &&
					code.eType == QuadooVM::I4 && FAILED(code.lVal))
				{
					hr = code.lVal;
				}
				if(SUCCEEDED(result.pObject->Invoke(NULL, RSTRING_CAST(L"ToString"), &empty, &description)) &&
					description.eType == QuadooVM::String)
				{
					OnError(hr, RSTRING_CAST(L"lifecycle"), description.rstrVal);
				}
				QVMClearVariant(&description);
				QVMClearVariant(&code);
			}
		}
		QVMClearVariant(&result);
		if(FAILED(hr))
		{
			OnError(hr, RSTRING_CAST(L"lifecycle"), RSTRING_CAST(L"Script destruction failed during application cleanup."));
		}
		m_callbackDepth--;
	}
}

VOID CApplicationRuntime::EndScriptEntry()
{
	m_entryActive = FALSE;
}

HRESULT CApplicationRuntime::BeginMessageLoop(BOOL* run)
{
	HRESULT hr;
	MSG message = {0};
	CheckIf(!run, E_POINTER);
	*run = FALSE;
	Check(CheckThread());
	Check(m_hrError);
	CheckIf(!m_entryActive || m_started || m_stopping || m_creationDepth || m_callbackDepth || m_cleaning,
		QuadooForm::EInvalidState);
	m_started = TRUE;
	Sweep();
	Check(m_hrError);
	// Drain pending work even with no Forms. A posted lifetime wake can mask
	// Win32's low-priority synthetic WM_QUIT until that wake is dispatched.
	*run = HasLifetime() || HasCompletedRoots() || PeekMessageW(&message, NULL, 0, 0, PM_NOREMOVE);
	m_pumping = *run;
	if(m_pumping)
	{
		QueueLifetimeCheck();
	}
	else
	{
		m_stopping = TRUE;
	}
Cleanup:
	return hr;
}

VOID CApplicationRuntime::EndMessageLoop()
{
	m_pumping = FALSE;
	m_stopping = TRUE;
}

HRESULT CApplicationRuntime::EnterCallback()
{
	HRESULT hr;
	Check(CheckThread());
	m_callbackDepth++;
Cleanup:
	return hr;
}

VOID CApplicationRuntime::LeaveCallback()
{
	Assert(m_callbackDepth > 0);
	if(m_callbackDepth)
	{
		m_callbackDepth--;
		if(!m_callbackDepth && !m_cleaning)
		{
			QueueLifetimeCheck();
		}
	}
}

BOOL CApplicationRuntime::HasLifetime()
{
	if(m_leases)
	{
		return TRUE;
	}
	for(sysint n = 0; n < m_roots.Length(); n++)
	{
		if(m_roots[n]->form && !m_roots[n]->completed)
		{
			return TRUE;
		}
	}
	return FALSE;
}

BOOL CApplicationRuntime::HasCompletedRoots()
{
	for(sysint n = 0; n < m_roots.Length(); n++)
	{
		if(m_roots[n]->completed)
		{
			return TRUE;
		}
	}
	return FALSE;
}

VOID CApplicationRuntime::Sweep()
{
	if(!m_started || m_creationDepth || m_callbackDepth || m_cleaning || m_shuttingDown)
	{
		return;
	}
	m_cleaning = TRUE;
	if(m_async)
	{
		TStackRef<CAsyncCalls> calls(m_async);
		calls->ReapWaits();
		calls->ReapClosed();
	}
	BOOL released = FALSE;
	sysint budget = m_instances.Length() + m_roots.Length();
	while(budget-- > 0)
	{
		BOOL removed = FALSE;
		for(sysint n = 0; n < m_instances.Length(); n++)
		{
			if(m_instances[n]->Destroyed())
			{
				TStackRef<CFormInstance> instance;
				instance.Attach(m_instances[n]);
				m_instances.Remove(n, NULL);
				instance->Detach();
				released = TRUE;
				removed = TRUE;
				break;
			}
		}
		if(!removed)
		{
			for(sysint n = 0; n < m_roots.Length(); n++)
			{
				if(m_roots[n]->completed && m_roots[n]->committed)
				{
					ApplicationRoot* record = m_roots[n];
					m_roots.Remove(n, NULL);
					m_generation++;
					__delete record;
					released = TRUE;
					removed = TRUE;
					break;
				}
			}
		}
		if(!removed)
		{
			break;
		}
	}
	if(released)
	{
		CollectReleasedObjects();
	}
	m_cleaning = FALSE;
	if(m_checkRetry)
	{
		QueueLifetimeCheck();
	}
	if(HasCompletedRoots())
	{
		QueueLifetimeCheck();
	}
}

VOID CApplicationRuntime::CheckLifetime()
{
	if(!m_started || !m_pumping || m_creationDepth || m_callbackDepth || m_cleaning || m_shuttingDown || m_exitCommitted)
	{
		return;
	}
	ULONGLONG generation = m_generation;
	Sweep();
	if(SUCCEEDED(m_hrError) && (generation != m_generation || HasCompletedRoots()))
	{
		QueueLifetimeCheck();
	}
	else if(FAILED(m_hrError) || !HasLifetime())
	{
		MSG pending;
		// WM_QUIT may not be synthesized while ordinary posts remain, even
		// with a WM_QUIT-only filter. Drain those first so our PostQuitMessage
		// cannot overwrite an external quit's code.
		if(PeekMessageW(&pending, NULL, 0, 0, PM_NOREMOVE))
		{
			if(pending.message == WM_QUIT)
			{
				m_stopping = TRUE;
				m_exitCommitted = TRUE;
			}
			else
			{
				QueueLifetimeCheck();
			}
		}
		else
		{
			m_stopping = TRUE;
			m_exitCommitted = TRUE;
			PostQuitMessage(FAILED(m_hrError) ? static_cast<INT>(m_hrError) : 0);
		}
	}
}

HRESULT CApplicationRuntime::AcquireLifetimeLease(IUnknown** lease)
{
	HRESULT hr;
	CApplicationLease* created = NULL;
	CheckIf(!lease, E_POINTER);
	*lease = NULL;
	Check(CheckThread());
	CheckIf(!m_lifetimeLink || m_stopping, QuadooForm::EInvalidState);
	CheckIf(m_leases == UINT_MAX, HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW));
	created = __new CApplicationLease(m_lifetimeLink);
	CheckAlloc(created);
	m_leases++;
	m_generation++;
	*lease = created;
	QueueLifetimeCheck();
Cleanup:
	return hr;
}

VOID CApplicationRuntime::ReleaseLifetimeLease()
{
	if(SUCCEEDED(CheckThread()) && m_leases && !m_shuttingDown)
	{
		m_leases--;
		m_generation++;
		QueueLifetimeCheck();
	}
}

VOID CApplicationRuntime::ShutdownLifetime()
{
	if(m_lifetimeLink)
	{
		m_lifetimeLink->runtime = NULL;
	}
	if(m_dispatch)
	{
		SetWindowLongPtrW(m_dispatch, GWLP_USERDATA, 0);
		DestroyWindow(m_dispatch);
		m_dispatch = NULL;
	}
	m_checkPending = FALSE;
	m_checkRetry = FALSE;
	while(m_roots.Length())
	{
		ApplicationRoot* record = m_roots[m_roots.Length() - 1];
		m_roots.Remove(m_roots.Length() - 1, NULL);
		record->component->Destroy();
		__delete record;
	}
	SafeRelease(m_lifetimeLink);
	m_lifetimeCallbacks.Release();
	m_leases = 0;
}

HRESULT CApplicationRuntime::GetAsyncWaitNotification(HWND* window, UINT* message)
{
	HRESULT hr;
	CheckIf(!window || !message,E_POINTER);
	*window = NULL;
	*message = 0;
	Check(CheckAsync());
	CheckIf(!m_dispatch,QuadooForm::EInvalidState);
	*window = m_dispatch;
	*message = AsyncWaitFailureMessage();
	hr = S_OK;
Cleanup:
	return hr;
}
