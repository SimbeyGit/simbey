// Phase A tests exercise only the published runtime contract.
#include "TestIcon.h"
static HRESULT DesignerRuntimeProperties()
{
	HRESULT hr;
	TStackRef<IJSONObject> definition, first, second; TStackRef<IQuadooForm> form;
	TStackRef<IQuadooControl> child, copy; TStackRef<IQuadooFormSerializer> serializer;
	TStackRef<IJSONValue> icon, invalid; TStackRef<IQuadooProperties> properties; TStackRef<IQuadooScrollable> scrolling;
	TStackRef<IQuadooLayoutControl> layout; TStackRef<IQuadooChildWindowHost> host;
	HWND window, content, button; UINT index; HICON image = NULL;
	QuadooForm::Rect stored, after; QuadooForm::Point position = { 100,80 }, actual; QuadooForm::Size extent;
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"RuntimePrerequisites\",\"properties\":{\"Width\":300,\"Height\":200},\"children\":[{\"type\":\"Button\",\"name\":\"far\",\"properties\":{\"Left\":600,\"Top\":450}}]}",&definition));
	Check(QuadooCreateForm(NULL,NULL,definition,&form)); Check(form->GetHandle(&window)); Check(form->FindControl(RSTRING_CAST(L"far"),&child));
	{ TStackRef<IQuadooWinControl> native; Check(child->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native))); Check(native->GetHandle(&button)); }
	Check(form->QueryInterface(__uuidof(IQuadooChildWindowHost),reinterpret_cast<PVOID*>(&host))); Check(host->GetChildHostHandle(&content)); REQUIRE(content != window && GetParent(button) == content && GetParent(content) == window);
	Check(TestIconValue(&icon)); Check(QuadooLoadIconValue(icon,16,16,&image)); REQUIRE(image != NULL); DestroyIcon(image); image = NULL;
	Check(Property(form,L"Icon",&properties,&index)); Check(properties->SetValueJSON(index,icon)); REQUIRE(SendMessageW(window,WM_GETICON,ICON_SMALL,0) && SendMessageW(window,WM_GETICON,ICON_BIG,0));
	Check(JSONCreateStringW(L"invalid",7,&invalid)); REQUIRE(FAILED(properties->SetValueJSON(index,invalid))); invalid.Release(); Check(properties->GetValueJSON(index,&invalid)); REQUIRE(JSONCompare(icon,invalid) == 0); invalid.Release();
	{ TStackRef<IJSONObject> broken; Check(icon->GetObject(&broken)); Check(broken->AddValueW(L"extra",NULL)); REQUIRE(FAILED(properties->SetValueJSON(index,icon))); broken->RemoveValueW(L"extra"); }
	{
		static PCWSTR const corrupt[] = { L"!!!!",L"AAAB",L"AAABAA==garbage==",L"AAABAAEA\n",L"AAABAAEA====" };
		for(UINT n = 0; n < ARRAYSIZE(corrupt); n++)
		{
			TStackRef<IJSONValue> candidate, text, unchanged; TStackRef<IJSONObject> object;
			Check(JSONClone(icon,&candidate,TRUE)); Check(candidate->GetObject(&object)); Check(JSONCreateStringW(corrupt[n],lstrlenW(corrupt[n]),&text)); Check(object->AddValueW(L"data",text));
			REQUIRE(FAILED(properties->SetValueJSON(index,candidate))); REQUIRE(FAILED(QuadooLoadIconValue(candidate,16,16,&image)) && !image); Check(properties->GetValueJSON(index,&unchanged)); REQUIRE(JSONCompare(icon,unchanged) == 0);
		}
		{
			TStackRef<IJSONValue> candidate, text; TStackRef<IJSONObject> object; CRString encoded; PWSTR write;
			const UINT excessive = (1024 * 1024 + 2) / 3 * 4 + 4;
			Check(RStrAllocW(excessive,&encoded,&write)); for(UINT n = 0; n < excessive; n++) write[n] = L'A';
			Check(JSONClone(icon,&candidate,TRUE)); Check(candidate->GetObject(&object)); Check(JSONCreateString(encoded,&text)); Check(object->AddValueW(L"data",text)); REQUIRE(FAILED(properties->SetValueJSON(index,candidate)));
		}
		{
			BYTE shortHeader[6] = { 0,0,1,0,1,0 }; TStackRef<IJSONValue> candidate;
			REQUIRE(FAILED(QuadooCreateIconValue(shortHeader,sizeof(shortHeader),&candidate)) && !candidate);
			REQUIRE(FAILED(QuadooCreateIconValue(shortHeader,1024 * 1024 + 1,&candidate)) && !candidate);
		}
	}
	Check(QuadooGetFormSerializer(&serializer)); Check(serializer->Serialize(form,&first));
	for(INT style = 0; style < 4; style++)
	{
		Check(SetInt(form,L"BorderStyle",style)); REQUIRE((GetWindowLongW(window,GWL_STYLE) & WS_THICKFRAME) == (style == QuadooForm::BorderSizeable ? WS_THICKFRAME : 0));
		RECT client; REQUIRE(GetClientRect(window,&client)); REQUIRE(client.right == 300 && client.bottom == 200);
	}
	Check(SetInt(form,L"BorderStyle",QuadooForm::BorderSizeable)); Check(SetInt(form,L"BorderIcons",QuadooForm::BorderSystemMenu | QuadooForm::BorderClose)); REQUIRE(!(GetWindowLongW(window,GWL_STYLE) & (WS_MINIMIZEBOX | WS_MAXIMIZEBOX)));
	Check(SetInt(form,L"FormStyle",QuadooForm::FormStayOnTop)); REQUIRE(GetWindowLongW(window,GWL_EXSTYLE) & WS_EX_TOPMOST); Check(SetInt(form,L"FormStyle",QuadooForm::FormNormal));
	Check(ChoiceBool(form,L"AutoScroll",TRUE)); Check(form->QueryInterface(__uuidof(IQuadooScrollable),reinterpret_cast<PVOID*>(&scrolling)));
	{ RECT beforeStyle, afterStyle; REQUIRE(GetClientRect(window,&beforeStyle)); Check(SetInt(form,L"BorderStyle",QuadooForm::BorderSingle)); REQUIRE(GetClientRect(window,&afterStyle) && EqualRect(&beforeStyle,&afterStyle)); Check(SetInt(form,L"BorderStyle",QuadooForm::BorderSizeable)); REQUIRE(GetClientRect(window,&afterStyle) && EqualRect(&beforeStyle,&afterStyle)); }
	Check(child->GetBounds(&stored)); Check(scrolling->GetContentSize(&extent)); REQUIRE(extent.Width >= 700 && extent.Height >= 478);
	Check(scrolling->SetScrollPosition(&position)); Check(scrolling->GetScrollPosition(&actual)); REQUIRE(actual.X == 100 && actual.Y == 80); Check(child->GetBounds(&after)); REQUIRE(EqualBounds(stored,after));
	Check(form->ShowForm(SW_SHOWNOACTIVATE)); { TStackRef<IQuadooWinControl> native; Check(child->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native))); Check(native->SetFocus()); }
	Check(scrolling->GetScrollPosition(&actual)); REQUIRE(actual.X > 100 && actual.Y > 80);
	Check(form->QueryInterface(__uuidof(IQuadooLayoutControl),reinterpret_cast<PVOID*>(&layout)));
	for(UINT dpi = 96; dpi <= 192; dpi += 48) { QuadooForm::Rect bounds = { 0,0,300,200 }; Check(layout->Arrange(&bounds,dpi)); Check(child->GetBounds(&after)); REQUIRE(EqualBounds(stored,after)); }
	Check(properties->SetValueJSON(index,NULL)); Check(properties->GetValueJSON(index,&invalid)); REQUIRE(!invalid); Check(properties->SetValueJSON(index,icon));
	first.Release(); Check(serializer->Serialize(form,&first));
	{ TStackRef<IQuadooConstructionContext> context; Check(Context(96,NULL,&context)); Check(QuadooCreateControl(context,NULL,first,&copy)); Check(serializer->Serialize(copy,&second)); REQUIRE(JSONCompareObjects(first,second) == 0); }
	Check(copy->Destroy()); copy.Release();
