// Published interfaces only. IRStringArray is the existing SimbeyCore collection.
static const WCHAR ItemExampleDefinition[] =
	L"{\"type\":\"Form\",\"name\":\"ItemsExample\",\"properties\":{\"Caption\":\"QuadooForm text items\",\"Width\":490,\"Height\":250},\"children\":["
	L"{\"type\":\"ComboBox\",\"name\":\"editable\",\"properties\":{\"Left\":12,\"Top\":12,\"Width\":210,\"Items\":[\"Zulu\",\"Alpha\",\"Alpha\",\"\"],\"ItemIndex\":1},\"events\":{\"OnSelectionChange\":\"Select\",\"OnChange\":\"Changed\",\"OnDestroy\":\"Destroy\",\"OnKeyDown\":\"Key\",\"OnKeyUp\":\"Key\"}},"
	L"{\"type\":\"ComboBox\",\"name\":\"selection\",\"properties\":{\"Left\":245,\"Top\":12,\"Width\":210,\"Style\":\"DropDownList\",\"Items\":[\"Zulu\",\"Alpha\",\"Alpha\",\"\"],\"ItemIndex\":2},\"events\":{\"OnSelectionChange\":\"Select\",\"OnChange\":\"Changed\",\"OnDestroy\":\"Destroy\"}},"
	L"{\"type\":\"ListBox\",\"name\":\"list\",\"properties\":{\"Left\":12,\"Top\":58,\"Width\":443,\"Height\":170,\"Anchors\":15,\"Items\":[\"Zulu\",\"Alpha\",\"Alpha\",\"\"],\"ItemIndex\":1},\"events\":{\"OnSelectionChange\":\"Select\",\"OnDestroy\":\"Destroy\"}}]}";
