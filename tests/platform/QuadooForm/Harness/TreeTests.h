class CTreeTestBinder;
class CTreeTestDelegate : public CBaseUnknown, public IQuadooDelegate
{
	CTreeTestBinder* m_binder;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooDelegate)
	END_UNK_MAP
	CTreeTestDelegate(CTreeTestBinder* binder);
	~CTreeTestDelegate();
	STDMETHOD(Invoke)(IQuadooEventArgs* args);
};
class CTreeTestBinder : public CBaseUnknown, public IQuadooEventsBinder
{
public:
	UINT changes;
	UINT changing;
	UINT expanded;
	UINT collapsed;
	UINT checks;
	UINT edited;
	UINT keys;
	BOOL cancel;
	BOOL destroy;
	BOOL reenter;
	BOOL replaceLabel;
	BOOL destroyAfter;
	BOOL handleSpace;
	IQuadooTreeNodes* nodes;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooEventsBinder)
	END_UNK_MAP
	CTreeTestBinder() : changes(0), changing(0), expanded(0), collapsed(0), checks(0), edited(0), keys(0),
		cancel(FALSE), destroy(FALSE), reenter(FALSE), replaceLabel(FALSE), destroyAfter(FALSE), handleSpace(FALSE), nodes(NULL)
	{
	}
	STDMETHOD(BindEvent)(RSTRING, IQuadooDelegate** result)
	{
		*result = __new CTreeTestDelegate(this);
		return *result ? S_OK : E_OUTOFMEMORY;
	}
	STDMETHOD(PostBind)(IQuadooControl*)
	{
		return S_OK;
	}
};
CTreeTestDelegate::CTreeTestDelegate(CTreeTestBinder* binder) : m_binder(binder)
{
	m_binder->AddRef();
}
CTreeTestDelegate::~CTreeTestDelegate()
{
	m_binder->Release();
}
STDMETHODIMP CTreeTestDelegate::Invoke(IQuadooEventArgs* args)
{
	HRESULT hr;
	CRString event;
	TStackRef<IQuadooEventObjectArgs> objects;
	TStackRef<IUnknown> object;
	TStackRef<IQuadooTreeNode> node;
	TStackRef<IQuadooComponent> component;
	Check(args->GetEventName(&event));
	if(event == RSTRING_CAST(L"OnKeyDown"))
	{
		m_binder->keys++;
		if(m_binder->handleSpace)
		{
			Check(args->SetBool(1,TRUE));
		}
		hr = S_OK;
		goto Cleanup;
	}
	Check(args->QueryInterface(__uuidof(IQuadooEventObjectArgs),reinterpret_cast<PVOID*>(&objects)));
	BOOL before = event == RSTRING_CAST(L"OnChanging") || event == RSTRING_CAST(L"OnExpanding") ||
		event == RSTRING_CAST(L"OnCollapsing") || event == RSTRING_CAST(L"OnEditing") || event == RSTRING_CAST(L"OnEdited");
	Check(objects->GetObject(before ? 1 : 0,&object));
	if(object)
	{
		Check(object->QueryInterface(__uuidof(IQuadooTreeNode),reinterpret_cast<PVOID*>(&node)));
		REQUIRE(object->QueryInterface(__uuidof(IQuadooComponent),reinterpret_cast<PVOID*>(&component)) == E_NOINTERFACE);
	}
	if(event == RSTRING_CAST(L"OnChanging"))
	{
		m_binder->changing++;
	}
	else if(event == RSTRING_CAST(L"OnChange"))
	{
		m_binder->changes++;
	}
	else if(event == RSTRING_CAST(L"OnExpanded"))
	{
		m_binder->expanded++;
	}
	else if(event == RSTRING_CAST(L"OnCollapsed"))
	{
		m_binder->collapsed++;
	}
	else if(event == RSTRING_CAST(L"OnCheck"))
	{
		m_binder->checks++;
	}
	else if(event == RSTRING_CAST(L"OnEdited"))
	{
		m_binder->edited++;
		if(m_binder->replaceLabel)
		{
			Check(args->SetString(2,RSTRING_CAST(L"Delegate label")));
		}
	}
	if(before && m_binder->cancel)
	{
		Check(args->SetBool(0,TRUE));
	}
	if(m_binder->reenter && m_binder->nodes)
	{
		REQUIRE(m_binder->nodes->Clear() == QuadooForm::EInvalidState);
		TStackRef<IQuadooComponent> sender;
		TStackRef<IQuadooWinControl> window;
		HWND handle;
		Check(args->GetSender(&sender));
		Check(sender->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&window)));
		Check(window->GetHandle(&handle));
		HTREEITEM selected = TreeView_GetSelection(handle);
		REQUIRE(!TreeView_SelectItem(handle,TreeView_GetRoot(handle)) && TreeView_GetSelection(handle) == selected);
	}
	if(m_binder->destroy && (!m_binder->destroyAfter || !before))
	{
		Check(args->GetSender(&component));
		Check(component->Destroy());
	}
	hr = S_OK;