Cleanup:
	if(image) DestroyIcon(image); if(copy) copy->Destroy(); if(form) form->Destroy();
	return hr;
}
static CBinder* g_dialogBinder;
static HWND g_dialogButton, g_dialogOwner;
static IQuadooControl* g_dialogControl;
static IQuadooForm* g_dialogForm;
static UINT g_dialogTicks;
static BOOL g_dialogCancel, g_dialogNone, g_dialogDisabled;
static HRESULT g_dialogError;
static VOID CALLBACK DesignerModalTimer(HWND hwnd, UINT, UINT_PTR id, DWORD)
{
	if(++g_dialogTicks > 4) { KillTimer(hwnd,id); g_dialogError = E_FAIL; g_dialogForm->Destroy(); return; }
	g_dialogDisabled = !IsWindowEnabled(g_dialogOwner);
	SendMessageW(g_dialogButton,BM_CLICK,0,0);
	if(g_dialogTicks == 1 && g_dialogCancel)
	{
		TStackRef<IQuadooProperties> values; UINT index; LONGLONG pending = -1;
		g_dialogError = Property(g_dialogForm,L"ModalResult",&values,&index);
		if(SUCCEEDED(g_dialogError)) g_dialogError = values->GetValueLong(index,&pending);
		if(pending != 0) g_dialogError = E_FAIL; g_dialogBinder->cancel = FALSE;
	}
	if(g_dialogTicks == 1 && g_dialogNone)
	{
		TStackRef<IQuadooProperties> values; UINT index;
		g_dialogError = Property(g_dialogControl,L"ModalResult",&values,&index);
		if(SUCCEEDED(g_dialogError)) g_dialogError = values->SetValueLong(index,IDOK);
	}
}
static HRESULT DesignerModalButtons()
{
	HRESULT hr;
	TStackRef<IJSONObject> definition; TStackRef<IQuadooForm> owner, form; TStackRef<CBinder> binder;
	TStackRef<IQuadooControl> root, button; TStackRef<IQuadooWinControl> native; TStackRef<IQuadooConstructionContext> context;
	HWND window; LRESULT result;
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"Owner\"}",&definition)); Check(QuadooCreateForm(NULL,NULL,definition,&owner)); definition.Release(); Check(owner->GetHandle(&g_dialogOwner));
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"Dialog\",\"events\":{\"OnCloseQuery\":\"Query\",\"OnClose\":\"Closed\"},\"children\":[{\"type\":\"Panel\",\"name\":\"panel\",\"children\":[{\"type\":\"Button\",\"name\":\"finish\",\"properties\":{\"Caption\":\"OK\",\"ModalResult\":\"mrOk\"},\"events\":{\"OnClick\":\"Click\"}}]}]}",&definition));
	for(UINT test = 0; test < 4; test++)
	{
		binder.Attach(__new CBinder); CheckAlloc(binder);
		if(test == 3)
		{ Check(Context(96,binder,&context,NULL,QuadooForm::Preview,g_dialogOwner)); Check(QuadooCreateControl(context,NULL,definition,&root)); Check(root->QueryInterface(__uuidof(IQuadooForm),reinterpret_cast<PVOID*>(&form))); }
		else { Check(QuadooCreateForm(binder,owner,definition,&form)); }
		Check(form->GetHandle(&window)); REQUIRE(!(GetWindowLongW(window,GWL_STYLE) & WS_CHILD)); Check(form->FindControl(RSTRING_CAST(L"finish"),&button)); Check(button->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native))); Check(native->GetHandle(&g_dialogButton));
		// Zero-result clicks remain ordinary actions on modeless forms.
		{
			TStackRef<IQuadooProperties> values;
			UINT index;
			Check(Property(button,L"ModalResult",&values,&index));
			Check(values->SetValueLong(index,0));
			SendMessageW(g_dialogButton,BM_CLICK,0,0);
			REQUIRE(IsWindow(window));
			Check(values->SetValueLong(index,IDOK));
		}
		g_dialogCancel = test == 1; g_dialogNone = test == 2; binder->cancel = g_dialogCancel;
		if(g_dialogNone) { TStackRef<IQuadooProperties> values; UINT index; Check(Property(button,L"ModalResult",&values,&index)); Check(values->SetValueLong(index,0)); }
		g_dialogBinder = binder; g_dialogControl = button; g_dialogForm = form; g_dialogTicks = 0; g_dialogError = S_OK;
		REQUIRE(SetTimer(window,10,10,DesignerModalTimer)); Check(form->ShowModal(&result)); Check(g_dialogError); REQUIRE(result == IDOK && !IsWindow(window) && IsWindowEnabled(g_dialogOwner) && g_dialogDisabled);
		if(test == 3) REQUIRE(binder->binds == 0 && binder->clicks == 0 && binder->closed == 0);
		else REQUIRE(binder->clicks == g_dialogTicks + 1 && binder->closed == 1);
		REQUIRE(g_dialogTicks == ((g_dialogCancel || g_dialogNone) ? 2 : 1));
		native.Release(); button.Release(); root.Release(); form.Release(); context.Release(); binder.Release();
	}
