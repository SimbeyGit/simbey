#include "..\..\..\..\platform\QuadooForm\ScriptAdapter\AsyncCalls.h"

// Native test containers use only the published QuadooForm interfaces. The
// independent lifetime-test base supplies ABI stubs, not runtime implementation.
class CIndependentAsyncContainer : public CIndependentLifetimeForm, public IQuadooContainerMessageTarget
{
	HWND m_asyncWindow;
	BOOL m_ownsWindow;
	BOOL m_finalizing;
	TStackRef<IQuadooConstructionContext> m_context;
	static LRESULT CALLBACK Procedure(HWND window, UINT message, WPARAM wp, LPARAM lp)
	{
		CIndependentAsyncContainer* object = reinterpret_cast<CIndependentAsyncContainer*>(GetWindowLongPtrW(window,GWLP_USERDATA));
		if(message == WM_NCCREATE)
		{
			object = static_cast<CIndependentAsyncContainer*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
			SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(object));
		}
		TStackRef<CIndependentAsyncContainer> keepAlive;
		if(object && !object->m_finalizing)
		{
			keepAlive = object;
		}
		BOOL handled = FALSE;
		LRESULT result = 0;
		if(object && (message == WM_NCDESTROY || message == QuadooForm::ContainerCallbackMessage()))
		{
			QuadooForm::ForwardContainerMessage(object->m_context,window,message,wp,lp,&handled,&result);
		}
		if(!handled)
		{
			result = DefWindowProcW(window,message,wp,lp);
		}
		if(message == WM_NCDESTROY)
		{
			SetWindowLongPtrW(window,GWLP_USERDATA,0);
		}
		return result;
	}
public:
	IMP_UNKNOWN(CIndependentLifetimeForm)
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooContainerMessageTarget)
		UNK_CHAIN_TO(CIndependentLifetimeForm)
	END_UNK_MAP
	CIndependentAsyncContainer() : m_asyncWindow(NULL), m_ownsWindow(TRUE), m_finalizing(FALSE)
	{
	}
	~CIndependentAsyncContainer()
	{
		m_finalizing = TRUE;
		Destroy();
	}
	HRESULT Initialize(IQuadooConstructionContext* context, HWND shared = NULL)
	{
		HRESULT hr = S_OK;
		WNDCLASSW type = {0};
		m_context = context;
		if(shared)
		{
			m_asyncWindow = shared;
			m_ownsWindow = FALSE;
			goto Cleanup;
		}
		type.lpfnWndProc = Procedure;
		type.hInstance = GetModuleHandleW(NULL);
		type.lpszClassName = L"Quadoo.Async.IndependentTestContainer";
		CheckIf(!RegisterClassW(&type) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS,HrEnsureLastError());
		m_asyncWindow = CreateWindowExW(0,type.lpszClassName,L"",WS_OVERLAPPEDWINDOW,0,0,160,100,NULL,NULL,type.hInstance,this);
		CheckIf(!m_asyncWindow,HrEnsureLastError());
	Cleanup:
		return hr;
	}
	HRESULT ReplaceWindow()
	{
		HRESULT hr;
		HWND previous = m_asyncWindow;
		hr = Initialize(m_context);
		if(SUCCEEDED(hr))
		{
			DestroyWindow(previous);
		}
		return hr;
	}
	STDMETHOD(GetHandle)(HWND* window)
	{
		if(!window)
		{
			return E_POINTER;
		}
		*window = m_asyncWindow;
		return IsWindow(m_asyncWindow) ? S_OK : QuadooForm::EDestroyed;
	}
	STDMETHOD(GetName)(RSTRING* name)
	{
		return RStrCreateW(LSP(L"IndependentAsync"),name);
	}
	STDMETHOD(Destroy)()
	{
		if(m_ownsWindow && IsWindow(m_asyncWindow))
		{
			DestroyWindow(m_asyncWindow);
		}
		m_asyncWindow = NULL;
		return CIndependentLifetimeForm::Destroy();
	}
};

class CUnsupportedAsyncContainer : public CIndependentAsyncContainer
{
public:
	IMP_UNKNOWN(CIndependentAsyncContainer)
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooForm)
		UNK_INTERFACE_PATH(IQuadooControlContainer,IQuadooForm)
		UNK_INTERFACE_PATH(IQuadooWinControl,IQuadooForm)
		UNK_INTERFACE_PATH(IQuadooControl,IQuadooForm)
		UNK_INTERFACE_PATH(IQuadooComponent,IQuadooForm)
		UNK_INTERFACE(IQuadooComponentLifecycle)
	END_UNK_MAP
};

