#include "..\..\..\..\platform\QuadooForm\QuadooForm\TreeView.h"
#include "..\TestImages\ImageListFixture.h"

static BOOL s_failTreeNative;
class CFailTreeView : public QF::CTreeView
{
	virtual HRESULT NativeCreated()
	{
		HRESULT hr;
		Check(QF::CTreeView::NativeCreated());
		CheckIf(s_failTreeNative,E_OUTOFMEMORY);
	Cleanup:
		return hr;
	}
public:
	CFailTreeView(QF::CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) :
		CTreeView(factory,context,parent)
	{
	}
	static const QF::TypeSpec* Type()
	{
		static const QF::TypeSpec type = {L"FailTreeView",QF::CTreeView::Type(),NULL,0,NULL,0,0};
		return &type;
	}
};
class CImageListReentryProbe : public CBaseUnknown, public IQuadooImageListObserver
{
public:
	IQuadooComponent* component;
	HRESULT edit;
	UINT calls;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooImageListObserver)
	END_UNK_MAP
	CImageListReentryProbe() : component(NULL), edit(S_OK), calls(0)
	{
	}
	STDMETHOD_(VOID, OnImageListChanged)(IQuadooImageList*)
	{
		calls++;
		edit = ImageNumber(component,L"Width",32);
	}
};
static HRESULT TreeRollbackTests()
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
	TStackRef<IQuadooControlContainer> form;
	TStackRef<IQuadooTreeView> tree;
	TStackRef<IQuadooTreeNodes> nodes;
	TStackRef<IQuadooTreeNode> first;
	TStackRef<IQuadooTreeNode> second;
	TStackRef<IQuadooTreeNode> failed;
	TStackRef<IQuadooFormSerializer> serializer;
	TStackRef<IQuadooComponent> component;
	TStackRef<IQuadooComponentHost> host;
	TStackRef<IQuadooComponentCollection> components;
	TStackRef<IQuadooImageList> images;
	TStackRef<IQuadooImageListSnapshot> snapshot;
	TStackRef<IQuadooImageListSnapshot> invalid;
	TStackRef<IQuadooWinControl> window;
	QuadooForm::ContextOptions options = {0};
	UINT count;
	HIMAGELIST original;
	HIMAGELIST current;
	HWND originalWindow;
	HWND currentWindow;
	Check(QuadooGetControlRegistry(&registry));
	factory.Attach(__new QF::TFactory<CFailTreeView>);
	CheckAlloc(factory);
	Check(factory->Initialize());
	Check(registry->RegisterFactory(factory));
	options.cbSize = sizeof(options);
	options.DPI = 96;
	options.pRegistry = registry;
	Check(QuadooCreateConstructionContext(&options,&context));
	Check(NativeDefinition(ImageListNativeDefinition,&definition));
	Check(QuadooCreateControl(context,NULL,definition,&root));
	Check(root->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&form)));
	definition.Release();
	Check(NativeDefinition(L"{\"type\":\"FailTreeView\",\"name\":\"Tree\",\"properties\":{\"CheckBoxes\":true,\"Items\":[{\"Text\":\"First\"},{\"Text\":\"Second\"}]}}",&definition));
	Check(QuadooCreateControl(context,form,definition,&control));
	Check(control->QueryInterface(__uuidof(IQuadooTreeView),reinterpret_cast<PVOID*>(&tree)));
	Check(control->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&window)));
	Check(tree->GetNodes(&nodes));
	Check(nodes->GetAt(0,&first));
	Check(nodes->GetAt(1,&second));
	Check(QuadooGetFormSerializer(&serializer));
	Check(serializer->Serialize(root,&before));
	Check(window->GetHandle(&originalWindow));
	s_failTreeNative = TRUE;
	VERIFY(nodes->Insert(NULL,0,RSTRING_CAST(L"Failed"),&failed) == E_OUTOFMEMORY && !failed);
	VERIFY(nodes->Move(first,second,0) == E_OUTOFMEMORY);
	VERIFY(nodes->Delete(second) == E_OUTOFMEMORY);
	VERIFY(first->SetText(RSTRING_CAST(L"Failed rename")) == E_OUTOFMEMORY);
	VERIFY(nodes->Clear() == E_OUTOFMEMORY);
	s_failTreeNative = FALSE;
	Check(window->GetHandle(&currentWindow));
	VERIFY(currentWindow == originalWindow && TreeView_GetCount(currentWindow) == 2);
	Check(serializer->Serialize(root,&after));
	VERIFY(JSONCompareObjects(before,after) == 0);
	Check(nodes->GetCount(&count));
	VERIFY(count == 2);
	Check(nodes->Move(first,NULL,1));
	Check(root->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&host)));
	Check(host->GetComponents(&components));
	Check(components->FindComponent(RSTRING_CAST(L"ImageList1"),&component));
	Check(component->QueryInterface(__uuidof(IQuadooImageList),reinterpret_cast<PVOID*>(&images)));
	Check(images->Acquire(96,&snapshot));
	Check(snapshot->GetHandle(&original));
	QFI::FailAllocationAfter(0);
	VERIFY(ImageNumber(component,L"Width",24) == E_OUTOFMEMORY);
	QFI::FailAllocationAfter(-1);
	Check(images->Acquire(96,&invalid));
	Check(invalid->GetHandle(&current));
	VERIFY(current == original);
	invalid.Release();
	QFI::FailAllocationAfter(0);
	VERIFY(images->Acquire(144,&invalid) == E_OUTOFMEMORY && !invalid);
	QFI::FailAllocationAfter(-1);
	Check(images->Acquire(144,&invalid));
	{
		TStackRef<CImageListReentryProbe> observer;
		observer.Attach(__new CImageListReentryProbe);
		CheckAlloc(observer);
		observer->component = component;
		Check(images->AddImageListObserver(observer));
		Check(ImageNumber(component,L"Width",20));
		VERIFY(observer->calls == 1 && observer->edit == QuadooForm::EInvalidState);
		Check(images->RemoveImageListObserver(observer));
	}
	Check(root->Destroy());
	VERIFY(ImageList_GetImageCount(original) == 2);
	hr = S_OK;
Cleanup:
	s_failTreeNative = FALSE;
	QFI::FailAllocationAfter(-1);
	if(root)
	{
		root->Destroy();
	}
	return hr;
}
