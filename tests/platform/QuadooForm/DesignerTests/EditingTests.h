// Native inspector and dialog tests; no private runtime classes are used.
static INT InspectorRow(QFD::CDesignerEditor* editor, PCWSTR name)
{
	HWND list = GetDlgItem(editor->Window(),104);
	for(INT row = 0; row < ListView_GetItemCount(list); row++)
	{
		WCHAR text[128]; ListView_GetItemText(list,row,0,text,ARRAYSIZE(text));
		if(!lstrcmpW(text,name)) return row;
	}
	return -1;
}
static VOID InspectorEvents(QFD::CDesignerEditor* editor, BOOL events)
{
	HWND tabs = GetDlgItem(editor->Window(),103);
	TabCtrl_SetCurSel(tabs,events ? 1 : 0);
	NMHDR notification = { tabs,103,TCN_SELCHANGE };
	SendMessageW(editor->Window(),WM_NOTIFY,103,reinterpret_cast<LPARAM>(&notification));
}
static HWND StartInline(QFD::CDesignerEditor* editor, PCWSTR name)
{
	HWND list = GetDlgItem(editor->Window(),104);
	NMITEMACTIVATE notification; ZeroMemory(&notification,sizeof(notification));
	notification.hdr.hwndFrom = list; notification.hdr.idFrom = 104; notification.hdr.code = NM_DBLCLK;
	notification.iItem = InspectorRow(editor,name); notification.iSubItem = 1;
	SendMessageW(editor->Window(),WM_NOTIFY,104,reinterpret_cast<LPARAM>(&notification));
	HWND field = GetDlgItem(list,120);
	return field && IsWindowVisible(field) ? field : NULL;
}
struct DialogExercise
{
	HWND owner;
	BOOL order, accept, seen, correct;
	UINT ticks, count;
	PCWSTR text, caption;
};
static DialogExercise g_dialog;
static BOOL CALLBACK ExerciseDialog(HWND window, LPARAM)
{
	WCHAR type[32]; GetClassNameW(window,type,ARRAYSIZE(type));
	if(lstrcmpW(type,L"#32770") || GetWindow(window,GW_OWNER) != g_dialog.owner) return TRUE;
	g_dialog.seen = TRUE;
	if(g_dialog.order)
	{
		HWND list = GetDlgItem(window,100); WCHAR name[256];
		g_dialog.correct = list && SendMessageW(list,LB_GETCOUNT,0,0) == g_dialog.count && !IsWindowEnabled(GetDlgItem(window,10));
		if(g_dialog.count > 1)
		{
			SendMessageW(list,LB_GETTEXT,0,reinterpret_cast<LPARAM>(name));
			g_dialog.correct = g_dialog.correct && IsWindowEnabled(GetDlgItem(window,11)) && !lstrcmpW(name,g_dialog.text);
			SendMessageW(window,WM_COMMAND,11,0);
			SendMessageW(list,LB_GETTEXT,1,reinterpret_cast<LPARAM>(name));
			g_dialog.correct = g_dialog.correct && SendMessageW(list,LB_GETCURSEL,0,0) == 1 && IsWindowEnabled(GetDlgItem(window,10)) && !lstrcmpW(name,g_dialog.text);
			SendMessageW(window,WM_COMMAND,10,0);
			SendMessageW(window,WM_COMMAND,11,0);
		}
	}
	else
	{
		WCHAR title[128]; GetWindowTextW(window,title,ARRAYSIZE(title));
		HWND edit = FindWindowExW(window,NULL,L"EDIT",NULL);
		g_dialog.correct = edit && (GetWindowLongW(edit,GWL_STYLE) & ES_MULTILINE) && !lstrcmpW(title,g_dialog.caption);
		if(!g_dialog.correct) { CRString message; if(SUCCEEDED(RStrFormatW(&message,L"Dialog mismatch: title=%ls expected=%ls edit=%p style=%x\r\n",title,g_dialog.caption,edit,GetWindowLongW(edit,GWL_STYLE)))) Log(message.c_wstr()); }
		if(edit) SetWindowTextW(edit,g_dialog.text);
	}
	// A 10 ms test timer can close a new dialog before its first WM_PAINT.
	// Render its controls while alive so font/theme caches warm deterministically.
	RedrawWindow(window,NULL,NULL,RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);
	SendMessageW(window,WM_COMMAND,g_dialog.accept ? IDOK : IDCANCEL,0);
	return FALSE;
}
static VOID CALLBACK DialogTimer(HWND window, UINT, UINT_PTR id, DWORD)
{
	if(++g_dialog.ticks > 100) { KillTimer(window,id); PostQuitMessage(1); return; }
	EnumThreadWindows(GetCurrentThreadId(),ExerciseDialog,0);
}
static HRESULT DrivePropertyDialog(QFD::CDesignerEditor* editor, HWND host, PCWSTR name, PCWSTR text, BOOL accept, BOOL forceDialog)
{
	HRESULT hr = S_OK;
	HWND list = GetDlgItem(editor->Window(),104); INT row = InspectorRow(editor,name);
	g_dialog.owner = GetAncestor(editor->Window(),GA_ROOT); g_dialog.order = FALSE; g_dialog.accept = accept;
	g_dialog.seen = g_dialog.correct = FALSE; g_dialog.ticks = 0; g_dialog.caption = name; g_dialog.text = text;
	CheckIf(row < 0,E_INVALIDARG);
	CheckIf(!SetTimer(host,123,10,DialogTimer),HrEnsureLastError());
	if(forceDialog)
	{
		ListView_SetItemState(list,row,LVIS_SELECTED | LVIS_FOCUSED,LVIS_SELECTED | LVIS_FOCUSED);
		NMLVKEYDOWN notification; ZeroMemory(&notification,sizeof(notification));
		notification.hdr.hwndFrom = list; notification.hdr.idFrom = 104; notification.hdr.code = LVN_KEYDOWN; notification.wVKey = VK_F4;
		SendMessageW(editor->Window(),WM_NOTIFY,104,reinterpret_cast<LPARAM>(&notification));
	}
	else StartInline(editor,name);
	REQUIRE(g_dialog.seen && g_dialog.correct);
Cleanup:
	KillTimer(host,123); g_dialog.owner = NULL;
	return hr;
}
static HRESULT InlineEditing()
{
	HRESULT hr;
	HWND host = Host(), field = NULL; TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IJSONObject> definition, before, after; TStackRef<IQuadooControlContainer> root;
	TStackRef<IQuadooControl> button, entry; CRString text; GUID id, entryID, selected;
	CheckIf(!host,HrEnsureLastError());
	Check(CreateEditor(host,&editor));
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"InlineForm\",\"children\":[{\"type\":\"Button\",\"name\":\"button\",\"properties\":{\"Caption\":\"Initial\"}},{\"type\":\"TextBox\",\"name\":\"entry\"}]}",&definition));
	Check(editor->LoadDefinition(definition));
	Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root)));
	Check(root->FindControl(RSTRING_CAST(L"button"),&button));
	Check(root->FindControl(RSTRING_CAST(L"entry"),&entry));
	Check(button->GetID(&id));
	Check(entry->GetID(&entryID));
	Check(editor->SelectID(id));
	field = StartInline(editor,L"Caption");
	REQUIRE(field && !(GetWindowLongW(field,GWL_STYLE) & ES_MULTILINE));
	SetWindowTextW(field,L"R\x00e9sum\x00e9 \x65e5\x672c\x8a9e"); SendMessageW(field,WM_KEYDOWN,VK_RETURN,0);
	Check(QFD::StringProperty(button,L"Caption",&text));
	REQUIRE(text == RSTRING_CAST(L"R\x00e9sum\x00e9 \x65e5\x672c\x8a9e") && !IsWindowVisible(field) && editor->Document().Dirty());
	Check(editor->Document().Serialize(&before));
	editor->Document().SetDirty(FALSE);
	field = StartInline(editor,L"Name");
	REQUIRE(field != NULL);
	SetWindowTextW(field,L"entry");
	REQUIRE(editor->CommitPropertyEdit() == QuadooForm::EDuplicateName && IsWindowVisible(field));
	Check(editor->Document().Serialize(&after));
	REQUIRE(JSONCompareObjects(before,after) == 0 && !editor->Document().Dirty()); after.Release();
	REQUIRE(editor->SelectID(entryID) == QuadooForm::EDuplicateName);
	Check(editor->GetSelectedID(&selected));
	REQUIRE(IsEqualGUID(selected,id));
	InspectorEvents(editor,TRUE);
	REQUIRE(InspectorRow(editor,L"Name") >= 0 && IsWindowVisible(field));
	SendMessageW(field,WM_KEYDOWN,VK_ESCAPE,0);
	REQUIRE(!IsWindowVisible(field) && !editor->Document().Dirty());
	field = StartInline(editor,L"Name"); SetWindowTextW(field,L"renamed"); SendMessageW(field,WM_KEYDOWN,VK_RETURN,0);
	text = static_cast<RSTRING>(NULL);
	Check(button->GetName(&text));
	REQUIRE(text == RSTRING_CAST(L"renamed") && FindTree(editor->TreeWindow(),TreeView_GetRoot(editor->TreeWindow()),L"renamed (") != NULL);
	field = StartInline(editor,L"Caption"); SetWindowTextW(field,L"draft");
	CheckIf(!MoveWindow(editor->Window(),0,0,950,620,TRUE),HrEnsureLastError());
	{ RECT fieldRect, client; GetWindowRect(field,&fieldRect); MapWindowPoints(NULL,GetParent(field),reinterpret_cast<POINT*>(&fieldRect),2); GetClientRect(GetParent(field),&client); REQUIRE(fieldRect.right <= client.right && fieldRect.left >= 0); }
	SendMessageW(field,WM_KEYDOWN,VK_ESCAPE,0);
	CheckIf(!MoveWindow(editor->Window(),0,0,1230,720,TRUE),HrEnsureLastError());
	field = StartInline(editor,L"Caption"); SetWindowTextW(field,L"focus commit");
	Check(editor->SelectID(entryID));
	text = static_cast<RSTRING>(NULL);
	Check(QFD::StringProperty(button,L"Caption",&text));
	REQUIRE(text == RSTRING_CAST(L"focus commit") && !IsWindowVisible(field));
	field = StartInline(editor,L"Text"); SetWindowTextW(field,L"abc"); SendMessageW(field,EM_SETSEL,0,1);
	{
		MSG message;
		ZeroMemory(&message,sizeof(message));
		message.hwnd = field;
		message.message = WM_KEYDOWN;
		message.wParam = VK_DELETE;
		REQUIRE(editor->TranslateEditorMessage(&message));
	}
	SendMessageW(field,WM_KEYDOWN,VK_RETURN,0);
	text = static_cast<RSTRING>(NULL);
	Check(QFD::StringProperty(entry,L"Text",&text));
	REQUIRE(text == RSTRING_CAST(L"bc") && TreeView_GetCount(editor->TreeWindow()) == 3);
	field = StartInline(editor,L"Name"); SetWindowTextW(field,L"textEntry"); SendMessageW(field,WM_KEYDOWN,VK_TAB,0);
	// Tab advances through numeric inline fields as well as strings/dropdowns.
	field = GetDlgItem(GetDlgItem(editor->Window(),104),120);
	REQUIRE(field && IsWindowVisible(field) && !(GetWindowLongW(field,GWL_STYLE) & ES_MULTILINE));
	SetWindowTextW(field,L"35");
	SendMessageW(field,WM_KEYDOWN,VK_RETURN,0);
	REQUIRE(QFD::IntProperty(entry,L"Left") == 35);
	SendMessageW(field,WM_KEYDOWN,VK_ESCAPE,0);
	Check(editor->SelectID(id));
	InspectorEvents(editor,TRUE);
	field = StartInline(editor,L"OnClick");
	REQUIRE(field != NULL);
	SetWindowTextW(field,L"Button_Click"); SendMessageW(field,WM_KEYDOWN,VK_RETURN,0);
	text = static_cast<RSTRING>(NULL);
	Check(QFD::StringProperty(button,L"OnClick",&text));
	REQUIRE(text == RSTRING_CAST(L"Button_Click"));
	field = StartInline(editor,L"OnClick"); SetWindowTextW(field,L""); InspectorEvents(editor,FALSE);
	text = static_cast<RSTRING>(NULL);
	Check(QFD::StringProperty(button,L"OnClick",&text));
	REQUIRE(RStrLen(text) == 0 && InspectorRow(editor,L"Caption") >= 0);
	Check(DrivePropertyDialog(editor,host,L"Caption",L"First line\r\nSecond line \x03a9",TRUE,TRUE));
	text = static_cast<RSTRING>(NULL);
	Check(QFD::StringProperty(button,L"Caption",&text));
	REQUIRE(text == RSTRING_CAST(L"First line\r\nSecond line \x03a9"));
	Check(DrivePropertyDialog(editor,host,L"Caption",L"Canceled",FALSE,FALSE));
	text = static_cast<RSTRING>(NULL);
	Check(QFD::StringProperty(button,L"Caption",&text));
	REQUIRE(text == RSTRING_CAST(L"First line\r\nSecond line \x03a9"));
	// A public observer refresh deferred during a draft must still run on Cancel.
	field = StartInline(editor,L"Name"); SetWindowTextW(field,L"CanceledDraft");
	{
		TStackRef<IQuadooProperties> properties; UINT index;
		Check(QFD::Property(button,L"Caption",&properties,&index));
		Check(properties->SetValueString(index,RSTRING_CAST(L"External update")));
		CheckIf(PumpPendingMessages(),E_ABORT);
		REQUIRE(IsWindowVisible(field));
	}
	SendMessageW(field,WM_KEYDOWN,VK_ESCAPE,0);
	{ WCHAR cell[128]; ListView_GetItemText(GetDlgItem(editor->Window(),104),InspectorRow(editor,L"Caption"),1,cell,ARRAYSIZE(cell)); REQUIRE(!lstrcmpW(cell,L"External update")); }