class CUnobservedAsyncContainer : public CIndependentAsyncContainer
{
public:
	IMP_UNKNOWN(CIndependentAsyncContainer)
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooForm)
		UNK_INTERFACE_PATH(IQuadooControlContainer,IQuadooForm)
		UNK_INTERFACE_PATH(IQuadooWinControl,IQuadooForm)
		UNK_INTERFACE_PATH(IQuadooControl,IQuadooForm)
		UNK_INTERFACE_PATH(IQuadooComponent,IQuadooForm)
		UNK_INTERFACE(IQuadooContainerMessageTarget)
	END_UNK_MAP
};

class CAsyncFixtureScript : public CBaseUnknown, public CQuadooObjectImpl, public IQuadooInterface
{
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooObject)
		UNK_INTERFACE(IQuadooInterface)
	END_UNK_MAP
	STDMETHOD(GetInterface)(RSTRING, IQuadooInterface** methods)
	{
		*methods = this;
		AddRef();
		return S_OK;
	}
	STDMETHOD(ResolveMethod)(RSTRING, ULONG* ordinal)
	{
		*ordinal = 0;
		return S_OK;
	}
	STDMETHOD(Invoke)(ULONG, QuadooVM::QVPARAMS*, QuadooVM::QVARIANT* result)
	{
		result->eType = QuadooVM::Null;
		return S_OK;
	}
};

class CAsyncFixture
{
public:
	TStackRef<AQVM::CApplicationRuntime> runtime;
	TStackRef<CAsyncFixtureScript> script;
	TStackRef<AQVM::CFormInstance> binding;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<CIndependentAsyncContainer> container;
	TStackRef<IQuadooObject> wrapper;
	~CAsyncFixture()
	{
		if(binding)
		{
			binding->Detach();
		}
		if(container)
		{
			container->Destroy();
		}
		wrapper.Release();
		context.Release();
		binding.Release();
		script.Release();
		if(runtime)
		{
			runtime->EndScriptEntry();
			runtime->Shutdown();
		}
	}
	HRESULT Initialize()
	{
		HRESULT hr;
		TStackRef<IJSONObject> catalog;
		TStackRef<IJSONObject> unit;
		TStackRef<IJSONObject> definition;
		QuadooForm::ContextOptions options = {0};
		static const WCHAR text[] = L"{\"schemaVersion\":2,\"startupClass\":\"AsyncForm\",\"units\":[{\"class\":\"AsyncForm\",\"kind\":\"form\",\"eventsInterface\":\"IAsyncForm\",\"unitId\":\"{F7000000-0000-0000-0000-000000000001}\"}]}";
		Check(AQVM::ParseObject(SLP(text),&catalog));
		Check(AQVM::ParseObject(SLP(L"{\"kind\":\"form\",\"eventsInterface\":\"IAsyncForm\"}"),&unit));
		Check(AQVM::ParseObject(SLP(L"{}"),&definition));
		runtime.Attach(__new AQVM::CApplicationRuntime);
		script.Attach(__new CAsyncFixtureScript);
		CheckAlloc(runtime);
		CheckAlloc(script);
		Check(runtime->Initialize(GetModuleHandleW(NULL),catalog));
		Check(runtime->BeginScriptEntry());
		binding.Attach(__new AQVM::CFormInstance(runtime,script));
		CheckAlloc(binding);
		Check(binding->Initialize(unit,definition));
		options.cbSize = sizeof(options);
		options.DPI = 96;
		options.pBinder = binding;
		options.pReport = runtime;
		Check(QuadooCreateConstructionContext(&options,&context));
		container.Attach(__new CIndependentAsyncContainer);
		CheckAlloc(container);
		Check(container->Initialize(context));
		binding->Root()->AttachComponent(container);
		Check(binding->Wrap(container,&wrapper));
	Cleanup:
		return hr;
	}
};

static HRESULT AsyncEnqueue(IQuadooObject* wrapper, IQuadooObject* callback)
{
	HRESULT hr;
	QuadooVM::QVARIANT argument = {QuadooVM::Object};
	QuadooVM::QVARIANT result = {QuadooVM::Null};
	QuadooVM::QVPARAMS parameters = {&argument,1};
	argument.pObject = callback;
	hr = wrapper->Invoke(NULL,RSTRING_CAST(L"Async"),&parameters,&result);
	QVMClearVariant(&result);
	return hr;
}
static HRESULT DrainAsync()
{
	HRESULT hr = S_OK;
	MSG message;
	UINT dispatched = 0;
	UINT id = QuadooForm::ContainerCallbackMessage();
	while(PeekMessageW(&message,NULL,id,id,PM_REMOVE))
	{
		CheckIf(++dispatched > 2000,E_UNEXPECTED);
		if(message.message == WM_QUIT)
		{
			PostQuitMessage(static_cast<INT>(message.wParam));
			break;
		}
		DispatchMessageW(&message);
	}
Cleanup:
	return hr;
}

