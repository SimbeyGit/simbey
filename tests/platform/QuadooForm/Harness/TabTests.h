// Tab/page tests use only published interfaces and the native common control.
static const WCHAR TabExampleDefinition[] =
	L"{\"type\":\"Form\",\"name\":\"TabExample\",\"properties\":{\"Caption\":\"QuadooForm tab pages\",\"Width\":520,\"Height\":330},\"children\":["
	L"{\"type\":\"TabControl\",\"name\":\"tabs\",\"properties\":{\"Align\":\"Client\",\"ActivePageIndex\":0},\"events\":{\"OnChanging\":\"Changing\",\"OnChange\":\"Changed\"},\"children\":["
	L"{\"type\":\"TabPage\",\"name\":\"general\",\"properties\":{\"Caption\":\"General\"},\"events\":{\"OnShow\":\"Show\",\"OnHide\":\"Hide\"},\"children\":["
	L"{\"type\":\"Label\",\"name\":\"prompt\",\"properties\":{\"Caption\":\"General settings\",\"Left\":12,\"Top\":12,\"AutoSize\":true}},"
	L"{\"type\":\"TextBox\",\"name\":\"firstEdit\",\"properties\":{\"Left\":12,\"Top\":40,\"Width\":230,\"Text\":\"Ctrl+Tab changes pages\"}},"
	L"{\"type\":\"Button\",\"name\":\"firstButton\",\"properties\":{\"Caption\":\"Apply\",\"Left\":12,\"Top\":80,\"TabOrder\":1}}]},"
	L"{\"type\":\"TabPage\",\"name\":\"hidden\",\"properties\":{\"Caption\":\"Hidden page\",\"TabVisible\":false}},"
	L"{\"type\":\"TabPage\",\"name\":\"advanced\",\"properties\":{\"Caption\":\"Advanced\"},\"events\":{\"OnShow\":\"Show\",\"OnHide\":\"Hide\"},\"children\":["
	L"{\"type\":\"ComboBox\",\"name\":\"secondEdit\",\"properties\":{\"Left\":12,\"Top\":12,\"Width\":230,\"Items\":[\"Automatic\",\"Manual\"],\"ItemIndex\":0}},"
	L"{\"type\":\"Panel\",\"name\":\"pageClient\",\"properties\":{\"Top\":60,\"Width\":240,\"Height\":120,\"Anchors\":15},\"children\":[{\"type\":\"CheckBox\",\"name\":\"option\",\"properties\":{\"Caption\":\"Enable advanced settings\",\"Width\":230}}]}]}]}]}";
