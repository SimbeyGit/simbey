#include <psapi.h>
#pragma comment(lib,"psapi.lib")

class CVirtualListBinder : public CBaseUnknown, public IQuadooEventsBinder, public IQuadooDelegate,
	public IQuadooConstructionReport
{
public:
	IQuadooListView* view;
	UINT reads;
	UINT updates;
	UINT ranges;
	UINT hints;
	UINT finds;
	UINT changes;
	UINT reports;
	UINT checkedEvents;
	UINT maxIndex;
	INT fields;
	INT column;
	BOOL fail;
	BOOL failUpdate;
	BOOL reject;
	BOOL handleUpdate;
	BOOL replaceCheck;
	BOOL replaceText;
	BOOL nested;
	BOOL destroy;
	BOOL checks[4];
	HRESULT sameIndex;
	HRESULT otherIndex;
	HRESULT mutation;
	CRString caption;
	CRString cell;
	TStackRef<IQuadooListItem> retained;
	TStackRef<IQuadooListItem> updated;
	TStackRef<IUnknown> payload;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooEventsBinder)
		UNK_INTERFACE(IQuadooDelegate)
		UNK_INTERFACE(IQuadooConstructionReport)
	END_UNK_MAP
	CVirtualListBinder() : view(NULL), reads(0), updates(0), ranges(0), hints(0), finds(0), changes(0),
		reports(0), checkedEvents(0), maxIndex(0), fields(0), column(0), fail(FALSE), failUpdate(FALSE), reject(FALSE),
		handleUpdate(TRUE), replaceCheck(FALSE), replaceText(FALSE), nested(FALSE), destroy(FALSE),
		sameIndex(S_OK), otherIndex(S_OK), mutation(S_OK)
	{
		ZeroMemory(checks,sizeof(checks));
	}
	STDMETHOD(BindEvent)(RSTRING, IQuadooDelegate** result)
	{
		*result = this;
		AddRef();
		return S_OK;
	}
	STDMETHOD(PostBind)(IQuadooControl*)
	{
		return S_OK;
	}
	STDMETHOD(OnError)(HRESULT, RSTRING, RSTRING)
	{
		reports++;
		return S_OK;
	}
	STDMETHOD(Invoke)(IQuadooEventArgs* args)
	{
		HRESULT hr = S_OK;
		CRString name;
		TStackRef<IQuadooEventObjectArgs> objects;
		TStackRef<IUnknown> object;
		TStackRef<IQuadooListItem> item;
		UINT index;
		Check(args->GetEventName(&name));
		if(name == RSTRING_CAST(L"OnDataHint"))
		{
			hints++;
		}
		else if(name == RSTRING_CAST(L"OnDataFind"))
		{
			finds++;
			Check(args->SetInt(3,2));
		}
		else if(name == RSTRING_CAST(L"OnDataStateChange"))
		{
			ranges++;
		}
		else
		{
			Check(args->QueryInterface(__uuidof(IQuadooEventObjectArgs),reinterpret_cast<PVOID*>(&objects)));
			UINT argument = name == RSTRING_CAST(L"OnDataUpdate") || name == RSTRING_CAST(L"OnEditing") ||
				name == RSTRING_CAST(L"OnEdited") || name == RSTRING_CAST(L"OnItemChecking") ? 1 : 0;
			Check(objects->GetObject(argument,&object));
			if(object)
			{
				Check(object->QueryInterface(__uuidof(IQuadooListItem),reinterpret_cast<PVOID*>(&item)));
				Check(item->GetItemIndex(&index));
			}
			if(name == RSTRING_CAST(L"OnData"))
			{
				CRString text;
				reads++;
				maxIndex = max(maxIndex,index);
				CheckIf(fail,E_FAIL);
				Check(args->GetInt(1,&fields));
				Check(args->GetInt(2,&column));
				Check(RStrFormatW(&text,L"Row %u",index));
				Check(item->SetCaption(index == 1 && caption ? static_cast<RSTRING>(caption) : static_cast<RSTRING>(text)));
				Check(item->SetSubItem(0,index == 1 && cell ? static_cast<RSTRING>(cell) : RSTRING_CAST(L"detail")));
				Check(item->SetImageIndex(index % 2));
				Check(item->SetChecked(index < ARRAYSIZE(checks) ? checks[index] : FALSE));
				Check(item->SetData(payload));
				retained = item;
				if(nested && index == 1)
				{
					TStackRef<IQuadooListItems> items;
					TStackRef<IQuadooListItem> request;
					Check(view->GetItems(&items));
					sameIndex = items->GetAt(index,&request);
					otherIndex = items->GetAt(2,&request);
					mutation = items->SetCount(10);
				}
				if(destroy)
				{
					TStackRef<IQuadooComponent> sender;
					Check(args->GetSender(&sender));
					Check(sender->Destroy());
				}
			}
			else if(name == RSTRING_CAST(L"OnDataUpdate"))
			{
				INT mask;
				INT column;
				updates++;
				updated = item;
				Check(args->SetBool(0,reject));
				Check(args->SetBool(4,handleUpdate));
				CheckIf(failUpdate,E_FAIL);
				if(!reject && handleUpdate)
				{
					Check(args->GetInt(2,&mask));
					Check(args->GetInt(3,&column));
					if(mask == QuadooForm::ListChangeState && index < ARRAYSIZE(checks))
					{
						BOOL value;
						Check(item->GetChecked(&value));
						checks[index] = value;
					}
					else if(index == 1 && column == 0)
					{
						caption = NULL;
						Check(item->GetCaption(&caption));
					}
					else if(index == 1)
					{
						cell = NULL;
						Check(item->GetSubItem(column - 1,&cell));
					}
				}
			}
			else if(name == RSTRING_CAST(L"OnItemChecking"))
			{
				Check(args->SetBool(0,reject));
				if(replaceCheck)
				{
					Check(args->SetBool(2,FALSE));
				}
			}
			else if(name == RSTRING_CAST(L"OnEdited"))
			{
				Check(args->SetBool(0,reject));
				if(replaceText)
				{
					Check(args->SetString(3,RSTRING_CAST(L"edited replacement")));
				}
			}
			else if(name == RSTRING_CAST(L"OnEditing"))
			{
				Check(args->SetBool(0,reject));
			}
			else if(name == RSTRING_CAST(L"OnItemChecked"))
			{
				checkedEvents++;
			}
			else if(name == RSTRING_CAST(L"OnChange"))
			{
				changes++;
			}
		}
	Cleanup:
		return hr;
	}
};

