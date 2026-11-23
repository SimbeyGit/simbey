#include "ClipboardTests.h"

static PCWSTR CommandDocument =
L"{\"type\":\"Form\",\"name\":\"CommandForm\",\"children\":["
L"{\"type\":\"Panel\",\"name\":\"Panel1\",\"properties\":{\"Left\":8,\"Top\":8,\"Width\":280,\"Height\":150},\"children\":["
L"{\"type\":\"Button\",\"name\":\"Button1\",\"properties\":{\"Left\":12,\"Top\":12,\"Caption\":\"Copy \\u03a9\",\"TabOrder\":4},\"events\":{\"OnClick\":\"Button1_Click\"}},"
L"{\"type\":\"Button\",\"name\":\"Button2\",\"properties\":{\"Left\":30,\"Top\":55,\"TabOrder\":2}},"
L"{\"type\":\"Label\",\"name\":\"Label1\",\"properties\":{\"Left\":140,\"Top\":80}}]},"
L"{\"type\":\"TabControl\",\"name\":\"Tabs1\",\"properties\":{\"Left\":8,\"Top\":170,\"Width\":280,\"Height\":120,\"ActivePageIndex\":1},\"children\":["
L"{\"type\":\"TabPage\",\"name\":\"Page1\"},{\"type\":\"TabPage\",\"name\":\"Page2\"}]}],"
L"\"components\":[{\"type\":\"Timer\",\"name\":\"Timer1\",\"properties\":{\"Interval\":1234},\"events\":{\"OnTimer\":\"Timer1_Tick\"}}]}";

static HRESULT NamedID(QFD::CDesignerEditor* editor, PCWSTR name, GUID* id)
{
	HRESULT hr;
	TStackRef<IQuadooControlContainer> root;
	TStackRef<IQuadooControl> control;
	Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root)));
	Check(root->FindControl(RSTRING_CAST(name),&control));
	Check(control->GetID(id));
Cleanup:
	return hr;
}
static HRESULT ChildName(QFD::CDesignerEditor* editor, REFGUID parent, UINT index, PCWSTR expected)
{
	HRESULT hr;
	TStackRef<IQuadooControl> control, child;
	TStackRef<IQuadooControlContainer> container;
	CRString name;
	Check(editor->Document().Find(parent,&control));
	Check(control->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&container)));
	Check(container->GetChild(index,&child));
	Check(child->GetName(&name));
	REQUIRE(name == RSTRING_CAST(expected));
Cleanup:
	return hr;
}
struct ContextProbe { QFD::CDesignerEditor* editor; GUID id; BOOL seen, checked, container; };
static ContextProbe* g_contextProbe;
static LRESULT CALLBACK ProbeContext(INT code, WPARAM wp, LPARAM lp)
{
	if(code >= 0 && g_contextProbe)
	{
		CWPSTRUCT* call = reinterpret_cast<CWPSTRUCT*>(lp);
		if(call->message == WM_INITMENUPOPUP)
		{
			HMENU menu = reinterpret_cast<HMENU>(call->wParam);
			if(GetMenuState(menu,QFD::ToggleEnabled,MF_BYCOMMAND) != static_cast<UINT>(-1))
			{
				ContextProbe* probe = g_contextProbe;
				probe->seen = TRUE;
				probe->editor->GetSelectedID(&probe->id);
				probe->checked = (GetMenuState(menu,QFD::ToggleEnabled,MF_BYCOMMAND) & MF_CHECKED) != 0;
				probe->container = !(GetMenuState(menu,QFD::TabOrder,MF_BYCOMMAND) & MF_GRAYED);
				PostMessageW(call->hwnd,WM_CANCELMODE,0,0);
			}
		}
	}
	return CallNextHookEx(NULL,code,wp,lp);
}
static VOID CALLBACK CancelContext(HWND, UINT, UINT_PTR, DWORD) { if(g_contextProbe) EndMenu(); }
static HRESULT ContextAt(QFD::CDesignerEditor* editor, HWND source, POINT screen, REFGUID expected, BOOL checked, BOOL container)
{
	HRESULT hr = S_OK;
	ContextProbe probe; ZeroMemory(&probe,sizeof(probe)); probe.editor = editor;
	UINT_PTR timeout = 0;
	HHOOK hook = SetWindowsHookExW(WH_CALLWNDPROC,ProbeContext,NULL,GetCurrentThreadId());
	CheckIf(!hook,HrEnsureLastError());
	g_contextProbe = &probe;
	SetForegroundWindow(GetAncestor(source,GA_ROOT));
	timeout = SetTimer(NULL,0,1500,CancelContext);
	SendMessageW(source,WM_CONTEXTMENU,reinterpret_cast<WPARAM>(source),MAKELPARAM(screen.x,screen.y));
	REQUIRE(probe.seen && IsEqualGUID(probe.id,expected) && probe.checked == checked && probe.container == container);
Cleanup:
	if(timeout) KillTimer(NULL,timeout);
	g_contextProbe = NULL;
	if(hook) UnhookWindowsHookEx(hook);
	return hr;
}

