// Exercise the same shared editor used by the standalone designer and Studio.
static HRESULT SelectionClick(QFD::CDesignerEditor* editor, REFGUID id, BOOL shift, INT dx = 0, INT dy = 0, BOOL cancel = FALSE)
{
	HRESULT hr;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooWinControl> native;
	HWND window;
	RECT bounds;
	POINT point;
	BYTE saved[256];
	BYTE keyboard[256] = {0};
	BOOL restore = FALSE;
	Check(editor->Document().Find(id,&control));
	Check(control->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native)));
	Check(native->GetHandle(&window));
	CheckIf(!GetWindowRect(window,&bounds),HrEnsureLastError());
	MapWindowPoints(NULL,editor->OverlayWindow(),reinterpret_cast<POINT*>(&bounds),2);
	point.x = (bounds.left + bounds.right) / 2;
	point.y = (bounds.top + bounds.bottom) / 2;
	CheckIf(!GetKeyboardState(saved),HrEnsureLastError());
	keyboard[VK_SHIFT] = shift ? 0x80 : 0;
	CheckIf(!SetKeyboardState(keyboard),HrEnsureLastError());
	restore = TRUE;
	SendMessageW(editor->OverlayWindow(),WM_LBUTTONDOWN,MK_LBUTTON | (shift ? MK_SHIFT : 0),MAKELPARAM(point.x,point.y));
	SendMessageW(editor->OverlayWindow(),WM_MOUSEMOVE,MK_LBUTTON | (shift ? MK_SHIFT : 0),MAKELPARAM(point.x + dx,point.y + dy));
	if(cancel)
	{
		SendMessageW(editor->OverlayWindow(),WM_KEYDOWN,VK_ESCAPE,0);
	}
	SendMessageW(editor->OverlayWindow(),WM_LBUTTONUP,shift ? MK_SHIFT : 0,MAKELPARAM(point.x + dx,point.y + dy));
	hr = S_OK;
Cleanup:
	if(restore)
	{
		SetKeyboardState(saved);
	}
	return hr;
}

static HRESULT SelectionBounds(QFD::CDesignerEditor* editor, REFGUID id, INT left, INT top)
{
	HRESULT hr;
	TStackRef<IQuadooControl> control;
	QuadooForm::Rect bounds;
	Check(editor->Document().Find(id,&control));
	Check(control->GetBounds(&bounds));
	REQUIRE(bounds.Left == left && bounds.Top == top);
Cleanup:
	return hr;
}

static HRESULT SelectionText(QFD::CDesignerEditor* editor, REFGUID id, PCWSTR property, PCWSTR expected)
{
	HRESULT hr;
	TStackRef<IQuadooComponent> component;
	CRString text;
	Check(editor->Document().FindComponent(id,&component));
	Check(QFD::StringProperty(component,property,&text));
	REQUIRE(text == RSTRING_CAST(expected));
Cleanup:
	return hr;
}

static HRESULT SelectionPropertyIndex(QFD::CDesignerEditor* editor, PCWSTR name, UINT* index)
{
	HRESULT hr;
	TStackRef<IQuadooComponent> component;
	TStackRef<IQuadooProperties> properties;
	GUID id;
	Check(editor->GetSelectedID(&id));
	Check(editor->Document().FindComponent(id,&component));
	Check(QFD::Property(component,name,&properties,index));
Cleanup:
	return hr;
}