Cleanup:
	if(host) DestroyWindow(host); editor.Release();
	return hr;
}
class CRejectTabOrder : public CBaseUnknown, public IQuadooPropertyObserver
{
public:
	BOOL armed;
	CRejectTabOrder() : armed(TRUE) {}
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooPropertyObserver)
	END_UNK_MAP
	STDMETHOD(OnPropertyChanged)(IQuadooComponent*, UINT) { if(armed) { armed = FALSE; return E_ABORT; } return S_OK; }
	STDMETHOD(OnBoundsChanged)(IQuadooControl*, const QuadooForm::Rect*) { return S_OK; }
};
static HRESULT AddOrdered(QFD::CDesignerEditor* editor, IQuadooControlContainer* parent, PCWSTR type, IQuadooControl** control)
{
	HRESULT hr; GUID id; QuadooForm::Rect bounds = { 10,10,120,32 };
	Check(editor->AddControl(RSTRING_CAST(type),parent,&bounds,&id));
	Check(editor->Document().Find(id,control));
Cleanup:
	return hr;
}
static HRESULT DriveTabOrder(QFD::CDesignerEditor* editor, HWND host, PCWSTR first, UINT count, BOOL accept)
{
	HRESULT hr;
	g_dialog.owner = GetAncestor(editor->Window(),GA_ROOT); g_dialog.order = TRUE; g_dialog.accept = accept;
	g_dialog.seen = g_dialog.correct = FALSE; g_dialog.ticks = 0; g_dialog.text = first; g_dialog.count = count;
	CheckIf(!SetTimer(host,123,10,DialogTimer),HrEnsureLastError());
	hr = editor->EditTabOrder();
	Check(hr);
	REQUIRE(g_dialog.seen && g_dialog.correct && (accept || hr == S_FALSE));
Cleanup:
	KillTimer(host,123); g_dialog.owner = NULL;
	return hr;
}
static HRESULT TabOrderEditing()
{
	HRESULT hr;
	HWND host = Host(); TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IQuadooControlContainer> root, panel, group, scope;
	TStackRef<IQuadooControl> button, label, panelControl, groupControl, edit, nested, groupChild, extra;
	TStackRef<IJSONObject> before, after; TStackRef<IJSONValue> value;
	TStackRef<IQuadooPropertyNotifications> notifications; TStackRef<CRejectTabOrder> reject;
	TArray<GUID> order, children; GUID buttonID, editID, panelID, groupID, childID, nestedID, found; UINT count; CRString name;
	CheckIf(!host,HrEnsureLastError());
	Check(CreateEditor(host,&editor));
	Check(editor->NewDocument(TRUE));
	Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root)));
	Check(AddOrdered(editor,root,L"Button",&button));
	Check(AddOrdered(editor,root,L"Label",&label));
	Check(AddOrdered(editor,root,L"Panel",&panelControl));
	Check(panelControl->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&panel)));
	Check(AddOrdered(editor,root,L"TextBox",&edit));
	Check(AddOrdered(editor,root,L"GroupBox",&groupControl));
	Check(groupControl->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&group)));
	REQUIRE(QFD::IntProperty(button,L"TabOrder") == 0 && QFD::IntProperty(panel,L"TabOrder") == 1 && QFD::IntProperty(edit,L"TabOrder") == 2 && QFD::IntProperty(group,L"TabOrder") == 3);
	Check(AddOrdered(editor,panel,L"TextBox",&nested));
	Check(AddOrdered(editor,group,L"Button",&groupChild));
	REQUIRE(QFD::IntProperty(nested,L"TabOrder") == 0 && QFD::IntProperty(groupChild,L"TabOrder") == 0);
	Check(AddOrdered(editor,panel,L"Button",&extra));
	REQUIRE(QFD::IntProperty(extra,L"TabOrder") == 1); extra.Release();
	Check(button->GetID(&buttonID));
	Check(edit->GetID(&editID));
	Check(panel->GetID(&panelID));
	Check(group->GetID(&groupID));
	Check(nested->GetID(&nestedID));
	Check(groupChild->GetID(&childID));
	Check(editor->SelectID(nestedID));
	Check(editor->GetTabOrderContainer(&scope));
	REQUIRE(QFD::SameObject(scope,panel)); scope.Release();
	Check(editor->SelectID(groupID));
	Check(editor->GetTabOrderContainer(&scope));
	REQUIRE(QFD::SameObject(scope,group)); scope.Release();
	Check(editor->SelectID(buttonID));
	Check(editor->GetTabOrderContainer(&scope));
	REQUIRE(QFD::SameObject(scope,root)); scope.Release();
	Check(editor->Document().GetTabOrder(root,order));
	REQUIRE(order.Length() == 4 && IsEqualGUID(order[0],buttonID) && IsEqualGUID(order[1],panelID) && IsEqualGUID(order[2],editID) && IsEqualGUID(order[3],groupID));
	REQUIRE(editor->Document().SetTabOrder(root,&order[0],4) == S_FALSE);
	Check(root->GetChildCount(&count));
	for(UINT n = 0; n < count; n++)
	{
		TStackRef<IQuadooControl> child; GUID id;
		Check(root->GetChild(n,&child));
		Check(child->GetID(&id));
		Check(children.Append(id));
	}
	Check(editor->Document().Serialize(&before));
	editor->Document().SetDirty(FALSE);
	REQUIRE(editor->Document().SetTabOrder(root,&order[0],3) == E_INVALIDARG);
	order[1] = order[0];
	REQUIRE(editor->Document().SetTabOrder(root,&order[0],4) == E_INVALIDARG); order[1] = nestedID;
	REQUIRE(editor->Document().SetTabOrder(root,&order[0],4) == E_INVALIDARG); order[1] = panelID;
	Check(editor->Document().Serialize(&after));
	REQUIRE(JSONCompareObjects(before,after) == 0 && !editor->Document().Dirty()); after.Release();
	Check(order.Swap(0,3));
	reject.Attach(__new CRejectTabOrder);
	CheckAlloc(reject);
	Check(group->QueryInterface(__uuidof(IQuadooPropertyNotifications),reinterpret_cast<PVOID*>(&notifications)));
	Check(notifications->AddObserver(reject));
	REQUIRE(editor->Document().SetTabOrder(root,&order[0],4) == E_ABORT);
	Check(notifications->RemoveObserver(reject));
	Check(editor->Document().Serialize(&after));
	REQUIRE(JSONCompareObjects(before,after) == 0 && !editor->Document().Dirty()); after.Release(); before.Release();
	Check(editor->Document().SetTabOrder(root,&order[0],4));
	REQUIRE(QFD::IntProperty(group,L"TabOrder") == 0 && QFD::IntProperty(button,L"TabOrder") == 3 && QFD::IntProperty(nested,L"TabOrder") == 0 && editor->Document().Dirty());
	for(UINT n = 0; n < count; n++)
	{
		TStackRef<IQuadooControl> child;
		Check(root->GetChild(n,&child));
		Check(child->GetID(&found));
		REQUIRE(IsEqualGUID(found,children[n]));
	}
	Check(editor->Document().Serialize(&before));
	editor->Document().SetDirty(FALSE);
	Check(group->GetName(&name));
	Check(DriveTabOrder(editor,host,name.c_wstr(),4,FALSE));
	Check(editor->Document().Serialize(&after));
	REQUIRE(JSONCompareObjects(before,after) == 0 && !editor->Document().Dirty()); after.Release();
	Check(DriveTabOrder(editor,host,name.c_wstr(),4,TRUE));
	REQUIRE(QFD::IntProperty(panel,L"TabOrder") == 0 && QFD::IntProperty(group,L"TabOrder") == 1 && editor->Document().Dirty());
	Check(editor->SelectID(nestedID));
	{
		CRString nestedName;
		Check(nested->GetName(&nestedName));
		Check(DriveTabOrder(editor,host,nestedName.c_wstr(),2,TRUE));
		REQUIRE(QFD::IntProperty(nested,L"TabOrder") == 1 && QFD::IntProperty(groupChild,L"TabOrder") == 0);
	}
	Check(JSONCreateInteger(20,&value));
	Check(Edit(editor,button,L"TabOrder",value));
	value.Release();
	Check(AddOrdered(editor,root,L"CheckBox",&extra));
	REQUIRE(QFD::IntProperty(extra,L"TabOrder") == 21); extra.Release();
	Check(JSONCreateInteger(1000000,&value));
	Check(Edit(editor,button,L"TabOrder",value));
	value.Release();
	before.Release();
	Check(editor->Document().Serialize(&before));
	editor->Document().SetDirty(FALSE);
	REQUIRE(AddOrdered(editor,root,L"Button",&extra) == E_INVALIDARG);
	Check(editor->Document().Serialize(&after));
	REQUIRE(JSONCompareObjects(before,after) == 0 && !editor->Document().Dirty()); after.Release();
	Check(editor->LoadDefinition(before));
	button.Release();
	Check(editor->Document().Find(buttonID,&button));
	REQUIRE(QFD::IntProperty(button,L"TabOrder") == 1000000);
	Check(editor->Document().Find(nestedID,&extra));
	REQUIRE(QFD::IntProperty(extra,L"TabOrder") == 1); extra.Release();
	Check(editor->NewDocument(FALSE));
	Check(DriveTabOrder(editor,host,L"",0,TRUE));
	REQUIRE(!editor->Document().Dirty());
	REQUIRE(FAILED(editor->Document().GetTabOrder(root,order))); // An old document cannot be edited.
Cleanup:
	if(host) DestroyWindow(host); editor.Release();
	return hr;
}