struct ClipboardLock { HANDLE ready, release; BOOL opened; DWORD duration; };
static DWORD WINAPI HoldClipboard(PVOID data)
{
	ClipboardLock* lock = static_cast<ClipboardLock*>(data);
	lock->opened = QFD::OpenDesignerClipboard(NULL);
	SetEvent(lock->ready);
	WaitForSingleObject(lock->release,lock->duration ? lock->duration : 10000);
	if(lock->opened) CloseClipboard();
	return 0;
}
static HRESULT LockedClipboardCut(QFD::CDesignerEditor* editor, HWND host)
{
	HRESULT hr;
	ClipboardLock lock;
	HANDLE thread = NULL;
	GUID id;
	TStackRef<IQuadooComponent> component;
	TStackRef<IJSONObject> before, after;
	ZeroMemory(&lock,sizeof(lock));
	Check(editor->GetSelectedID(&id));
	Check(editor->Document().Serialize(&before));
	lock.ready = CreateEventW(NULL,TRUE,FALSE,NULL); lock.release = CreateEventW(NULL,TRUE,FALSE,NULL);
	CheckIf(!lock.ready || !lock.release,HrEnsureLastError());
	thread = CreateThread(NULL,0,HoldClipboard,&lock,0,NULL);
	CheckIf(!thread,HrEnsureLastError());
	REQUIRE(WaitForSingleObject(lock.ready,5000) == WAIT_OBJECT_0 && lock.opened);
	REQUIRE(FAILED(editor->ExecuteCommand(QFD::Cut,host)));
	Check(editor->Document().FindComponent(id,&component));
	Check(editor->Document().Serialize(&after));
	REQUIRE(JSONCompareObjects(before,after) == 0);
	SetEvent(lock.release); WaitForSingleObject(thread,10000); CloseHandle(thread); thread = NULL;
	ResetEvent(lock.ready); ResetEvent(lock.release); lock.duration = 30; lock.opened = FALSE;
	thread = CreateThread(NULL,0,HoldClipboard,&lock,0,NULL);
	CheckIf(!thread,HrEnsureLastError());
	REQUIRE(WaitForSingleObject(lock.ready,5000) == WAIT_OBJECT_0 && lock.opened);
	// A short ownership handoff succeeds without asking the user to retry.
	Check(editor->ExecuteCommand(QFD::Copy,host));
Cleanup:
	if(lock.release) SetEvent(lock.release);
	if(thread) { WaitForSingleObject(thread,10000); CloseHandle(thread); }
	if(lock.ready) CloseHandle(lock.ready);
	if(lock.release) CloseHandle(lock.release);
	return hr;
}

