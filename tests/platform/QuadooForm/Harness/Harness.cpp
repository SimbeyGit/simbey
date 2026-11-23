#include <windows.h>
#include <commctrl.h>
#include "Library\Core\BaseUnknown.h"
#include "Library\Core\Array.h"
#include "Library\Core\MemoryStream.h"
#include "Library\Util\Formatting.h"
#include "..\..\..\..\platform\QuadooForm\Published\QuadooForm.h"
#include "Published\SimbeyCore.h"
#include "..\TestExtension\ExternalControl.h"

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

static UINT g_checks = 0;
static VOID WINAPI AssertTrace(HRESULT error, PCSTR file, UINT line);
static VOID Log(PCWSTR text)
{
	CHAR buffer[4096]; DWORD written;
	INT length = WideCharToMultiByte(CP_UTF8, 0, text, -1, buffer, sizeof(buffer), NULL, NULL);
	if(length > 0) WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buffer, length - 1, &written, NULL);
}
static VOID WINAPI AssertTrace(HRESULT error, PCSTR file, UINT line)
{
	CRString text;
	if(SUCCEEDED(RStrFormatW(&text, L"check 0x%.8x at %S:%u\r\n", error, file, line))) Log(text.c_wstr());
}
static BOOL Expect(BOOL condition, PCWSTR expression, UINT line)
{
	g_checks++;
	if(!condition)
	{
		CRString text;
		if(SUCCEEDED(RStrFormatW(&text, L"FAILED line %u: %ls\r\n", line, expression))) Log(text.c_wstr());
	}
	return condition;
}
#define WIDE_LITERAL_(text) L##text
#define WIDE_LITERAL(text) WIDE_LITERAL_(text)
#define REQUIRE(condition) CheckIf(!Expect(!!(condition), WIDE_LITERAL(#condition), __LINE__), E_FAIL)

static BOOL EqualBounds(const QuadooForm::Rect& a, const QuadooForm::Rect& b)
{ return a.Left == b.Left && a.Top == b.Top && a.Width == b.Width && a.Height == b.Height; }
static HRESULT Parse(PCWSTR text, IJSONObject** definition)
{
	HRESULT hr;
	TStackRef<IJSONValue> value;
	*definition = NULL;
	Check(JSONParse(NULL, text, TStrLenAssert(text), &value));
	Check(value->GetObject(definition));
Cleanup:
	return hr;
}
static HRESULT Property(IQuadooComponent* control, PCWSTR name, IQuadooProperties** properties, UINT* index)
{
	HRESULT hr;
	TStackRef<IQuadooProperties> values;
	TStackRef<IQuadooPropertyDescriptors> descriptors;
	UINT count;
	*properties = NULL; Check(control->GetProperties(&values)); Check(values->GetDescriptors(&descriptors)); Check(descriptors->GetCount(&count));
	for(UINT n = 0; n < count; n++)
	{
		TStackRef<IQuadooPropertyDescriptor> descriptor; CRString text;
		Check(descriptors->GetAt(n, &descriptor));
		Check(descriptor->GetName(&text));
		if(text == RSTRING_CAST(name))
		{
			*index = n;
			*properties = values.Detach();
			hr = S_OK;
			goto Cleanup;
		}
	}
	hr = DISP_E_MEMBERNOTFOUND;
Cleanup:
	return hr;
}
static HRESULT SetInt(IQuadooComponent* control, PCWSTR name, INT value)
{
	HRESULT hr; TStackRef<IQuadooProperties> properties; UINT index;
	Check(Property(control, name, &properties, &index)); Check(properties->SetValueInt(index, value));
Cleanup:
	return hr;
}
static HRESULT Context(UINT dpi, IQuadooEventsBinder* binder, IQuadooConstructionContext** context,
	IQuadooControlRegistry* registry = NULL, QuadooForm::ContextMode mode = QuadooForm::Runtime, HWND host = NULL)
{
	QuadooForm::ContextOptions options;
	ZeroMemory(&options, sizeof(options)); options.cbSize = sizeof(options); options.DPI = dpi;
	options.pBinder = binder; options.pRegistry = registry; options.Mode = mode; options.hwndHost = host;
	return QuadooCreateConstructionContext(&options, context);
}
class CBinder;
class CDelegate : public CBaseUnknown, public IQuadooDelegate
{
	CBinder* m_binder; CRString m_name;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooDelegate)
	END_UNK_MAP
	CDelegate(CBinder* binder, RSTRING name);
	~CDelegate();
	STDMETHOD(Invoke)(IQuadooEventArgs* args);
};
class CBinder : public CBaseUnknown, public IQuadooEventsBinder
{
public:
	UINT binds, posts, created, clicks, changed, closed, destroyed;
	BOOL cancel, destroySender;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooEventsBinder)
	END_UNK_MAP
	CBinder() : binds(0), posts(0), created(0), clicks(0), changed(0), closed(0), destroyed(0), cancel(FALSE), destroySender(FALSE) {}
	STDMETHOD(BindEvent)(RSTRING name, IQuadooDelegate** delegate)
	{
		HRESULT hr = S_OK;
		*delegate = NULL;
		CheckIf(CRString(name) == RSTRING_CAST(L"Missing"), HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
		*delegate = __new CDelegate(this, name); CheckAlloc(*delegate); binds++;
Cleanup:
		return hr;
	}
	STDMETHOD(PostBind)(IQuadooControl* control)
	{
		HRESULT hr;
		QuadooForm::ComponentState state; Check(control->GetState(&state));
		REQUIRE(state == QuadooForm::Binding); REQUIRE(created == 0); posts++;
Cleanup:
		return hr;
	}
};
CDelegate::CDelegate(CBinder* binder, RSTRING name) : m_binder(binder), m_name(name) { m_binder->AddRef(); }
CDelegate::~CDelegate() { m_binder->Release(); }
STDMETHODIMP CDelegate::Invoke(IQuadooEventArgs* args)
{
	HRESULT hr = S_OK;
	if(m_name == RSTRING_CAST(L"Create")) m_binder->created++;
	else if(m_name == RSTRING_CAST(L"Click"))
	{
		m_binder->clicks++;
		if(m_binder->destroySender) { TStackRef<IQuadooComponent> sender; Check(args->GetSender(&sender)); Check(sender->Destroy()); }
	}
	else if(m_name == RSTRING_CAST(L"Changed")) m_binder->changed++;
	else if(m_name == RSTRING_CAST(L"Query"))
	{
		INT wrongType; BOOL invalid;
		REQUIRE(args->GetInt(0, &wrongType) == DISP_E_TYPEMISMATCH);
		REQUIRE(FAILED(args->GetBool(99, &invalid)));
		Check(args->SetBool(0, m_binder->cancel));
	}
	else if(m_name == RSTRING_CAST(L"Closed")) m_binder->closed++;
	else if(m_name == RSTRING_CAST(L"Destroyed")) m_binder->destroyed++;
Cleanup:
	return hr;
}
static const WCHAR Basic[] =
	L"{\"schemaVersion\":1,\"type\":\"Form\",\"name\":\"Main\",\"properties\":{\"Width\":400,\"Height\":300},"
	L"\"events\":{\"OnCreate\":\"Create\",\"OnCloseQuery\":\"Query\",\"OnClose\":\"Closed\",\"OnDestroy\":\"Destroyed\"},"
	L"\"children\":[{\"type\":\"Panel\",\"name\":\"panel\",\"properties\":{\"Width\":380,\"Height\":260},\"children\":["
	L"{\"type\":\"Button\",\"name\":\"btn\",\"properties\":{\"Caption\":\"Click\",\"Left\":10,\"Top\":10},\"events\":{\"OnClick\":\"Click\"}},"
	L"{\"type\":\"TextBox\",\"name\":\"edit\",\"properties\":{\"Top\":50,\"Width\":200},\"events\":{\"OnChange\":\"Changed\"}},"
	L"{\"type\":\"Label\",\"name\":\"label\",\"properties\":{\"Top\":90,\"Caption\":\"Native QuadooForm\",\"AutoSize\":true}}]}]}";