struct AsyncProbeCounts
{
	UINT calls;
	UINT destructed;
	UINT deleted;
	BOOL arguments;
	AsyncProbeCounts() : calls(0), destructed(0), deleted(0), arguments(TRUE)
	{
	}
};
class CAsyncProbe : public CBaseUnknown, public CQuadooObjectImpl
{
	AsyncProbeCounts* m_counts;
public:
	HRESULT failure;
	BOOL returnObject;
	BOOL pump;
	BOOL quit;
	INT quitCode;
	HWND ownerWindow;
	BOOL* ownerDisabled;
	TStackRef<IQuadooComponent> destroy;
	TStackRef<IQuadooForm> modal;
	TStackRef<IQuadooObject> target;
	TStackRef<IQuadooObject> next;
	AQVM::CApplicationRuntime* runtime;
	TStackRef<IQuadooForm> registerForm;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooObject)
	END_UNK_MAP
	CAsyncProbe(AsyncProbeCounts* counts) : m_counts(counts), failure(S_OK), returnObject(FALSE), pump(FALSE),
		quit(FALSE), quitCode(0), ownerWindow(NULL), ownerDisabled(NULL), runtime(NULL)
	{
	}
	~CAsyncProbe()
	{
		m_counts->deleted++;
	}
	STDMETHOD(Destruct)()
	{
		m_counts->destructed++;
		return S_FALSE;
	}
	STDMETHOD(Invoke)(IQuadooVM* vm, RSTRING method, QuadooVM::QVPARAMS* parameters, QuadooVM::QVARIANT* result)
	{
		HRESULT hr = S_OK;
		m_counts->calls++;
		m_counts->arguments = m_counts->arguments && !vm && !method && parameters && !parameters->cArgs && !parameters->pqvArgs;
		result->eType = QuadooVM::Null;
		if(ownerDisabled)
		{
			*ownerDisabled = IsWindow(ownerWindow) && !IsWindowEnabled(ownerWindow);
		}
		if(next)
		{
			Check(AsyncEnqueue(target,next));
		}
		if(destroy)
		{
			Check(destroy->Destroy());
		}
		if(registerForm)
		{
			Check(runtime->RegisterForm(registerForm));
		}
		if(pump)
		{
			Check(DrainAsync());
		}
		if(modal)
		{
			LRESULT modalResult;
			Check(modal->ShowModal(&modalResult));
		}
		if(returnObject)
		{
			result->eType = QuadooVM::Object;
			result->pObject = this;
			AddRef();
		}
		if(quit)
		{
			PostQuitMessage(quitCode);
		}
		hr = failure;
	Cleanup:
		return hr;
	}
};

struct AsyncThreadCall
{
	IQuadooObject* wrapper;
	IQuadooObject* callback;
	HRESULT result;
};
static DWORD WINAPI AsyncWrongThread(PVOID data)
{
	AsyncThreadCall* call = static_cast<AsyncThreadCall*>(data);
	call->result = AsyncEnqueue(call->wrapper,call->callback);
	return 0;
}

