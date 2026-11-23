class CListTestBinder : public CBaseUnknown, public IQuadooEventsBinder, public IQuadooDelegate
{
public:
	UINT clicks;
	UINT popups;
	INT column;
	POINT point;
	BOOL handle;
	BOOL destroy;
	BOOL reenter;
	HRESULT nested;
	IQuadooListColumns* columns;
	TStackRef<IQuadooListColumn> clicked;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooEventsBinder)
		UNK_INTERFACE(IQuadooDelegate)
	END_UNK_MAP
	CListTestBinder() : clicks(0), popups(0), column(-1), handle(TRUE), destroy(FALSE),
		reenter(FALSE), nested(S_OK), columns(NULL)
	{
		point.x = 0;
		point.y = 0;
	}
	STDMETHOD(BindEvent)(RSTRING name, IQuadooDelegate** value)
	{
		HRESULT hr = S_OK;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		CheckIf(!(CRString(name) == RSTRING_CAST(L"Column")) && !(CRString(name) == RSTRING_CAST(L"Context")),QuadooForm::EHandlerNotFound);
		*value = this;
		AddRef();
	Cleanup:
		return hr;
	}
	STDMETHOD(PostBind)(IQuadooControl* control)
	{
		return S_OK;
	}
	STDMETHOD(Invoke)(IQuadooEventArgs* args)
	{
		HRESULT hr;
		CRString name;
		TStackRef<IQuadooEventObjectArgs> objects;
		TStackRef<IUnknown> object;
		Check(args->GetEventName(&name));
		Check(args->QueryInterface(__uuidof(IQuadooEventObjectArgs),reinterpret_cast<PVOID*>(&objects)));
		Check(objects->GetObject(0,&object));
		if(name == RSTRING_CAST(L"OnColumnClick"))
		{
			clicks++;
			clicked.Release();
			Check(object->QueryInterface(__uuidof(IQuadooListColumn),reinterpret_cast<PVOID*>(&clicked)));
			Check(args->GetInt(1,&column));
		}
		else
		{
			popups++;
			REQUIRE(!object);
			Check(args->GetInt(1,&column));
			INT coordinate;
			Check(args->GetInt(2,&coordinate));
			point.x = coordinate;
			Check(args->GetInt(3,&coordinate));
			point.y = coordinate;
			Check(args->SetBool(4,handle));
		}
		if(reenter)
		{
			nested = columns->Clear();
		}
		if(destroy)
		{
			TStackRef<IQuadooComponent> sender;
			Check(args->GetSender(&sender));
			Check(sender->Destroy());
		}
		hr = S_OK;
	Cleanup:
		return hr;
	}
};

static const WCHAR ListViewChildDefinition[] =
	L"{\"type\":\"ListView\",\"name\":\"List1\",\"properties\":{\"Width\":360,\"Height\":180,"
	L"\"SmallImages\":\"{A9000000-0000-0000-0000-000000000001}\","
	L"\"LargeImages\":\"{A9000000-0000-0000-0000-000000000001}\","
	L"\"StateImages\":\"{A9000000-0000-0000-0000-000000000001}\","
	L"\"Columns\":[{\"Caption\":\"Name\",\"Width\":100},{\"Caption\":\"Size\",\"Width\":80},"
	L"{\"Caption\":\"Description\",\"Width\":120}]},\"events\":{\"OnColumnClick\":\"Column\",\"OnContextPopup\":\"Context\"}}";

static HRESULT ListViewRoot(IJSONObject** result)
{
	HRESULT hr;
	TStackRef<IJSONObject> root;
	TStackRef<IJSONObject> child;
	TStackRef<IJSONArray> children;
	TStackRef<IJSONValue> wrapped;
	Check(Parse(ImageListNativeDefinition,&root));
	Check(Parse(ListViewChildDefinition,&child));
	Check(JSONCreateArray(&children));
	Check(JSONWrapObject(child,&wrapped));
	Check(children->Add(wrapped));
	wrapped.Release();
	Check(JSONWrapArray(children,&wrapped));
	Check(root->AddValueW(L"children",wrapped));
	*result = root.Detach();
	Cleanup:
	return hr;
}

