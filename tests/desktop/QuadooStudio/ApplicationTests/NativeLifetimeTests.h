static HRESULT DrainLifetime(UINT* quits, INT* code);

// An independent native Form implementation. Uses only published capabilities
// and CBaseUnknown, with no inheritance from any QuadooForm implementation.
class CIndependentLifetimeForm : public CBaseUnknown, public IQuadooForm, public IQuadooComponentLifecycle
{
	QuadooForm::ComponentState m_state;
	TArray<IQuadooComponentDestroyedObserver*> m_observers;
	HWND m_window;

public:
	BOOL failRegistration;
	UINT subscriptions;
	IQuadooForm* closeDuringRegistration;
	UINT quitsDuringRegistration;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooForm)
		UNK_INTERFACE_PATH(IQuadooControlContainer, IQuadooForm)
		UNK_INTERFACE_PATH(IQuadooWinControl, IQuadooForm)
		UNK_INTERFACE_PATH(IQuadooControl, IQuadooForm)
		UNK_INTERFACE_PATH(IQuadooComponent, IQuadooForm)
		UNK_INTERFACE(IQuadooComponentLifecycle)
	END_UNK_MAP

	CIndependentLifetimeForm() : m_state(QuadooForm::Ready), m_window(NULL), failRegistration(FALSE),
		subscriptions(0), closeDuringRegistration(NULL), quitsDuringRegistration(0)
	{
	}

	~CIndependentLifetimeForm()
	{
		Destroy();
	}

	HRESULT Initialize()
	{
		m_window = CreateWindowExW(0, L"STATIC", L"Independent lifetime Form", WS_OVERLAPPEDWINDOW,
			0, 0, 100, 100, NULL, NULL, GetModuleHandleW(NULL), NULL);
		return m_window ? S_OK : HrEnsureLastError();
	}

	STDMETHOD(AddDestroyedObserver)(IQuadooComponentDestroyedObserver* observer)
	{
		HRESULT hr;
		CheckIf(!observer, E_INVALIDARG);
		CheckIf(m_state >= QuadooForm::Destroying, QuadooForm::EDestroyed);
		for(sysint n = 0; n < m_observers.Length(); n++)
		{
			CheckIfIgnore(AQVM::SameIdentity(observer, m_observers[n]), S_FALSE);
		}
		Check(m_observers.Append(observer));
		observer->AddRef();
		subscriptions++;
		if(closeDuringRegistration)
		{
			INT code;
			Check(closeDuringRegistration->Destroy());
			Check(DrainLifetime(&quitsDuringRegistration, &code));
		}
		// Deliberately fail after a partial external mutation to test rollback.
		CheckIf(failRegistration, E_OUTOFMEMORY);
	Cleanup:
		return hr;
	}

	STDMETHOD(RemoveDestroyedObserver)(IQuadooComponentDestroyedObserver* observer)
	{
		HRESULT hr = S_FALSE;
		for(sysint n = 0; n < m_observers.Length(); n++)
		{
			if(AQVM::SameIdentity(observer, m_observers[n]))
			{
				m_observers.Remove(n, NULL);
				subscriptions--;
				observer->Release();
				hr = S_OK;
				break;
			}
		}
		return hr;
	}

	STDMETHOD(Destroy)()
	{
		HRESULT hr = S_OK;
		CheckIfIgnore(m_state >= QuadooForm::Destroying, S_FALSE);
		m_state = QuadooForm::Destroying;
		if(m_window)
		{
			DestroyWindow(m_window);
			m_window = NULL;
		}
		m_state = QuadooForm::Destroyed;
		while(m_observers.Length())
		{
			TStackRef<IQuadooComponentDestroyedObserver> observer;
			observer.Attach(m_observers[0]);
			m_observers.Remove(0, NULL);
			subscriptions--;
			observer->OnComponentDestroyed(this);
		}
	Cleanup:
		return hr;
	}

	STDMETHOD(GetState)(QuadooForm::ComponentState* state)
	{
		if(!state)
		{
			return E_POINTER;
		}
		*state = m_state;
		return S_OK;
	}

	STDMETHOD(GetHandle)(HWND* window)
	{
		if(!window)
		{
			return E_POINTER;
		}
		*window = m_window;
		return m_state == QuadooForm::Destroyed ? QuadooForm::EDestroyed : S_OK;
	}

	STDMETHOD(GetID)(GUID*)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(GetName)(RSTRING*)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(SetName)(RSTRING)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(GetTypeName)(RSTRING*)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(GetProperties)(IQuadooProperties**)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(GetParent)(IQuadooControlContainer**)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(GetBounds)(QuadooForm::Rect*)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(SetBounds)(const QuadooForm::Rect*)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(GetEffectiveBounds)(QuadooForm::Rect*)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(GetVisible)(BOOL*)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(SetVisible)(BOOL)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(GetEnabled)(BOOL*)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(SetEnabled)(BOOL)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(SetFocus)()
	{
		return E_NOTIMPL;
	}
	STDMETHOD(GetTabOrder)(INT*)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(SetTabOrder)(INT)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(GetChildCount)(UINT*)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(GetChild)(UINT, IQuadooControl**)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(FindControl)(RSTRING, IQuadooControl**)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(RegisterControl)(IQuadooControl*)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(UnregisterControl)(IQuadooControl*)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(BeginUpdate)()
	{
		return E_NOTIMPL;
	}
	STDMETHOD(EndUpdate)()
	{
		return E_NOTIMPL;
	}
	STDMETHOD(UpdateLayout)()
	{
		return E_NOTIMPL;
	}
	STDMETHOD(ShowForm)(INT)
	{
		return S_OK;
	}
	STDMETHOD(HideForm)()
	{
		return S_OK;
	}
	STDMETHOD(ShowModal)(LRESULT*)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(Close)()
	{
		return Destroy();
	}
};

