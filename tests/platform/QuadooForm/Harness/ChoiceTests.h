// Included by Harness.cpp: native interaction tests use the published ABI only.
static const WCHAR ChoiceExampleDefinition[] =
	L"{\"type\":\"Form\",\"name\":\"ChoiceExample\",\"properties\":{\"Caption\":\"QuadooForm choice controls\",\"Width\":470,\"Height\":245},\"children\":["
	L"{\"type\":\"GroupBox\",\"name\":\"options\",\"properties\":{\"Caption\":\"Options\",\"Left\":12,\"Top\":12,\"Width\":220,\"Height\":220},\"children\":["
	L"{\"type\":\"CheckBox\",\"name\":\"enabled\",\"properties\":{\"Caption\":\"&Enable updates\",\"Left\":12,\"Top\":24,\"Width\":195}},"
	L"{\"type\":\"CheckBox\",\"name\":\"inherited\",\"properties\":{\"Caption\":\"&Inherited setting\",\"Left\":12,\"Top\":56,\"Width\":195,\"ThreeState\":true,\"CheckState\":2,\"TabOrder\":1}},"
	L"{\"type\":\"RadioButton\",\"name\":\"automatic\",\"properties\":{\"Caption\":\"&Automatic\",\"Left\":12,\"Top\":110,\"Width\":195,\"Checked\":true,\"TabOrder\":2}},"
	L"{\"type\":\"RadioButton\",\"name\":\"manual\",\"properties\":{\"Caption\":\"&Manual\",\"Left\":12,\"Top\":142,\"Width\":195,\"TabOrder\":3}}]},"
	L"{\"type\":\"GroupBox\",\"name\":\"destination\",\"properties\":{\"Caption\":\"Destination\",\"Left\":244,\"Top\":12,\"Width\":214,\"Height\":220,\"TabOrder\":1},\"children\":["
	L"{\"type\":\"RadioButton\",\"name\":\"local\",\"properties\":{\"Caption\":\"&Local\",\"Left\":12,\"Top\":24,\"Width\":190,\"Checked\":true}},"
	L"{\"type\":\"RadioButton\",\"name\":\"remote\",\"properties\":{\"Caption\":\"&Remote\",\"Left\":12,\"Top\":56,\"Width\":190,\"TabOrder\":1}},"
	L"{\"type\":\"Label\",\"name\":\"hint\",\"properties\":{\"Caption\":\"Tab enters each group.\\nArrow keys select a radio.\\nSpace toggles a check box.\",\"Left\":12,\"Top\":112,\"AutoSize\":true}}]}]}";
