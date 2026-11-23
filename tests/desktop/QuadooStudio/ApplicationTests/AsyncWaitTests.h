#include "..\..\..\..\platform\QuadooForm\ScriptAdapter\AsyncWaits.h"

struct WaitProbeCounts
{
	DWORD thread;
	UINT calls;
	UINT destructed;
	UINT deleted;
	BOOL correctThread;
	BOOL arguments;
	WaitProbeCounts() : thread(GetCurrentThreadId()), calls(0), destructed(0), deleted(0), correctThread(TRUE), arguments(TRUE)
	{
	}
};

// Independent published-interface provider. All its interface/refcount calls
// are checked against the creating thread, including final script destruction.
class CWaitProbe : public CBaseUnknown, public CQuadooObjectImpl, public IQuadooNativeHandle
{
	WaitProbeCounts* m_counts;
	HANDLE m_handle;
	VOID Note()
	{
		m_counts->correctThread = m_counts->correctThread && GetCurrentThreadId() == m_counts->thread;
	}
public:
	HANDLE handleOverride;
	BOOL useHandleOverride;
	TStackRef<IQuadooComponent> destroy;
	HRESULT queryError;
	HANDLE signalOnDestruct;
	HANDLE waitOnDestruct;
	BOOL pumpOnDestruct;
	HANDLE signalOnInvoke;
	HANDLE waitOnInvoke;
	BOOL pumpOnInvoke;
	STDMETHOD(QueryInterface)(REFIID iid, PVOID* object)
	{
		Note();
		if(iid == __uuidof(IQuadooNativeHandle) && queryError != S_OK)
		{
			*object = NULL;
			return queryError == S_FALSE ? S_OK : queryError;
		}
		return CBaseUnknown::QueryInterface(iid,object);
	}
	STDMETHOD_(ULONG,AddRef)()
	{
		Note();
		return CBaseUnknown::AddRef();
	}
	STDMETHOD_(ULONG,Release)()
	{
		Note();
		return CBaseUnknown::Release();
	}
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooObject)
		UNK_INTERFACE(IQuadooNativeHandle)
	END_UNK_MAP
	CWaitProbe(WaitProbeCounts* counts) : m_counts(counts), m_handle(NULL), handleOverride(INVALID_HANDLE_VALUE), useHandleOverride(FALSE), queryError(S_OK),
		signalOnDestruct(NULL), waitOnDestruct(NULL), pumpOnDestruct(FALSE),
		signalOnInvoke(NULL), waitOnInvoke(NULL), pumpOnInvoke(FALSE)
	{
	}
	~CWaitProbe()
	{
		Note();
		Close();
		m_counts->deleted++;
	}
	HRESULT Initialize(BOOL manual = TRUE, BOOL signaled = FALSE)
	{
		m_handle = CreateEventW(NULL,manual,signaled,NULL);
		return m_handle ? S_OK : HrEnsureLastError();
	}
	HANDLE Handle()
	{
		return m_handle;
	}
	VOID Close()
	{
		if(m_handle)
		{
			CloseHandle(m_handle);
			m_handle = NULL;
		}
	}
	STDMETHOD_(HANDLE,GetNativeHandle)()
	{
		Note();
		if(destroy)
		{
			destroy->Destroy();
		}
		return useHandleOverride ? handleOverride : m_handle;
	}
	STDMETHOD(Destruct)()
	{
		HRESULT hr = S_FALSE;
		Note();
		m_counts->destructed++;
		if(signalOnDestruct)
		{
			CheckIfGetLastError(!SetEvent(signalOnDestruct));
		}
		if(waitOnDestruct)
		{
			CheckIf(WaitForSingleObject(waitOnDestruct,5000) != WAIT_OBJECT_0,E_FAIL);
		}
		if(pumpOnDestruct)
		{
			MSG message;
			PeekMessageW(&message,NULL,0,0,PM_NOREMOVE);
		}
	Cleanup:
		return hr;
	}
	STDMETHOD(Invoke)(IQuadooVM* vm, RSTRING method, QuadooVM::QVPARAMS* arguments, QuadooVM::QVARIANT* result)
	{
		HRESULT hr = S_OK;
		Note();
		m_counts->calls++;
		m_counts->arguments = m_counts->arguments && !vm && !method && arguments && !arguments->cArgs && !arguments->pqvArgs;
		result->eType = QuadooVM::Null;
		if(signalOnInvoke)
		{
			CheckIfGetLastError(!SetEvent(signalOnInvoke));
		}
		if(waitOnInvoke)
		{
			CheckIf(WaitForSingleObject(waitOnInvoke,5000) != WAIT_OBJECT_0,E_FAIL);
		}
		if(pumpOnInvoke)
		{
			MSG message;
			PeekMessageW(&message,NULL,0,0,PM_NOREMOVE);
		}
	Cleanup:
		return hr;
	}
};