static HRESULT DesignerMultiSelection()
{
	HRESULT hr;
	HWND host = Host();
	TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONObject> before;
	TStackRef<IJSONObject> after;
	TStackRef<IJSONValue> value;
	TStackRef<IQuadooControl> control;
	TStackRef<IJSONArray> copied;
	GUID a;
	GUID b;
	GUID entry;
	GUID panel;
	GUID nested;
	GUID root;
	GUID ids[2];
	UINT index;
	WCHAR text[128];
	CheckIf(!host,HrEnsureLastError());
	Check(CreateEditor(host,&editor));
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"MultiSelect\",\"properties\":{\"Width\":560,\"Height\":320},\"children\":["
		L"{\"type\":\"Button\",\"name\":\"a\",\"properties\":{\"Left\":16,\"Top\":24,\"Caption\":\"First\"}},"
		L"{\"type\":\"Button\",\"name\":\"b\",\"properties\":{\"Left\":160,\"Top\":80,\"Caption\":\"Second\",\"Enabled\":false}},"
		L"{\"type\":\"TextBox\",\"name\":\"entry\",\"properties\":{\"Left\":20,\"Top\":160}},"
		L"{\"type\":\"Panel\",\"name\":\"panel\",\"properties\":{\"Left\":310,\"Top\":20,\"Width\":200,\"Height\":220},\"children\":["
		L"{\"type\":\"Button\",\"name\":\"nested\",\"properties\":{\"Left\":20,\"Top\":40}}]}]}",&definition));
	Check(editor->LoadDefinition(definition));
	Check(editor->Document().Root()->GetID(&root));
	Check(NamedID(editor,L"a",&a));
	Check(NamedID(editor,L"b",&b));
	Check(NamedID(editor,L"entry",&entry));
	Check(NamedID(editor,L"panel",&panel));
	Check(NamedID(editor,L"nested",&nested));
	Check(SelectionClick(editor,a,FALSE));
	Check(SelectionClick(editor,b,TRUE));
	REQUIRE(editor->SelectionCount() == 2 && editor->IsSelected(a) && editor->IsSelected(b));
	Check(SelectionClick(editor,nested,TRUE));
	REQUIRE(editor->SelectionCount() == 2 && !editor->IsSelected(nested));
	REQUIRE(editor->SelectID(root,TRUE) == S_FALSE && editor->SelectionCount() == 2);
	Check(SelectionClick(editor,a,TRUE));
	REQUIRE(editor->SelectionCount() == 2);
	Check(SelectionClick(editor,b,FALSE));
	REQUIRE(editor->SelectionCount() == 1 && editor->IsSelected(b));
	Check(SelectionClick(editor,a,TRUE));
	Check(SelectionClick(editor,a,FALSE,13,7));
	REQUIRE(editor->SelectionCount() == 2);
	Check(SelectionBounds(editor,a,29,31));
	Check(SelectionBounds(editor,b,173,87));
	PumpPendingMessages();
	REQUIRE(editor->SelectionCount() == 2);
	for(UINT dpi = 96; dpi <= 192; dpi += 48)
	{
		editor->SetDPI(dpi);
		editor->SetZoom(150);
		Check(NudgeKey(editor,editor->OverlayWindow(),VK_RIGHT,TRUE,FALSE,FALSE,TRUE));
		Check(NudgeKey(editor,editor->OverlayWindow(),VK_DOWN,TRUE,FALSE,FALSE,TRUE));
	}
	Check(SelectionBounds(editor,a,32,34));
	Check(SelectionBounds(editor,b,176,90));
	editor->SetDPI(96);
	editor->SetZoom(100);
	// One snapped anchor supplies the delta; the distance between siblings stays exact.
	editor->SetSnap(TRUE);
	Check(SelectionClick(editor,a,FALSE,9,9));
	Check(SelectionBounds(editor,a,40,40));
	Check(SelectionBounds(editor,b,184,96));
	editor->SetSnap(FALSE);
	Check(SelectionClick(editor,a,FALSE,24,24,TRUE));
	REQUIRE(editor->SelectionCount() == 2);
	Check(SelectionBounds(editor,a,40,40));
	Check(SelectionBounds(editor,b,184,96));
	Check(editor->SelectID(a));
	Check(editor->SelectID(panel,TRUE));
	Check(SelectionClick(editor,a,FALSE,10,10));
	Check(SelectionBounds(editor,a,50,50));
	Check(SelectionBounds(editor,panel,320,30));
	Check(SelectionBounds(editor,nested,20,40));
	Check(editor->SelectID(nested));
	REQUIRE(editor->SelectID(panel,TRUE) == S_FALSE && editor->SelectionCount() == 1);
	Check(editor->SelectID(a));
	Check(editor->SelectID(b,TRUE));
	HWND list = GetDlgItem(editor->Window(),104);
	REQUIRE(InspectorRow(editor,L"Caption") >= 0);
	ListView_GetItemText(list,InspectorRow(editor,L"Caption"),1,text,ARRAYSIZE(text));
	REQUIRE(!lstrcmpW(text,L"(multiple values)"));
	Check(SelectionPropertyIndex(editor,L"Name",&index));
	Check(JSONCreateString(RSTRING_CAST(L"Duplicate"),&value));
	REQUIRE(editor->SetSelectedProperty(index,value) == E_ACCESSDENIED);
	value.Release();
	editor->Document().SetDirty(FALSE);
	HWND field = StartInline(editor,L"Caption");
	REQUIRE(field != NULL && GetWindowTextLengthW(field) == 0);
	Check(editor->CommitPropertyEdit());
	REQUIRE(!editor->Document().Dirty());
	field = StartInline(editor,L"Caption");
	REQUIRE(field != NULL);
	SetWindowTextW(field,L"Together");
	Check(editor->CommitPropertyEdit());
	Check(SelectionText(editor,a,L"Caption",L"Together"));
	Check(SelectionText(editor,b,L"Caption",L"Together"));
	REQUIRE(editor->SelectionCount() == 2);
	// A blank mixed numeric field leaves every selected value untouched until edited.
	Check(editor->SelectID(a));
	field = StartInline(editor,L"Width");
	REQUIRE(field != NULL);
	SetWindowTextW(field,L"110");
	Check(editor->CommitPropertyEdit());
	Check(editor->SelectID(b,TRUE));
	editor->Document().SetDirty(FALSE);
	field = StartInline(editor,L"Width");
	REQUIRE(field != NULL && GetWindowTextLengthW(field) == 0);
	Check(editor->CommitPropertyEdit());
	REQUIRE(!editor->Document().Dirty());
	field = StartInline(editor,L"Width");
	REQUIRE(field != NULL);
	SetWindowTextW(field,L"120");
	Check(editor->CommitPropertyEdit());
	for(UINT n = 0; n < 2; n++)
	{
		TStackRef<IQuadooControl> selected;
		Check(editor->Document().Find(n ? b : a,&selected));
		REQUIRE(QFD::IntProperty(selected,L"Width") == 120);
	}
	REQUIRE(editor->SelectionCount() == 2);
	field = StartInline(editor,L"Enabled");
	REQUIRE(field != NULL && SendMessageW(field,CB_GETCOUNT,0,0) == 3 && SendMessageW(field,CB_GETCURSEL,0,0) == 2);
	SendMessageW(field,CB_SETCURSEL,1,0);
	Check(editor->CommitPropertyEdit());
	REQUIRE(editor->IsCommandChecked(QFD::ToggleEnabled));
	Check(editor->ExecuteCommand(QFD::ToggleEnabled,host));
	REQUIRE(!editor->IsCommandChecked(QFD::ToggleEnabled));
	InspectorEvents(editor,TRUE);
	field = StartInline(editor,L"OnClick");
	REQUIRE(field != NULL);
	SetWindowTextW(field,L"SharedClick");
	Check(editor->CommitPropertyEdit());
	Check(SelectionText(editor,a,L"OnClick",L"SharedClick"));
	Check(SelectionText(editor,b,L"OnClick",L"SharedClick"));
	InspectorEvents(editor,FALSE);
	Check(editor->SelectID(entry,TRUE));
	REQUIRE(editor->SelectionCount() == 3 && InspectorRow(editor,L"Caption") < 0 && InspectorRow(editor,L"Text") < 0 && InspectorRow(editor,L"BackColor") >= 0);
	Check(editor->SelectID(a));
	Check(editor->SelectID(b,TRUE));
	Check(SelectionPropertyIndex(editor,L"Enabled",&index));
	Check(JSONCreateString(RSTRING_CAST(L"invalid bool"),&value));
	Check(editor->Document().Serialize(&before));
	editor->Document().SetDirty(FALSE);
	REQUIRE(FAILED(editor->SetSelectedProperty(index,value)));
	value.Release();
	Check(editor->Document().Serialize(&after));
	REQUIRE(!JSONCompareObjects(before,after) && !editor->Document().Dirty() && editor->SelectionCount() == 2);
	after.Release();
	{
		struct RejectGroup : QFD::ISourceSite
		{
			UINT calls;
			RejectGroup() : calls(0)
			{
			}
			virtual HRESULT PrepareDefinition(IJSONObject*, QFD::ISourceEdit** edit)
			{
				*edit = NULL;
				calls++;
				return E_ACCESSDENIED;
			}
			virtual HRESULT OpenHandler(REFGUID, RSTRING)
			{
				return E_NOTIMPL;
			}
		} reject;
		// Always detach the borrowed source site before any assertion can leave scope.
		editor->Document().SetSourceSite(&reject);
		HRESULT removed = editor->Delete();
		ids[0] = a;
		ids[1] = b;
		HRESULT moved = editor->Document().MoveControls(ids,2,1,0);
		editor->Document().SetSourceSite(NULL);
		REQUIRE(removed == E_ACCESSDENIED && moved == E_ACCESSDENIED && reject.calls == 2);
	}
	Check(editor->Document().Serialize(&after));
	REQUIRE(!JSONCompareObjects(before,after) && !editor->Document().Dirty());
	after.Release();
	before.Release();
	ids[0] = b;
	ids[1] = a;
	Check(editor->Document().CopyControls(ids,2,&value));
	Check(value->GetArray(&copied));
	REQUIRE(copied->Count() == 2);
	{
		TStackRef<IJSONObject> item;
		TStackRef<IJSONValue> name;
		CRString firstName;
		Check(copied->GetObject(0,&item));
		Check(item->FindNonNullValueW(L"name",&name));
		Check(name->GetString(&firstName));
		REQUIRE(firstName == RSTRING_CAST(L"a"));
	}
	{
		TArray<GUID> added;
		GUID first;
		Check(editor->Document().PasteDefinition(panel,value,&first,&added));
		REQUIRE(added.Length() == 2);
		Check(editor->SelectID(first));
		Check(editor->SelectID(added[1],TRUE));
		REQUIRE(editor->SelectionCount() == 2);
	}
	Check(editor->Delete());
	REQUIRE(editor->SelectionCount() == 1);
	PumpPendingMessages();
	REQUIRE(editor->SelectionCount() == 1);
	// Explicit page-property edits change both the saved initial page and the
	// current design view. Other bulk edits must preserve that view.
	definition.Release();
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"GroupTabs\",\"children\":["
		L"{\"type\":\"TabControl\",\"name\":\"leftTabs\",\"children\":[{\"type\":\"TabPage\",\"name\":\"leftOne\"},{\"type\":\"TabPage\",\"name\":\"leftTwo\"}]},"
		L"{\"type\":\"TabControl\",\"name\":\"rightTabs\",\"children\":[{\"type\":\"TabPage\",\"name\":\"rightOne\"},{\"type\":\"TabPage\",\"name\":\"rightTwo\"}]}]}",&definition));
	Check(editor->LoadDefinition(definition));
	Check(NamedID(editor,L"leftTabs",&ids[0]));
	Check(NamedID(editor,L"rightTabs",&ids[1]));
	Check(editor->SelectID(ids[0]));
	Check(editor->SelectID(ids[1],TRUE));
	Check(SelectionPropertyIndex(editor,L"ActivePageIndex",&index));
	value.Release();
	Check(JSONCreateInteger(1,&value));
	Check(editor->SetSelectedProperty(index,value));
	value.Release();
	for(UINT n = 0; n < ARRAYSIZE(ids); n++)
	{
		TStackRef<IQuadooControl> tab;
		Check(editor->Document().Find(ids[n],&tab));
		REQUIRE(QFD::IntProperty(tab,L"ActivePageIndex") == 1);
	}
	Check(editor->ExecuteCommand(QFD::ToggleEnabled,host));
	for(UINT n = 0; n < ARRAYSIZE(ids); n++)
	{
		TStackRef<IQuadooTabControl> tabs;
		INT active;
		control.Release();
		Check(editor->Document().Find(ids[n],&control));
		Check(control->QueryInterface(__uuidof(IQuadooTabControl),reinterpret_cast<PVOID*>(&tabs)));
		Check(tabs->GetActivePageIndex(&active));
		REQUIRE(active == 1);
	}
	Check(JSONCreateInteger(0,&value));
	Check(editor->SetSelectedProperty(index,value));
	value.Release();
	Check(NamedID(editor,L"leftTwo",&a));
	Check(NamedID(editor,L"rightTwo",&b));
	Check(editor->SelectID(a));
	Check(editor->SelectID(b));
	Check(editor->SelectID(ids[0]));
	Check(editor->SelectID(ids[1],TRUE));
	Check(JSONCreateInteger(1,&value));
	Check(editor->SetSelectedProperty(index,value));
	value.Release();
	definition.Release();
	Check(editor->Document().Serialize(&definition));
	Check(editor->LoadDefinition(definition));
	for(UINT n = 0; n < ARRAYSIZE(ids); n++)
	{
		control.Release();
		Check(editor->Document().Find(ids[n],&control));
		REQUIRE(QFD::IntProperty(control,L"ActivePageIndex") == 1);
	}
