#include <ColorDlg.h>

static HRESULT SwatchJSON(IQuadooComponent* control, PCWSTR name, PCWSTR source)
{
	HRESULT hr;
	TStackRef<IJSONValue> value;
	Check(JSONParse(NULL,source,TStrLenAssert(source),&value));
	Check(ImageSet(control,name,value));
Cleanup:
	return hr;
}
static INT SwatchColor(IQuadooComponent* control, UINT index)
{
	TStackRef<IJSONValue> value, color;
	TStackRef<IJSONArray> colors;
	INT number = -1;
	if(SUCCEEDED(ImageRead(control,L"Colors",&value)) && SUCCEEDED(value->GetArray(&colors)) &&
		SUCCEEDED(colors->GetValue(index,&color))) color->GetInteger(&number);
	return number;
}
class CSwatchEvents : public CBaseUnknown, public IQuadooEventsBinder, public IQuadooDelegate
{
public:
	UINT clicks;
	INT selected, color;
	BOOL destroy;
	CSwatchEvents() : clicks(0), selected(-2), color(-1), destroy(FALSE) {}
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooEventsBinder)
		UNK_INTERFACE(IQuadooDelegate)
	END_UNK_MAP
	STDMETHOD(BindEvent)(RSTRING, IQuadooDelegate** result) { *result = this; AddRef(); return S_OK; }
	STDMETHOD(PostBind)(IQuadooControl*) { return S_OK; }
	STDMETHOD(Invoke)(IQuadooEventArgs* args)
	{
		HRESULT hr;
		TStackRef<IQuadooComponent> sender;
		Check(args->GetSender(&sender));
		clicks++;
		selected = ImageInteger(sender,L"Selected");
		color = SwatchColor(sender,0);
		if(destroy)
		{
			Check(sender->Destroy());
		}
	Cleanup:
		return hr;
	}
};
class CSwatchObserver : public CBaseUnknown, public IQuadooPropertyObserver
{
public:
	UINT changes;
	BOOL trimming, coherent;
	CSwatchObserver() : changes(0), trimming(FALSE), coherent(TRUE) {}
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooPropertyObserver)
	END_UNK_MAP
	STDMETHOD(OnPropertyChanged)(IQuadooComponent* control, UINT)
	{
		HRESULT hr = S_OK;
		changes++;
		if(trimming)
		{
			TStackRef<IJSONValue> value;
			TStackRef<IJSONArray> labels;
			Check(ImageRead(control,L"Labels",&value));
			Check(value->GetArray(&labels));
			coherent &= ImageInteger(control,L"Selected") == -1 && labels->Count() == 1;
		}
	Cleanup:
		return hr;
	}
	STDMETHOD(OnBoundsChanged)(IQuadooControl*, const QuadooForm::Rect*) { return S_OK; }
};
// A public-interface-only input that fails while copying a replacement palette.
// The live Colors/Labels/Selected transaction must not begin on this failure.
class CFailingSwatchValue : public CBaseUnknown, public IJSONValue
{
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IJSONValue)
	END_UNK_MAP
	JSON::Type STDMETHODCALLTYPE GetType() { return JSON::Array; }
	STDMETHOD(GetString)(RSTRING*) { return E_OUTOFMEMORY; }
	STDMETHOD(GetStringAsGuid)(GUID&) { return E_OUTOFMEMORY; }
	STDMETHOD(GetObject)(IJSONObject**) { return E_OUTOFMEMORY; }
	STDMETHOD(GetArray)(IJSONArray** value) { *value = NULL; return E_OUTOFMEMORY; }
	STDMETHOD(GetArrayLength)(sysint*) { return E_OUTOFMEMORY; }
	STDMETHOD(GetArrayItem)(sysint, IJSONValue**) { return E_OUTOFMEMORY; }
	STDMETHOD(GetBoolean)(bool*) { return E_OUTOFMEMORY; }
	STDMETHOD(GetInteger)(INT*) { return E_OUTOFMEMORY; }
	STDMETHOD(GetLongInteger)(LONGLONG*) { return E_OUTOFMEMORY; }
	STDMETHOD(GetDouble)(DOUBLE*) { return E_OUTOFMEMORY; }
	STDMETHOD(GetFloat)(FLOAT*) { return E_OUTOFMEMORY; }
	STDMETHOD(GetDWord)(DWORD*) { return E_OUTOFMEMORY; }
};
static const WCHAR SwatchDefinition[] =
	L"{\"type\":\"Form\",\"name\":\"SwatchForm\",\"properties\":{\"Width\":360,\"Height\":180},\"children\":["
	L"{\"type\":\"ColorSwatch\",\"name\":\"swatch\",\"properties\":{\"Left\":10,\"Top\":12,\"Width\":101,\"Height\":36,"
	L"\"Colors\":[255,65280,16711680,0]},\"events\":{\"OnClick\":\"Click\"}}]}";