static HRESULT Lifecycle()
{
	HRESULT hr;
	TStackRef<CBinder> binder; TStackRef<IJSONObject> definition;
	TStackRef<IQuadooForm> form; TStackRef<IQuadooControl> button, edit, panel;
	TStackRef<IQuadooWinControl> buttonWindow, editWindow;
	HWND hwnd, childWindow, after; UINT count;
	binder.Attach(__new CBinder); CheckAlloc(binder); Check(Parse(Basic, &definition)); Check(QuadooCreateForm(binder, NULL, definition, &form));
	REQUIRE(binder->binds == 6 && binder->posts == 1 && binder->created == 1);
	Check(form->GetHandle(&hwnd)); REQUIRE(IsWindow(hwnd)); Check(form->GetChildCount(&count)); REQUIRE(count == 1);
	Check(form->FindControl(RSTRING_CAST(L"btn"), &button)); Check(form->FindControl(RSTRING_CAST(L"edit"), &edit)); Check(form->FindControl(RSTRING_CAST(L"panel"), &panel));
	Check(button->QueryInterface(__uuidof(IQuadooWinControl), reinterpret_cast<PVOID*>(&buttonWindow)));
	Check(buttonWindow->GetHandle(&childWindow)); SendMessageW(childWindow, BM_CLICK, 0, 0); REQUIRE(binder->clicks == 1);
	REQUIRE(button->SetName(RSTRING_CAST(L"edit")) == QuadooForm::EDuplicateName);
	Check(button->SetName(RSTRING_CAST(L"renamed"))); button.Release(); Check(form->FindControl(RSTRING_CAST(L"renamed"), &button));
	REQUIRE(form->RegisterControl(button) == E_INVALIDARG); REQUIRE(form->UnregisterControl(panel) == QuadooForm::EInvalidState);
	Check(edit->QueryInterface(__uuidof(IQuadooWinControl), reinterpret_cast<PVOID*>(&editWindow))); Check(editWindow->GetHandle(&after));
	SetWindowTextW(after, L"typed text"); REQUIRE(binder->changed == 1);
	Check(form->ShowForm(SW_SHOWNOACTIVATE)); REQUIRE(IsWindowVisible(hwnd)); Check(form->HideForm()); Check(form->GetHandle(&after)); REQUIRE(after == hwnd && IsWindow(hwnd));
	binder->cancel = TRUE; REQUIRE(form->Close() == S_FALSE); REQUIRE(IsWindow(hwnd));
	binder->destroySender = TRUE; SendMessageW(childWindow, BM_CLICK, 0, 0); REQUIRE(binder->clicks == 2);
	REQUIRE(buttonWindow->GetHandle(&after) == QuadooForm::EDestroyed && after == NULL);
	binder->cancel = FALSE; Check(form->Close()); REQUIRE(!IsWindow(hwnd) && binder->closed == 1 && binder->destroyed == 1);
	REQUIRE(form->Destroy() == S_FALSE); Check(form->GetChildCount(&count)); REQUIRE(count == 0);
	{ TStackRef<IQuadooControlContainer> parent; Check(button->GetParent(&parent)); REQUIRE(!parent); }
Cleanup:
	if(form) form->Destroy();
	return hr;
}
class CObserver : public CBaseUnknown, public IQuadooPropertyObserver
{
public:
	UINT properties, bounds; IQuadooPropertyNotifications* source; IQuadooPropertyObserver* remove;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooPropertyObserver)
	END_UNK_MAP
	CObserver() : properties(0), bounds(0), source(NULL), remove(NULL) {}
	STDMETHOD(OnPropertyChanged)(IQuadooComponent*, UINT) { properties++; if(source && remove) source->RemoveObserver(remove); return S_OK; }
	STDMETHOD(OnBoundsChanged)(IQuadooControl*, const QuadooForm::Rect*) { bounds++; return S_OK; }
};
static HRESULT LayoutAndDPI()
{
	HRESULT hr;
	TStackRef<IJSONObject> definition; TStackRef<IQuadooConstructionContext> context;
	TStackRef<IQuadooControl> root, top, client, anchored, label;
	TStackRef<IQuadooForm> form; TStackRef<IQuadooLayoutControl> layout;
	TStackRef<IQuadooPropertyNotifications> notifications; TStackRef<CObserver> observer, removed;
	QuadooForm::Rect bounds, stored, original;
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"Layout\",\"properties\":{\"Width\":400,\"Height\":300,\"Padding\":{\"Left\":10,\"Top\":10,\"Right\":10,\"Bottom\":10}},\"children\":["
		L"{\"type\":\"Panel\",\"name\":\"top\",\"properties\":{\"Align\":\"Top\",\"Height\":30}},"
		L"{\"type\":\"Panel\",\"name\":\"left\",\"properties\":{\"Align\":\"Left\",\"Width\":80}},"
		L"{\"type\":\"Panel\",\"name\":\"client\",\"properties\":{\"Align\":\"Client\"}},"
		L"{\"type\":\"Button\",\"name\":\"anchor\",\"properties\":{\"Left\":300,\"Top\":250,\"Width\":50,\"Height\":20,\"Anchors\":12}},"
		L"{\"type\":\"Label\",\"name\":\"label\",\"properties\":{\"Caption\":\"AutoSize\",\"AutoSize\":true}}]}", &definition));
	Check(Context(96, NULL, &context)); Check(QuadooCreateControl(context, NULL, definition, &root));
	Check(root->QueryInterface(__uuidof(IQuadooForm), reinterpret_cast<PVOID*>(&form)));
	Check(root->QueryInterface(__uuidof(IQuadooLayoutControl), reinterpret_cast<PVOID*>(&layout)));
	Check(form->FindControl(RSTRING_CAST(L"top"), &top)); Check(form->FindControl(RSTRING_CAST(L"client"), &client));
	Check(form->FindControl(RSTRING_CAST(L"anchor"), &anchored)); Check(form->FindControl(RSTRING_CAST(L"label"), &label));
	Check(top->GetEffectiveBounds(&bounds)); REQUIRE(bounds.Left == 10 && bounds.Top == 10 && bounds.Width == 380 && bounds.Height == 30);
	Check(client->GetEffectiveBounds(&bounds)); REQUIRE(bounds.Left == 90 && bounds.Top == 40 && bounds.Width == 300 && bounds.Height == 250);
	Check(label->GetEffectiveBounds(&bounds)); REQUIRE(bounds.Width > 0 && bounds.Width != 100 && bounds.Height > 0);
	Check(anchored->GetBounds(&stored)); Check(root->GetBounds(&original)); bounds = original; bounds.Width += 100; bounds.Height += 100; Check(root->SetBounds(&bounds));
	Check(anchored->GetEffectiveBounds(&bounds)); REQUIRE(bounds.Left == 400 && bounds.Top == 350); Check(anchored->GetBounds(&bounds)); REQUIRE(EqualBounds(stored, bounds));
	Check(root->SetBounds(&original));
	for(UINT dpi = 96; dpi <= 192; dpi += 48)
	{
		Check(layout->Arrange(&original, dpi)); Check(client->GetEffectiveBounds(&bounds)); REQUIRE(bounds.Left == 90 && bounds.Width == 300);
		TStackRef<IQuadooWinControl> window; HWND hwnd; RECT physical;
		Check(client->QueryInterface(__uuidof(IQuadooWinControl), reinterpret_cast<PVOID*>(&window))); Check(window->GetHandle(&hwnd)); GetWindowRect(hwnd, &physical);
		REQUIRE(physical.right - physical.left == MulDiv(bounds.Width, dpi, 96));
	}
	Check(layout->Arrange(&original, 96));
	observer.Attach(__new CObserver); removed.Attach(__new CObserver); CheckAlloc(observer); CheckAlloc(removed);
	Check(top->QueryInterface(__uuidof(IQuadooPropertyNotifications), reinterpret_cast<PVOID*>(&notifications)));
	Check(notifications->AddObserver(observer)); Check(notifications->AddObserver(removed)); observer->source = notifications; observer->remove = removed;
	Check(form->BeginUpdate()); Check(SetInt(top, L"Height", 40)); Check(SetInt(top, L"Height", 45)); REQUIRE(observer->properties == 0);
	Check(form->EndUpdate()); REQUIRE(observer->properties == 1 && observer->bounds == 1 && removed->properties == 0 && removed->bounds == 0);
	REQUIRE(SetInt(top, L"Align", QuadooForm::AlignClient) == E_INVALIDARG);