Cleanup:
	return hr;
}
#include "..\TestImages\TreeFixture.h"

static HRESULT TreeNative()
{
	HRESULT hr;
	TStackRef<CTreeTestBinder> binder;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONObject> child;
	TStackRef<IJSONObject> saved;
	TStackRef<IJSONObject> again;
	TStackRef<IJSONValue> wrapped;
	TStackRef<IJSONArray> children;
	TStackRef<IQuadooControl> root;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooControlContainer> container;
	TStackRef<IQuadooTreeView> tree;
	TStackRef<IQuadooTreeNodes> nodes;
	TStackRef<IQuadooTreeNode> first;
	TStackRef<IQuadooTreeNode> second;
	TStackRef<IQuadooTreeNode> nested;
	TStackRef<IQuadooTreeNode> added;
	TStackRef<IQuadooTreeNode> selected;
	TStackRef<IQuadooComponentHost> host;
	TStackRef<IQuadooComponentCollection> components;
	TStackRef<IQuadooComponent> images;
	TStackRef<IQuadooImageList> imageList;
	TStackRef<IQuadooImageListSnapshot> snapshot;
	TStackRef<IQuadooImageListSnapshot> scaled;
	TStackRef<IQuadooImageListSnapshot> rebuilt;
	TStackRef<IQuadooWinControl> native;
	TStackRef<IQuadooFormSerializer> serializer;
	GUID stable;
	UINT count;
	UINT index;
	UINT width;
	UINT height;
	UINT dpi;
	BOOL flag;
	HWND window;
	HIMAGELIST handle;
	HIMAGELIST newHandle;
	binder.Attach(__new CTreeTestBinder);
	CheckAlloc(binder);
	Check(QuadooGetControlRegistry(&registry));
	Check(Context(96,binder,&context,registry));
	Check(Parse(ImageListNativeDefinition,&definition));
	Check(Parse(TreeChildDefinition,&child));
	Check(JSONCreateArray(&children));
	Check(JSONWrapObject(child,&wrapped));
	Check(children->Add(wrapped));
	wrapped.Release();
	Check(JSONWrapArray(children,&wrapped));
	Check(definition->AddValueW(L"children",wrapped));
	Check(QuadooCreateControl(context,NULL,definition,&root));
	Check(root->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&container)));
	Check(container->FindControl(RSTRING_CAST(L"Tree1"),&control));
	Check(control->QueryInterface(__uuidof(IQuadooTreeView),reinterpret_cast<PVOID*>(&tree)));
	Check(control->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native)));
	Check(tree->GetNodes(&nodes));
	binder->nodes = nodes;
	Check(nodes->GetCount(&count));
	REQUIRE(count == 2);
	Check(nodes->GetAt(0,&first));
	Check(nodes->GetAt(1,&second));
	Check(first->GetID(&stable));
	Check(first->GetChild(0,&nested));
	Check(native->GetHandle(&window));
	REQUIRE(TreeView_GetCount(window) == 3);
	{
		TVITEMW item = {0};
		item.mask = TVIF_IMAGE | TVIF_SELECTEDIMAGE;
		item.hItem = TreeView_GetNextSibling(window,TreeView_GetRoot(window));
		REQUIRE(SendMessageW(window,TVM_GETITEMW,0,reinterpret_cast<LPARAM>(&item)));
		// Common controls store these indexes in 16-bit native fields.
		REQUIRE(static_cast<SHORT>(item.iImage) == I_IMAGENONE && static_cast<SHORT>(item.iSelectedImage) == I_IMAGENONE);
	}
	Check(tree->GetImages(&images));
	Check(images->QueryInterface(__uuidof(IQuadooImageList),reinterpret_cast<PVOID*>(&imageList)));
	Check(imageList->Acquire(96,&snapshot));
	Check(snapshot->GetHandle(&handle));
	REQUIRE(handle && TreeView_GetImageList(window,TVSIL_NORMAL) == handle && ImageList_GetImageCount(handle) == 2);
	Check(imageList->Acquire(144,&scaled));
	Check(scaled->GetSize(&width,&height,&dpi));
	REQUIRE(width == 24 && height == 24 && dpi == 144);
	REQUIRE(imageList->Acquire(0,&rebuilt) == E_INVALIDARG);
	Check(SetInt(images,L"Width",20));
	Check(imageList->Acquire(96,&rebuilt));
	Check(rebuilt->GetHandle(&newHandle));
	REQUIRE(newHandle != handle && ImageList_GetImageCount(handle) == 2);
	REQUIRE(TreeView_GetImageList(window,TVSIL_NORMAL) == newHandle);
	{
		CImageTestCanvas canvas;
		Check(canvas.Create(40,40));
		canvas.Fill(RGB(17,33,49));
		DWORD empty = canvas.Hash();
		REQUIRE(ImageList_Draw(handle,0,canvas.DC(),0,0,ILD_TRANSPARENT));
		REQUIRE(canvas.Hash() != empty && canvas.Pixel(0,0) == RGB(17,33,49));
		TStackRef<IQuadooLayoutControl> layout;
		QuadooForm::Rect bounds;
		Check(control->QueryInterface(__uuidof(IQuadooLayoutControl),reinterpret_cast<PVOID*>(&layout)));
		Check(control->GetBounds(&bounds));
		Check(layout->Arrange(&bounds,192));
		HIMAGELIST dpiImages = TreeView_GetImageList(window,TVSIL_NORMAL);
		INT cx;
		INT cy;
		REQUIRE(ImageList_GetIconSize(dpiImages,&cx,&cy) && cx == 40 && cy == 32);
		Check(layout->Arrange(&bounds,96));
		REQUIRE(TreeView_GetImageList(window,TVSIL_NORMAL) == newHandle);
	}
	Check(nodes->Insert(NULL,0,RSTRING_CAST(L"Inserted"),&added));
	Check(first->GetIndex(&index));
	REQUIRE(index == 1);
	Check(nodes->Move(first,NULL,2));
	Check(first->GetIndex(&index));
	REQUIRE(index == 2);
	REQUIRE(nodes->Move(first,nested,0) == E_INVALIDARG);
	selected.Release();
	Check(nodes->Find(stable,&selected));
	REQUIRE(selected == static_cast<IQuadooTreeNode*>(first));
	selected.Release();
	binder->cancel = TRUE;
	REQUIRE(tree->SetSelected(first) == S_FALSE);
	Check(tree->GetSelected(&selected));
	REQUIRE(!selected && binder->changes == 0);
	binder->cancel = FALSE;
	binder->reenter = TRUE;
	Check(tree->SetSelected(first));
	REQUIRE(binder->changes == 1);
	{
		TStackRef<IQuadooProperties> properties;
		CRString selectedID;
		GUID parsed;
		Check(Property(control,L"SelectedNode",&properties,&index));
		Check(properties->GetValueString(index,&selectedID));
		CheckIf(FAILED(CLSIDFromString(const_cast<PWSTR>(selectedID.c_wstr()),&parsed)),E_FAIL);
		REQUIRE(IsEqualGUID(parsed,stable));
	}
	REQUIRE(tree->SetSelected(first) == S_FALSE && binder->changes == 1);
	Check(first->SetExpanded(TRUE));
	Check(first->GetExpanded(&flag));
	REQUIRE(flag && binder->expanded == 1);
	Check(first->SetExpanded(FALSE));
	REQUIRE(binder->collapsed == 1);
	Check(nested->SetChecked(TRUE));
	Check(nested->GetChecked(&flag));
	REQUIRE(flag && binder->checks == 1);
	binder->reenter = FALSE;
	{
		Check(tree->SetSelected(nested));
		Check(first->GetExpanded(&flag));
		REQUIRE(flag);
		Check(first->SetExpanded(FALSE));
		selected.Release();
		Check(tree->GetSelected(&selected));
		REQUIRE(selected == static_cast<IQuadooTreeNode*>(first));
		Check(first->GetExpanded(&flag));
		REQUIRE(!flag);
		Check(native->GetHandle(&window));
		UINT expanded = binder->expanded;
		SendMessageW(window,WM_KEYDOWN,VK_RIGHT,0);
		Check(first->GetExpanded(&flag));
		REQUIRE(flag && binder->expanded == expanded + 1);
		UINT changed = binder->changes;
		SendMessageW(window,WM_KEYDOWN,VK_DOWN,0);
		REQUIRE(binder->changes == changed + 1);
		TStackRef<IQuadooForm> form;
		Check(root->QueryInterface(__uuidof(IQuadooForm),reinterpret_cast<PVOID*>(&form)));
		Check(form->ShowForm(SW_SHOWNOACTIVATE));
		binder->cancel = TRUE;
		REQUIRE(tree->BeginLabelEdit(first) == S_FALSE);
		binder->cancel = FALSE;
		Check(tree->BeginLabelEdit(first));
		HWND edit = TreeView_GetEditControl(window);
		REQUIRE(edit != NULL);
		SetWindowTextW(edit,L"Cancelled label");
		binder->cancel = TRUE;
		TreeView_EndEditLabelNow(window,FALSE);
		CRString text;
		Check(first->GetText(&text));
		REQUIRE(text == RSTRING_CAST(L"Root"));
		binder->cancel = FALSE;
		binder->replaceLabel = TRUE;
		Check(tree->BeginLabelEdit(first));
		edit = TreeView_GetEditControl(window);
		REQUIRE(edit != NULL);
		SetWindowTextW(edit,L"Native label");
		TreeView_EndEditLabelNow(window,FALSE);
		text = NULL;
		Check(first->GetText(&text));
		REQUIRE(text == RSTRING_CAST(L"Delegate label") && binder->edited == 2);
		Check(native->GetHandle(&window));
		SendMessageW(window,WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA),0);
		Check(form->HideForm());
	}
	{
		TStackRef<IQuadooProperties> properties;
		Check(Property(control,L"OnKeyDown",&properties,&index));
		Check(properties->SetValueEvent(index,RSTRING_CAST(L"Event")));
		Check(tree->SetSelected(first));
		BOOL previous;
		Check(first->GetChecked(&previous));
		Check(native->GetHandle(&window));
		binder->handleSpace = TRUE;
		SendMessageW(window,WM_KEYDOWN,VK_SPACE,0);
		Check(first->GetChecked(&flag));
		REQUIRE(flag == previous && binder->keys == 1);
		binder->handleSpace = FALSE;
		SendMessageW(window,WM_KEYDOWN,VK_SPACE,0);
		Check(first->GetChecked(&flag));
		REQUIRE(flag != previous && binder->keys == 2);
	}
	Check(nodes->Move(nested,NULL,0));
	Check(nested->SetText(RSTRING_CAST(L"Moved child")));
	Check(QuadooGetFormSerializer(&serializer));
	Check(serializer->Serialize(root,&saved));
	Check(tree->SetSelected(nested));
	Check(nodes->Delete(nested));
	Check(nested->GetAttached(&flag));
	REQUIRE(!flag);
	{
		CRString text;
		REQUIRE(nested->GetText(&text) == QuadooForm::EDestroyed);
	}
	Check(root->Destroy());
	Check(first->GetAttached(&flag));
	REQUIRE(!flag && !IsWindow(window));
	REQUIRE(nodes->GetCount(&count) == QuadooForm::EDestroyed);
	REQUIRE(ImageList_GetImageCount(handle) == 2);
	native.Release();
	tree.Release();
	control.Release();
	container.Release();
	root.Release();
	Check(QuadooCreateControl(context,NULL,saved,&root));
	Check(serializer->Serialize(root,&again));
	REQUIRE(JSONCompareObjects(saved,again) == 0);
	Check(root->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&container)));
	Check(container->FindControl(RSTRING_CAST(L"Tree1"),&control));
	Check(control->QueryInterface(__uuidof(IQuadooTreeView),reinterpret_cast<PVOID*>(&tree)));
	selected.Release();
	Check(tree->GetSelected(&selected));
	REQUIRE(selected != NULL);
	binder->destroy = TRUE;
	REQUIRE(tree->SetSelected(NULL) == QuadooForm::EDestroyed);
	Check(root->Destroy());
	binder->destroy = FALSE;
	native.Release();
	tree.Release();
	control.Release();
	container.Release();
	root.Release();
	Check(QuadooCreateControl(context,NULL,saved,&root));
	Check(root->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&container)));
	Check(container->FindControl(RSTRING_CAST(L"Tree1"),&control));
	Check(control->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native)));
	Check(native->GetHandle(&window));
	binder->destroy = TRUE;
	binder->destroyAfter = TRUE;
	HTREEITEM next = TreeView_GetRoot(window);
	if(next == TreeView_GetSelection(window))
	{
		next = TreeView_GetNextSibling(window,next);
	}
	REQUIRE(next != NULL);
	TreeView_SelectItem(window,next);
	REQUIRE(!IsWindow(window));
	Check(root->Destroy());
	binder->nodes = NULL;