static HRESULT ColorSwatchInteraction()
{
	HRESULT hr;
	TStackRef<IJSONObject> definition;
	TStackRef<IQuadooForm> form;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooStaticControl> painted;
	TStackRef<IQuadooMessageHandler> handler;
	TStackRef<IQuadooWinControl> windowed;
	TStackRef<IQuadooChildWindowHost> host;
	TStackRef<CSwatchEvents> events;
	TStackRef<CSwatchObserver> observer;
	TStackRef<IQuadooPropertyNotifications> notifications;
	TStackRef<IJSONValue> snapshot, current;
	CImageTestCanvas canvas;
	RECT area = {2,2,103,38};
	HWND window;
	BOOL hit;
	events.Attach(__new CSwatchEvents);
	observer.Attach(__new CSwatchObserver);
	CheckAlloc(events);
	CheckAlloc(observer);
	Check(Parse(SwatchDefinition,&definition));
	Check(QuadooCreateForm(events,NULL,definition,&form));
	Check(form->FindControl(RSTRING_CAST(L"swatch"),&control));
	Check(control->QueryInterface(__uuidof(IQuadooStaticControl),reinterpret_cast<PVOID*>(&painted)));
	Check(control->QueryInterface(__uuidof(IQuadooMessageHandler),reinterpret_cast<PVOID*>(&handler)));
	Check(control->QueryInterface(__uuidof(IQuadooPropertyNotifications),reinterpret_cast<PVOID*>(&notifications)));
	Check(notifications->AddObserver(observer));
	REQUIRE(control->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&windowed)) == E_NOINTERFACE && !windowed);
	{
		TStackRef<IUnknown> first, second, third;
		Check(control->QueryInterface(__uuidof(IUnknown),reinterpret_cast<PVOID*>(&first)));
		Check(painted->QueryInterface(__uuidof(IUnknown),reinterpret_cast<PVOID*>(&second)));
		Check(handler->QueryInterface(__uuidof(IUnknown),reinterpret_cast<PVOID*>(&third)));
		REQUIRE(first == second && second == third);
	}
	Check(form->QueryInterface(__uuidof(IQuadooChildWindowHost),reinterpret_cast<PVOID*>(&host)));
	Check(host->GetChildHostHandle(&window));
	REQUIRE(!GetWindow(window,GW_CHILD));
	Check(canvas.Create(140,100));
	REQUIRE(painted->Paint(NULL,&area) == E_INVALIDARG);
	{
		RECT overflow = {INT_MIN,0,INT_MAX,10};
		REQUIRE(painted->Paint(canvas.DC(),&overflow) == E_INVALIDARG);
	}
	canvas.Fill(RGB(99,88,77));
	Check(painted->Paint(canvas.DC(),&area));
	REQUIRE(canvas.Pixel(2,2) == RGB(99,88,77));
	REQUIRE(canvas.Pixel(12,20) == RGB(255,0,0) && canvas.Pixel(32,20) == RGB(0,255,0));
	REQUIRE(canvas.Pixel(62,20) == RGB(0,0,255) && canvas.Pixel(92,20) == RGB(0,0,0));
	REQUIRE(canvas.Pixel(10,3) == RGB(99,88,77) && canvas.Pixel(1,20) == RGB(99,88,77));
	Check(ImageNumber(control,L"Selected",1));
	canvas.Fill(RGB(99,88,77));
	Check(painted->Paint(canvas.DC(),&area));
	REQUIRE(canvas.Pixel(37,2) == RGB(0,204,0) && canvas.Pixel(37,3) == RGB(0,255,0));
	REQUIRE(canvas.Pixel(12,2) == RGB(99,88,77) && canvas.Pixel(37,37) == RGB(0,204,0));
	Check(ImageNumber(control,L"Selected",3));
	Check(painted->Paint(canvas.DC(),&area));
	REQUIRE(canvas.Pixel(90,2) == RGB(48,48,48) && canvas.Pixel(90,3) == RGB(0,0,0));
	REQUIRE(!events->clicks);
	for(INT n = 0; n < 4; n++)
	{
		// Include the exact boundaries of unevenly divided 101-pixel widths.
		SendMessageW(window,WM_LBUTTONUP,0,MAKELPARAM(10 + n * 101 / 4,25));
		REQUIRE(ImageInteger(control,L"Selected") == n && events->selected == n && events->clicks == static_cast<UINT>(n + 1));
	}
	SendMessageW(window,WM_LBUTTONUP,0,MAKELPARAM(110,25));
	REQUIRE(events->clicks == 5 && ImageInteger(control,L"Selected") == 3);
	SendMessageW(window,WM_LBUTTONUP,0,MAKELPARAM(111,25));
	REQUIRE(events->clicks == 5);
	Check(control->SetEnabled(FALSE));
	Check(painted->HitTest(20,20,&hit));
	REQUIRE(!hit);
	SendMessageW(window,WM_LBUTTONUP,0,MAKELPARAM(15,25));
	REQUIRE(events->clicks == 5);
	Check(control->SetEnabled(TRUE));
	Check(control->SetVisible(FALSE));
	Check(painted->HitTest(20,20,&hit));
	REQUIRE(!hit);
	Check(control->SetVisible(TRUE));
	Check(SwatchJSON(control,L"Labels",L"[\"Red\",\"Green\",\"Blue\",\"Black\"]"));
	Check(ImageNumber(control,L"Selected",-1));
	canvas.Fill(RGB(99,88,77));
	Check(painted->Paint(canvas.DC(),&area));
	REQUIRE(canvas.Pixel(12,30) != RGB(255,0,0));
	DWORD textHash = canvas.Hash();
	Check(ImageNumber(control,L"ForeColor",RGB(255,0,255)));
	canvas.Fill(RGB(99,88,77));
	Check(painted->Paint(canvas.DC(),&area));
	REQUIRE(canvas.Hash() != textHash);
	Check(ImageNumber(control,L"Selected",1));
	{
		HGDIOBJ font = GetCurrentObject(canvas.DC(),OBJ_FONT), pen = GetCurrentObject(canvas.DC(),OBJ_PEN), brush = GetCurrentObject(canvas.DC(),OBJ_BRUSH);
		GdiFlush();
		DWORD beforePaint = GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
		for(UINT n = 0; n < 200; n++)
		{
			Check(painted->Paint(canvas.DC(),&area));
		}
		GdiFlush();
		REQUIRE(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS) <= beforePaint);
		REQUIRE(GetCurrentObject(canvas.DC(),OBJ_FONT) == font && GetCurrentObject(canvas.DC(),OBJ_PEN) == pen && GetCurrentObject(canvas.DC(),OBJ_BRUSH) == brush);
	}
	Check(ImageNumber(control,L"Selected",-1));
	Check(ImageRead(control,L"Colors",&snapshot));
	{
		TStackRef<IJSONArray> array;
		TStackRef<IJSONValue> added;
		Check(snapshot->GetArray(&array));
		Check(JSONCreateInteger(123,&added));
		Check(array->Add(added));
	}
	Check(ImageRead(control,L"Colors",&current));
	REQUIRE(JSONCompare(snapshot,current) != 0 && SwatchColor(control,0) == 255);
	Check(ImageSet(control,L"Colors",snapshot));
	{
		TStackRef<IJSONArray> array;
		TStackRef<IJSONValue> added;
		Check(snapshot->GetArray(&array));
		Check(JSONCreateInteger(456,&added));
		Check(array->Add(added));
	}
	current.Release();
	Check(ImageRead(control,L"Colors",&current));
	REQUIRE(JSONCompare(snapshot,current) != 0 && SwatchColor(control,4) == 123);
	Check(SwatchJSON(control,L"Colors",L"[255,65280,16711680,0]"));
	UINT beforeInvalid = observer->changes;
	{
		TStackRef<CFailingSwatchValue> failure;
		failure.Attach(__new CFailingSwatchValue);
		CheckAlloc(failure);
		REQUIRE(ImageSet(control,L"Colors",failure) == E_OUTOFMEMORY && observer->changes == beforeInvalid);
		REQUIRE(SwatchColor(control,0) == 255 && ImageInteger(control,L"Selected") == -1);
	}
	REQUIRE(FAILED(ImageNumber(control,L"Mode",1)) && ImageInteger(control,L"Mode") == 0);
	REQUIRE(FAILED(ImageNumber(control,L"Selected",4)) && ImageInteger(control,L"Selected") == -1);
	REQUIRE(FAILED(ImageNumber(control,L"Selected",-2)));
	static PCWSTR const invalidColors[] = {L"[]",L"null",L"{}",L"[null]",L"[true]",L"[\"red\"]",L"[16777216]",L"[-1]"};
	for(UINT n = 0; n < ARRAYSIZE(invalidColors); n++)
	{
		REQUIRE(FAILED(SwatchJSON(control,L"Colors",invalidColors[n])));
		REQUIRE(SwatchColor(control,0) == 255 && ImageInteger(control,L"Selected") == -1);
	}
	REQUIRE(FAILED(SwatchJSON(control,L"Labels",L"[\"one\",\"two\",\"three\",\"four\",\"five\"]")));
	REQUIRE(FAILED(SwatchJSON(control,L"Labels",L"[\"line\\nbreak\"]")));
	REQUIRE(FAILED(SwatchJSON(control,L"Labels",L"[null]")));
	REQUIRE(observer->changes == beforeInvalid);
	{
		TStackRef<IJSONArray> tooMany;
		TStackRef<IJSONValue> color, wrapped;
		Check(JSONCreateArray(&tooMany));
		Check(JSONCreateInteger(0,&color));
		for(UINT n = 0; n < 257; n++)
		{
			Check(tooMany->Add(color));
		}
		Check(JSONWrapArray(tooMany,&wrapped));
		REQUIRE(FAILED(ImageSet(control,L"Colors",wrapped)) && observer->changes == beforeInvalid);
	}
	Check(ImageNumber(control,L"Selected",3));
	observer->trimming = TRUE;
	UINT beforeTrim = observer->changes;
	Check(SwatchJSON(control,L"Colors",L"[-2147483635]"));
	REQUIRE(ImageInteger(control,L"Selected") == -1 && observer->coherent && observer->changes == beforeTrim + 3);
	observer->trimming = FALSE;
	{
		TStackRef<IJSONValue> labels;
		TStackRef<IJSONArray> array;
		Check(ImageRead(control,L"Labels",&labels));
		Check(labels->GetArray(&array));
		REQUIRE(array->Count() == 1);
	}
	Check(SwatchJSON(control,L"Labels",L"[]"));
	Check(painted->Paint(canvas.DC(),&area));
	REQUIRE(canvas.Pixel(50,20) == GetSysColor(COLOR_HIGHLIGHT));
	Check(ImageNumber(control,L"Mode",1));
	REQUIRE(FAILED(SwatchJSON(control,L"Colors",L"[1,2]")) && SwatchColor(control,0) == static_cast<INT>(0x80000000 | COLOR_HIGHLIGHT));
	REQUIRE(!events->destroy && events->clicks == 5);
	Check(ImageNumber(control,L"Mode",0));
	events->destroy = TRUE;
	SendMessageW(window,WM_LBUTTONUP,0,MAKELPARAM(15,25));
	QuadooForm::ComponentState state;
	Check(control->GetState(&state));
	REQUIRE(state == QuadooForm::Destroyed && events->clicks == 6);
	REQUIRE(FAILED(painted->Paint(canvas.DC(),&area)));