Cleanup:
	if(root) root->Destroy();
	return hr;
}
static HRESULT ScrollingAndPersistence()
{
	HRESULT hr;
	TStackRef<IJSONObject> definition, before, after, frameDefinition, frameSaved;
	TStackRef<IQuadooForm> form, reloaded; TStackRef<IQuadooFormSerializer> serializer;
	TStackRef<IQuadooControl> scrollControl, button, frameChild; TStackRef<IQuadooScrollable> scroll;
	TStackRef<IQuadooChildWindowHost> host; TStackRef<IQuadooControlContainer> scrollContainer;
	TStackRef<IQuadooWinControl> buttonWindow; TStackRef<IQuadooFrame> first, second;
	QuadooForm::Rect stored, current; QuadooForm::Point position = { 0, 200 }, actual;
	HWND viewport, content, child; UINT count; GUID id1, id2;
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"ScrollForm\",\"extra\":{\"preserve\":true},\"children\":[{\"type\":\"ScrollingPanel\",\"name\":\"scroll\",\"properties\":{\"Width\":200,\"Height\":150},\"children\":[{\"type\":\"Button\",\"name\":\"far\",\"properties\":{\"Left\":20,\"Top\":700}}, {\"type\":\"ScrollingPanel\",\"name\":\"nested\",\"properties\":{\"Width\":100,\"Height\":100},\"children\":[{\"type\":\"TextBox\",\"name\":\"nestedEdit\",\"properties\":{\"Top\":500}}]}]}]}", &definition));
	Check(QuadooCreateForm(NULL, NULL, definition, &form)); Check(QuadooGetFormSerializer(&serializer)); Check(serializer->Serialize(form, &before));
	Check(form->FindControl(RSTRING_CAST(L"scroll"), &scrollControl)); Check(form->FindControl(RSTRING_CAST(L"far"), &button));
	Check(scrollControl->QueryInterface(__uuidof(IQuadooScrollable), reinterpret_cast<PVOID*>(&scroll)));
	Check(scrollControl->QueryInterface(__uuidof(IQuadooControlContainer), reinterpret_cast<PVOID*>(&scrollContainer)));
	Check(scrollControl->QueryInterface(__uuidof(IQuadooChildWindowHost), reinterpret_cast<PVOID*>(&host)));
	Check(button->QueryInterface(__uuidof(IQuadooWinControl), reinterpret_cast<PVOID*>(&buttonWindow)));
	Check(scrollContainer->GetHandle(&viewport)); Check(host->GetChildHostHandle(&content)); Check(buttonWindow->GetHandle(&child));
	REQUIRE(content != viewport && ::GetParent(child) == content && ::GetParent(content) == viewport);
	Check(button->GetBounds(&stored)); Check(scroll->SetScrollPosition(&position)); Check(scroll->GetScrollPosition(&actual)); REQUIRE(actual.Y == 200);
	SendMessageW(viewport,WM_VSCROLL,SB_TOP,0);
	for(UINT n = 0; n < 20; n++)
	{
		SendMessageW(viewport,WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA),0);
		SendMessageW(viewport,WM_VSCROLL,SB_LINEUP,0);
		SendMessageW(viewport,WM_VSCROLL,SB_PAGEUP,0);
		SendMessageW(viewport,WM_HSCROLL,SB_LINELEFT,0);
	}
	Check(scroll->GetScrollPosition(&actual)); REQUIRE(actual.X == 0 && actual.Y == 0);
	{ QuadooForm::Point invalid = {-1,-1}; REQUIRE(scroll->SetScrollPosition(&invalid) == E_INVALIDARG); }
	Check(scroll->SetScrollPosition(&position));
	Check(button->GetBounds(&current)); REQUIRE(EqualBounds(stored, current)); Check(serializer->Serialize(form, &after)); REQUIRE(JSONCompareObjects(before, after) == 0);
	Check(Parse(L"{\"type\":\"Frame\",\"id\":\"11111111-1111-1111-1111-111111111111\",\"name\":\"first\",\"children\":[{\"type\":\"Label\",\"id\":\"22222222-2222-2222-2222-222222222222\",\"name\":\"privateLabel\"}]}", &frameDefinition));
	Check(QuadooCreateFrame(NULL, scrollContainer, frameDefinition, &first));
	{ TStackRef<IJSONValue> name; Check(JSONCreateStringW(L"second", 6, &name)); Check(frameDefinition->AddValueW(L"name", name)); }
	Check(QuadooCreateFrame(NULL, scrollContainer, frameDefinition, &second));
	Check(first->GetID(&id1)); Check(second->GetID(&id2)); REQUIRE(IsEqualGUID(id1, id2));
	REQUIRE(FAILED(form->FindControl(RSTRING_CAST(L"privateLabel"), &frameChild))); Check(first->FindControl(RSTRING_CAST(L"privateLabel"), &frameChild));
	REQUIRE(second->SetName(RSTRING_CAST(L"first")) == QuadooForm::EDuplicateName);
	REQUIRE(QuadooValidateControlName(second, RSTRING_CAST(L"first")) == QuadooForm::EDuplicateName);
	after.Release(); Check(serializer->Serialize(form, &after)); REQUIRE(JSONCompareObjects(before, after) == 0);
	Check(serializer->Serialize(first, &frameSaved)); Check(first->GetChildCount(&count)); REQUIRE(count == 1);
	Check(QuadooCreateForm(NULL, NULL, after, &reloaded));
	{ TStackRef<IJSONObject> saved; Check(serializer->Serialize(reloaded, &saved)); REQUIRE(JSONCompareObjects(saved, after) == 0); }
	// The returned event object must not alias the control's mutable binding metadata.
	{ TStackRef<IJSONValue> events; TStackRef<IJSONObject> object; Check(after->FindNonNullValueW(L"events", &events)); Check(events->GetObject(&object)); TStackRef<IJSONValue> handler; Check(JSONCreateStringW(L"Missing", 7, &handler)); Check(object->AddValueW(L"OnCreate", handler)); }
	{ TStackRef<IJSONObject> saved; Check(serializer->Serialize(form, &saved)); REQUIRE(JSONCompareObjects(saved, after) != 0); }