static HRESULT NativeAsyncTests()
{
	HRESULT hr;
	Stage("Async native forwarding, tickets and object ownership.\r\n");
	{
		AsyncProbeCounts counts;
		CAsyncFixture fixture;
		TStackRef<CAsyncProbe> probe;
		HWND window;
		MSG message;
		Check(fixture.Initialize());
		Check(fixture.container->GetHandle(&window));
		LONG_PTR procedure = GetWindowLongPtrW(window,GWLP_WNDPROC);
		EnableWindow(window,FALSE);
		probe.Attach(__new CAsyncProbe(&counts));
		CheckAlloc(probe);
		probe->returnObject = TRUE;
		Check(AsyncEnqueue(fixture.wrapper,probe));
		REQUIRE(!counts.calls && AQVM::AsyncTests().requests == 1);
		UINT id = QuadooForm::ContainerCallbackMessage();
		REQUIRE(PeekMessageW(&message,NULL,id,id,PM_REMOVE));
		REQUIRE(message.hwnd == window && message.wParam && message.lParam == reinterpret_cast<LPARAM>(static_cast<IQuadooObject*>(probe)));
		SendMessageW(window,id,message.wParam,1);
		SendMessageW(window,id,0,1);
		REQUIRE(!counts.calls && AQVM::AsyncTests().requests == 1);
		probe.Release();
		DispatchMessageW(&message);
		DispatchMessageW(&message);
		REQUIRE(counts.calls == 1 && counts.arguments && counts.destructed == 1 && counts.deleted == 1);
		REQUIRE(!AQVM::AsyncTests().requests && !AQVM::AsyncTests().targets && !RStrLen(fixture.runtime->Diagnostic()));
		REQUIRE(procedure == GetWindowLongPtrW(window,GWLP_WNDPROC));
		EnableWindow(window,TRUE);
		TStackRef<IQuadooContainerMessageTarget> capability;
		Check(fixture.container->QueryInterface(__uuidof(IQuadooContainerMessageTarget),reinterpret_cast<PVOID*>(&capability)));
		REQUIRE(AQVM::SameIdentity(capability,static_cast<IQuadooForm*>(fixture.container)));
	}
	Stage("Async failures, thread affinity and independent container capability.\r\n");
	{
		AsyncProbeCounts counts;
		CAsyncFixture fixture;
		TStackRef<CAsyncProbe> probe;
		TStackRef<CUnsupportedAsyncContainer> unsupported;
		TStackRef<IQuadooObject> wrapper;
		Check(fixture.Initialize());
		probe.Attach(__new CAsyncProbe(&counts));
		CheckAlloc(probe);
		for(INT n = 0; n < 4; n++)
		{
			AQVM::AsyncTests().failAllocation = n;
			REQUIRE(AsyncEnqueue(fixture.wrapper,probe) == E_OUTOFMEMORY);
			REQUIRE(!AQVM::AsyncTests().requests && !AQVM::AsyncTests().targets && !fixture.container->subscriptions);
		}
		AQVM::AsyncTests().failAllocation = -1;
		AQVM::AsyncTests().postError = ERROR_NOT_ENOUGH_QUOTA;
		REQUIRE(AsyncEnqueue(fixture.wrapper,probe) == HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_QUOTA));
		AQVM::AsyncTests().failTicket = TRUE;
		REQUIRE(AsyncEnqueue(fixture.wrapper,probe) == HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW));
		AQVM::AsyncTests().failTicket = FALSE;
		fixture.container->failRegistration = TRUE;
		REQUIRE(AsyncEnqueue(fixture.wrapper,probe) == E_OUTOFMEMORY);
		fixture.container->failRegistration = FALSE;
		REQUIRE(!fixture.container->subscriptions && !AQVM::AsyncTests().requests && !AQVM::AsyncTests().targets);
		unsupported.Attach(__new CUnsupportedAsyncContainer);
		CheckAlloc(unsupported);
		Check(unsupported->Initialize(fixture.context));
		Check(fixture.binding->Wrap(unsupported,&wrapper));
		REQUIRE(AsyncEnqueue(wrapper,probe) == E_NOINTERFACE);
		AsyncThreadCall call = {fixture.wrapper,probe,S_OK};
		HANDLE thread = CreateThread(NULL,0,AsyncWrongThread,&call,0,NULL);
		CheckIf(!thread,HrEnsureLastError());
		DWORD waited = WaitForSingleObject(thread,10000);
		CloseHandle(thread);
		REQUIRE(waited == WAIT_OBJECT_0 && call.result == QuadooForm::EWrongThread);
		QuadooVM::QVARIANT value = {QuadooVM::Null};
		QuadooVM::QVPARAMS parameters = {0};
		REQUIRE(fixture.wrapper->Invoke(NULL,RSTRING_CAST(L"Async"),&parameters,&value) == DISP_E_BADPARAMCOUNT);
		parameters.cArgs = 1;
		REQUIRE(fixture.wrapper->Invoke(NULL,RSTRING_CAST(L"Async"),&parameters,&value) == E_POINTER);
		parameters.pqvArgs = &value;
		REQUIRE(fixture.wrapper->Invoke(NULL,RSTRING_CAST(L"Async"),&parameters,&value) == DISP_E_TYPEMISMATCH);
		REQUIRE(fixture.wrapper->Invoke(NULL,RSTRING_CAST(L"Async"),NULL,&value) == E_POINTER);
		REQUIRE(fixture.wrapper->Invoke(NULL,RSTRING_CAST(L"Async"),&parameters,NULL) == E_POINTER);
		REQUIRE(AsyncEnqueue(fixture.wrapper,NULL) == E_INVALIDARG);
		Check(DrainAsync());
		probe.Release();
		REQUIRE(!counts.calls && counts.deleted == 1);
	}
	Stage("Async cancellation, HWND replacement, reentrancy and shared HWNDs.\r\n");
	{
		AsyncProbeCounts first;
		AsyncProbeCounts second;
		CAsyncFixture fixture;
		TStackRef<CAsyncProbe> a;
		TStackRef<CAsyncProbe> b;
		Check(fixture.Initialize());
		a.Attach(__new CAsyncProbe(&first));
		b.Attach(__new CAsyncProbe(&second));
		CheckAlloc(a);
		CheckAlloc(b);
		Check(AsyncEnqueue(fixture.wrapper,a));
		Check(AsyncEnqueue(fixture.wrapper,a));
		Check(fixture.container->ReplaceWindow());
		REQUIRE(!AQVM::AsyncTests().requests);
		Check(AsyncEnqueue(fixture.wrapper,b));
		Check(DrainAsync());
		REQUIRE(!first.calls && second.calls == 1 && !AQVM::AsyncTests().targets);
		a->target = fixture.wrapper;
		a->next = b;
		a->pump = TRUE;
		Check(AsyncEnqueue(fixture.wrapper,a));
		Check(DrainAsync());
		REQUIRE(first.calls == 1 && second.calls == 2);
		a->next.Release();
		a->pump = FALSE;
		HWND window;
		Check(fixture.container->GetHandle(&window));
		TStackRef<CIndependentAsyncContainer> alias;
		TStackRef<IQuadooObject> aliasWrapper;
		alias.Attach(__new CIndependentAsyncContainer);
		CheckAlloc(alias);
		Check(alias->Initialize(fixture.context,window));
		Check(fixture.binding->Wrap(alias,&aliasWrapper));
		Check(AsyncEnqueue(aliasWrapper,b));
		Check(AsyncEnqueue(fixture.wrapper,a));
		Check(DrainAsync());
		REQUIRE(first.calls == 2 && second.calls == 3);
		a->destroy = fixture.container;
		Check(AsyncEnqueue(fixture.wrapper,a));
		Check(AsyncEnqueue(fixture.wrapper,b));
		Check(DrainAsync());
		REQUIRE(first.calls == 3 && second.calls == 3 && !AQVM::AsyncTests().requests && !AQVM::AsyncTests().targets);
	}
	Stage("Async nonfatal diagnostics and modal delivery.\r\n");
	{
		AsyncProbeCounts failures;
		AsyncProbeCounts closed;
		AsyncProbeCounts outer;
		CAsyncFixture fixture;
		TStackRef<CAsyncProbe> fail;
		TStackRef<CAsyncProbe> close;
		TStackRef<CAsyncProbe> show;
		TStackRef<IJSONObject> definition;
		TStackRef<IQuadooForm> modal;
		TStackRef<IQuadooForm> owner;
		TStackRef<IQuadooWinControl> ownerControl;
		HWND ownerWindow;
		BOOL ownerDisabled = FALSE;
		Check(fixture.Initialize());
		Check(AQVM::ParseObject(SLP(L"{\"type\":\"Form\",\"name\":\"AsyncModal\"}"),&definition));
		Check(QuadooCreateForm(NULL,NULL,definition,&owner));
		Check(owner->QueryInterface(&ownerControl));
		Check(owner->GetHandle(&ownerWindow));
		Check(owner->ShowForm(SW_SHOWNOACTIVATE));
		Check(QuadooCreateForm(NULL,ownerControl,definition,&modal));
		fail.Attach(__new CAsyncProbe(&failures));
		close.Attach(__new CAsyncProbe(&closed));
		show.Attach(__new CAsyncProbe(&outer));
		CheckAlloc(fail);
		CheckAlloc(close);
		CheckAlloc(show);
		fail->failure = E_ACCESSDENIED;
		close->destroy = modal;
		close->ownerWindow = ownerWindow;
		close->ownerDisabled = &ownerDisabled;
		show->modal = modal;
		Check(AsyncEnqueue(fixture.wrapper,show));
		Check(AsyncEnqueue(fixture.wrapper,fail));
		Check(AsyncEnqueue(fixture.wrapper,close));
		Check(DrainAsync());
		REQUIRE(outer.calls == 1 && failures.calls == 1 && closed.calls == 1);
		REQUIRE(ownerDisabled && IsWindowEnabled(ownerWindow));
		REQUIRE(SUCCEEDED(fixture.runtime->Error()) && RStrLen(fixture.runtime->Diagnostic()) != 0);
		Check(AsyncEnqueue(fixture.wrapper,fail));
		Check(DrainAsync());
		REQUIRE(failures.calls == 2 && SUCCEEDED(fixture.runtime->Error()));
		close->destroy.Release();
		modal.Release();
		Check(QuadooCreateForm(NULL,ownerControl,definition,&modal));
		show->modal = modal;
		close->quit = TRUE;
		close->quitCode = 123;
		Check(AsyncEnqueue(fixture.wrapper,show));
		Check(AsyncEnqueue(fixture.wrapper,close));
		Check(DrainAsync());
		UINT quits = 0;
		INT quitCode = 0;
		Check(DrainLifetime(&quits,&quitCode));
		REQUIRE(quits == 1 && quitCode == 123);
		REQUIRE(IsWindowEnabled(ownerWindow) && SUCCEEDED(fixture.runtime->Error()));
		Check(modal->Destroy());
		Check(owner->Destroy());
	}
	Stage("Async optional lifecycle support and registration-time destruction.\r\n");
	{
		AsyncProbeCounts counts;
		CAsyncFixture fixture;
		TStackRef<CAsyncProbe> probe;
		TStackRef<CUnobservedAsyncContainer> container;
		TStackRef<IQuadooObject> wrapper;
		Check(fixture.Initialize());
		probe.Attach(__new CAsyncProbe(&counts));
		container.Attach(__new CUnobservedAsyncContainer);
		CheckAlloc(probe);
		CheckAlloc(container);
		Check(container->Initialize(fixture.context));
		Check(fixture.binding->Wrap(container,&wrapper));
		Check(AsyncEnqueue(wrapper,probe));
		Check(container->Destroy());
		REQUIRE(!AQVM::AsyncTests().requests);
		Check(AsyncEnqueue(fixture.wrapper,probe));
		Check(DrainAsync());
		REQUIRE(counts.calls == 1 && !AQVM::AsyncTests().targets);
		fixture.container->closeDuringRegistration = fixture.container;
		REQUIRE(AsyncEnqueue(fixture.wrapper,probe) == QuadooForm::EDestroyed);
		REQUIRE(!AQVM::AsyncTests().requests && !AQVM::AsyncTests().targets && !fixture.container->subscriptions);
	}
	Stage("Async detach, shutdown and repeated reference cleanup.\r\n");
	for(UINT n = 0; n < 24; n++)
	{
		AsyncProbeCounts counts;
		CAsyncFixture fixture;
		TStackRef<CAsyncProbe> probe;
		Check(fixture.Initialize());
		probe.Attach(__new CAsyncProbe(&counts));
		CheckAlloc(probe);
		Check(AsyncEnqueue(fixture.wrapper,probe));
		probe.Release();
		if(n % 3 == 0)
		{
			fixture.binding->Detach();
		}
		else if(n % 3 == 1)
		{
			fixture.runtime->Shutdown();
		}
		Check(DrainAsync());
		REQUIRE(counts.deleted == 1 && counts.destructed == 1 && counts.calls == (n % 3 == 2 ? 1 : 0));
		REQUIRE(!AQVM::AsyncTests().requests && !AQVM::AsyncTests().targets);
	}
	Stage("Async pending-work lease without a live Form.\r\n");
	for(UINT cancel = 0; cancel < 2; cancel++)
	{
		AsyncProbeCounts counts;
		CAsyncFixture fixture;
		TStackRef<CAsyncProbe> probe;
		BOOL run = FALSE;
		UINT quits = 0;
		INT code = -1;
		Check(fixture.Initialize());
		probe.Attach(__new CAsyncProbe(&counts));
		CheckAlloc(probe);
		Check(AsyncEnqueue(fixture.wrapper,probe));
		probe.Release();
		if(cancel)
		{
			fixture.binding->Detach();
		}
		Check(fixture.runtime->BeginMessageLoop(&run));
		REQUIRE(run);
		Check(DrainLifetime(&quits,&code));
		REQUIRE(quits == 1 && code == 0 && counts.calls == (cancel ? 0 : 1));
		REQUIRE(counts.destructed == 1 && counts.deleted == 1 && !AQVM::AsyncTests().requests && !AQVM::AsyncTests().targets);
		fixture.runtime->EndMessageLoop();
	}
	hr = S_OK;
