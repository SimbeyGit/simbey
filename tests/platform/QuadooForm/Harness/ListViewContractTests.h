#include "..\ListViewCompatibility\ListViewContract.h"

class CListProbeObserver : public CBaseUnknown, public IQuadooImageListObserver
{
public:
	UINT changes;
	BOOL destroyed;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooImageListObserver)
	END_UNK_MAP
	CListProbeObserver() : changes(0), destroyed(FALSE)
	{
	}
	STDMETHOD_(VOID, OnImageListChanged)(IQuadooImageList* images)
	{
		TStackRef<IQuadooImageListSnapshot> snapshot;
		changes++;
		destroyed = images->Acquire(96,&snapshot) == QuadooForm::EDestroyed;
	}
};

static HRESULT ListViewManagedImages()
{
	HRESULT hr;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooComponentRegistry> factories;
	TStackRef<IQuadooComponentFactory> factory;
	TStackRef<IQuadooFactoryLifetime> lifetime;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IJSONObject> definition;
	TStackRef<IQuadooControl> root;
	TStackRef<IQuadooComponentHost> host;
	TStackRef<IQuadooComponentCollection> components;
	TStackRef<IQuadooComponent> component;
	TStackRef<IQuadooImageList> images;
	TStackRef<IQuadooImageListSnapshot> snapshots[3];
	TStackRef<IQuadooImageListSnapshot> replacement;
	TStackRef<IQuadooImageListSnapshot> dead;
	TStackRef<CListProbeObserver> observer;
	HIMAGELIST handles[3] = {NULL,NULL,NULL};
	HIMAGELIST rebuilt = NULL;
	HWND parent = NULL;
	HWND first = NULL;
	HWND second = NULL;
	UINT baseline;
	UINT live;
	INITCOMMONCONTROLSEX controls = {sizeof(controls),ICC_LISTVIEW_CLASSES};
	REQUIRE(InitCommonControlsEx(&controls));
	REQUIRE(!IsEqualGUID(__uuidof(IQuadooListItem),__uuidof(IQuadooTreeNode)));
	REQUIRE(!IsEqualGUID(__uuidof(IQuadooListView),__uuidof(IQuadooListItems)));
	REQUIRE(!IsEqualGUID(__uuidof(IQuadooListColumn),__uuidof(IQuadooListColumns)));
	Check(QuadooGetControlRegistry(&registry));
	Check(registry->QueryInterface(&factories));
	Check(factories->FindComponentFactory(RSTRING_CAST(L"ImageList"),&factory));
	Check(factory->QueryInterface(&lifetime));
	Check(lifetime->GetLiveCount(&baseline));
	Check(Context(96,NULL,&context,registry));
	Check(Parse(ImageListNativeDefinition,&definition));
	Check(QuadooCreateControl(context,NULL,definition,&root));
	Check(root->QueryInterface(&host));
	Check(host->GetComponents(&components));
	Check(components->FindComponent(RSTRING_CAST(L"ImageList1"),&component));
	Check(component->QueryInterface(&images));
	for(UINT n = 0; n < ARRAYSIZE(snapshots); n++)
	{
		UINT width;
		UINT height;
		UINT dpi;
		Check(images->Acquire(96 + n * 48,&snapshots[n]));
		Check(snapshots[n]->GetHandle(&handles[n]));
		Check(snapshots[n]->GetSize(&width,&height,&dpi));
		REQUIRE(width == 16 + n * 8 && height == width && dpi == 96 + n * 48);
		REQUIRE(ImageList_GetImageCount(handles[n]) == 2);
	}
	observer.Attach(__new CListProbeObserver);
	CheckAlloc(observer);
	Check(images->AddImageListObserver(observer));
	parent = CreateWindowExW(0,L"STATIC",L"Managed ListView images",WS_OVERLAPPED,
		0,0,400,200,NULL,NULL,GetModuleHandleW(NULL),NULL);
	REQUIRE(parent != NULL);
	first = CreateWindowExW(0,WC_LISTVIEWW,L"",WS_CHILD | LVS_REPORT | LVS_SHAREIMAGELISTS,
		0,0,200,200,parent,NULL,GetModuleHandleW(NULL),NULL);
	second = CreateWindowExW(0,WC_LISTVIEWW,L"",WS_CHILD | LVS_ICON | LVS_SHAREIMAGELISTS,
		0,0,200,200,parent,NULL,GetModuleHandleW(NULL),NULL);
	REQUIRE(first != NULL && second != NULL);
	ListView_SetImageList(first,handles[0],LVSIL_SMALL);
	ListView_SetImageList(first,handles[0],LVSIL_STATE);
	ListView_SetImageList(second,handles[0],LVSIL_NORMAL);
	ListView_SetImageList(second,handles[0],LVSIL_SMALL);
	Check(SetInt(component,L"Width",24));
	REQUIRE(observer->changes == 1 && !observer->destroyed);
	Check(images->Acquire(96,&replacement));
	Check(replacement->GetHandle(&rebuilt));
	REQUIRE(rebuilt != handles[0] && ImageList_GetImageCount(handles[0]) == 2);
	REQUIRE(ListView_SetImageList(first,rebuilt,LVSIL_SMALL) == handles[0]);
	REQUIRE(ListView_SetImageList(second,rebuilt,LVSIL_NORMAL) == handles[0]);
	REQUIRE(DestroyWindow(first));
	first = NULL;
	REQUIRE(ImageList_GetImageCount(handles[0]) == 2 && ImageList_GetImageCount(rebuilt) == 2);
	Check(component->Destroy());
	REQUIRE(observer->destroyed && observer->changes == 2);
	REQUIRE(images->Acquire(96,&dead) == QuadooForm::EDestroyed && !dead);
	ListView_SetImageList(second,NULL,LVSIL_NORMAL);
	ListView_SetImageList(second,NULL,LVSIL_SMALL);
	REQUIRE(DestroyWindow(second));
	second = NULL;
	Check(root->Destroy());
	component.Release();
	images.Release();
	components.Release();
	host.Release();
	root.Release();
	context.Release();
	Check(lifetime->GetLiveCount(&live));
	REQUIRE(live > baseline);
	REQUIRE(ImageList_GetImageCount(handles[0]) == 2 && ImageList_GetImageCount(rebuilt) == 2);
	replacement.Release();
	for(UINT n = 0; n < ARRAYSIZE(snapshots); n++)
	{
		snapshots[n].Release();
	}
	Check(lifetime->GetLiveCount(&live));
	REQUIRE(live == baseline);
Cleanup:
	if(parent)
	{
		DestroyWindow(parent);
	}
	if(images && observer)
	{
		images->RemoveImageListObserver(observer);
	}
	if(root)
	{
		root->Destroy();
	}
	return hr;
}