static HRESULT ExtensionClipboard(HWND host)
{
	HRESULT hr;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooControlFactory> factory;
	TStackRef<IQuadooFactoryLifetime> lifetime;
	TStackRef<IExternalDesignPolicy> policy;
	TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IJSONObject> definition, before, after;
	TStackRef<IQuadooControl> control;
	GUID id, added;
	UINT count;
	BOOL registered = FALSE;
	Check(QuadooGetControlRegistry(&registry));
	Check(CreateExternalFactory(&factory));
	Check(factory->QueryInterface(__uuidof(IQuadooFactoryLifetime),reinterpret_cast<PVOID*>(&lifetime)));
	Check(factory->QueryInterface(__uuidof(IExternalDesignPolicy),reinterpret_cast<PVOID*>(&policy)));
	Check(registry->RegisterFactory(factory));
	registered = TRUE;
	editor.Attach(__new QFD::CDesignerEditor);
	CheckAlloc(editor);
	Check(editor->Create(host,registry));
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"ExtensionCommands\",\"children\":[{\"type\":\"ExternalMeter\",\"name\":\"Meter1\",\"properties\":{\"Level\":57},\"events\":{\"OnPulse\":\"Meter1_Pulse\"}},{\"type\":\"Button\",\"name\":\"OtherButton\"}]}",&definition));
	Check(editor->LoadDefinition(definition));
	Check(NamedID(editor,L"Meter1",&id));
	Check(editor->SelectID(id));
	Check(editor->ExecuteCommand(QFD::Copy,host));
	Check(editor->Document().Serialize(&before));
	editor->Document().SetDirty(FALSE);
	Check(policy->SetDesignResult(E_ABORT));
	REQUIRE(editor->ExecuteCommand(QFD::SendToFront,host) == E_ABORT && !editor->Document().Dirty());
	REQUIRE(editor->ExecuteCommand(QFD::Paste,host) == E_ABORT && !editor->Document().Dirty());
	Check(editor->Document().Serialize(&after));
	REQUIRE(JSONCompareObjects(before,after) == 0);
	Check(policy->SetDesignResult(S_OK));
	Check(editor->ExecuteCommand(QFD::Paste,host));
	Check(editor->GetSelectedID(&added));
	Check(editor->Document().Find(added,&control));
	{
		CRString name, handler;
		Check(control->GetName(&name));
		Check(QFD::StringProperty(control,L"OnPulse",&handler));
		REQUIRE(!IsEqualGUID(id,added) && name == RSTRING_CAST(L"Meter2") && handler == RSTRING_CAST(L"Meter1_Pulse") && QFD::IntProperty(control,L"Level") == 57);
		}
	control.Release();
	Check(editor->ExecuteCommand(QFD::SendToBack,host));
	Check(editor->ExecuteCommand(QFD::Cut,host));
	REQUIRE(FAILED(editor->Document().Find(added,&control)));
	Check(editor->ExecuteCommand(QFD::Paste,host));
	hr = S_OK;
Cleanup:
	control.Release();
	if(editor && editor->Window()) DestroyWindow(editor->Window());
	editor.Release();
	if(lifetime && SUCCEEDED(lifetime->GetLiveCount(&count)) && !Expect(count == 0,L"External factory live count after clipboard commands",__LINE__) && SUCCEEDED(hr)) hr = E_FAIL;
	if(registered) registry->UnregisterFactory(RSTRING_CAST(L"ExternalMeter"));
	policy.Release(); lifetime.Release(); factory.Release(); registry.Release();
	if(SUCCEEDED(hr)) hr = UnloadExternalExtension();
	return hr;
}

