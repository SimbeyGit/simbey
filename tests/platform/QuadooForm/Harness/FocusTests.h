class CFocusRecorder : public CBaseUnknown, public IQuadooEventsBinder, public IQuadooDelegate
{
public:
	CRString log;
	BOOL destroy;
	HWND redirect;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooEventsBinder)
		UNK_INTERFACE(IQuadooDelegate)
	END_UNK_MAP
	CFocusRecorder() : destroy(FALSE), redirect(NULL)
	{
	}
	STDMETHOD(BindEvent)(RSTRING, IQuadooDelegate** delegate)
	{
		if(delegate)
		{
			*delegate = this;
			AddRef();
		}
		return delegate ? S_OK : E_POINTER;
	}
	STDMETHOD(PostBind)(IQuadooControl*)
	{
		return S_OK;
	}
	STDMETHOD(Invoke)(IQuadooEventArgs* args)
	{
		HRESULT hr;
		TStackRef<IQuadooComponent> sender;
		CRString name;
		CRString event;
		CRString next;
		Check(args->GetSender(&sender));
		Check(sender->GetName(&name));
		Check(args->GetEventName(&event));
		Check(RStrFormatW(&next,L"%r%r.%r;",log ? static_cast<RSTRING>(log) : RSTRING_CAST(L""),static_cast<RSTRING>(name),static_cast<RSTRING>(event)));
		log = next;
		if(redirect && name == RSTRING_CAST(L"A") && event == RSTRING_CAST(L"OnEnter"))
		{
			HWND target = redirect;
			redirect = NULL;
			SetFocus(target);
		}
		if(destroy && name == RSTRING_CAST(L"Destroy") && event == RSTRING_CAST(L"OnEnter"))
		{
			destroy = FALSE;
			Check(sender->Destroy());
		}
	Cleanup:
		return hr;
	}
};