Cleanup:
	if(reloaded) reloaded->Destroy(); if(form) form->Destroy();
	return hr;
}
static DWORD WINAPI WrongThread(PVOID parameter)
{
	IQuadooForm* form = static_cast<IQuadooForm*>(parameter); HWND hwnd = reinterpret_cast<HWND>(1);
	HRESULT result = form->GetHandle(&hwnd); form->Release();
	return result == QuadooForm::EWrongThread && hwnd == NULL ? 0 : 1;
}
static HRESULT Failures()
{
	HRESULT hr;
	TStackRef<IJSONObject> definition; TStackRef<IQuadooForm> form;
	TStackRef<CBinder> binder; TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooConstructionContext> context; TStackRef<IQuadooControl> root;
	TStackRef<IQuadooControlFactory> factory; TStackRef<IQuadooFactoryLifetime> lifetime;
	UINT count; HWND hwnd; HANDLE thread = NULL;
	static PCWSTR const failures[] =
	{
		L"{\"type\":\"Form\",\"children\":[{\"type\":\"Panel\",\"name\":\"same\"},{\"type\":\"Button\",\"name\":\"same\"}]}",
		L"{\"type\":\"Form\",\"children\":[{\"type\":\"Frame\"}]}",
		L"{\"type\":\"Form\",\"children\":[{\"type\":\"Unknown\"}]}",
		L"{\"type\":\"Form\",\"properties\":{\"Width\":-1}}",
		L"{\"type\":\"Form\",\"properties\":{\"Unsupported\":true}}",
		L"{\"type\":\"Form\",\"children\":[{\"type\":\"Panel\",\"properties\":{\"Align\":\"Client\"}},{\"type\":\"Panel\",\"properties\":{\"Align\":\"Client\"}}]}",
		L"{\"type\":\"Form\",\"children\":[{\"type\":\"Button\",\"id\":\"11111111-1111-1111-1111-111111111111\"},{\"type\":\"Button\",\"id\":\"11111111-1111-1111-1111-111111111111\"}]}"
	};
	Check(QuadooGetControlRegistry(&registry)); Check(Context(96, NULL, &context, registry));
	for(UINT n = 0; n < ARRAYSIZE(failures); n++)
	{
		definition.Release(); Check(Parse(failures[n], &definition)); REQUIRE(FAILED(QuadooCreateControl(context, NULL, definition, &root)) && !root);
		factory.Release(); lifetime.Release(); Check(registry->FindFactory(RSTRING_CAST(L"Form"), &factory));
		Check(factory->QueryInterface(__uuidof(IQuadooFactoryLifetime), reinterpret_cast<PVOID*>(&lifetime))); Check(lifetime->GetLiveCount(&count)); REQUIRE(count == 0);
	}
	binder.Attach(__new CBinder); CheckAlloc(binder); definition.Release();
	Check(Parse(L"{\"type\":\"Form\",\"events\":{\"OnCreate\":\"Create\"},\"children\":[{\"type\":\"Button\",\"events\":{\"OnClick\":\"Missing\"}}]}", &definition));
	REQUIRE(FAILED(QuadooCreateForm(binder, NULL, definition, &form)) && !form && binder->created == 0);
	definition.Release(); Check(Parse(L"{\"type\":\"Form\",\"name\":\"Thread\"}", &definition)); Check(QuadooCreateForm(NULL, NULL, definition, &form));
	form->AddRef(); thread = CreateThread(NULL, 0, WrongThread, static_cast<IQuadooForm*>(form), 0, NULL); CheckIf(!thread, HrEnsureLastError());
	WaitForSingleObject(thread, INFINITE); DWORD exitCode; GetExitCodeThread(thread, &exitCode); REQUIRE(exitCode == 0);
	Check(form->GetHandle(&hwnd)); REQUIRE(DestroyWindow(hwnd));
	{ QuadooForm::ComponentState state; Check(form->GetState(&state)); REQUIRE(state == QuadooForm::Destroyed); }
Cleanup:
	if(thread) CloseHandle(thread); if(root) root->Destroy(); if(form) form->Destroy();
	return hr;
}
static IQuadooForm* g_modal = NULL;
static VOID CALLBACK ModalTimer(HWND hwnd, UINT, UINT_PTR id, DWORD) { KillTimer(hwnd, id); if(g_modal) g_modal->Close(); }
static const UINT ModalQuitMarker = WM_APP + 0x51;
static BOOL ReadModalQuit(MSG* quit, BOOL* markerSeen)
{
	MSG message;
	ZeroMemory(quit, sizeof(*quit)); *markerSeen = FALSE;
	// PostQuitMessage synthesizes WM_QUIT after posted messages drain. On XP,
	// filtering only for WM_QUIT can return FALSE while other messages remain.
	// Pump like the outer application loop, with a bound so a lost quit fails.
	for(UINT n = 0; n < 256 && PeekMessageW(&message, NULL, 0, 0, PM_REMOVE); n++)
	{
		if(message.message == WM_QUIT) { *quit = message; return TRUE; }
		if(!message.hwnd && message.message == ModalQuitMarker) *markerSeen = TRUE;
		TranslateMessage(&message); DispatchMessageW(&message);
	}
	return FALSE;
}
static HRESULT Modal()
{
	HRESULT hr;
	TStackRef<IJSONObject> definition; TStackRef<IQuadooForm> owner, form;
	TStackRef<IQuadooProperties> properties; UINT index; HWND ownerWindow, window; LRESULT result;
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"Modal\"}", &definition));
	Check(QuadooCreateForm(NULL, NULL, definition, &owner)); Check(owner->GetHandle(&ownerWindow));
	Check(QuadooCreateForm(NULL, owner, definition, &form)); Check(form->GetHandle(&window));
	Check(Property(form, L"ModalResult", &properties, &index)); Check(properties->SetValueLong(index, 42));
	g_modal = form; REQUIRE(SetTimer(window, 1, 10, ModalTimer) != 0); Check(form->ShowModal(&result));
	g_modal = NULL;
	REQUIRE(result == 42 && IsWindowEnabled(ownerWindow)); form.Release(); properties.Release();
	static const INT exitCodes[] = { 123, 0, -123 };
	for(UINT n = 0; n < ARRAYSIZE(exitCodes); n++)
	{
		MSG message; BOOL markerSeen;
		Check(QuadooCreateForm(NULL, owner, definition, &form)); Check(form->GetHandle(&window)); PostQuitMessage(exitCodes[n]);
		REQUIRE(form->ShowModal(&result) == HRESULT_FROM_WIN32(ERROR_CANCELLED));
		REQUIRE(result == 0 && !IsWindow(window)); REQUIRE(IsWindowEnabled(ownerWindow));
		// Keep an ordinary posted message ahead of the pending quit, including
		// on hosts where owner activation happens not to leave messages queued.
		CheckIf(!PostThreadMessageW(GetCurrentThreadId(), ModalQuitMarker, 0, 0), HrEnsureLastError());
		REQUIRE(ReadModalQuit(&message, &markerSeen)); REQUIRE(static_cast<INT>(message.wParam) == exitCodes[n]); REQUIRE(markerSeen);
		form.Release();
	}
	{
		MSG message; BOOL markerSeen;
		CheckIf(!PostThreadMessageW(GetCurrentThreadId(), ModalQuitMarker, 0, 0), HrEnsureLastError());
		REQUIRE(!ReadModalQuit(&message, &markerSeen)); REQUIRE(markerSeen);
	}
