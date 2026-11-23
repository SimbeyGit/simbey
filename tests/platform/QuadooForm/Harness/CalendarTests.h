static HRESULT CalendarSetDate(IQuadooComponent* component, PCWSTR name, RSTRING date)
{
	HRESULT hr;
	TStackRef<IQuadooProperties> properties;
	UINT index;
	Check(Property(component,name,&properties,&index));
	Check(properties->SetValueString(index,date));
Cleanup:
	return hr;
}

static HRESULT CalendarCheckDate(IQuadooControl* control, RSTRING expected)
{
	HRESULT hr;
	TStackRef<IQuadooProperties> properties;
	TStackRef<IQuadooWinControl> window;
	CRString value;
	CRString native;
	SYSTEMTIME selection;
	UINT index;
	HWND handle;
	Check(Property(control,L"Date",&properties,&index));
	Check(properties->GetValueString(index,&value));
	Check(control->QueryInterface(&window));
	Check(window->GetHandle(&handle));
	REQUIRE(MonthCal_GetCurSel(handle,&selection));
	Check(RStrFormatW(&native,L"%.4u-%.2u-%.2u",selection.wYear,selection.wMonth,selection.wDay));
	REQUIRE(value == expected && native == expected);
Cleanup:
	return hr;
}

static HRESULT NativeCalendars()
{
	HRESULT hr;
	TStackRef<CBinder> binder;
	TStackRef<CReport> report;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONObject> saved;
	TStackRef<IJSONObject> again;
	TStackRef<IQuadooControl> form;
	TStackRef<IQuadooControl> copy;
	TStackRef<IQuadooControlContainer> container;
	TStackRef<IQuadooControl> calendar;
	TStackRef<IQuadooWinControl> window;
	TStackRef<IQuadooLayoutControl> layout;
	TStackRef<IQuadooFormSerializer> serializer;
	QuadooForm::ContextOptions options = {0};
	QuadooForm::Rect bounds;
	SYSTEMTIME range[2];
	RECT minimum;
	RECT native;
	WCHAR className[64];
	HWND handle;
	binder.Attach(__new CBinder);
	CheckAlloc(binder);
	report.Attach(__new CReport);
	CheckAlloc(report);
	options.cbSize = sizeof(options);
	options.Mode = QuadooForm::Runtime;
	options.DPI = 96;
	options.pBinder = binder;
	options.pReport = report;
	Check(QuadooCreateConstructionContext(&options,&context));
	Check(Parse(L"{\"type\":\"Form\",\"children\":[{\"type\":\"Calendar\",\"name\":\"Dates\","
		L"\"properties\":{\"Date\":\"2032-02-29\",\"MinDate\":\"2032-02-01\",\"MaxDate\":\"2032-03-31\"},"
		L"\"events\":{\"OnChange\":\"Changed\",\"OnSelect\":\"Click\"}}]}",&definition));
	Check(QuadooCreateControl(context,NULL,definition,&form));
	Check(form->QueryInterface(&container));
	Check(container->FindControl(RSTRING_CAST(L"Dates"),&calendar));
	Check(calendar->QueryInterface(&window));
	Check(window->GetHandle(&handle));
	REQUIRE(GetClassNameW(handle,className,ARRAYSIZE(className)) && !lstrcmpW(className,MONTHCAL_CLASSW));
	REQUIRE((GetWindowLongW(handle,GWL_STYLE) & (WS_TABSTOP | MCS_MULTISELECT)) == WS_TABSTOP);
	Check(CalendarCheckDate(calendar,RSTRING_CAST(L"2032-02-29")));
	REQUIRE(!binder->changed && !binder->clicks);
	REQUIRE(MonthCal_GetRange(handle,range) == (GDTR_MIN | GDTR_MAX) && range[0].wMonth == 2 && range[1].wMonth == 3);
	Check(CalendarSetDate(calendar,L"MaxDate",RSTRING_CAST(L"2032-02-20")));
	Check(CalendarCheckDate(calendar,RSTRING_CAST(L"2032-02-20")));
	REQUIRE(binder->changed == 1 && !binder->clicks);
	REQUIRE(CalendarSetDate(calendar,L"MaxDate",RSTRING_CAST(L"2032-01-31")) == E_INVALIDARG);
	REQUIRE(CalendarSetDate(calendar,L"Date",RSTRING_CAST(L"2032-02-21")) == E_INVALIDARG);
	REQUIRE(CalendarSetDate(calendar,L"Date",RSTRING_CAST(L"2031-02-29")) == E_INVALIDARG);
	REQUIRE(CalendarSetDate(calendar,L"Date",RSTRING_CAST(L"2032-02-20T00:00:00")) == E_INVALIDARG);
	REQUIRE(CalendarSetDate(calendar,L"Date",RSTRING_CAST(L"")) == E_INVALIDARG);
	REQUIRE(MonthCal_GetRange(handle,range) == (GDTR_MIN | GDTR_MAX) && range[1].wMonth == 2 && range[1].wDay == 20);
	REQUIRE(binder->changed == 1 && !binder->clicks);
	Check(CalendarSetDate(calendar,L"MaxDate",RSTRING_CAST(L"")));
	Check(CalendarSetDate(calendar,L"Date",RSTRING_CAST(L"2032-03-15")));
	REQUIRE(CalendarSetDate(calendar,L"Date",RSTRING_CAST(L"2032-03-15")) == S_FALSE);
	REQUIRE(binder->changed == 2 && !binder->clicks);
	SendMessageW(handle,WM_KEYDOWN,VK_RIGHT,0);
	Check(CalendarCheckDate(calendar,RSTRING_CAST(L"2032-03-16")));
	REQUIRE(binder->changed == 3 && !binder->clicks);
	{
		// The mouse produces MCN_SELECT after MCN_SELCHANGE, even on a repeated date.
		NMSELCHANGE selected = {{handle,0,MCN_SELECT},{0},{0}};
		REQUIRE(MonthCal_GetCurSel(handle,&selected.stSelStart));
		selected.stSelEnd = selected.stSelStart;
		SendMessageW(GetParent(handle),WM_NOTIFY,0,reinterpret_cast<LPARAM>(&selected));
		REQUIRE(binder->changed == 3 && binder->clicks == 1);
	}
	{
		static PCWSTR const names[] = {L"ShowWeekNumbers",L"ShowToday",L"ShowTodayCircle"};
		for(UINT n = 0; n < ARRAYSIZE(names); n++)
		{
			TStackRef<IQuadooProperties> properties;
			UINT index;
			Check(Property(calendar,names[n],&properties,&index));
			Check(properties->SetValueBool(index,n == 0));
		}
	}
	Check(window->GetHandle(&handle));
	DWORD styles = MCS_WEEKNUMBERS | MCS_NOTODAY | MCS_NOTODAYCIRCLE;
	REQUIRE((GetWindowLongW(handle,GWL_STYLE) & styles) == styles && binder->changed == 3 && binder->clicks == 1);
	Check(CalendarCheckDate(calendar,RSTRING_CAST(L"2032-03-16")));
	Check(calendar->GetBounds(&bounds));
	Check(calendar->QueryInterface(&layout));
	for(UINT dpi = 96; dpi <= 144; dpi += 48)
	{
		Check(layout->Arrange(&bounds,dpi));
		REQUIRE(GetClientRect(handle,&native) && MonthCal_GetMinReqRect(handle,&minimum));
		REQUIRE(native.right == MulDiv(bounds.Width,dpi,96) && native.bottom == MulDiv(bounds.Height,dpi,96));
		REQUIRE(native.right >= minimum.right && native.bottom >= minimum.bottom);
	}
	Check(QuadooGetFormSerializer(&serializer));
	Check(serializer->Serialize(form,&saved));
	Check(QuadooCreateControl(context,NULL,saved,&copy));
	Check(serializer->Serialize(copy,&again));
	REQUIRE(!JSONCompareObjects(saved,again));
	{
		TStackRef<IQuadooControlContainer> copiedContainer;
		TStackRef<IQuadooControl> copiedCalendar;
		Check(copy->QueryInterface(&copiedContainer));
		Check(copiedContainer->FindControl(RSTRING_CAST(L"Dates"),&copiedCalendar));
		Check(CalendarCheckDate(copiedCalendar,RSTRING_CAST(L"2032-03-16")));
	}
	Check(copy->Destroy());
	{
		TStackRef<IQuadooProperties> properties;
		UINT index;
		Check(Property(calendar,L"OnChange",&properties,&index));
		Check(properties->SetValueEvent(index,RSTRING_CAST(L"Click")));
		binder->destroySender = TRUE;
		SendMessageW(handle,WM_KEYDOWN,VK_RIGHT,0);
		REQUIRE(!IsWindow(handle) && binder->clicks == 2 && report->errors == 0);
	}
	Check(form->Destroy());
	hr = S_OK;
Cleanup:
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
