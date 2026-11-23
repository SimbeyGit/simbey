// These tests create real HWNDs and run only when GUI testing is authorized.
static HRESULT ButtonFormCompletionTests()
{
	HRESULT hr;
	TStackRef<IQuadooForm> form;
	static PCWSTR const types[] = {L"Button",L"ImageButton"};
	for(UINT type = 0; type < ARRAYSIZE(types); type++)
	{
		for(UINT action = 0; action < 4; action++)
		{
			TStackRef<IJSONObject> definition;
			TStackRef<CBinder> binder;
			TStackRef<IQuadooControl> button;
			TStackRef<IQuadooControl> edit;
			TStackRef<IQuadooFormKeyboard> keyboard;
			TStackRef<IQuadooButton> command;
			TStackRef<IQuadooProperties> values;
			CRString json;
			HWND window;
			HWND buttonWindow;
			HWND editWindow;
			UINT index;
			LONGLONG result;
			Check(RStrFormatW(&json,
				L"{\"type\":\"Form\",\"name\":\"ButtonCompletion\",\"events\":{\"OnCloseQuery\":\"Query\",\"OnClose\":\"Closed\"},"
				L"\"children\":[{\"type\":\"Panel\",\"name\":\"scope\",\"properties\":{\"Width\":300,\"Height\":120},\"children\":["
				L"{\"type\":\"%ls\",\"name\":\"finish\",\"properties\":{\"Caption\":\"Close\",\"Cancel\":true,\"Default\":true},"
				L"\"events\":{\"OnClick\":\"Click\"}},"
				L"{\"type\":\"TextBox\",\"name\":\"edit\",\"properties\":{\"Top\":50}}]}]}",types[type]));
			Check(Parse(json.c_wstr(),&definition));
			binder.Attach(__new CBinder);
			CheckAlloc(binder);
			Check(QuadooCreateForm(binder,NULL,definition,&form));
			Check(form->GetHandle(&window));
			Check(form->FindControl(RSTRING_CAST(L"finish"),&button));
			Check(form->FindControl(RSTRING_CAST(L"edit"),&edit));
			Check(form->QueryInterface(__uuidof(IQuadooFormKeyboard),reinterpret_cast<PVOID*>(&keyboard)));
			Check(button->QueryInterface(__uuidof(IQuadooButton),reinterpret_cast<PVOID*>(&command)));
			Check(KeyWindow(button,&buttonWindow));
			Check(KeyWindow(edit,&editWindow));
			Check(form->ShowForm(SW_SHOW));
			SetFocus(editWindow);
			// A zero result is an ordinary click, even on a Cancel button.
			Check(command->Click());
			REQUIRE(IsWindow(window) && binder->clicks == 1 && !binder->closed);
			Check(Property(form,L"ModalResult",&values,&index));
			Check(values->SetValueLong(index,41));
			Check(ImageNumber(button,L"ModalResult",IDCANCEL));
			binder->cancel = TRUE;
			for(UINT attempt = 0; attempt < 2; attempt++)
			{
				if(action == 0 || action == 3)
				{
					Check(KeyDispatch(keyboard,editWindow,WM_KEYDOWN,action == 0 ? VK_ESCAPE : VK_RETURN));
				}
				else if(action == 1)
				{
					SendMessageW(buttonWindow,BM_CLICK,0,0);
				}
				else
				{
					Check(command->Click());
				}
				Check(values->GetValueLong(index,&result));
				if(!attempt)
				{
					REQUIRE(IsWindow(window) && result == 41 && binder->clicks == 2 && !binder->closed);
					binder->cancel = FALSE;
				}
				else
				{
					REQUIRE(!IsWindow(window) && result == IDCANCEL && binder->clicks == 3 && binder->closed == 1);
				}
			}
			form.Release();
		}
	}
	hr = S_OK;
Cleanup:
	if(form)
	{
		form->Destroy();
	}
	return hr;
}

static HWND s_imageButtonModal;
static VOID CALLBACK ImageButtonModalTimer(HWND, UINT, UINT_PTR timer, DWORD)
{
	KillTimer(NULL,timer);
	SendMessageW(s_imageButtonModal,BM_CLICK,0,0);
}

