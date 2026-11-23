static BOOL PaletteIconSize(HICON icon, INT pixels)
{
	ICONINFO info; BITMAP bitmap; BOOL valid = FALSE;
	if(icon && GetIconInfo(icon,&info))
	{
		if(info.hbmColor && GetObject(info.hbmColor,sizeof(bitmap),&bitmap)) valid = bitmap.bmWidth == pixels && bitmap.bmHeight == pixels;
		if(info.hbmColor) DeleteObject(info.hbmColor);
		if(info.hbmMask) DeleteObject(info.hbmMask);
	}
	return valid;
}
struct PaletteThreadCall { IQuadooPaletteIcon* provider; HRESULT result; HICON icon; };
static DWORD WINAPI PaletteWrongThread(PVOID data)
{
	PaletteThreadCall* call = static_cast<PaletteThreadCall*>(data);
	call->result = call->provider->GetPaletteIcon(20,20,&call->icon);
	return 0;
}
static HRESULT VerifyFactoryIcon(IUnknown* factory, const IID& factoryIID)
{
	HRESULT hr;
	TStackRef<IQuadooPaletteIcon> provider;
	TStackRef<IQuadooFactoryLifetime> lifetime;
	HICON first = NULL, second = NULL;
	HANDLE thread = NULL;
	UINT before, after;
	const IID* paths[] = { &factoryIID,&__uuidof(IQuadooFactoryLifetime),&__uuidof(IQuadooPaletteIcon) };
	PaletteThreadCall call;
	Check(factory->QueryInterface(__uuidof(IQuadooPaletteIcon),reinterpret_cast<PVOID*>(&provider)));
	Check(factory->QueryInterface(__uuidof(IQuadooFactoryLifetime),reinterpret_cast<PVOID*>(&lifetime)));
	Check(CheckCOMIdentity(factory,paths,ARRAYSIZE(paths)));
	Check(lifetime->GetLiveCount(&before));
	Check(provider->GetPaletteIcon(20,20,&first));
	Check(provider->GetPaletteIcon(20,20,&second));
	REQUIRE(first != second && PaletteIconSize(first,20) && PaletteIconSize(second,20));
	DestroyIcon(first); first = NULL;
	REQUIRE(PaletteIconSize(second,20));
	DestroyIcon(second); second = NULL;
	Check(provider->GetPaletteIcon(30,30,&first));
	REQUIRE(PaletteIconSize(first,30));
	REQUIRE(provider->GetPaletteIcon(20,20,NULL) == E_POINTER);
	second = reinterpret_cast<HICON>(1);
	REQUIRE(provider->GetPaletteIcon(0,20,&second) == E_INVALIDARG && !second);
	REQUIRE(provider->GetPaletteIcon(20,257,&second) == E_INVALIDARG && !second);
	call.provider = provider; call.icon = NULL; call.result = S_OK;
	thread = CreateThread(NULL,0,PaletteWrongThread,&call,0,NULL);
	CheckIf(!thread,HrEnsureLastError());
	REQUIRE(WaitForSingleObject(thread,10000) == WAIT_OBJECT_0);
	REQUIRE(call.result == QuadooForm::EWrongThread && !call.icon);
	Check(lifetime->GetLiveCount(&after));
	REQUIRE(before == after);
Cleanup:
	if(thread) CloseHandle(thread);
	if(first) DestroyIcon(first);
	if(second && second != reinterpret_cast<HICON>(1)) DestroyIcon(second);
	return hr;
}
static HRESULT PaletteFactoryIcons()
{
	HRESULT hr;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooFactoryCatalog> catalog;
	TStackRef<IQuadooComponentRegistry> components;
	UINT count;
	HICON icon = NULL;
	Check(QuadooGetControlRegistry(&registry));
	Check(registry->QueryInterface(__uuidof(IQuadooFactoryCatalog),reinterpret_cast<PVOID*>(&catalog)));
	Check(registry->QueryInterface(__uuidof(IQuadooComponentRegistry),reinterpret_cast<PVOID*>(&components)));
	Check(catalog->GetFactoryCount(&count));
	REQUIRE(count == 25);
	for(UINT n = 0; n < count; n++)
	{
		TStackRef<IQuadooControlFactory> factory;
		Check(catalog->GetFactory(n,&factory));
		Check(VerifyFactoryIcon(factory,__uuidof(IQuadooControlFactory)));
	}
	Check(components->GetComponentFactoryCount(&count));
	REQUIRE(count == 8);
	for(UINT n = 0; n < count; n++)
	{
		TStackRef<IQuadooComponentFactory> factory;
		Check(components->GetComponentFactory(n,&factory));
		Check(VerifyFactoryIcon(factory,__uuidof(IQuadooComponentFactory)));
	}
	// Verify these are ordinary module ICON resources, not just generated handles.
	for(UINT id = 101; id <= 121; id++)
	{
		icon = static_cast<HICON>(LoadImageW(GetModuleHandleW(L"QuadooForm.dll"),MAKEINTRESOURCEW(id),IMAGE_ICON,20,20,0));
		REQUIRE(PaletteIconSize(icon,20));
		DestroyIcon(icon); icon = NULL;
	}
Cleanup:
	if(icon) DestroyIcon(icon);
	return hr;
}
static INT PaletteRowImage(HWND list, INT row)
{
	LVITEMW item; ZeroMemory(&item,sizeof(item)); item.mask = LVIF_IMAGE; item.iItem = row;
	return ListView_GetItem(list,&item) ? item.iImage : -1;
}
static HRESULT PaletteViews()
{
	HRESULT hr;
	HWND host = Host(), tools, tray;
	TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooComponentRegistry> components;
	TStackRef<IQuadooControlFactory> visual;
	TStackRef<IQuadooComponentFactory> service;
	TStackRef<IExternalPaletteTest> visualTest, serviceTest;
	TStackRef<IJSONObject> before, after, missing;
	TStackRef<IQuadooComponentCollection> collection;
	TStackRef<IQuadooComponent> selectedComponent;
	GUID selected, first, second;
	HIMAGELIST images;
	INT cx, cy, imageCount, firstImage;
	UINT calls, currentCalls, count;
	WCHAR name[80];
	CheckIf(!host,HrEnsureLastError());
	Check(CreateEditor(host,&editor));
	tools = GetDlgItem(editor->Window(),102); tray = editor->TrayWindow();
	GetClassNameW(tools,name,ARRAYSIZE(name));
	REQUIRE(!lstrcmpW(name,WC_LISTVIEWW) && ListView_GetItemCount(tools) == 31);
	REQUIRE(FindTool(tools,L"ImageButton") > 0 && PaletteRowImage(tools,FindTool(tools,L"ImageButton")) >= 4);
	REQUIRE(FindTool(tools,L"Form") < 0 && FindTool(tools,L"Frame") < 0 && FindTool(tools,L"--- Visual Controls ---") < 0 && FindTool(tools,L"--- Nonvisual Components ---") < 0);
	ListView_GetItemText(tools,0,0,name,ARRAYSIZE(name));
	REQUIRE(!lstrcmpW(name,L"Pointer") && ListView_GetNextItem(tools,-1,LVNI_SELECTED) == 0);
	{
		RECT item;
		REQUIRE(ListView_GetItemRect(tools,0,&item,LVIR_BOUNDS) && item.top >= 0);
	}
	images = ListView_GetImageList(tools,LVSIL_SMALL);
	REQUIRE(images && images == ListView_GetImageList(tray,LVSIL_SMALL));
	REQUIRE(ImageList_GetIconSize(images,&cx,&cy) && cx == 20 && cy == 20);
	REQUIRE(ImageList_GetImageCount(images) == 34);
	Check(editor->NewDocument(TRUE));
	// Native keyboard selection must insert only once despite resetting Pointer.
	SetFocus(tools);
	SendMessageW(tools,WM_KEYDOWN,VK_END,0);
	Check(editor->GetSelectedID(&first));
	REQUIRE(ListView_GetItemCount(tray) == 1);
	{
		// Completed-click notification. Native mouse tracking pumps input while
		// pressed, so a synchronous fake WM_LBUTTONDOWN/UP pair is not a click.
		NMITEMACTIVATE click; ZeroMemory(&click,sizeof(click));
		click.hdr.hwndFrom = tools; click.hdr.idFrom = 102; click.hdr.code = NM_CLICK;
		click.iItem = FindTool(tools,L"Timer");
		SendMessageW(editor->Window(),WM_NOTIFY,102,reinterpret_cast<LPARAM>(&click));
	}
	Check(editor->GetSelectedID(&second));
	if(IsEqualGUID(first,second) || ListView_GetItemCount(tray) != 2 || ListView_GetNextItem(tools,-1,LVNI_SELECTED) != 0)
	{
		CRString diagnostic;
		Check(RStrFormatW(&diagnostic,L"Palette click: same=%u count=%u selected=%d focused=%d\r\n",IsEqualGUID(first,second),ListView_GetItemCount(tray),ListView_GetNextItem(tools,-1,LVNI_SELECTED),ListView_GetNextItem(tools,-1,LVNI_FOCUSED)));
		Log(diagnostic.c_wstr());
	}
	REQUIRE(!IsEqualGUID(first,second) && ListView_GetItemCount(tray) == 2 && ListView_GetNextItem(tools,-1,LVNI_SELECTED) == 0);
	firstImage = PaletteRowImage(tools,FindTool(tools,L"Timer"));
	REQUIRE(firstImage >= 4 && PaletteRowImage(tray,0) == firstImage && PaletteRowImage(tray,1) == firstImage && ImageList_GetImageCount(images) == 34);
	ListView_SetItemState(tray,0,LVIS_SELECTED | LVIS_FOCUSED,LVIS_SELECTED | LVIS_FOCUSED);
	Check(editor->GetSelectedID(&selected));
	REQUIRE(IsEqualGUID(first,selected));
	Check(editor->Document().FindComponent(first,&selectedComponent));
	{ TStackRef<IQuadooProperties> properties; TStackRef<IJSONValue> value; UINT index;
		Check(QFD::Property(selectedComponent,L"Name",&properties,&index));
		Check(JSONCreateString(RSTRING_CAST(L"RenamedTimer"),&value));
		Check(editor->SetSelectedProperty(index,value));
	}
	CheckIf(PumpPendingMessages(),E_ABORT);
	ListView_GetItemText(tray,0,0,name,ARRAYSIZE(name));
	REQUIRE(!lstrcmpW(name,L"RenamedTimer") && PaletteRowImage(tray,0) == firstImage);
	Check(editor->Document().Serialize(&before));
	for(UINT pass = 0; pass < 8; pass++)
	{
		Check(editor->SelectID(pass & 1 ? first : second));
		REQUIRE(ImageList_GetImageCount(images) == 34);
	}
	editor->SetZoom(150);
	REQUIRE(ListView_GetImageList(tools,LVSIL_SMALL) == images);
	editor->SetDPI(144);
	images = ListView_GetImageList(tools,LVSIL_SMALL);
	REQUIRE(images == ListView_GetImageList(tray,LVSIL_SMALL) && ImageList_GetIconSize(images,&cx,&cy) && cx == 30 && cy == 30);
	Check(editor->Document().Serialize(&after));
	REQUIRE(JSONCompareObjects(before,after) == 0);
	after.Release(); before.Release(); selectedComponent.Release();
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"Missing\",\"components\":[{\"type\":\"UnavailableService\",\"name\":\"Lost\"}]}",&missing));
	Check(editor->LoadDefinition(missing));
	REQUIRE(ListView_GetItemCount(tray) == 1 && PaletteRowImage(tray,0) == 3);
	Check(editor->NewDocument(FALSE));
	REQUIRE(!ListView_GetItemCount(tray));
	DestroyWindow(host); host = NULL; editor.Release();

	// The same contract loads resources from an unrelated extension DLL.
	Check(QuadooGetControlRegistry(&registry));
	Check(registry->QueryInterface(__uuidof(IQuadooComponentRegistry),reinterpret_cast<PVOID*>(&components)));
	Check(CreateExternalFactory(&visual));
	Check(CreateExternalComponentFactory(&service));
	Check(VerifyFactoryIcon(visual,__uuidof(IQuadooControlFactory)));
	Check(VerifyFactoryIcon(service,__uuidof(IQuadooComponentFactory)));
	Check(visual->QueryInterface(__uuidof(IExternalPaletteTest),reinterpret_cast<PVOID*>(&visualTest)));
	Check(service->QueryInterface(__uuidof(IExternalPaletteTest),reinterpret_cast<PVOID*>(&serviceTest)));
	Check(registry->RegisterFactory(visual));
	Check(components->RegisterComponentFactory(service));
	host = Host();
	CheckIf(!host,HrEnsureLastError());
	Check(CreateEditor(host,&editor,registry));
	Check(editor->NewDocument(TRUE));
	tools = GetDlgItem(editor->Window(),102); tray = editor->TrayWindow();
	REQUIRE(ListView_GetItemCount(tools) == 33 && PaletteRowImage(tools,FindTool(tools,L"ExternalMeter")) >= 4);
	Check(PickTool(editor,L"ExternalService"));
	REQUIRE(ListView_GetItemCount(tray) == 1 && PaletteRowImage(tray,0) == PaletteRowImage(tools,FindTool(tools,L"ExternalService")));
	Check(TestComponents(editor->Document().Root(),&collection));
	Check(collection->GetCount(&count));
	REQUIRE(count == 1 && ExternalStatistics().objects == 1);
	Check(visualTest->GetIconRequests(&calls));
	Check(editor->GetSelectedID(&selected));
	Check(editor->SelectID(selected));
	Check(visualTest->GetIconRequests(&currentCalls));
	REQUIRE(calls == currentCalls);
	Check(editor->Document().Serialize(&before));
	images = ListView_GetImageList(tools,LVSIL_SMALL); imageCount = ImageList_GetImageCount(images);
	Check(visualTest->SetIconResult(E_OUTOFMEMORY));
	editor->SetDPI(144);
	REQUIRE(ListView_GetImageList(tools,LVSIL_SMALL) == images && ListView_GetImageList(tray,LVSIL_SMALL) == images && ImageList_GetImageCount(images) == imageCount);
	Check(editor->GetSelectedID(&first));
	REQUIRE(IsEqualGUID(first,selected));
	Check(editor->Document().Serialize(&after));
	REQUIRE(JSONCompareObjects(before,after) == 0);
	Check(visualTest->SetIconResult(2)); // Invalid S_OK/NULL provider result.
	Check(serviceTest->SetIconResult(S_FALSE));
	editor->SetDPI(144);
	REQUIRE(PaletteRowImage(tools,FindTool(tools,L"ExternalMeter")) == 1 && PaletteRowImage(tray,0) == 2);
	Check(visualTest->SetIconResult(E_ABORT));
	editor->SetDPI(192);
	REQUIRE(PaletteRowImage(tools,FindTool(tools,L"ExternalMeter")) == 1 && RStrLen(editor->Status()));
	Check(visualTest->SetIconResult(S_OK));
	Check(serviceTest->SetIconResult(S_OK));
	editor->SetDPI(96);
	REQUIRE(PaletteRowImage(tools,FindTool(tools,L"ExternalMeter")) >= 4 && PaletteRowImage(tray,0) >= 4);
	// One list-view window can disappear without destroying the other's images.
	images = ListView_GetImageList(tray,LVSIL_SMALL);
	REQUIRE(DestroyWindow(tools) && ImageList_GetImageCount(images) == 36);
	Check(editor->SelectID(selected));
	Check(editor->Delete());
	REQUIRE(!ListView_GetItemCount(tray));
	collection.Release();