Cleanup:
	g_modal = NULL; if(form) form->Destroy(); if(owner) owner->Destroy();
	return hr;
}
static HRESULT ShowExample(PCWSTR example = Basic)
{
	HRESULT hr;
	TStackRef<IJSONObject> definition; TStackRef<IQuadooForm> form; TStackRef<CBinder> binder; TStackRef<IQuadooFormKeyboard> keyboard;
	HWND hwnd; MSG message;
	binder.Attach(__new CBinder); CheckAlloc(binder); Check(Parse(example, &definition)); Check(QuadooCreateForm(binder, NULL, definition, &form));
	Check(form->GetHandle(&hwnd)); Check(form->ShowForm(SW_SHOW));
	Check(form->QueryInterface(__uuidof(IQuadooFormKeyboard),reinterpret_cast<PVOID*>(&keyboard)));
	while(IsWindow(hwnd))
	{
		INT received = GetMessageW(&message, NULL, 0, 0); if(received == 0) break; CheckIf(received < 0, HrEnsureLastError());
		BOOL processed; Check(keyboard->PreprocessMessage(&message,&processed));
		if(!processed) { TranslateMessage(&message); DispatchMessageW(&message); }
	}
Cleanup:
	if(form) form->Destroy();
	return hr;
}
class CReport : public CBaseUnknown, public IQuadooConstructionReport
{
public:
	UINT errors; CRString firstPath, detail;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooConstructionReport)
	END_UNK_MAP
	CReport() : errors(0) {}
	STDMETHOD(OnError)(HRESULT, RSTRING path, RSTRING text) { errors++; if(!firstPath) firstPath = path; detail = text; return S_OK; }
};
class CBrokenFactory : public CBaseUnknown, public IQuadooControlFactory, public IQuadooFactoryLifetime
{
	IQuadooControlFactory* m_base;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooControlFactory)
		UNK_INTERFACE(IQuadooFactoryLifetime)
	END_UNK_MAP
	CBrokenFactory(IQuadooControlFactory* base) { SetInterface(m_base, base); }
	~CBrokenFactory() { SafeRelease(m_base); }
	STDMETHOD(GetTypeName)(RSTRING* name) { return RStrCreateW(6, L"Broken", name); }
	STDMETHOD(GetProperties)(IQuadooPropertyDescriptors** values) { return m_base->GetProperties(values); }
	STDMETHOD(GetEvents)(IQuadooEventDescriptors** values) { return m_base->GetEvents(values); }
	STDMETHOD(GetLiveCount)(UINT* count) { *count = 0; return S_OK; }
	STDMETHOD(CreateControl)(IQuadooConstructionContext* context, IQuadooControlContainer* parent, IJSONObject* definition, IQuadooControl** control)
	{
		HRESULT hr;
		TStackRef<IJSONValue> clone, type; TStackRef<IJSONObject> object; TStackRef<IQuadooControl> child;
		*control = NULL; Check(JSONCloneObject(definition, &clone, TRUE)); Check(clone->GetObject(&object));
		Check(JSONCreateStringW(L"Button", 6, &type)); Check(object->AddValueW(L"type", type));
		Check(m_base->CreateControl(context, parent, object, &child));
		// Deliberately violate factory rollback to exercise the transaction's recovery.
		hr = E_FAIL;
Cleanup:
		return hr;
	}
};
static HRESULT ExtensionAndDesign()
{
	HRESULT hr;
	TStackRef<IQuadooControlRegistry> registry; TStackRef<IQuadooControlFactory> buttonFactory;
	TStackRef<CBrokenFactory> broken; TStackRef<CReport> report; TStackRef<CBinder> binder;
	TStackRef<IQuadooConstructionContext> context, design; TStackRef<IQuadooControl> root, child, designed;
	TStackRef<IQuadooForm> form; TStackRef<IJSONObject> definition, buttonDefinition, saved;
	TStackRef<IQuadooFormSerializer> serializer; TStackRef<IQuadooFactoryLifetime> lifetime;
	QuadooForm::ContextOptions options; UINT count, live; HWND host = NULL;

	Check(QuadooGetControlRegistry(&registry));
	Check(registry->FindFactory(RSTRING_CAST(L"Button"), &buttonFactory));

	{
		PVOID missing = NULL;
		REQUIRE(buttonFactory->QueryInterface(__uuidof(IQuadooFrame), &missing) == E_NOINTERFACE && missing == NULL);
	}

	broken.Attach(__new CBrokenFactory(buttonFactory)); report.Attach(__new CReport); binder.Attach(__new CBinder);
	CheckAlloc(broken); CheckAlloc(report); CheckAlloc(binder); Check(registry->RegisterFactory(broken));
	REQUIRE(registry->RegisterFactory(broken) == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS));
	ZeroMemory(&options, sizeof(options)); options.cbSize = sizeof(options); options.DPI = 96; options.pRegistry = registry; options.pReport = report;
	Check(QuadooCreateConstructionContext(&options, &context));
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"Extension\"}", &definition)); Check(QuadooCreateControl(context, NULL, definition, &root));
	Check(root->QueryInterface(__uuidof(IQuadooForm), reinterpret_cast<PVOID*>(&form)));
	Check(Parse(L"{\"type\":\"Broken\",\"name\":\"fault\"}", &buttonDefinition));
	REQUIRE(QuadooCreateControl(context, form, buttonDefinition, &child) == E_FAIL && !child);
	Check(form->GetChildCount(&count)); REQUIRE(count == 0 && report->errors > 0);
	Check(buttonFactory->QueryInterface(__uuidof(IQuadooFactoryLifetime), reinterpret_cast<PVOID*>(&lifetime))); Check(lifetime->GetLiveCount(&live)); REQUIRE(live == 0);
	buttonDefinition.Release(); Check(Parse(L"{\"type\":\"Button\",\"name\":\"dynamic\",\"properties\":{\"Width\":120}}", &buttonDefinition));
	Check(QuadooCreateControl(context, form, buttonDefinition, &child));
	REQUIRE(registry->UnregisterFactory(RSTRING_CAST(L"Button")) == HRESULT_FROM_WIN32(ERROR_BUSY));
	REQUIRE(form->RegisterControl(child) == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS));
	Check(child->Destroy()); child.Release(); Check(lifetime->GetLiveCount(&live)); REQUIRE(live == 0);
	Check(registry->UnregisterFactory(RSTRING_CAST(L"Button"))); Check(registry->RegisterFactory(buttonFactory));
	host = CreateWindowExW(0, L"STATIC", L"Design host", WS_OVERLAPPED, 0, 0, 800, 600, NULL, NULL, GetModuleHandle(NULL), NULL); CheckIf(!host, HrEnsureLastError());
	Check(Context(144, binder, &design, NULL, QuadooForm::Design, host)); definition.Release();
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"Design\",\"properties\":{\"FutureSetting\":123},\"events\":{\"OnCreate\":\"Missing\"},\"children\":[{\"type\":\"Button\",\"name\":\"futureButton\",\"events\":{\"OnClick\":\"Missing\"}}]}", &definition));
	Check(QuadooCreateControl(design, NULL, definition, &designed)); REQUIRE(binder->binds == 0 && binder->posts == 0 && binder->created == 0);
	{ TStackRef<IQuadooWinControl> window; HWND hwnd; Check(designed->QueryInterface(__uuidof(IQuadooWinControl), reinterpret_cast<PVOID*>(&window))); Check(window->GetHandle(&hwnd)); REQUIRE(::GetParent(hwnd) == host); }
	Check(QuadooGetFormSerializer(&serializer)); Check(serializer->Serialize(designed, &saved));
	{ TStackRef<IJSONValue> value; TStackRef<IJSONObject> properties; INT number; Check(saved->FindNonNullValueW(L"properties", &value)); Check(value->GetObject(&properties)); value.Release(); Check(properties->FindNonNullValueW(L"FutureSetting", &value)); Check(value->GetInteger(&number)); REQUIRE(number == 123); }
	// Reading mutable JSON does not permit bypassing property validation or observers.
	{ TStackRef<IQuadooProperties> properties; UINT index; TStackRef<IJSONValue> value, changed; TStackRef<IJSONObject> font;
		Check(Property(designed, L"Font", &properties, &index)); Check(properties->GetValueJSON(index, &value)); Check(value->GetObject(&font));
		Check(JSONCreateInteger(200, &changed)); Check(font->AddValueW(L"Size", changed)); value.Release(); font.Release();
		Check(properties->GetValueJSON(index, &value)); Check(value->GetObject(&font)); changed.Release(); Check(font->FindNonNullValueW(L"Size", &changed)); INT size; Check(changed->GetInteger(&size)); REQUIRE(size == 9);
		TStackRef<IQuadooPropertyDescriptors> descriptors; TStackRef<IQuadooPropertyDescriptor> descriptor;
		Check(properties->GetDescriptors(&descriptors)); Check(descriptors->Find(RSTRING_CAST(L"Align"), &descriptor)); UINT enums; Check(descriptor->GetEnumCount(&enums)); REQUIRE(enums == 6);
		REQUIRE(descriptor->GetDefaultValue(NULL) == E_POINTER);
	}
Cleanup:
	if(designed) designed->Destroy(); if(root) root->Destroy(); if(host) DestroyWindow(host);
	return hr;
}

