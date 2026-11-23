#include "..\..\..\..\platform\QuadooForm\ScriptAdapter\ListViewObjects.h"

class CListOpaqueData : public CBaseUnknown
{
public:
	BEGIN_UNK_MAP
		UNK_INTERFACE_PATH(IUnknown,CBaseUnknown)
	END_UNK_MAP
};
static HRESULT TestListAdapters(IQuadooVM* vm, CAQVMHost* host)
{
	HRESULT hr;
	QuadooVM::QVARIANT script = {QuadooVM::Null};
	QuadooVM::QVARIANT root = {QuadooVM::Null};
	QuadooVM::QVARIANT list = {QuadooVM::Null};
	QuadooVM::QVARIANT columns = {QuadooVM::Null};
	QuadooVM::QVARIANT column = {QuadooVM::Null};
	QuadooVM::QVARIANT result = {QuadooVM::Null};
	QuadooVM::QVARIANT wrapped = {QuadooVM::Null};
	QuadooVM::QVARIANT data = {QuadooVM::Null};
	QuadooVM::QVARIANT text = {QuadooVM::String};
	QuadooVM::QVPARAMS args = {0};
	TStackRef<IQuadooComponent> component;
	TStackRef<IQuadooControlContainer> container;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooWinControl> native;
	TStackRef<IQuadooListView> view;
	TStackRef<IQuadooListItems> items;
	TStackRef<IQuadooListItem> item;
	TStackRef<CListOpaqueData> payload;
	TStackRef<IUnknown> actual;
	HWND window;
	Check(Call(vm,L"ListEventForm",&script));
	Check(script.pObject->GetProperty(NULL,RSTRING_CAST(AQVM::RootProperty),&root));
	Check(host->Runtime()->Unwrap(root.pObject,&component));
	Check(script.pObject->GetProperty(NULL,RSTRING_CAST(L"List1"),&list));
	Check(list.pObject->GetProperty(NULL,RSTRING_CAST(L"Columns"),&columns));
	text.rstrVal = RSTRING_CAST(L"Column");
	args.cArgs = 1;
	args.pqvArgs = &text;
	Check(columns.pObject->Invoke(NULL,RSTRING_CAST(L"Add"),&args,&column));
	Check(component->QueryInterface(&container));
	Check(container->FindControl(RSTRING_CAST(L"List1"),&control));
	Check(control->QueryInterface(&native));
	Check(control->QueryInterface(&view));
	Check(native->GetHandle(&window));
	{
		TStackRef<IQuadooMessageHandler> handler;
		NMLISTVIEW click = {0};
		BOOL handled;
		LRESULT ignored;
		Check(control->QueryInterface(&handler));
		click.hdr.hwndFrom = window;
		click.hdr.code = LVN_COLUMNCLICK;
		click.iSubItem = 0;
		Check(handler->HandleMessage(window,WM_NOTIFY,0,reinterpret_cast<LPARAM>(&click),&handled,&ignored));
		REQUIRE(handled);
		Check(Call(vm,L"ListClickedColumn",&result));
		REQUIRE(result.eType == QuadooVM::Object && AQVM::SameIdentity(result.pObject,column.pObject));
		QVMClearVariant(&result);
	}
	Check(view->GetItems(&items));
	Check(items->Insert(0,RSTRING_CAST(L"Opaque"),&item));
	Check(AQVM::WrapListItem(item,items,&wrapped,NULL));
	payload.Attach(__new CListOpaqueData);
	CheckAlloc(payload);
	Check(item->SetData(payload));
	Check(wrapped.pObject->GetProperty(NULL,RSTRING_CAST(L"Data"),&data));
	REQUIRE(data.eType == QuadooVM::Object);
	Check(item->SetData(NULL));
	Check(wrapped.pObject->SetProperty(NULL,RSTRING_CAST(L"Data"),&data));
	Check(item->GetData(&actual));
	REQUIRE(AQVM::SameIdentity(actual,payload));
	Check(component->Destroy());
	{
		BOOL attached;
		Check(item->GetAttached(&attached));
		REQUIRE(!attached);
	}
	host->Runtime()->Sweep();
	hr = S_OK;
Cleanup:
	if(component)
	{
		component->Destroy();
	}
	QVMClearVariant(&data);
	QVMClearVariant(&wrapped);
	QVMClearVariant(&result);
	QVMClearVariant(&column);
	QVMClearVariant(&columns);
	QVMClearVariant(&list);
	QVMClearVariant(&root);
	QVMClearVariant(&script);
	return hr;
}