Cleanup:
	if(root)
	{
		root->Destroy();
	}
	return hr;
}

static HRESULT TreeResources()
{
	HRESULT hr;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooComponentRegistry> components;
	TStackRef<IQuadooControlFactory> treeFactory;
	TStackRef<IQuadooComponentFactory> imageFactory;
	TStackRef<IQuadooFactoryLifetime> treeLifetime;
	TStackRef<IQuadooFactoryLifetime> imageLifetime;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONObject> child;
	TStackRef<IJSONArray> children;
	TStackRef<IJSONValue> value;
	CGuiResourceMonitor monitor(L"TreeView / native ImageList");
	Check(QuadooGetControlRegistry(&registry));
	Check(registry->FindFactory(RSTRING_CAST(L"TreeView"),&treeFactory));
	Check(treeFactory->QueryInterface(__uuidof(IQuadooFactoryLifetime),reinterpret_cast<PVOID*>(&treeLifetime)));
	Check(registry->QueryInterface(__uuidof(IQuadooComponentRegistry),reinterpret_cast<PVOID*>(&components)));
	Check(components->FindComponentFactory(RSTRING_CAST(L"ImageList"),&imageFactory));
	Check(imageFactory->QueryInterface(__uuidof(IQuadooFactoryLifetime),reinterpret_cast<PVOID*>(&imageLifetime)));
	Check(Parse(ImageListNativeDefinition,&definition));
	Check(Parse(TreeChildDefinition,&child));
	Check(JSONCreateArray(&children));
	Check(JSONWrapObject(child,&value));
	Check(children->Add(value));
	value.Release();
	Check(JSONWrapArray(children,&value));
	Check(definition->AddValueW(L"children",value));
	while(!monitor.Complete())
	{
		TStackRef<IQuadooTreeNode> retained;
		TStackRef<IQuadooImageListSnapshot> retainedImages;
		UINT treeLive;
		UINT imageLive;
		{
			TStackRef<IQuadooConstructionContext> context;
			TStackRef<IQuadooControl> root;
			TStackRef<IQuadooControl> control;
			TStackRef<IQuadooControlContainer> container;
			TStackRef<IQuadooTreeView> tree;
			TStackRef<IQuadooTreeNodes> nodes;
			TStackRef<IQuadooTreeNode> second;
			TStackRef<IQuadooComponent> component;
			TStackRef<IQuadooImageList> images;
			TStackRef<IQuadooImageListSnapshot> unavailable;
			TStackRef<IQuadooWinControl> native;
			TStackRef<IQuadooLayoutControl> layout;
			TStackRef<IUnknown> identity;
			TStackRef<IUnknown> capabilityIdentity;
			QuadooForm::Rect bounds;
			HWND window;
			Check(Context(96,NULL,&context,registry,QuadooForm::Preview));
			Check(QuadooCreateControl(context,NULL,definition,&root));
			Check(root->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&container)));
			Check(container->FindControl(RSTRING_CAST(L"Tree1"),&control));
			Check(control->QueryInterface(__uuidof(IQuadooTreeView),reinterpret_cast<PVOID*>(&tree)));
			Check(control->QueryInterface(IID_IUnknown,reinterpret_cast<PVOID*>(&identity)));
			Check(tree->QueryInterface(IID_IUnknown,reinterpret_cast<PVOID*>(&capabilityIdentity)));
			REQUIRE(identity == static_cast<IUnknown*>(capabilityIdentity));
			Check(tree->GetNodes(&nodes));
			Check(nodes->GetAt(0,&retained));
			Check(nodes->GetAt(1,&second));
			Check(tree->SetSelected(retained));
			Check(retained->SetExpanded(TRUE));
			Check(nodes->Move(second,retained,1));
			Check(retained->SetChecked(TRUE));
			Check(tree->GetImages(&component));
			Check(component->QueryInterface(__uuidof(IQuadooImageList),reinterpret_cast<PVOID*>(&images)));
			Check(images->Acquire(96,&retainedImages));
			Check(control->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native)));
			Check(control->QueryInterface(__uuidof(IQuadooLayoutControl),reinterpret_cast<PVOID*>(&layout)));
			Check(control->GetBounds(&bounds));
			for(UINT dpi = 96; dpi <= 192; dpi += 48)
			{
				Check(layout->Arrange(&bounds,dpi));
			}
			Check(native->GetHandle(&window));
			Check(component->Destroy());
			REQUIRE(TreeView_GetImageList(window,TVSIL_NORMAL) == NULL);
			{
				TStackRef<IQuadooProperties> properties;
				TStackRef<IJSONValue> reference;
				UINT index;
				Check(Property(control,L"Images",&properties,&index));
				Check(properties->GetValueJSON(index,&reference));
				REQUIRE(!reference);
			}
			REQUIRE(images->Acquire(96,&unavailable) == QuadooForm::EDestroyed);
			Check(root->Destroy());
			REQUIRE(!IsWindow(window));
		}
		Check(treeLifetime->GetLiveCount(&treeLive));
		Check(imageLifetime->GetLiveCount(&imageLive));
		REQUIRE(treeLive != 0 && imageLive != 0);
		REQUIRE(registry->UnregisterFactory(RSTRING_CAST(L"TreeView")) == HRESULT_FROM_WIN32(ERROR_BUSY));
		REQUIRE(components->UnregisterComponentFactory(RSTRING_CAST(L"ImageList")) == HRESULT_FROM_WIN32(ERROR_BUSY));
		retained.Release();
		retainedImages.Release();
		Check(treeLifetime->GetLiveCount(&treeLive));
		Check(imageLifetime->GetLiveCount(&imageLive));
		REQUIRE(treeLive == 0 && imageLive == 0);
		Check(monitor.AfterCycle());
	}
Cleanup:
	return hr;
}