static const WCHAR VirtualListDefinition[] =
	L"{\"type\":\"Form\",\"name\":\"VirtualForm\",\"children\":[{\"type\":\"ListView\",\"name\":\"VirtualList\","
	L"\"properties\":{\"OwnerData\":true,\"CheckBoxes\":true,\"MultiSelect\":true,\"Width\":350,\"Height\":180,"
	L"\"Columns\":[{\"Caption\":\"Name\"},{\"Caption\":\"Detail\"}]},"
	L"\"events\":{\"OnData\":\"E\",\"OnDataUpdate\":\"E\",\"OnDataHint\":\"E\",\"OnDataFind\":\"E\","
	L"\"OnDataStateChange\":\"E\",\"OnItemChecking\":\"E\",\"OnItemChecked\":\"E\",\"OnEditing\":\"E\",\"OnEdited\":\"E\",\"OnChange\":\"E\"}}]}";

static HRESULT VirtualImagesMatch(HIMAGELIST source, HIMAGELIST combined)
{
	HRESULT hr;
	INT sourceWidth;
	INT sourceHeight;
	INT width;
	INT height;
	CImageTestCanvas expected;
	CImageTestCanvas actual;
	REQUIRE(ImageList_GetIconSize(source,&sourceWidth,&sourceHeight));
	REQUIRE(ImageList_GetIconSize(combined,&width,&height));
	Check(expected.Create(width,height));
	Check(actual.Create(width,height));
	for(INT index = 0; index < ImageList_GetImageCount(source); index++)
	{
		expected.Fill(RGB(17,33,49));
		actual.Fill(RGB(17,33,49));
		REQUIRE(ImageList_Draw(source,index,expected.DC(),width - sourceWidth,(height - sourceHeight) / 2,ILD_TRANSPARENT));
		REQUIRE(ImageList_Draw(combined,(index + 1) * 2,actual.DC(),0,0,ILD_TRANSPARENT));
		BOOL equal = TRUE;
		for(INT y = 0; equal && y < sourceHeight; y++)
		{
			for(INT x = 0; x < sourceWidth; x++)
			{
				UINT px = width - sourceWidth + x;
				UINT py = (height - sourceHeight) / 2 + y;
				COLORREF reference = expected.Pixel(px,py);
				COLORREF rendered = actual.Pixel(px,py);
				// Common-control GetIcon/ReplaceIcon can round premultiplied
				// channels by one level while preserving alpha and masks.
				if(abs(static_cast<INT>(GetRValue(reference)) - GetRValue(rendered)) > 1 ||
					abs(static_cast<INT>(GetGValue(reference)) - GetGValue(rendered)) > 1 ||
					abs(static_cast<INT>(GetBValue(reference)) - GetBValue(rendered)) > 1)
				{
					CRString mismatch;
					Check(RStrFormatW(&mismatch,L"Composite image %d pixel %u,%u expected %x actual %x\r\n",
						index,px,py,expected.Pixel(px,py),actual.Pixel(px,py)));
					Log(mismatch.c_wstr());
					equal = FALSE;
					break;
				}
			}
		}
		REQUIRE(equal);
	}
Cleanup:
	return hr;
}

