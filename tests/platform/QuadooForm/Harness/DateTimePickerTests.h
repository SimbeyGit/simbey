static HRESULT PickerText(IQuadooControl* control, PCWSTR name, RSTRING text)
{
	HRESULT hr;
	TStackRef<IQuadooProperties> properties;
	TStackRef<IJSONValue> value;
	UINT index;
	Check(Property(control,name,&properties,&index));
	Check(JSONCreateString(text,&value));
	Check(properties->SetValueJSON(index,value));
Cleanup:
	return hr;
}

static HRESULT PickerBool(IQuadooControl* control, PCWSTR name, BOOL value)
{
	HRESULT hr;
	TStackRef<IQuadooProperties> properties;
	UINT index;
	Check(Property(control,name,&properties,&index));
	Check(properties->SetValueBool(index,value));
Cleanup:
	return hr;
}

class CPickerObserver : public CBaseUnknown, public IQuadooPropertyObserver
{
public:
	BOOL armed;
	BOOL destroy;
	UINT calls;
	CPickerObserver() : armed(TRUE), destroy(FALSE), calls(0)
	{
	}
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooPropertyObserver)
	END_UNK_MAP
	STDMETHOD(OnPropertyChanged)(IQuadooComponent* component, UINT)
	{
		HRESULT hr = S_OK;
		if(armed)
		{
			TStackRef<IQuadooDateTimePicker> picker;
			SYSTEMTIME value;
			armed = FALSE;
			calls++;
			Check(component->QueryInterface(&picker));
			Check(picker->GetDateTime(&value));
			if(destroy)
			{
				Check(component->Destroy());
			}
			else
			{
				value.wMinute = 7;
				Check(picker->SetDateTime(&value));
			}
		}
	Cleanup:
		return hr;
	}
	STDMETHOD(OnBoundsChanged)(IQuadooControl*, const QuadooForm::Rect*)
	{
		return S_OK;
	}
};

static HRESULT DateTimePickerResources();