Cleanup:
	if(host) DestroyWindow(host);
	editor.Release(); visualTest.Release(); serviceTest.Release(); visual.Release(); service.Release(); components.Release(); registry.Release();
	if(SUCCEEDED(hr)) hr = UnloadExternalExtension();
	return hr;
}
static HRESULT PaletteResources()
{
	HRESULT hr;
	CGuiResourceMonitor resources(L"Palette images and factories");
	while(!resources.Complete())
	{
		HWND host = NULL;
		TStackRef<QFD::CDesignerEditor> editor;
		TStackRef<IQuadooControlRegistry> registry;
		TStackRef<IQuadooComponentRegistry> components;
		TStackRef<IQuadooControlFactory> visual;
		TStackRef<IQuadooComponentFactory> service;
		HICON copied = NULL;
		Check(QuadooGetControlRegistry(&registry));
		Check(registry->QueryInterface(__uuidof(IQuadooComponentRegistry),reinterpret_cast<PVOID*>(&components)));
		Check(CreateExternalFactory(&visual));
		Check(CreateExternalComponentFactory(&service));
		Check(registry->RegisterFactory(visual));
		Check(components->RegisterComponentFactory(service));
		host = Host();
		CheckIf(!host,HrEnsureLastError());
		hr = CreateEditor(host,&editor,registry);
		if(SUCCEEDED(hr)) hr = editor->NewDocument(TRUE);
		if(SUCCEEDED(hr))
		{
			HWND tools = GetDlgItem(editor->Window(),102);
			for(UINT pass = 0; pass < 4; pass++) editor->SetDPI(pass & 1 ? 96 : 144);
			copied = ImageList_GetIcon(ListView_GetImageList(tools,LVSIL_SMALL),PaletteRowImage(tools,FindTool(tools,L"ExternalMeter")),ILD_NORMAL);
			hr = copied ? S_OK : E_FAIL;
		}
		if(SUCCEEDED(hr)) hr = registry->UnregisterFactory(RSTRING_CAST(L"ExternalMeter"));
		if(SUCCEEDED(hr)) hr = components->UnregisterComponentFactory(RSTRING_CAST(L"ExternalService"));
		visual.Release(); service.Release();
		if(SUCCEEDED(hr) && ExternalCanUnload() != S_FALSE) hr = E_FAIL; // The live palette retains both factories.
		if(host) DestroyWindow(host);
		// The C++ editor can remain retained after its window closes.
		if(SUCCEEDED(hr) && ExternalCanUnload() != S_OK) hr = E_FAIL;
		editor.Release(); components.Release(); registry.Release();
		if(SUCCEEDED(hr)) hr = UnloadExternalExtension();
		BOOL valid = PaletteIconSize(copied,20);
		if(copied) DestroyIcon(copied);
		Check(hr);
		REQUIRE(valid);
		Check(resources.AfterCycle());
	}
Cleanup:
	return hr;
}
static HRESULT PumpPaletteInput()
{
	HRESULT hr;
	DWORD start = GetTickCount(), delay = GetDoubleClickTime() + 100;
	// The list view can defer revealing its focused row until a timer fires.
	// Draining only the messages currently queued misses that scrolling.
	for(;;)
	{
		CheckIf(PumpPendingMessages(),E_ABORT);
		DWORD elapsed = GetTickCount() - start;
		if(elapsed >= delay) break;
		MsgWaitForMultipleObjects(0,NULL,FALSE,delay - elapsed,QS_ALLINPUT);
	}
	hr = S_OK;
Cleanup:
	return hr;
}
static HRESULT PaletteScrollPreserved(BOOL form, UINT dpi)
{
	HRESULT hr;
	HWND host = Host(), tools, rootWindow;
	TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IQuadooControlContainer> root;
	TStackRef<IQuadooChildWindowHost> native;
	UINT children;
	CheckIf(!host,HrEnsureLastError());
	Check(CreateEditor(host,&editor));
	// Force scrolling even with the small standard palette at 96 DPI.
	editor->SetLayout(190,300,480);
	editor->SetDPI(dpi);
	Check(editor->NewDocument(form));
	Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root)));
	Check(root->QueryInterface(__uuidof(IQuadooChildWindowHost),reinterpret_cast<PVOID*>(&native)));
	Check(native->GetChildHostHandle(&rootWindow));
	tools = GetDlgItem(editor->Window(),102);
	for(UINT pass = 0; pass < 2; pass++)
	{
		RECT before, after;
		INT top, scroll;
		POINT point = { 24 + static_cast<INT>(pass) * 35,40 + static_cast<INT>(pass) * 32 };
		GUID selected;
		TStackRef<IQuadooControl> control;
		CRString type;
		Check(PickTool(editor,L"TextBox"));
		REQUIRE(ListView_EnsureVisible(tools,FindTool(tools,L"TextBox"),FALSE));
		REQUIRE(ListView_GetItemRect(tools,0,&before,LVIR_BOUNDS));
		top = ListView_GetTopIndex(tools); scroll = GetScrollPos(tools,SB_VERT);
		REQUIRE(top > 0 && before.top < 0);
		MapWindowPoints(rootWindow,editor->CanvasWindow(),&point,1);
		SendMessageW(editor->OverlayWindow(),WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(point.x,point.y));
		SendMessageW(editor->OverlayWindow(),WM_LBUTTONUP,0,MAKELPARAM(point.x,point.y));
		CheckIf(PumpPendingMessages(),E_ABORT);
		Check(root->GetChildCount(&children));
		REQUIRE(children == pass + 1 && ListView_GetNextItem(tools,-1,LVNI_SELECTED) == 0);
		Check(editor->GetSelectedID(&selected));
		Check(editor->Document().Find(selected,&control));
		Check(control->GetTypeName(&type));
		REQUIRE(type == RSTRING_CAST(L"TextBox"));
		REQUIRE(ListView_GetItemRect(tools,0,&after,LVIR_BOUNDS));
		REQUIRE(top == ListView_GetTopIndex(tools) && scroll == GetScrollPos(tools,SB_VERT) && before.top == after.top);
	}
	PCWSTR components[] = { L"OpenFile",L"SaveFile",L"Timer",L"Timer",L"SaveFile",L"Timer" };
	for(UINT pass = 0; pass < ARRAYSIZE(components); pass++)
	{
		RECT before, after;
		INT top, scroll;
		GUID selected;
		TStackRef<IQuadooComponent> component;
		CRString type;
		INT row = FindTool(tools,components[pass]);
		REQUIRE(row >= 0 && ListView_EnsureVisible(tools,row,FALSE));
		SetFocus(tools);
		REQUIRE(GetFocus() == tools);
		REQUIRE(ListView_GetItemRect(tools,0,&before,LVIR_BOUNDS));
		top = ListView_GetTopIndex(tools); scroll = GetScrollPos(tools,SB_VERT);
		REQUIRE(top > 0 && before.top < 0);
		if(pass < 4)
		{
			RECT item;
			REQUIRE(ListView_GetItemRect(tools,row,&item,LVIR_BOUNDS));
			LPARAM point = MAKELPARAM((item.left + item.right) / 2,(item.top + item.bottom) / 2);
			REQUIRE(PostMessageW(tools,WM_LBUTTONDOWN,MK_LBUTTON,point));
			REQUIRE(PostMessageW(tools,WM_LBUTTONUP,0,point));
		}
		else
			REQUIRE(PostMessageW(tools,WM_KEYDOWN,pass == 4 ? VK_UP : VK_DOWN,0));
		Check(PumpPaletteInput());
		REQUIRE(ListView_GetItemCount(editor->TrayWindow()) == static_cast<INT>(pass + 1) && ListView_GetNextItem(tools,-1,LVNI_SELECTED) == 0);
		REQUIRE(GetFocus() == tools && ListView_GetNextItem(tools,-1,LVNI_FOCUSED) == row);
		Check(editor->GetSelectedID(&selected));
		Check(editor->Document().FindComponent(selected,&component));
		Check(component->GetTypeName(&type));
		REQUIRE(!lstrcmpW(type.c_wstr(),components[pass]));
		REQUIRE(ListView_GetItemRect(tools,0,&after,LVIR_BOUNDS));
		REQUIRE(top == ListView_GetTopIndex(tools) && scroll == GetScrollPos(tools,SB_VERT) && before.top == after.top);
	}
Cleanup:
	if(host) DestroyWindow(host);
	editor.Release();
	return hr;
}
static HRESULT PaletteTests()
{
	HRESULT hr;
	for(UINT dpi = 96; dpi <= 144; dpi += 48)
	{
		Check(PaletteScrollPreserved(TRUE,dpi));
		Check(PaletteScrollPreserved(FALSE,dpi));
	}
	Check(PaletteFactoryIcons());
	Check(PaletteViews());
	Check(PaletteResources());
Cleanup:
	return hr;
}