class CTabBinder : public CBaseUnknown, public IQuadooEventsBinder, public IQuadooDelegate
{
public:
	UINT changing, changed, shows, hides;
	BOOL cancel, failChanging, failChange, destroyChange, destroyChanging, reenter;
	HRESULT reentered;
	IQuadooTabPage* deletePage;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooEventsBinder)
		UNK_INTERFACE(IQuadooDelegate)
	END_UNK_MAP
	CTabBinder() : changing(0), changed(0), shows(0), hides(0), cancel(FALSE), failChanging(FALSE), failChange(FALSE), destroyChange(FALSE), destroyChanging(FALSE), reenter(FALSE), reentered(S_OK), deletePage(NULL) {}
	STDMETHOD(BindEvent)(RSTRING name, IQuadooDelegate** result)
	{ if(!result) return E_POINTER; *result = NULL; if(CRString(name) == RSTRING_CAST(L"Missing")) return QuadooForm::EHandlerNotFound; *result = this; AddRef(); return S_OK; }
	STDMETHOD(PostBind)(IQuadooControl*) { return S_OK; }
	STDMETHOD(Invoke)(IQuadooEventArgs* args)
	{
		HRESULT hr;
		CRString name; TStackRef<IQuadooComponent> sender;
		Check(args->GetEventName(&name)); Check(args->GetSender(&sender));
		if(name == RSTRING_CAST(L"OnChanging"))
		{
			changing++; Check(args->SetBool(0,cancel));
			if(reenter) { TStackRef<IQuadooTabControl> tabs; INT index; Check(sender->QueryInterface(__uuidof(IQuadooTabControl),reinterpret_cast<PVOID*>(&tabs))); Check(tabs->GetActivePageIndex(&index)); reentered = tabs->SetActivePageIndex(index == 0 ? 2 : 0); }
			if(deletePage) { IQuadooTabPage* page = deletePage; deletePage = NULL; Check(page->Destroy()); }
			if(destroyChanging) { Check(sender->Destroy()); }
			CheckIf(failChanging,E_ABORT);
		}
		else if(name == RSTRING_CAST(L"OnChange"))
		{
			TStackRef<IQuadooTabControl> tabs; TStackRef<IQuadooTabPage> active; INT index; HWND hwnd;
			changed++; Check(sender->QueryInterface(__uuidof(IQuadooTabControl),reinterpret_cast<PVOID*>(&tabs))); Check(tabs->GetActivePage(&active)); Check(tabs->GetActivePageIndex(&index));
			if(active) { Check(active->GetHandle(&hwnd)); REQUIRE((GetWindowLongW(hwnd,GWL_STYLE) & WS_VISIBLE) != 0); REQUIRE(index >= 0); }
			if(destroyChange) { Check(sender->Destroy()); }
			CheckIf(failChange,E_ABORT);
		}
		else if(name == RSTRING_CAST(L"OnShow")) shows++;
		else if(name == RSTRING_CAST(L"OnHide")) hides++;
		hr = S_OK;
	Cleanup:
		return hr;
	}
};
static HRESULT TabForm(CTabBinder* binder, IQuadooConstructionContext** context, IQuadooForm** form, IQuadooTabControl** tabs, IQuadooControlRegistry* registry = NULL, QuadooForm::ContextMode mode = QuadooForm::Runtime)
{
	HRESULT hr; TStackRef<IJSONObject> definition; TStackRef<IQuadooControl> root, child;
	Check(Context(96,binder,context,registry,mode)); Check(Parse(TabExampleDefinition,&definition)); Check(QuadooCreateControl(*context,NULL,definition,&root));
	Check(root->QueryInterface(__uuidof(IQuadooForm),reinterpret_cast<PVOID*>(form))); Check((*form)->FindControl(RSTRING_CAST(L"tabs"),&child));
	Check(child->QueryInterface(__uuidof(IQuadooTabControl),reinterpret_cast<PVOID*>(tabs)));
Cleanup:
	if(FAILED(hr) && root) root->Destroy();
	return hr;
}
static HRESULT TabVisiblePages(IQuadooTabControl* tabs, INT selected)
{
	HRESULT hr; UINT count; INT index;
	Check(tabs->GetActivePageIndex(&index));
	if(index != selected) { CRString message; if(SUCCEEDED(RStrFormatW(&message,L"tab index: expected %d, actual %d\r\n",selected,index))) Log(message.c_wstr()); }
	REQUIRE(index == selected); Check(tabs->GetPageCount(&count));
	for(UINT n = 0; n < count; n++)
	{
		TStackRef<IQuadooTabPage> page; HWND hwnd; BOOL visible;
		Check(tabs->GetPage(n,&page)); Check(page->GetHandle(&hwnd)); Check(page->GetVisible(&visible));
		REQUIRE(((GetWindowLongW(hwnd,GWL_STYLE) & WS_VISIBLE) != 0) == (visible && static_cast<INT>(n) == selected));
	}
Cleanup:
	return hr;
}
static HRESULT TabClick(IQuadooTabControl* tabs, INT header)
{
	HRESULT hr; HWND hwnd; RECT rect;
	Check(tabs->GetHandle(&hwnd)); REQUIRE(SendMessageW(hwnd,TCM_GETITEMRECT,header,reinterpret_cast<LPARAM>(&rect)));
	LPARAM point = MAKELPARAM((rect.left + rect.right) / 2,(rect.top + rect.bottom) / 2);
	SendMessageW(hwnd,WM_LBUTTONDOWN,MK_LBUTTON,point); SendMessageW(hwnd,WM_LBUTTONUP,0,point); hr = S_OK;
Cleanup:
	return hr;
}
static HRESULT TabCtrlKey(HWND target, BOOL reverse, BOOL dialog, HWND form)
{
	HRESULT hr;
	BYTE original[256], keys[256]; BOOL saved = FALSE;
	CheckIf(!GetKeyboardState(original),HrEnsureLastError()); CopyMemory(keys,original,sizeof(keys));
	keys[VK_CONTROL] |= 0x80; keys[VK_SHIFT] = reverse ? 0x80 : 0;
	CheckIf(!SetKeyboardState(keys),HrEnsureLastError()); saved = TRUE;
	if(dialog)
	{ MSG message; ZeroMemory(&message,sizeof(message)); message.hwnd = target; message.message = WM_KEYDOWN; message.wParam = VK_TAB; if(!IsDialogMessageW(form,&message)) DispatchMessageW(&message); }
	else SendMessageW(target,WM_KEYDOWN,VK_TAB,0);
	hr = S_OK;
Cleanup:
	if(saved) SetKeyboardState(original);
	return hr;
}
static HRESULT TabInteraction()
{
	HRESULT hr;
	TStackRef<CTabBinder> binder; TStackRef<IQuadooConstructionContext> context;
	TStackRef<IQuadooForm> form; TStackRef<IQuadooTabControl> tabs; TStackRef<IQuadooTabPage> first, hidden, second, active;
	TStackRef<IQuadooWinControl> edit, combo; HWND hwnd, firstWindow, secondWindow, formWindow; UINT count; INT index;
	binder.Attach(__new CTabBinder); CheckAlloc(binder); Check(TabForm(binder,&context,&form,&tabs));
	Check(tabs->GetPageCount(&count)); REQUIRE(count == 3); Check(tabs->GetHandle(&hwnd)); REQUIRE(SendMessageW(hwnd,TCM_GETITEMCOUNT,0,0) == 2);
	Check(tabs->GetPage(0,&first)); Check(tabs->GetPage(1,&hidden)); Check(tabs->GetPage(2,&second)); Check(TabVisiblePages(tabs,0));
	REQUIRE(tabs->RegisterControl(first) == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)); REQUIRE(SendMessageW(hwnd,TCM_GETITEMCOUNT,0,0) == 2);
	Check(tabs->GetActivePage(&active)); REQUIRE(active == static_cast<IQuadooTabPage*>(first)); active.Release(); REQUIRE(binder->changing == 0 && binder->changed == 0);
	Check(form->ShowForm(SW_SHOW)); Check(form->GetHandle(&formWindow)); PumpPendingMessages();
	Check(ChoiceWindow(first,L"firstEdit",&edit,&firstWindow)); Check(ChoiceWindow(second,L"secondEdit",&combo,&secondWindow)); Check(edit->SetFocus());
	Check(tabs->SetActivePage(second)); Check(TabVisiblePages(tabs,2)); REQUIRE(IsChild(secondWindow,GetFocus()) || GetFocus() == secondWindow);
	Check(tabs->SetActivePageIndex(0)); REQUIRE(GetFocus() == firstWindow); REQUIRE(binder->changing == 0 && binder->changed == 0);
	REQUIRE(tabs->SetActivePageIndex(3) == E_INVALIDARG && tabs->SetActivePageIndex(-2) == E_INVALIDARG); REQUIRE(tabs->SetActivePageIndex(0) == S_FALSE);
	Check(TabClick(tabs,1)); Check(TabVisiblePages(tabs,2)); REQUIRE(binder->changing == 1 && binder->changed == 1);
	binder->cancel = TRUE; Check(TabClick(tabs,0)); Check(TabVisiblePages(tabs,2)); REQUIRE(binder->changing == 2 && binder->changed == 1); binder->cancel = FALSE;
	Check(tabs->SetFocus()); SendMessageW(hwnd,WM_KEYDOWN,VK_HOME,0); Check(TabVisiblePages(tabs,0));
	SendMessageW(hwnd,WM_KEYDOWN,VK_RIGHT,0); Check(TabVisiblePages(tabs,2));
	SendMessageW(hwnd,WM_KEYDOWN,VK_END,0); Check(TabVisiblePages(tabs,2));
	Check(tabs->SetActivePageIndex(0)); Check(edit->SetFocus()); Check(TabCtrlKey(firstWindow,FALSE,TRUE,formWindow)); Check(TabVisiblePages(tabs,2));
	// The ComboBox edit child participates in dialog Ctrl+Tab without script hooks.
	Check(combo->SetFocus()); COMBOBOXINFO info; ZeroMemory(&info,sizeof(info)); info.cbSize = sizeof(info); REQUIRE(GetComboBoxInfo(secondWindow,&info));
	Check(TabCtrlKey(info.hwndItem,TRUE,TRUE,formWindow)); Check(TabVisiblePages(tabs,0)); REQUIRE(GetFocus() == firstWindow);
	UINT changes = binder->changed, changing = binder->changing;
	REQUIRE(SendMessageW(hwnd,TCM_SETCURSEL,1,0) == 0); Check(TabVisiblePages(tabs,2)); REQUIRE(binder->changed == changes && binder->changing == changing);
	REQUIRE(SendMessageW(hwnd,TCM_DELETEALLITEMS,0,0) == FALSE && SendMessageW(hwnd,TCM_GETITEMCOUNT,0,0) == 2);
	// Header visibility and page indexes are independent. Hidden pages remain
	// selectable by object or all-page index and serialize as ordinary containers.
	Check(tabs->SetActivePage(hidden)); Check(TabVisiblePages(tabs,1)); REQUIRE(SendMessageW(hwnd,TCM_GETCURSEL,0,0) == -1);
	{ TStackRef<IQuadooProperties> properties; UINT caption; Check(Property(hidden,L"Caption",&properties,&caption)); Check(properties->SetValueString(caption,RSTRING_CAST(L"Renamed hidden page"))); }
	Check(TabVisiblePages(tabs,1)); REQUIRE(SendMessageW(hwnd,TCM_GETITEMCOUNT,0,0) == 2 && SendMessageW(hwnd,TCM_GETCURSEL,0,0) == -1);
	Check(hidden->SetTabVisible(TRUE)); REQUIRE(SendMessageW(hwnd,TCM_GETITEMCOUNT,0,0) == 3 && SendMessageW(hwnd,TCM_GETCURSEL,0,0) == 1);
	Check(hidden->SetTabVisible(FALSE)); Check(TabVisiblePages(tabs,2));
	Check(combo->SetFocus()); Check(second->SetTabVisible(FALSE)); Check(TabVisiblePages(tabs,0)); REQUIRE(GetFocus() == firstWindow);
	Check(second->SetTabVisible(TRUE)); Check(tabs->SetActivePage(second));
	Check(second->SetPageIndex(0)); Check(second->GetPageIndex(&index)); REQUIRE(index == 0); Check(TabVisiblePages(tabs,0));
	{ WCHAR text[80]; TCITEMW item; ZeroMemory(&item,sizeof(item)); item.mask = TCIF_TEXT; item.pszText = text; item.cchTextMax = ARRAYSIZE(text);
		REQUIRE(SendMessageW(hwnd,TCM_GETITEMW,0,reinterpret_cast<LPARAM>(&item))); REQUIRE(0 == TStrCmpAssert(text,L"Advanced")); }
	Check(tabs->MovePage(first,0)); Check(TabVisiblePages(tabs,1)); REQUIRE(binder->changed == changes);
	Check(tabs->SetActivePage(NULL)); Check(TabVisiblePages(tabs,-1)); Check(tabs->SelectNextPage(TRUE)); Check(TabVisiblePages(tabs,0));
	binder->reenter = TRUE; Check(TabClick(tabs,1)); REQUIRE(binder->reentered == QuadooForm::EInvalidState); binder->reenter = FALSE;
	binder->failChanging = TRUE; changing = binder->changed; Check(TabClick(tabs,0)); Check(TabVisiblePages(tabs,1)); REQUIRE(binder->changed == changing); binder->failChanging = FALSE;
	binder->failChange = TRUE; Check(TabClick(tabs,0)); Check(TabVisiblePages(tabs,0)); binder->failChange = FALSE;
	Check(form->GetChildCount(&count)); REQUIRE(count == 1);