static HRESULT ChoiceBool(IQuadooComponent* component, PCWSTR name, BOOL value)
{
	HRESULT hr; TStackRef<IQuadooProperties> properties; UINT index;
	Check(Property(component, name, &properties, &index)); Check(properties->SetValueBool(index, value));
Cleanup:
	return hr;
}
static INT ChoiceInt(IQuadooComponent* component, PCWSTR name)
{
	TStackRef<IQuadooProperties> properties; UINT index; INT value = -1;
	if(SUCCEEDED(Property(component, name, &properties, &index))) properties->GetValueInt(index, &value);
	return value;
}
static HRESULT ChoiceWindow(IQuadooControlContainer* parent, PCWSTR name, IQuadooWinControl** control, HWND* hwnd)
{
	HRESULT hr; TStackRef<IQuadooControl> child;
	Check(parent->FindControl(RSTRING_CAST(name), &child));
	Check(child->QueryInterface(__uuidof(IQuadooWinControl), reinterpret_cast<PVOID*>(control)));
	Check((*control)->GetHandle(hwnd));
Cleanup:
	return hr;
}
class CChoiceBinder : public CBaseUnknown, public IQuadooEventsBinder, public IQuadooDelegate
{
public:
	UINT changes, clicks, keys;
	BOOL failChange, destroyChange, handleKeys, initCheck, resizeGroup;
	IQuadooRadioButton* reenter; // Test-owned, borrowed.
	HRESULT reentered;
	IQuadooConstructionContext* createContext;
	IQuadooControlContainer* createParent;
	BOOL createChecked;
	HRESULT createResult;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooEventsBinder)
		UNK_INTERFACE(IQuadooDelegate)
	END_UNK_MAP
	CChoiceBinder() : changes(0), clicks(0), keys(0), failChange(FALSE), destroyChange(FALSE), handleKeys(FALSE), initCheck(FALSE), resizeGroup(FALSE), reenter(NULL), reentered(S_OK), createContext(NULL), createParent(NULL), createChecked(FALSE), createResult(S_OK) {}
	STDMETHOD(BindEvent)(RSTRING name, IQuadooDelegate** delegate)
	{
		if(!delegate) return E_POINTER;
		*delegate = NULL;
		if(CRString(name) == RSTRING_CAST(L"Missing")) return QuadooForm::EHandlerNotFound;
		*delegate = this; AddRef(); return S_OK;
	}
	STDMETHOD(PostBind)(IQuadooControl*) { return S_OK; }
	STDMETHOD(Invoke)(IQuadooEventArgs* args)
	{
		HRESULT hr;
		CRString name;
		Check(args->GetEventName(&name));
		if(name == RSTRING_CAST(L"OnChange"))
		{
			changes++;
			if(reenter) reentered = reenter->SetChecked(TRUE);
			if(createChecked)
			{
				TStackRef<IJSONObject> definition; TStackRef<IQuadooControl> control;
				createChecked = FALSE;
				Check(Parse(L"{\"type\":\"RadioButton\",\"name\":\"reentrantRadio\",\"properties\":{\"Checked\":true}}", &definition));
				createResult = QuadooCreateControl(createContext, createParent, definition, &control);
				if(control) control->Destroy();
			}
			if(destroyChange) { TStackRef<IQuadooComponent> sender; Check(args->GetSender(&sender)); Check(sender->Destroy()); }
			CheckIf(failChange, E_ABORT);
		}
		else if(name == RSTRING_CAST(L"OnClick")) clicks++;
		else if(name == RSTRING_CAST(L"OnKeyDown")) { keys++; if(handleKeys) { Check(args->SetBool(1, TRUE)); } }
		else if(name == RSTRING_CAST(L"OnCreate") && initCheck)
		{
			TStackRef<IQuadooComponent> sender; TStackRef<IQuadooWinControl> control; HWND hwnd;
			Check(args->GetSender(&sender)); Check(sender->QueryInterface(__uuidof(IQuadooWinControl), reinterpret_cast<PVOID*>(&control)));
			Check(control->GetHandle(&hwnd)); SendMessageW(hwnd, BM_SETCHECK, BST_CHECKED, 0);
		}
		else if(name == RSTRING_CAST(L"OnCreate") && resizeGroup)
		{
			TStackRef<IQuadooComponent> sender; TStackRef<IQuadooControlContainer> container; TStackRef<IQuadooControl> group;
			Check(args->GetSender(&sender)); Check(sender->QueryInterface(__uuidof(IQuadooControlContainer), reinterpret_cast<PVOID*>(&container)));
			Check(container->FindControl(RSTRING_CAST(L"group"), &group)); Check(SetInt(group, L"Width", 280));
		}
	Cleanup:
		return hr;
	}
};
static const WCHAR ChoiceDefinition[] =
	L"{\"type\":\"Form\",\"name\":\"Choices\",\"properties\":{\"Width\":520,\"Height\":420},\"children\":["
	L"{\"type\":\"GroupBox\",\"name\":\"group\",\"properties\":{\"Caption\":\"Options\",\"Width\":240,\"Height\":250},\"children\":["
	L"{\"type\":\"CheckBox\",\"name\":\"check\",\"properties\":{\"Left\":12,\"Top\":24,\"Caption\":\"Enable\",\"TabOrder\":1},\"events\":{\"OnChange\":\"Change\",\"OnClick\":\"Click\",\"OnKeyDown\":\"Key\"}},"
	L"{\"type\":\"CheckBox\",\"name\":\"tri\",\"properties\":{\"Left\":12,\"Top\":54,\"ThreeState\":true,\"CheckState\":\"Indeterminate\",\"TabOrder\":2},\"events\":{\"OnChange\":\"Change\",\"OnClick\":\"Click\"}},"
	L"{\"type\":\"RadioButton\",\"name\":\"a\",\"properties\":{\"Left\":12,\"Top\":85,\"Checked\":true,\"TabOrder\":3},\"events\":{\"OnChange\":\"Change\",\"OnClick\":\"Click\",\"OnKeyDown\":\"Key\"}},"
	L"{\"type\":\"Button\",\"name\":\"between\",\"properties\":{\"Left\":120,\"Top\":85,\"TabOrder\":4}},"
	L"{\"type\":\"RadioButton\",\"name\":\"b\",\"properties\":{\"Left\":12,\"Top\":115,\"TabOrder\":5},\"events\":{\"OnChange\":\"Change\",\"OnClick\":\"Click\",\"OnKeyDown\":\"Key\"}},"
	L"{\"type\":\"RadioButton\",\"name\":\"disabled\",\"properties\":{\"Top\":145,\"Enabled\":false,\"TabOrder\":6}},"
	L"{\"type\":\"RadioButton\",\"name\":\"hidden\",\"properties\":{\"Top\":175,\"Visible\":false,\"TabOrder\":7}},"
	L"{\"type\":\"Label\",\"name\":\"fill\",\"properties\":{\"Align\":\"Client\",\"Caption\":\"Client\"}},"
	L"{\"type\":\"Panel\",\"name\":\"nested\",\"properties\":{\"Left\":120,\"Top\":120,\"Width\":100,\"Height\":100},\"children\":["
	L"{\"type\":\"RadioButton\",\"name\":\"inner\",\"properties\":{\"Checked\":true}}]}]},"
	L"{\"type\":\"GroupBox\",\"name\":\"other\",\"properties\":{\"Left\":260,\"Width\":240},\"children\":["
	L"{\"type\":\"RadioButton\",\"name\":\"separate\",\"properties\":{\"Checked\":true,\"Top\":24}}]}]}";