static HRESULT ListVirtualItems()
{
	HRESULT hr;
	TStackRef<CVirtualListBinder> binder;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONObject> saved;
	TStackRef<IQuadooControl> root;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooControlContainer> container;
	TStackRef<IQuadooListView> view;
	TStackRef<IQuadooListItems> items;
	TStackRef<IQuadooListItem> first;
	TStackRef<IQuadooListItem> second;
	TStackRef<IQuadooListItem> old;
	TStackRef<IQuadooWinControl> native;
	TStackRef<IQuadooListColumns> columns;
	TStackRef<IQuadooListColumn> column;
	TStackRef<IQuadooFormSerializer> serializer;
	TStackRef<IQuadooLayoutControl> layout;
	TStackRef<IQuadooComponentHost> host;
	TStackRef<IQuadooComponentCollection> components;
	TStackRef<IQuadooComponent> imageComponent;
	TStackRef<IQuadooImageList> imageList;
	TStackRef<IQuadooImageListSnapshot> imageSnapshot;
	CRString text;
	UINT count;
	UINT index;
	BOOL flag;
	GUID id;
	ULONGLONG generation;
	HWND window;
	HWND original;
	QuadooForm::ContextOptions options = {0};
	binder.Attach(__new CVirtualListBinder);
	CheckAlloc(binder);
	options.cbSize = sizeof(options);
	options.DPI = 96;
	options.pBinder = binder;
	options.pReport = binder;
	Check(QuadooCreateConstructionContext(&options,&context));
	Check(Parse(ImageListNativeDefinition,&definition));
	{
		TStackRef<IJSONObject> children;
		TStackRef<IJSONValue> value;
		Check(Parse(VirtualListDefinition,&children));
		Check(children->FindNonNullValueW(L"children",&value));
		Check(definition->AddValueW(L"children",value));
	}
	Check(QuadooCreateControl(context,NULL,definition,&root));
	Check(root->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&container)));
	Check(container->FindControl(RSTRING_CAST(L"VirtualList"),&control));
	Check(control->QueryInterface(__uuidof(IQuadooListView),reinterpret_cast<PVOID*>(&view)));
	Check(control->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native)));
	Check(control->QueryInterface(__uuidof(IQuadooLayoutControl),reinterpret_cast<PVOID*>(&layout)));
	Check(view->GetItems(&items));
	Check(root->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&host)));
	Check(host->GetComponents(&components));
	Check(components->FindComponent(RSTRING_CAST(L"ImageList1"),&imageComponent));
	Check(imageComponent->QueryInterface(__uuidof(IQuadooImageList),reinterpret_cast<PVOID*>(&imageList)));
	Check(view->SetImages(QuadooForm::ListLargeImages,imageComponent));
	Check(view->SetImages(QuadooForm::ListSmallImages,imageComponent));
	binder->view = view;
	Check(native->GetHandle(&window));
	Check(items->GetCount(&count));
	REQUIRE(count == 0 && binder->reads == 0);
	Check(items->SetCount(4));
	REQUIRE(ListView_GetItemCount(window) == 4);
	Check(items->GetAt(1,&first));
	Check(first->GetIsVirtual(&flag));
	REQUIRE(flag);
	Check(first->GetID(&id));
	REQUIRE(IsEqualGUID(id,GUID_NULL));
	Check(first->GetCaption(&text));
	REQUIRE(text == RSTRING_CAST(L"Row 1"));
	Check(first->GetDataGeneration(&generation));
	REQUIRE(generation > 0);
	REQUIRE(first->SetCaption(RSTRING_CAST(L"frozen")) == QuadooForm::EInvalidState);
	REQUIRE(first->SetSelected(TRUE) == QuadooForm::EInvalidState);
	REQUIRE(items->Insert(0,RSTRING_CAST(L"bad"),&second) == QuadooForm::EInvalidState);
	REQUIRE(items->Delete(first) == QuadooForm::EInvalidState);
	REQUIRE(items->Clear() == QuadooForm::EInvalidState);
	REQUIRE(view->Sort(0,TRUE) == QuadooForm::EInvalidState);
	REQUIRE(items->SetCount(100000001) == E_INVALIDARG);
	binder->nested = TRUE;
	Check(items->GetAt(1,&second));
	REQUIRE(binder->sameIndex == QuadooForm::EInvalidState && SUCCEEDED(binder->otherIndex) &&
		binder->mutation == QuadooForm::EInvalidState);
	binder->nested = FALSE;
	REQUIRE(first != static_cast<IQuadooListItem*>(second));
	{
		WCHAR buffer[8] = {0};
		ListView_GetItemText(window,1,0,buffer,ARRAYSIZE(buffer));
		REQUIRE(!lstrcmpW(buffer,L"Row 1"));
		REQUIRE(binder->fields & QuadooForm::ListDataCaption);
		ListView_GetItemText(window,1,1,buffer,ARRAYSIZE(buffer));
		REQUIRE(!lstrcmpW(buffer,L"detail") && binder->column == 1);
	}
	Check(view->UpdateVirtualText(1,0,RSTRING_CAST(L"accepted")));
	text = NULL;
	Check(first->GetCaption(&text));
	REQUIRE(text == RSTRING_CAST(L"Row 1"));
	REQUIRE(binder->updated->SetCaption(RSTRING_CAST(L"late")) == QuadooForm::EInvalidState);
	second.Release();
	Check(items->GetAt(1,&second));
	text = NULL;
	Check(second->GetCaption(&text));
	REQUIRE(text == RSTRING_CAST(L"accepted"));
	binder->reject = TRUE;
	REQUIRE(view->UpdateVirtualText(1,0,RSTRING_CAST(L"rejected")) == S_FALSE);
	binder->reject = FALSE;
	binder->failUpdate = TRUE;
	REQUIRE(view->UpdateVirtualText(1,0,RSTRING_CAST(L"failed")) == E_FAIL);
	REQUIRE(binder->caption == RSTRING_CAST(L"accepted") &&
		binder->updated->SetCaption(RSTRING_CAST(L"late")) == QuadooForm::EInvalidState);
	binder->failUpdate = FALSE;
	binder->handleUpdate = FALSE;
	REQUIRE(view->UpdateVirtualChecked(1,TRUE) == S_FALSE && !binder->checks[1]);
	binder->handleUpdate = TRUE;
	binder->replaceCheck = TRUE;
	REQUIRE(view->UpdateVirtualChecked(1,TRUE) == S_FALSE && !binder->checks[1]);
	binder->replaceCheck = FALSE;
	Check(view->UpdateVirtualChecked(1,TRUE));
	REQUIRE(binder->checks[1] && binder->checkedEvents == 1);
	Check(view->SetSelected(second));
	Check(view->SetFocused(second));
	Check(view->GetSelectedCount(&count));
	REQUIRE(count == 1);
	REQUIRE(view->SetItemSelected(1,TRUE) == S_FALSE);
	original = window;
	Check(view->GetColumns(&columns));
	Check(columns->GetAt(0,&column));
	Check(column->SetWidth(180));
	Check(native->GetHandle(&window));
	REQUIRE(window == original && ListView_GetSelectedCount(window) == 1);
	Check(first->GetAttached(&flag));
	REQUIRE(!flag);
	{
		QuadooForm::Rect bounds;
		Check(control->GetBounds(&bounds));
		for(UINT mode = 0; mode < 4; mode++)
		{
			Check(SetInt(control,L"ViewStyle",mode));
			Check(layout->Arrange(&bounds,144));
			Check(native->GetHandle(&window));
			REQUIRE(window == original && ListView_GetItemCount(window) == 4);
			HIMAGELIST images = ListView_GetImageList(window,mode == 0 ? LVSIL_NORMAL : mode == 1 ? LVSIL_SMALL : LVSIL_STATE);
			REQUIRE(images && ImageList_GetImageCount(images) == (mode < 2 ? 6 : 2));
			INT x;
			INT y;
			REQUIRE(ImageList_GetIconSize(images,&x,&y) && y >= 19);
			if(mode < 2)
			{
				HIMAGELIST source;
				imageSnapshot.Release();
				Check(imageList->Acquire(144,&imageSnapshot));
				Check(imageSnapshot->GetHandle(&source));
				REQUIRE(source != images && ImageList_GetImageCount(source) == 2);
				Check(VirtualImagesMatch(source,images));
			}
			binder->checks[1] = FALSE;
			Check(view->SetItemSelected(1,TRUE));
			SendMessageW(window,WM_KEYDOWN,VK_SPACE,0);
			REQUIRE(binder->checks[1]);
			LVITEMW row = {0};
			row.mask = LVIF_IMAGE | LVIF_STATE;
			row.stateMask = LVIS_STATEIMAGEMASK;
			row.iItem = 1;
			REQUIRE(ListView_GetItem(window,&row));
			REQUIRE(mode < 2 ? row.iImage == 5 : ((row.state & LVIS_STATEIMAGEMASK) >> 12) == 2);
			RECT hit;
			REQUIRE(ListView_GetItemRect(window,1,&hit,mode < 2 ? LVIR_ICON : LVIR_BOUNDS));
			UINT updates = binder->updates;
			LPARAM point = MAKELPARAM(hit.left + 4,(hit.top + hit.bottom) / 2);
			SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,point);
			SendMessageW(window,WM_LBUTTONUP,0,point);
			REQUIRE(!binder->checks[1] && binder->updates == updates + 1);
		}
	}
	Check(SetInt(control,L"ViewStyle",QuadooForm::ListViewReport));
	// Native edit controls use the normal form keyboard route and backend writeback.
	{
		TStackRef<IQuadooFormKeyboard> keyboard;
		MSG message = {0};
		BOOL handled;
		Check(root->SetVisible(TRUE));
		Check(root->QueryInterface(__uuidof(IQuadooFormKeyboard),reinterpret_cast<PVOID*>(&keyboard)));
		second.Release();
		Check(items->GetAt(1,&second));
		Check(view->BeginLabelEdit(second));
		HWND edit = ListView_GetEditControl(window);
		REQUIRE(edit && GetFocus() == edit);
		SetWindowTextW(edit,L"native virtual edit");
		message.hwnd = edit;
		message.message = WM_KEYDOWN;
		message.wParam = VK_RETURN;
		Check(keyboard->PreprocessMessage(&message,&handled));
		REQUIRE(!handled);
		DispatchMessageW(&message);
		REQUIRE(!ListView_GetEditControl(window) && binder->caption == RSTRING_CAST(L"native virtual edit"));
		binder->replaceText = TRUE;
		Check(view->BeginLabelEdit(second));
		edit = ListView_GetEditControl(window);
		SetWindowTextW(edit,L"typed");
		SendMessageW(edit,WM_KEYDOWN,VK_RETURN,0);
		REQUIRE(binder->caption == RSTRING_CAST(L"edited replacement"));
		binder->replaceText = FALSE;
		Check(view->BeginLabelEdit(second));
		edit = ListView_GetEditControl(window);
		SetWindowTextW(edit,L"discarded");
		SendMessageW(edit,WM_KEYDOWN,VK_ESCAPE,0);
		REQUIRE(binder->caption == RSTRING_CAST(L"edited replacement"));
		Check(view->BeginLabelEdit(second));
		Check(view->ResetVirtualData(4));
		REQUIRE(!ListView_GetEditControl(window));
		binder->caption = RSTRING_CAST(L"accepted");
		Check(root->SetVisible(FALSE));
	}
	Check(QuadooGetFormSerializer(&serializer));
	Check(serializer->Serialize(root,&saved));
	{
		TStackRef<IQuadooControl> copy;
		TStackRef<IQuadooControl> child;
		TStackRef<IQuadooControlContainer> parent;
		TStackRef<IQuadooListView> copied;
		TStackRef<IQuadooListItems> collection;
		TStackRef<IQuadooListItem> foreign;
		TStackRef<IJSONObject> repeated;
		Check(QuadooCreateControl(context,NULL,saved,&copy));
		Check(copy->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&parent)));
		Check(parent->FindControl(RSTRING_CAST(L"VirtualList"),&child));
		Check(child->QueryInterface(__uuidof(IQuadooListView),reinterpret_cast<PVOID*>(&copied)));
		Check(copied->GetItems(&collection));
		Check(collection->GetCount(&count));
		REQUIRE(count == 0);
		Check(serializer->Serialize(copy,&repeated));
		REQUIRE(JSONCompareObjects(saved,repeated) == 0);
		Check(collection->SetCount(4));
		Check(collection->GetAt(1,&foreign));
		REQUIRE(view->SetSelected(foreign) == E_INVALIDARG);
		Check(copy->Destroy());
	}
	{
		TStackRef<IQuadooProperties> properties;
		TStackRef<IJSONValue> serialized;
		TStackRef<IJSONArray> array;
		UINT property;
		Check(Property(control,L"Items",&properties,&property));
		Check(properties->GetValueJSON(property,&serialized));
		Check(serialized->GetArray(&array));
		REQUIRE(array->Count() == 0);
	}
	binder->fail = TRUE;
	{
		WCHAR buffer[12] = L"unsafe";
		UINT reports = binder->reports;
		ListView_GetItemText(window,1,0,buffer,ARRAYSIZE(buffer));
		REQUIRE(!buffer[0] && binder->reports > reports);
		old.Release();
		REQUIRE(items->GetAt(1,&old) == E_FAIL && !old);
	}
	binder->fail = FALSE;
	Check(items->GetAt(1,&old));
	Check(view->SetSelected(old));
	Check(items->SetCount(8));
	Check(old->GetAttached(&flag));
	REQUIRE(!flag && ListView_GetSelectedCount(window) == 1);
	Check(view->ResetVirtualData(4));
	REQUIRE(ListView_GetSelectedCount(window) == 0);
	{
		PROCESS_MEMORY_COUNTERS_EX before = {sizeof(before)};
		PROCESS_MEMORY_COUNTERS_EX after = {sizeof(after)};
		REQUIRE(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&before),sizeof(before)));
		Check(items->SetCount(100000000));
		REQUIRE(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&after),sizeof(after)));
		REQUIRE(ListView_GetItemCount(window) == 100000000 && after.PrivateUsage < before.PrivateUsage + 32 * 1024 * 1024);
		UINT reads = binder->reads;
		ListView_SetItemState(window,-1,LVIS_SELECTED,LVIS_SELECTED);
		Check(view->GetSelectedCount(&count));
		REQUIRE(count == 100000000 && binder->reads <= reads + 2);
		CRString report;
		Check(RStrFormatW(&report,L"OwnerData Report count 100000000: private-byte growth %q\r\n",
			static_cast<ULONGLONG>(after.PrivateUsage > before.PrivateUsage ? after.PrivateUsage - before.PrivateUsage : 0)));
		Log(report.c_wstr());
		Check(view->ResetVirtualData(4));
	}
	{
		TStackRef<IQuadooMessageHandler> handler;
		BOOL handled;
		LRESULT result;
		Check(control->QueryInterface(__uuidof(IQuadooMessageHandler),reinterpret_cast<PVOID*>(&handler)));
		NMLVCACHEHINT hint = {0};
		hint.hdr.hwndFrom = window;
		hint.hdr.code = LVN_ODCACHEHINT;
		hint.iFrom = 0;
		hint.iTo = 3;
		Check(handler->HandleMessage(window,WM_NOTIFY,0,reinterpret_cast<LPARAM>(&hint),&handled,&result));
		REQUIRE(binder->hints > 0);
		NMLVFINDITEMW find = {0};
		find.hdr.hwndFrom = window;
		find.hdr.code = LVN_ODFINDITEMW;
		find.lvfi.flags = LVFI_STRING | LVFI_PARTIAL | LVFI_WRAP;
		find.lvfi.psz = L"Row";
		Check(handler->HandleMessage(window,WM_NOTIFY,0,reinterpret_cast<LPARAM>(&find),&handled,&result));
		REQUIRE(result == 2 && binder->finds == 1);
	}
	first.Release();
	Check(items->GetAt(1,&first));
	Check(root->Destroy());
	Check(first->GetAttached(&flag));
	REQUIRE(!flag);
	text = NULL;
	Check(first->GetCaption(&text));
	REQUIRE(text == RSTRING_CAST(L"accepted"));
	Check(first->GetItemIndex(&index));
	REQUIRE(index == 1 && first->SetChecked(FALSE) == QuadooForm::EInvalidState);
	hr = S_OK;
Cleanup:
	if(root)
	{
		root->Destroy();
	}
	return hr;
}
