#include "..\..\..\..\platform\QuadooForm\QuadooForm\Runtime.h"

static UINT g_checks;
static VOID Log(PCWSTR value)
{
	CHAR text[2048]; DWORD written;
	INT size = WideCharToMultiByte(CP_UTF8,0,value,-1,text,ARRAYSIZE(text),NULL,NULL);
	if(size) WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),text,size - 1,&written,NULL);
}
#ifdef _DEBUG
static VOID WINAPI AssertTrace(HRESULT error, PCSTR file, UINT line)
{
	CRString text;
	if(SUCCEEDED(RStrFormatW(&text,L"check 0x%.8x at %S:%u\r\n",error,file,line)))
	{
		Log(text.c_wstr());
	}
}
#endif
#define VERIFY(value) CheckIf((++g_checks, !(value)), E_FAIL)

class CProbe : public QF::TControlInterface<QF::CControl,IQuadooControl>
{
public:
	static const QF::TypeSpec* Type() { return &QF::FormType; }
	CProbe(QF::CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : TControlInterface(factory,context,parent) { m_name = RSTRING_CAST(L"Probe"); }
	HRESULT ReadMissing(IJSONValue** value) { return StoredValue(L"Missing",value); }
	HRESULT MarkMissing() { return Dirty(L"Missing"); }
	HRESULT WriteMissing(IJSONValue* value) { return SetStoredValue(L"Missing",value); }
	HRESULT Load(IJSONObject* definition) { return LoadProperties(definition); }
	UINT Index(PCWSTR name) { return FindProperty(name); }
};
class CProbeFactory : public QF::CFactory
{
	virtual QF::CControl* Allocate(IQuadooConstructionContext* context, IQuadooControlContainer* parent) { return __new CProbe(this,context,parent); }
	virtual BOOL AcceptParent(IQuadooControlContainer* parent) { return TRUE; }
public:
	CProbeFactory(const QF::TypeSpec* type) : CFactory(type) {}
};
static HRESULT MetadataFailure(const QF::TypeSpec* type)
{
	HRESULT hr = S_OK;
	TStackRef<CProbeFactory> factory;
	factory.Attach(__new CProbeFactory(type)); CheckAlloc(factory);
	VERIFY(FAILED(factory->Initialize()));
Cleanup:
	return hr;
}
static HRESULT MetadataTests()
{
	HRESULT hr;
	const DWORD stored = QuadooForm::PropertySerializable;
	const DWORD layout = stored | QuadooForm::PropertyAffectsLayout;
	const QF::PropertySpec wrongType[] = { { L"Width",QuadooProperty::Boolean,layout,0,3 } };
	const QF::PropertySpec wrongFlags[] = { { L"Width",QuadooProperty::Int,stored,0,3 } };
	const QF::PropertySpec wrongOrder[] = { { L"Width",QuadooProperty::Int,layout,0,99 } };
	const QF::PropertySpec wrongDefault[] = { { L"Width",QuadooProperty::Int,layout,-1,3 } };
	const QF::PropertySpec duplicate[] = { { L"Extra",QuadooProperty::Int,stored,1,24 }, { L"Extra",QuadooProperty::Int,stored,2,25 } };
	const QF::PropertySpec compatible[] = { { L"Width",QuadooProperty::Int,layout,321,3 } };
	const QF::PropertySpec collision[] = { { L"OnCreate",QuadooProperty::String,stored,0,24 } };
	const QF::EnumSpec choices[] = { { L"Zero",0 }, { L"One",1 } };
	const QF::EnumSpec duplicateValues[] = { { L"Zero",0 }, { L"One",0 } };
	const QF::EnumSpec duplicateNames[] = { { L"Zero",0 }, { L"Zero",1 } };
	const QF::EnumSpec nullName[] = { { NULL,0 } };
	const QF::PropertySpec nullChoices[] = { { L"Extra",QuadooProperty::Enum,stored,0,24,NULL,1 } };
	const QF::PropertySpec wrongChoiceType[] = { { L"Extra",QuadooProperty::Int,stored,0,24,choices,2 } };
	const QF::PropertySpec wrongChoiceDefault[] = { { L"Extra",QuadooProperty::Enum,stored,2,24,choices,2 } };
	const QF::PropertySpec duplicateChoiceValues[] = { { L"Extra",QuadooProperty::Enum,stored,0,24,duplicateValues,2 } };
	const QF::PropertySpec duplicateChoiceNames[] = { { L"Extra",QuadooProperty::Enum,stored,0,24,duplicateNames,2 } };
	const QF::PropertySpec nullChoiceName[] = { { L"Extra",QuadooProperty::Enum,stored,0,24,nullName,1 } };
	const QF::PropertySpec wrongChecked[] = { { L"Checked",QuadooProperty::Int,stored,0,24 } };
	const QF::PropertySpec wrongDomain[] = { { L"CheckState",QuadooProperty::Enum,stored,0,24,choices,2 } };
	const QF::PropertySpec wrongItems[] = { { L"Items",QuadooProperty::Collection,stored,0,24 } };
	const QF::PropertySpec wrongItemIndex[] = { { L"ItemIndex",QuadooProperty::Int,stored,-2,25 } };
	const QF::PropertySpec wrongActiveIndex[] = { { L"ActivePageIndex",QuadooProperty::Int,stored,-2,24 } };
	const QF::PropertySpec wrongActiveType[] = { { L"ActivePageIndex",QuadooProperty::Long,stored,-1,24 } };
	const QF::PropertySpec wrongTabVisible[] = { { L"TabVisible",QuadooProperty::Boolean,stored,1,24 } };
	const QF::PropertySpec wrongKeyPreview[] = { { L"KeyPreview",QuadooProperty::Int,stored,0,40 } };
	const QF::PropertySpec wrongDefaultButton[] = { { L"Default",QuadooProperty::Boolean,stored | QuadooForm::PropertyReadOnly,0,40 } };
	const QF::PropertySpec wrongCancelButton[] = { { L"Cancel",QuadooProperty::Int,stored,0,40 } };
	const QF::EventSpec duplicateEvents[] = { { L"OnExtra",0,0,NULL,20 }, { L"OnExtra",0,0,NULL,21 } };
	QF::TypeSpec type = { L"Probe",&QF::FormType,NULL,0,NULL,0 };
	TStackRef<CProbeFactory> factory;
	TStackRef<IQuadooPropertyDescriptors> descriptors;
	TStackRef<IQuadooPropertyDescriptor> descriptor;
	TStackRef<IJSONValue> value;
	INT width;
	type.properties = wrongType; type.propertyCount = 1; Check(MetadataFailure(&type));
	type.properties = wrongFlags; Check(MetadataFailure(&type));
	type.properties = wrongOrder; Check(MetadataFailure(&type));
	type.properties = wrongDefault; Check(MetadataFailure(&type));
	type.properties = duplicate; type.propertyCount = ARRAYSIZE(duplicate); Check(MetadataFailure(&type));
	type.properties = collision; type.propertyCount = 1; Check(MetadataFailure(&type));
	type.properties = NULL; type.propertyCount = 0; type.events = duplicateEvents; type.eventCount = ARRAYSIZE(duplicateEvents); Check(MetadataFailure(&type));
	type.events = NULL; type.eventCount = 1; Check(MetadataFailure(&type));
	type.eventCount = 0; type.propertyCount = 4097; Check(MetadataFailure(&type));
	type.propertyCount = 0; type.base = &type; Check(MetadataFailure(&type));
	type.base = NULL; Check(MetadataFailure(&type));
	type.base = &QF::FormType; type.propertyCount = 1;
	type.properties = nullChoices; Check(MetadataFailure(&type));
	type.properties = wrongChoiceType; Check(MetadataFailure(&type));
	type.properties = wrongChoiceDefault; Check(MetadataFailure(&type));
	type.properties = duplicateChoiceValues; Check(MetadataFailure(&type));
	type.properties = duplicateChoiceNames; Check(MetadataFailure(&type));
	type.properties = nullChoiceName; Check(MetadataFailure(&type));
	type.properties = wrongChecked; Check(MetadataFailure(&type));
	type.base = &QF::CheckBoxType; type.properties = wrongDomain; Check(MetadataFailure(&type));
	type.base = &QF::ComboBoxType; type.properties = wrongItems; Check(MetadataFailure(&type));
	type.properties = wrongItemIndex; Check(MetadataFailure(&type));
	type.base = &QF::TabControlType; type.properties = wrongActiveIndex; Check(MetadataFailure(&type));
	type.properties = wrongActiveType; Check(MetadataFailure(&type));
	type.base = &QF::TabPageType; type.properties = wrongTabVisible; Check(MetadataFailure(&type));
	type.base = &QF::LabelType; type.properties = wrongKeyPreview; Check(MetadataFailure(&type));
	type.properties = wrongDefaultButton; Check(MetadataFailure(&type));
	type.properties = wrongCancelButton; Check(MetadataFailure(&type));
	type.base = &QF::FormType; type.properties = compatible; type.propertyCount = 1;
	factory.Attach(__new CProbeFactory(&type)); CheckAlloc(factory); Check(factory->Initialize());
	Check(factory->GetProperties(&descriptors)); Check(descriptors->Find(RSTRING_CAST(L"Width"),&descriptor)); Check(descriptor->GetDefaultValue(&value)); Check(value->GetInteger(&width)); VERIFY(width == 321);
Cleanup:
	return hr;
}
static HRESULT StorageTests()
{
	HRESULT hr;
	TStackRef<CProbeFactory> factory;
	TStackRef<CProbe> probe;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONValue> value, integer;
	QuadooForm::ContextOptions options;
	ZeroMemory(&options,sizeof(options)); options.cbSize = sizeof(options); options.DPI = 96;
	factory.Attach(__new CProbeFactory(&QF::FormType)); CheckAlloc(factory); Check(factory->Initialize());
	Check(QuadooCreateConstructionContext(&options,&context));
	probe.Attach(__new CProbe(factory,context,NULL)); CheckAlloc(probe);
	Check(JSONCreateInteger(5,&integer));
	// Empty/partial storage is safe even when metadata names exist in the factory.
	VERIFY(probe->ReadMissing(&value) == DISP_E_MEMBERNOTFOUND && !value);
	VERIFY(probe->ReadMissing(NULL) == E_POINTER); VERIFY(probe->MarkMissing() == DISP_E_MEMBERNOTFOUND); VERIFY(probe->WriteMissing(integer) == DISP_E_MEMBERNOTFOUND);
	Check(JSONCreateObject(&definition)); Check(probe->Load(definition));
	VERIFY(probe->ReadMissing(&value) == DISP_E_MEMBERNOTFOUND && !value);
	VERIFY(probe->MarkMissing() == DISP_E_MEMBERNOTFOUND); VERIFY(probe->WriteMissing(integer) == DISP_E_MEMBERNOTFOUND);
	VERIFY(probe->GetValueJSON(UINT_MAX,&value) == HRESULT_FROM_WIN32(ERROR_INVALID_INDEX) && !value);
	VERIFY(probe->SetValueJSON(UINT_MAX,integer) == HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
	VERIFY(probe->GetValueJSON(UINT_MAX - 1,&value) == HRESULT_FROM_WIN32(ERROR_INVALID_INDEX) && !value);
	VERIFY(probe->SetValueInt(UINT_MAX,10) == HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
	VERIFY(probe->SetValueEvent(UINT_MAX,RSTRING_CAST(L"Missing")) == HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
Cleanup:
	if(probe) probe->Destroy();
	return hr;
}
static HRESULT MemoTests()
{
	HRESULT hr;
	const QF::PropertySpec notes[] = { { L"Notes",QuadooProperty::Memo,QuadooForm::PropertySerializable,0,40 } };
	const QF::TypeSpec type = { L"MemoProbe",&QF::FormType,notes,ARRAYSIZE(notes),NULL,0 };
	TStackRef<CProbeFactory> factory; TStackRef<CProbe> probe, copy; TStackRef<IQuadooConstructionContext> context;
	TStackRef<IJSONObject> definition, properties; TStackRef<IJSONValue> value, wrapped;
	TStackRef<IQuadooPropertyDescriptors> descriptors; TStackRef<IQuadooPropertyDescriptor> descriptor;
	CRString text; QuadooProperty::Type actual; UINT index;
	QuadooForm::ContextOptions options;
	ZeroMemory(&options,sizeof(options)); options.cbSize = sizeof(options); options.DPI = 96;
	factory.Attach(__new CProbeFactory(&type));
	CheckAlloc(factory);
	Check(factory->Initialize());
	Check(factory->GetProperties(&descriptors));
	Check(descriptors->Find(RSTRING_CAST(L"Notes"),&descriptor));
	Check(descriptor->GetType(&actual));
	VERIFY(actual == QuadooProperty::Memo && QuadooProperty::Collection == 16 && QuadooProperty::Memo == 17);
	Check(QuadooCreateConstructionContext(&options,&context));
	probe.Attach(__new CProbe(factory,context,NULL));
	CheckAlloc(probe);
	Check(JSONCreateObject(&definition));
	Check(probe->Load(definition));
	index = probe->Index(L"Notes");
	Check(probe->GetValueString(index,&text));
	VERIFY(!RStrLen(text));
	Check(probe->SetValueString(index,RSTRING_CAST(L"First line\r\nSecond line\n\x03a9")));
	text = static_cast<RSTRING>(NULL);
	Check(probe->GetValueString(index,&text));
	VERIFY(text == RSTRING_CAST(L"First line\r\nSecond line\n\x03a9"));
	Check(JSONCreateInteger(9,&value));
	VERIFY(FAILED(probe->SetValueJSON(index,value)));
	value.Release();
	Check(probe->GetValueJSON(index,&value));
	Check(JSONCreateObject(&properties));
	Check(properties->AddValueW(L"Notes",value));
	Check(JSONWrapObject(properties,&wrapped));
	Check(definition->AddValueW(L"properties",wrapped));
	copy.Attach(__new CProbe(factory,context,NULL));
	CheckAlloc(copy);
	Check(copy->Load(definition));
	text = static_cast<RSTRING>(NULL);
	Check(copy->GetValueString(copy->Index(L"Notes"),&text));
	VERIFY(text == RSTRING_CAST(L"First line\r\nSecond line\n\x03a9"));
Cleanup:
	if(probe) probe->Destroy(); if(copy) copy->Destroy();
	return hr;
}
static BOOL g_failItemWindow;
template<class T> class TFailingItems : public T
{
protected:
	virtual HRESULT NativeCreated()
	{
		HRESULT hr;
		Check(T::NativeCreated()); CheckIf(g_failItemWindow, E_ABORT);
	Cleanup:
		return hr;
	}
public:
	TFailingItems(QF::CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : T(factory,context,parent) {}
};
static HRESULT ItemRollbackTests()
{
	HRESULT hr;
	TStackRef<QF::CFactory> factory;
	TStackRef<IQuadooConstructionContext> context; TStackRef<IQuadooForm> form;
	TStackRef<IQuadooControl> control; TStackRef<IQuadooComboBox> combo; TStackRef<IQuadooConstructionParticipant> participant;
	TStackRef<IQuadooProperties> properties; TStackRef<IQuadooPropertyDescriptors> descriptors; TStackRef<IQuadooPropertyDescriptor> descriptor;
	TStackRef<IJSONValue> parsed; TStackRef<IJSONObject> definition, original, result;
	TStackRef<IQuadooFormSerializer> serializer; TStackRef<IRStringArray> items;
	QuadooForm::ContextOptions options; HWND window, current; UINT count, live, style; INT index;
	PCWSTR formJSON = L"{\"type\":\"Form\",\"name\":\"Rollback\"}";
	PCWSTR itemJSON = L"{\"type\":\"ComboBox\",\"name\":\"items\",\"properties\":{\"Items\":[\"One\",\"Two\"],\"ItemIndex\":1}}";
	ZeroMemory(&options,sizeof(options)); options.cbSize = sizeof(options); options.DPI = 96;
	Check(QuadooCreateConstructionContext(&options,&context));
	Check(JSONParse(NULL,formJSON,TStrLenAssert(formJSON),&parsed)); Check(parsed->GetObject(&definition));
	Check(QuadooCreateForm(NULL,NULL,definition,&form)); definition.Release(); parsed.Release();
	Check(JSONParse(NULL,itemJSON,TStrLenAssert(itemJSON),&parsed)); Check(parsed->GetObject(&definition));
	factory.Attach(__new QF::TFactory<TFailingItems<QF::CComboBox> >); CheckAlloc(factory); Check(factory->Initialize());
	Check(factory->CreateControl(context,form,definition,&control));
	Check(control->QueryInterface(__uuidof(IQuadooConstructionParticipant),reinterpret_cast<PVOID*>(&participant)));
	Check(participant->BindEvents(context)); Check(form->UpdateLayout()); Check(participant->InitializeControl()); Check(participant->ActivateControl());
	Check(control->QueryInterface(__uuidof(IQuadooComboBox),reinterpret_cast<PVOID*>(&combo)));
	Check(control->GetProperties(&properties)); Check(properties->GetDescriptors(&descriptors)); Check(descriptors->GetCount(&count));
	for(style = 0; style < count; style++)
	{ CRString name; descriptor.Release(); Check(descriptors->GetAt(style,&descriptor)); Check(descriptor->GetName(&name)); if(name == RSTRING_CAST(L"Style")) break; }
	VERIFY(style < count);
	Check(QuadooGetFormSerializer(&serializer)); Check(serializer->Serialize(form,&original));
	Check(combo->GetHandle(&window));
	Check(ScCreateRStringArray(&items)); Check(items->AddStringW(L"New",3)); Check(items->AddStringW(L"Values",6)); Check(items->AddStringW(L"Third",5));
	// Failure occurs after the staged HWND and all its native strings exist.
	g_failItemWindow = TRUE;
	VERIFY(combo->SetItems(items) == E_ABORT); VERIFY(properties->SetValueInt(style,QuadooForm::DropDownList) == E_ABORT);
	g_failItemWindow = FALSE;
	Check(combo->GetHandle(&current)); VERIFY(current == window && IsWindow(window));
	VERIFY(SendMessageW(window,CB_GETCOUNT,0,0) == 2 && SendMessageW(window,CB_GETCURSEL,0,0) == 1);
	Check(combo->GetItemIndex(&index)); VERIFY(index == 1);
	Check(serializer->Serialize(form,&result)); VERIFY(JSONCompareObjects(original,result) == 0);
	Check(form->GetChildCount(&count)); VERIFY(count == 1); Check(factory->GetLiveCount(&live)); VERIFY(live == 1);
	// The restored subclass data remains usable; retries can replace that HWND.
	VERIFY(SendMessageW(window,CB_SETCURSEL,0,0) == 0); Check(combo->GetItemIndex(&index)); VERIFY(index == 0);
	Check(combo->SetItems(items)); Check(combo->GetHandle(&current)); VERIFY(current != window && !IsWindow(window));
	Check(properties->SetValueInt(style,QuadooForm::DropDownList)); Check(combo->GetHandle(&current));
	VERIFY((GetWindowLongW(current,GWL_STYLE) & 3) == CBS_DROPDOWNLIST);
	Check(form->Destroy()); VERIFY(!IsWindow(current));
	control.Release(); combo.Release(); participant.Release(); properties.Release(); Check(factory->GetLiveCount(&live)); VERIFY(live == 0);
Cleanup:
	g_failItemWindow = FALSE; if(form) form->Destroy();
	return hr;
}
static INT g_tabNativeFailure;
static BOOL g_tabObserverFailure;
static BOOL g_tabLayoutFailure;
class CTabProbe : public QF::CTabControl
{
protected:
	virtual LRESULT Message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled)
	{
		if((g_tabNativeFailure == 1 && msg == TCM_INSERTITEMW) || (g_tabNativeFailure == 2 && msg == TCM_SETITEMW))
		{ *handled = TRUE; return msg == TCM_INSERTITEMW ? -1 : FALSE; }
		return QF::CTabControl::Message(hwnd,msg,wp,lp,content,handled);
	}
public:
	CTabProbe(QF::CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : CTabControl(factory,context,parent) {}
};
class CTabPageProbe : public QF::CTabPage
{
public:
	CTabPageProbe(QF::CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : CTabPage(factory,context,parent) {}
	STDMETHOD(AddObserver)(IQuadooPropertyObserver* observer)
	{ HRESULT hr = QF::CTabPage::AddObserver(observer); return SUCCEEDED(hr) && g_tabObserverFailure ? E_ABORT : hr; }
	STDMETHOD(Arrange)(const QuadooForm::Rect* bounds, UINT dpi)
	{ return g_tabLayoutFailure ? E_ABORT : QF::CTabPage::Arrange(bounds,dpi); }
};
static HRESULT NativeDefinition(PCWSTR text, IJSONObject** definition)
{
	HRESULT hr; TStackRef<IJSONValue> parsed;
	Check(JSONParse(NULL,text,TStrLenAssert(text),&parsed)); Check(parsed->GetObject(definition));
Cleanup:
	return hr;
}
static HRESULT NativeActivate(IQuadooControl* control, IQuadooConstructionContext* context)
{
	HRESULT hr; TStackRef<IQuadooConstructionParticipant> phases;
	Check(control->QueryInterface(__uuidof(IQuadooConstructionParticipant),reinterpret_cast<PVOID*>(&phases)));
	Check(phases->BindEvents(context)); Check(phases->InitializeControl()); Check(phases->ActivateControl());
Cleanup:
	return hr;
}
static HRESULT TabRollbackTests()
{
	HRESULT hr;
	TStackRef<QF::CFactory> tabsFactory, pageFactory;
	TStackRef<IQuadooConstructionContext> context; TStackRef<IQuadooForm> form;
	TStackRef<IQuadooControl> control, first, second, failed; TStackRef<IQuadooTabControl> tabs; TStackRef<IQuadooTabPage> page;
	TStackRef<IQuadooFormSerializer> serializer; TStackRef<IJSONObject> definition, before, after;
	TStackRef<IQuadooProperties> properties; TStackRef<IQuadooPropertyDescriptors> descriptors;
	QuadooForm::ContextOptions options; HWND hwnd; UINT count, live, caption = 0; INT active;
	ZeroMemory(&options,sizeof(options)); options.cbSize = sizeof(options); options.DPI = 96;
	Check(QuadooCreateConstructionContext(&options,&context)); Check(NativeDefinition(L"{\"type\":\"Form\",\"name\":\"TabRollback\"}",&definition));
	Check(QuadooCreateForm(NULL,NULL,definition,&form)); definition.Release();
	tabsFactory.Attach(__new QF::TFactory<CTabProbe>); pageFactory.Attach(__new QF::TFactory<CTabPageProbe>); CheckAlloc(tabsFactory); CheckAlloc(pageFactory); Check(tabsFactory->Initialize()); Check(pageFactory->Initialize());
	Check(NativeDefinition(L"{\"type\":\"TabControl\",\"name\":\"tabs\"}",&definition)); Check(tabsFactory->CreateControl(context,form,definition,&control)); definition.Release();
	Check(NativeActivate(control,context)); Check(control->QueryInterface(__uuidof(IQuadooTabControl),reinterpret_cast<PVOID*>(&tabs))); Check(tabs->GetHandle(&hwnd));
	Check(NativeDefinition(L"{\"type\":\"TabPage\",\"name\":\"first\",\"properties\":{\"Caption\":\"First\"}}",&definition)); Check(pageFactory->CreateControl(context,tabs,definition,&first)); definition.Release(); Check(NativeActivate(first,context));
	Check(form->UpdateLayout()); Check(QuadooGetFormSerializer(&serializer)); Check(serializer->Serialize(form,&before));
	Check(NativeDefinition(L"{\"type\":\"TabPage\",\"name\":\"second\",\"properties\":{\"Caption\":\"Second\"}}",&definition));
	for(INT failure = 0; failure < 3; failure++)
	{
		g_tabNativeFailure = failure == 0 ? 1 : 0; g_tabObserverFailure = failure == 1; g_tabLayoutFailure = failure == 2;
		VERIFY(FAILED(pageFactory->CreateControl(context,tabs,definition,&failed)) && !failed);
		g_tabNativeFailure = 0; g_tabObserverFailure = FALSE; g_tabLayoutFailure = FALSE;
		Check(tabs->GetChildCount(&count)); VERIFY(count == 1); Check(tabs->GetPageCount(&count)); VERIFY(count == 1);
		VERIFY(SendMessageW(hwnd,TCM_GETITEMCOUNT,0,0) == 1); Check(tabs->GetActivePageIndex(&active)); VERIFY(active == 0);
		Check(pageFactory->GetLiveCount(&live)); VERIFY(live == 1); Check(serializer->Serialize(form,&after)); VERIFY(JSONCompareObjects(before,after) == 0); after.Release();
	}
	Check(first->GetProperties(&properties)); Check(properties->GetDescriptors(&descriptors)); Check(descriptors->GetCount(&count));
	for(caption = 0; caption < count; caption++) { TStackRef<IQuadooPropertyDescriptor> descriptor; CRString name; Check(descriptors->GetAt(caption,&descriptor)); Check(descriptor->GetName(&name)); if(name == RSTRING_CAST(L"Caption")) break; }
	VERIFY(caption < count); g_tabNativeFailure = 2; VERIFY(properties->SetValueString(caption,RSTRING_CAST(L"Failed caption")) == E_OUTOFMEMORY); g_tabNativeFailure = 0;
	Check(serializer->Serialize(form,&after)); VERIFY(JSONCompareObjects(before,after) == 0); after.Release(); before.Release();
	Check(pageFactory->CreateControl(context,tabs,definition,&second)); Check(NativeActivate(second,context));
	Check(second->QueryInterface(__uuidof(IQuadooTabPage),reinterpret_cast<PVOID*>(&page))); Check(serializer->Serialize(form,&before));
	g_tabNativeFailure = 1; VERIFY(page->SetPageIndex(0) == E_OUTOFMEMORY); g_tabNativeFailure = 0;
	Check(serializer->Serialize(form,&after)); VERIFY(JSONCompareObjects(before,after) == 0); after.Release();
	Check(page->SetTabVisible(FALSE)); before.Release(); Check(serializer->Serialize(form,&before));
	g_tabNativeFailure = 1; VERIFY(page->SetTabVisible(TRUE) == E_OUTOFMEMORY); g_tabNativeFailure = 0;
	Check(serializer->Serialize(form,&after)); VERIFY(JSONCompareObjects(before,after) == 0);
	Check(page->SetTabVisible(TRUE)); Check(page->SetPageIndex(0)); Check(form->Destroy()); VERIFY(!IsWindow(hwnd));
	first.Release(); second.Release(); page.Release(); properties.Release(); control.Release(); tabs.Release();
	Check(pageFactory->GetLiveCount(&live)); VERIFY(live == 0); Check(tabsFactory->GetLiveCount(&live)); VERIFY(live == 0);
Cleanup:
	g_tabNativeFailure = 0; g_tabObserverFailure = FALSE; g_tabLayoutFailure = FALSE; if(form) form->Destroy();
	return hr;
}
#include "FileDialogTests.h"
#include "ImageTests.h"
#include "ColorTests.h"
#include "MenuTests.h"
#include "TreeTests.h"
#include "ListViewTests.h"
#include "ListVirtualTests.h"
#include "ListStoredTests.h"
#include "KeyboardInputTests.h"
#include "ImageButtonTests.h"
#include "CanvasTests.h"
int wmain(int count, WCHAR** arguments)
{
	HRESULT hr;
	CRString summary;
	BOOL apartment = FALSE;
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
	#ifdef _DEBUG
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
	#endif
	g_hInstance = GetModuleHandle(NULL);
	#ifdef _DEBUG
	for(INT n = 1; n < count; n++)
	{
		if(!lstrcmpW(arguments[n],L"--trace"))
		{
			_SetAssertCallback(AssertTrace);
		}
	}
	#endif
	if(count > 1 && !lstrcmpW(arguments[1],L"--canvas-rendering-only"))
	{
		hr = CanvasRenderingTests();
		goto Summary;
	}
	if(count > 1 && !lstrcmpW(arguments[1],L"--image-button-headless"))
	{
		hr = ImageButtonHeadlessTests();
		goto Summary;
	}
	CoInitialize(NULL);
	apartment = TRUE;
	if(count > 1 && !lstrcmpW(arguments[1],L"--keyboard-input-only"))
	{
		hr = KeyboardInputTests();
		goto Summary;
	}
	if(count > 1 && !lstrcmpW(arguments[1],L"--listview-only"))
	{
		hr = ListViewRollbackTests();
		if(SUCCEEDED(hr))
		{
			hr = ListVirtualRollbackTests();
		}
		if(SUCCEEDED(hr))
		{
			hr = ListStoredRollbackTests();
		}
		goto Summary;
	}
	if(count > 1 && !lstrcmpW(arguments[1],L"--tree-only"))
	{
		hr = TreeRollbackTests();
		goto Summary;
	}
	hr = MetadataTests(); if(SUCCEEDED(hr)) hr = StorageTests(); if(SUCCEEDED(hr)) hr = MemoTests(); if(SUCCEEDED(hr)) hr = ItemRollbackTests(); if(SUCCEEDED(hr)) hr = TabRollbackTests();
	if(SUCCEEDED(hr)) hr = FileDialogTests();
	if(SUCCEEDED(hr)) hr = TimerRollbackTests();
	if(SUCCEEDED(hr)) hr = ImagePrivateTests();
	if(SUCCEEDED(hr)) hr = ColorStorageTests();
	if(SUCCEEDED(hr))
	{
		hr = MenuRollbackTests();
	}
	if(SUCCEEDED(hr))
	{
		hr = TreeRollbackTests();
	}
	if(SUCCEEDED(hr))
	{
		hr = ListViewRollbackTests();
	}
	if(SUCCEEDED(hr))
	{
		hr = ListStoredRollbackTests();
	}
	if(SUCCEEDED(hr))
	{
		hr = ListVirtualRollbackTests();
	}
	if(SUCCEEDED(hr))
	{
		hr = KeyboardInputTests();
	}
	if(SUCCEEDED(hr))
	{
		hr = ImageButtonHeadlessTests();
	}
	if(SUCCEEDED(hr))
	{
		hr = CanvasRenderingTests();
	}
Summary:
	if(SUCCEEDED(RStrFormatW(&summary,L"Native storage and metadata: %u assertions; result 0x%.8x\r\n",g_checks,hr))) Log(summary.c_wstr());
	if(apartment)
	{
		CoUninitialize();
	}
	return FAILED(hr) ? 1 : 0;
}