static HRESULT DateTimePickerNativeTests()
{
	HRESULT hr;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooControlFactory> factory;
	TStackRef<IQuadooFactoryLifetime> lifetime;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONObject> saved;
	TStackRef<IQuadooControl> form;
	TStackRef<IQuadooControl> copy;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooControlContainer> container;
	TStackRef<IQuadooWinControl> window;
	TStackRef<IQuadooDateTimePicker> picker;
	TStackRef<CBinder> binder;
	TStackRef<IQuadooPaletteIcon> palette;
	TStackRef<IQuadooPropertyNotifications> notifications;
	TStackRef<IQuadooFormSerializer> serializer;
	TStackRef<CPickerObserver> observer;
	SYSTEMTIME value;
	SYSTEMTIME native;
	HWND handle;
	HICON icon = NULL;
	UINT baseline;
	UINT live;
	BOOL checked;
	Check(QuadooGetControlRegistry(&registry));
	Check(registry->FindFactory(RSTRING_CAST(L"DateTimePicker"),&factory));
	Check(factory->QueryInterface(&lifetime));
	Check(lifetime->GetLiveCount(&baseline));
	Check(factory->QueryInterface(&palette));
	Check(palette->GetPaletteIcon(20,20,&icon));
	REQUIRE(icon != NULL);
	DestroyIcon(icon);
	icon = NULL;
	binder.Attach(__new CBinder);
	CheckAlloc(binder);
	Check(Context(96,binder,&context,registry));
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"PickerTest\",\"children\":[{\"type\":\"DateTimePicker\",\"name\":\"Picker\","
		L"\"properties\":{\"DateTime\":\"2024-02-29T13:45:17.123\"},\"events\":{\"OnChange\":\"Changed\",\"OnDropDown\":\"Click\",\"OnCloseUp\":\"Closed\"}}]}",&definition));
	Check(QuadooCreateControl(context,NULL,definition,&form));
	Check(form->QueryInterface(&container));
	Check(container->FindControl(RSTRING_CAST(L"Picker"),&control));
	Check(control->QueryInterface(&picker));
	Check(control->QueryInterface(&window));
	Check(window->GetHandle(&handle));
	{
		WCHAR className[64];
		REQUIRE(GetClassNameW(handle,className,ARRAYSIZE(className)) && !lstrcmpW(className,DATETIMEPICK_CLASSW));
		const IID* paths[] = {&__uuidof(IQuadooDateTimePicker),&__uuidof(IQuadooWinControl),&__uuidof(IQuadooProperties)};
		Check(CheckCOMIdentity(control,paths,ARRAYSIZE(paths)));
	}
	REQUIRE((GetWindowLongW(handle,GWL_STYLE) & WS_TABSTOP) != 0);
	Check(picker->GetDateTime(&value));
	REQUIRE(value.wYear == 2024 && value.wMonth == 2 && value.wDay == 29 && value.wHour == 13 && value.wMinute == 45 && value.wMilliseconds == 123);
	REQUIRE(SendMessageW(handle,DTM_GETSYSTEMTIME,0,reinterpret_cast<LPARAM>(&native)) == GDT_VALID);
	REQUIRE(!memcmp(&native,&value,sizeof(value)) && binder->changed == 0);
	Check(picker->GetChecked(&checked));
	REQUIRE(checked && picker->GetDateTime(NULL) == E_POINTER && picker->GetChecked(NULL) == E_POINTER && picker->SetDateTime(NULL) == E_POINTER);
	REQUIRE(picker->SetChecked(FALSE) == E_INVALIDARG);
	static PCWSTR const bad[] = {L"2023-02-29",L"2024-04-31",L"2024-01-01T24:00:00",L"2024-01-01T12:60:00",L"2024-01-01T12:00:60",L"1500-01-01",L"2024-01-01Z",L"2024-01-01T00:00:00.12",L""};
	for(UINT n = 0; n < ARRAYSIZE(bad); n++)
	{
		REQUIRE(PickerText(control,L"DateTime",RSTRING_CAST(bad[n])) == E_INVALIDARG);
	}
	REQUIRE(!binder->changed);
	value.wYear = 2025;
	REQUIRE(picker->SetDateTime(&value) == E_INVALIDARG);
	value.wYear = 2024;
	value.wSecond++;
	Check(picker->SetDateTime(&value));
	REQUIRE(binder->changed == 1 && picker->SetDateTime(&value) == S_FALSE);
	Check(PickerBool(control,L"ShowCheckBox",TRUE));
	Check(window->GetHandle(&handle));
	REQUIRE((GetWindowLongW(handle,GWL_STYLE) & DTS_SHOWNONE) != 0 && binder->changed == 1);
	Check(picker->SetChecked(FALSE));
	REQUIRE(binder->changed == 2 && SendMessageW(handle,DTM_GETSYSTEMTIME,0,reinterpret_cast<LPARAM>(&native)) == GDT_NONE);
	Check(PickerText(control,L"Kind",RSTRING_CAST(L"Time")));
	Check(window->GetHandle(&handle));
	REQUIRE((GetWindowLongW(handle,GWL_STYLE) & DTS_TIMEFORMAT) == DTS_TIMEFORMAT && binder->changed == 2);
	Check(picker->SetChecked(TRUE));
	REQUIRE(SendMessageW(handle,DTM_GETSYSTEMTIME,0,reinterpret_cast<LPARAM>(&native)) == GDT_VALID && !memcmp(&value,&native,sizeof(value)) && binder->changed == 3);
	Check(picker->SetChecked(FALSE));
	Check(PickerBool(control,L"ShowCheckBox",FALSE));
	Check(picker->GetChecked(&checked));
	REQUIRE(checked && binder->changed == 5);
	Check(PickerText(control,L"Kind",RSTRING_CAST(L"Date")));
	Check(PickerText(control,L"DateFormat",RSTRING_CAST(L"Long")));
	Check(PickerText(control,L"DateMode",RSTRING_CAST(L"UpDown")));
	Check(window->GetHandle(&handle));
	REQUIRE((GetWindowLongW(handle,GWL_STYLE) & DTS_LONGDATEFORMAT) != 0 && (GetWindowLongW(handle,GWL_STYLE) & DTS_UPDOWN) != 0);
	Check(PickerText(control,L"Format",RSTRING_CAST(L"yyyy-MM-dd HH:mm:ss")));
	REQUIRE(PickerText(control,L"Format",RSTRING_CAST(L"XXX")) == E_INVALIDARG);
	REQUIRE(PickerText(control,L"Format",RSTRING_CAST(L"'unclosed")) == E_INVALIDARG);
	Check(PickerText(control,L"MinDate",RSTRING_CAST(L"2024-03-01")));
	Check(picker->GetDateTime(&value));
	REQUIRE(value.wMonth == 3 && value.wDay == 1 && value.wHour == 13 && binder->changed == 6);
	REQUIRE(PickerText(control,L"MaxDate",RSTRING_CAST(L"2024-02-29")) == E_INVALIDARG);
	REQUIRE(PickerText(control,L"DateTime",RSTRING_CAST(L"2024-02-29")) == E_INVALIDARG);
	Check(PickerText(control,L"MaxDate",RSTRING_CAST(L"2024-03-31")));
	Check(PickerText(control,L"DateTime",RSTRING_CAST(L"2024-03-31T23:59:59.999")));
	REQUIRE(SendMessageW(handle,DTM_GETSYSTEMTIME,0,reinterpret_cast<LPARAM>(&native)) == GDT_VALID && native.wDay == 31 && native.wHour == 23 && native.wMilliseconds == 999);
	Check(PickerText(control,L"MinDate",RSTRING_CAST(L"")));
	Check(PickerText(control,L"MaxDate",RSTRING_CAST(L"")));
	Check(window->GetHandle(&handle));
	value = native;
	value.wDay = 30;
	UINT changes = binder->changed;
	REQUIRE(SendMessageW(handle,DTM_SETSYSTEMTIME,GDT_VALID,reinterpret_cast<LPARAM>(&value)) != 0);
	{
		// Setting the native value is not a user edit. Reflect the notification
		// sent by the real picker when its keyboard/calendar changes the value.
		NMDATETIMECHANGE notice = {{handle,0,DTN_DATETIMECHANGE},GDT_VALID,value};
		SendMessageW(GetParent(handle),WM_NOTIFY,0,reinterpret_cast<LPARAM>(&notice));
		SendMessageW(GetParent(handle),WM_NOTIFY,0,reinterpret_cast<LPARAM>(&notice));
	}
	REQUIRE(binder->changed == changes + 1);
	Check(picker->GetDateTime(&native));
	REQUIRE(native.wDay == 30);
	{
		NMHDR notice = {handle,0,DTN_DROPDOWN};
		SendMessageW(GetParent(handle),WM_NOTIFY,0,reinterpret_cast<LPARAM>(&notice));
		notice.code = DTN_CLOSEUP;
		SendMessageW(GetParent(handle),WM_NOTIFY,0,reinterpret_cast<LPARAM>(&notice));
		REQUIRE(binder->clicks == 1 && binder->closed == 1);
	}
	Check(control->QueryInterface(&notifications));
	observer.Attach(__new CPickerObserver);
	CheckAlloc(observer);
	Check(notifications->AddObserver(observer));
	changes = binder->changed;
	value.wSecond = 1;
	Check(picker->SetDateTime(&value));
	Check(picker->GetDateTime(&native));
	REQUIRE(native.wMinute == 7 && observer->calls == 1 && binder->changed == changes + 1);
	Check(notifications->RemoveObserver(observer));
	{
		TStackRef<IQuadooLayoutControl> layout;
		QuadooForm::Rect bounds;
		Check(control->GetBounds(&bounds));
		Check(control->QueryInterface(&layout));
		Check(layout->Arrange(&bounds,144));
		Check(layout->Arrange(&bounds,96));
		Check(picker->GetDateTime(&value));
		REQUIRE(!memcmp(&value,&native,sizeof(value)));
	}
	Check(QuadooGetFormSerializer(&serializer));
	Check(serializer->Serialize(form,&saved));
	Check(QuadooCreateControl(context,NULL,saved,&copy));
	{
		TStackRef<IQuadooControlContainer> other;
		TStackRef<IQuadooControl> child;
		TStackRef<IQuadooDateTimePicker> copied;
		Check(copy->QueryInterface(&other));
		Check(other->FindControl(RSTRING_CAST(L"Picker"),&child));
		Check(child->QueryInterface(&copied));
		Check(copied->GetDateTime(&value));
		REQUIRE(!memcmp(&value,&native,sizeof(value)));
	}
	Check(copy->Destroy());
	copy.Release();
	observer->armed = TRUE;
	observer->destroy = TRUE;
	Check(notifications->AddObserver(observer));
	value.wSecond++;
	REQUIRE(picker->SetDateTime(&value) == QuadooForm::EDestroyed);
	REQUIRE(picker->GetDateTime(&value) == QuadooForm::EDestroyed && !IsWindow(handle));
	Check(form->Destroy());
	notifications.Release();
	control.Release();
	picker.Release();
	window.Release();
	container.Release();
	form.Release();
	Check(lifetime->GetLiveCount(&live));
	REQUIRE(live == baseline);
	Check(registry->UnregisterFactory(RSTRING_CAST(L"DateTimePicker")));
	Check(DateTimePickerResources());
	hr = S_OK;