Cleanup:
	if(form) form->Destroy();
	return hr;
}

static HWND g_swatchOwner;
static IQuadooControl* g_swatchEditing;
static BOOL g_swatchAccept, g_swatchSeen, g_swatchCorrect;
static UINT g_swatchTicks, g_swatchAction;
static COLORREF g_swatchInitial;
static BOOL CALLBACK SwatchDialog(HWND window, LPARAM)
{
	if(GetWindow(window,GW_OWNER) != g_swatchOwner || !GetDlgItem(window,COLOR_RED) || !GetDlgItem(window,COLOR_BLUE)) return TRUE;
	g_swatchSeen = TRUE;
	g_swatchCorrect = GetDlgItemInt(window,COLOR_RED,NULL,FALSE) == GetRValue(g_swatchInitial) &&
		GetDlgItemInt(window,COLOR_GREEN,NULL,FALSE) == GetGValue(g_swatchInitial) && GetDlgItemInt(window,COLOR_BLUE,NULL,FALSE) == GetBValue(g_swatchInitial);
	if(g_swatchAction == 1)
	{
		SwatchJSON(g_swatchEditing,L"Colors",L"[67305985]"); // Invalid tags leave revision unchanged.
		TStackRef<IQuadooMessageHandler> handler;
		BOOL handled = TRUE;
		LRESULT result = -1;
		if(SUCCEEDED(g_swatchEditing->QueryInterface(__uuidof(IQuadooMessageHandler),reinterpret_cast<PVOID*>(&handler))))
			g_swatchCorrect &= handler->HandleMessage(GetWindow(g_swatchOwner,GW_CHILD),WM_LBUTTONUP,0,MAKELPARAM(20,25),&handled,&result) == S_FALSE && !handled;
		else g_swatchCorrect = FALSE;
	}
	if(g_swatchAction == 2) SwatchJSON(g_swatchEditing,L"Colors",L"[197121]");
	if(g_swatchAction == 3) g_swatchEditing->Destroy();
	if(g_swatchAccept)
	{
		SetDlgItemInt(window,COLOR_RED,41,FALSE);
		SetDlgItemInt(window,COLOR_GREEN,82,FALSE);
		SetDlgItemInt(window,COLOR_BLUE,123,FALSE);
	}
	SendMessageW(window,WM_COMMAND,g_swatchAccept ? IDOK : IDCANCEL,0);
	return FALSE;
}
static VOID CALLBACK SwatchTimer(HWND window, UINT, UINT_PTR timer, DWORD)
{
	if(++g_swatchTicks > 100) { KillTimer(window,timer); PostQuitMessage(1); return; }
	EnumThreadWindows(GetCurrentThreadId(),SwatchDialog,0);
}
static HRESULT ColorSwatchPicker()
{
	HRESULT hr;
	TStackRef<IJSONObject> definition;
	TStackRef<IQuadooForm> form;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooChildWindowHost> host;
	TStackRef<CSwatchEvents> events;
	HWND content = NULL, window = NULL;
	events.Attach(__new CSwatchEvents);
	CheckAlloc(events);
	Check(Parse(SwatchDefinition,&definition));
	Check(QuadooCreateForm(events,NULL,definition,&form));
	Check(form->FindControl(RSTRING_CAST(L"swatch"),&control));
	Check(SwatchJSON(control,L"Colors",L"[-2147483633]"));
	Check(ImageNumber(control,L"Mode",1));
	Check(form->GetHandle(&window));
	Check(form->QueryInterface(__uuidof(IQuadooChildWindowHost),reinterpret_cast<PVOID*>(&host)));
	Check(host->GetChildHostHandle(&content));
	g_swatchOwner = window;
	g_swatchEditing = control;
	for(UINT n = 0; n < 5; n++)
	{
		g_swatchInitial = ResolveQuadooColor(SwatchColor(control,0));
		g_swatchAccept = n != 0;
		g_swatchAction = n ? n - 1 : 0;
		g_swatchSeen = g_swatchCorrect = FALSE;
		g_swatchTicks = 0;
		CheckIf(!SetTimer(window,5440,10,SwatchTimer),HrEnsureLastError());
		SendMessageW(content,WM_LBUTTONUP,0,MAKELPARAM(20,25));
		KillTimer(window,5440);
		REQUIRE(g_swatchSeen && g_swatchCorrect);
		if(n == 0)
		{
			REQUIRE(!events->clicks && SwatchColor(control,0) == static_cast<INT>(0x80000000 | COLOR_BTNFACE));
		}
		if(n == 1 || n == 2)
		{
			REQUIRE(events->clicks == n && events->color == RGB(41,82,123) && SwatchColor(control,0) == RGB(41,82,123));
		}
		if(n == 3)
		{
			REQUIRE(events->clicks == 2 && SwatchColor(control,0) == RGB(1,2,3));
		}
		if(n == 4)
		{
			QuadooForm::ComponentState state;
			Check(control->GetState(&state));
			REQUIRE(state == QuadooForm::Destroyed && events->clicks == 2);
		}
	}
Cleanup:
	if(window) KillTimer(window,5440);
	g_swatchEditing = NULL;
	if(form) form->Destroy();
	return hr;
}

