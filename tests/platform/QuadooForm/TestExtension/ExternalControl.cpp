#include <windows.h>
#include <commctrl.h>
#include "Library\Core\BaseUnknown.h"
#include "Library\Core\Array.h"
#include "Library\Util\Formatting.h"
#include "..\..\..\..\platform\QuadooForm\Published\QuadooForm.h"
#include "ExternalControl.h"
#include "..\..\..\..\platform\QuadooScript\Published\QuadooVM.h"

static ExternalCounts g_externalCounts = { 0, 0, 0, 0, 0 };
static volatile LONG g_moduleObjects;
class CModuleObject : public CBaseUnknown
{
public:
 CModuleObject() { InterlockedIncrement(&g_moduleObjects); }
 virtual ~CModuleObject() { InterlockedDecrement(&g_moduleObjects); }
};

namespace External
{
class CIconFactory : public CModuleObject, public IQuadooPaletteIcon, public IExternalPaletteTest
{
	UINT m_resource, m_requests;
	HRESULT m_result;
	DWORD m_thread;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooPaletteIcon)
		UNK_INTERFACE(IExternalPaletteTest)
	END_UNK_MAP
	CIconFactory(UINT resource) : m_resource(resource), m_requests(0), m_result(S_OK), m_thread(GetCurrentThreadId()) {}
	STDMETHOD(SetIconResult)(HRESULT result) { m_result = result; return S_OK; }
	STDMETHOD(GetIconRequests)(UINT* count) { if(count) *count = m_requests; return count ? S_OK : E_POINTER; }
	STDMETHOD(GetPaletteIcon)(UINT cx, UINT cy, HICON* icon)
	{
		HRESULT hr = S_OK;
		HMODULE module;
		CheckIf(!icon,E_POINTER);
		*icon = NULL;
		CheckIf(m_thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
		CheckIf(!cx || !cy || cx > 256 || cy > 256,E_INVALIDARG);
		m_requests++;
		hr = m_result;
		if(hr != S_OK) { if(hr == 2) hr = S_OK; goto Cleanup; }
		CheckIf(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<PCWSTR>(&g_externalCounts),&module),HrEnsureLastError());
		*icon = static_cast<HICON>(LoadImageW(module,MAKEINTRESOURCEW(m_resource),IMAGE_ICON,cx,cy,0));
		CheckIf(!*icon,HrEnsureLastError());
	Cleanup:
		return hr;
	}
};
static BOOL Equal(RSTRING a, RSTRING b)
{
	INT result;
	return SUCCEEDED(RStrCompareRStr(a, b, &result)) && result == 0;
}
static HRESULT CopyText(PCWSTR text, RSTRING* value) { return value ? RStrCreateW(TStrLenAssert(text), text, value) : E_POINTER; }
static const DWORD Stored = QuadooForm::PropertySerializable;
static const DWORD Layout = Stored | QuadooForm::PropertyAffectsLayout;
struct Spec { PCWSTR name; QuadooProperty::Type type; DWORD flags; INT initial; };
// Deliberately unrelated indexes and defaults to those of the native controls.
static const Spec specs[] =
{
	{ L"Level", QuadooProperty::Int, Stored, 17 },
	{ L"Legend", QuadooProperty::String, Stored, 0 },
	{ L"Width", QuadooProperty::Int, Layout, 73 },
	{ L"Height", QuadooProperty::Int, Layout, 19 },
	{ L"Left", QuadooProperty::Int, Layout, 0 },
	{ L"Top", QuadooProperty::Int, Layout, 0 },
	{ L"Visible", QuadooProperty::Boolean, Layout, 1 },
	{ L"Enabled", QuadooProperty::Boolean, Stored, 1 },
	{ L"Align", QuadooProperty::Enum, Layout, 0 },
	{ L"Anchors", QuadooProperty::Flags, Layout, 3 },
	{ L"AutoSize", QuadooProperty::Boolean, Layout, 0 },
	{ L"OnPulse", QuadooProperty::Event, Stored, 0 },
	{ L"OnFinalizing", QuadooProperty::Event, Stored, 0 },
	{ L"Name", QuadooProperty::String, Stored, 0 }
};
static UINT Index(PCWSTR name)
{
	UINT n;
	for(n = 0; n < ARRAYSIZE(specs); n++) if(0 == TStrCmpAssert(name, specs[n].name)) break;
	return n;
}
static HRESULT Default(UINT index, IJSONValue** value)
{
	HRESULT hr;
	CheckIf(!value, E_POINTER); *value = NULL;
	CheckIf(index >= ARRAYSIZE(specs), HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
	if(specs[index].type == QuadooProperty::String || specs[index].type == QuadooProperty::Event) hr = JSONCreateStringW(L"", 0, value);
	else if(specs[index].type == QuadooProperty::Boolean) hr = JSONCreateBool(specs[index].initial != 0, value);
	else hr = JSONCreateInteger(specs[index].initial, value);
Cleanup:
	return hr;
}
class CProperty : public CModuleObject, public IQuadooPropertyDescriptor
{
	UINT m_index;
	ExternalSchema m_schema;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		*ppvObj = NULL;
		UNK_INTERFACE(IQuadooPropertyDescriptor)
	END_UNK_MAP
	CProperty(UINT index, ExternalSchema schema) : m_index(index), m_schema(schema) {}
	STDMETHOD(GetName)(RSTRING* value) { return CopyText(specs[m_index].name, value); }
	STDMETHOD(GetDisplayName)(RSTRING* value) { return GetName(value); }
	STDMETHOD(GetCategory)(RSTRING* value) { return CopyText(L"External meter", value); }
	STDMETHOD(GetDescription)(RSTRING* value) { return GetName(value); }
	STDMETHOD(GetType)(QuadooProperty::Type* value) { if(value) *value = m_schema == ExternalMemoSchema && m_index == Index(L"Legend") ? QuadooProperty::Memo : m_schema == ExternalWrongWidthType && m_index == Index(L"Width") ? QuadooProperty::Boolean : specs[m_index].type; return value ? S_OK : E_POINTER; }
	STDMETHOD(GetFlags)(DWORD* value) { if(value) *value = m_schema == ExternalWrongWidthFlags && m_index == Index(L"Width") ? Stored : specs[m_index].flags; return value ? S_OK : E_POINTER; }
	STDMETHOD(GetDefaultValue)(IJSONValue** value) { return m_schema == ExternalWrongDefault && m_index == Index(L"Width") ? JSONCreateStringW(L"bad",3,value) : Default(m_index, value); }
	STDMETHOD(GetEnumCount)(UINT* value) { if(value) *value = m_index == Index(L"Align") ? (m_schema == ExternalBadEnumCount ? 5 : 6) : m_index == Index(L"Anchors") ? 4 : 0; return value ? S_OK : E_POINTER; }
	STDMETHOD(GetEnumValue)(UINT index, RSTRING* name, LONGLONG* value)
	{
		HRESULT hr; UINT count;
		static PCWSTR const alignNames[] = { L"None", L"Top", L"Bottom", L"Left", L"Right", L"Client" };
		static PCWSTR const anchorNames[] = { L"Left", L"Top", L"Right", L"Bottom" };
		CheckIf(!name || !value,E_POINTER); *name = NULL; Check(GetEnumCount(&count)); CheckIf(index >= count,HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
		Check(CopyText(m_index == Index(L"Align") ? alignNames[index] : anchorNames[index],name)); *value = m_index == Index(L"Align") ? index : (1 << index);
		if(m_schema == ExternalBadEnumValue) *value = 99;
	Cleanup:
		return hr;
	}
};
class CProperties : public CModuleObject, public IQuadooPropertyDescriptors
{
	ExternalSchema m_schema;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		*ppvObj = NULL;
		UNK_INTERFACE(IQuadooPropertyDescriptors)
	END_UNK_MAP
	CProperties(ExternalSchema schema) : m_schema(schema) {}
	STDMETHOD(GetCount)(UINT* value) { if(value) *value = m_schema == ExternalHugeCount ? 4097 : m_schema == ExternalMissingName ? ARRAYSIZE(specs) - 1 : m_schema == ExternalDuplicateProperty ? ARRAYSIZE(specs) + 1 : ARRAYSIZE(specs); return value ? S_OK : E_POINTER; }
	STDMETHOD(GetAt)(UINT index, IQuadooPropertyDescriptor** value)
	{
		HRESULT hr = S_OK;
		CheckIf(!value, E_POINTER); *value = NULL; CheckIf(index >= ARRAYSIZE(specs) + (m_schema == ExternalDuplicateProperty ? 1 : 0), HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
		if(m_schema == ExternalNullDescriptor) { hr = S_OK; goto Cleanup; }
		if(index == ARRAYSIZE(specs)) index = Index(L"Width");
		if(m_schema == ExternalReorderedProperties && index < 2) index = 1 - index;
		*value = __new CProperty(index, m_schema); CheckAlloc(*value);
	Cleanup:
		return hr;
	}
	STDMETHOD(Find)(RSTRING name, IQuadooPropertyDescriptor** value)
	{
		HRESULT hr = DISP_E_MEMBERNOTFOUND;
		CheckIf(!value, E_POINTER); *value = NULL;
		for(UINT n = 0; n < ARRAYSIZE(specs); n++) if(Equal(name, RSTRING_CAST(specs[n].name))) { hr = GetAt(n, value); break; }
	Cleanup:
		return hr;
	}
};
class CEvent : public CModuleObject, public IQuadooEventDescriptor
{
	UINT m_index;
	ExternalSchema m_schema;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		*ppvObj = NULL;
		UNK_INTERFACE(IQuadooEventDescriptor)
	END_UNK_MAP
	CEvent(UINT index, ExternalSchema schema) : m_index(index), m_schema(schema) {}
	STDMETHOD(GetName)(RSTRING* value) { return CopyText(m_schema == ExternalEventCollision ? L"Level" : m_schema == ExternalDuplicateEvent || !m_index ? L"OnPulse" : L"OnFinalizing", value); }
	STDMETHOD(GetCategory)(RSTRING* value) { return CopyText(L"External meter", value); }
	STDMETHOD(GetFlags)(DWORD* value) { if(value) *value = 0; return value ? S_OK : E_POINTER; }
	STDMETHOD(GetArgumentCount)(UINT* value) { if(value) *value = m_index ? 0 : 1; return value ? S_OK : E_POINTER; }
	STDMETHOD(GetArgument)(UINT index, RSTRING* name, QuadooForm::EventValueType* type, BOOL* writable)
	{
		HRESULT hr;
		CheckIf(!name || !type || !writable, E_POINTER); *name = NULL;
		CheckIf(m_index || index, HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
		Check(CopyText(L"Value", name)); *type = QuadooForm::ValueInt; *writable = TRUE;
	Cleanup:
		return hr;
	}
};
class CEvents : public CModuleObject, public IQuadooEventDescriptors
{
	ExternalSchema m_schema;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		*ppvObj = NULL;
		UNK_INTERFACE(IQuadooEventDescriptors)
	END_UNK_MAP
	CEvents(ExternalSchema schema) : m_schema(schema) {}
	STDMETHOD(GetCount)(UINT* value) { if(value) *value = 2; return value ? S_OK : E_POINTER; }
	STDMETHOD(GetAt)(UINT index, IQuadooEventDescriptor** value)
	{
		HRESULT hr = S_OK;
		CheckIf(!value, E_POINTER); *value = NULL; CheckIf(index >= 2, HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
		*value = __new CEvent(index, m_schema); CheckAlloc(*value);
	Cleanup:
		return hr;
	}
	STDMETHOD(Find)(RSTRING name, IQuadooEventDescriptor** value)
	{
		if(value) *value = NULL;
		return !value ? E_POINTER : Equal(name, RSTRING_CAST(L"OnPulse")) ? GetAt(0, value) : Equal(name, RSTRING_CAST(L"OnFinalizing")) ? GetAt(1, value) : DISP_E_MEMBERNOTFOUND;
	}
};
class CArgs : public CModuleObject, public IQuadooEventArgs
{
	IQuadooComponent* m_sender;
	BOOL m_final;
	INT m_value;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		*ppvObj = NULL;
		UNK_INTERFACE(IQuadooEventArgs)
	END_UNK_MAP
	CArgs(IQuadooComponent* sender, BOOL finalizing, INT value) : m_final(finalizing), m_value(value) { SetInterface(m_sender, sender); }
	~CArgs() { m_sender->Release(); }
	STDMETHOD(GetEventName)(RSTRING* value) { return CopyText(m_final ? L"OnFinalizing" : L"OnPulse", value); }
	STDMETHOD(GetSender)(IQuadooComponent** value) { if(value) { *value = m_sender; m_sender->AddRef(); } return value ? S_OK : E_POINTER; }
	STDMETHOD(GetArgumentCount)(UINT* value) { if(value) *value = m_final ? 0 : 1; return value ? S_OK : E_POINTER; }
	STDMETHOD(GetArgumentName)(UINT index, RSTRING* value) { return m_final || index ? HRESULT_FROM_WIN32(ERROR_INVALID_INDEX) : CopyText(L"Value", value); }
	STDMETHOD(GetArgumentType)(UINT index, QuadooForm::EventValueType* value) { if(value) *value = QuadooForm::ValueInt; return !value ? E_POINTER : m_final || index ? HRESULT_FROM_WIN32(ERROR_INVALID_INDEX) : S_OK; }
	STDMETHOD(GetInt)(UINT index, INT* value) { if(value) *value = m_value; return !value ? E_POINTER : m_final || index ? HRESULT_FROM_WIN32(ERROR_INVALID_INDEX) : S_OK; }
	STDMETHOD(SetInt)(UINT index, INT value) { if(!m_final && !index) m_value = value; return m_final || index ? HRESULT_FROM_WIN32(ERROR_INVALID_INDEX) : S_OK; }
	STDMETHOD(GetLong)(UINT index, LONGLONG* value) { return DISP_E_TYPEMISMATCH; }
	STDMETHOD(SetLong)(UINT index, LONGLONG value) { return DISP_E_TYPEMISMATCH; }
	STDMETHOD(GetBool)(UINT index, BOOL* value) { return DISP_E_TYPEMISMATCH; }
	STDMETHOD(SetBool)(UINT index, BOOL value) { return DISP_E_TYPEMISMATCH; }
	STDMETHOD(GetString)(UINT index, RSTRING* value) { if(value) *value = NULL; return DISP_E_TYPEMISMATCH; }
	STDMETHOD(SetString)(UINT index, RSTRING value) { return DISP_E_TYPEMISMATCH; }
	STDMETHOD(GetComponent)(UINT index, IQuadooComponent** value) { if(value) *value = NULL; return DISP_E_TYPEMISMATCH; }
	STDMETHOD(SetComponent)(UINT index, IQuadooComponent* value) { return DISP_E_TYPEMISMATCH; }
};

class CFactory;
class CMeter : public CModuleObject, public IQuadooStaticControl, public IQuadooProperties,
	public IQuadooPropertyNotifications, public IQuadooConstructionParticipant,
	public IQuadooLayoutControl, public IQuadooMessageHandler, public IExternalMeter, public IExternalTestControl
{
	CFactory* m_factory;
	IQuadooConstructionContext* m_context;
	IQuadooControlContainer* m_parent;
	TArray<IJSONValue*> m_values;
	TArray<IQuadooPropertyObserver*> m_observers;
	IQuadooDelegate* m_pulse;
	IQuadooDelegate* m_final;
	CRString m_name;
	GUID m_id;
	DWORD m_thread;
	QuadooForm::ComponentState m_state;
	QuadooForm::Rect m_bounds, m_effective;
	UINT m_arranges, m_dpi;
	BOOL m_abort;
	ExternalFailure m_failure;
	HRESULT m_failureResult;
	BOOL m_isolated;
	ExternalSchema m_instanceSchema;
	HRESULT Fault(ExternalFailure point) { return m_failure == point ? m_failureResult : S_OK; }
	HRESULT Thread() { return m_thread == GetCurrentThreadId() ? S_OK : QuadooForm::EWrongThread; }
	HRESULT Alive() { HRESULT hr = Thread(); return FAILED(hr) ? hr : m_state >= QuadooForm::Destroying ? QuadooForm::EDestroyed : S_OK; }
	INT Number(PCWSTR name) { INT value = 0; m_values[Index(name)]->GetInteger(&value); return value; }
	HRESULT Notify(UINT index)
	{
		HRESULT hr = S_OK;
		TStackRef<CMeter> keepAlive(this);
		TArray<IQuadooPropertyObserver*> observers;
		for(sysint n = 0; n < m_observers.Length(); n++) { Check(observers.Append(m_observers[n])); m_observers[n]->AddRef(); }
		for(sysint n = 0; n < observers.Length(); n++) { Check(observers[n]->OnPropertyChanged(static_cast<IQuadooStaticControl*>(this), index)); }
	Cleanup:
		for(sysint n = 0; n < observers.Length(); n++) observers[n]->Release();
		return hr;
	}
	HRESULT Bind(RSTRING name, IQuadooDelegate** delegate)
	{
		HRESULT hr = S_OK;
		TStackRef<IQuadooEventsBinder> binder;
		QuadooForm::ContextMode mode;
		*delegate = NULL; Check(m_context->GetMode(&mode));
		if(RStrLen(name) && mode == QuadooForm::Runtime)
		{
			Check(m_context->GetBinder(&binder)); CheckIf(!binder, QuadooForm::EHandlerNotFound);
			hr = binder->BindEvent(name, delegate); CheckIf(hr == S_FALSE || (SUCCEEDED(hr) && !*delegate), QuadooForm::EHandlerNotFound); Check(hr);
		}
	Cleanup:
		return hr;
	}
public:
	STDMETHOD(QueryInterface)(REFIID iid, PVOID* result)
	{
		if(!result) return E_POINTER; *result = NULL;
		if(iid == __uuidof(IQuadooWinControl))
		{
			if(m_failure == ExternalNullWindowQI) return S_OK;
			if(m_failure == ExternalFalseWindowQI) return S_FALSE;
			if(m_failure == ExternalInvalidWindowQI) { *result = static_cast<IQuadooStaticControl*>(this); AddRef(); return E_NOINTERFACE; }
		}
		HRESULT fault = iid == IID_IUnknown ? Fault(ExternalIdentity) : iid == __uuidof(IQuadooWinControl) ? Fault(ExternalWindowQI) : iid == __uuidof(IQuadooFrame) ? Fault(ExternalFrameQI) : iid == __uuidof(IQuadooPropertyNotifications) ? Fault(ExternalNotificationsQI) : S_OK;
		return FAILED(fault) ? fault : CBaseUnknown::QueryInterface(iid,result);
	}
	STDMETHOD_(ULONG, AddRef)() { return CBaseUnknown::AddRef(); }
	STDMETHOD_(ULONG, Release)() { return CBaseUnknown::Release(); }
	BEGIN_UNK_MAP
		*ppvObj = NULL;
		UNK_INTERFACE(IQuadooComponent)
		UNK_INTERFACE(IQuadooControl)
		UNK_INTERFACE(IQuadooStaticControl)
		UNK_INTERFACE(IQuadooProperties)
		UNK_INTERFACE(IQuadooPropertyNotifications)
		UNK_INTERFACE(IQuadooConstructionParticipant)
		UNK_INTERFACE(IQuadooLayoutControl)
		UNK_INTERFACE(IQuadooMessageHandler)
		UNK_INTERFACE(IExternalMeter)
		UNK_INTERFACE(IExternalTestControl)
	END_UNK_MAP
	CMeter(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent);
	~CMeter();
	HRESULT Construct(IJSONObject* definition, BOOL registerControl = TRUE);
	STDMETHOD(GetID)(GUID* value) { HRESULT fault = Fault(ExternalID); if(FAILED(fault)) return fault; if(value) *value = m_id; return value ? Thread() : E_POINTER; }
	STDMETHOD(GetName)(RSTRING* value) { HRESULT fault = Fault(ExternalName); if(m_failure == ExternalAfterSubscribe && m_observers.Length()) fault = m_failureResult; if(FAILED(fault)) return fault; if(value) RStrSet(*value, m_name); return value ? Thread() : E_POINTER; }
	STDMETHOD(SetName)(RSTRING value)
	{
		HRESULT hr;
		TStackRef<IJSONValue> json;
		Check(Alive()); CheckIf(!value || !RStrIsWide(value) || !RStrLen(value), E_INVALIDARG);
		Check(JSONCreateString(value,&json)); Check(QuadooValidateControlName(this, value));
		if(static_cast<UINT>(m_values.Length()) > Index(L"Name")) ReplaceInterface(m_values[Index(L"Name")],static_cast<IJSONValue*>(json));
		m_name = value;
		if(m_state == QuadooForm::Ready) { Check(Notify(Index(L"Name"))); }
	Cleanup:
		return hr;
	}
	STDMETHOD(GetTypeName)(RSTRING* value) { return CopyText(L"ExternalMeter", value); }
	STDMETHOD(GetProperties)(IQuadooProperties** value) { HRESULT fault = Fault(ExternalProperties); if(FAILED(fault)) return fault; if(m_failure == ExternalNullProperties) { if(value) *value = NULL; return S_OK; } if(value) { *value = static_cast<IQuadooProperties*>(this); AddRef(); } return value ? Thread() : E_POINTER; }
	STDMETHOD(GetState)(QuadooForm::ComponentState* value) { HRESULT fault = Fault(ExternalState); if(FAILED(fault)) return fault; if(value) *value = m_state; return value ? Thread() : E_POINTER; }
	STDMETHOD(Destroy)(VOID);
	STDMETHOD(GetParent)(IQuadooControlContainer** value) { HRESULT fault = Fault(ExternalParent); if(FAILED(fault)) return fault; if(value) { *value = m_parent; if(*value) (*value)->AddRef(); } return value ? Thread() : E_POINTER; }
	STDMETHOD(GetBounds)(QuadooForm::Rect* value) { if(value) *value = m_bounds; return value ? Thread() : E_POINTER; }
	STDMETHOD(GetEffectiveBounds)(QuadooForm::Rect* value) { if(value) *value = m_effective; return value ? Thread() : E_POINTER; }
	STDMETHOD(SetBounds)(const QuadooForm::Rect* value)
	{
		HRESULT hr;
		Check(Alive()); CheckIf(!value || value->Width < 0 || value->Height < 0, E_INVALIDARG);
		Check(SetValueInt(Index(L"Left"), value->Left)); Check(SetValueInt(Index(L"Top"), value->Top));
		Check(SetValueInt(Index(L"Width"), value->Width)); Check(SetValueInt(Index(L"Height"), value->Height));
	Cleanup:
		return hr;
	}
	STDMETHOD(GetVisible)(BOOL* value) { return GetValueBool(Index(L"Visible"), value); }
	STDMETHOD(SetVisible)(BOOL value) { return SetValueBool(Index(L"Visible"), value); }
	STDMETHOD(GetEnabled)(BOOL* value) { return GetValueBool(Index(L"Enabled"), value); }
	STDMETHOD(SetEnabled)(BOOL value) { return SetValueBool(Index(L"Enabled"), value); }
	STDMETHOD(GetDescriptors)(IQuadooPropertyDescriptors** value);
	STDMETHOD(SetFailure)(ExternalFailure failure, HRESULT error) { m_failure = failure; m_failureResult = error; return S_OK; }
	STDMETHOD(GetTestState)(UINT* observers, ULONG* references, BOOL* isolated)
	{
		if(!observers || !references || !isolated) return E_POINTER;
		*observers = static_cast<UINT>(m_observers.Length()); *references = CBaseUnknown::AddRef() - 1; CBaseUnknown::Release(); *isolated = m_isolated; return S_OK;
	}
	STDMETHOD(SendObserverNotification)(VOID) { return Notify(Index(L"Width")); }
	STDMETHOD(GetValueJSON)(UINT index, IJSONValue** value)
	{
		HRESULT hr;
		CheckIf(!value, E_POINTER); *value = NULL; Check(Thread()); CheckIf(index >= static_cast<UINT>(m_values.Length()), HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
		Check(JSONClone(m_values[index], value, TRUE));
	Cleanup:
		return hr;
	}
	STDMETHOD(SetValueJSON)(UINT index, IJSONValue* value)
	{
		HRESULT hr;
		TStackRef<IJSONValue> clone;
		TStackRef<IQuadooDelegate> delegate;
		INT number;
		bool flag;
		CRString text;
		Check(Alive()); CheckIf(!value || index >= static_cast<UINT>(m_values.Length()), E_INVALIDARG);
		if(index == Index(L"Name")) { Check(value->GetString(&text)); Check(SetName(text)); goto Cleanup; }
		if(specs[index].type == QuadooProperty::Event || specs[index].type == QuadooProperty::String)
		{ Check(value->GetString(&text)); CheckIf(!RStrIsWide(static_cast<RSTRING>(text)), E_INVALIDARG); if(specs[index].type == QuadooProperty::Event && m_state != QuadooForm::Constructing) { Check(Bind(text, &delegate)); } }
		else if(specs[index].type == QuadooProperty::Boolean) { Check(value->GetBoolean(&flag)); }
		else { Check(value->GetInteger(&number)); if(index == Index(L"Width") || index == Index(L"Height")) { CheckIf(number < 0, E_INVALIDARG); } }
		Check(JSONClone(value, &clone, TRUE)); SafeRelease(m_values[index]); m_values[index] = clone.Detach();
		if(index == Index(L"OnPulse")) { SafeRelease(m_pulse); m_pulse = delegate.Detach(); }
		if(index == Index(L"OnFinalizing")) { SafeRelease(m_final); m_final = delegate.Detach(); }
		m_bounds.Left = Number(L"Left"); m_bounds.Top = Number(L"Top"); m_bounds.Width = Number(L"Width"); m_bounds.Height = Number(L"Height");
		if(m_state == QuadooForm::Ready) { Check(Notify(index)); }
	Cleanup:
		return hr;
	}
	STDMETHOD(GetValueInt)(UINT index, INT* value) { return !value ? E_POINTER : index >= static_cast<UINT>(m_values.Length()) ? E_INVALIDARG : m_values[index]->GetInteger(value); }
	STDMETHOD(SetValueInt)(UINT index, INT value) { HRESULT hr; TStackRef<IJSONValue> json; Check(JSONCreateInteger(value, &json)); Check(SetValueJSON(index, json)); Cleanup: return hr; }
	STDMETHOD(GetValueBool)(UINT index, BOOL* value) { HRESULT hr; bool flag; CheckIf(!value,E_POINTER); CheckIf(index >= static_cast<UINT>(m_values.Length()),E_INVALIDARG); Check(m_values[index]->GetBoolean(&flag)); *value = flag; Cleanup: return hr; }
	STDMETHOD(SetValueBool)(UINT index, BOOL value) { HRESULT hr; TStackRef<IJSONValue> json; Check(JSONCreateBool(value != 0, &json)); Check(SetValueJSON(index, json)); Cleanup: return hr; }
	STDMETHOD(GetValueString)(UINT index, RSTRING* value) { if(value) *value = NULL; return !value ? E_POINTER : index >= static_cast<UINT>(m_values.Length()) ? E_INVALIDARG : m_values[index]->GetString(value); }
	STDMETHOD(SetValueString)(UINT index, RSTRING value) { HRESULT hr; TStackRef<IJSONValue> json; Check(JSONCreateString(value, &json)); Check(SetValueJSON(index, json)); Cleanup: return hr; }
	STDMETHOD(GetValueEvent)(UINT index, RSTRING* value) { return GetValueString(index, value); }
	STDMETHOD(SetValueEvent)(UINT index, RSTRING value) { return SetValueString(index, value); }
	STDMETHOD(GetValueLong)(UINT index, LONGLONG* value) { return DISP_E_TYPEMISMATCH; }
	STDMETHOD(SetValueLong)(UINT index, LONGLONG value) { return DISP_E_TYPEMISMATCH; }
	STDMETHOD(GetValueFloat)(UINT index, DOUBLE* value) { return DISP_E_TYPEMISMATCH; }
	STDMETHOD(SetValueFloat)(UINT index, DOUBLE value) { return DISP_E_TYPEMISMATCH; }
	STDMETHOD(AddObserver)(IQuadooPropertyObserver* observer)
	{
		HRESULT hr; Check(Alive()); CheckIf(!observer, E_INVALIDARG); Check(Fault(ExternalObserverBefore));
		Check(m_observers.Append(observer)); observer->AddRef();
		if(m_failure == ExternalObserverReentrant)
		{
			UINT before, after; TStackRef<IQuadooControl> found;
			Check(m_parent->GetChildCount(&before));
			m_isolated = m_parent->FindControl(m_name,&found) == HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
			m_isolated = (m_parent->BeginUpdate() == QuadooForm::EInvalidState) && m_isolated;
			m_isolated = (m_parent->UpdateLayout() == S_FALSE) && m_isolated;
			m_isolated = (m_parent->RegisterControl(this) == QuadooForm::EInvalidState) && m_isolated;
			m_isolated = (m_parent->Destroy() == QuadooForm::EInvalidState) && m_isolated;
			Check(observer->OnPropertyChanged(this,Index(L"Width"))); Check(m_parent->GetChildCount(&after)); m_isolated = before == after && m_isolated;
		}
		CheckIf(m_failure == ExternalObserverAfter || m_failure == ExternalObserverReentrant || m_failure == ExternalObserverRollbackFailure, m_failureResult);
	Cleanup:
		return hr;
	}
	STDMETHOD(RemoveObserver)(IQuadooPropertyObserver* observer)
	{
		HRESULT hr = S_FALSE;
		if(m_failure == ExternalObserverRollbackFailure) return m_failureResult;
		for(sysint n = 0; n < m_observers.Length(); n++) if(observer == m_observers[n]) { m_observers.Remove(n, NULL); observer->Release(); hr = S_OK; break; }
		return hr;
	}
	STDMETHOD(BindEvents)(IQuadooConstructionContext* context)
	{
		HRESULT hr; CRString pulse, finalizing;
		Check(Alive()); CheckIf(context != m_context || m_state != QuadooForm::Constructing, QuadooForm::EInvalidState);
		m_state = QuadooForm::Binding; Check(GetValueEvent(Index(L"OnPulse"), &pulse)); Check(GetValueEvent(Index(L"OnFinalizing"), &finalizing));
		Check(Bind(pulse, &m_pulse)); Check(Bind(finalizing, &m_final));
	Cleanup:
		return hr;
	}
	STDMETHOD(InitializeControl)(VOID)
	{
		HRESULT hr; INT accepted;
		Check(Alive()); CheckIf(m_state != QuadooForm::Binding, QuadooForm::EInvalidState);
		m_state = QuadooForm::Initializing; Check(Pulse(Number(L"Level"), &accepted)); m_state = QuadooForm::Ready;
	Cleanup:
		return hr;
	}
	STDMETHOD(ActivateControl)(VOID) { return m_state == QuadooForm::Ready ? S_OK : QuadooForm::EInvalidState; }
	STDMETHOD(AbortConstruction)(VOID) { m_abort = TRUE; return Destroy(); }
	STDMETHOD(Measure)(QuadooForm::Size* value) { if(value) { value->Width = 111; value->Height = 23; } return value ? Alive() : E_POINTER; }
	STDMETHOD(Arrange)(const QuadooForm::Rect* value, UINT dpi)
	{
		HRESULT hr; Check(Alive()); CheckIf(!value || dpi < 48, E_INVALIDARG);
		m_effective = *value; m_dpi = dpi; m_arranges++;
	Cleanup:
		return hr;
	}
	STDMETHOD(GetArrangeCount)(UINT* count, UINT* dpi) { if(count) *count = m_arranges; if(dpi) *dpi = m_dpi; return count && dpi ? S_OK : E_POINTER; }
	STDMETHOD(Pulse)(INT value, INT* accepted)
	{
		HRESULT hr = S_OK;
		TStackRef<CMeter> keepAlive(this); TStackRef<CArgs> args;
		CheckIf(!accepted,E_POINTER); *accepted = value; Check(Alive());
		if(m_pulse)
		{ TStackRef<IQuadooDelegate> delegate(m_pulse); args.Attach(__new CArgs(this,FALSE,value)); CheckAlloc(args); Check(delegate->Invoke(args)); Check(args->GetInt(0,accepted)); }
	Cleanup:
		return hr;
	}
	STDMETHOD(Paint)(HDC dc, const RECT* bounds)
	{
		HRESULT hr; Check(Alive()); CheckIf(!dc || !bounds,E_INVALIDARG);
		FillRect(dc,bounds,static_cast<HBRUSH>(GetStockObject(GRAY_BRUSH))); g_externalCounts.paints++;
	Cleanup:
		return hr;
	}
	STDMETHOD(HitTest)(INT x, INT y, BOOL* hit)
	{
		HRESULT hr; BOOL visible, enabled;
		CheckIf(!hit,E_POINTER); *hit = FALSE; Check(Alive()); Check(GetVisible(&visible)); Check(GetEnabled(&enabled));
		*hit = visible && enabled && x >= m_effective.Left && y >= m_effective.Top && x < m_effective.Left + m_effective.Width && y < m_effective.Top + m_effective.Height;
	Cleanup:
		return hr;
	}
	STDMETHOD(HandleMessage)(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result)
	{
		HRESULT hr = S_OK; INT accepted;
		CheckIf(!handled || !result,E_POINTER); *handled = FALSE; *result = 0; Check(Alive());
		if(msg == WM_LBUTTONUP) { *handled = TRUE; g_externalCounts.messages++; Check(Pulse(Number(L"Level"),&accepted)); }
	Cleanup:
		return hr;
	}
};
#include "ExternalScript.inl"

class CFactory : public CIconFactory, public IQuadooControlFactory, public IQuadooFactoryLifetime, public IExternalTestFactory, public IExternalDesignPolicy
{
	UINT m_instances;
	BOOL m_scriptHandler;
	ExternalSchema m_schema;
	HRESULT m_designResult;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		*ppvObj = NULL;
		UNK_INTERFACE(IQuadooControlFactory)
		UNK_INTERFACE(IQuadooFactoryLifetime)
		UNK_INTERFACE(IExternalTestFactory)
		UNK_INTERFACE(IExternalDesignPolicy)
		UNK_CHAIN_TO(CIconFactory)
	END_UNK_MAP
	CFactory(BOOL scriptHandler = FALSE) : CIconFactory(101), m_instances(0), m_scriptHandler(scriptHandler), m_schema(ExternalValidSchema), m_designResult(S_OK)
	{
	}
	STDMETHOD(SetDesignResult)(HRESULT result) { m_designResult = result; return S_OK; }
	STDMETHOD(SetSchema)(ExternalSchema schema) { m_schema = schema; return S_OK; }
	STDMETHOD(CreateControl)(IQuadooConstructionContext* context, IQuadooControlContainer* parent, IJSONObject* definition, IQuadooControl** value) { return Create(context,parent,definition,value,TRUE); }
	STDMETHOD(CreateCandidate)(IQuadooConstructionContext* context, IQuadooControlContainer* parent, IJSONObject* definition, IQuadooControl** value) { return Create(context,parent,definition,value,FALSE); }
	~CFactory() { g_externalCounts.factories++; }
	VOID AddInstance() { m_instances++; }
	VOID RemoveInstance() { m_instances--; }
	STDMETHOD(GetTypeName)(RSTRING* value)
	{
		return m_scriptHandler ? RStrCreateW(LSP(L"ExternalScriptMeter"),value) : RStrCreateW(LSP(L"ExternalMeter"),value);
	}
	STDMETHOD(GetLiveCount)(UINT* value) { if(value) *value = m_instances; return value ? S_OK : E_POINTER; }
	STDMETHOD(GetProperties)(IQuadooPropertyDescriptors** value) { if(value) *value = __new CProperties(m_schema); return !value ? E_POINTER : *value ? S_OK : E_OUTOFMEMORY; }
	STDMETHOD(GetEvents)(IQuadooEventDescriptors** value) { if(value) *value = __new CEvents(m_schema); return !value ? E_POINTER : *value ? S_OK : E_OUTOFMEMORY; }
	HRESULT Create(IQuadooConstructionContext* context, IQuadooControlContainer* parent, IJSONObject* definition, IQuadooControl** value, BOOL registerControl)
	{
		HRESULT hr;
		TStackRef<CMeter> meter;
		QuadooForm::ContextMode mode;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		CheckIf(!context || !definition,E_INVALIDARG);
		Check(context->GetMode(&mode));
		if(mode == QuadooForm::Design)
		{
			Check(m_designResult);
		}
		if(m_scriptHandler)
		{
			meter.Attach(__new CScriptMeter(this,context,parent));
		}
		else
		{
			meter.Attach(__new CMeter(this,context,parent));
		}
		CheckAlloc(meter);
		Check(meter->Construct(definition,registerControl));
		*value = meter.Detach();
	Cleanup:
		if(FAILED(hr) && meter)
		{
			meter->AbortConstruction();
		}
		return hr;
	}
};
CMeter::CMeter(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) :
	m_factory(factory), m_parent(parent), m_pulse(NULL), m_final(NULL), m_thread(GetCurrentThreadId()),
	m_state(QuadooForm::Constructing), m_arranges(0), m_dpi(96), m_abort(FALSE), m_failure(ExternalNoFailure), m_failureResult(E_ABORT), m_isolated(TRUE), m_instanceSchema(ExternalValidSchema)
{
	SetInterface(m_context,context); factory->AddRef(); factory->AddInstance(); g_externalCounts.objects++;
	ZeroMemory(&m_id,sizeof(m_id)); ZeroMemory(&m_bounds,sizeof(m_bounds)); m_effective = m_bounds;
}
CMeter::~CMeter()
{
	for(sysint n = 0; n < m_values.Length(); n++) m_values[n]->Release();
	for(sysint n = 0; n < m_observers.Length(); n++) m_observers[n]->Release();
	SafeRelease(m_pulse); SafeRelease(m_final); SafeRelease(m_context);
	m_factory->RemoveInstance(); m_factory->Release(); g_externalCounts.objects--;
}
HRESULT CMeter::Construct(IJSONObject* definition, BOOL registerControl)
{
	HRESULT hr;
	TStackRef<IJSONValue> json;
	TStackRef<IJSONObject> properties, events;
	Check(CoCreateGuid(&m_id));
	if(definition->HasField(RSTRING_CAST(L"id"))) { Check(definition->FindNonNullValueW(L"id",&json)); Check(json->GetStringAsGuid(m_id)); json.Release(); }
	Check(definition->FindNonNullValueW(L"name",&json)); Check(json->GetString(&m_name)); json.Release();
	for(UINT n = 0; n < ARRAYSIZE(specs); n++)
	{ Check(Default(n,&json)); Check(m_values.Append(json)); json.Detach(); }
	if(definition->HasField(RSTRING_CAST(L"properties"))) { Check(definition->FindNonNullValueW(L"properties",&json)); Check(json->GetObject(&properties)); json.Release(); }
	if(definition->HasField(RSTRING_CAST(L"events"))) { Check(definition->FindNonNullValueW(L"events",&json)); Check(json->GetObject(&events)); json.Release(); }
	for(UINT n = 0; n < ARRAYSIZE(specs); n++)
	{
		IJSONObject* source = specs[n].type == QuadooProperty::Event ? static_cast<IJSONObject*>(events) : static_cast<IJSONObject*>(properties);
		if(source && source->HasField(RSTRING_CAST(specs[n].name)))
		{ Check(source->FindNonNullValueW(specs[n].name,&json)); Check(SetValueJSON(n,json)); json.Release(); }
	}
	SafeRelease(m_values[Index(L"Name")]); Check(JSONCreateString(m_name, &m_values[Index(L"Name")]));
	m_bounds.Left = Number(L"Left"); m_bounds.Top = Number(L"Top"); m_bounds.Width = Number(L"Width"); m_bounds.Height = Number(L"Height"); m_effective = m_bounds;
	if(m_parent && registerControl) { Check(m_parent->RegisterControl(this)); }
	if(definition->HasField(RSTRING_CAST(L"testInstanceSchema"))) { INT schema; Check(definition->FindNonNullValueW(L"testInstanceSchema",&json)); Check(json->GetInteger(&schema)); m_instanceSchema = static_cast<ExternalSchema>(schema); }
Cleanup:
	return hr;
}
STDMETHODIMP CMeter::GetDescriptors(IQuadooPropertyDescriptors** value)
{
	HRESULT hr; CheckIf(!value,E_POINTER); *value = NULL; Check(Fault(ExternalDescriptors));
	if(m_failure == ExternalNullDescriptors) { hr = S_OK; goto Cleanup; }
	if(m_instanceSchema != ExternalValidSchema) { *value = __new CProperties(m_instanceSchema); CheckAlloc(*value); hr = S_OK; }
	else hr = m_factory->GetProperties(value);
Cleanup:
	return hr;
}
STDMETHODIMP CMeter::Destroy()
{
	HRESULT hr;
	TStackRef<CMeter> keepAlive(this);
	Check(Thread()); CheckIfIgnore(m_state >= QuadooForm::Destroying,S_FALSE); m_state = QuadooForm::Destroying;
	if(m_final && !m_abort)
	{
		TStackRef<CArgs> args; args.Attach(__new CArgs(this,TRUE,0));
		hr = args ? m_final->Invoke(args) : E_OUTOFMEMORY;
	}
	if(m_parent) { IQuadooControlContainer* parent = m_parent; m_parent = NULL; parent->UnregisterControl(this); }
	SafeRelease(m_pulse); SafeRelease(m_final); SafeRelease(m_context);
	for(sysint n = 0; n < m_observers.Length(); n++) m_observers[n]->Release(); m_observers.Clear();
	m_state = QuadooForm::Destroyed; g_externalCounts.destroyed++;
Cleanup:
	return hr;
}
#include "ExternalComponent.inl"
}

extern "C" HRESULT WINAPI CreateExternalFactory(IQuadooControlFactory** factory)
{
	HRESULT hr = S_OK;
	CheckIf(!factory,E_POINTER); *factory = __new External::CFactory; CheckAlloc(*factory);
Cleanup:
	return hr;
}

extern "C" HRESULT WINAPI GetExternalCounts(ExternalCounts* counts)
{
 if(counts) *counts = g_externalCounts; return counts ? S_OK : E_POINTER;
}
extern "C" HRESULT WINAPI CreateExternalComponentFactory(IQuadooComponentFactory** factory)
{
	HRESULT hr = S_OK;
	CheckIf(!factory,E_POINTER); *factory = __new External::CServiceFactory; CheckAlloc(*factory);
Cleanup:
	return hr;
}
extern "C" HRESULT WINAPI DllCanUnloadNow() { return g_moduleObjects ? S_FALSE : S_OK; }
static LRESULT CALLBACK ExternalKeyClaims(HWND window, UINT message, WPARAM key, LPARAM flags, UINT_PTR id, DWORD_PTR data)
{
	if(message == WM_GETDLGCODE && (key == VK_RETURN || key == VK_ESCAPE)) return DLGC_WANTMESSAGE;
	if(message == WM_KEYDOWN && (key == VK_RETURN || key == VK_ESCAPE)) g_externalCounts.messages++;
	if(message == WM_NCDESTROY)
	{
		RemoveWindowSubclass(window,ExternalKeyClaims,id);
		LRESULT result = DefSubclassProc(window,message,key,flags); InterlockedDecrement(&g_moduleObjects); return result;
	}
	return DefSubclassProc(window,message,key,flags);
}
extern "C" HRESULT WINAPI ConfigureExternalKeyClaims(IQuadooWinControl* control, BOOL enable)
{
	HRESULT hr; HWND window; DWORD_PTR previous;
	CheckIf(!control,E_INVALIDARG); Check(control->GetHandle(&window)); CheckIf(GetWindowThreadProcessId(window,NULL) != GetCurrentThreadId(),QuadooForm::EWrongThread);
	BOOL installed = GetWindowSubclass(window,ExternalKeyClaims,1,&previous);
	if(enable && !installed) { CheckIf(!SetWindowSubclass(window,ExternalKeyClaims,1,0),E_FAIL); InterlockedIncrement(&g_moduleObjects); }
	else if(!enable && installed) { CheckIf(!RemoveWindowSubclass(window,ExternalKeyClaims,1),E_FAIL); InterlockedDecrement(&g_moduleObjects); }
	hr = S_OK;
Cleanup:
	return hr;
}

extern "C" HRESULT WINAPI CreateExternalScriptFactory(IQuadooControlFactory** factory)
{
	HRESULT hr = S_OK;
	CheckIf(!factory,E_POINTER);
	*factory = __new External::CFactory(TRUE);
	CheckAlloc(*factory);
Cleanup:
	return hr;
}