static HRESULT ChoiceInteraction()
{
	HRESULT hr;
	TStackRef<CChoiceBinder> binder;
	TStackRef<IJSONObject> definition;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IQuadooControl> root, inner, separate;
	TStackRef<IQuadooForm> form;
	TStackRef<IQuadooWinControl> check, tri, aWindow, bWindow;
	TStackRef<IQuadooRadioButton> a, b;
	TStackRef<IQuadooProperties> values;
	TStackRef<IQuadooPropertyDescriptors> descriptors;
	TStackRef<IQuadooPropertyDescriptor> descriptor;
	TStackRef<IQuadooPropertyNotifications> notifications;
	TStackRef<CObserver> observer;
	HWND mainWindow, checkWindow, triWindow, aw, bw, after;
	UINT index, count, changed, clicked;
	BOOL checked;
	binder.Attach(__new CChoiceBinder); CheckAlloc(binder);
	Check(Parse(ChoiceDefinition, &definition)); Check(Context(96, binder, &context)); Check(QuadooCreateControl(context, NULL, definition, &root));
	Check(root->QueryInterface(__uuidof(IQuadooForm), reinterpret_cast<PVOID*>(&form))); Check(form->GetHandle(&mainWindow));
	Check(ChoiceWindow(form, L"check", &check, &checkWindow)); Check(ChoiceWindow(form, L"tri", &tri, &triWindow));
	Check(ChoiceWindow(form, L"a", &aWindow, &aw)); Check(ChoiceWindow(form, L"b", &bWindow, &bw));
	Check(aWindow->QueryInterface(__uuidof(IQuadooRadioButton), reinterpret_cast<PVOID*>(&a)));
	Check(bWindow->QueryInterface(__uuidof(IQuadooRadioButton), reinterpret_cast<PVOID*>(&b)));
	REQUIRE((SendMessageW(aw, WM_GETDLGCODE, 0, 0) & (DLGC_RADIOBUTTON | DLGC_BUTTON | DLGC_WANTARROWS)) == (DLGC_RADIOBUTTON | DLGC_BUTTON | DLGC_WANTARROWS));
	Check(form->FindControl(RSTRING_CAST(L"inner"), &inner)); Check(form->FindControl(RSTRING_CAST(L"separate"), &separate));
	REQUIRE(binder->changes == 0 && binder->clicks == 0);
	Check(Property(check, L"CheckState", &values, &index)); Check(values->GetDescriptors(&descriptors));
	Check(descriptors->GetAt(index, &descriptor)); Check(descriptor->GetEnumCount(&count)); REQUIRE(count == 3);
	for(UINT n = 0; n < count; n++) { CRString name; LONGLONG number; Check(descriptor->GetEnumValue(n, &name, &number)); REQUIRE(number == n && RStrLen(name) > 0); }
	REQUIRE(values->SetValueInt(index, -1) == E_INVALIDARG); REQUIRE(values->SetValueInt(index, 3) == E_INVALIDARG);
	REQUIRE(values->SetValueInt(index, 2) == E_INVALIDARG && ChoiceInt(check, L"CheckState") == 0);
	Check(check->QueryInterface(__uuidof(IQuadooPropertyNotifications), reinterpret_cast<PVOID*>(&notifications)));
	observer.Attach(__new CObserver); CheckAlloc(observer); Check(notifications->AddObserver(observer));
	SendMessageW(checkWindow, BM_CLICK, 0, 0); REQUIRE(ChoiceInt(check, L"CheckState") == 1 && SendMessageW(checkWindow, BM_GETCHECK, 0, 0) == BST_CHECKED);
	REQUIRE(binder->changes == 1 && binder->clicks == 1 && observer->properties == 1);
	REQUIRE(values->SetValueInt(index, 1) == S_FALSE && binder->changes == 1 && observer->properties == 1);
	SendMessageW(checkWindow, BM_CLICK, 0, 0); REQUIRE(ChoiceInt(check, L"CheckState") == 0 && binder->changes == 2 && binder->clicks == 2);
	Check(ChoiceBool(check, L"ThreeState", TRUE)); REQUIRE((GetWindowLongW(checkWindow, GWL_STYLE) & BS_TYPEMASK) == BS_3STATE);
	for(UINT n = 0; n < 3; n++) { SendMessageW(checkWindow, BM_CLICK, 0, 0); REQUIRE(ChoiceInt(check, L"CheckState") == static_cast<INT>((n + 1) % 3)); }
	SendMessageW(triWindow, BM_CLICK, 0, 0); REQUIRE(ChoiceInt(tri, L"CheckState") == 0);
	SendMessageW(triWindow, BM_SETCHECK, BST_INDETERMINATE, 0); REQUIRE(ChoiceInt(tri, L"CheckState") == 2);
	REQUIRE(ChoiceBool(tri, L"ThreeState", FALSE) == E_INVALIDARG && ChoiceInt(tri, L"CheckState") == 2);
	Check(SetInt(tri, L"CheckState", 0)); Check(ChoiceBool(tri, L"ThreeState", FALSE));
	SendMessageW(triWindow, BM_SETCHECK, BST_INDETERMINATE, 0); REQUIRE(ChoiceInt(tri, L"CheckState") == 0);
	Check(form->ShowForm(SW_SHOWNOACTIVATE)); Check(check->SetFocus());
	{
		TStackRef<IQuadooWinControl> nested; HWND nestedWindow;
		Check(inner->QueryInterface(__uuidof(IQuadooWinControl), reinterpret_cast<PVOID*>(&nested))); Check(nested->GetHandle(&nestedWindow));
		REQUIRE(GetNextDlgTabItem(mainWindow, NULL, FALSE) == nestedWindow);
		REQUIRE(GetNextDlgTabItem(mainWindow, nestedWindow, FALSE) == checkWindow);
		REQUIRE(GetNextDlgTabItem(mainWindow, triWindow, FALSE) == aw);
	}
	changed = binder->changes; clicked = binder->clicks;
	SendMessageW(checkWindow, WM_KEYDOWN, VK_SPACE, 0); SendMessageW(checkWindow, WM_KEYUP, VK_SPACE, 0);
	REQUIRE(ChoiceInt(check, L"CheckState") == 1 && binder->changes == changed + 1 && binder->clicks == clicked + 1);
	Check(ChoiceBool(check, L"ThreeState", FALSE)); changed = binder->changes; clicked = binder->clicks;
	SendMessageW(checkWindow, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(5, 5)); SendMessageW(checkWindow, WM_LBUTTONUP, 0, MAKELPARAM(5, 5));
	REQUIRE(ChoiceInt(check, L"CheckState") == 0 && binder->changes == changed + 1 && binder->clicks == clicked + 1);
	changed = binder->changes; clicked = binder->clicks;
	Check(b->SetChecked(TRUE)); Check(a->GetChecked(&checked)); REQUIRE(!checked); Check(b->GetChecked(&checked)); REQUIRE(checked);
	{ TStackRef<IQuadooProperties> properties; UINT property; INT number; Check(Property(inner, L"Checked", &properties, &property)); REQUIRE(properties->GetValueInt(property, &number) == DISP_E_TYPEMISMATCH); }
	{ TStackRef<IQuadooRadioButton> radio; Check(inner->QueryInterface(__uuidof(IQuadooRadioButton), reinterpret_cast<PVOID*>(&radio))); Check(radio->GetChecked(&checked)); REQUIRE(checked); }
	{ TStackRef<IQuadooRadioButton> radio; Check(separate->QueryInterface(__uuidof(IQuadooRadioButton), reinterpret_cast<PVOID*>(&radio))); Check(radio->GetChecked(&checked)); REQUIRE(checked); }
	REQUIRE(binder->changes == changed + 2 && binder->clicks == clicked);
	REQUIRE(b->SetChecked(TRUE) == S_FALSE && binder->changes == changed + 2);
	REQUIRE(!(GetWindowLongW(aw, GWL_STYLE) & WS_TABSTOP) && (GetWindowLongW(bw, GWL_STYLE) & WS_TABSTOP));
	SendMessageW(aw, BM_CLICK, 0, 0); REQUIRE(binder->changes == changed + 4 && binder->clicks == clicked + 1);
	SendMessageW(aw, BM_CLICK, 0, 0); REQUIRE(binder->changes == changed + 4 && binder->clicks == clicked + 2);
	Check(a->SetFocus()); changed = binder->changes; clicked = binder->clicks;
	SendMessageW(aw, WM_KEYDOWN, VK_SPACE, 0); SendMessageW(aw, WM_KEYUP, VK_SPACE, 0);
	REQUIRE(binder->changes == changed && binder->clicks == clicked + 1);
	Check(a->SetFocus()); changed = binder->changes; clicked = binder->clicks;
	{ MSG message; ZeroMemory(&message, sizeof(message)); message.hwnd = aw; message.message = WM_KEYDOWN; message.wParam = VK_RIGHT; REQUIRE(IsDialogMessageW(mainWindow, &message)); }
	Check(b->GetChecked(&checked)); REQUIRE(checked && GetFocus() == bw); REQUIRE(binder->changes == changed + 2 && binder->clicks == clicked + 1);
	// Wrap skips disabled and hidden radios, and never enters a nested container.
	SendMessageW(bw, WM_KEYDOWN, VK_RIGHT, 0); Check(a->GetChecked(&checked)); REQUIRE(checked && GetFocus() == aw);
	binder->handleKeys = TRUE; SendMessageW(aw, WM_KEYDOWN, VK_RIGHT, 0); Check(a->GetChecked(&checked)); REQUIRE(checked && GetFocus() == aw); binder->handleKeys = FALSE;
	Check(a->SetTabOrder(9)); SendMessageW(aw, WM_KEYDOWN, VK_LEFT, 0); Check(b->GetChecked(&checked)); REQUIRE(checked && GetFocus() == bw);
	binder->reenter = b; binder->reentered = S_OK; Check(a->SetChecked(TRUE)); REQUIRE(binder->reentered == QuadooForm::EInvalidState); binder->reenter = NULL;
	Check(a->GetChecked(&checked)); REQUIRE(checked); Check(b->GetChecked(&checked)); REQUIRE(!checked);
	{
		TStackRef<IQuadooControlContainer> parent; UINT before, afterCount; Check(a->GetParent(&parent)); Check(parent->GetChildCount(&before));
		binder->createContext = context; binder->createParent = parent; binder->createChecked = TRUE;
		Check(b->SetChecked(TRUE)); REQUIRE(binder->createResult == QuadooForm::EInvalidState);
		binder->createContext = NULL; binder->createParent = NULL;
		Check(parent->GetChildCount(&afterCount)); REQUIRE(afterCount == before); Check(a->SetChecked(TRUE));
	}
	binder->failChange = TRUE; REQUIRE(b->SetChecked(TRUE) == E_ABORT); binder->failChange = FALSE;
	Check(b->GetChecked(&checked)); REQUIRE(checked); Check(a->GetChecked(&checked)); REQUIRE(!checked); Check(a->SetChecked(TRUE));
	Check(a->SetChecked(FALSE)); REQUIRE((GetWindowLongW(bw, GWL_STYLE) & WS_TABSTOP) && !(GetWindowLongW(aw, GWL_STYLE) & WS_TABSTOP));
	SendMessageW(bw, BM_SETCHECK, BST_CHECKED, 0); Check(b->GetChecked(&checked)); REQUIRE(checked);
	SendMessageW(bw, BM_SETCHECK, BST_INDETERMINATE, 0); Check(b->GetChecked(&checked)); REQUIRE(checked);
	// Destroying in OnChange must not continue to OnClick or touch the dead HWND.
	binder->destroyChange = TRUE; clicked = binder->clicks; SendMessageW(checkWindow, BM_CLICK, 0, 0); binder->destroyChange = FALSE;
	REQUIRE(!IsWindow(checkWindow) && binder->clicks == clicked && check->GetHandle(&after) == QuadooForm::EDestroyed);
Cleanup:
	if(binder) { binder->reenter = NULL; binder->createParent = NULL; binder->createContext = NULL; }
	if(root) root->Destroy();
	return hr;
}