Cleanup:
	if(editor)
	{
		editor->Document().SetSourceSite(NULL);
	}
	if(host)
	{
		DestroyWindow(host);
	}
	return hr;
}

static HRESULT DesignerMultiClipboard()
{
	HRESULT hr;
	HWND host = Host();
	TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONValue> value;
	TStackRef<IJSONArray> items;
	TStackRef<IQuadooControl> control;
	CTestClipboard saved;
	GUID first;
	GUID second;
	GUID ids[2];
	CheckIf(!host,HrEnsureLastError());
	Check(CreateEditor(host,&editor));
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"ClipboardGroup\",\"children\":["
		L"{\"type\":\"Button\",\"name\":\"a\",\"events\":{\"OnClick\":\"SharedClick\"}},"
		L"{\"type\":\"Panel\",\"name\":\"b\",\"children\":[{\"type\":\"Button\",\"name\":\"nested\"}]}]}",&definition));
	Check(editor->LoadDefinition(definition));
	Check(NamedID(editor,L"a",&first));
	Check(NamedID(editor,L"b",&second));
	Check(editor->SelectID(first));
	Check(editor->SelectID(second,TRUE));
	Check(saved.Save(host));
	Check(editor->ExecuteCommand(QFD::Copy,host));
	Check(QFD::ReadDefinitionClipboard(host,&value));
	Check(value->GetArray(&items));
	REQUIRE(items->Count() == 2 && editor->SelectionCount() == 2);
	Check(LockedClipboardCut(editor,host));
	Check(editor->ExecuteCommand(QFD::Cut,host));
	REQUIRE(editor->Document().Find(first,&control) == HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
	REQUIRE(editor->Document().Find(second,&control) == HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
	Check(editor->ExecuteCommand(QFD::Paste,host));
	REQUIRE(editor->SelectionCount() == 2);
	Check(editor->GetSelectionID(0,&ids[0]));
	Check(editor->GetSelectionID(1,&ids[1]));
	REQUIRE(!IsEqualGUID(ids[0],first) && !IsEqualGUID(ids[1],second));
	Check(SelectionText(editor,ids[0],L"OnClick",L"SharedClick"));
	// The primary is a Panel. Pasting a sibling group must target their common
	// parent, rather than putting the group inside that selected Panel.
	Check(editor->ExecuteCommand(QFD::Copy,host));
	Check(editor->ExecuteCommand(QFD::Paste,host));
	REQUIRE(editor->SelectionCount() == 2);
	for(UINT n = 0; n < editor->SelectionCount(); n++)
	{
		TStackRef<IQuadooControlContainer> parent;
		GUID id;
		Check(editor->GetSelectionID(n,&id));
		control.Release();
		Check(editor->Document().Find(id,&control));
		Check(control->GetParent(&parent));
		REQUIRE(QFD::SameObject(parent,editor->Document().Root()));
	}
	Check(editor->Delete());
	REQUIRE(editor->SelectionCount() == 1);
Cleanup:
	if(host)
	{
		DestroyWindow(host);
	}
	return hr;
}

// Clipboard tests use a private window station, leaving the user's clipboard
// (including delayed/OLE shell formats) untouched. Run before creating any UI.
static HRESULT IsolatedSelectionClipboard()
{
	HRESULT hr;
	HWINSTA originalStation = GetProcessWindowStation();
	HDESK originalDesktop = GetThreadDesktop(GetCurrentThreadId());
	HWINSTA station = CreateWindowStationW(NULL,0,WINSTA_ALL_ACCESS,NULL);
	HDESK desktop = NULL;
	BOOL initialized = FALSE;
	BOOL switched = FALSE;
	INITCOMMONCONTROLSEX controls = {sizeof(controls),ICC_WIN95_CLASSES};
	CheckIf(!station,HrEnsureLastError());
	CheckIf(!SetProcessWindowStation(station),HrEnsureLastError());
	switched = TRUE;
	desktop = CreateDesktopW(L"SelectionTests",NULL,NULL,0,GENERIC_ALL,NULL);
	CheckIf(!desktop,HrEnsureLastError());
	CheckIf(!SetThreadDesktop(desktop),HrEnsureLastError());
	CheckIf(!InitCommonControlsEx(&controls),HrEnsureLastError());
	Check(CoInitialize(NULL));
	initialized = TRUE;
	Check(DesignerMultiClipboard());
Cleanup:
	if(initialized)
	{
		CoUninitialize();
	}
	if(switched)
	{
		SetThreadDesktop(originalDesktop);
		SetProcessWindowStation(originalStation);
	}
	if(desktop)
	{
		CloseDesktop(desktop);
	}
	if(station)
	{
		CloseWindowStation(station);
	}
	return hr;
}