static BOOL PumpPendingMessages()
{
	MSG message;
	for(UINT n = 0; n < 256 && PeekMessageW(&message, NULL, 0, 0, PM_REMOVE); n++)
	{
		if(message.message == WM_QUIT) { PostQuitMessage(static_cast<INT>(message.wParam)); return TRUE; }
		TranslateMessage(&message); DispatchMessageW(&message);
	}
	return FALSE;
}
#include "GuiResourceTests.h"
static HRESULT WarmNativeTextInput()
{
	HRESULT hr;
	HWND host = NULL, edit = NULL;
	GuiResourceCounts counts;
	// Let the first focused EDIT initialize while it is still alive. Destroying
	// it before pumping skips lazy text-input/theme work on older Windows.
	host = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPEDWINDOW, 0, 0, 400, 200, NULL, NULL, GetModuleHandle(NULL), NULL);
	CheckIf(!host, HrEnsureLastError());
	edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 10, 10, 100, 28, host, NULL, GetModuleHandle(NULL), NULL);
	CheckIf(!edit, HrEnsureLastError()); ShowWindow(host, SW_SHOWNOACTIVATE); SetFocus(edit);
	Check(SettleGuiResources(NULL, &counts)); ShowWindow(host, SW_HIDE);
Cleanup:
	if(host) DestroyWindow(host); GdiFlush();
	return hr;
}
static HRESULT SizingFocusAndResources()
{
	HRESULT hr;
	TStackRef<IJSONObject> definition; TStackRef<IQuadooForm> form;
	TStackRef<IQuadooControl> button, edit, autoPanel, label; TStackRef<IQuadooWinControl> buttonWindow, editWindow;
	TStackRef<IQuadooLayoutControl> layout; HWND rootWindow, firstWindow, secondWindow; QuadooForm::Rect bounds, stored;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooConstructionContext> context;
	CGuiResourceMonitor resources(L"Sizing/focus resources");
	Check(WarmNativeTextInput());
	Check(GuiResourceMonitorTests());
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"Sizing\",\"properties\":{\"Width\":400,\"Height\":300,\"MinWidth\":250,\"MaxWidth\":500},\"children\":[{\"type\":\"Button\",\"name\":\"first\",\"properties\":{\"TabOrder\":1,\"Margins\":{\"Left\":3,\"Right\":5},\"Align\":\"Top\",\"Height\":30}}, {\"type\":\"TextBox\",\"name\":\"second\",\"properties\":{\"TabOrder\":2,\"Top\":50}}, {\"type\":\"Panel\",\"name\":\"auto\",\"properties\":{\"AutoSize\":true,\"Top\":100,\"Padding\":{\"Right\":5,\"Bottom\":7}},\"children\":[{\"type\":\"Label\",\"name\":\"inside\",\"properties\":{\"Left\":10,\"Top\":20,\"Width\":80,\"Height\":15}}]}]}", &definition));
	Check(QuadooCreateForm(NULL, NULL, definition, &form)); Check(form->GetHandle(&rootWindow));
	Check(form->FindControl(RSTRING_CAST(L"first"), &button)); Check(form->FindControl(RSTRING_CAST(L"second"), &edit));
	Check(form->FindControl(RSTRING_CAST(L"auto"), &autoPanel)); Check(form->FindControl(RSTRING_CAST(L"inside"), &label));
	Check(button->GetEffectiveBounds(&bounds)); REQUIRE(bounds.Left == 3 && bounds.Width == 392 && bounds.Height == 30);
	Check(autoPanel->GetEffectiveBounds(&bounds)); REQUIRE(bounds.Width == 95 && bounds.Height == 42);
	Check(form->GetBounds(&stored)); bounds = stored; bounds.Width = 100; Check(form->SetBounds(&bounds)); Check(form->GetEffectiveBounds(&bounds)); REQUIRE(bounds.Width == 250);
	bounds = stored; bounds.Width = 600; Check(form->SetBounds(&bounds)); Check(form->GetEffectiveBounds(&bounds)); REQUIRE(bounds.Width == 500);
	Check(form->SetBounds(&stored)); Check(button->QueryInterface(__uuidof(IQuadooWinControl), reinterpret_cast<PVOID*>(&buttonWindow))); Check(edit->QueryInterface(__uuidof(IQuadooWinControl), reinterpret_cast<PVOID*>(&editWindow)));
	Check(buttonWindow->GetHandle(&firstWindow)); Check(editWindow->GetHandle(&secondWindow)); Check(form->ShowForm(SW_SHOWNOACTIVATE));
	REQUIRE(GetNextDlgTabItem(rootWindow, NULL, FALSE) == firstWindow); Check(buttonWindow->SetTabOrder(3)); REQUIRE(GetNextDlgTabItem(rootWindow, NULL, FALSE) == secondWindow);
	Check(editWindow->SetFocus()); REQUIRE(GetFocus() == secondWindow); Check(form->HideForm());
	Check(form->QueryInterface(__uuidof(IQuadooLayoutControl), reinterpret_cast<PVOID*>(&layout)));
	for(UINT dpi = 96; dpi <= 192; dpi += 48) { Check(layout->Arrange(&stored, dpi)); Check(button->GetEffectiveBounds(&bounds)); REQUIRE(bounds.Left == 3 && bounds.Width == 392); }
	Check(form->Destroy()); form.Release(); button.Release(); edit.Release(); autoPanel.Release(); label.Release(); buttonWindow.Release(); editWindow.Release(); layout.Release();
	GdiFlush();
	Check(QuadooGetControlRegistry(&registry)); Check(Context(0, NULL, &context, registry));
	while(!resources.Complete())
	{
		{
			TStackRef<IQuadooForm> cycle; TStackRef<IQuadooControl> cycleRoot, cycleEdit;
			TStackRef<IQuadooWinControl> cycleWindow; TStackRef<IQuadooLayoutControl> cycleLayout;
			TArray<HWND> windows; GuiResourceCounts counts;
			Check(QuadooCreateControl(context, NULL, definition, &cycleRoot));
			Check(cycleRoot->QueryInterface(__uuidof(IQuadooForm), reinterpret_cast<PVOID*>(&cycle)));
			Check(cycle->FindControl(RSTRING_CAST(L"second"), &cycleEdit)); Check(cycleEdit->QueryInterface(__uuidof(IQuadooWinControl), reinterpret_cast<PVOID*>(&cycleWindow)));
			Check(cycle->QueryInterface(__uuidof(IQuadooLayoutControl), reinterpret_cast<PVOID*>(&cycleLayout)));
			Check(cycle->ShowForm(SW_SHOWNOACTIVATE)); Check(cycleWindow->SetFocus()); Check(SettleGuiResources(NULL, &counts)); Check(cycle->HideForm());
			for(UINT dpi = 96; dpi <= 192; dpi += 48) { Check(cycleLayout->Arrange(&stored, dpi)); }
			Check(CollectNativeWindows(cycleRoot, windows));
			Check(cycle->Destroy());
			for(sysint n = 0; n < windows.Length(); n++) { REQUIRE(!IsWindow(windows[n])); }
		}
		static PCWSTR const kinds[] = { L"Form", L"Frame", L"Panel", L"ScrollingPanel", L"Label", L"Button", L"TextBox" };
		for(UINT k = 0; k < ARRAYSIZE(kinds); k++)
		{
			TStackRef<IQuadooControlFactory> factory; TStackRef<IQuadooFactoryLifetime> lifetime; UINT live;
			Check(registry->FindFactory(RSTRING_CAST(kinds[k]), &factory)); Check(factory->QueryInterface(__uuidof(IQuadooFactoryLifetime), reinterpret_cast<PVOID*>(&lifetime)));
			Check(lifetime->GetLiveCount(&live)); REQUIRE(live == 0);
		}
		Check(resources.AfterCycle());
	}
