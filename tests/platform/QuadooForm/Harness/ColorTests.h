static INT SystemColorValue(INT index) { return static_cast<INT>(0x80000000u | index); }
static HRESULT ColorJSONBoundaries()
{
	HRESULT hr;
	TStackRef<IJSONObject> definition, saved;
	TStackRef<IJSONValue> value, parsed, first;
	TStackRef<IJSONArray> colors;
	TStackRef<IQuadooForm> form, copy;
	TStackRef<IQuadooControl> swatch;
	TStackRef<IQuadooFormSerializer> serializer;
	CMemoryStream stream;
	INT raw;
	Check(Parse(L"{\"type\":\"Form\",\"properties\":{\"BackColor\":-2147483648,\"ForeColor\":-2147483648},\"children\":[{\"type\":\"ColorSwatch\",\"name\":\"colors\",\"properties\":{\"Colors\":[-2147483648,255]}}]}",&definition));
	Check(QuadooCreateForm(NULL,NULL,definition,&form));
	REQUIRE(ImageInteger(form,L"BackColor") == INT_MIN && ImageInteger(form,L"ForeColor") == INT_MIN);
	Check(form->FindControl(RSTRING_CAST(L"colors"),&swatch));
	Check(ImageRead(swatch,L"Colors",&value));
	Check(value->GetArray(&colors));
	Check(colors->GetValue(0,&first));
	Check(first->GetInteger(&raw));
	REQUIRE(raw == INT_MIN);
	value.Release();
	Check(JSONParse(NULL,L"[-2147483648]",13,&value));
	Check(ImageSet(swatch,L"Colors",value));
	value.Release();
	Check(JSONCreateLongInteger(INT_MIN,&value));
	Check(ImageSet(form,L"BackColor",value));
	value.Release();
	Check(JSONCreateLongInteger(static_cast<LONGLONG>(INT_MIN) - 1,&value));
	REQUIRE(ImageSet(form,L"BackColor",value) == E_INVALIDARG && ImageInteger(form,L"BackColor") == INT_MIN);
	value.Release();
	Check(JSONParse(NULL,L"[2147483648]",12,&value));
	REQUIRE(ImageSet(swatch,L"Colors",value) == E_INVALIDARG);
	value.Release(); colors.Release(); first.Release();
	Check(QuadooGetFormSerializer(&serializer));
	Check(serializer->Serialize(form,&saved));
	Check(JSONSerializeObject(saved,&stream));
	Check(JSONParse(NULL,stream.TGetReadPtr<WCHAR>(),stream.DataRemaining() / sizeof(WCHAR),&parsed));
	definition.Release();
	Check(parsed->GetObject(&definition));
	Check(QuadooCreateForm(NULL,NULL,definition,&copy));
	REQUIRE(ImageInteger(copy,L"BackColor") == INT_MIN && ImageInteger(copy,L"ForeColor") == INT_MIN);
	// Failure to replace a palette must preserve its preceding INT_MIN entry.
	swatch.Release();
	Check(copy->FindControl(RSTRING_CAST(L"colors"),&swatch));
	Check(ImageRead(swatch,L"Colors",&value));
	Check(value->GetArray(&colors));
	REQUIRE(colors->Count() == 1);
	Check(colors->GetValue(0,&first));
	Check(first->GetInteger(&raw));
	REQUIRE(raw == INT_MIN);
Cleanup:
	if(form) form->Destroy();
	if(copy) copy->Destroy();
	return hr;
}
static HRESULT ColorProperties()
{
	HRESULT hr;
	TStackRef<IJSONObject> definition, saved;
	TStackRef<IQuadooForm> form, reopened;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooFormSerializer> serializer;
	CImageTestCanvas canvas;
	RECT area = {0,0,160,80};
	Check(ColorJSONBoundaries());
	REQUIRE(GetProcAddress(GetModuleHandleW(L"QuadooForm.dll"),"ResolveQuadooColor") != NULL);
	REQUIRE(GetProcAddress(GetModuleHandleW(L"QuadooForm.dll"),MAKEINTRESOURCEA(15)) == GetProcAddress(GetModuleHandleW(L"QuadooForm.dll"),"ResolveQuadooColor"));
	REQUIRE(ResolveQuadooColor(RGB(231,19,57)) == RGB(231,19,57));
	for(INT n = 0; n <= COLOR_MENUBAR; n++)
	{
		REQUIRE(ResolveQuadooColor(SystemColorValue(n)) == GetSysColor(n));
	}
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"Colors\",\"children\":[{\"type\":\"Panel\",\"name\":\"panel\"},{\"type\":\"ScrollingPanel\",\"name\":\"scroll\"},{\"type\":\"GroupBox\",\"name\":\"group\"},{\"type\":\"TabControl\",\"name\":\"tabs\",\"children\":[{\"type\":\"TabPage\",\"name\":\"page\"}]},{\"type\":\"Label\",\"name\":\"label\",\"properties\":{\"Left\":250,\"Caption\":\"MM\",\"Font\":{\"Size\":32,\"Bold\":true}}},{\"type\":\"Image\",\"name\":\"image\",\"properties\":{\"Left\":250,\"Transparent\":false}},{\"type\":\"TextBox\",\"name\":\"edit\"},{\"type\":\"Memo\",\"name\":\"memo\"},{\"type\":\"ComboBox\",\"name\":\"combo\"},{\"type\":\"ListBox\",\"name\":\"list\"},{\"type\":\"Button\",\"name\":\"button\"},{\"type\":\"CheckBox\",\"name\":\"check\"},{\"type\":\"RadioButton\",\"name\":\"radio\"}]}",&definition));
	Check(QuadooCreateForm(NULL,NULL,definition,&form));
	REQUIRE(ImageInteger(form,L"BackColor") == SystemColorValue(COLOR_BTNFACE));
	REQUIRE(ImageInteger(form,L"ForeColor") == SystemColorValue(COLOR_WINDOWTEXT));
	Check(form->FindControl(RSTRING_CAST(L"edit"),&control));
	REQUIRE(ImageInteger(control,L"BackColor") == SystemColorValue(COLOR_WINDOW));
	control.Release();
	Check(ImageNumber(form,L"BackColor",SystemColorValue(COLOR_APPWORKSPACE)));
	Check(ImageNumber(form,L"ForeColor",SystemColorValue(COLOR_HIGHLIGHT)));
	Check(canvas.Create(160,80));
	{
		static PCWSTR const containers[] = {L"Colors",L"panel",L"scroll",L"group",L"page"};
		for(UINT n = 0; n < ARRAYSIZE(containers); n++)
		{
			TStackRef<IQuadooChildWindowHost> host;
			HWND window;
			control.Release();
			if(!n) control = form;
			else
			{
				Check(form->FindControl(RSTRING_CAST(containers[n]),&control));
			}
			Check(ImageNumber(control,L"BackColor",SystemColorValue(COLOR_APPWORKSPACE)));
			Check(control->QueryInterface(__uuidof(IQuadooChildWindowHost),reinterpret_cast<PVOID*>(&host)));
			Check(host->GetChildHostHandle(&window));
			canvas.Fill(RGB(1,2,3));
			SendMessageW(window,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(canvas.DC()),PRF_CLIENT);
			REQUIRE(canvas.Pixel(2,2) == GetSysColor(COLOR_APPWORKSPACE));
			SendMessageW(window,WM_SYSCOLORCHANGE,0,0);
			REQUIRE(ImageInteger(control,L"BackColor") == SystemColorValue(COLOR_APPWORKSPACE));
		}
	}
	{
		static PCWSTR const names[] = {L"edit",L"memo",L"combo",L"list",L"button",L"check",L"radio"};
		for(UINT n = 0; n < ARRAYSIZE(names); n++)
		{
			TStackRef<IQuadooWinControl> windowed;
			HWND window;
			control.Release();
			Check(form->FindControl(RSTRING_CAST(names[n]),&control));
			Check(ImageNumber(control,L"BackColor",SystemColorValue(COLOR_INFOBK)));
			Check(ImageNumber(control,L"ForeColor",SystemColorValue(COLOR_HIGHLIGHT)));
			Check(control->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&windowed)));
			Check(windowed->GetHandle(&window));
			UINT message = n < 3 ? WM_CTLCOLOREDIT : n == 3 ? WM_CTLCOLORLISTBOX : WM_CTLCOLORBTN;
			if(n == 2) SendMessageW(window,message,reinterpret_cast<WPARAM>(canvas.DC()),reinterpret_cast<LPARAM>(window));
			else SendMessageW(GetParent(window),message,reinterpret_cast<WPARAM>(canvas.DC()),reinterpret_cast<LPARAM>(window));
			REQUIRE(GetBkColor(canvas.DC()) == GetSysColor(COLOR_INFOBK) && GetTextColor(canvas.DC()) == GetSysColor(COLOR_HIGHLIGHT));
			REQUIRE(ImageInteger(control,L"ForeColor") == SystemColorValue(COLOR_HIGHLIGHT));
		}
	}
	{
		TStackRef<IQuadooStaticControl> painted;
		control.Release();
		Check(form->FindControl(RSTRING_CAST(L"image"),&control));
		Check(ImageNumber(control,L"BackColor",SystemColorValue(COLOR_INFOBK)));
		Check(control->QueryInterface(__uuidof(IQuadooStaticControl),reinterpret_cast<PVOID*>(&painted)));
		Check(painted->Paint(canvas.DC(),&area));
		REQUIRE(canvas.Pixel(2,2) == GetSysColor(COLOR_INFOBK));
		REQUIRE(ImageNumber(control,L"BackColor",0x01000000) == E_INVALIDARG);
		REQUIRE(ImageNumber(control,L"BackColor",static_cast<INT>(0xff000000)) == E_INVALIDARG);
		REQUIRE(ImageInteger(control,L"BackColor") == SystemColorValue(COLOR_INFOBK));
		painted.Release(); control.Release();
		Check(form->FindControl(RSTRING_CAST(L"label"),&control));
		Check(ImageNumber(control,L"ForeColor",SystemColorValue(COLOR_HIGHLIGHT)));
		Check(control->QueryInterface(__uuidof(IQuadooStaticControl),reinterpret_cast<PVOID*>(&painted)));
		canvas.Fill(GetSysColor(COLOR_HIGHLIGHT) ^ 0xffffff);
		Check(painted->Paint(canvas.DC(),&area));
		BOOL ink = FALSE;
		for(UINT y = 0; y < 80; y++) for(UINT x = 0; x < 160; x++) if(canvas.Pixel(x,y) == GetSysColor(COLOR_HIGHLIGHT)) ink = TRUE;
		REQUIRE(ink);
	}
	Check(QuadooGetFormSerializer(&serializer));
	Check(serializer->Serialize(form,&saved));
	Check(QuadooCreateForm(NULL,NULL,saved,&reopened));
	REQUIRE(ImageInteger(reopened,L"BackColor") == SystemColorValue(COLOR_APPWORKSPACE));
	control.Release();
	Check(reopened->FindControl(RSTRING_CAST(L"edit"),&control));
	REQUIRE(ImageInteger(control,L"BackColor") == SystemColorValue(COLOR_INFOBK) && ImageInteger(control,L"ForeColor") == SystemColorValue(COLOR_HIGHLIGHT));
	Check(form->Destroy());
	Check(reopened->Destroy());
	hr = S_OK;
Cleanup:
	if(form) form->Destroy();
	if(reopened) reopened->Destroy();
	return hr;
}