static HRESULT ColorSwatchPersistence()
{
	HRESULT hr;
	TStackRef<CSwatchEvents> events;
	TStackRef<IJSONObject> definition, serialized;
	TStackRef<IQuadooForm> form, copy;
	TStackRef<IQuadooControl> control, copied;
	TStackRef<IQuadooFormSerializer> serializer;
	TStackRef<IQuadooLayoutControl> layout;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooControlFactory> factory;
	TStackRef<IQuadooFactoryLifetime> lifetime;
	CImageTestCanvas canvas;
	UINT live;
	events.Attach(__new CSwatchEvents);
	CheckAlloc(events);
	Check(QuadooGetControlRegistry(&registry));
	Check(registry->FindFactory(RSTRING_CAST(L"ColorSwatch"),&factory));
	Check(factory->QueryInterface(__uuidof(IQuadooFactoryLifetime),reinterpret_cast<PVOID*>(&lifetime)));
	Check(Parse(SwatchDefinition,&definition));
	Check(QuadooCreateForm(events,NULL,definition,&form));
	Check(form->FindControl(RSTRING_CAST(L"swatch"),&control));
	Check(SwatchJSON(control,L"Colors",L"[-2147483635,0,16777215]"));
	Check(SwatchJSON(control,L"Labels",L"[\"Accent\",\"Black\",\"White \\u03a9\"]"));
	Check(ImageNumber(control,L"Selected",2));
	Check(ImageNumber(control,L"Anchors",11));
	Check(QuadooGetFormSerializer(&serializer));
	Check(serializer->Serialize(form,&serialized));
	Check(QuadooCreateForm(events,NULL,serialized,&copy));
	Check(copy->FindControl(RSTRING_CAST(L"swatch"),&copied));
	static PCWSTR const names[] = {L"Colors",L"Labels",L"Selected",L"Mode",L"Font",L"ForeColor"};
	for(UINT n = 0; n < ARRAYSIZE(names); n++)
	{
		TStackRef<IJSONValue> a,b;
		Check(ImageRead(control,names[n],&a));
		Check(ImageRead(copied,names[n],&b));
		REQUIRE(!JSONCompare(a,b));
	}
	Check(form->QueryInterface(__uuidof(IQuadooLayoutControl),reinterpret_cast<PVOID*>(&layout)));
	Check(canvas.Create(400,160));
	for(UINT dpi = 96; dpi <= 192; dpi += 48)
	{
		QuadooForm::Rect requested, effective;
		TStackRef<IQuadooStaticControl> paint;
		Check(form->GetBounds(&requested));
		Check(layout->Arrange(&requested,dpi));
		Check(control->GetEffectiveBounds(&effective));
		REQUIRE(effective.Left == 10 && effective.Width == 101);
		Check(control->QueryInterface(__uuidof(IQuadooStaticControl),reinterpret_cast<PVOID*>(&paint)));
		RECT area = {0,0,MulDiv(effective.Width,dpi,96),MulDiv(effective.Height,dpi,96)};
		Check(paint->Paint(canvas.DC(),&area));
		REQUIRE(canvas.Pixel(area.right / 6,area.bottom / 4) == GetSysColor(COLOR_HIGHLIGHT));
		RECT tiny = {1,1,2,2};
		Check(paint->Paint(canvas.DC(),&tiny));
		RECT zero = {1,1,1,1};
		Check(paint->Paint(canvas.DC(),&zero));
	}
	Check(form->Destroy());
	Check(copy->Destroy());
	control.Release(); copied.Release(); layout.Release(); form.Release(); copy.Release();
	Check(lifetime->GetLiveCount(&live));
	REQUIRE(live == 0);
	static PCWSTR const invalid[] = {
		L"{\"Colors\":[]}",L"{\"Colors\":[1,2],\"Mode\":\"Editable\"}",
		L"{\"Colors\":[1],\"Selected\":1}",L"{\"Colors\":[1],\"Labels\":[\"one\",\"two\"]}"};
	for(UINT n = 0; n < ARRAYSIZE(invalid); n++)
	{
		TStackRef<IJSONObject> bad, props;
		TStackRef<IJSONValue> wrapped;
		Check(Parse(L"{\"type\":\"Form\",\"name\":\"Bad\",\"children\":[{\"type\":\"ColorSwatch\",\"name\":\"bad\"}]}",&bad));
		Check(Parse(invalid[n],&props));
		TStackRef<IJSONValue> children, child;
		TStackRef<IJSONArray> array;
		TStackRef<IJSONObject> object;
		Check(bad->FindNonNullValueW(L"children",&children));
		Check(children->GetArray(&array));
		Check(array->GetValue(0,&child));
		Check(child->GetObject(&object));
		Check(JSONWrapObject(props,&wrapped));
		Check(object->AddValueW(L"properties",wrapped));
		REQUIRE(FAILED(QuadooCreateForm(NULL,NULL,bad,&form)) && !form);
		Check(lifetime->GetLiveCount(&live));
		REQUIRE(live == 0);
	}
Cleanup:
	if(form) form->Destroy();
	if(copy) copy->Destroy();
	return hr;
}