Cleanup:
	g_dialogBinder = NULL; g_dialogControl = NULL; g_dialogForm = NULL;
	if(form) form->Destroy(); if(owner) owner->Destroy();
	return hr;
}
static HRESULT DesignerCapabilities()
{
	HRESULT hr;
	TStackRef<IQuadooControlRegistry> registry; TStackRef<IQuadooFactoryCatalog> catalog; TStackRef<IQuadooConstructionContext> context;
	TStackRef<IQuadooControlFactory> factory; TStackRef<IQuadooControl> root, combo; TStackRef<IJSONObject> definition;
	TStackRef<IQuadooForm> form; TStackRef<IQuadooDesignCapabilities> capabilities;
	UINT count; DWORD flags; BOOL allowed;
	Check(QuadooGetControlRegistry(&registry));
	Check(registry->QueryInterface(__uuidof(IQuadooFactoryCatalog),reinterpret_cast<PVOID*>(&catalog)));
	Check(catalog->GetFactoryCount(&count));
	REQUIRE(count == 25);
	for(UINT n = 0; n < count; n++) { TStackRef<IQuadooControlFactory> item; TStackRef<IQuadooDesignFactory> placement; Check(catalog->GetFactory(n,&item)); Check(item->QueryInterface(__uuidof(IQuadooDesignFactory),reinterpret_cast<PVOID*>(&placement))); Check(placement->CanCreate(NULL,&allowed)); }
	REQUIRE(catalog->GetFactory(count,&factory) == HRESULT_FROM_WIN32(ERROR_INVALID_INDEX) && !factory);
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"Caps\",\"children\":[{\"type\":\"ComboBox\",\"name\":\"combo\"}]}",&definition)); Check(QuadooCreateForm(NULL,NULL,definition,&form)); Check(form->FindControl(RSTRING_CAST(L"combo"),&combo));
	Check(combo->QueryInterface(__uuidof(IQuadooDesignCapabilities),reinterpret_cast<PVOID*>(&capabilities))); Check(capabilities->GetDesignCapabilities(&flags)); REQUIRE((flags & QuadooForm::DesignResizeWidth) && !(flags & QuadooForm::DesignResizeHeight));
	Check(SetInt(combo,L"Align",QuadooForm::AlignClient)); Check(capabilities->GetDesignCapabilities(&flags)); REQUIRE(flags == 0);
Cleanup:
	if(form) form->Destroy();
	return hr;
}