Cleanup:
	if(form) form->Destroy();
	return hr;
}
static HRESULT TabPersistence()
{
	HRESULT hr;
	TStackRef<CTabBinder> binder; TStackRef<IQuadooConstructionContext> context;
	TStackRef<IQuadooForm> form; TStackRef<IQuadooTabControl> tabs; TStackRef<IQuadooTabPage> page;
	TStackRef<IQuadooControlRegistry> registry; TStackRef<IQuadooControlFactory> external;
	TStackRef<IQuadooControl> meter, copy; TStackRef<IJSONObject> definition, first, second;
	TStackRef<IQuadooFormSerializer> serializer; HWND hwnd, pageWindow;
	binder.Attach(__new CTabBinder); CheckAlloc(binder); Check(QuadooGetControlRegistry(&registry)); Check(CreateExternalFactory(&external)); Check(registry->RegisterFactory(external));
	Check(TabForm(binder,&context,&form,&tabs,registry)); Check(tabs->GetPage(2,&page));
	// The first layout establishes anchors against the final page viewport.
	{ TStackRef<IQuadooControl> anchored; QuadooForm::Rect bounds;
		Check(form->FindControl(RSTRING_CAST(L"pageClient"),&anchored)); Check(anchored->GetEffectiveBounds(&bounds)); REQUIRE(bounds.Width == 240 && bounds.Height == 120); }
	Check(Parse(L"{\"type\":\"ExternalMeter\",\"name\":\"pageExtension\",\"properties\":{\"Align\":2,\"Height\":20,\"Level\":17}}",&definition)); Check(QuadooCreateControl(context,page,definition,&meter)); definition.Release();
	Check(tabs->SetActivePage(page));
	{ const IID* paths[] = { &__uuidof(IQuadooComponent),&__uuidof(IQuadooControl),&__uuidof(IQuadooWinControl),&__uuidof(IQuadooControlContainer),&__uuidof(IQuadooChildWindowHost),&__uuidof(IQuadooTabControl),&__uuidof(IQuadooProperties),&__uuidof(IQuadooPropertyNotifications),&__uuidof(IQuadooConstructionParticipant),&__uuidof(IQuadooLayoutControl),&__uuidof(IQuadooMessageHandler) }; Check(CheckCOMIdentity(tabs,paths,ARRAYSIZE(paths))); }
	{ const IID* paths[] = { &__uuidof(IQuadooComponent),&__uuidof(IQuadooControl),&__uuidof(IQuadooWinControl),&__uuidof(IQuadooControlContainer),&__uuidof(IQuadooChildWindowHost),&__uuidof(IQuadooTabPage),&__uuidof(IQuadooProperties),&__uuidof(IQuadooPropertyNotifications),&__uuidof(IQuadooConstructionParticipant),&__uuidof(IQuadooLayoutControl),&__uuidof(IQuadooMessageHandler) }; Check(CheckCOMIdentity(page,paths,ARRAYSIZE(paths))); }
	{ TStackRef<IUnknown> wrong; REQUIRE(tabs->QueryInterface(__uuidof(IQuadooTabPage),reinterpret_cast<PVOID*>(&wrong)) == E_NOINTERFACE && !wrong); REQUIRE(page->QueryInterface(__uuidof(IQuadooTabControl),reinterpret_cast<PVOID*>(&wrong)) == E_NOINTERFACE && !wrong); }
	Check(tabs->GetHandle(&hwnd)); Check(page->GetHandle(&pageWindow)); REQUIRE(GetParent(pageWindow) == hwnd);
	QuadooForm::Rect stored; Check(page->GetBounds(&stored));
	for(UINT dpi = 96; dpi <= 192; dpi += 48)
	{
		TStackRef<IQuadooLayoutControl> layout; QuadooForm::Rect bounds = { 0,0,520,330 }, effective, after; RECT client, native;
		Check(form->QueryInterface(__uuidof(IQuadooLayoutControl),reinterpret_cast<PVOID*>(&layout))); Check(layout->Arrange(&bounds,dpi));
		REQUIRE(GetClientRect(hwnd,&client)); SendMessageW(hwnd,TCM_ADJUSTRECT,FALSE,reinterpret_cast<LPARAM>(&client));
		Check(page->GetEffectiveBounds(&effective)); REQUIRE(effective.Left == MulDiv(client.left,96,dpi) && effective.Top == MulDiv(client.top,96,dpi));
		REQUIRE(GetWindowRect(pageWindow,&native)); MapWindowPoints(NULL,hwnd,reinterpret_cast<POINT*>(&native),2);
		REQUIRE(abs(native.left - client.left) <= 1 && abs(native.top - client.top) <= 1);
		Check(page->GetBounds(&after)); REQUIRE(EqualBounds(stored,after));
		Check(meter->GetEffectiveBounds(&effective)); REQUIRE(effective.Top + effective.Height <= MulDiv(native.bottom - native.top,96,dpi));
	}
	Check(ChoiceBool(tabs,L"MultiLine",TRUE)); Check(ChoiceBool(tabs,L"HotTrack",TRUE)); REQUIRE((GetWindowLongW(hwnd,GWL_STYLE) & (TCS_MULTILINE | TCS_HOTTRACK)) == (TCS_MULTILINE | TCS_HOTTRACK));
	{ TStackRef<IQuadooProperties> properties; UINT index; Check(Property(page,L"Caption",&properties,&index)); Check(properties->SetValueString(index,RSTRING_CAST(L"Edited tab caption"))); }
	Check(page->SetPageIndex(0)); Check(QuadooGetFormSerializer(&serializer)); Check(serializer->Serialize(form,&first));
	Check(QuadooCreateControl(context,NULL,first,&copy)); Check(serializer->Serialize(copy,&second)); REQUIRE(JSONCompareObjects(first,second) == 0);
	{ TStackRef<IQuadooControlContainer> container; TStackRef<IQuadooControl> found; Check(copy->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&container))); Check(container->FindControl(RSTRING_CAST(L"pageExtension"),&found)); REQUIRE(ChoiceInt(found,L"Level") == 17); }
	Check(copy->Destroy()); copy.Release();
	// The content's requested Visible value is preserved independently of selection.
	Check(page->SetVisible(FALSE)); Check(TabVisiblePages(tabs,0)); Check(page->SetVisible(TRUE)); Check(TabVisiblePages(tabs,0));
	{ TStackRef<IQuadooPropertyNotifications> notifications; TStackRef<CObserver> observer;
		observer.Attach(__new CObserver); CheckAlloc(observer); Check(tabs->QueryInterface(__uuidof(IQuadooPropertyNotifications),reinterpret_cast<PVOID*>(&notifications))); Check(notifications->AddObserver(observer));
		Check(form->BeginUpdate()); Check(tabs->SetActivePageIndex(1)); Check(tabs->SetActivePageIndex(0)); REQUIRE(observer->properties == 0); Check(form->EndUpdate()); REQUIRE(observer->properties == 1); Check(notifications->RemoveObserver(observer)); }