class CItemBinder : public CBaseUnknown, public IQuadooEventsBinder, public IQuadooDelegate
{
public:
	UINT selections, texts, destroys, keys;
	BOOL destroySelection, failSelection, reenter, handleKeys;
	HRESULT reentered;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooEventsBinder)
		UNK_INTERFACE(IQuadooDelegate)
	END_UNK_MAP
	CItemBinder() : selections(0), texts(0), destroys(0), keys(0), destroySelection(FALSE), failSelection(FALSE), reenter(FALSE), handleKeys(FALSE), reentered(S_OK) {}
	STDMETHOD(BindEvent)(RSTRING name, IQuadooDelegate** delegate)
	{ if(!delegate) return E_POINTER; *delegate = this; AddRef(); return S_OK; }
	STDMETHOD(PostBind)(IQuadooControl*) { return S_OK; }
	STDMETHOD(Invoke)(IQuadooEventArgs* args)
	{
		HRESULT hr;
		CRString event; TStackRef<IQuadooComponent> sender;
		Check(args->GetEventName(&event)); Check(args->GetSender(&sender));
		if(event == RSTRING_CAST(L"OnDestroy")) { destroys++; hr = S_OK; goto Cleanup; }
		if(event == RSTRING_CAST(L"OnKeyDown") || event == RSTRING_CAST(L"OnKeyUp"))
		{ keys++; if(handleKeys) { Check(args->SetBool(1,TRUE)); } }
		if(event == RSTRING_CAST(L"OnSelectionChange") || event == RSTRING_CAST(L"OnChange"))
		{
			TStackRef<IQuadooProperties> properties; TStackRef<IJSONValue> value; TStackRef<IJSONArray> items;
			UINT property; INT index;
			Check(Property(sender, L"Items", &properties, &property)); Check(properties->GetValueJSON(property, &value)); Check(value->GetArray(&items));
			properties.Release(); Check(Property(sender, L"ItemIndex", &properties, &property)); Check(properties->GetValueInt(property, &index));
			REQUIRE(index >= -1 && index < items->Count());
			TStackRef<IQuadooComboBox> combo;
			if(SUCCEEDED(sender->QueryInterface(__uuidof(IQuadooComboBox), reinterpret_cast<PVOID*>(&combo))) && index >= 0)
			{
				CRString selected, text; Check(items->GetString(index, &selected));
				properties.Release(); Check(Property(sender, L"Text", &properties, &property)); Check(properties->GetValueString(property, &text));
				REQUIRE(selected == static_cast<RSTRING>(text));
				HWND hwnd; CRString native; PWSTR write;
				Check(combo->GetHandle(&hwnd)); INT length = GetWindowTextLengthW(hwnd);
				Check(RStrAllocW(length,&native,&write)); GetWindowTextW(hwnd,write,length + 1);
				REQUIRE(selected == static_cast<RSTRING>(native));
			}
			if(event == RSTRING_CAST(L"OnSelectionChange"))
			{
				selections++;
				if(reenter) { properties.Release(); Check(Property(sender, L"ItemIndex", &properties, &property)); reentered = properties->SetValueInt(property, index == 0 ? 1 : 0); }
				if(destroySelection) { Check(sender->Destroy()); }
				CheckIf(failSelection, E_ABORT);
			}
			else texts++;
		}
		hr = S_OK;
	Cleanup:
		return hr;
	}
};
// A separate implementation proves setters consume the published array contract,
// without relying on SimbeyCore's concrete class, Clone, Compare, or GetArray.
class CInputItems : public CBaseUnknown, public IRStringArray
{
	TStackRef<IRStringArray> m_source;
public:
	INT failure;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IRStringArray)
	END_UNK_MAP
	CInputItems(IRStringArray* source) : m_source(source), failure(0) {}
	virtual sysint STDMETHODCALLTYPE Length() { return failure == 1 ? -1 : m_source->Length(); }
	STDMETHOD(GetRString)(sysint index, RSTRING* value)
	{ if(!value) return E_POINTER; *value = NULL; return failure == 2 && index == 1 ? E_ABORT : failure == 3 ? S_OK : m_source->GetRString(index,value); }
	STDMETHOD(GetStringW)(sysint,PWSTR,INT,INT*) { return E_NOTIMPL; }
	STDMETHOD(GetStringA)(sysint,PSTR,INT,INT*) { return E_NOTIMPL; }
	STDMETHOD(AddRString)(RSTRING) { return E_NOTIMPL; }
	STDMETHOD(AddStringW)(PCWSTR,INT) { return E_NOTIMPL; }
	STDMETHOD(AddStringA)(PCSTR,INT,INT) { return E_NOTIMPL; }
	STDMETHOD(InsertAt)(RSTRING,sysint) { return E_NOTIMPL; }
	STDMETHOD(Replace)(sysint,RSTRING) { return E_NOTIMPL; }
	STDMETHOD(Swap)(sysint,sysint) { return E_NOTIMPL; }
	virtual RSTRING* STDMETHODCALLTYPE GetArray(sysint* count) { if(count) *count = 0; return NULL; }
	STDMETHOD(RemoveString)(sysint,RSTRING*) { return E_NOTIMPL; }
	virtual VOID STDMETHODCALLTYPE RemoveAll() {}
	STDMETHOD(Compact)() { return E_NOTIMPL; }
	STDMETHOD(IndexOf)(RSTRING,sysint*,sysint) { return E_NOTIMPL; }
	STDMETHOD(Clone)(IRStringArray** copy) { if(copy) *copy = NULL; return E_NOTIMPL; }
	STDMETHOD(Compare)(IRStringArray*,INT*) { return E_NOTIMPL; }
};
static HRESULT ItemText(IQuadooComponent* control, PCWSTR text)
{
	HRESULT hr; TStackRef<IQuadooProperties> properties; UINT index;
	Check(Property(control, L"Text", &properties, &index)); Check(properties->SetValueString(index, RSTRING_CAST(text)));
Cleanup:
	return hr;
}
static HRESULT ItemString(IQuadooComponent* control, PCWSTR property, PCWSTR expected)
{
	HRESULT hr; TStackRef<IQuadooProperties> properties; UINT index; CRString text;
	Check(Property(control, property, &properties, &index)); Check(properties->GetValueString(index, &text)); REQUIRE(text == RSTRING_CAST(expected));
Cleanup:
	return hr;
}
static HRESULT CompareItemSnapshots(IRStringArray* first, IRStringArray* second, INT expected)
{
	HRESULT hr;
	INT comparison = INT_MAX;
	hr = first->Compare(second,&comparison);
	Check(hr);
	REQUIRE(hr == (expected == 0 ? S_OK : S_FALSE));
	REQUIRE((comparison < 0 ? -1 : comparison > 0 ? 1 : 0) == expected);
Cleanup:
	return hr;
}
static HRESULT ItemNative(IQuadooWinControl* control, BOOL combo, IRStringArray* expected, INT selected)
{
	HRESULT hr; HWND hwnd;
	Check(control->GetHandle(&hwnd));
	REQUIRE(SendMessageW(hwnd, combo ? CB_GETCOUNT : LB_GETCOUNT, 0, 0) == expected->Length());
	REQUIRE(SendMessageW(hwnd, combo ? CB_GETCURSEL : LB_GETCURSEL, 0, 0) == selected);
	for(sysint n = 0; n < expected->Length(); n++)
	{
		CRString text, native; PWSTR write; Check(expected->GetRString(n, &text));
		INT length = static_cast<INT>(SendMessageW(hwnd, combo ? CB_GETLBTEXTLEN : LB_GETTEXTLEN, n, 0));
		REQUIRE(length == RStrLen(text)); Check(RStrAllocW(length, &native, &write));
		REQUIRE(SendMessageW(hwnd, combo ? CB_GETLBTEXT : LB_GETTEXT, n, reinterpret_cast<LPARAM>(write)) == length);
		REQUIRE(native == static_cast<RSTRING>(text));
	}
Cleanup:
	return hr;
}
static HRESULT ItemForm(CItemBinder* binder, IQuadooConstructionContext** context, IQuadooForm** form,
	IQuadooComboBox** combo, IQuadooComboBox** fixed, IQuadooListBox** list, IQuadooControlRegistry* registry = NULL)
{
	HRESULT hr; TStackRef<IJSONObject> definition; TStackRef<IQuadooControl> root, child;
	Check(Context(96, binder, context, registry)); Check(Parse(ItemExampleDefinition, &definition)); Check(QuadooCreateControl(*context, NULL, definition, &root));
	Check(root->QueryInterface(__uuidof(IQuadooForm), reinterpret_cast<PVOID*>(form)));
	Check((*form)->FindControl(RSTRING_CAST(L"editable"), &child)); Check(child->QueryInterface(__uuidof(IQuadooComboBox), reinterpret_cast<PVOID*>(combo))); child.Release();
	Check((*form)->FindControl(RSTRING_CAST(L"selection"), &child)); Check(child->QueryInterface(__uuidof(IQuadooComboBox), reinterpret_cast<PVOID*>(fixed))); child.Release();
	Check((*form)->FindControl(RSTRING_CAST(L"list"), &child)); Check(child->QueryInterface(__uuidof(IQuadooListBox), reinterpret_cast<PVOID*>(list)));
Cleanup:
	if(FAILED(hr) && root) root->Destroy();
	return hr;
}
static HRESULT ItemInteraction()
{
	HRESULT hr;
	TStackRef<CItemBinder> binder; TStackRef<IQuadooConstructionContext> context;
	TStackRef<IQuadooForm> form; TStackRef<IQuadooComboBox> combo, fixed; TStackRef<IQuadooListBox> list;
	TStackRef<IRStringArray> snapshot, original, other;
	HWND hwnd, listWindow; INT index; UINT selections, texts;
	binder.Attach(__new CItemBinder); CheckAlloc(binder);
	Check(ItemForm(binder, &context, &form, &combo, &fixed, &list)); REQUIRE(binder->selections == 0 && binder->texts == 0);
	Check(combo->GetItems(&snapshot)); Check(combo->GetItems(&original)); Check(fixed->GetItems(&other));
	REQUIRE(snapshot->Length() == 4 && static_cast<IRStringArray*>(snapshot) != static_cast<IRStringArray*>(original));
	Check(CompareItemSnapshots(snapshot,original,0)); Check(CompareItemSnapshots(original,snapshot,0));
	// Prefix differences exercise both length-ordering paths. Comparisons must
	// leave snapshots usable and never affect the controls they came from.
	Check(snapshot->AddStringW(L"Tail",4));
	Check(CompareItemSnapshots(snapshot,original,1)); Check(CompareItemSnapshots(original,snapshot,-1));
	Check(snapshot->RemoveString(4,NULL)); Check(CompareItemSnapshots(snapshot,original,0));
	{
		TStackRef<IRStringArray> empty, duplicate; TStackRef<CInputItems> input;
		Check(ScCreateRStringArray(&empty)); Check(ScCreateRStringArray(&duplicate));
		Check(CompareItemSnapshots(empty,duplicate,0)); Check(CompareItemSnapshots(empty,original,-1)); Check(CompareItemSnapshots(original,empty,1));
		Check(duplicate->AddStringW(L"Managed",7));
		// Keep one returned string reference while comparing equal arrays. The
		// comparison must balance its temporary references, including self-compare.
		CRString text; Check(duplicate->GetRString(0,&text));
		LONG references = RStrAddRef(text); RStrRelease(text);
		for(UINT n = 0; n < 3; n++) { Check(CompareItemSnapshots(duplicate,duplicate,0)); }
		LONG after = RStrAddRef(text); RStrRelease(text); REQUIRE(after == references);
		input.Attach(__new CInputItems(original)); CheckAlloc(input); Check(CompareItemSnapshots(snapshot,input,0));
		INT comparison; input->failure = 2; REQUIRE(snapshot->Compare(input,&comparison) == E_ABORT);
		Check(CompareItemSnapshots(snapshot,original,0));
	}
	Check(ItemNative(combo, TRUE, original, 1)); Check(ItemNative(fixed, TRUE, other, 2)); Check(ItemNative(list, FALSE, original, 1));
	Check(ItemString(combo, L"Text", L"Alpha"));
	Check(snapshot->Replace(1, RSTRING_CAST(L"Edited"))); Check(snapshot->AddStringW(L"\x03a9", 1));
	Check(CompareItemSnapshots(snapshot,original,1)); Check(CompareItemSnapshots(original,snapshot,-1));
	Check(ItemNative(combo, TRUE, original, 1)); REQUIRE(binder->texts == 0); // snapshot edits have no effect
	Check(combo->GetHandle(&hwnd)); Check(combo->SetItems(snapshot)); REQUIRE(!IsWindow(hwnd));
	Check(ItemNative(combo, TRUE, snapshot, 1)); Check(ItemString(combo, L"Text", L"Edited")); REQUIRE(binder->selections == 0 && binder->texts == 1);
	REQUIRE(combo->SetItems(snapshot) == S_FALSE); REQUIRE(binder->texts == 1);
	Check(snapshot->Replace(1, RSTRING_CAST(L"Later"))); Check(ItemString(combo, L"Text", L"Edited"));
	Check(combo->SetItemIndex(2)); REQUIRE(binder->selections == 1 && binder->texts == 2);
	REQUIRE(combo->SetItemIndex(2) == S_FALSE); REQUIRE(combo->SetItemIndex(-2) == E_INVALIDARG && combo->SetItemIndex(50) == E_INVALIDARG);
	Check(ItemText(combo, L"Free text")); Check(combo->GetItemIndex(&index)); REQUIRE(index == -1 && binder->selections == 2 && binder->texts == 3);
	Check(combo->SetItemIndex(0)); Check(ItemText(combo, L"Zulu")); Check(combo->GetItemIndex(&index)); REQUIRE(index == 0);
	Check(combo->SetItemIndex(-1)); Check(ItemString(combo, L"Text", L"Zulu"));
	REQUIRE(ItemText(fixed, L"Free text") == E_INVALIDARG); Check(fixed->GetItemIndex(&index)); REQUIRE(index == 2);
	// Native programmatic selection updates logical state. A later notification
	// for that same selection does not deliver another logical event.
	selections = binder->selections; texts = binder->texts;
	Check(combo->GetHandle(&hwnd)); REQUIRE(SendMessageW(hwnd, CB_SETCURSEL, 3, 0) == 3);
	SendMessageW(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(0, CBN_SELCHANGE), reinterpret_cast<LPARAM>(hwnd));
	Check(combo->GetItemIndex(&index)); REQUIRE(index == 3 && binder->selections == selections + 1 && binder->texts == texts + 1);
	Check(list->GetHandle(&listWindow)); selections = binder->selections;
	REQUIRE(SendMessageW(listWindow, LB_SETCURSEL, 0, 0) == 0);
	SendMessageW(GetParent(listWindow), WM_COMMAND, MAKEWPARAM(0, LBN_SELCHANGE), reinterpret_cast<LPARAM>(listWindow));
	Check(list->GetItemIndex(&index)); REQUIRE(index == 0 && binder->selections == selections + 1);
	Check(form->ShowForm(SW_SHOW)); PumpPendingMessages(); Check(combo->SetFocus()); Check(list->SetFocus());
	REQUIRE(IsWindowVisible(hwnd) && IsWindowVisible(listWindow));
	selections = binder->selections; SendMessageW(listWindow, WM_KEYDOWN, VK_DOWN, 0);
	Check(list->GetItemIndex(&index)); REQUIRE(index == 1 && binder->selections == selections + 1);
	Check(fixed->GetHandle(&listWindow)); Check(fixed->SetFocus()); selections = binder->selections;
	SendMessageW(listWindow, WM_KEYDOWN, VK_DOWN, 0); Check(fixed->GetItemIndex(&index));
	REQUIRE(index == 3 && binder->selections == selections + 1);
	SendMessageW(listWindow, CB_SHOWDROPDOWN, TRUE, 0); SendMessageW(listWindow, WM_KEYDOWN, VK_UP, 0); SendMessageW(listWindow, WM_KEYDOWN, VK_ESCAPE, 0);
	Check(fixed->GetItemIndex(&index)); REQUIRE(SendMessageW(listWindow, CB_GETCURSEL,0,0) == index);
	Check(list->GetHandle(&listWindow));
	// The editable child emits native text notifications. Read Text independently
	// of selection notifications (Win32 may still expose the previous edit text there).
	COMBOBOXINFO info; ZeroMemory(&info, sizeof(info)); info.cbSize = sizeof(info); REQUIRE(GetComboBoxInfo(hwnd, &info));
	selections = binder->selections; texts = binder->texts;
	REQUIRE(SetWindowTextW(info.hwndItem, L"Typed"));
	SendMessageW(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(0, CBN_EDITCHANGE), reinterpret_cast<LPARAM>(hwnd));
	Check(ItemString(combo, L"Text", L"Typed")); Check(combo->GetItemIndex(&index)); REQUIRE(index == -1 && binder->selections == selections + 1 && binder->texts == texts + 1);
	REQUIRE(SendMessageW(hwnd,CB_GETCURSEL,0,0) == -1);
	SendMessageW(info.hwndItem,EM_SETSEL,2,4); binder->handleKeys = TRUE;
	SendMessageW(info.hwndItem,WM_KEYDOWN,VK_LEFT,0); SendMessageW(info.hwndItem,WM_KEYUP,VK_LEFT,0);
	REQUIRE(binder->keys == 2); binder->handleKeys = FALSE;
	{ DWORD start = 0, end = 0; SendMessageW(info.hwndItem,EM_GETSEL,reinterpret_cast<WPARAM>(&start),reinterpret_cast<LPARAM>(&end)); REQUIRE(start == 2 && end == 4); }
	texts = binder->texts; SendMessageW(info.hwndItem,WM_CHAR,L'X',0);
	Check(ItemString(combo,L"Text",L"TyXd")); REQUIRE(binder->texts == texts + 1);
	Check(ItemText(combo,L"Typed"));
	SendMessageW(info.hwndItem,WM_KEYDOWN,VK_DOWN,0); SendMessageW(info.hwndItem,WM_KEYUP,VK_DOWN,0);
	REQUIRE(binder->keys == 4); Check(combo->GetItemIndex(&index)); REQUIRE(SendMessageW(hwnd,CB_GETCURSEL,0,0) == index);
	Check(ItemText(combo,L"Typed"));
	// Raw native item mutations are rejected, rather than silently diverging.
	REQUIRE(SendMessageW(hwnd, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Bypass")) == CB_ERR);
	REQUIRE(SendMessageW(listWindow, LB_DELETESTRING, 0, 0) == LB_ERR);
	{ HWND window; Check(fixed->GetHandle(&window)); REQUIRE(SendMessageW(window,CB_SELECTSTRING,-1,reinterpret_cast<LPARAM>(L"Zu")) == 0);
		Check(fixed->GetItemIndex(&index)); REQUIRE(index == 0); }
	REQUIRE(SendMessageW(listWindow,LB_SELECTSTRING,-1,reinterpret_cast<LPARAM>(L"Zu")) == 0);
	Check(list->GetItemIndex(&index)); REQUIRE(index == 0);
	// Empty replacement invalidates an index, but preserves editable free text.
	snapshot.Release(); Check(ScCreateRStringArray(&snapshot)); Check(combo->SetItems(snapshot)); Check(ItemString(combo, L"Text", L"Typed"));
	Check(list->SetItems(snapshot)); Check(list->GetItemIndex(&index)); REQUIRE(index == -1); Check(ItemNative(list, FALSE, snapshot, -1));
	Check(fixed->SetItems(snapshot)); Check(ItemString(fixed, L"Text", L"")); Check(fixed->GetItemIndex(&index)); REQUIRE(index == -1);
	Check(combo->SetItems(original));
	binder->reenter = TRUE; Check(combo->SetItemIndex(1)); REQUIRE(binder->reentered == QuadooForm::EInvalidState); binder->reenter = FALSE;
	binder->failSelection = TRUE; REQUIRE(combo->SetItemIndex(2) == E_ABORT); Check(combo->GetItemIndex(&index)); REQUIRE(index == 2); binder->failSelection = FALSE;
	binder->destroySelection = TRUE; Check(combo->GetHandle(&hwnd)); Check(combo->SetItemIndex(0)); REQUIRE(!IsWindow(hwnd));
	REQUIRE(combo->SetItems(original) == QuadooForm::EDestroyed); snapshot.Release(); Check(combo->GetItems(&snapshot)); REQUIRE(snapshot->Length() == 4);
	REQUIRE(combo->Destroy() == S_FALSE);
	binder->destroySelection = FALSE; Check(list->SetItems(original)); Check(list->SetItemIndex(0)); Check(list->GetHandle(&listWindow));
	binder->destroySelection = TRUE; SendMessageW(listWindow,WM_KEYDOWN,VK_DOWN,0); REQUIRE(!IsWindow(listWindow));
	Check(form->Destroy()); REQUIRE(binder->destroys == 3);
Cleanup:
	if(form) form->Destroy();
	return hr;
}

static HRESULT ItemPersistence()
{
	HRESULT hr;
	TStackRef<CItemBinder> binder; TStackRef<IQuadooConstructionContext> context;
	TStackRef<IQuadooForm> form; TStackRef<IQuadooComboBox> combo, fixed; TStackRef<IQuadooListBox> list;
	TStackRef<IQuadooFormSerializer> serializer; TStackRef<IJSONObject> first, second, definition;
	TStackRef<IQuadooControl> copy, invalid; TStackRef<IQuadooProperties> properties; TStackRef<IJSONValue> value, before;
	TStackRef<IRStringArray> items; UINT property, count; INT index; HWND hwnd, previous;
	TStackRef<CInputItems> input;
	binder.Attach(__new CItemBinder); CheckAlloc(binder); Check(ItemForm(binder, &context, &form, &combo, &fixed, &list));
	Check(ScCreateRStringArray(&items)); Check(items->AddStringW(L"One", 3)); Check(items->AddStringA("\xce\xa9", 2, CP_UTF8)); Check(combo->SetItems(items));
	input.Attach(__new CInputItems(items)); CheckAlloc(input); Check(combo->GetHandle(&previous));
	for(INT failure = 1; failure <= 3; failure++)
	{
		input->failure = failure; REQUIRE(FAILED(combo->SetItems(input))); Check(combo->GetHandle(&hwnd)); REQUIRE(hwnd == previous);
		Check(combo->GetItemIndex(&index)); REQUIRE(index == 1);
	}
	input->failure = 0; REQUIRE(combo->SetItems(input) == S_FALSE); REQUIRE(combo->SetItems(NULL) == E_INVALIDARG);
	items.Release(); Check(combo->GetItems(&items));
	Check(combo->GetItemIndex(&index)); REQUIRE(index == 1); Check(ItemString(combo, L"Text", L"\x03a9"));
	Check(Property(combo, L"Items", &properties, &property)); Check(properties->GetValueJSON(property, &before));
	Check(JSONCreateInteger(12, &value)); REQUIRE(FAILED(properties->SetValueJSON(property, value))); value.Release();
	Check(properties->GetValueJSON(property, &value)); REQUIRE(JSONCompare(before, value) == 0); value.Release();
	Check(Parse(L"{\"items\":[\"Okay\",null,3]}", &definition)); Check(definition->FindNonNullValueW(L"items", &value)); REQUIRE(FAILED(properties->SetValueJSON(property, value))); value.Release(); definition.Release();
	Check(Parse(L"{\"items\":[\"Nul\\u0000tail\"]}", &definition)); Check(definition->FindNonNullValueW(L"items", &value)); REQUIRE(properties->SetValueJSON(property, value) == E_INVALIDARG); value.Release(); definition.Release();
	items.Release(); Check(ScCreateRStringArray(&items)); Check(items->AddStringW(L"First",5)); Check(items->AddStringW(L"Second",6)); Check(list->SetItems(items));
	Check(Parse(L"{\"items\":[\"New\",\"Order\",\"\",\"Order\"]}", &definition)); Check(definition->FindNonNullValueW(L"items", &value)); Check(properties->SetValueJSON(property, value)); value.Release(); definition.Release(); properties.Release();
	Check(combo->GetHandle(&previous)); Check(SetInt(combo, L"Style", QuadooForm::DropDownList)); REQUIRE(!IsWindow(previous)); Check(combo->GetHandle(&hwnd));
	REQUIRE((GetWindowLongW(hwnd, GWL_STYLE) & 3) == CBS_DROPDOWNLIST); Check(ItemString(combo, L"Text", L"Order"));
	Check(combo->SetItemIndex(-1)); Check(ItemString(combo, L"Text", L""));
	Check(SetInt(combo, L"Style", QuadooForm::DropDown)); Check(ItemText(combo, L"Free saved text"));
	REQUIRE(SetInt(combo, L"Style", 2) == E_INVALIDARG);
	{
		TStackRef<IQuadooPropertyNotifications> notifications; TStackRef<CObserver> observer;
		Check(combo->QueryInterface(__uuidof(IQuadooPropertyNotifications),reinterpret_cast<PVOID*>(&notifications)));
		observer.Attach(__new CObserver); CheckAlloc(observer); Check(notifications->AddObserver(observer));
		Check(combo->SetItemIndex(0)); REQUIRE(observer->properties == 2); // ItemIndex and Text
		REQUIRE(combo->SetItemIndex(0) == S_FALSE && observer->properties == 2);
		Check(form->BeginUpdate()); Check(combo->SetItems(items)); Check(combo->SetItemIndex(1)); REQUIRE(observer->properties == 2);
		Check(form->EndUpdate()); REQUIRE(observer->properties == 5); // Items, ItemIndex, Text coalesce
		Check(notifications->RemoveObserver(observer)); Check(combo->SetItemIndex(-1)); Check(ItemText(combo,L"Free saved text"));
	}
	Check(form->ShowForm(SW_SHOW)); Check(combo->SetFocus()); Check(combo->GetHandle(&previous));
	Check(SetInt(combo,L"Style",QuadooForm::DropDownList)); Check(combo->GetHandle(&hwnd));
	REQUIRE(!IsWindow(previous) && IsWindowVisible(hwnd) && (GetFocus() == hwnd || IsChild(hwnd,GetFocus())));
	Check(SetInt(combo,L"Style",QuadooForm::DropDown)); Check(ItemText(combo,L"Free saved text"));
	{ RECT initial, enlarged; Check(combo->GetHandle(&hwnd));
		REQUIRE(SendMessageW(hwnd,CB_GETDROPPEDCONTROLRECT,0,reinterpret_cast<LPARAM>(&initial)));
		Check(SetInt(combo,L"DropDownHeight",240)); REQUIRE(SendMessageW(hwnd,CB_GETDROPPEDCONTROLRECT,0,reinterpret_cast<LPARAM>(&enlarged)));
		REQUIRE(enlarged.bottom - enlarged.top > initial.bottom - initial.top); Check(SetInt(combo,L"DropDownHeight",160)); }
	{
		HDC dc = CreateCompatibleDC(NULL); CheckIf(!dc,HrEnsureLastError());
		COMBOBOXINFO info; ZeroMemory(&info,sizeof(info)); info.cbSize = sizeof(info);
		hr = SetInt(combo,L"ForeColor",RGB(10,20,30)); if(SUCCEEDED(hr)) hr = SetInt(combo,L"BackColor",RGB(200,210,220));
		if(SUCCEEDED(hr)) hr = combo->GetHandle(&hwnd);
		if(SUCCEEDED(hr) && GetComboBoxInfo(hwnd,&info))
		{
			SendMessageW(hwnd,WM_CTLCOLORLISTBOX,reinterpret_cast<WPARAM>(dc),reinterpret_cast<LPARAM>(info.hwndList));
			if(!Expect(GetTextColor(dc) == RGB(10,20,30) && GetBkColor(dc) == RGB(200,210,220),L"ComboBox colors",__LINE__)) hr = E_FAIL;
		}
		else if(SUCCEEDED(hr)) hr = HrEnsureLastError();
		if(SUCCEEDED(hr)) hr = SetInt(list,L"ForeColor",RGB(20,30,40)); if(SUCCEEDED(hr)) hr = SetInt(list,L"BackColor",RGB(210,220,230));
		if(SUCCEEDED(hr)) hr = list->GetHandle(&hwnd);
		if(SUCCEEDED(hr))
		{
			SendMessageW(GetParent(hwnd),WM_CTLCOLORLISTBOX,reinterpret_cast<WPARAM>(dc),reinterpret_cast<LPARAM>(hwnd));
			if(!Expect(GetTextColor(dc) == RGB(20,30,40) && GetBkColor(dc) == RGB(210,220,230),L"ListBox colors",__LINE__)) hr = E_FAIL;
		}
		DeleteDC(dc); Check(hr);
	}
	// Physical sizes and docking survive replacement and explicit DPI changes.
	Check(SetInt(list, L"Align", QuadooForm::AlignBottom));
	for(UINT dpi = 96; dpi <= 192; dpi += 48)
	{
		TStackRef<IQuadooLayoutControl> layout; QuadooForm::Rect bounds = { 0,0,490,250 }, effective; RECT native;
		Check(form->QueryInterface(__uuidof(IQuadooLayoutControl), reinterpret_cast<PVOID*>(&layout))); Check(layout->Arrange(&bounds, dpi));
		Check(list->GetEffectiveBounds(&effective)); Check(items->Replace(0,dpi == 144 ? RSTRING_CAST(L"Changed") : RSTRING_CAST(L"First")));
		Check(list->SetItems(items)); Check(list->GetHandle(&hwnd)); REQUIRE(GetWindowRect(hwnd, &native));
		REQUIRE(native.right - native.left == MulDiv(effective.Width, dpi, 96) && native.bottom - native.top == MulDiv(effective.Height, dpi, 96));
		Check(combo->GetEffectiveBounds(&effective)); Check(combo->GetHandle(&hwnd)); REQUIRE(GetWindowRect(hwnd, &native));
		REQUIRE(native.right - native.left == MulDiv(effective.Width, dpi, 96) && native.bottom - native.top == MulDiv(effective.Height, dpi, 96));
	}
	Check(QuadooGetFormSerializer(&serializer)); Check(serializer->Serialize(form, &first));
	Check(QuadooCreateControl(context, NULL, first, &copy)); Check(serializer->Serialize(copy, &second)); REQUIRE(JSONCompareObjects(first, second) == 0);
	{ TStackRef<IQuadooControlContainer> container; TStackRef<IQuadooControl> child; TStackRef<IQuadooComboBox> saved;
		Check(copy->QueryInterface(__uuidof(IQuadooControlContainer), reinterpret_cast<PVOID*>(&container))); Check(container->FindControl(RSTRING_CAST(L"editable"), &child));
		Check(child->QueryInterface(__uuidof(IQuadooComboBox), reinterpret_cast<PVOID*>(&saved))); Check(ItemString(saved, L"Text", L"Free saved text"));
		Check(saved->GetItemIndex(&index)); REQUIRE(index == -1); items.Release(); Check(saved->GetItems(&items)); Check(ItemNative(saved, TRUE, items, -1)); }
	Check(copy->Destroy()); copy.Release();
	// Construction must reject bad arrays/indexes/styles and unwind registered siblings.
	static PCWSTR const bad[] = { L"\"Items\":null", L"\"Items\":{}", L"\"Items\":[1]", L"\"ItemIndex\":0", L"\"Items\":[\"A\"],\"ItemIndex\":1", L"\"ItemIndex\":-2", L"\"Style\":2", L"\"Style\":\"Simple\"", L"\"Style\":1,\"Text\":\"Invalid\"" };
	for(UINT n = 0; n < ARRAYSIZE(bad); n++)
	{
		CRString json; Check(RStrFormatW(&json,L"{\"type\":\"Form\",\"name\":\"BadItems\",\"children\":[{\"type\":\"ListBox\",\"name\":\"first\"},{\"type\":\"ComboBox\",\"name\":\"bad\",\"properties\":{%ls}}]}", bad[n]));
		Check(Parse(json.c_wstr(), &definition)); REQUIRE(FAILED(QuadooCreateControl(context, NULL, definition, &invalid)) && !invalid); definition.Release();
	}
	Check(form->GetChildCount(&count)); REQUIRE(count == 3);
Cleanup:
	if(copy) copy->Destroy(); if(form) form->Destroy();
	return hr;
}
static HRESULT ItemResources()
{
	HRESULT hr;
	TStackRef<IQuadooForm> form; TStackRef<IQuadooComboBox> combo, fixed; TStackRef<IQuadooListBox> list;
	TStackRef<IQuadooConstructionContext> context; TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooFactoryLifetime> comboLife, listLife; TStackRef<IQuadooControlFactory> comboFactory, listFactory;
	TStackRef<IRStringArray> items; TStackRef<CItemBinder> binder;
	CGuiResourceMonitor resources(L"Item control resources"); UINT live;
	binder.Attach(__new CItemBinder); CheckAlloc(binder);
	Check(QuadooGetControlRegistry(&registry)); Check(registry->FindFactory(RSTRING_CAST(L"ComboBox"), &comboFactory)); Check(registry->FindFactory(RSTRING_CAST(L"ListBox"), &listFactory));
	Check(comboFactory->QueryInterface(__uuidof(IQuadooFactoryLifetime), reinterpret_cast<PVOID*>(&comboLife))); Check(listFactory->QueryInterface(__uuidof(IQuadooFactoryLifetime), reinterpret_cast<PVOID*>(&listLife)));
	Check(ScCreateRStringArray(&items)); Check(items->AddStringW(L"Replacement", 11));
	for(UINT cycle = 0; !resources.Complete(); cycle++)
	{
		TArray<HWND> windows; HWND hwnd; COMBOBOXINFO info;
		Check(ItemForm(binder, &context, &form, &combo, &fixed, &list, registry)); Check(form->ShowForm(SW_SHOW));
		Check(combo->SetFocus()); PumpPendingMessages();
		Check(combo->GetHandle(&hwnd)); ZeroMemory(&info, sizeof(info)); info.cbSize = sizeof(info); REQUIRE(GetComboBoxInfo(hwnd, &info));
		// Internal edit/drop-list HWNDs are owned by the combo, not logical children.
		Check(combo->SetItems(items)); REQUIRE(!IsWindow(hwnd) && !IsWindow(info.hwndItem) && !IsWindow(info.hwndList));
		Check(SetInt(fixed, L"Style", QuadooForm::DropDown)); Check(list->SetItems(items));
		Check(combo->GetHandle(&hwnd)); info.cbSize = sizeof(info); REQUIRE(GetComboBoxInfo(hwnd, &info));
		Check(windows.Append(info.hwndItem)); Check(windows.Append(info.hwndList));
		SendMessageW(hwnd, CB_SHOWDROPDOWN, TRUE, 0); PumpPendingMessages(); SendMessageW(hwnd, CB_SHOWDROPDOWN, FALSE, 0);
		Check(list->SetFocus()); PumpPendingMessages(); Check(CollectNativeWindows(form, windows));
		Check(form->Destroy()); for(sysint n = 0; n < windows.Length(); n++) REQUIRE(!IsWindow(windows[n]));
		REQUIRE(registry->UnregisterFactory(RSTRING_CAST(L"ComboBox")) == HRESULT_FROM_WIN32(ERROR_BUSY));
		Check(comboLife->GetLiveCount(&live)); REQUIRE(live == 2); Check(listLife->GetLiveCount(&live)); REQUIRE(live == 1);
		combo.Release(); fixed.Release(); list.Release(); form.Release(); context.Release();
		Check(comboLife->GetLiveCount(&live)); REQUIRE(live == 0); Check(listLife->GetLiveCount(&live)); REQUIRE(live == 0);
		Check(resources.AfterCycle());
	}
Cleanup:
	if(form) form->Destroy();
	return hr;
}