static HRESULT AsyncWaitEnqueue(IQuadooObject* wrapper, IQuadooObject* source, IQuadooObject* callback)
{
	HRESULT hr;
	QuadooVM::QVARIANT values[2] = {{QuadooVM::Object},{QuadooVM::Object}};
	QuadooVM::QVARIANT result = {QuadooVM::Null};
	QuadooVM::QVPARAMS arguments = {values,2};
	// Native QVPARAMS stores script arguments in reverse order.
	values[1].pObject = source;
	values[0].pObject = callback;
	hr = wrapper->Invoke(NULL,RSTRING_CAST(L"AsyncWait"),&arguments,&result);
	QVMClearVariant(&result);
	return hr;
}

struct WaitBarrier
{
	HANDLE entered;
	HANDLE finish;
};
static DWORD WINAPI FinishWaitWorker(PVOID data)
{
	WaitBarrier* barrier = static_cast<WaitBarrier*>(data);
	DWORD result = WaitForSingleObject(barrier->entered,5000);
	SetEvent(barrier->finish);
	return result == WAIT_OBJECT_0 ? 0 : 1;
}

struct WaitThreadCall
{
	IQuadooObject* wrapper;
	IQuadooObject* source;
	IQuadooObject* callback;
	HRESULT result;
};
static DWORD WINAPI WaitWrongThread(PVOID data)
{
	WaitThreadCall* call = static_cast<WaitThreadCall*>(data);
	call->result = AsyncWaitEnqueue(call->wrapper,call->source,call->callback);
	return 0;
}

static BOOL NoWaitResources()
{
	AQVM::AsyncWaitTestHooks& hooks = AQVM::AsyncWaitTests();
	return !hooks.waits && !hooks.handles && !hooks.registrations &&
		!AQVM::AsyncTests().requests && !AQVM::AsyncTests().targets;
}

static HRESULT PumpAsyncWaits()
{
	HRESULT hr = S_OK;
	DWORD started = GetTickCount();
	while(!NoWaitResources())
	{
		MSG message;
		CheckIf(GetTickCount() - started > 10000,HRESULT_FROM_WIN32(ERROR_TIMEOUT));
		if(PeekMessageW(&message,NULL,0,0,PM_REMOVE))
		{
			if(message.message == WM_QUIT)
			{
				PostQuitMessage(static_cast<INT>(message.wParam));
				Check(E_UNEXPECTED);
			}
			DispatchMessageW(&message);
		}
		else
		{
			MsgWaitForMultipleObjects(0,NULL,FALSE,50,QS_ALLINPUT);
		}
	}
Cleanup:
	return hr;
}