Cleanup:
	if(copy) copy->Destroy(); if(form) form->Destroy();
	return hr;
}
static HRESULT TabResources()
{
	HRESULT hr;
	TStackRef<CTabBinder> binder; TStackRef<IQuadooConstructionContext> context;
	TStackRef<IQuadooForm> form; TStackRef<IQuadooTabControl> tabs; TStackRef<IQuadooTabPage> page;
	TStackRef<IQuadooControlRegistry> registry; TStackRef<IQuadooControlFactory> factory; TStackRef<IQuadooFactoryLifetime> life[2];
	TStackRef<IQuadooControl> failed; TStackRef<IJSONObject> definition, before, after; TStackRef<IQuadooFormSerializer> serializer;
	CGuiResourceMonitor resources(L"Tab control resources"); UINT count, live;
	binder.Attach(__new CTabBinder); CheckAlloc(binder); Check(QuadooGetControlRegistry(&registry));
	Check(registry->FindFactory(RSTRING_CAST(L"TabControl"),&factory)); Check(factory->QueryInterface(__uuidof(IQuadooFactoryLifetime),reinterpret_cast<PVOID*>(&life[0]))); factory.Release();
	Check(registry->FindFactory(RSTRING_CAST(L"TabPage"),&factory)); Check(factory->QueryInterface(__uuidof(IQuadooFactoryLifetime),reinterpret_cast<PVOID*>(&life[1]))); factory.Release();
	Check(TabForm(binder,&context,&form,&tabs,registry)); Check(QuadooGetFormSerializer(&serializer)); Check(serializer->Serialize(form,&before));
	static PCWSTR const failures[] = {
		L"{\"type\":\"TabPage\",\"name\":\"bad\"}",
		L"{\"type\":\"TabControl\",\"name\":\"bad\",\"children\":[{\"type\":\"Button\",\"name\":\"illegal\"}]}",
		L"{\"type\":\"TabControl\",\"name\":\"bad\",\"properties\":{\"ActivePageIndex\":2},\"children\":[{\"type\":\"TabPage\",\"name\":\"only\"}]}",
		L"{\"type\":\"TabControl\",\"name\":\"bad\",\"children\":[{\"type\":\"TabPage\",\"name\":\"valid\"},{\"type\":\"TabPage\",\"name\":\"valid\"}]}",
		L"{\"type\":\"TabControl\",\"name\":\"bad\",\"events\":{\"OnChange\":\"Missing\"},\"children\":[{\"type\":\"TabPage\",\"name\":\"valid\"}]}" };
	for(UINT n = 0; n < ARRAYSIZE(failures); n++)
	{ Check(Parse(failures[n],&definition)); REQUIRE(FAILED(QuadooCreateControl(context,form,definition,&failed)) && !failed); definition.Release(); Check(serializer->Serialize(form,&after)); REQUIRE(JSONCompareObjects(before,after) == 0); after.Release(); }
	Check(tabs->GetPage(0,&page)); Check(page->Destroy()); page.Release(); Check(tabs->GetPageCount(&count)); REQUIRE(count == 2); Check(TabVisiblePages(tabs,1));
	Check(tabs->GetPage(1,&page)); Check(page->Destroy()); page.Release(); Check(TabVisiblePages(tabs,-1));
	Check(Parse(L"{\"type\":\"TabPage\",\"name\":\"dynamic\",\"properties\":{\"Caption\":\"Dynamic\"},\"children\":[{\"type\":\"Button\",\"name\":\"dynamicButton\",\"properties\":{\"Width\":100,\"Height\":28,\"Anchors\":15}}]}",&definition)); Check(QuadooCreateControl(context,tabs,definition,&failed)); definition.Release(); Check(TabVisiblePages(tabs,1));
	{ TStackRef<IQuadooControl> anchored; TStackRef<IQuadooLayoutControl> layout; QuadooForm::Rect bounds = { 0,0,620,430 }, before, after;
		Check(form->FindControl(RSTRING_CAST(L"dynamicButton"),&anchored)); Check(anchored->GetEffectiveBounds(&before)); REQUIRE(before.Width == 100 && before.Height == 28);
		Check(form->QueryInterface(__uuidof(IQuadooLayoutControl),reinterpret_cast<PVOID*>(&layout))); Check(layout->Arrange(&bounds,96)); Check(anchored->GetEffectiveBounds(&after)); REQUIRE(after.Width == before.Width + 100 && after.Height == before.Height + 100); }
	Check(failed->Destroy()); failed.Release();
	Check(form->Destroy()); form.Release(); tabs.Release(); context.Release();
	Check(TabForm(binder,&context,&form,&tabs,registry)); Check(tabs->GetPage(2,&page)); binder->deletePage = page;
	Check(TabClick(tabs,1)); page.Release(); Check(TabVisiblePages(tabs,0)); Check(tabs->GetPageCount(&count)); REQUIRE(count == 2);
	Check(form->Destroy()); form.Release(); tabs.Release(); context.Release();
	// Destruction during both cancellable and committed native notification paths.
	Check(TabForm(binder,&context,&form,&tabs,registry)); binder->destroyChanging = TRUE; Check(TabClick(tabs,1)); binder->destroyChanging = FALSE;
	{ QuadooForm::ComponentState state; Check(tabs->GetState(&state)); REQUIRE(state == QuadooForm::Destroyed); }
	Check(form->Destroy()); form.Release(); tabs.Release(); context.Release();
	Check(TabForm(binder,&context,&form,&tabs,registry)); binder->destroyChange = TRUE; Check(TabClick(tabs,1)); binder->destroyChange = FALSE;
	{ QuadooForm::ComponentState state; Check(tabs->GetState(&state)); REQUIRE(state == QuadooForm::Destroyed); }
	Check(form->Destroy()); form.Release(); tabs.Release(); context.Release();
	while(!resources.Complete())
	{
		TArray<HWND> windows; HWND hwnd;
		Check(TabForm(binder,&context,&form,&tabs,registry)); Check(form->ShowForm(SW_SHOWNOACTIVATE)); Check(tabs->GetHandle(&hwnd));
		Check(tabs->SetFocus()); Check(TabClick(tabs,1)); Check(tabs->GetPage(2,&page)); Check(page->SetPageIndex(0)); page.Release();
		for(UINT dpi = 96; dpi <= 192; dpi += 48) { TStackRef<IQuadooLayoutControl> layout; QuadooForm::Rect bounds = { 0,0,520,330 }; Check(form->QueryInterface(__uuidof(IQuadooLayoutControl),reinterpret_cast<PVOID*>(&layout))); Check(layout->Arrange(&bounds,dpi)); }
		PumpPendingMessages(); Check(CollectNativeWindows(form,windows)); Check(form->Destroy()); for(sysint n = 0; n < windows.Length(); n++) REQUIRE(!IsWindow(windows[n]));
		Check(life[0]->GetLiveCount(&live)); REQUIRE(live == 1); REQUIRE(registry->UnregisterFactory(RSTRING_CAST(L"TabControl")) == HRESULT_FROM_WIN32(ERROR_BUSY));
		tabs.Release(); form.Release(); context.Release();
		for(UINT n = 0; n < 2; n++) { Check(life[n]->GetLiveCount(&live)); REQUIRE(live == 0); }
		Check(resources.AfterCycle());
	}
Cleanup:
	if(binder) binder->deletePage = NULL; if(failed) failed->Destroy(); if(form) form->Destroy();
	return hr;
}