// Simulate an older/unsupported root while retaining the same public Form ABI.
class CUnobservedLifetimeForm : public CIndependentLifetimeForm
{
public:
	IMP_UNKNOWN(CIndependentLifetimeForm)
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooForm)
		UNK_INTERFACE_PATH(IQuadooControlContainer, IQuadooForm)
		UNK_INTERFACE_PATH(IQuadooWinControl, IQuadooForm)
		UNK_INTERFACE_PATH(IQuadooControl, IQuadooForm)
		UNK_INTERFACE_PATH(IQuadooComponent, IQuadooForm)
	END_UNK_MAP
};

static HRESULT DrainLifetime(UINT* quits, INT* code)
{
	HRESULT hr = S_OK;
	MSG message;
	UINT count = 0;
	while(PeekMessageW(&message, NULL, 0, 0, PM_REMOVE))
	{
		CheckIf(++count > 1000, E_FAIL);
		if(message.message == WM_QUIT)
		{
			(*quits)++;
			*code = static_cast<INT>(message.wParam);
		}
		else
		{
			TranslateMessage(&message);
			DispatchMessageW(&message);
		}
	}
Cleanup:
	return hr;
}

class CFinalLifetimeObserver : public CBaseUnknown, public IQuadooComponentDestroyedObserver
{
public:
	UINT calls;
	BOOL finalState;
	HWND window;
	IQuadooComponentLifecycle* removeFrom;
	IQuadooComponentDestroyedObserver* removeOther;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooComponentDestroyedObserver)
	END_UNK_MAP

	CFinalLifetimeObserver() : calls(0), finalState(FALSE), window(NULL), removeFrom(NULL), removeOther(NULL)
	{
	}

	STDMETHOD_(VOID,OnComponentDestroyed)(IQuadooComponent* component)
	{
		QuadooForm::ComponentState state;
		calls++;
		finalState = SUCCEEDED(component->GetState(&state)) && state == QuadooForm::Destroyed && !IsWindow(window);
		if(removeFrom && removeOther)
		{
			removeFrom->RemoveDestroyedObserver(removeOther);
		}
	}
};

class CFailLifetimeBinder : public CBaseUnknown, public IQuadooEventsBinder, public IQuadooDelegate
{
public:
	TStackRef<CFinalLifetimeObserver> observer;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooEventsBinder)
		UNK_INTERFACE(IQuadooDelegate)
	END_UNK_MAP

	STDMETHOD(BindEvent)(RSTRING, IQuadooDelegate** delegate)
	{
		*delegate = this;
		AddRef();
		return S_OK;
	}

	STDMETHOD(PostBind)(IQuadooControl* control)
	{
		HRESULT hr;
		TStackRef<IQuadooComponentLifecycle> lifecycle;
		Check(control->QueryInterface(&lifecycle));
		Check(lifecycle->AddDestroyedObserver(observer));
	Cleanup:
		return hr;
	}

	STDMETHOD(Invoke)(IQuadooEventArgs*)
	{
		return E_FAIL;
	}
};