static HRESULT FocusEnterLeave()
{
	HRESULT hr;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooComponentRegistry> components;
	TStackRef<IJSONObject> definition;
	TStackRef<CFocusRecorder> recorder;
	TStackRef<IQuadooForm> form;
	TStackRef<IQuadooControl> controls[5];
	TStackRef<IQuadooWinControl> native;
	HWND host = NULL;
	HWND windows[5] = {0};
	PCWSTR const names[] = {L"A",L"B",L"Combo",L"Outside",L"Destroy"};
	PCWSTR const types[] = {L"Form",L"Frame",L"Panel",L"ScrollingPanel",L"Button",L"TextBox",L"Memo",L"CheckBox",L"RadioButton",L"GroupBox",L"ComboBox",L"ListBox",L"TabControl",L"TabPage",L"Label",L"Image",L"ColorSwatch"};
	Check(QuadooGetControlRegistry(&registry));
	for(UINT n = 0; n < ARRAYSIZE(types); n++)
	{
		TStackRef<IQuadooControlFactory> factory;
		TStackRef<IQuadooPropertyDescriptors> properties;
		TStackRef<IQuadooEventDescriptors> events;
		TStackRef<IQuadooPropertyDescriptor> property;
		TStackRef<IQuadooEventDescriptor> event;
		Check(registry->FindFactory(RSTRING_CAST(types[n]),&factory));
		Check(factory->GetProperties(&properties));
		Check(factory->GetEvents(&events));
		REQUIRE(SUCCEEDED(properties->Find(RSTRING_CAST(L"OnEnter"),&property)) == (n < 14));
		REQUIRE(SUCCEEDED(events->Find(RSTRING_CAST(L"OnLeave"),&event)) == (n < 14));
	}
	Check(registry->QueryInterface(&components));
	{
		TStackRef<IQuadooComponentFactory> timer;
		TStackRef<IQuadooEventDescriptors> events;
		TStackRef<IQuadooEventDescriptor> event;
		Check(components->FindComponentFactory(RSTRING_CAST(L"Timer"),&timer));
		Check(timer->GetEvents(&events));
		REQUIRE(FAILED(events->Find(RSTRING_CAST(L"OnEnter"),&event)));
	}
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"Form\",\"events\":{\"OnEnter\":\"Focus\",\"OnLeave\":\"Focus\"},\"children\":["
		L"{\"type\":\"Panel\",\"name\":\"Panel\",\"properties\":{\"Width\":300,\"Height\":200},\"events\":{\"OnEnter\":\"Focus\",\"OnLeave\":\"Focus\"},\"children\":["
		L"{\"type\":\"TextBox\",\"name\":\"A\",\"events\":{\"OnEnter\":\"Focus\",\"OnLeave\":\"Focus\"}},"
		L"{\"type\":\"Button\",\"name\":\"B\",\"properties\":{\"Top\":40},\"events\":{\"OnEnter\":\"Focus\",\"OnLeave\":\"Focus\"}},"
		L"{\"type\":\"ComboBox\",\"name\":\"Combo\",\"properties\":{\"Top\":80},\"events\":{\"OnEnter\":\"Focus\",\"OnLeave\":\"Focus\"}},"
		L"{\"type\":\"Button\",\"name\":\"Destroy\",\"properties\":{\"Top\":120},\"events\":{\"OnEnter\":\"Focus\",\"OnLeave\":\"Focus\"}}]},"
		L"{\"type\":\"Button\",\"name\":\"Outside\",\"properties\":{\"Left\":320},\"events\":{\"OnEnter\":\"Focus\",\"OnLeave\":\"Focus\"}}]}",&definition));
	recorder.Attach(__new CFocusRecorder);
	CheckAlloc(recorder);
	Check(QuadooCreateForm(recorder,NULL,definition,&form));
	Check(form->ShowForm(SW_SHOWNOACTIVATE));
	for(UINT n = 0; n < ARRAYSIZE(names); n++)
	{
		Check(form->FindControl(RSTRING_CAST(names[n]),&controls[n]));
		Check(controls[n]->QueryInterface(&native));
		Check(native->GetHandle(&windows[n]));
		native.Release();
	}
	host = CreateWindowExW(0,L"STATIC",L"Focus test",WS_OVERLAPPEDWINDOW | WS_VISIBLE,0,0,100,100,NULL,NULL,GetModuleHandleW(NULL),NULL);
	CheckIf(!host,HrEnsureLastError());
	SetFocus(host);
	REQUIRE(GetFocus() == host);
	recorder->log = static_cast<RSTRING>(NULL);
	SetFocus(windows[0]);
	REQUIRE(recorder->log == RSTRING_CAST(L"Form.OnEnter;Panel.OnEnter;A.OnEnter;"));
	recorder->log = static_cast<RSTRING>(NULL);
	SetFocus(windows[1]);
	REQUIRE(recorder->log == RSTRING_CAST(L"A.OnLeave;B.OnEnter;"));
	recorder->log = static_cast<RSTRING>(NULL);
	SetFocus(windows[2]);
	REQUIRE(recorder->log == RSTRING_CAST(L"B.OnLeave;Combo.OnEnter;"));
	{
		COMBOBOXINFO info = {sizeof(info)};
		REQUIRE(GetComboBoxInfo(windows[2],&info) && GetFocus() == info.hwndItem);
		recorder->log = static_cast<RSTRING>(NULL);
		SetFocus(info.hwndItem);
		REQUIRE(!RStrLen(recorder->log));
	}
	Check(SetInt(controls[2],L"Style",QuadooForm::DropDownList));
	Check(controls[2]->QueryInterface(&native));
	Check(native->GetHandle(&windows[2]));
	native.Release();
	REQUIRE(GetFocus() == windows[2]);
	REQUIRE(!RStrLen(recorder->log));
	recorder->log = static_cast<RSTRING>(NULL);
	SetFocus(windows[1]);
	REQUIRE(recorder->log == RSTRING_CAST(L"Combo.OnLeave;B.OnEnter;"));
	recorder->log = static_cast<RSTRING>(NULL);
	SetFocus(windows[3]);
	REQUIRE(recorder->log == RSTRING_CAST(L"B.OnLeave;Panel.OnLeave;Outside.OnEnter;"));
	recorder->log = static_cast<RSTRING>(NULL);
	SetFocus(host);
	REQUIRE(recorder->log == RSTRING_CAST(L"Outside.OnLeave;Form.OnLeave;"));
	recorder->log = static_cast<RSTRING>(NULL);
	recorder->redirect = windows[1];
	SetFocus(windows[0]);
	REQUIRE(GetFocus() == windows[1]);
	REQUIRE(recorder->log == RSTRING_CAST(L"Form.OnEnter;Panel.OnEnter;A.OnEnter;A.OnLeave;B.OnEnter;"));
	SetFocus(host);
	recorder->destroy = TRUE;
	SetFocus(windows[4]);
	{
		QuadooForm::ComponentState state;
		Check(controls[4]->GetState(&state));
		REQUIRE(state == QuadooForm::Destroyed && !IsWindow(windows[4]));
	}
	SetFocus(host);
	recorder->log = static_cast<RSTRING>(NULL);
	SetFocus(windows[0]);
	REQUIRE(recorder->log == RSTRING_CAST(L"Form.OnEnter;Panel.OnEnter;A.OnEnter;"));
	{
		TStackRef<IQuadooFormSerializer> serializer;
		TStackRef<IJSONObject> saved;
		TStackRef<IJSONValue> value;
		TStackRef<IJSONObject> events;
		CRString handler;
		Check(QuadooGetFormSerializer(&serializer));
		Check(serializer->Serialize(form,&saved));
		Check(saved->FindNonNullValueW(L"events",&value));
		Check(value->GetObject(&events));
		value.Release();
		Check(events->FindNonNullValueW(L"OnEnter",&value));
		Check(value->GetString(&handler));
		REQUIRE(handler == RSTRING_CAST(L"Focus"));
	}
	hr = S_OK;
Cleanup:
	if(form)
	{
		form->Destroy();
	}
	if(host)
	{
		DestroyWindow(host);
	}
	return hr;
}
