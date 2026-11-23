class CFailVirtualSnapshot : public CBaseUnknown, public IQuadooImageListSnapshot
{
	TStackRef<IQuadooImageListSnapshot> m_source;
	UINT m_calls;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooImageListSnapshot)
	END_UNK_MAP
	CFailVirtualSnapshot(IQuadooImageListSnapshot* source) : m_source(source), m_calls(0)
	{
	}
	STDMETHOD(GetHandle)(HIMAGELIST* handle)
	{
		HRESULT hr;
		CheckIf(!handle,E_POINTER);
		*handle = NULL;
		CheckIf(++m_calls > 1,E_OUTOFMEMORY);
		Check(m_source->GetHandle(handle));
	Cleanup:
		return hr;
	}
	STDMETHOD(GetSize)(UINT* x, UINT* y, UINT* dpi)
	{
		return m_source->GetSize(x,y,dpi);
	}
};
class CFailVirtualSource : public CListExternalSource
{
	TStackRef<IQuadooImageList> m_source;
public:
	CFailVirtualSource(IQuadooComponentCollection* owner, IQuadooComponent* metadata, IQuadooImageList* source) :
		CListExternalSource(owner,metadata,source), m_source(source)
	{
	}
	STDMETHOD(GetName)(RSTRING* name)
	{
		return RStrCreateW(LSP(L"FailingVirtualImages"),name);
	}
	STDMETHOD(Acquire)(UINT dpi, IQuadooImageListSnapshot** snapshot)
	{
		HRESULT hr;
		TStackRef<IQuadooImageListSnapshot> source;
		CheckIf(!snapshot,E_POINTER);
		*snapshot = NULL;
		Check(m_source->Acquire(dpi,&source));
		*snapshot = __new CFailVirtualSnapshot(source);
		CheckAlloc(*snapshot);
	Cleanup:
		return hr;
	}
};