static HRESULT ListViewNative()
{
	HRESULT hr;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONObject> serialized;
	TStackRef<IJSONObject> repeated;
	TStackRef<IQuadooControl> root;
	TStackRef<IQuadooControl> copy;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooControl> secondControl;
	TStackRef<IQuadooControlContainer> container;
	TStackRef<IQuadooListView> view;
	TStackRef<IQuadooListView> secondView;
	TStackRef<IQuadooWinControl> native;
	TStackRef<IQuadooListColumns> columns;
	TStackRef<IQuadooListColumn> first;
	TStackRef<IQuadooListColumn> second;
	TStackRef<IQuadooListColumn> third;
	TStackRef<IQuadooListColumn> added;
	TStackRef<IQuadooListColumn> found;
	TStackRef<IQuadooComponentHost> host;
	TStackRef<IQuadooComponentCollection> components;
	TStackRef<IQuadooComponent> images;
	TStackRef<IQuadooImageList> imageList;
	TStackRef<IQuadooImageListSnapshot> snapshot;
	TStackRef<IQuadooImageListSnapshot> scaled;
	TStackRef<IQuadooImageListSnapshot> rebuilt;
	TStackRef<IQuadooLayoutControl> layout;
	TStackRef<IQuadooFormSerializer> serializer;
	TStackRef<IQuadooProperties> properties;
	TStackRef<CListTestBinder> binder;
	GUID stable;
	UINT count;
	UINT index;
	INT number;
	BOOL attached;
	HWND window;
	HIMAGELIST handle;
	HIMAGELIST replacement;
	QuadooForm::Rect bounds;
	binder.Attach(__new CListTestBinder);
	CheckAlloc(binder);
	Check(QuadooGetControlRegistry(&registry));
	Check(Context(96,binder,&context,registry));
	Check(ListViewRoot(&definition));
	Check(QuadooCreateControl(context,NULL,definition,&root));
	Check(root->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&container)));
	Check(container->FindControl(RSTRING_CAST(L"List1"),&control));
	Check(control->QueryInterface(__uuidof(IQuadooListView),reinterpret_cast<PVOID*>(&view)));
	Check(control->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native)));
	Check(view->GetColumns(&columns));
	binder->columns = columns;
	Check(columns->GetCount(&count));
	REQUIRE(count == 3);
	Check(columns->GetAt(0,&first));
	Check(columns->GetAt(1,&second));
	Check(columns->GetAt(2,&third));
	Check(first->GetID(&stable));
	Check(native->GetHandle(&window));
	REQUIRE((GetWindowLongW(window,GWL_STYLE) & LVS_TYPEMASK) == LVS_REPORT);
	REQUIRE(GetWindowLongW(window,GWL_STYLE) & LVS_SHAREIMAGELISTS);
	REQUIRE(GetWindowLongW(window,GWL_STYLE) & LVS_EDITLABELS);
	REQUIRE(ListView_GetExtendedListViewStyle(window) & LVS_EX_FULLROWSELECT);
	REQUIRE(Header_GetItemCount(ListView_GetHeader(window)) == 3 && ListView_GetItemCount(window) == 0);
	{
		HWND header = ListView_GetHeader(window);
		RECT bounds;
		REQUIRE(GetWindowLongW(header,GWL_STYLE) & WS_VISIBLE);
		REQUIRE(GetWindowRect(header,&bounds) && bounds.bottom > bounds.top && bounds.right > bounds.left);
	}
	{
		TStackRef<IUnknown> expected;
		TStackRef<IUnknown> actual;
		TStackRef<IQuadooListItems> unavailable;
		Check(control->QueryInterface(__uuidof(IUnknown),reinterpret_cast<PVOID*>(&expected)));
		Check(view->QueryInterface(__uuidof(IUnknown),reinterpret_cast<PVOID*>(&actual)));
		REQUIRE(expected == static_cast<IUnknown*>(actual));
		actual.Release();
		Check(native->QueryInterface(__uuidof(IUnknown),reinterpret_cast<PVOID*>(&actual)));
		REQUIRE(expected == static_cast<IUnknown*>(actual));
		Check(view->GetItems(&unavailable));
		REQUIRE(unavailable != NULL);
		REQUIRE(columns->GetAt(3,&found) == DISP_E_BADINDEX && !found);
		REQUIRE(columns->GetAt(0,NULL) == E_POINTER);
		REQUIRE(first->SetAlignment(QuadooForm::ListColumnCenter) == E_INVALIDARG);
		REQUIRE(first->SetWidth(-1) == E_INVALIDARG);
		REQUIRE(first->SetDisplayIndex(3) == E_INVALIDARG);
		Check(Property(control,L"Items",&properties,&index));
		TStackRef<IQuadooPropertyDescriptors> descriptors;
		TStackRef<IQuadooPropertyDescriptor> descriptor;
		DWORD flags;
		Check(properties->GetDescriptors(&descriptors));
		Check(descriptors->GetAt(index,&descriptor));
		Check(descriptor->GetFlags(&flags));
		REQUIRE((flags & (QuadooForm::PropertyHidden | QuadooForm::PropertySerializable)) ==
			(QuadooForm::PropertyHidden | QuadooForm::PropertySerializable));
	}
	Check(root->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&host)));
	Check(host->GetComponents(&components));
	Check(components->FindComponent(RSTRING_CAST(L"ImageList1"),&images));
	Check(images->QueryInterface(__uuidof(IQuadooImageList),reinterpret_cast<PVOID*>(&imageList)));
	Check(imageList->Acquire(96,&snapshot));
	Check(snapshot->GetHandle(&handle));
	for(UINT role = 0; role < 3; role++)
	{
		TStackRef<IQuadooComponent> assigned;
		static const INT roles[] = {LVSIL_NORMAL,LVSIL_SMALL,LVSIL_STATE};
		Check(view->GetImages(static_cast<QuadooForm::ListImageRole>(role),&assigned));
		REQUIRE(assigned == static_cast<IQuadooComponent*>(images));
		REQUIRE(ListView_GetImageList(window,roles[role]) == handle);
	}
	REQUIRE(MenuFlag(control,L"CheckBoxes",TRUE) == E_INVALIDARG);
	Check(control->QueryInterface(__uuidof(IQuadooLayoutControl),reinterpret_cast<PVOID*>(&layout)));
	Check(control->GetBounds(&bounds));
	for(UINT dpi = 144; dpi <= 192; dpi += 48)
	{
		Check(layout->Arrange(&bounds,dpi));
		Check(native->GetHandle(&window));
		Check(imageList->Acquire(dpi,&scaled));
		Check(scaled->GetHandle(&replacement));
		REQUIRE(ListView_GetImageList(window,LVSIL_SMALL) == replacement);
		REQUIRE(ListView_GetColumnWidth(window,0) == MulDiv(100,dpi,96));
		Check(first->GetWidth(&number));
		REQUIRE(number == 100);
		scaled.Release();
	}
	Check(layout->Arrange(&bounds,96));
	Check(native->GetHandle(&window));
	Check(SetInt(images,L"Width",20));
	Check(imageList->Acquire(96,&rebuilt));
	Check(rebuilt->GetHandle(&replacement));
	REQUIRE(replacement != handle && ImageList_GetImageCount(handle) == 2);
	REQUIRE(ListView_GetImageList(window,LVSIL_NORMAL) == replacement && ListView_GetImageList(window,LVSIL_STATE) == replacement);
	{
		CImageTestCanvas canvas;
		Check(canvas.Create(40,40));
		canvas.Fill(RGB(17,33,49));
		DWORD empty = canvas.Hash();
		REQUIRE(ImageList_Draw(replacement,0,canvas.DC(),0,0,ILD_TRANSPARENT));
		REQUIRE(canvas.Hash() != empty);
	}
	definition.Release();
	Check(Parse(L"{\"type\":\"ListView\",\"name\":\"List2\"}",&definition));
	Check(QuadooCreateControl(context,container,definition,&secondControl));
	Check(secondControl->QueryInterface(__uuidof(IQuadooListView),reinterpret_cast<PVOID*>(&secondView)));
	Check(secondView->SetImages(QuadooForm::ListSmallImages,images));
	Check(view->SetImages(QuadooForm::ListStateImages,NULL));
	Check(MenuFlag(control,L"CheckBoxes",TRUE));
	Check(native->GetHandle(&window));
	REQUIRE(ListView_GetImageList(window,LVSIL_STATE) != NULL && ListView_GetImageList(window,LVSIL_STATE) != replacement);
	REQUIRE(view->SetImages(QuadooForm::ListStateImages,images) == E_INVALIDARG);
	Check(MenuFlag(control,L"CheckBoxes",FALSE));
	Check(view->SetImages(QuadooForm::ListStateImages,images));
	{
		const QuadooForm::ListViewStyle modes[] = {QuadooForm::ListViewIcon,QuadooForm::ListViewSmallIcon,QuadooForm::ListViewList,QuadooForm::ListViewReport};
		const INT styles[] = {LVS_ICON,LVS_SMALLICON,LVS_LIST,LVS_REPORT};
		for(UINT n = 0; n < ARRAYSIZE(modes); n++)
		{
			Check(SetInt(control,L"ViewStyle",modes[n]));
			Check(native->GetHandle(&window));
			REQUIRE((GetWindowLongW(window,GWL_STYLE) & LVS_TYPEMASK) == styles[n]);
			REQUIRE(ListView_GetImageList(window,LVSIL_SMALL) == replacement);
			REQUIRE(Header_GetItemCount(ListView_GetHeader(window)) == 3);
		}
	}
	Check(MenuFlag(control,L"ReadOnly",TRUE));
	Check(MenuFlag(control,L"OwnerData",TRUE));
	Check(MenuFlag(control,L"MultiSelect",TRUE));
	Check(MenuFlag(control,L"GridLines",TRUE));
	Check(MenuFlag(control,L"ColumnReorder",TRUE));
	Check(native->GetHandle(&window));
	REQUIRE((GetWindowLongW(window,GWL_STYLE) & (LVS_OWNERDATA | LVS_SINGLESEL | LVS_EDITLABELS)) == LVS_OWNERDATA);
	REQUIRE((ListView_GetExtendedListViewStyle(window) & (LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT | LVS_EX_HEADERDRAGDROP)) ==
		(LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT | LVS_EX_HEADERDRAGDROP));
	Check(MenuFlag(control,L"OwnerData",FALSE));
	Check(view->BeginUpdate());
	Check(columns->Insert(1,RSTRING_CAST(L"Inserted"),&added));
	Check(first->SetCaption(RSTRING_CAST(L"Renamed")));
	Check(first->SetWidth(110));
	Check(view->EndUpdate());
	Check(native->GetHandle(&window));
	REQUIRE(GetWindowLongW(window,GWL_STYLE) & WS_VISIBLE);
	REQUIRE(GetWindowLongW(ListView_GetHeader(window),GWL_STYLE) & WS_VISIBLE);
	REQUIRE(view->EndUpdate() == QuadooForm::EInvalidState);
	Check(second->GetIndex(&index));
	REQUIRE(index == 2);
	Check(columns->Move(first,3));
	Check(first->GetIndex(&index));
	REQUIRE(index == 3);
	Check(columns->Find(stable,&found));
	REQUIRE(found == static_cast<IQuadooListColumn*>(first));
	found.Release();
	Check(first->SetDisplayIndex(2));
	Check(first->GetDisplayIndex(&index));
	REQUIRE(index == 2);
	Check(third->SetAlignment(QuadooForm::ListColumnRight));
	Check(columns->Delete(added));
	Check(added->GetAttached(&attached));
	REQUIRE(!attached && added->GetWidth(&number) == QuadooForm::EDestroyed);
	Check(added->GetID(&stable));
	Check(first->AutoSize(QuadooForm::ListSizeToHeader));
	Check(first->GetWidth(&number));
	REQUIRE(number > 16);
	{
		NMHEADERW notification = {0};
		TStackRef<IQuadooMessageHandler> handler;
		BOOL handled;
		LRESULT result;
		INT order[] = {2,0,1};
		Check(control->QueryInterface(__uuidof(IQuadooMessageHandler),reinterpret_cast<PVOID*>(&handler)));
		Check(native->GetHandle(&window));
		REQUIRE(ListView_SetColumnWidth(window,0,123));
		notification.hdr.hwndFrom = ListView_GetHeader(window);
		notification.hdr.code = HDN_ENDTRACKW;
		Check(handler->HandleMessage(window,WM_NOTIFY,0,reinterpret_cast<LPARAM>(&notification),&handled,&result));
		Check(second->GetWidth(&number));
		REQUIRE(number == 123);
		REQUIRE(ListView_SetColumnWidth(window,0,141));
		Check(handler->HandleMessage(window,WM_NOTIFY,0,reinterpret_cast<LPARAM>(&notification),&handled,&result));
		// A setter must compare against the synchronized width, not its stale
		// value from before the queued native header update.
		REQUIRE(second->SetWidth(123) == S_OK);
		Check(native->GetHandle(&window));
		REQUIRE(ListView_GetColumnWidth(window,0) == 123);
		REQUIRE(ListView_SetColumnOrderArray(window,3,order));
		notification.hdr.hwndFrom = ListView_GetHeader(window);
		notification.hdr.code = HDN_ENDDRAG;
		Check(handler->HandleMessage(window,WM_NOTIFY,0,reinterpret_cast<LPARAM>(&notification),&handled,&result));
		Check(first->GetDisplayIndex(&index));
		REQUIRE(index == 0);
	}
	{
		NMLISTVIEW click = {0};
		Check(native->GetHandle(&window));
		click.hdr.hwndFrom = window;
		click.hdr.code = LVN_COLUMNCLICK;
		click.iSubItem = 2;
		binder->reenter = TRUE;
		SendMessageW(GetParent(window),WM_NOTIFY,0,reinterpret_cast<LPARAM>(&click));
		REQUIRE(binder->clicks == 1 && binder->column == 2 && binder->nested == QuadooForm::EInvalidState);
		REQUIRE(binder->clicked == static_cast<IQuadooListColumn*>(first));
		SendMessageW(window,WM_CONTEXTMENU,reinterpret_cast<WPARAM>(window),MAKELPARAM(-20,-30));
		REQUIRE(binder->popups == 1 && binder->column == -1 && binder->point.x == -20 && binder->point.y == -30);
		SendMessageW(window,WM_CONTEXTMENU,reinterpret_cast<WPARAM>(ListView_GetHeader(window)),MAKELPARAM(-20,-30));
		REQUIRE(binder->popups == 1);
		SendMessageW(window,WM_CONTEXTMENU,reinterpret_cast<WPARAM>(window),-1);
		REQUIRE(binder->popups == 2);
		binder->reenter = FALSE;
		binder->clicked.Release();
	}
	Check(QuadooGetFormSerializer(&serializer));
	Check(serializer->Serialize(root,&serialized));
	Check(QuadooCreateControl(context,NULL,serialized,&copy));
	Check(serializer->Serialize(copy,&repeated));
	REQUIRE(JSONCompareObjects(serialized,repeated) == 0);
	Check(copy->Destroy());
	{
		TStackRef<IQuadooControlContainer> foreign;
		TStackRef<IQuadooComponentHost> foreignHost;
		TStackRef<IQuadooComponentCollection> foreignComponents;
		TStackRef<IQuadooComponent> foreignImages;
		copy.Release();
		definition.Release();
		Check(Parse(ImageListNativeDefinition,&definition));
		Check(QuadooCreateControl(context,NULL,definition,&copy));
		Check(copy->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&foreignHost)));
		Check(foreignHost->GetComponents(&foreignComponents));
		Check(foreignComponents->FindComponent(RSTRING_CAST(L"ImageList1"),&foreignImages));
		REQUIRE(view->SetImages(QuadooForm::ListSmallImages,foreignImages) == E_INVALIDARG);
		Check(copy->Destroy());
	}
	Check(images->Destroy());
	Check(native->GetHandle(&window));
	REQUIRE(!ListView_GetImageList(window,LVSIL_NORMAL) && !ListView_GetImageList(window,LVSIL_SMALL) && !ListView_GetImageList(window,LVSIL_STATE));
	REQUIRE(ImageList_GetImageCount(handle) == 2 && ImageList_GetImageCount(replacement) == 2);
	Check(root->Destroy());
	Check(first->GetAttached(&attached));
	REQUIRE(!attached && columns->GetCount(&count) == QuadooForm::EDestroyed);
	hr = S_OK;
	Cleanup:
	if(root)
	{
		root->Destroy();
	}
	if(copy)
	{
		copy->Destroy();
	}
	return hr;
}