static HRESULT NativeWaitStateTests()
{
	HRESULT hr;
	WaitProbeCounts counts;
	TStackRef<CWaitProbe> source;
	TStackRef<AQVM::CAsyncWait> wait;
	HANDLE posted = CreateEventW(NULL,TRUE,FALSE,NULL);
	AQVM::AsyncDelivery delivery = {NULL,WM_APP,1,0,ERROR_ACCESS_DENIED};
	Stage("AsyncWait native state, without HWNDs or message dispatch.\r\n");
	CheckIf(!posted,HrEnsureLastError());
	source.Attach(__new CWaitProbe(&counts));
	wait.Attach(__new AQVM::CAsyncWait);
	CheckAlloc(source);
	CheckAlloc(wait);
	Check(source->Initialize(TRUE,TRUE));
	AQVM::AsyncWaitTests().posted = posted;
	AQVM::AsyncWaitTests().waitBeforeRegisterReturn = TRUE;
	Check(wait->Initialize(source,delivery));
	Check(wait->Register());
	REQUIRE(wait->Fired());
	Check(wait->Drain());
	REQUIRE(wait->Failed() && wait->PostResult() == E_ACCESSDENIED);
	REQUIRE(!AQVM::AsyncWaitTests().handles && !AQVM::AsyncWaitTests().registrations);
	wait->ReleaseSource();
	wait.Release();
	QVMReleaseObject(source.Detach());
	REQUIRE(counts.correctThread && !counts.calls && counts.destructed == 1 && counts.deleted == 1);
	REQUIRE(NoWaitResources());
Cleanup:
	AQVM::AsyncWaitTests().posted = NULL;
	AQVM::AsyncWaitTests().waitBeforeRegisterReturn = FALSE;
	if(wait)
	{
		wait->Cancel();
		wait->Drain();
		wait->ReleaseSource();
	}
	if(posted)
	{
		CloseHandle(posted);
	}
	return hr;
}