Cleanup:
	if(form) form->Destroy();
	return hr;
}
#include "ExternalTests.h"
#include "ChoiceTests.h"
#include "ItemTests.h"
#include "TabTests.h"
#include "DesignerPrerequisiteTests.h"
#include "NonvisualTests.h"
#include "TimerTests.h"
#include "ColorDialogTests.h"
#include "ComponentExample.h"
#include "KeyboardMemoTests.h"
#include "ImageTests.h"
#include "ColorTests.h"
#include "ColorSwatchTests.h"
#include "FocusTests.h"
#include "MenuTests.h"
#include "ImageListTests.h"
#include "TreeTests.h"
#include "ListViewContractTests.h"
#include "ListViewTests.h"
#include "ListStoredTests.h"
#include "ListVirtualTests.h"
#include "ListVirtualSafety.h"
#include "ListColumnDPITests.h"
#include "ImageButtonTests.h"
#include "ProgressBarTests.h"
#include "DateTimePickerTests.h"
#include "CalendarTests.h"
#include "PasswordTests.h"
#include "StatusBarTests.h"
#include "CanvasTests.h"

int wmain(int argc, WCHAR** argv)
{
	HRESULT hr = S_OK;
	INITCOMMONCONTROLSEX controls = { sizeof(controls), ICC_STANDARD_CLASSES }; InitCommonControlsEx(&controls);
	CoInitialize(NULL);
	if(argc > 1 && !lstrcmpW(argv[1],L"--calendar-only"))
	{
	#ifdef _DEBUG
		_SetAssertCallback(AssertTrace);
	#endif
		hr = NativeCalendars();
		CRString report;
		if(SUCCEEDED(RStrFormatW(&report,L"Calendar: %u assertions, result 0x%.8x\r\n",g_checks,hr)))
		{
			Log(report.c_wstr());
		}
		CoUninitialize();
		return FAILED(hr) ? 1 : 0;
	}
	if(argc > 1 && !lstrcmpW(argv[1],L"--password-only"))
	{
	#ifdef _DEBUG
		_SetAssertCallback(AssertTrace);
	#endif
		hr = TextBoxPasswords();
		CRString report;
		if(SUCCEEDED(RStrFormatW(&report,L"TextBox Password: %u assertions; result 0x%.8x\r\n",g_checks,hr)))
		{
			Log(report.c_wstr());
		}
		CoUninitialize();
		return FAILED(hr) ? 1 : 0;
	}
	if(argc > 1 && !lstrcmpW(argv[1],L"--datetime-picker-only"))
	{
	#ifdef _DEBUG
		_SetAssertCallback(AssertTrace);
	#endif
		hr = DateTimePickerNativeTests();
		CRString report;
		if(SUCCEEDED(RStrFormatW(&report,L"DateTimePicker: %u assertions; result 0x%.8x\r\n",g_checks,hr)))
		{
			Log(report.c_wstr());
		}
		CoUninitialize();
		return FAILED(hr) ? 1 : 0;
	}
	if(argc > 1 && !lstrcmpW(argv[1],L"--canvas-only"))
	{
	#ifdef _DEBUG
		_SetAssertCallback(AssertTrace);
	#endif
		hr = CanvasNativeTests();
		CRString report;
		if(SUCCEEDED(RStrFormatW(&report,L"Canvas: %u assertions; result 0x%.8x\r\n",g_checks,hr)))
		{
			Log(report.c_wstr());
		}
		CoUninitialize();
		return FAILED(hr) ? 1 : 0;
	}
	if(argc > 1 && !lstrcmpW(argv[1],L"--status-bar-only"))
	{
	#ifdef _DEBUG
		_SetAssertCallback(AssertTrace);
	#endif
		hr = StatusBarNativeTests();
		CRString summary;
		if(SUCCEEDED(RStrFormatW(&summary,L"StatusBar: %u assertions; result 0x%.8x\r\n",g_checks,hr)))
		{
			Log(summary.c_wstr());
		}
		CoUninitialize();
		return FAILED(hr) ? 1 : 0;
	}
	if(argc > 1 && !lstrcmpW(argv[1],L"--progress-bar-only"))
	{
		hr = ProgressBarNativeTests();
		CRString progressSummary;
		if(SUCCEEDED(RStrFormatW(&progressSummary,L"ProgressBar: %u assertions; result 0x%.8x\r\n",g_checks,hr)))
		{
			Log(progressSummary.c_wstr());
		}
		CoUninitialize();
		return FAILED(hr) ? 1 : 0;
	}
	if(argc > 1 && !lstrcmpW(argv[1],L"--image-button-only"))
	{
		hr = ImageButtonNativeTests();
		CRString summary;
		if(SUCCEEDED(RStrFormatW(&summary,L"ImageButton native: %u assertions; result 0x%.8x\r\n",g_checks,hr)))
		{
			Log(summary.c_wstr());
		}
		CoUninitialize();
		return FAILED(hr) ? 1 : 0;
	}
	if(argc > 1 && !lstrcmpW(argv[1],L"--listview-virtual-only"))
	{
		#ifdef _DEBUG
		_SetAssertCallback(AssertTrace);
		#endif
		hr = ListVirtualItems();
		if(SUCCEEDED(hr))
		{
			hr = ListVirtualSafety();
		}
		if(SUCCEEDED(hr))
		{
			hr = ListVirtualResources();
		}
		CRString message;
		if(SUCCEEDED(RStrFormatW(&message,L"ListView OwnerData: %u assertions; result 0x%.8x\r\n",g_checks,hr)))
		{
			Log(message.c_wstr());
		}
		CoUninitialize();
		return FAILED(hr) ? 1 : 0;
	}
	if(argc > 1 && !lstrcmpW(argv[1],L"--listview-only"))
	{
		#ifdef _DEBUG
		_SetAssertCallback(AssertTrace);
		#endif
		hr = ListViewNative();
		if(SUCCEEDED(hr))
		{
			hr = ListColumnDPI();
		}
		if(SUCCEEDED(hr))
		{
			hr = ListStoredItems();
		}
		if(SUCCEEDED(hr))
		{
			hr = ListViewResources();
		}
		CRString message;
		if(SUCCEEDED(RStrFormatW(&message,L"ListView foundation: %u assertions; result 0x%.8x\r\n",g_checks,hr)))
		{
			Log(message.c_wstr());
		}
		CoUninitialize();
		return FAILED(hr) ? 1 : 0;
	}
	if(argc > 1 && !lstrcmpW(argv[1],L"--listview-contract-only"))
	{
		#ifdef _DEBUG
		_SetAssertCallback(AssertTrace);
		#endif
		hr = ListViewManagedImages();
		CRString message;
		if(SUCCEEDED(RStrFormatW(&message,L"ListView managed-image contract: %u assertions; result 0x%.8x\r\n",g_checks,hr)))
		{
			Log(message.c_wstr());
		}
		CoUninitialize();
		return FAILED(hr) ? 1 : 0;
	}
	#ifdef _DEBUG
	for(INT n = 1; n < argc; n++) if(!lstrcmpW(argv[n],L"--tree-only") || !lstrcmpW(argv[n],L"--trace") || !lstrcmpW(argv[n],L"--phase-a") || !lstrcmpW(argv[n],L"--keyboard-only")) _SetAssertCallback(AssertTrace);
	#endif
	if(argc > 1 && !lstrcmpW(argv[1],L"--tree-only"))
	{
		hr = TreeNative();
		if(SUCCEEDED(hr))
		{
			hr = TreeResources();
		}
		CRString message;
		if(SUCCEEDED(RStrFormatW(&message,L"TreeView native: %u assertions; result 0x%.8x\r\n",g_checks,hr)))
		{
			Log(message.c_wstr());
		}
		CoUninitialize();
		return FAILED(hr) ? 1 : 0;
	}
	if(argc > 1 && !lstrcmpW(argv[1],L"--image-list-only"))
	{
		hr = ImageListNative();
		CRString message;
		if(SUCCEEDED(RStrFormatW(&message,L"ImageList native: %u assertions; result 0x%.8x\r\n",g_checks,hr)))
		{
			Log(message.c_wstr());
		}
		CoUninitialize();
		return FAILED(hr) ? 1 : 0;
	}
	if(argc > 1 && 0 == TStrCmpAssert(argv[1], L"--show")) hr = ShowExample();
	else if(argc > 1 && 0 == TStrCmpAssert(argv[1], L"--show-choices")) hr = ShowExample(ChoiceExampleDefinition);
	else if(argc > 1 && 0 == TStrCmpAssert(argv[1], L"--show-items")) hr = ShowExample(ItemExampleDefinition);
	else if(argc > 1 && 0 == TStrCmpAssert(argv[1], L"--show-tabs")) hr = ShowExample(TabExampleDefinition);
	else if(argc > 1 && !lstrcmpW(argv[1],L"--show-components")) hr = ShowComponentExample();
	else
	{
		struct Test { PCWSTR name; HRESULT (*run)(); };
		const Test tests[] = { { L"Lifecycle and native events", Lifecycle }, { L"Docking, anchors, DPI, observers", LayoutAndDPI },
			{ L"Scrolling, frames, serialization", ScrollingAndPersistence }, { L"Rollback and thread affinity", Failures }, { L"Modal owner and WM_QUIT", Modal },
			{ L"Factory transactions and design mode", ExtensionAndDesign }, { L"Sizing, focus, native resource lifetime", SizingFocusAndResources },
			{ L"Independent external component DLL", ExternalComponent }, { L"Native capabilities and metadata", NativeCapabilities },
			{ L"Extension failures and registration rollback", ExtensionFailures }, { L"Extension DLL lifetime and unload", ExtensionModuleLifetime },
			{ L"CheckBox and RadioButton native interaction", ChoiceInteraction },
			{ L"GroupBox layout, extension DLL and persistence", ChoiceGroupBox },
			{ L"Choice control rollback and native resources", ChoiceRollbackAndResources },
			{ L"String item snapshots and native selection", ItemInteraction },
			{ L"Item persistence, validation and layout", ItemPersistence },
			{ L"Item control destruction and native resources", ItemResources },
			{ L"Tab pages, native selection and focus", TabInteraction },
			{ L"Tab layout, metadata and persistence", TabPersistence },
			{ L"Tab rollback, destruction and native resources", TabResources },
			{ L"Designer Form properties, icons and scrolling", DesignerRuntimeProperties },
			{ L"Modal buttons and independent Preview", DesignerModalButtons },
			{ L"Designer factory and resize capabilities", DesignerCapabilities },
			{ L"Nonvisual components and independent DLL", RuntimeComponents },
			{ L"Timer components and UI-thread events", TimerComponents }, { L"Form keyboard routing and Memo", KeyboardAndMemo },
			{ L"Keyboard scopes, native claims and destruction", KeyboardScopes }, { L"Keyboard modal completion and Memo resources", KeyboardModalAndResources },
			{ L"Windowless Image capabilities, properties, compositing and events", ImageInteraction },
			{ L"Image persistence, DPI, AutoSize and docking", ImagePersistenceAndLayout },
			{ L"Quadoo colors, native painting and persistence", ColorProperties },
			{ L"ColorSwatch geometry, selection, properties and events", ColorSwatchInteraction },
			{ L"ColorSwatch standard picker and modal callbacks", ColorSwatchPicker },
			{ L"ColorSwatch persistence, DPI, rollback and factory lifetime", ColorSwatchPersistence },
			{ L"VCL focus enter/leave and container ancestry", FocusEnterLeave },
			{ L"ColorDialog metadata, identity and lifetime", ColorDialogMetadata },
			{ L"Native menus, hierarchy, geometry, events and cleanup", NativeMenus } };
		UINT firstTest = argc > 1 && !lstrcmpW(argv[1],L"--keyboard-only") ? 25 : argc > 1 && 0 == TStrCmpAssert(argv[1],L"--phase-a") ? 20 : argc > 1 && !lstrcmpW(argv[1],L"--components-only") ? 23 : 0;
		if(argc > 1 && !lstrcmpW(argv[1],L"--image-only")) firstTest = 28;
		if(argc > 1 && !lstrcmpW(argv[1],L"--colors-only")) firstTest = 30;
		if(argc > 1 && !lstrcmpW(argv[1],L"--swatches-only")) firstTest = 31;
		if(argc > 1 && !lstrcmpW(argv[1],L"--color-dialog-only")) firstTest = ARRAYSIZE(tests) - 2;
		BOOL focusOnly = argc > 1 && !lstrcmpW(argv[1],L"--focus-only");
		if(focusOnly)
		{
			firstTest = ARRAYSIZE(tests) - 3;
		}
		if(argc > 1 && !lstrcmpW(argv[1],L"--menus-only")) firstTest = ARRAYSIZE(tests) - 1;
		BOOL tabResourcesOnly = argc > 1 && !lstrcmpW(argv[1],L"--tab-resources-only");
		if(tabResourcesOnly) firstTest = 19;
		BOOL continueOnFailure = argc > 1 && !lstrcmpW(argv[1],L"--continue-on-failure");
		HRESULT firstFailure = S_OK;
		for(UINT n = firstTest; n < (focusOnly ? firstTest + 1 : tabResourcesOnly ? 20 : ARRAYSIZE(tests)); n++)
		{
			Log(tests[n].name); Log(L"... "); hr = tests[n].run();
			if(FAILED(hr))
			{
				CRString message;
				if(SUCCEEDED(RStrFormatW(&message,L"FAIL 0x%.8x\r\n",hr))) Log(message.c_wstr());
				if(SUCCEEDED(firstFailure)) firstFailure = hr;
				if(!continueOnFailure) break;
				continue;
			}
			Log(L"passed\r\n");
		}
		if(firstTest == 0 && (SUCCEEDED(firstFailure) || continueOnFailure))
		{
			const Test collectionTests[] =
			{
				{L"ImageList component persistence",ImageListNative},
				{L"Canvas drawing, measurement, paint, PNG and lifetime",CanvasNativeTests},
					{L"StatusBar panels, docking, icons, DPI and lifetime",StatusBarNativeTests},
				{L"ProgressBar native range, steps, DPI, persistence and lifetime",ProgressBarNativeTests},
				{L"DateTimePicker values, events, native styles, DPI and lifetime",DateTimePickerNativeTests},
				{L"Calendar selection, native styles, DPI and persistence",NativeCalendars},
				{L"TextBox Password property, native masking and persistence",TextBoxPasswords},
				{L"ImageButton native input, painting, DPI and modal completion",ImageButtonNativeTests},
				{L"TreeView nodes, native images, events and persistence",TreeNative},
				{L"TreeView and ImageList resource/factory lifetime",TreeResources},
				{L"ListView native foundation and columns",ListViewNative},
				{L"ListView persisted column DPI normalization",ListColumnDPI},
				{L"ListView stored rows and logical events",ListStoredItems},
				{L"ListView OwnerData requests and snapshots",ListVirtualItems},
				{L"ListView OwnerData providers, modal failures and destruction",ListVirtualSafety},
				{L"ListView OwnerData image and factory lifetime",ListVirtualResources},
				{L"ListView resource and factory lifetime",ListViewResources}
			};
			for(UINT n = 0; n < ARRAYSIZE(collectionTests); n++)
			{
				Log(collectionTests[n].name);
				Log(L"... ");
				hr = collectionTests[n].run();
				if(FAILED(hr))
				{
					Log(L"FAILED\r\n");
					if(SUCCEEDED(firstFailure))
					{
						firstFailure = hr;
					}
					if(!continueOnFailure)
					{
						break;
					}
				}
				else
				{
					Log(L"passed\r\n");
				}
			}
		}
		if(SUCCEEDED(hr)) hr = UnloadExternalExtension();
		if(FAILED(firstFailure)) hr = firstFailure;
		CRString message; if(SUCCEEDED(RStrFormatW(&message, L"%u assertions; result 0x%.8x\r\n", g_checks, hr))) Log(message.c_wstr());
	}
	CoUninitialize();
	return FAILED(hr) ? 1 : 0;
}
