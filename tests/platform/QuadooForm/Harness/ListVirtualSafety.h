static HWND s_virtualWindow;
static IQuadooForm* s_virtualModal;
static BOOL s_virtualModalBlank;

static VOID CALLBACK VirtualModalTimer(HWND window, UINT, UINT_PTR id, DWORD)
{
	KillTimer(window,id);
	WCHAR text[16] = L"unsafe";
	ListView_GetItemText(s_virtualWindow,1,0,text,ARRAYSIZE(text));
	s_virtualModalBlank = !text[0];
	s_virtualModal->Close();
}

static HRESULT ListVirtualSafety()
{
	HRESULT hr;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IJSONObject> definition;
	TStackRef<IQuadooControl> root;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooControlContainer> container;
	TStackRef<IQuadooListView> view;
	TStackRef<IQuadooListItems> items;
	TStackRef<IQuadooListItem> item;
	TStackRef<IQuadooWinControl> native;
	TStackRef<IQuadooForm> form;
	TStackRef<CVirtualListBinder> binder;
	CRString text;
	UINT count;
	INT image;
	HWND window;
	HWND designHost = NULL;
	QuadooForm::ContextOptions options = {0};
	options.cbSize = sizeof(options);
	options.DPI = 96;
	for(UINT mode = 0; mode < 3; mode++)
	{
		options.Mode = static_cast<QuadooForm::ContextMode>(mode);
		if(options.Mode == QuadooForm::Design)
		{
			designHost = CreateWindowExW(0,L"STATIC",L"",WS_OVERLAPPED,0,0,640,480,NULL,NULL,GetModuleHandleW(NULL),NULL);
			CheckIf(!designHost,HrEnsureLastError());
			options.hwndHost = designHost;
		}
		Check(QuadooCreateConstructionContext(&options,&context));
		Check(Parse(L"{\"type\":\"Form\",\"children\":[{\"type\":\"ListView\",\"name\":\"VirtualList\",\"properties\":{\"OwnerData\":true,\"CheckBoxes\":true}}]}",&definition));
		Check(QuadooCreateControl(context,NULL,definition,&root));
		Check(root->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&container)));
		Check(container->FindControl(RSTRING_CAST(L"VirtualList"),&control));
		Check(control->QueryInterface(__uuidof(IQuadooListView),reinterpret_cast<PVOID*>(&view)));
		Check(view->GetItems(&items));
		Check(items->GetCount(&count));
		REQUIRE(count == 0);
		if(options.Mode == QuadooForm::Design)
		{
			REQUIRE(items->SetCount(4) == QuadooForm::EInvalidState);
		}
		else
		{
			Check(items->SetCount(4));
			Check(items->GetAt(1,&item));
			Check(item->GetCaption(&text));
			Check(item->GetImageIndex(&image));
			REQUIRE(!RStrLen(text) && image == -1);
			REQUIRE(view->BeginLabelEdit(item) == QuadooForm::EInvalidState);
			REQUIRE(view->UpdateVirtualText(1,0,RSTRING_CAST(L"ignored")) == S_FALSE);
			item.Release();
			text = NULL;
		}
		Check(root->Destroy());
		root.Release();
		control.Release();
		container.Release();
		items.Release();
		view.Release();
		context.Release();
		definition.Release();
		if(designHost)
		{
			DestroyWindow(designHost);
			designHost = NULL;
			options.hwndHost = NULL;
		}
	}
	binder.Attach(__new CVirtualListBinder);
	CheckAlloc(binder);
	options.Mode = QuadooForm::Runtime;
	options.pBinder = binder;
	options.pReport = binder;
	Check(QuadooCreateConstructionContext(&options,&context));
	Check(Parse(L"{\"type\":\"Form\",\"children\":[{\"type\":\"ListView\",\"name\":\"VirtualList\",\"properties\":{\"OwnerData\":true},\"events\":{\"OnData\":\"E\"}}]}",&definition));
	Check(QuadooCreateControl(context,NULL,definition,&root));
	Check(root->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&container)));
	Check(container->FindControl(RSTRING_CAST(L"VirtualList"),&control));
	Check(control->QueryInterface(__uuidof(IQuadooListView),reinterpret_cast<PVOID*>(&view)));
	Check(view->GetItems(&items));
	Check(items->SetCount(4));
	binder->fail = TRUE;
	Check(view->SetItemSelected(1,TRUE));
	REQUIRE(binder->reads == 0);
	binder->fail = FALSE;
	Check(root->Destroy());
	root.Release();
	control.Release();
	container.Release();
	items.Release();
	view.Release();
	definition.Release();
	Check(Parse(VirtualListDefinition,&definition));
	Check(QuadooCreateControl(context,NULL,definition,&root));
	Check(root->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&container)));
	Check(container->FindControl(RSTRING_CAST(L"VirtualList"),&control));
	Check(control->QueryInterface(__uuidof(IQuadooListView),reinterpret_cast<PVOID*>(&view)));
	Check(control->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native)));
	Check(root->QueryInterface(__uuidof(IQuadooForm),reinterpret_cast<PVOID*>(&form)));
	Check(view->GetItems(&items));
	binder->view = view;
	Check(items->SetCount(4));
	Check(native->GetHandle(&s_virtualWindow));
	Check(form->GetHandle(&window));
	{
		TStackRef<IQuadooProperties> properties;
		UINT index;
		Check(Property(form,L"ModalResult",&properties,&index));
		Check(properties->SetValueLong(index,7));
	}
	s_virtualModal = form;
	s_virtualModalBlank = FALSE;
	binder->fail = TRUE;
	REQUIRE(SetTimer(window,441,10,VirtualModalTimer) != 0);
	LRESULT result;
	Check(form->ShowModal(&result));
	REQUIRE(result == 7 && s_virtualModalBlank && binder->reports > 0);
	form.Release();
	native.Release();
	Check(root->Destroy());
	root.Release();
	control.Release();
	container.Release();
	items.Release();
	view.Release();
	binder->fail = FALSE;
	Check(QuadooCreateControl(context,NULL,definition,&root));
	Check(root->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&container)));
	Check(container->FindControl(RSTRING_CAST(L"VirtualList"),&control));
	Check(control->QueryInterface(__uuidof(IQuadooListView),reinterpret_cast<PVOID*>(&view)));
	Check(view->GetItems(&items));
	binder->view = view;
	Check(items->SetCount(4));
	binder->destroy = TRUE;
	REQUIRE(items->GetAt(1,&item) == QuadooForm::EDestroyed && !item);
	BOOL attached;
	Check(binder->retained->GetAttached(&attached));
	REQUIRE(!attached);
	Check(binder->retained->GetCaption(&text));
	REQUIRE(text == RSTRING_CAST(L"Row 1"));
	hr = S_OK;