static HRESULT DesignerCommands()
{
	HRESULT hr;
	HWND host = Host();
	CTestClipboard clipboard;
	TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IJSONObject> definition, before, after;
	TStackRef<IJSONValue> value;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooComponent> component;
	GUID rootID, panelID, buttonID, secondID, addedID, pageID, tabsID, timerID;
	HMENU menu = NULL;
	CheckIf(!host,HrEnsureLastError());
	Check(clipboard.Save(host));
	Check(CreateEditor(host,&editor));
	Check(Parse(CommandDocument,&definition));
	Check(editor->LoadDefinition(definition));
	definition.Release();
	Check(editor->Document().Root()->GetID(&rootID));
	Check(NamedID(editor,L"Panel1",&panelID));
	Check(NamedID(editor,L"Button1",&buttonID));
	Check(NamedID(editor,L"Button2",&secondID));
	Check(NamedID(editor,L"Tabs1",&tabsID));
	Check(NamedID(editor,L"Page1",&pageID));
	REQUIRE(!editor->CanExecute(QFD::Cut) && !editor->CanExecute(QFD::Copy) && !editor->CanExecute(QFD::SendToBack) && editor->CanExecute(QFD::TabOrder));
	Check(editor->SelectID(buttonID));
	Check(LockedClipboardCut(editor,host));
	menu = QFD::CreateControlMenu();
	REQUIRE(menu != NULL);
	editor->UpdateCommandMenu(menu);
	REQUIRE((GetMenuState(menu,QFD::ToggleEnabled,MF_BYCOMMAND) & MF_CHECKED) != 0);
	REQUIRE((GetMenuState(menu,QFD::TabOrder,MF_BYCOMMAND) & MF_GRAYED) != 0);
	Check(editor->ExecuteCommand(QFD::ToggleEnabled,host));
	REQUIRE(!editor->IsCommandChecked(QFD::ToggleEnabled));
	Check(editor->Document().Find(buttonID,&control));
	REQUIRE(!QFD::BoolProperty(control,L"Enabled"));
	control.Release();
	// Right clicking changes selection without moving, dirtying, or enabling a control.
	{
		RECT rect; POINT point;
		Check(editor->SelectionRect(&rect));
		point.x = rect.left + 8;
		point.y = rect.top + 8;
		ClientToScreen(editor->CanvasWindow(),&point);
		Check(editor->SelectID(rootID));
		editor->Document().SetDirty(FALSE);
		Check(ContextAt(editor,editor->OverlayWindow(),point,buttonID,FALSE,FALSE));
		REQUIRE(!editor->Document().Dirty());
		HTREEITEM item = FindTree(editor->TreeWindow(),TreeView_GetRoot(editor->TreeWindow()),L"Button1 (");
		TreeView_EnsureVisible(editor->TreeWindow(),item);
		REQUIRE(TreeView_GetItemRect(editor->TreeWindow(),item,&rect,TRUE));
		point.x = rect.left + 5; point.y = (rect.top + rect.bottom) / 2; ClientToScreen(editor->TreeWindow(),&point);
		Check(editor->SelectID(rootID));
		Check(ContextAt(editor,editor->TreeWindow(),point,buttonID,FALSE,FALSE));
		point.x = point.y = -1;
		Check(ContextAt(editor,editor->OverlayWindow(),point,buttonID,FALSE,FALSE));
	}
	Check(editor->ExecuteCommand(QFD::SendToFront,host));
	Check(ChildName(editor,panelID,2,L"Button1"));
	Check(editor->Document().Find(buttonID,&control));
	REQUIRE(QFD::IntProperty(control,L"TabOrder") == 4);
	control.Release();
	REQUIRE(TreeView_GetNextSibling(editor->TreeWindow(),FindTree(editor->TreeWindow(),TreeView_GetRoot(editor->TreeWindow()),L"Button1 (")) == NULL);
	Check(editor->ExecuteCommand(QFD::SendToBack,host));
	Check(ChildName(editor,panelID,0,L"Button1"));
	editor->Document().SetDirty(FALSE);
	REQUIRE(editor->Document().Reorder(buttonID,FALSE) == S_FALSE && !editor->Document().Dirty());
	Check(editor->ExecuteCommand(QFD::Copy,host));
	REQUIRE(!editor->Document().Dirty());
	Check(QFD::ReadDefinitionClipboard(host,&value));
	REQUIRE(value->GetType() == JSON::Object);
	value.Release();
	Check(editor->SelectID(panelID));
	Check(editor->ExecuteCommand(QFD::Paste,host));
	Check(editor->GetSelectedID(&addedID));
	REQUIRE(!IsEqualGUID(addedID,buttonID));
	Check(editor->Document().Find(addedID,&control));
	{
		CRString name, caption, handler;
		Check(control->GetName(&name));
		Check(QFD::StringProperty(control,L"Caption",&caption));
		Check(QFD::StringProperty(control,L"OnClick",&handler));
		REQUIRE(name == RSTRING_CAST(L"Button3") && caption == RSTRING_CAST(L"Copy \x03a9") && handler == RSTRING_CAST(L"Button1_Click"));
		if(QFD::BoolProperty(control,L"Enabled") || QFD::IntProperty(control,L"TabOrder") != 5)
		{
			CRString diagnostic;
			RStrFormatW(&diagnostic,L"Pasted Enabled=%d TabOrder=%d\r\n",QFD::BoolProperty(control,L"Enabled"),QFD::IntProperty(control,L"TabOrder"));
			Log(diagnostic.c_wstr());
		}
		REQUIRE(!QFD::BoolProperty(control,L"Enabled") && QFD::IntProperty(control,L"TabOrder") == 5);
	}
	control.Release();
	Check(editor->Delete());
	// Text-editing shortcuts stay inside the inline edit; palette shortcuts do
	// not unexpectedly copy or delete the selected component.
	Check(editor->SelectID(buttonID));
	Check(NudgeKey(editor,editor->ToolsWindow(),'C',TRUE,FALSE,FALSE,FALSE));
	{
		HWND field = StartInline(editor,L"Name");
		REQUIRE(field != NULL);
		SetFocus(field); SendMessageW(field,EM_SETSEL,0,-1);
		Check(NudgeKey(editor,field,'C',TRUE,FALSE,FALSE,TRUE));
		SendMessageW(field,WM_KEYDOWN,VK_ESCAPE,0); }
	Check(NudgeKey(editor,editor->OverlayWindow(),'C',TRUE,FALSE,FALSE,TRUE));
	// Copy a whole container; every descendant gets a distinct identity and name.
	Check(editor->SelectID(panelID));
	Check(editor->ExecuteCommand(QFD::Copy,host));
	Check(editor->SelectID(rootID));
	Check(editor->ExecuteCommand(QFD::Paste,host));
	Check(editor->GetSelectedID(&addedID));
	Check(ChildName(editor,addedID,0,L"Button3"));
	Check(editor->Delete());
	// Paste into the active page and retain the separate persisted page selection.
	Check(editor->SelectID(buttonID));
	Check(editor->ExecuteCommand(QFD::Copy,host));
	Check(editor->SelectID(pageID));
	Check(editor->SelectID(tabsID));
	Check(editor->ExecuteCommand(QFD::Paste,host));
	Check(editor->GetSelectedID(&addedID));
	{
		TStackRef<IQuadooControlContainer> parent;
		GUID id;
		Check(editor->Document().Find(addedID,&control));
		Check(control->GetParent(&parent));
		Check(parent->GetID(&id));
		REQUIRE(IsEqualGUID(id,pageID));
		control.Release();
	}
	Check(editor->SelectID(pageID));
	Check(editor->ExecuteCommand(QFD::SendToFront,host));
	Check(ChildName(editor,tabsID,1,L"Page1"));
	Check(editor->Document().Find(tabsID,&control));
	REQUIRE(QFD::IntProperty(control,L"ActivePageIndex") == 1);
	control.Release();
	Check(editor->Document().Serialize(&definition));
	Check(editor->LoadDefinition(definition));
	definition.Release();
	Check(editor->Document().Find(tabsID,&control));
	REQUIRE(QFD::IntProperty(control,L"ActivePageIndex") == 0);
	control.Release();
	// Arrays can mix visual controls and nonvisual components. Invalid later
	// entries must roll back the entire operation, including GUIDs and names.
	Check(editor->SelectID(rootID));
	Check(clipboard.Put(host,L"[{\"type\":\"Button\",\"name\":\"ArrayButton\"},{\"type\":\"Timer\",\"name\":\"ArrayTimer\",\"properties\":{\"Interval\":4321}}]"));
	Check(editor->ExecuteCommand(QFD::Paste,host));
	Check(NamedID(editor,L"ArrayButton",&addedID));
	{ TStackRef<IQuadooComponentHost> componentHost; TStackRef<IQuadooComponentCollection> components;
		Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&componentHost)));
		Check(componentHost->GetComponents(&components));
		Check(components->FindComponent(RSTRING_CAST(L"Timer1"),&component));
		Check(component->GetID(&timerID));
		component.Release();
		Check(components->FindComponent(RSTRING_CAST(L"ArrayTimer"),&component));
		REQUIRE(QFD::IntProperty(component,L"Interval") == 4321);
		component.Release();
		}
	Check(editor->SelectID(timerID));
	REQUIRE(editor->CanExecute(QFD::Cut) && !editor->CanExecute(QFD::TabOrder) && editor->CanExecute(QFD::SendToFront));
	Check(editor->ExecuteCommand(QFD::SendToFront,host));
	{
		POINT point = { -1,-1 };
		Check(ContextAt(editor,editor->TrayWindow(),point,timerID,TRUE,FALSE));
	}
	Check(editor->ExecuteCommand(QFD::Cut,host));
	REQUIRE(FAILED(editor->Document().FindComponent(timerID,&component)));
	Check(editor->ExecuteCommand(QFD::Paste,host));
	Check(editor->GetSelectedID(&addedID));
	Check(editor->Document().FindComponent(addedID,&component));
	REQUIRE(QFD::IntProperty(component,L"Interval") == 1234);
	{
		CRString handler;
		Check(QFD::StringProperty(component,L"OnTimer",&handler));
		REQUIRE(handler == RSTRING_CAST(L"Timer1_Tick"));
	}
	component.Release();
	Check(editor->SelectID(rootID));
	editor->Document().SetDirty(FALSE);
	Check(editor->Document().Serialize(&before));
	for(UINT n = 0; n < 4; n++)
	{
		static PCWSTR const invalid[] = { L"{invalid",L"[]",L"[null]",L"[{\"type\":\"Button\",\"name\":\"ValidFirst\"},{\"type\":\"Button\",\"name\":\"InvalidSecond\",\"properties\":{\"Width\":\"bad\"}}]" };
		Check(clipboard.Put(host,invalid[n]));
		IQuadooControl* identity = editor->Document().Root();
		REQUIRE(FAILED(editor->ExecuteCommand(QFD::Paste,host)) && editor->Document().Root() == identity && !editor->Document().Dirty());
		after.Release();
		Check(editor->Document().Serialize(&after));
		REQUIRE(JSONCompareObjects(before,after) == 0);
	}
	Check(ExtensionClipboard(host));
	hr = S_OK;
Cleanup:
	if(menu) DestroyMenu(menu);
	control.Release(); component.Release();
	if(host) DestroyWindow(host);
	editor.Release(); PumpPendingMessages();
	return hr;
}