Cleanup:
	AQVM::AsyncTests().failAllocation = -1;
	AQVM::AsyncTests().postError = 0;
	AQVM::AsyncTests().failTicket = FALSE;
	return hr;
}

static HRESULT ScriptAsyncTests(PCWSTR path)
{
	HRESULT hr;
	HRESULT registered;
	TStackRef<CAQVMHost> host;
	TStackRef<IQuadooInstanceLoader> loader;
	TStackRef<IQuadooVM> vm;
	TStackRef<IQuadooObject> exception;
	TStackRef<IJSONObject> catalog;
	TStackRef<IQuadooControlRegistry> registry;
	CHostedModules modules;
	CMemoryStream code;
	CCompilerStatus compiler;
	QuadooVM::QVARIANT value = {QuadooVM::Null};
	QuadooVM::QVARIANT form = {QuadooVM::Null};
	QuadooVM::QVPARAMS empty = {0};
	INT expectedCalls = 10;
	static const WCHAR text[] = L"{\"schemaVersion\":2,\"startupClass\":\"AsyncForm\",\"units\":["
		L"{\"class\":\"AsyncForm\",\"kind\":\"form\",\"eventsInterface\":\"IAsyncForm\",\"unitId\":\"{F7000000-0000-0000-0000-000000000001}\"},"
		L"{\"class\":\"AsyncFrame\",\"kind\":\"frame\",\"eventsInterface\":\"IAsyncFrame\",\"unitId\":\"{F7000000-0000-0000-0000-000000000002}\"}]}";
	Stage("Async real VM callables, captures and result classification.\r\n");
	Check(AQVM::ParseObject(SLP(text),&catalog));
	Check(QuadooParseToStream(path,0,&code,NULL,&compiler));
	host.Attach(__new CAQVMHost);
	CheckAlloc(host);
	Check(host->Initialize(&modules,RSTRING_CAST(path),0,NULL));
	Check(QuadooGetControlRegistry(&registry));
	Check(host->InitializeApplication(GetModuleHandleW(NULL),catalog,registry));
	Check(QVMCreateLoader(NULL,&loader));
	host->AttachLoader(loader);
	Check(loader->AddInstance(RSTRING_CAST(L"AsyncTests"),NULL,&code,code.DataRemaining(),NULL));
	Check(loader->LoadVM(RSTRING_CAST(L"AsyncTests"),&registered,&vm));
	Check(vm->AddGlobal(RSTRING_CAST(L"Host"),host));
	Check(vm->AddExternalClassLoader(host));
	Check(vm->RunConstructor(&exception));
	REQUIRE(!exception);
	Check(host->Runtime()->BeginScriptEntry());
	Check(Call(vm,L"AsyncMake",&form));
	REQUIRE(form.eType == QuadooVM::Object);
	Check(Call(vm,L"AsyncQueueScript",&value));
	if(value.eType == QuadooVM::Object)
	{
		QuadooVM::QVARIANT description = {QuadooVM::Null};
		if(SUCCEEDED(value.pObject->Invoke(NULL,RSTRING_CAST(L"ToString"),&empty,&description)) && description.eType == QuadooVM::String)
		{
			CHAR text[2048];
			WideCharToMultiByte(CP_UTF8,0,RStrToWide(description.rstrVal),-1,text,ARRAYSIZE(text),NULL,NULL);
			Stage(text);
			Stage("\r\n");
		}
		QVMClearVariant(&description);
	}
	REQUIRE(value.eType == QuadooVM::I4 && !value.lVal);
	QVMClearVariant(&value);
	Check(DrainAsync());
	Check(Call(vm,L"AsyncCounts",&value));
	if(value.eType != QuadooVM::I4 || value.lVal != 8)
	{
		CHAR text[256];
		wsprintfA(text,"Async calls type=%u value=%d pending=%u targets=%u\r\n",value.eType,value.lVal,AQVM::AsyncTests().requests,AQVM::AsyncTests().targets);
		Stage(text);
		if(RStrLen(host->Runtime()->Diagnostic()))
		{
			CHAR diagnostic[2048];
			WideCharToMultiByte(CP_UTF8,0,RStrToWide(host->Runtime()->Diagnostic()),-1,diagnostic,ARRAYSIZE(diagnostic),NULL,NULL);
			Stage(diagnostic);
			Stage("\r\n");
		}
	}
	REQUIRE(value.eType == QuadooVM::I4 && value.lVal == 8);
	QVMClearVariant(&value);
	REQUIRE(!RStrLen(host->Runtime()->Diagnostic()));
	Check(Call(vm,L"AsyncDestructed",&value));
	REQUIRE(value.eType == QuadooVM::I4 && value.lVal == 1);
	QVMClearVariant(&value);
	Check(Call(vm,L"AsyncQueueThrow",&value));
	QVMClearVariant(&value);
	Check(DrainAsync());
	REQUIRE(SUCCEEDED(host->Runtime()->Error()) && wcsstr(RStrToWide(host->Runtime()->Diagnostic()),L"Deliberate deferred Async failure") != NULL);
	Check(Call(vm,L"AsyncWrongArity",&value));
	QVMClearVariant(&value);
	Check(DrainAsync());
	Check(Call(vm,L"AsyncCounts",&value));
	REQUIRE(value.eType == QuadooVM::I4 && value.lVal == 10);
	QVMClearVariant(&value);
	{
		QuadooVM::QVARIANT object = {QuadooVM::Null};
		QuadooVM::QVARIANT returned = {QuadooVM::Null};
		QuadooVM::QVARIANT root = {QuadooVM::Null};
		Check(Call(vm,L"AsyncGetDefault",&object));
		HRESULT direct = object.pObject->Invoke(NULL,NULL,&empty,&returned);
		CHAR text[128];
		wsprintfA(text,"Existing VM default-forwarding native Invoke: 0x%.8x\r\n",direct);
		Stage(text);
		QVMClearVariant(&returned);
		REQUIRE(direct == S_OK);
		Check(Call(vm,L"AsyncCounts",&value));
		INT afterDirect = value.lVal;
		INT directCalls = afterDirect - 10;
		QVMClearVariant(&value);
		REQUIRE(directCalls == 1);
		Check(form.pObject->GetProperty(NULL,RSTRING_CAST(L"__QuadooRoot"),&root));
		HRESULT queued = AsyncEnqueue(root.pObject,object.pObject);
		QVMClearVariant(&root);
		QVMClearVariant(&object);
		Check(queued);
		Check(Call(vm,L"AsyncQueueGlobal",&value));
		QVMClearVariant(&value);
		Check(DrainAsync());
		Check(Call(vm,L"AsyncCounts",&value));
		REQUIRE(value.eType == QuadooVM::I4 && value.lVal == afterDirect + directCalls + 1);
		QVMClearVariant(&value);
		Check(Call(vm,L"AsyncQueueDefaultWrapped",&value));
		QVMClearVariant(&value);
		Check(DrainAsync());
		Check(Call(vm,L"AsyncCounts",&value));
		expectedCalls = afterDirect + directCalls + 2;
		REQUIRE(value.eType == QuadooVM::I4 && value.lVal == expectedCalls);
		QVMClearVariant(&value);
	}
	Check(Call(vm,L"AsyncRollback",&value));
	REQUIRE(value.eType == QuadooVM::Bool && value.fVal);
	QVMClearVariant(&value);
	Check(DrainAsync());
	Check(Call(vm,L"AsyncCounts",&value));
	REQUIRE(value.eType == QuadooVM::I4 && value.lVal == expectedCalls);
	QVMClearVariant(&value);
	Check(Call(vm,L"AsyncQueueClose",&value));
	QVMClearVariant(&value);
	Stage("Async last-Form replacement and normal application exit.\r\n");
	Check(host->Invoke(NULL,RSTRING_CAST(L"RunApplication"),&empty,&value));
	REQUIRE(SUCCEEDED(host->Runtime()->Error()));
	REQUIRE(!AQVM::AsyncTests().requests && !AQVM::AsyncTests().targets);
	{
		TStackRef<IQuadooFactoryCatalog> factories;
		UINT count;
		Check(registry->QueryInterface(&factories));
		Check(factories->GetFactoryCount(&count));
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IQuadooControlFactory> factory;
			TStackRef<IQuadooFactoryLifetime> lifetime;
			UINT live;
			Check(factories->GetFactory(n,&factory));
			Check(factory->QueryInterface(&lifetime));
			Check(lifetime->GetLiveCount(&live));
			REQUIRE(live == 0);
		}
	}
	{
		MSG quit;
		REQUIRE(!PeekMessageW(&quit,NULL,WM_QUIT,WM_QUIT,PM_REMOVE));
	}
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

static HRESULT AsyncTests(PCWSTR path)
{
	HRESULT hr;
	Check(NativeAsyncTests());
	Check(ScriptAsyncTests(path));
Cleanup:
	return hr;
}