Cleanup:
	s_virtualModal = NULL;
	if(root)
	{
		root->Destroy();
	}
	if(designHost)
	{
		DestroyWindow(designHost);
	}
	return hr;
}

static HRESULT ListVirtualResources()
{
	HRESULT hr;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooControlFactory> factory;
	TStackRef<IQuadooFactoryLifetime> lifetime;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONObject> child;
	TStackRef<IJSONValue> value;
	CGuiResourceMonitor monitor(L"OwnerData derived checkbox images and retained snapshots");
	Check(QuadooGetControlRegistry(&registry));
	Check(registry->FindFactory(RSTRING_CAST(L"ListView"),&factory));
	Check(factory->QueryInterface(__uuidof(IQuadooFactoryLifetime),reinterpret_cast<PVOID*>(&lifetime)));
	Check(Parse(ImageListNativeDefinition,&definition));
	Check(Parse(L"{\"children\":[{\"type\":\"ListView\",\"name\":\"VirtualList\",\"properties\":{\"OwnerData\":true,\"CheckBoxes\":true,\"SmallImages\":\"{A9000000-0000-0000-0000-000000000001}\",\"LargeImages\":\"{A9000000-0000-0000-0000-000000000001}\"}}]}",&child));
	Check(child->FindNonNullValueW(L"children",&value));
	Check(definition->AddValueW(L"children",value));
	while(!monitor.Complete())
	{
		TStackRef<IQuadooListItem> retained;
		UINT live;
		{
			TStackRef<IQuadooConstructionContext> context;
			TStackRef<IQuadooControl> root;
			TStackRef<IQuadooControl> control;
			TStackRef<IQuadooControlContainer> container;
			TStackRef<IQuadooListView> view;
			TStackRef<IQuadooListItems> items;
			TStackRef<IQuadooLayoutControl> layout;
			QuadooForm::Rect bounds;
			Check(Context(96,NULL,&context,registry,QuadooForm::Runtime));
			Check(QuadooCreateControl(context,NULL,definition,&root));
			Check(root->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&container)));
			Check(container->FindControl(RSTRING_CAST(L"VirtualList"),&control));
			Check(control->QueryInterface(__uuidof(IQuadooListView),reinterpret_cast<PVOID*>(&view)));
			Check(control->QueryInterface(__uuidof(IQuadooLayoutControl),reinterpret_cast<PVOID*>(&layout)));
			Check(view->GetItems(&items));
			Check(items->SetCount(100));
			Check(items->GetAt(5,&retained));
			Check(control->GetBounds(&bounds));
			for(UINT mode = 0; mode < 4; mode++)
			{
				Check(SetInt(control,L"ViewStyle",mode));
				Check(layout->Arrange(&bounds,mode % 2 ? 144 : 192));
			}
			Check(root->Destroy());
		}
		Check(lifetime->GetLiveCount(&live));
		REQUIRE(live > 0);
		REQUIRE(registry->UnregisterFactory(RSTRING_CAST(L"ListView")) == HRESULT_FROM_WIN32(ERROR_BUSY));
		retained.Release();
		Check(lifetime->GetLiveCount(&live));
		REQUIRE(live == 0);
		Check(monitor.AfterCycle());
	}
	hr = S_OK;
Cleanup:
	return hr;
}