static HRESULT NativeAsyncWaitTests()
{
	HRESULT hr;
	AQVM::AsyncWaitTestHooks& hooks = AQVM::AsyncWaitTests();
	HANDLE posted = CreateEventW(NULL,TRUE,FALSE,NULL);
	HANDLE entered = CreateEventW(NULL,TRUE,FALSE,NULL);
	HANDLE finish = CreateEventW(NULL,TRUE,FALSE,NULL);
	HANDLE worker = NULL;
	WaitBarrier barrier = {entered,finish};
	CheckIf(!posted || !entered || !finish,HrEnsureLastError());
	Stage("AsyncWait early notification, opaque pointer validation and worker drain race.\r\n");
	{
		WaitProbeCounts counts;
		CAsyncFixture fixture;
		TStackRef<CWaitProbe> probe;
		Check(fixture.Initialize());
		probe.Attach(__new CWaitProbe(&counts));
		CheckAlloc(probe);
		Check(probe->Initialize());
		hooks.posted = posted;
		hooks.finish = finish;
		hooks.drainEntered = entered;
		Check(AsyncWaitEnqueue(fixture.wrapper,probe,probe));
		HWND window;
		Check(fixture.container->GetHandle(&window));
		WNDPROC procedure = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(window,GWLP_WNDPROC));
		// Capture the real ticket by letting the worker post but not finish.
		CheckIf(!SetEvent(probe->Handle()),HrEnsureLastError());
		CheckIf(WaitForSingleObject(posted,5000) != WAIT_OBJECT_0,E_FAIL);
		MSG message;
		REQUIRE(PeekMessageW(&message,window,QuadooForm::ContainerCallbackMessage(),QuadooForm::ContainerCallbackMessage(),PM_NOREMOVE));
		REQUIRE(message.lParam == reinterpret_cast<LPARAM>(static_cast<IQuadooObject*>(probe)));
		worker = CreateThread(NULL,0,FinishWaitWorker,&barrier,0,NULL);
		CheckIf(!worker,HrEnsureLastError());
		Check(DrainAsync());
		CheckIf(WaitForSingleObject(worker,5000) != WAIT_OBJECT_0,E_FAIL);
		CloseHandle(worker);
		worker = NULL;
		REQUIRE(counts.calls == 1 && counts.arguments && counts.correctThread);
		REQUIRE(procedure == reinterpret_cast<WNDPROC>(GetWindowLongPtrW(window,GWLP_WNDPROC)));
		DispatchMessageW(&message);
		REQUIRE(counts.calls == 1 && NoWaitResources());
		hooks.finish = NULL;
		hooks.drainEntered = NULL;
		ResetEvent(posted);
		ResetEvent(probe->Handle());
		Check(AsyncWaitEnqueue(fixture.wrapper,probe,probe));
		// Even an exact current ticket/pointer must not dispatch an unfired wait.
		message.wParam = hooks.lastTicket;
		DispatchMessageW(&message);
		REQUIRE(counts.calls == 1 && AQVM::AsyncTests().requests == 1);
		Check(fixture.container->Destroy());
		REQUIRE(NoWaitResources() && counts.calls == 1);
	}
	Stage("AsyncWait retained objects, original-handle closure and one-shot delivery.\r\n");
	for(UINT manual = 0; manual < 2; manual++)
	{
		WaitProbeCounts sourceCounts, callbackCounts;
		CAsyncFixture fixture;
		TStackRef<CWaitProbe> source, callback;
		HANDLE signal = NULL;
		Check(fixture.Initialize());
		source.Attach(__new CWaitProbe(&sourceCounts));
		callback.Attach(__new CWaitProbe(&callbackCounts));
		CheckAlloc(source);
		CheckAlloc(callback);
		Check(source->Initialize(manual != 0));
		CheckIf(!DuplicateHandle(GetCurrentProcess(),source->Handle(),GetCurrentProcess(),&signal,EVENT_MODIFY_STATE,FALSE,0),HrEnsureLastError());
		ResetEvent(posted);
		Check(AsyncWaitEnqueue(fixture.wrapper,source,callback));
		source->Close();
		source.Release();
		callback.Release();
		REQUIRE(!sourceCounts.deleted && !callbackCounts.deleted);
		SetEvent(signal);
		SetEvent(signal);
		CheckIf(WaitForSingleObject(posted,5000) != WAIT_OBJECT_0,E_FAIL);
		CloseHandle(signal);
		Check(PumpAsyncWaits());
		REQUIRE(callbackCounts.calls == 1 && callbackCounts.arguments && callbackCounts.correctThread);
		REQUIRE(sourceCounts.deleted == 1 && sourceCounts.destructed == 1 && sourceCounts.correctThread);
		REQUIRE(callbackCounts.deleted == 1 && callbackCounts.destructed == 1);
	}
	Stage("AsyncWait rollback failures and reentrant provider destruction.\r\n");
	for(UINT failure = 0; failure < 9; failure++)
	{
		WaitProbeCounts counts;
		CAsyncFixture fixture;
		TStackRef<CWaitProbe> probe;
		Check(fixture.Initialize());
		probe.Attach(__new CWaitProbe(&counts));
		CheckAlloc(probe);
		Check(probe->Initialize());
		switch(failure)
		{
		case 0:
			probe->queryError = E_NOINTERFACE;
			break;
		case 1:
			probe->queryError = S_FALSE;
			break;
		case 2:
			probe->useHandleOverride = TRUE;
			probe->handleOverride = NULL;
			break;
		case 3:
			hooks.duplicateError = ERROR_ACCESS_DENIED;
			break;
		case 4:
			hooks.registerError = ERROR_NOT_ENOUGH_MEMORY;
			break;
		case 5:
			AQVM::AsyncTests().failTicket = TRUE;
			break;
		case 6:
			AQVM::AsyncTests().failAllocation = 3;
			break;
		case 7:
			probe->destroy = fixture.container;
			break;
		case 8:
			probe->useHandleOverride = TRUE;
			probe->handleOverride = reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(0x7ffffffc));
			break;
		}
		REQUIRE(FAILED(AsyncWaitEnqueue(fixture.wrapper,probe,probe)));
		hooks.duplicateError = 0;
		hooks.registerError = 0;
		AQVM::AsyncTests().failTicket = FALSE;
		AQVM::AsyncTests().failAllocation = -1;
		REQUIRE(NoWaitResources());
		REQUIRE(!counts.calls && counts.correctThread);
	}
	Stage("AsyncWait allocation rollback at every preparation boundary.\r\n");
	for(INT allocation = 0; allocation < 5; allocation++)
	{
		WaitProbeCounts counts;
		CAsyncFixture fixture;
		TStackRef<CWaitProbe> probe;
		Check(fixture.Initialize());
		probe.Attach(__new CWaitProbe(&counts));
		CheckAlloc(probe);
		Check(probe->Initialize());
		AQVM::AsyncTests().failAllocation = allocation;
		REQUIRE(AsyncWaitEnqueue(fixture.wrapper,probe,probe) == E_OUTOFMEMORY);
		AQVM::AsyncTests().failAllocation = -1;
		REQUIRE(NoWaitResources() && counts.correctThread);
	}
	Stage("AsyncWait multiple registrations, wrong thread and unsupported container.\r\n");
	{
		WaitProbeCounts counts;
		CAsyncFixture fixture;
		TStackRef<CWaitProbe> probe;
		TStackRef<CUnsupportedAsyncContainer> unsupported;
		TStackRef<IQuadooObject> unsupportedWrapper;
		Check(fixture.Initialize());
		probe.Attach(__new CWaitProbe(&counts));
		CheckAlloc(probe);
		Check(probe->Initialize());
		WaitThreadCall call = {fixture.wrapper,probe,probe,E_PENDING};
		worker = CreateThread(NULL,0,WaitWrongThread,&call,0,NULL);
		CheckIf(!worker,HrEnsureLastError());
		CheckIf(WaitForSingleObject(worker,5000) != WAIT_OBJECT_0,E_FAIL);
		CloseHandle(worker);
		worker = NULL;
		REQUIRE(call.result == QuadooForm::EWrongThread && counts.correctThread && NoWaitResources());
		unsupported.Attach(__new CUnsupportedAsyncContainer);
		CheckAlloc(unsupported);
		Check(unsupported->Initialize(fixture.context));
		Check(fixture.binding->Wrap(unsupported,&unsupportedWrapper));
		REQUIRE(AsyncWaitEnqueue(unsupportedWrapper,probe,probe) == E_NOINTERFACE);
		Check(unsupported->Destroy());
		probe->useHandleOverride = TRUE;
		REQUIRE(AsyncWaitEnqueue(fixture.wrapper,probe,probe) == HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE));
		probe->useHandleOverride = FALSE;
		ResetEvent(posted);
		Check(AsyncWaitEnqueue(fixture.wrapper,probe,probe));
		Check(AsyncWaitEnqueue(fixture.wrapper,probe,probe));
		REQUIRE(hooks.handles == 2 && hooks.registrations == 2);
		SetEvent(probe->Handle());
		Check(PumpAsyncWaits());
		REQUIRE(counts.calls == 2 && counts.correctThread && counts.arguments && NoWaitResources());
	}
	Stage("AsyncWait failed post and unregister retry use independent UI maintenance.\r\n");
	for(UINT retry = 0; retry < 2; retry++)
	{
		WaitProbeCounts counts;
		CAsyncFixture fixture;
		TStackRef<CWaitProbe> probe;
		Check(fixture.Initialize());
		probe.Attach(__new CWaitProbe(&counts));
		CheckAlloc(probe);
		Check(probe->Initialize(TRUE,retry == 0));
		hooks.postError = retry ? 0 : ERROR_NOT_ENOUGH_QUOTA;
		Check(AsyncWaitEnqueue(fixture.wrapper,probe,probe));
		hooks.postError = 0;
		probe.Release();
		if(retry)
		{
			hooks.unregisterFailures = 1;
			Check(fixture.container->Destroy());
			REQUIRE(NoWaitResources());
		}
		Check(PumpAsyncWaits());
		REQUIRE(!counts.calls && counts.destructed == 1 && counts.deleted == 1 && counts.correctThread);
		REQUIRE(SUCCEEDED(fixture.runtime->Error()) && RStrLen(fixture.runtime->Diagnostic()));
	}
	Stage("AsyncWait cancellation, detach, shutdown, handle replacement and repeated cleanup.\r\n");
	for(UINT mode = 0; mode < 30; mode++)
	{
		WaitProbeCounts counts;
		CAsyncFixture fixture;
		TStackRef<CWaitProbe> probe;
		Check(fixture.Initialize());
		probe.Attach(__new CWaitProbe(&counts));
		CheckAlloc(probe);
		Check(probe->Initialize(TRUE,mode % 5 == 4));
		ResetEvent(posted);
		hooks.waitBeforeRegisterReturn = mode % 5 == 4;
		Check(AsyncWaitEnqueue(fixture.wrapper,probe,probe));
		hooks.waitBeforeRegisterReturn = FALSE;
		probe.Release();
		switch(mode % 5)
		{
		case 0:
			Check(fixture.container->Destroy());
			break;
		case 1:
			fixture.binding->Detach();
			break;
		case 2:
			fixture.runtime->Shutdown();
			break;
		case 3:
			Check(fixture.container->ReplaceWindow());
			break;
		case 4:
			Check(fixture.container->Destroy());
			break;
		}
		Check(DrainAsync());
		REQUIRE(counts.destructed == 1 && counts.deleted == 1 && !counts.calls && counts.correctThread);
		if(mode % 5 == 3)
		{
			// A replaced HWND retains the closed lifecycle endpoint until the
			// next sweep/binding detach, just as ordinary Async does.
			fixture.binding->Detach();
		}
		REQUIRE(NoWaitResources());
	}
	Stage("AsyncWait reentrant modal delivery, diagnostic continuation and owner restoration.\r\n");
	{
		WaitProbeCounts sourceCounts;
		AsyncProbeCounts showCounts, failedCounts, closeCounts;
		CAsyncFixture fixture;
		TStackRef<CWaitProbe> source;
		TStackRef<CAsyncProbe> show, fail, close;
		TStackRef<IJSONObject> definition;
		TStackRef<IQuadooForm> owner, modal;
		TStackRef<IQuadooWinControl> ownerControl;
		HWND ownerWindow;
		BOOL disabled = FALSE;
		Check(fixture.Initialize());
		Check(AQVM::ParseObject(SLP(L"{\"type\":\"Form\",\"name\":\"AsyncWaitModal\"}"),&definition));
		Check(QuadooCreateForm(NULL,NULL,definition,&owner));
		Check(owner->QueryInterface(&ownerControl));
		Check(owner->GetHandle(&ownerWindow));
		Check(owner->ShowForm(SW_SHOWNOACTIVATE));
		Check(QuadooCreateForm(NULL,ownerControl,definition,&modal));
		source.Attach(__new CWaitProbe(&sourceCounts));
		show.Attach(__new CAsyncProbe(&showCounts));
		fail.Attach(__new CAsyncProbe(&failedCounts));
		close.Attach(__new CAsyncProbe(&closeCounts));
		CheckAlloc(source);
		CheckAlloc(show);
		CheckAlloc(fail);
		CheckAlloc(close);
		Check(source->Initialize(TRUE,TRUE));
		show->modal = modal;
		fail->failure = E_ACCESSDENIED;
		close->destroy = modal;
		close->ownerWindow = ownerWindow;
		close->ownerDisabled = &disabled;
		Check(AsyncEnqueue(fixture.wrapper,show));
		hooks.waitBeforeRegisterReturn = TRUE;
		ResetEvent(posted);
		Check(AsyncWaitEnqueue(fixture.wrapper,source,fail));
		ResetEvent(posted);
		Check(AsyncWaitEnqueue(fixture.wrapper,source,close));
		hooks.waitBeforeRegisterReturn = FALSE;
		Check(PumpAsyncWaits());
		REQUIRE(showCounts.calls == 1 && failedCounts.calls == 1 && closeCounts.calls == 1);
		REQUIRE(disabled && IsWindowEnabled(ownerWindow) && SUCCEEDED(fixture.runtime->Error()));
		REQUIRE(RStrLen(fixture.runtime->Diagnostic()) && NoWaitResources());
		Check(owner->Destroy());
		Check(modal->Destroy());
	}
	Stage("AsyncWait self-destruction cancels its pending sibling and releases last-Form leases.\r\n");
	{
		WaitProbeCounts sourceCounts;
		AsyncProbeCounts firstCounts, siblingCounts;
		CAsyncFixture fixture;
		TStackRef<CWaitProbe> source;
		TStackRef<CAsyncProbe> first, sibling;
		BOOL run;
		UINT quits = 0;
		INT code = -1;
		Check(fixture.Initialize());
		Check(fixture.runtime->RegisterForm(fixture.container));
		source.Attach(__new CWaitProbe(&sourceCounts));
		first.Attach(__new CAsyncProbe(&firstCounts));
		sibling.Attach(__new CAsyncProbe(&siblingCounts));
		CheckAlloc(source);
		CheckAlloc(first);
		CheckAlloc(sibling);
		Check(source->Initialize(TRUE,TRUE));
		first->destroy = fixture.container;
		hooks.waitBeforeRegisterReturn = TRUE;
		ResetEvent(posted);
		Check(AsyncWaitEnqueue(fixture.wrapper,source,first));
		ResetEvent(posted);
		Check(AsyncWaitEnqueue(fixture.wrapper,source,sibling));
		hooks.waitBeforeRegisterReturn = FALSE;
		Check(fixture.runtime->BeginMessageLoop(&run));
		REQUIRE(run);
		Check(DrainLifetime(&quits,&code));
		REQUIRE(quits == 1 && code == 0 && firstCounts.calls == 1 && !siblingCounts.calls);
		REQUIRE(NoWaitResources());
		fixture.runtime->EndMessageLoop();
	}
	Stage("AsyncWait argument contract and default invocation.\r\n");
	{
		CAsyncFixture fixture;
		QuadooVM::QVARIANT values[2] = {{QuadooVM::Null},{QuadooVM::Null}};
		QuadooVM::QVARIANT result = {QuadooVM::Null};
		QuadooVM::QVPARAMS parameters = {values,0};
		Check(fixture.Initialize());
		REQUIRE(fixture.wrapper->Invoke(NULL,RSTRING_CAST(L"AsyncWait"),&parameters,&result) == DISP_E_BADPARAMCOUNT);
		parameters.cArgs = 2;
		REQUIRE(fixture.wrapper->Invoke(NULL,RSTRING_CAST(L"AsyncWait"),&parameters,&result) == DISP_E_TYPEMISMATCH);
		values[0].eType = QuadooVM::Object;
		values[1].eType = QuadooVM::Object;
		REQUIRE(fixture.wrapper->Invoke(NULL,RSTRING_CAST(L"AsyncWait"),&parameters,&result) == E_INVALIDARG);
		parameters.pqvArgs = NULL;
		REQUIRE(fixture.wrapper->Invoke(NULL,RSTRING_CAST(L"AsyncWait"),&parameters,&result) == E_POINTER);
		REQUIRE(NoWaitResources());
	}
	hr = S_OK;