static HRESULT ListViewResources()
{
	HRESULT hr;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooControlFactory> factory;
	TStackRef<IQuadooFactoryLifetime> lifetime;
	TStackRef<IJSONObject> definition;
	CGuiResourceMonitor monitor(L"ListView / shared images / staged HWNDs");
	Check(QuadooGetControlRegistry(&registry));
	Check(registry->FindFactory(RSTRING_CAST(L"ListView"),&factory));
	Check(factory->QueryInterface(__uuidof(IQuadooFactoryLifetime),reinterpret_cast<PVOID*>(&lifetime)));
	Check(ListViewRoot(&definition));
	while(!monitor.Complete())
	{
		TStackRef<IQuadooListColumn> retained;
		TStackRef<IQuadooListItem> retainedItem;
		UINT live;
		{
			TStackRef<IQuadooConstructionContext> context;
			TStackRef<CListTestBinder> binder;
			TStackRef<IQuadooControl> root;
			TStackRef<IQuadooControl> control;
			TStackRef<IQuadooControlContainer> container;
			TStackRef<IQuadooListView> view;
			TStackRef<IQuadooListColumns> columns;
			TStackRef<IQuadooListItems> items;
			TStackRef<IQuadooWinControl> native;
			TStackRef<IQuadooLayoutControl> layout;
			QuadooForm::Rect bounds;
			HWND window;
			binder.Attach(__new CListTestBinder);
			CheckAlloc(binder);
			Check(Context(96,binder,&context,registry,QuadooForm::Preview));
			Check(QuadooCreateControl(context,NULL,definition,&root));
			Check(root->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&container)));
			Check(container->FindControl(RSTRING_CAST(L"List1"),&control));
			Check(control->QueryInterface(__uuidof(IQuadooListView),reinterpret_cast<PVOID*>(&view)));
			Check(control->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native)));
			Check(control->QueryInterface(__uuidof(IQuadooLayoutControl),reinterpret_cast<PVOID*>(&layout)));
			Check(view->GetColumns(&columns));
			Check(view->GetItems(&items));
			Check(items->Insert(0,RSTRING_CAST(L"retained row"),&retainedItem));
			Check(retainedItem->SetImageIndex(0));
			Check(retainedItem->SetSubItem(0,RSTRING_CAST(L"subitem")));
			Check(view->SetSelected(retainedItem));
			Check(columns->GetAt(0,&retained));
			Check(retained->SetDisplayIndex(2));
			Check(control->GetBounds(&bounds));
			for(UINT dpi = 144; dpi <= 192; dpi += 48)
			{
				Check(layout->Arrange(&bounds,dpi));
			}
			Check(view->SetImages(QuadooForm::ListStateImages,NULL));
			Check(MenuFlag(control,L"CheckBoxes",TRUE));
			Check(MenuFlag(control,L"CheckBoxes",FALSE));
			Check(SetInt(control,L"ViewStyle",QuadooForm::ListViewSmallIcon));
			Check(SetInt(control,L"ViewStyle",QuadooForm::ListViewReport));
			Check(native->GetHandle(&window));
			REQUIRE(ListView_GetItemCount(window) == 1 && ListView_GetSelectedCount(window) == 1);
			Check(root->Destroy());
			REQUIRE(!IsWindow(window));
		}
		Check(lifetime->GetLiveCount(&live));
		REQUIRE(live != 0);
		REQUIRE(registry->UnregisterFactory(RSTRING_CAST(L"ListView")) == HRESULT_FROM_WIN32(ERROR_BUSY));
		retained.Release();
		Check(lifetime->GetLiveCount(&live));
		REQUIRE(live != 0);
		retainedItem.Release();
		Check(lifetime->GetLiveCount(&live));
		REQUIRE(live == 0);
		Check(monitor.AfterCycle());
	}
	Check(registry->UnregisterFactory(RSTRING_CAST(L"ListView")));
	Cleanup:
	return hr;
}