static HRESULT ChoiceGroupBox()
{
	HRESULT hr;
	TStackRef<CChoiceBinder> binder;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooControlFactory> factory;
	TStackRef<IQuadooFactoryLifetime> lifetime;
	TStackRef<IQuadooControlFactory> groupFactory;
	TStackRef<IQuadooFactoryLifetime> groupLifetime;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IJSONObject> definition, serialized, copied;
	TStackRef<IQuadooControl> root, groupControl, fill, meter, copy;
	TStackRef<IQuadooForm> form;
	TStackRef<IQuadooControlContainer> group;
	TStackRef<IQuadooLayoutControl> layout;
	TStackRef<IExternalMeter> external;
	TStackRef<IQuadooFormSerializer> serializer;
	TStackRef<IJSONValue> first, second;
	QuadooForm::Rect bounds, effective, stored;
	UINT count, dpi, arranges;
	INT accepted;
	HWND groupWindow, rootWindow;
	HDC dc = NULL;
	HDC screen = NULL;
	HBITMAP bitmap = NULL;
	HGDIOBJ old = NULL;
	binder.Attach(__new CChoiceBinder); CheckAlloc(binder);
	Check(QuadooGetControlRegistry(&registry)); Check(CreateExternalFactory(&factory)); Check(registry->RegisterFactory(factory));
	Check(factory->QueryInterface(__uuidof(IQuadooFactoryLifetime), reinterpret_cast<PVOID*>(&lifetime)));
	Check(registry->FindFactory(RSTRING_CAST(L"GroupBox"), &groupFactory));
	Check(groupFactory->QueryInterface(__uuidof(IQuadooFactoryLifetime), reinterpret_cast<PVOID*>(&groupLifetime)));
	Check(Context(96, binder, &context, registry)); Check(Parse(ChoiceDefinition, &definition)); Check(QuadooCreateControl(context, NULL, definition, &root));
	Check(root->QueryInterface(__uuidof(IQuadooForm), reinterpret_cast<PVOID*>(&form))); Check(form->GetHandle(&rootWindow));
	Check(form->FindControl(RSTRING_CAST(L"group"), &groupControl)); Check(groupControl->QueryInterface(__uuidof(IQuadooControlContainer), reinterpret_cast<PVOID*>(&group)));
	Check(group->GetHandle(&groupWindow)); REQUIRE((GetWindowLongW(groupWindow, GWL_STYLE) & BS_TYPEMASK) == BS_GROUPBOX);
	REQUIRE((GetWindowLongW(groupWindow, GWL_EXSTYLE) & WS_EX_CONTROLPARENT) && !(GetWindowLongW(groupWindow, GWL_STYLE) & WS_TABSTOP));
	Check(form->FindControl(RSTRING_CAST(L"fill"), &fill)); Check(fill->GetBounds(&stored)); Check(group->GetBounds(&bounds));
	Check(group->QueryInterface(__uuidof(IQuadooLayoutControl), reinterpret_cast<PVOID*>(&layout)));
	for(UINT scale = 96; scale <= 192; scale += 48)
	{
		Check(layout->Arrange(&bounds, scale)); Check(fill->GetEffectiveBounds(&effective));
		REQUIRE(effective.Left == 8 && effective.Top > 12 && effective.Width == bounds.Width - 16 && effective.Height == bounds.Height - effective.Top - 8);
		QuadooForm::Rect logical; Check(fill->GetBounds(&logical)); REQUIRE(EqualBounds(logical, stored));
		TStackRef<IQuadooWinControl> check; HWND child; RECT physical;
		Check(ChoiceWindow(form, L"check", &check, &child)); REQUIRE(GetParent(child) == groupWindow);
		GetWindowRect(child, &physical); MapWindowPoints(NULL, groupWindow, reinterpret_cast<POINT*>(&physical), 2);
		REQUIRE(physical.left == MulDiv(12, scale, 96) && physical.top == MulDiv(24, scale, 96));
	}
	Check(layout->Arrange(&bounds, 96));
	// Explicit batches still defer descendants; top-down layout after EndUpdate
	// must update a ready GroupBox and its existing painted children.
	Check(fill->GetEffectiveBounds(&effective));
	Check(form->BeginUpdate()); Check(SetInt(group, L"Width", 280));
	{ QuadooForm::Rect pending; Check(fill->GetEffectiveBounds(&pending)); REQUIRE(EqualBounds(pending, effective)); }
	Check(form->EndUpdate()); Check(fill->GetEffectiveBounds(&effective)); REQUIRE(effective.Width == 264);
	Check(SetInt(group, L"Width", 240));
	{
		TStackRef<IQuadooProperties> properties; TStackRef<IJSONObject> object; TStackRef<IJSONValue> value; UINT index;
		Check(Property(group, L"Font", &properties, &index)); Check(Parse(L"{\"Size\":18}", &object)); Check(JSONWrapObject(object, &value));
		Check(fill->GetEffectiveBounds(&effective)); INT originalTop = effective.Top;
		Check(properties->SetValueJSON(index, value)); Check(fill->GetEffectiveBounds(&effective)); REQUIRE(effective.Top > originalTop + 8);
		value.Release(); object.Release(); Check(Parse(L"{\"Size\":9}", &object)); Check(JSONWrapObject(object, &value)); Check(properties->SetValueJSON(index, value));
		Check(fill->GetEffectiveBounds(&effective)); REQUIRE(effective.Top == originalTop);
		properties.Release(); value.Release(); object.Release();
		Check(Property(group, L"Padding", &properties, &index)); Check(Parse(L"{\"Left\":2,\"Top\":3,\"Right\":4,\"Bottom\":5}", &object));
		Check(JSONWrapObject(object, &value)); Check(properties->SetValueJSON(index, value));
		Check(fill->GetEffectiveBounds(&effective)); REQUIRE(effective.Left == 10 && effective.Top == originalTop + 3 && effective.Width == 218);
		value.Release(); object.Release(); Check(JSONCreateObject(&object)); Check(JSONWrapObject(object, &value)); Check(properties->SetValueJSON(index, value));
		properties.Release(); Check(Property(group, L"Caption", &properties, &index)); Check(properties->SetValueString(index, RSTRING_CAST(L"Edited options")));
		WCHAR caption[64]; GetWindowTextW(groupWindow, caption, ARRAYSIZE(caption)); REQUIRE(0 == TStrCmpAssert(caption, L"Edited options"));
	}
	// Native group frame/caption and framework-painted children share a DC.
	dc = CreateCompatibleDC(NULL); CheckIf(!dc, HrEnsureLastError());
	screen = GetDC(NULL); CheckIf(!screen, HrEnsureLastError());
	bitmap = CreateCompatibleBitmap(screen, bounds.Width, bounds.Height); CheckIf(!bitmap, HrEnsureLastError());
	ReleaseDC(NULL, screen); screen = NULL;
	old = SelectObject(dc, bitmap);
	SendMessageW(groupWindow, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT);
	{ COLORREF background = GetPixel(dc, 0, bounds.Height / 2); BOOL frame = FALSE;
		for(INT x = 0; x < 6; x++) if(GetPixel(dc, x, bounds.Height / 2) != background) frame = TRUE;
		REQUIRE(background != CLR_INVALID && frame); }
	SelectObject(dc, old); old = NULL; DeleteObject(bitmap); bitmap = NULL; DeleteDC(dc); dc = NULL;
	// A third-party painted control can be constructed, laid out, inspected,
	// bound, hit-tested and serialized inside a native GroupBox.
	Check(SetInt(fill, L"Align", 0)); definition.Release();
	Check(Parse(L"{\"type\":\"ExternalMeter\",\"name\":\"meterInGroup\",\"properties\":{\"Align\":5,\"Level\":31},\"events\":{\"OnPulse\":\"Pulse\",\"OnFinalizing\":\"Final\"}}", &definition));
	Check(QuadooCreateControl(context, group, definition, &meter)); Check(meter->QueryInterface(__uuidof(IExternalMeter), reinterpret_cast<PVOID*>(&external)));
	Check(SetInt(meter, L"Level", 47)); REQUIRE(ChoiceInt(meter, L"Level") == 47);
	Check(meter->GetEffectiveBounds(&effective));
	REQUIRE(effective.Left == 8 && effective.Top > 12 && effective.Width == 224);
	Check(external->GetArrangeCount(&arranges, &dpi)); REQUIRE(arranges > 0 && dpi == 96);
	Check(external->Pulse(5, &accepted)); REQUIRE(accepted == 5);
	{ UINT messages = ExternalStatistics().messages; SendMessageW(groupWindow, WM_LBUTTONUP, 0, MAKELPARAM(effective.Left + 1, effective.Top + 1)); REQUIRE(ExternalStatistics().messages == messages + 1); }
	{ TStackRef<IQuadooControlContainer> parent; Check(meter->GetParent(&parent)); REQUIRE(parent == static_cast<IQuadooControlContainer*>(group)); }
	Check(QuadooGetFormSerializer(&serializer)); Check(serializer->Serialize(root, &serialized));
	Check(QuadooCreateControl(context, NULL, serialized, &copy)); Check(serializer->Serialize(copy, &copied));
	Check(JSONWrapObject(serialized, &first)); Check(JSONWrapObject(copied, &second)); REQUIRE(JSONCompare(first, second) == 0);
	Check(copy->Destroy()); copy.Release(); Check(lifetime->GetLiveCount(&count)); REQUIRE(count == 1);
	REQUIRE(registry->UnregisterFactory(RSTRING_CAST(L"ExternalMeter")) == HRESULT_FROM_WIN32(ERROR_BUSY));
	Check(group->Destroy()); REQUIRE(!IsWindow(groupWindow) && IsWindow(rootWindow));
	Check(lifetime->GetLiveCount(&count)); REQUIRE(count == 1); external.Release(); meter.Release(); Check(lifetime->GetLiveCount(&count)); REQUIRE(count == 0);
	Check(registry->UnregisterFactory(RSTRING_CAST(L"ExternalMeter")));
	{ TStackRef<IQuadooControl> found; REQUIRE(FAILED(form->FindControl(RSTRING_CAST(L"meterInGroup"), &found)) && !found); }
	Check(root->Destroy());
	Check(groupLifetime->GetLiveCount(&count)); REQUIRE(count == 1);
	REQUIRE(registry->UnregisterFactory(RSTRING_CAST(L"GroupBox")) == HRESULT_FROM_WIN32(ERROR_BUSY));
	layout.Release(); groupControl.Release(); group.Release(); Check(groupLifetime->GetLiveCount(&count)); REQUIRE(count == 0);
	Check(registry->UnregisterFactory(RSTRING_CAST(L"GroupBox")));
Cleanup:
	if(screen) ReleaseDC(NULL, screen);
	if(dc) { if(old) SelectObject(dc, old); DeleteDC(dc); }
	if(bitmap) DeleteObject(bitmap);
	if(copy) copy->Destroy(); if(root) root->Destroy();
	return hr;
}

