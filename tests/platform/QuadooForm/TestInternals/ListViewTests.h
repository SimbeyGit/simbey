#include "..\..\..\..\platform\QuadooForm\QuadooForm\ListView.h"
#include "..\TestImages\ListViewExternal.h"

static BOOL s_failListNative;
static UINT s_failListMessage;
class CListErrorReport : public CBaseUnknown, public IQuadooConstructionReport
{
public:
	UINT calls;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooConstructionReport)
	END_UNK_MAP
	CListErrorReport() : calls(0)
	{
	}
	STDMETHOD(OnError)(HRESULT error, RSTRING path, RSTRING detail)
	{
		calls++;
		return S_OK;
	}
};
class CFailListView : public QF::CListView
{
	virtual LRESULT Message(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled)
	{
		if(s_failListMessage && message == s_failListMessage)
		{
			s_failListMessage = 0;
			*handled = TRUE;
			return message == LVM_INSERTITEMW || message == LVM_INSERTCOLUMNW ? -1 : FALSE;
		}
		return CListView::Message(window,message,wp,lp,content,handled);
	}
	virtual HRESULT NativeCreated()
	{
		HRESULT hr;
		Check(QF::CListView::NativeCreated());
		CheckIf(s_failListNative,E_OUTOFMEMORY);
	Cleanup:
		return hr;
	}
public:
	CFailListView(QF::CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) :
		CListView(factory,context,parent)
	{
	}
	static const QF::TypeSpec* Type()
	{
		static const QF::TypeSpec type = {L"FailListView",QF::CListView::Type(),NULL,0,NULL,0,0};
		return &type;
	}
};