Cleanup:
	SetEvent(finish);
	if(worker)
	{
		WaitForSingleObject(worker,5000);
		CloseHandle(worker);
	}
	hooks.posted = NULL;
	hooks.finish = NULL;
	hooks.drainEntered = NULL;
	hooks.waitBeforeRegisterReturn = FALSE;
	hooks.duplicateError = 0;
	hooks.registerError = 0;
	hooks.postError = 0;
	hooks.unregisterFailures = 0;
	AQVM::AsyncTests().failTicket = FALSE;
	AQVM::AsyncTests().failAllocation = -1;
	if(posted)
	{
		CloseHandle(posted);
	}
	if(entered)
	{
		CloseHandle(entered);
	}
	if(finish)
	{
		CloseHandle(finish);
	}
	return hr;
}

static HRESULT ScriptAsyncWaitTests(PCWSTR path)
{
	HRESULT hr;
	HRESULT registered;
	TStackRef<CAQVMHost> host;
	TStackRef<IQuadooInstanceLoader> loader;
	TStackRef<IQuadooVM> vm;
	TStackRef<IQuadooObject> exception;
	TStackRef<IJSONObject> catalog;
	CHostedModules modules;
	CMemoryStream code;
	CCompilerStatus compiler;
	QuadooVM::QVARIANT value = {QuadooVM::Null};
	QuadooVM::QVARIANT form = {QuadooVM::Null};
	static const WCHAR text[] = L"{\"schemaVersion\":2,\"startupClass\":\"AsyncForm\",\"units\":["
		L"{\"class\":\"AsyncForm\",\"kind\":\"form\",\"eventsInterface\":\"IAsyncForm\",\"unitId\":\"{F7000000-0000-0000-0000-000000000001}\"},"
		L"{\"class\":\"AsyncFrame\",\"kind\":\"frame\",\"eventsInterface\":\"IAsyncFrame\",\"unitId\":\"{F7000000-0000-0000-0000-000000000002}\"}]}";
	Stage("AsyncWait real VM event, Form/Frame/container callables, captures and cancellation.\r\n");
	Check(AQVM::ParseObject(SLP(text),&catalog));
	Check(QuadooParseToStream(path,0,&code,NULL,&compiler));
	host.Attach(__new CAQVMHost);
	CheckAlloc(host);
	Check(host->Initialize(&modules,RSTRING_CAST(path),0,NULL));
	Check(host->InitializeApplication(GetModuleHandleW(NULL),catalog));
	Check(QVMCreateLoader(NULL,&loader));
	host->AttachLoader(loader);
	Check(loader->AddInstance(RSTRING_CAST(L"AsyncWaitTests"),NULL,&code,code.DataRemaining(),NULL));
	Check(loader->LoadVM(RSTRING_CAST(L"AsyncWaitTests"),&registered,&vm));
	Check(vm->AddGlobal(RSTRING_CAST(L"Host"),host));
	Check(vm->AddExternalClassLoader(host));
	Check(vm->RunConstructor(&exception));
	REQUIRE(!exception);
	Check(host->Runtime()->BeginScriptEntry());
	Check(Call(vm,L"AsyncMake",&form));
	REQUIRE(form.eType == QuadooVM::Object);
	Check(Call(vm,L"WaitQueue",&value));
	if(value.eType == QuadooVM::Object)
	{
		QuadooVM::QVARIANT description = {QuadooVM::Null};
		QuadooVM::QVPARAMS empty = {0};
		if(SUCCEEDED(value.pObject->Invoke(NULL,RSTRING_CAST(L"ToString"),&empty,&description)) && description.eType == QuadooVM::String)
		{
			CHAR text[2048];
			WideCharToMultiByte(CP_UTF8,0,RStrToWide(description.rstrVal),-1,text,ARRAYSIZE(text),NULL,NULL);
			Stage(text);
			Stage("\r\n");
		}
		QVMClearVariant(&description);
	}
	REQUIRE(value.eType == QuadooVM::I4 && value.lVal == 0);
	QVMClearVariant(&value);
	Check(Call(vm,L"WaitCounts",&value));
	REQUIRE(value.eType == QuadooVM::I4 && value.lVal == 0);
	QVMClearVariant(&value);
	Check(Call(vm,L"WaitSignal",&value));
	QVMClearVariant(&value);
	Check(PumpAsyncWaits());
	Check(Call(vm,L"WaitCounts",&value));
	REQUIRE(value.eType == QuadooVM::I4 && value.lVal == 301);
	QVMClearVariant(&value);
	Check(Call(vm,L"WaitThrow",&value));
	QVMClearVariant(&value);
	Check(PumpAsyncWaits());
	REQUIRE(SUCCEEDED(host->Runtime()->Error()) && wcsstr(RStrToWide(host->Runtime()->Diagnostic()),L"Deliberate AsyncWait callback failure"));
	Check(Call(vm,L"WaitCounts",&value));
	REQUIRE(value.eType == QuadooVM::I4 && value.lVal == 401);
	QVMClearVariant(&value);
	Check(Call(vm,L"WaitCancel",&value));
	QVMClearVariant(&value);
	Check(PumpAsyncWaits());
	Check(Call(vm,L"WaitCounts",&value));
	REQUIRE(value.eType == QuadooVM::I4 && value.lVal == 402);
	REQUIRE(NoWaitResources());
Cleanup:
	if(host)
	{
		host->ShutdownApplication();
	}
	QVMClearVariant(&value);
	QVMClearVariant(&form);
	exception.Release();
	if(vm)
	{
		vm->Unload();
	}
	if(host)
	{
		host->UnloadModules();
	}
	return hr;
}

#include "AsyncWaitNotificationTests.h"

static HRESULT AsyncWaitTests(PCWSTR path)
{
	HRESULT hr;
	Check(NativeWaitStateTests());
	Check(NativeAsyncWaitTests());
	Check(NativeAsyncWaitNotificationTests());
	Check(ScriptAsyncWaitTests(path));
Cleanup:
	return hr;
}