static HRESULT ImageButtonNativeTests()
{
	HRESULT hr;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONObject> saved;
	TStackRef<IJSONValue> picture;
	TStackRef<IQuadooForm> form;
	TStackRef<IQuadooControl> button;
	TStackRef<IQuadooControl> edit;
	TStackRef<IQuadooButton> click;
	TStackRef<IQuadooFormKeyboard> keyboard;
	TStackRef<IQuadooWinControl> native;
	TStackRef<IQuadooLayoutControl> layout;
	TStackRef<IQuadooFormSerializer> serializer;
	TStackRef<CKeyboardBinder> binder;
	HWND window = NULL;
	HWND editWindow;
	UINT_PTR timer = 0;
	CImageTestCanvas canvas;
	Check(ButtonFormCompletionTests());
	binder.Attach(__new CKeyboardBinder);
	CheckAlloc(binder);
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"ImageButtonTest\",\"children\":[{\"type\":\"ImageButton\",\"name\":\"Pictures\",\"properties\":{\"Width\":100,\"Height\":56,\"Default\":true,\"Cancel\":true},\"events\":{\"OnClick\":\"Click\"}},{\"type\":\"TextBox\",\"name\":\"Edit\",\"properties\":{\"Top\":70}}]}",&definition));
	Check(QuadooCreateForm(binder,NULL,definition,&form));
	Check(form->FindControl(RSTRING_CAST(L"Pictures"),&button));
	Check(form->FindControl(RSTRING_CAST(L"Edit"),&edit));
	Check(form->QueryInterface(__uuidof(IQuadooFormKeyboard),reinterpret_cast<PVOID*>(&keyboard)));
	Check(button->QueryInterface(__uuidof(IQuadooButton),reinterpret_cast<PVOID*>(&click)));
	Check(button->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native)));
	Check(native->GetHandle(&window));
	REQUIRE((GetWindowLongW(window,GWL_STYLE) & BS_TYPEMASK) == BS_OWNERDRAW);
	const IID* paths[] = {&__uuidof(IQuadooButton),&__uuidof(IQuadooButtonPresentation),&__uuidof(IQuadooWinControl),&__uuidof(IQuadooProperties)};
	Check(CheckCOMIdentity(button,paths,ARRAYSIZE(paths)));
	Check(form->ShowForm(SW_SHOW));
	Check(KeyWindow(edit,&editWindow));
	SetFocus(editWindow);
	Check(KeyDispatch(keyboard,editWindow,WM_KEYDOWN,VK_RETURN));
	Check(KeyDispatch(keyboard,editWindow,WM_KEYDOWN,VK_ESCAPE));
	REQUIRE(binder->clicks == 2 && binder->clicked == RSTRING_CAST(L"Pictures"));
	Check(click->Click());
	REQUIRE(binder->clicks == 3);
	SendMessageW(window,BM_SETSTYLE,BS_DEFPUSHBUTTON,TRUE);
	REQUIRE((GetWindowLongW(window,GWL_STYLE) & BS_TYPEMASK) == BS_OWNERDRAW);
	REQUIRE(SendMessageW(window,WM_GETDLGCODE,0,0) & DLGC_DEFPUSHBUTTON);
	SetFocus(window);
	Check(KeyDispatch(keyboard,window,WM_KEYDOWN,VK_SPACE));
	Check(KeyDispatch(keyboard,window,WM_KEYUP,VK_SPACE));
	REQUIRE(binder->clicks == 4);
	Check(ImageFixtureValue(L"png",ImageStripes,sizeof(ImageStripes),&picture));
	Check(ImageSet(button,L"Picture",picture));
	Check(canvas.Create(160,100));
	SendMessageW(window,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(canvas.DC()),PRF_CLIENT);
	COLORREF enabled = canvas.Pixel(44,27);
	Check(button->SetEnabled(FALSE));
	SendMessageW(window,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(canvas.DC()),PRF_CLIENT);
	COLORREF disabled = canvas.Pixel(44,27);
	REQUIRE(enabled != disabled && GetRValue(disabled) == GetGValue(disabled) && GetGValue(disabled) == GetBValue(disabled));
	Check(button->SetEnabled(TRUE));
	Check(form->SetEnabled(FALSE));
	SendMessageW(window,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(canvas.DC()),PRF_CLIENT);
	disabled = canvas.Pixel(44,27);
	REQUIRE(GetRValue(disabled) == GetGValue(disabled) && GetGValue(disabled) == GetBValue(disabled));
	Check(form->SetEnabled(TRUE));
	Check(button->QueryInterface(__uuidof(IQuadooLayoutControl),reinterpret_cast<PVOID*>(&layout)));
	QuadooForm::Rect bounds;
	Check(button->GetBounds(&bounds));
	Check(layout->Arrange(&bounds,144));
	RECT physical;
	GetWindowRect(window,&physical);
	REQUIRE(physical.right - physical.left == 150 && ImageInteger(button,L"ImageWidth") == 16);
	Check(QuadooGetFormSerializer(&serializer));
	Check(serializer->Serialize(form,&saved));
	REQUIRE(binder->clicks == 4);
	Check(ImageNumber(button,L"ModalResult",IDOK));
	s_imageButtonModal = window;
	timer = SetTimer(NULL,0,40,ImageButtonModalTimer);
	CheckIf(!timer,HrEnsureLastError());
	LRESULT result;
	Check(form->ShowModal(&result));
	REQUIRE(result == IDOK && binder->clicks == 5 && !IsWindow(window));
	hr = S_OK;
Cleanup:
	if(timer)
	{
		KillTimer(NULL,timer);
	}
	s_imageButtonModal = NULL;
	if(form)
	{
		form->Destroy();
	}
	return hr;
}