class CScriptLifetimeProbe : public CBaseUnknown, public CQuadooObjectImpl, public IQuadooInterface
{
	UINT* m_destroyed;

public:
	IQuadooForm* closeDuringLookup;
	UINT quitsDuringLookup;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooObject)
		UNK_INTERFACE(IQuadooInterface)
	END_UNK_MAP

	CScriptLifetimeProbe(UINT* destroyed) : m_destroyed(destroyed), closeDuringLookup(NULL), quitsDuringLookup(0)
	{
	}

	~CScriptLifetimeProbe()
	{
		(*m_destroyed)++;
	}

	STDMETHOD(GetInterface)(RSTRING, IQuadooInterface** methods)
	{
		HRESULT hr = S_OK;
		*methods = NULL;
		if(closeDuringLookup)
		{
			INT code;
			Check(closeDuringLookup->Destroy());
			Check(DrainLifetime(&quitsDuringLookup, &code));
		}
		*methods = this;
		AddRef();
	Cleanup:
		return hr;
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

static HRESULT RetainedApplicationDelegate()
{
	HRESULT hr;
	UINT destroyed = 0;
	TStackRef<AQVM::CApplicationRuntime> runtime;
	TStackRef<AQVM::CFormInstance> instance;
	TStackRef<IQuadooObject> script;
	TStackRef<IQuadooDelegate> delegate;
	TStackRef<IJSONObject> unit;
	TStackRef<IJSONObject> definition;
	runtime.Attach(__new AQVM::CApplicationRuntime);
	CheckAlloc(runtime);
	script.Attach(__new CScriptLifetimeProbe(&destroyed));
	CheckAlloc(script);
	instance.Attach(__new AQVM::CFormInstance(runtime, script));
	CheckAlloc(instance);
	Check(JSONCreateObject(&unit));
	Check(JSONCreateObject(&definition));
	Check(AQVM::PutString(unit, L"kind", RSTRING_CAST(L"form")));
	Check(AQVM::PutString(unit, L"eventsInterface", RSTRING_CAST(L"IProbe")));
	Check(instance->Initialize(unit, definition));
	Check(instance->BindEvent(RSTRING_CAST(L"Handler"), &delegate));
	script.Release();
	REQUIRE(destroyed == 0);
	instance->Detach();
	REQUIRE(destroyed == 1);
	REQUIRE(delegate->Invoke(NULL) == QuadooForm::EDestroyed);
Cleanup:
	if(instance)
	{
		instance->Detach();
	}
	return hr;
}

static HRESULT FinalLifecycleContract()
{
	HRESULT hr;
	TStackRef<IJSONObject> definition;
	TStackRef<IQuadooForm> form;
	TStackRef<IQuadooComponentHost> host;
	TStackRef<IQuadooComponentCollection> components;
	TStackRef<IQuadooComponent> timer;
	TStackRef<IQuadooComponentLifecycle> lifecycle;
	TStackRef<IQuadooComponentLifecycle> timerLifecycle;
	TStackRef<IUnknown> identity;
	TStackRef<IUnknown> otherIdentity;
	TStackRef<CFinalLifetimeObserver> first;
	TStackRef<CFinalLifetimeObserver> second;
	TStackRef<CFinalLifetimeObserver> nonvisual;
	TStackRef<CFailLifetimeBinder> failed;
	static const WCHAR json[] = L"{\"schemaVersion\":1,\"type\":\"Form\",\"name\":\"ObserverTest\","
		L"\"id\":\"{F6000000-0000-0000-0000-000000000010}\",\"components\":[{\"type\":\"Timer\","
		L"\"name\":\"Timer1\",\"id\":\"{F6000000-0000-0000-0000-000000000011}\"}]}";
	Check(AQVM::ParseObject(SLP(json), &definition));
	Check(QuadooCreateForm(NULL, NULL, definition, &form));
	Check(form->QueryInterface(&lifecycle));
	Check(form->QueryInterface(IID_IUnknown, reinterpret_cast<PVOID*>(&identity)));
	Check(lifecycle->QueryInterface(IID_IUnknown, reinterpret_cast<PVOID*>(&otherIdentity)));
	REQUIRE(identity == otherIdentity);
	first.Attach(__new CFinalLifetimeObserver);
	second.Attach(__new CFinalLifetimeObserver);
	nonvisual.Attach(__new CFinalLifetimeObserver);
	CheckAlloc(first);
	CheckAlloc(second);
	CheckAlloc(nonvisual);
	Check(form->GetHandle(&first->window));
	first->removeFrom = lifecycle;
	first->removeOther = second;
	Check(lifecycle->AddDestroyedObserver(first));
	REQUIRE(lifecycle->AddDestroyedObserver(first) == S_FALSE);
	Check(lifecycle->AddDestroyedObserver(second));
	Check(form->QueryInterface(&host));
	Check(host->GetComponents(&components));
	Check(components->GetAt(0, &timer));
	Check(timer->QueryInterface(&timerLifecycle));
	Check(timerLifecycle->AddDestroyedObserver(nonvisual));
	Check(form->Destroy());
	REQUIRE(first->calls == 1 && first->finalState);
	REQUIRE(second->calls == 0);
	REQUIRE(nonvisual->calls == 1 && nonvisual->finalState);
	REQUIRE(form->Destroy() == S_FALSE);
	REQUIRE(lifecycle->RemoveDestroyedObserver(first) == S_FALSE);
	REQUIRE(lifecycle->AddDestroyedObserver(second) == QuadooForm::EDestroyed);
	REQUIRE(first->calls == 1);
	form.Release();
	first->removeFrom = NULL;
	first->removeOther = NULL;
	{
		TStackRef<IJSONObject> events;
		TStackRef<IJSONValue> value;
		Check(JSONCreateObject(&events));
		Check(AQVM::PutString(events, L"OnCreate", RSTRING_CAST(L"Fail")));
		Check(JSONWrapObject(events, &value));
		Check(definition->AddValueW(L"events", value));
	}
	failed.Attach(__new CFailLifetimeBinder);
	CheckAlloc(failed);
	failed->observer = second;
	REQUIRE(QuadooCreateForm(failed, NULL, definition, &form) == E_FAIL);
	REQUIRE(!form && second->calls == 1 && second->finalState);
Cleanup:
	if(form)
	{
		form->Destroy();
	}
	return hr;
}

static HRESULT CreationLifetimeScope(IJSONObject* catalog)
{
	HRESULT hr;
	UINT destroyed = 0;
	UINT quits = 0;
	INT code = -1;
	BOOL run;
	TStackRef<AQVM::CApplicationRuntime> runtime;
	TStackRef<CIndependentLifetimeForm> form;
	TStackRef<CScriptLifetimeProbe> script;
	TStackRef<IJSONObject> unit;
	TStackRef<IJSONObject> definition;
	TStackRef<IQuadooObject> root;
	static const WCHAR json[] = L"{\"type\":\"Form\",\"name\":\"Pending\"}";
	runtime.Attach(__new AQVM::CApplicationRuntime);
	form.Attach(__new CIndependentLifetimeForm);
	script.Attach(__new CScriptLifetimeProbe(&destroyed));
	CheckAlloc(runtime);
	CheckAlloc(form);
	CheckAlloc(script);
	Check(runtime->Initialize(GetModuleHandleW(NULL), catalog));
	Check(form->Initialize());
	Check(runtime->RegisterForm(form));
	Check(runtime->BeginScriptEntry());
	Check(runtime->BeginMessageLoop(&run));
	REQUIRE(run);
	Check(JSONCreateObject(&unit));
	Check(AQVM::PutString(unit, L"kind", RSTRING_CAST(L"form")));
	Check(AQVM::PutString(unit, L"eventsInterface", RSTRING_CAST(L"IProbe")));
	Check(AQVM::ParseObject(SLP(json), &definition));
	script->closeDuringLookup = form;
	// This probe lacks script properties, so PostBind will deliberately fail.
	// Its earlier GetInterface re-entry must still be part of creation activity.
	REQUIRE(FAILED(runtime->Create(unit, definition, script, NULL, &root)));
	REQUIRE(!root && script->quitsDuringLookup == 0);
	REQUIRE(SUCCEEDED(runtime->Error()));
	Check(DrainLifetime(&quits, &code));
	REQUIRE(quits == 1 && code == 0);
	runtime->EndMessageLoop();
Cleanup:
	if(runtime)
	{
		runtime->Shutdown();
	}
	return hr;
}

static HRESULT NativeApplicationLifetime()
{
	HRESULT hr;
	TStackRef<IJSONObject> catalog;
	static const WCHAR catalogText[] = L"{\"schemaVersion\":2,\"startupClass\":\"Native\",\"units\":["
		L"{\"class\":\"Native\",\"kind\":\"form\",\"eventsInterface\":\"INative\","
		L"\"unitId\":\"{F6000000-0000-0000-0000-000000000001}\"}]}";
	const UINT order[][3] = {{0,1,2}, {0,2,1}, {1,0,2}, {1,2,0}, {2,0,1}, {2,1,0}};
	DWORD beforeUser;
	DWORD beforeGDI;
	Check(FinalLifecycleContract());
	Check(RetainedApplicationDelegate());
	Check(AQVM::ParseObject(SLP(catalogText), &catalog));
	Check(CreationLifetimeScope(catalog));
	{
		TStackRef<AQVM::CApplicationRuntime> warm;
		TStackRef<CIndependentLifetimeForm> window;
		TStackRef<CUnobservedLifetimeForm> unsupported;
		warm.Attach(__new AQVM::CApplicationRuntime);
		window.Attach(__new CIndependentLifetimeForm);
		CheckAlloc(warm);
		CheckAlloc(window);
		Check(warm->Initialize(GetModuleHandleW(NULL), catalog));
		Check(window->Initialize());
		Check(window->Destroy());
		unsupported.Attach(__new CUnobservedLifetimeForm);
		CheckAlloc(unsupported);
		Check(unsupported->Initialize());
		REQUIRE(warm->RegisterForm(unsupported) == E_NOINTERFACE);
		REQUIRE(wcsstr(RStrToWide(warm->Diagnostic()), L"IQuadooComponentLifecycle") != NULL);
		REQUIRE(SUCCEEDED(warm->Error()));
		Check(unsupported->Destroy());
		warm->Shutdown();
	}
	beforeUser = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
	beforeGDI = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
	for(UINT scenario = 0; scenario < ARRAYSIZE(order) + 5; scenario++)
	{
		TStackRef<AQVM::CApplicationRuntime> runtime;
		TStackRef<CIndependentLifetimeForm> forms[3];
		TStackRef<IUnknown> lease;
		UINT quits = 0;
		INT code = -1;
		BOOL run;
		runtime.Attach(__new AQVM::CApplicationRuntime);
		CheckAlloc(runtime);
		Check(runtime->Initialize(GetModuleHandleW(NULL), catalog));
		for(UINT n = 0; n < ARRAYSIZE(forms); n++)
		{
			forms[n].Attach(__new CIndependentLifetimeForm);
			CheckAlloc(forms[n]);
			Check(forms[n]->Initialize());
		}
		if(scenario < ARRAYSIZE(order))
		{
			for(UINT n = 0; n < ARRAYSIZE(forms); n++)
			{
				Check(runtime->RegisterForm(forms[n]));
				REQUIRE(runtime->RegisterForm(forms[n]) == S_FALSE);
				REQUIRE(forms[n]->subscriptions == 1);
			}
			Check(runtime->BeginScriptEntry());
			Check(runtime->BeginMessageLoop(&run));
			REQUIRE(run);
			for(UINT n = 0; n < ARRAYSIZE(forms); n++)
			{
				Check(forms[order[scenario][n]]->Destroy());
				Check(DrainLifetime(&quits, &code));
				REQUIRE(quits == (n == 2 ? 1U : 0U));
			}
			REQUIRE(code == 0);
		}
		else if(scenario == ARRAYSIZE(order))
		{
			forms[0]->failRegistration = TRUE;
			REQUIRE(runtime->RegisterForm(forms[0]) == E_OUTOFMEMORY);
			REQUIRE(forms[0]->subscriptions == 0);
			Check(runtime->BeginScriptEntry());
			Check(runtime->BeginMessageLoop(&run));
			Check(DrainLifetime(&quits, &code));
			REQUIRE(quits == (run ? 1U : 0U));
		}
		else if(scenario == ARRAYSIZE(order) + 1)
		{
			Check(runtime->AcquireLifetimeLease(&lease));
			Check(runtime->BeginScriptEntry());
			Check(runtime->BeginMessageLoop(&run));
			REQUIRE(run);
			Check(DrainLifetime(&quits, &code));
			REQUIRE(!quits);
			Check(runtime->RegisterForm(forms[0]));
			Check(forms[0]->Destroy());
			Check(DrainLifetime(&quits, &code));
			REQUIRE(!quits);
			lease.Release();
			Check(DrainLifetime(&quits, &code));
			REQUIRE(quits == 1 && code == 0);
			REQUIRE(runtime->AcquireLifetimeLease(&lease) == QuadooForm::EInvalidState);
		}
		else if(scenario == ARRAYSIZE(order) + 2)
		{
			Check(runtime->RegisterForm(forms[0]));
			Check(runtime->BeginScriptEntry());
			Check(runtime->BeginMessageLoop(&run));
			Check(runtime->EnterCallback());
			Check(forms[0]->Destroy());
			Check(DrainLifetime(&quits, &code));
			REQUIRE(!quits);
			Check(runtime->RegisterForm(forms[1]));
			runtime->LeaveCallback();
			Check(DrainLifetime(&quits, &code));
			REQUIRE(!quits);
			Check(forms[1]->Destroy());
			Check(DrainLifetime(&quits, &code));
			REQUIRE(quits == 1 && code == 0);
		}
		else if(scenario == ARRAYSIZE(order) + 3)
		{
			Check(runtime->AcquireLifetimeLease(&lease));
			runtime->Shutdown();
			lease.Release();
			Check(DrainLifetime(&quits, &code));
			REQUIRE(!quits);
		}
		else
		{
			Check(runtime->RegisterForm(forms[0]));
			Check(runtime->BeginScriptEntry());
			Check(runtime->BeginMessageLoop(&run));
			forms[1]->failRegistration = TRUE;
			forms[1]->closeDuringRegistration = forms[0];
			REQUIRE(runtime->RegisterForm(forms[1]) == E_OUTOFMEMORY);
			REQUIRE(forms[1]->subscriptions == 0 && forms[1]->quitsDuringRegistration == 0);
			Check(DrainLifetime(&quits, &code));
			REQUIRE(quits == 1 && code == 0);
		}
		runtime->EndMessageLoop();
		runtime->Shutdown();
	}
	REQUIRE(GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS) <= beforeUser);
	REQUIRE(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) <= beforeGDI);