static HRESULT ListVirtualRollbackTests()
{
	HRESULT hr;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<QF::CFactory> factory;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONObject> before;
	TStackRef<IJSONObject> after;
	TStackRef<IQuadooControl> root;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooControlContainer> container;
	TStackRef<IQuadooWinControl> native;
	TStackRef<IQuadooLayoutControl> layout;
	TStackRef<IQuadooListView> view;
	TStackRef<IQuadooListItems> items;
	TStackRef<IQuadooListItem> retained;
	TStackRef<IQuadooListColumns> columns;
	TStackRef<IQuadooListColumn> column;
	TStackRef<IQuadooFormSerializer> serializer;
	TStackRef<IQuadooComponentHost> host;
	TStackRef<IQuadooComponentCollection> components;
	TStackRef<IQuadooComponent> images;
	TStackRef<IQuadooImageList> imageList;
	TStackRef<CListExternalSource> external;
	TStackRef<CFailVirtualSource> failing;
	QuadooForm::ContextOptions options = {0};
	ULONGLONG generation;
	ULONGLONG currentGeneration;
	UINT count;
	INT width;
	BOOL attached;
	HWND window;
	HWND current;
	QuadooForm::Rect bounds;
	Check(QuadooGetControlRegistry(&registry));
	factory.Attach(__new QF::TFactory<CFailListView>);
	CheckAlloc(factory);
	Check(factory->Initialize());
	Check(registry->RegisterFactory(factory));
	options.cbSize = sizeof(options);
	options.DPI = 96;
	options.pRegistry = registry;
	Check(QuadooCreateConstructionContext(&options,&context));
	Check(NativeDefinition(ImageListNativeDefinition,&definition));
	Check(QuadooCreateControl(context,NULL,definition,&root));
	Check(root->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&container)));
	definition.Release();
	Check(NativeDefinition(L"{\"type\":\"FailListView\",\"name\":\"Virtual\",\"properties\":{\"OwnerData\":true,\"MultiSelect\":true,\"CheckBoxes\":true,\"Columns\":[{\"Caption\":\"First\"},{\"Caption\":\"Second\"}]}}",&definition));
	Check(QuadooCreateControl(context,container,definition,&control));
	Check(control->QueryInterface(__uuidof(IQuadooListView),reinterpret_cast<PVOID*>(&view)));
	Check(control->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native)));
	Check(control->QueryInterface(__uuidof(IQuadooLayoutControl),reinterpret_cast<PVOID*>(&layout)));
	Check(view->GetItems(&items));
	Check(view->GetColumns(&columns));
	Check(columns->GetAt(0,&column));
	Check(column->GetWidth(&width));
	Check(native->GetHandle(&window));
	Check(items->SetCount(100000000));
	Check(view->SetItemSelected(3,TRUE));
	Check(items->GetAt(3,&retained));
	Check(view->GetDataGeneration(&generation));
	Check(QuadooGetFormSerializer(&serializer));
	Check(serializer->Serialize(root,&before));
	s_failListMessage = LVM_SETITEMCOUNT;
	VERIFY(view->ResetVirtualData(12) == E_OUTOFMEMORY);
	Check(items->GetCount(&count));
	Check(view->GetDataGeneration(&currentGeneration));
	Check(retained->GetAttached(&attached));
	VERIFY(count == 100000000 && generation == currentGeneration && attached && ListView_GetSelectedCount(window) == 1);
	const UINT failures[] = {LVM_INSERTCOLUMNW,LVM_SETCOLUMNORDERARRAY};
	for(UINT n = 0; n < ARRAYSIZE(failures); n++)
	{
		s_failListMessage = failures[n];
		VERIFY(column->SetWidth(250) == E_OUTOFMEMORY);
		Check(native->GetHandle(&current));
		Check(column->GetWidth(&width));
		Check(view->GetDataGeneration(&currentGeneration));
		VERIFY(current == window && width == ListView_GetColumnWidth(window,0) && generation == currentGeneration);
		VERIFY(Header_GetItemCount(ListView_GetHeader(window)) == 2 && ListView_GetItemCount(window) == 100000000);
		Check(serializer->Serialize(root,&after));
		VERIFY(JSONCompareObjects(before,after) == 0);
		after.Release();
	}
	Check(root->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&host)));
	Check(host->GetComponents(&components));
	Check(components->FindComponent(RSTRING_CAST(L"ImageList1"),&images));
	Check(images->QueryInterface(__uuidof(IQuadooImageList),reinterpret_cast<PVOID*>(&imageList)));
	external.Attach(__new CListExternalSource(components,images,imageList));
	CheckAlloc(external);
	Check(external->Initialize());
	Check(components->RegisterComponent(external));
	Check(view->SetImages(QuadooForm::ListSmallImages,external));
	HIMAGELIST previous = ListView_GetImageList(window,LVSIL_SMALL);
	failing.Attach(__new CFailVirtualSource(components,images,imageList));
	CheckAlloc(failing);
	Check(failing->Initialize());
	Check(components->RegisterComponent(failing));
	VERIFY(view->SetImages(QuadooForm::ListLargeImages,failing) == E_OUTOFMEMORY);
	{
		TStackRef<IQuadooComponent> assigned;
		Check(view->GetImages(QuadooForm::ListLargeImages,&assigned));
		VERIFY(!assigned && failing->Observers() == 0 && !ListView_GetImageList(window,LVSIL_NORMAL));
		VERIFY(ListView_GetImageList(window,LVSIL_SMALL) == previous);
	}
	Check(control->GetBounds(&bounds));
	HFONT font = reinterpret_cast<HFONT>(SendMessageW(window,WM_GETFONT,0,0));
	external->acquireResult = E_OUTOFMEMORY;
	VERIFY(layout->Arrange(&bounds,144) == E_OUTOFMEMORY);
	external->acquireResult = S_OK;
	Check(native->GetHandle(&current));
	VERIFY(current == window && QF::ControlDPI(control) == 96 && ListView_GetImageList(window,LVSIL_SMALL) == previous);
	VERIFY(reinterpret_cast<HFONT>(SendMessageW(window,WM_GETFONT,0,0)) == font);
	// A style/header/DPI rebuild at a large count must preserve native range state.
	ListView_SetItemState(window,-1,LVIS_SELECTED,LVIS_SELECTED);
	Check(layout->Arrange(&bounds,144));
	Check(view->GetSelectedCount(&count));
	VERIFY(count == 100000000 && external->Observers() == 1);
	Check(view->ResetVirtualData(100));
	Check(ImageNumber(control,L"ViewStyle",QuadooForm::ListViewSmallIcon));
	Check(root->Destroy());
	VERIFY(external->Observers() == 0);
	Check(retained->GetAttached(&attached));
	VERIFY(!attached);
	hr = S_OK;
Cleanup:
	s_failListMessage = 0;
	if(root)
	{
		root->Destroy();
	}
	return hr;
}