Cleanup:
	if(icon)
	{
		DestroyIcon(icon);
	}
	if(copy)
	{
		copy->Destroy();
	}
	if(form)
	{
		form->Destroy();
	}
	return hr;
}

static HRESULT DateTimePickerResources()
{
	HRESULT hr;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooControlFactory> factory;
	TStackRef<IQuadooFactoryLifetime> lifetime;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IJSONObject> definition;
	TStackRef<IQuadooControl> form;
	CGuiResourceMonitor resources(L"DateTimePicker HWND, font and factory lifetime");
	UINT live;
	Check(QuadooGetControlRegistry(&registry));
	Check(registry->FindFactory(RSTRING_CAST(L"DateTimePicker"),&factory));
	Check(factory->QueryInterface(&lifetime));
	Check(Context(144,NULL,&context,registry));
	static PCWSTR const invalid[] =
	{
		L"{\"DateTime\":\"1900-02-29\"}",
		L"{\"DateTime\":\"2024-02-29\",\"MinDate\":\"2024-03-01\",\"MaxDate\":\"2024-02-29\"}",
		L"{\"Checked\":false}",
		L"{\"Kind\":3}",
		L"{\"Format\":\"XXX\"}"
	};
	for(UINT n = 0; n < ARRAYSIZE(invalid); n++)
	{
		CRString json;
		Check(RStrFormatW(&json,L"{\"type\":\"Form\",\"children\":[{\"type\":\"DateTimePicker\",\"properties\":%ls}]}",invalid[n]));
		Check(Parse(json.c_wstr(),&definition));
		REQUIRE(QuadooCreateControl(context,NULL,definition,&form) == E_INVALIDARG && !form);
		definition.Release();
		Check(lifetime->GetLiveCount(&live));
		REQUIRE(!live);
	}
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"Pickers\",\"children\":[{\"type\":\"DateTimePicker\",\"name\":\"Picker\",\"properties\":{\"DateTime\":\"2000-02-29\"}}]}",&definition));
	while(!resources.Complete())
	{
		{
			TStackRef<IQuadooControlContainer> container;
			TStackRef<IQuadooControl> control;
			TStackRef<IQuadooWinControl> window;
			TStackRef<IQuadooDateTimePicker> picker;
			SYSTEMTIME time;
			HWND handle;
			Check(QuadooCreateControl(context,NULL,definition,&form));
			Check(form->QueryInterface(&container));
			Check(container->FindControl(RSTRING_CAST(L"Picker"),&control));
			Check(control->QueryInterface(&window));
			Check(control->QueryInterface(&picker));
			Check(picker->GetDateTime(&time));
			REQUIRE(time.wYear == 2000 && time.wDay == 29 && !time.wHour);
			Check(PickerText(control,L"DateTime",RSTRING_CAST(L"1601-01-01")));
			Check(PickerText(control,L"DateTime",RSTRING_CAST(L"9999-12-31T23:59:59.999")));
			Check(PickerBool(control,L"ShowCheckBox",TRUE));
			Check(picker->SetChecked(FALSE));
			Check(PickerText(control,L"Kind",RSTRING_CAST(L"Time")));
			Check(PickerText(control,L"Kind",RSTRING_CAST(L"Date")));
			Check(PickerBool(control,L"ShowCheckBox",FALSE));
			Check(window->GetHandle(&handle));
			Check(form->Destroy());
			REQUIRE(!IsWindow(handle));
			Check(lifetime->GetLiveCount(&live));
			REQUIRE(live == 1 && registry->UnregisterFactory(RSTRING_CAST(L"DateTimePicker")) == HRESULT_FROM_WIN32(ERROR_BUSY));
		}
		form.Release();
		Check(lifetime->GetLiveCount(&live));
		REQUIRE(!live);
		Check(resources.AfterCycle());
	}
	hr = S_OK;
Cleanup:
	if(form)
	{
		form->Destroy();
	}
	return hr;
}