Cleanup:
	return hr;
}

static HRESULT ExternalApplicationQuit(BOOL closeBeforePump = FALSE)
{
	HRESULT hr;
	TStackRef<CAQVMHost> host;
	TStackRef<CIndependentLifetimeForm> form;
	TStackRef<IJSONObject> catalog;
	CHostedModules modules;
	QuadooVM::QVARIANT value = {QuadooVM::Null};
	QuadooVM::QVPARAMS empty = {0};
	QuadooForm::ComponentState state;
	static const WCHAR json[] = L"{\"schemaVersion\":2,\"startupClass\":\"Native\",\"units\":["
		L"{\"class\":\"Native\",\"kind\":\"form\",\"eventsInterface\":\"INative\","
		L"\"unitId\":\"{F6000000-0000-0000-0000-000000000001}\"}]}";
	Check(AQVM::ParseObject(SLP(json), &catalog));
	host.Attach(__new CAQVMHost);
	form.Attach(__new CIndependentLifetimeForm);
	CheckAlloc(host);
	CheckAlloc(form);
	Check(host->Initialize(&modules, RSTRING_CAST(L"Native quit test"), 0, NULL));
	Check(host->InitializeApplication(GetModuleHandleW(NULL), catalog));
	Check(form->Initialize());
	Check(host->Runtime()->RegisterForm(form));
	REQUIRE(host->Invoke(NULL, RSTRING_CAST(L"RunApplication"), &empty, &value) == QuadooForm::EInvalidState);
	empty.cArgs = 2;
	REQUIRE(host->Invoke(NULL, RSTRING_CAST(L"RunApplication"), &empty, &value) == DISP_E_BADPARAMCOUNT);
	empty.cArgs = 0;
	Check(host->Runtime()->BeginScriptEntry());
	if(closeBeforePump)
	{
		Check(form->Destroy());
	}
	PostQuitMessage(123);
	Check(host->Invoke(NULL, RSTRING_CAST(L"RunApplication"), &empty, &value));
	REQUIRE(value.eType == QuadooVM::I4 && value.lVal == 123);
	REQUIRE(host->Invoke(NULL, RSTRING_CAST(L"RunApplication"), &empty, &value) == QuadooForm::EInvalidState);
	Check(form->GetState(&state));
	REQUIRE(state == (closeBeforePump ? QuadooForm::Destroyed : QuadooForm::Ready));
	host->ShutdownApplication();
	Check(form->GetState(&state));
	REQUIRE(state == QuadooForm::Destroyed && form->subscriptions == 0);
Cleanup:
	if(host)
	{
		host->ShutdownApplication();
	}
	QVMClearVariant(&value);
	return hr;
}