static HRESULT ChoiceRollbackAndResources()
{
	HRESULT hr;
	TStackRef<CChoiceBinder> binder;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IQuadooControl> root, groupControl, failed;
	TStackRef<IQuadooControlContainer> group;
	TStackRef<IQuadooForm> form;
	TStackRef<IQuadooFormSerializer> serializer;
	TStackRef<IJSONObject> definition, before, after;
	TStackRef<IJSONValue> first, second;
	UINT count, originalCount;
	CGuiResourceMonitor resources(L"Choice control resources");
	static PCWSTR const types[] = { L"CheckBox", L"RadioButton", L"GroupBox" };
	static PCWSTR const failures[] =
	{
		L"{\"type\":\"CheckBox\",\"name\":\"bad\",\"properties\":{\"CheckState\":2}}",
		L"{\"type\":\"CheckBox\",\"name\":\"bad\",\"properties\":{\"ThreeState\":true,\"CheckState\":3}}",
		L"{\"type\":\"CheckBox\",\"name\":\"bad\",\"events\":{\"OnClick\":\"Missing\"}}",
		L"{\"type\":\"RadioButton\",\"name\":\"bad\",\"properties\":{\"Checked\":true}}",
		L"{\"type\":\"GroupBox\",\"name\":\"bad\",\"children\":[{\"type\":\"RadioButton\",\"name\":\"x\",\"properties\":{\"Checked\":true}},{\"type\":\"RadioButton\",\"name\":\"y\",\"properties\":{\"Checked\":true}}]}",
		L"{\"type\":\"GroupBox\",\"name\":\"bad\",\"children\":[{\"type\":\"CheckBox\",\"name\":\"x\",\"events\":{\"OnChange\":\"Missing\"}}]}"
	};
	binder.Attach(__new CChoiceBinder); CheckAlloc(binder);
	Check(QuadooGetControlRegistry(&registry)); Check(Context(96, binder, &context, registry));
	Check(Parse(ChoiceDefinition, &definition)); Check(QuadooCreateControl(context, NULL, definition, &root));
	Check(root->QueryInterface(__uuidof(IQuadooForm), reinterpret_cast<PVOID*>(&form)));
	Check(form->FindControl(RSTRING_CAST(L"group"), &groupControl)); Check(groupControl->QueryInterface(__uuidof(IQuadooControlContainer), reinterpret_cast<PVOID*>(&group)));
	Check(group->GetChildCount(&originalCount)); Check(QuadooGetFormSerializer(&serializer)); Check(serializer->Serialize(root, &before)); Check(JSONWrapObject(before, &first));
	for(UINT n = 0; n < ARRAYSIZE(failures); n++)
	{
		definition.Release(); Check(Parse(failures[n], &definition)); REQUIRE(FAILED(QuadooCreateControl(context, group, definition, &failed)) && !failed);
		Check(group->GetChildCount(&count)); REQUIRE(count == originalCount);
		{ TStackRef<IQuadooControl> found; REQUIRE(FAILED(form->FindControl(RSTRING_CAST(L"bad"), &found)) && !found); }
		after.Release(); second.Release(); Check(serializer->Serialize(root, &after)); Check(JSONWrapObject(after, &second)); REQUIRE(JSONCompare(first, second) == 0);
	}
	REQUIRE(binder->changes == 0 && binder->clicks == 0);
	definition.Release(); Check(Parse(L"{\"type\":\"CheckBox\",\"name\":\"init\",\"events\":{\"OnCreate\":\"Init\",\"OnChange\":\"Change\"}}", &definition));
	binder->initCheck = TRUE; Check(QuadooCreateControl(context, group, definition, &failed)); binder->initCheck = FALSE;
	REQUIRE(ChoiceInt(failed, L"CheckState") == BST_CHECKED && binder->changes == 0); Check(failed->Destroy()); failed.Release();
	definition.Release(); Check(Parse(L"{\"type\":\"CheckBox\",\"name\":\"bad\"}", &definition)); Check(QuadooCreateControl(context, group, definition, &failed)); Check(failed->Destroy()); failed.Release();
	Check(root->Destroy()); root.Release(); form.Release(); groupControl.Release(); group.Release();
	definition.Release(); Check(Parse(ChoiceDefinition, &definition));
	{
		TStackRef<IJSONObject> events; TStackRef<IJSONValue> json;
		Check(Parse(L"{\"OnCreate\":\"ResizeGroup\"}", &events)); Check(JSONWrapObject(events, &json)); Check(definition->AddValueW(L"events", json));
	}
	binder->resizeGroup = TRUE; Check(QuadooCreateControl(context, NULL, definition, &root)); binder->resizeGroup = FALSE;
	{
		TStackRef<IQuadooControlContainer> container; TStackRef<IQuadooControl> fill; QuadooForm::Rect effective;
		Check(root->QueryInterface(__uuidof(IQuadooControlContainer), reinterpret_cast<PVOID*>(&container))); Check(container->FindControl(RSTRING_CAST(L"fill"), &fill));
		Check(fill->GetEffectiveBounds(&effective)); REQUIRE(effective.Width == 264);
	}
	Check(root->Destroy()); root.Release();
	definition.Release(); Check(Parse(ChoiceExampleDefinition, &definition)); Check(QuadooCreateControl(context, NULL, definition, &root)); Check(root->Destroy()); root.Release();
	definition.Release(); Check(Parse(ChoiceDefinition, &definition));
	while(!resources.Complete())
	{
		{
			TStackRef<IQuadooControl> control; TStackRef<IQuadooForm> window; TStackRef<IQuadooLayoutControl> layout;
			TStackRef<IQuadooWinControl> check; HWND hwnd; QuadooForm::Rect bounds;
			TArray<HWND> windows; GuiResourceCounts counts;
			Check(QuadooCreateControl(context, NULL, definition, &control)); Check(control->QueryInterface(__uuidof(IQuadooForm), reinterpret_cast<PVOID*>(&window)));
			Check(window->ShowForm(SW_SHOWNOACTIVATE)); Check(ChoiceWindow(window, L"tri", &check, &hwnd)); Check(check->SetFocus()); SendMessageW(hwnd, BM_CLICK, 0, 0);
			Check(SettleGuiResources(NULL, &counts));
			Check(control->QueryInterface(__uuidof(IQuadooLayoutControl), reinterpret_cast<PVOID*>(&layout))); Check(control->GetBounds(&bounds));
			for(UINT dpi = 96; dpi <= 192; dpi += 48) { Check(layout->Arrange(&bounds, dpi)); }
			Check(CollectNativeWindows(control, windows)); Check(window->HideForm()); Check(control->Destroy());
			for(sysint n = 0; n < windows.Length(); n++) { REQUIRE(!IsWindow(windows[n])); }
		}
		for(UINT n = 0; n < ARRAYSIZE(types); n++)
		{
			TStackRef<IQuadooControlFactory> factory; TStackRef<IQuadooFactoryLifetime> lifetime;
			Check(registry->FindFactory(RSTRING_CAST(types[n]), &factory)); Check(factory->QueryInterface(__uuidof(IQuadooFactoryLifetime), reinterpret_cast<PVOID*>(&lifetime)));
			Check(lifetime->GetLiveCount(&count)); REQUIRE(count == 0);
		}
		Check(resources.AfterCycle());
	}
Cleanup:
	if(failed) failed->Destroy(); if(root) root->Destroy();
	return hr;
}