static HRESULT ListViewRollbackTests()
{
	HRESULT hr;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<QF::CFactory> factory;
	TStackRef<CListErrorReport> report;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONObject> before;
	TStackRef<IJSONObject> after;
	TStackRef<IQuadooControl> root;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooControlContainer> form;
	TStackRef<IQuadooWinControl> native;
	TStackRef<IQuadooLayoutControl> layout;
	TStackRef<IQuadooListView> view;
	TStackRef<IQuadooListColumns> columns;
	TStackRef<IQuadooListColumn> first;
	TStackRef<IQuadooListColumn> second;
	TStackRef<IQuadooListColumn> failed;
	TStackRef<IQuadooFormSerializer> serializer;
	TStackRef<IQuadooComponentHost> host;
	TStackRef<IQuadooComponentCollection> components;
	TStackRef<IQuadooComponent> images;
	TStackRef<IQuadooImageList> imageList;
	TStackRef<IQuadooImageListSnapshot> snapshot;
	TStackRef<CListExternalSource> external;
	TStackRef<IQuadooComponent> assigned;
	TStackRef<IQuadooProperties> properties;
	TStackRef<IJSONValue> value;
	QuadooForm::ContextOptions options = {0};
	HWND original;
	HWND current;
	HFONT originalFont;
	HIMAGELIST originalImages;
	UINT index;
	UINT count;
	Check(QuadooGetControlRegistry(&registry));
	factory.Attach(__new QF::TFactory<CFailListView>);
	CheckAlloc(factory);
	Check(factory->Initialize());
	Check(registry->RegisterFactory(factory));
	options.cbSize = sizeof(options);
	report.Attach(__new CListErrorReport);
	CheckAlloc(report);
	options.pReport = report;
	options.DPI = 96;
	options.pRegistry = registry;
	Check(QuadooCreateConstructionContext(&options,&context));
	Check(NativeDefinition(ImageListNativeDefinition,&definition));
	Check(QuadooCreateControl(context,NULL,definition,&root));
	Check(root->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&form)));
	definition.Release();
	Check(NativeDefinition(L"{\"type\":\"FailListView\",\"name\":\"List\",\"properties\":{\"Columns\":[{\"Caption\":\"First\"},{\"Caption\":\"Second\"}]}}",&definition));
	Check(QuadooCreateControl(context,form,definition,&control));
	Check(control->QueryInterface(__uuidof(IQuadooListView),reinterpret_cast<PVOID*>(&view)));
	Check(control->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native)));
	Check(control->QueryInterface(__uuidof(IQuadooLayoutControl),reinterpret_cast<PVOID*>(&layout)));
	Check(view->GetColumns(&columns));
	Check(columns->GetAt(0,&first));
	Check(columns->GetAt(1,&second));
	Check(native->GetHandle(&original));
	originalFont = reinterpret_cast<HFONT>(SendMessageW(original,WM_GETFONT,0,0));
	Check(QuadooGetFormSerializer(&serializer));
	Check(serializer->Serialize(root,&before));
	s_failListNative = TRUE;
	VERIFY(columns->Insert(0,RSTRING_CAST(L"Failed"),&failed) == E_OUTOFMEMORY && !failed);
	VERIFY(columns->Delete(second) == E_OUTOFMEMORY);
	VERIFY(columns->Move(first,1) == E_OUTOFMEMORY);
	VERIFY(columns->Clear() == E_OUTOFMEMORY);
	VERIFY(first->SetCaption(RSTRING_CAST(L"Failed rename")) == E_OUTOFMEMORY);
	VERIFY(first->SetWidth(250) == E_OUTOFMEMORY);
	VERIFY(first->SetDisplayIndex(1) == E_OUTOFMEMORY);
	VERIFY(second->SetAlignment(QuadooForm::ListColumnRight) == E_OUTOFMEMORY);
	VERIFY(ImageNumber(control,L"ViewStyle",QuadooForm::ListViewIcon) == E_OUTOFMEMORY);
	Check(ImageProperty(control,L"CheckBoxes",&properties,&index));
	VERIFY(properties->SetValueBool(index,TRUE) == E_OUTOFMEMORY);
	properties.Release();
	Check(ImageProperty(control,L"OwnerData",&properties,&index));
	VERIFY(properties->SetValueBool(index,TRUE) == E_OUTOFMEMORY);
	properties.Release();
	Check(ImageProperty(control,L"Columns",&properties,&index));
	Check(JSONParse(NULL,SLP(L"[{\"Caption\":\"Replacement\"}]"),&value));
	VERIFY(properties->SetValueJSON(index,value) == E_OUTOFMEMORY);
	{
		QuadooForm::Rect bounds;
		Check(control->GetBounds(&bounds));
		VERIFY(layout->Arrange(&bounds,144) == E_OUTOFMEMORY);
		VERIFY(QF::ControlDPI(control) == 96);
	}
	s_failListNative = FALSE;
	Check(native->GetHandle(&current));
	VERIFY(current == original && IsWindow(original));
	VERIFY(reinterpret_cast<HFONT>(SendMessageW(current,WM_GETFONT,0,0)) == originalFont);
	VERIFY(Header_GetItemCount(ListView_GetHeader(current)) == 2);
	Check(columns->GetCount(&count));
	VERIFY(count == 2);
	Check(serializer->Serialize(root,&after));
	VERIFY(JSONCompareObjects(before,after) == 0);
	after.Release();
	value.Release();
	Check(JSONParse(NULL,SLP(L"[{\"DisplayIndex\":0},{\"DisplayIndex\":0}]"),&value));
	VERIFY(properties->SetValueJSON(index,value) == E_INVALIDARG);
	value.Release();
	Check(JSONParse(NULL,SLP(L"[{\"Width\":-1}]"),&value));
	VERIFY(properties->SetValueJSON(index,value) == E_INVALIDARG);
	value.Release();
	Check(JSONParse(NULL,SLP(L"[{\"Caption\":\"a\\u0000b\"}]"),&value));
	VERIFY(properties->SetValueJSON(index,value) == E_INVALIDARG);
	value.Release();
	Check(JSONParse(NULL,SLP(L"[{\"id\":\"{11223344-0000-0000-0000-000000000001}\"},{\"id\":\"{11223344-0000-0000-0000-000000000001}\"}]"),&value));
	VERIFY(properties->SetValueJSON(index,value) == QuadooForm::EDuplicateID);
	Check(serializer->Serialize(root,&after));
	VERIFY(JSONCompareObjects(before,after) == 0);
	Check(root->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&host)));
	Check(host->GetComponents(&components));
	Check(components->FindComponent(RSTRING_CAST(L"ImageList1"),&images));
	Check(images->QueryInterface(__uuidof(IQuadooImageList),reinterpret_cast<PVOID*>(&imageList)));
	Check(imageList->Acquire(96,&snapshot));
	Check(snapshot->GetHandle(&originalImages));
	Check(view->SetImages(QuadooForm::ListSmallImages,images));
	external.Attach(__new CListExternalSource(components,images,imageList));
	CheckAlloc(external);
	Check(external->Initialize());
	Check(components->RegisterComponent(external));
	external->acquireResult = E_OUTOFMEMORY;
	VERIFY(view->SetImages(QuadooForm::ListSmallImages,external) == E_OUTOFMEMORY);
	VERIFY(external->Observers() == 0 && ListView_GetImageList(original,LVSIL_SMALL) == originalImages);
	external->acquireResult = S_OK;
	external->subscribeResult = E_ABORT;
	VERIFY(view->SetImages(QuadooForm::ListSmallImages,external) == E_ABORT);
	VERIFY(external->Observers() == 0);
	external->subscribeThenFail = TRUE;
	VERIFY(view->SetImages(QuadooForm::ListSmallImages,external) == E_ABORT);
	VERIFY(external->Observers() == 0);
	Check(view->GetImages(QuadooForm::ListSmallImages,&assigned));
	VERIFY(assigned == static_cast<IQuadooComponent*>(images));
	assigned.Release();
	external->subscribeResult = S_OK;
	external->destroyControl = control;
	Check(view->SetImages(QuadooForm::ListSmallImages,external));
	VERIFY(external->destroyResult == QuadooForm::EInvalidState);
	external->destroyControl = NULL;
	Check(view->SetImages(QuadooForm::ListLargeImages,external));
	Check(view->SetImages(QuadooForm::ListStateImages,external));
	VERIFY(external->Observers() == 3 && ListView_GetImageList(original,LVSIL_SMALL) == originalImages);
	external->destroyControl = control;
	Check(view->Refresh());
	VERIFY(external->destroyResult == QuadooForm::EInvalidState);
	external->destroyControl = NULL;
	{
		QuadooForm::Rect bounds;
		HFONT font = reinterpret_cast<HFONT>(SendMessageW(original,WM_GETFONT,0,0));
		Check(control->GetBounds(&bounds));
		external->acquireResult = E_OUTOFMEMORY;
		VERIFY(layout->Arrange(&bounds,144) == E_OUTOFMEMORY);
		external->acquireResult = S_OK;
		Check(native->GetHandle(&current));
		VERIFY(current == original && QF::ControlDPI(control) == 96);
		VERIFY(reinterpret_cast<HFONT>(SendMessageW(current,WM_GETFONT,0,0)) == font);
		VERIFY(ListView_GetImageList(current,LVSIL_SMALL) == originalImages);
		VERIFY(report->calls == 0);
	}
	{
		TStackRef<IJSONValue> reference;
		TStackRef<IQuadooProperties> values;
		GUID id;
		CRString text;
		UINT property;
		Check(external->GetID(&id));
		Check(QF::GuidString(id,&text));
		Check(JSONCreateString(text,&reference));
		Check(ImageProperty(control,L"PopupMenu",&values,&property));
		Check(values->SetValueJSON(property,reference));
		external->edit = columns;
		TStackRef<IQuadooListItems> popupItems;
		TStackRef<IQuadooListItem> popupRow;
		Check(view->GetItems(&popupItems));
		Check(popupItems->Insert(0,RSTRING_CAST(L"popup row"),&popupRow));
		external->editItems = popupItems;
		SendMessageW(original,WM_CONTEXTMENU,reinterpret_cast<WPARAM>(original),MAKELPARAM(-20,-30));
		VERIFY(external->popups == 1 && external->point.x == -20 && external->point.y == -30);
		VERIFY(SUCCEEDED(external->editResult));
		Check(popupItems->GetCount(&count));
		VERIFY(count == 0);
		external->editItems = NULL;
		Check(columns->GetCount(&count));
		VERIFY(count == 0);
		external->edit = NULL;
		Check(native->GetHandle(&current));
		SendMessageW(current,WM_CONTEXTMENU,reinterpret_cast<WPARAM>(ListView_GetHeader(current)),MAKELPARAM(-20,-30));
		VERIFY(external->popups == 1);
		Check(values->SetValueJSON(property,NULL));
	}
	Check(external->Destroy());
	Check(native->GetHandle(&current));
	VERIFY(!ListView_GetImageList(current,LVSIL_NORMAL) && !ListView_GetImageList(current,LVSIL_SMALL) && !ListView_GetImageList(current,LVSIL_STATE));
	VERIFY(external->Observers() == 0 && ImageList_GetImageCount(originalImages) == 2);
	Check(root->Destroy());
	hr = S_OK;
	Cleanup:
	s_failListNative = FALSE;
	if(external)
	{
		external->Destroy();
	}
	if(root)
	{
		root->Destroy();
	}
	return hr;
}
