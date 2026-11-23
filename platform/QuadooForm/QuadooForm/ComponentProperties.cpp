#include "Runtime.h"
#include "ComponentRuntime.h"
#include <float.h>

namespace QF
{
static HRESULT NormalizeComponentValue(IQuadooPropertyDescriptor* descriptor, IJSONValue* input, IJSONValue** value)
{
	HRESULT hr; CRString name; QuadooProperty::Type type; DWORD flags; UINT choices;
	TStackRef<IJSONValue> converted;
	CheckIf(!descriptor || !value,E_INVALIDARG); *value = NULL;
	Check(descriptor->GetName(&name)); Check(descriptor->GetType(&type)); Check(descriptor->GetFlags(&flags));
	Check(descriptor->GetEnumCount(&choices)); CheckIf(choices > 4096,QuadooForm::EInvalidDefinition);
	if(input && input->GetType() == JSON::String && choices && (type == QuadooProperty::Enum || type == QuadooProperty::Int || type == QuadooProperty::Long))
	{
		CRString label; Check(input->GetString(&label)); BOOL found = FALSE;
		for(UINT n = 0; n < choices; n++) { CRString candidate; LONGLONG number; Check(descriptor->GetEnumValue(n,&candidate,&number)); if(SameString(candidate,label)) { if(type == QuadooProperty::Long) { Check(JSONCreateLongInteger(number,&converted)); } else { CheckIf(number < INT_MIN || number > INT_MAX,E_INVALIDARG); Check(JSONCreateInteger(static_cast<INT>(number),&converted)); } found = TRUE; break; } }
		CheckIf(!found,E_INVALIDARG); input = converted;
	}
	if((type == QuadooProperty::Int || type == QuadooProperty::Enum || type == QuadooProperty::Flags) && !(flags & QuadooForm::PropertyAffectsLayout) && !SameString(name,RSTRING_CAST(L"ItemIndex")) && !SameString(name,RSTRING_CAST(L"ActivePageIndex")))
	{
		INT number; CheckIf(!input,E_INVALIDARG); Check(input->GetInteger(&number)); Check(JSONCreateInteger(number,value));
	}
	else if(type == QuadooProperty::Long && !SameString(name,RSTRING_CAST(L"ModalResult")))
	{
		LONGLONG number; CheckIf(!input,E_INVALIDARG);
		if(input->GetType() == JSON::Integer) { INT integer; Check(input->GetInteger(&integer)); number = integer; }
		else { Check(input->GetLongInteger(&number)); }
		Check(JSONCreateLongInteger(number,value));
	}
	else if(type == QuadooProperty::Float)
	{
		DOUBLE number; CheckIf(!input,E_INVALIDARG); Check(input->GetDouble(&number));
		CheckIf(!_finite(number),E_INVALIDARG); Check(JSONCreateDouble(number,value));
	}
	else if(type == QuadooProperty::Rect || type == QuadooProperty::Point || type == QuadooProperty::Size)
	{
		CheckIf(!input || input->GetType() != JSON::Object,E_INVALIDARG);
		if(SameString(name,RSTRING_CAST(L"Margins")) || SameString(name,RSTRING_CAST(L"Padding")))
		{ PropertySpec spec = { name.c_wstr(),type,flags,0,0 }; Check(NormalizeValue(&spec,input,value)); }
		else { Check(JSONClone(input,value,TRUE)); }
	}
	else if(type == QuadooProperty::ObjectReference || type == QuadooProperty::Collection)
	{
		if(input && type == QuadooProperty::Collection) { CheckIf(input->GetType() != JSON::Array && input->GetType() != JSON::Object,E_INVALIDARG); }
		Check(JSONClone(input,value,TRUE));
	}
	else
	{
		PropertySpec spec = { name.c_wstr(),type,flags,0,0 };
		Check(NormalizeValue(&spec,input,value));
	}
	if(choices && type != QuadooProperty::Flags)
	{
		LONGLONG number; if(type == QuadooProperty::Long) { Check((*value)->GetLongInteger(&number)); } else { INT integer; Check((*value)->GetInteger(&integer)); number = integer; }
		BOOL found = FALSE;
		for(UINT n = 0; n < choices; n++) { CRString label; LONGLONG candidate; Check(descriptor->GetEnumValue(n,&label,&candidate)); if(number == candidate) found = TRUE; }
		CheckIf(!found,E_INVALIDARG);
	}
	else if(choices && type == QuadooProperty::Flags)
	{
		INT number; LONGLONG allowed = 0; Check((*value)->GetInteger(&number));
		for(UINT n = 0; n < choices; n++) { CRString label; LONGLONG candidate; Check(descriptor->GetEnumValue(n,&label,&candidate)); allowed |= candidate; }
		CheckIf((static_cast<LONGLONG>(number) & ~allowed) != 0,E_INVALIDARG);
	}
Cleanup:
	if(FAILED(hr) && value) SafeRelease(*value);
	return hr;
}
static const EventSpec componentEvents[] =
{
	{ L"OnCreate",0,0,NULL,0 }, { L"OnDestroy",0,0,NULL,1 },
	{ L"OnClick",0,0,NULL,5 },
	{ L"OnAccept",0,0,NULL,2 }, { L"OnCancel",0,0,NULL,3 }, { L"OnTimer",0,0,NULL,4 }
};
CComponentObject::CComponentObject(IQuadooConstructionContext* context, IQuadooComponentCollection* owner, IUnknown* factory, UINT* live) :
	m_thread(GetCurrentThreadId()), m_id(GUID_NULL), m_state(QuadooForm::Constructing), m_mode(QuadooForm::Runtime), m_owner(owner), m_live(live),
	m_descriptors(NULL), m_events(NULL), m_definition(NULL), m_abort(FALSE), m_notifying(FALSE)
{
	SetInterface(m_context,context); SetInterface(m_factory,factory); if(m_live) ++*m_live;
}
CComponentObject::~CComponentObject()
{
	ClearBindings();
	for(sysint n = 0; n < m_values.Length(); n++) { RStrRelease(m_values[n].name); SafeRelease(m_values[n].value); }
	for(sysint n = 0; n < m_observers.Length(); n++) m_observers[n]->Release();
	SafeRelease(m_definition); SafeRelease(m_descriptors); SafeRelease(m_events); SafeRelease(m_context);
	if(m_live) --*m_live; SafeRelease(m_factory);
}
HRESULT CComponentObject::CheckThread() const { return m_thread == GetCurrentThreadId() ? S_OK : QuadooForm::EWrongThread; }
HRESULT CComponentObject::CheckAlive() const { HRESULT hr = CheckThread(); return SUCCEEDED(hr) && m_state >= QuadooForm::Destroying ? QuadooForm::EDestroyed : hr; }
UINT CComponentObject::Find(PCWSTR name) const
{
	UINT index; for(index = 0; index < static_cast<UINT>(m_values.Length()); index++) if(SameString(m_values[index].name,RSTRING_CAST(name))) break; return index;
}
INT CComponentObject::Integer(PCWSTR name) const { INT value = 0; UINT index = Find(name); if(index < static_cast<UINT>(m_values.Length()) && m_values[index].value) m_values[index].value->GetInteger(&value); return value; }
BOOL CComponentObject::Boolean(PCWSTR name) const { bool value = false; UINT index = Find(name); if(index < static_cast<UINT>(m_values.Length()) && m_values[index].value) m_values[index].value->GetBoolean(&value); return value; }
HRESULT CComponentObject::String(PCWSTR name, RSTRING* value) { return GetValueString(Find(name),value); }
HRESULT CComponentObject::SetNamed(PCWSTR name, IJSONValue* value) { return SetValueJSON(Find(name),value); }
HRESULT CComponentObject::ValidateName(RSTRING name) { return m_owner ? m_owner->ValidateIdentity(static_cast<IQuadooComponent*>(this),name,m_id) : S_OK; }

HRESULT CComponentObject::Construct(IJSONObject* definition, IQuadooPropertyDescriptors* properties, IQuadooEventDescriptors* events)
{
	HRESULT hr;
	TStackRef<IJSONValue> part; TStackRef<IJSONObject> values, handlers; INT version; UINT count;
	CheckIf(!definition || !properties || !events,E_INVALIDARG);
	Check(m_context->GetMode(&m_mode));
	Check(ReadInt(definition,L"schemaVersion",1,&version));
	CheckIf(version != 1,QuadooForm::EInvalidDefinition);
	Check(ReadString(definition,L"type",L"",&m_type));
	CheckIf(!RStrLen(m_type) || RStrLen(m_type) > 256,QuadooForm::EInvalidDefinition);
	for(INT n = 0; n < RStrLen(m_type); n++) { CheckIf(!m_type.c_wstr()[n],QuadooForm::EInvalidDefinition); }
	if(definition->HasField(RSTRING_CAST(L"id")))
	{ Check(definition->FindNonNullValueW(L"id",&part)); Check(part->GetStringAsGuid(m_id)); part.Release(); }
	else { Check(CoCreateGuid(&m_id)); }
	CheckIf(IsEqualGUID(m_id,GUID_NULL),QuadooForm::EInvalidDefinition);
	Check(ReadString(definition,L"name",L"",&m_name));
	if(!RStrLen(m_name)) { m_name = static_cast<RSTRING>(NULL); Check(RStrFormatW(&m_name,L"%ls_%.8x",m_type.c_wstr(),m_id.Data1)); }
	CheckIf(RStrLen(m_name) > 256,QuadooForm::EInvalidDefinition);
	for(INT n = 0; n < RStrLen(m_name); n++) { CheckIf(!m_name.c_wstr()[n],QuadooForm::EInvalidDefinition); }
	Check(CloneObject(definition,&m_definition));
	SetInterface(m_descriptors,properties); SetInterface(m_events,events);
	if(definition->HasField(RSTRING_CAST(L"properties")))
	{ Check(definition->FindNonNullValueW(L"properties",&part)); Check(part->GetObject(&values)); part.Release(); }
	if(definition->HasField(RSTRING_CAST(L"events")))
	{ Check(definition->FindNonNullValueW(L"events",&part)); Check(part->GetObject(&handlers)); part.Release(); }
	Check(properties->GetCount(&count));
	CheckIf(count > 4096,QuadooForm::EInvalidDefinition);
	for(UINT n = 0; n < count; n++)
	{
		Slot slot; TStackRef<IQuadooPropertyDescriptor> descriptor; TStackRef<IJSONValue> value;
		CRString name; ZeroMemory(&slot,sizeof(slot));
		Check(properties->GetAt(n,&descriptor));
		Check(descriptor->GetName(&name));
		Check(descriptor->GetType(&slot.type));
		Check(descriptor->GetFlags(&slot.flags));
		Check(descriptor->GetDefaultValue(&value));
		if(SameString(name,RSTRING_CAST(L"Name"))) { value.Release(); Check(JSONCreateString(m_name,&value)); }
		else
		{
			IJSONObject* source = slot.type == QuadooProperty::Event ? static_cast<IJSONObject*>(handlers) : static_cast<IJSONObject*>(values);
			if(source && source->HasField(name))
			{
				TStackRef<IJSONValue> input;
				CheckIf(slot.flags & (QuadooForm::PropertyReadOnly | QuadooForm::PropertyRuntimeOnly),E_INVALIDARG);
				Check(source->FindValue(name,&input)); value.Release();
				Check(NormalizeComponentValue(descriptor,input,&value));
			}
		}
		Check(ValidateProperty(name.c_wstr(),value));
		slot.name = name; slot.value = value;
		Check(m_values.Append(slot)); name.Detach(); value.Detach();
	}
	if(values)
		for(sysint n = 0; n < values->Count(); n++)
		{
			CRString name; Check(values->GetValueName(n,&name));
			CheckIf(SameString(name,RSTRING_CAST(L"Name")),E_INVALIDARG);
			CheckIf(m_mode != QuadooForm::Design && Find(name.c_wstr()) == static_cast<UINT>(m_values.Length()),DISP_E_MEMBERNOTFOUND);
		}
	if(handlers)
		for(sysint n = 0; n < handlers->Count(); n++)
		{
			CRString name, handler; Check(handlers->GetValueName(n,&name));
			Check(handlers->GetValueByIndex(n,&part));
			CheckIf(!part,E_INVALIDARG);
			Check(part->GetString(&handler)); part.Release();
			UINT index = Find(name.c_wstr());
			CheckIf(index < static_cast<UINT>(m_values.Length()) && m_values[index].type != QuadooProperty::Event,E_INVALIDARG);
			CheckIf(m_mode != QuadooForm::Design && index == static_cast<UINT>(m_values.Length()),DISP_E_MEMBERNOTFOUND);
		}
	Check(ValidateName(m_name));
	hr = S_OK;
Cleanup:
	return hr;
}
STDMETHODIMP CComponentObject::GetID(GUID* id) { HRESULT hr = CheckThread(); if(SUCCEEDED(hr)) { if(id) *id = m_id; else hr = E_POINTER; } return hr; }
STDMETHODIMP CComponentObject::GetName(RSTRING* name) { HRESULT hr; CheckIf(!name,E_POINTER); *name = NULL; Check(CheckThread()); RStrSet(*name,m_name); Cleanup: return hr; }
STDMETHODIMP CComponentObject::GetTypeName(RSTRING* name) { HRESULT hr; CheckIf(!name,E_POINTER); *name = NULL; Check(CheckThread()); RStrSet(*name,m_type); Cleanup: return hr; }
STDMETHODIMP CComponentObject::GetState(QuadooForm::ComponentState* state) { HRESULT hr = CheckThread(); if(SUCCEEDED(hr)) { if(state) *state = m_state; else hr = E_POINTER; } return hr; }
STDMETHODIMP CComponentObject::GetProperties(IQuadooProperties** properties) { HRESULT hr; CheckIf(!properties,E_POINTER); *properties = NULL; Check(CheckThread()); *properties = this; AddRef(); Cleanup: return hr; }
STDMETHODIMP CComponentObject::SetName(RSTRING name)
{
	HRESULT hr;
	TStackRef<CComponentObject> keepAlive(this); TStackRef<IJSONValue> value; UINT index = Find(L"Name");
	Check(CheckAlive());
	CheckIf(!name || !RStrIsWide(name) || !RStrLen(name) || RStrLen(name) > 256,E_INVALIDARG);
	for(INT n = 0; n < RStrLen(name); n++) { CheckIf(!RStrToWide(name)[n],E_INVALIDARG); }
	CheckIf(index >= static_cast<UINT>(m_values.Length()),QuadooForm::EInvalidState);
	CheckIfIgnore(SameString(name,m_name),S_FALSE);
	Check(JSONCreateString(name,&value));
	Check(ValidateName(name));
	ReplaceInterface(m_values[index].value,static_cast<IJSONValue*>(value)); m_name = name; m_values[index].edited = TRUE;
	Check(Notify(index));
Cleanup:
	return hr;
}
STDMETHODIMP CComponentObject::GetDescriptors(IQuadooPropertyDescriptors** descriptors)
{
	HRESULT hr; CheckIf(!descriptors,E_POINTER); *descriptors = NULL;
	Check(CheckThread()); *descriptors = m_descriptors; if(*descriptors) (*descriptors)->AddRef();
Cleanup:
	return hr;
}
STDMETHODIMP CComponentObject::GetValueJSON(UINT index, IJSONValue** value)
{
	HRESULT hr; CheckIf(!value,E_POINTER); *value = NULL;
	Check(CheckThread());
	CheckIf(index >= static_cast<UINT>(m_values.Length()),HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
	Check(JSONClone(m_values[index].value,value,TRUE));
Cleanup:
	return hr;
}
HRESULT CComponentObject::TypedValue(UINT index, QuadooProperty::Type type, IJSONValue** value)
{
	HRESULT hr;
	CheckIf(!value,E_POINTER); *value = NULL;
	Check(CheckThread());
	CheckIf(index >= static_cast<UINT>(m_values.Length()),HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
	CheckIf(type != m_values[index].type && !(type == QuadooProperty::String && m_values[index].type == QuadooProperty::Memo) && !(type == QuadooProperty::Int && (m_values[index].type == QuadooProperty::Enum || m_values[index].type == QuadooProperty::Flags || m_values[index].type == QuadooProperty::Color)),DISP_E_TYPEMISMATCH);
	Check(GetValueJSON(index,value));
Cleanup:
	return hr;
}
HRESULT CComponentObject::SetTyped(UINT index, QuadooProperty::Type type, IJSONValue* value)
{
	HRESULT hr; TStackRef<IJSONValue> previous;
	Check(TypedValue(index,type,&previous));
	Check(SetValueJSON(index,value));
Cleanup:
	return hr;
}
STDMETHODIMP CComponentObject::SetValueJSON(UINT index, IJSONValue* input)
{
	HRESULT hr;
	TStackRef<CComponentObject> keepAlive(this); TStackRef<IJSONValue> normalized, previous;
	Check(CheckAlive());
	CheckIf(index >= static_cast<UINT>(m_values.Length()),HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
	Slot& slot = m_values[index];
	CheckIf(slot.flags & (QuadooForm::PropertyReadOnly | QuadooForm::PropertyRuntimeOnly),E_ACCESSDENIED);
	CheckIf(slot.type == QuadooProperty::Event && m_mode != QuadooForm::Design && m_state != QuadooForm::Constructing,QuadooForm::EInvalidState);
	{ TStackRef<IQuadooPropertyDescriptor> descriptor; Check(m_descriptors->GetAt(index,&descriptor)); Check(NormalizeComponentValue(descriptor,input,&normalized)); }
	Check(ValidateProperty(RStrToWide(slot.name),normalized));
	if(SameString(slot.name,RSTRING_CAST(L"Name"))) { CRString name; Check(normalized->GetString(&name)); Check(SetName(name)); goto Cleanup; }
	CheckIfIgnore(!JSONCompare(slot.value,normalized),S_FALSE);
	previous.Attach(slot.value); slot.value = normalized.Detach();
	hr = ChangedProperty(RStrToWide(slot.name));
	if(FAILED(hr)) { SafeRelease(slot.value); slot.value = previous.Detach(); ChangedProperty(RStrToWide(slot.name)); goto Cleanup; }
	slot.edited = TRUE;
	Check(Notify(index));
Cleanup:
	return hr;
}
#define COMPONENT_GET(method,ctype,jsonmethod,qtype) \
STDMETHODIMP CComponentObject::method(UINT index, ctype* value) { HRESULT hr; TStackRef<IJSONValue> json; CheckIf(!value,E_POINTER); Check(TypedValue(index,qtype,&json)); CheckIf(!json,E_INVALIDARG); Check(json->jsonmethod(value)); Cleanup: return hr; }
COMPONENT_GET(GetValueInt,INT,GetInteger,QuadooProperty::Int)
COMPONENT_GET(GetValueLong,LONGLONG,GetLongInteger,QuadooProperty::Long)
COMPONENT_GET(GetValueString,RSTRING,GetString,QuadooProperty::String)
COMPONENT_GET(GetValueFloat,DOUBLE,GetDouble,QuadooProperty::Float)
COMPONENT_GET(GetValueEvent,RSTRING,GetString,QuadooProperty::Event)
#undef COMPONENT_GET
STDMETHODIMP CComponentObject::GetValueBool(UINT index, BOOL* value) { HRESULT hr; bool flag; TStackRef<IJSONValue> json; CheckIf(!value,E_POINTER); Check(TypedValue(index,QuadooProperty::Boolean,&json)); Check(json->GetBoolean(&flag)); *value = flag; Cleanup: return hr; }
#define COMPONENT_SET(method,ctype,create,qtype) \
STDMETHODIMP CComponentObject::method(UINT index, ctype value) { HRESULT hr; TStackRef<IJSONValue> json; Check(create(value,&json)); Check(SetTyped(index,qtype,json)); Cleanup: return hr; }
COMPONENT_SET(SetValueInt,INT,JSONCreateInteger,QuadooProperty::Int)
COMPONENT_SET(SetValueLong,LONGLONG,JSONCreateLongInteger,QuadooProperty::Long)
COMPONENT_SET(SetValueString,RSTRING,JSONCreateString,QuadooProperty::String)
COMPONENT_SET(SetValueFloat,DOUBLE,JSONCreateDouble,QuadooProperty::Float)
COMPONENT_SET(SetValueEvent,RSTRING,JSONCreateString,QuadooProperty::Event)
#undef COMPONENT_SET
STDMETHODIMP CComponentObject::SetValueBool(UINT index, BOOL value) { HRESULT hr; TStackRef<IJSONValue> json; Check(JSONCreateBool(value != FALSE,&json)); Check(SetTyped(index,QuadooProperty::Boolean,json)); Cleanup: return hr; }
STDMETHODIMP CComponentObject::AddObserver(IQuadooPropertyObserver* observer)
{
	HRESULT hr;
	Check(CheckAlive()); CheckIf(!observer,E_INVALIDARG);
	for(sysint n = 0; n < m_observers.Length(); n++) { CheckIfIgnore(SameObject(observer,m_observers[n]),S_FALSE); }
	Check(m_observers.Append(observer)); observer->AddRef();
Cleanup:
	return hr;
}
STDMETHODIMP CComponentObject::RemoveObserver(IQuadooPropertyObserver* observer)
{
	HRESULT hr;
	Check(CheckThread()); CheckIf(!observer,E_INVALIDARG);
	for(sysint n = 0; n < m_observers.Length(); n++) if(SameObject(observer,m_observers[n])) { m_observers[n]->Release(); m_observers.Remove(n,NULL); hr = S_OK; goto Cleanup; }
	hr = S_FALSE;
Cleanup:
	return hr;
}
HRESULT CComponentObject::Notify(UINT index)
{
	HRESULT hr = S_OK; TArray<IQuadooPropertyObserver*> observers; BOOL entered = FALSE;
	if(m_state != QuadooForm::Ready || m_notifying) goto Cleanup;
	for(sysint n = 0; n < m_observers.Length(); n++) { Check(observers.Append(m_observers[n])); m_observers[n]->AddRef(); }
	m_notifying = TRUE; entered = TRUE;
	for(sysint n = 0; n < observers.Length() && m_state == QuadooForm::Ready; n++) { Check(observers[n]->OnPropertyChanged(static_cast<IQuadooComponent*>(this),index)); }
Cleanup:
	if(entered) m_notifying = FALSE;
	for(sysint n = 0; n < observers.Length(); n++) observers[n]->Release();
	return hr;
}
VOID CComponentObject::ClearBindings() { for(sysint n = 0; n < m_bindings.Length(); n++) { RStrRelease(m_bindings[n].name); SafeRelease(m_bindings[n].delegate); } m_bindings.Clear(); }
STDMETHODIMP CComponentObject::BindEvents(IQuadooConstructionContext* context)
{
	HRESULT hr; TStackRef<IQuadooEventsBinder> binder; UINT count;
	Check(CheckAlive()); CheckIf(m_state != QuadooForm::Constructing || !SameObject(context,m_context),QuadooForm::EInvalidState);
	Check(context->GetBinder(&binder));
	Check(m_events->GetCount(&count));
	m_state = QuadooForm::Binding;
	for(UINT n = 0; n < count; n++)
	{
		TStackRef<IQuadooEventDescriptor> event; CRString name, handler; TStackRef<IQuadooDelegate> delegate; Binding binding;
		Check(m_events->GetAt(n,&event)); Check(event->GetName(&name));
		Check(GetValueEvent(Find(name.c_wstr()),&handler));
		if(m_mode == QuadooForm::Runtime && RStrLen(handler))
		{
			CheckIf(!binder,QuadooForm::EHandlerNotFound);
			hr = binder->BindEvent(handler,&delegate);
			CheckIf(hr == S_FALSE || (SUCCEEDED(hr) && !delegate),QuadooForm::EHandlerNotFound);
			Check(hr);
		}
		binding.name = name; binding.delegate = delegate;
		Check(m_bindings.Append(binding)); name.Detach(); delegate.Detach();
	}
	hr = S_OK;
Cleanup:
	if(FAILED(hr)) { ClearBindings(); context->ReportError(hr,m_name,RSTRING_CAST(L"Nonvisual event binding failed")); }
	return hr;
}
HRESULT CComponentObject::Dispatch(PCWSTR event)
{
	HRESULT hr = S_OK; TStackRef<CComponentObject> keepAlive(this);
	if(m_mode != QuadooForm::Runtime || m_abort || m_state < QuadooForm::Initializing || m_state == QuadooForm::Destroyed) goto Cleanup;
	for(sysint n = 0; n < m_bindings.Length(); n++) if(SameString(m_bindings[n].name,RSTRING_CAST(event)) && m_bindings[n].delegate)
	{
		TStackRef<IQuadooDelegate> delegate(m_bindings[n].delegate); const EventSpec* spec = NULL; TStackRef<CEventArgs> args;
		for(UINT e = 0; e < ARRAYSIZE(componentEvents); e++) if(!TStrCmpAssert(componentEvents[e].name,event)) spec = &componentEvents[e];
		CheckIf(!spec,E_UNEXPECTED);
		args.Attach(__new CEventArgs(spec,static_cast<IQuadooComponent*>(this))); CheckAlloc(args);
		Check(args->Initialize());
		Check(delegate->Invoke(args)); break;
	}
Cleanup:
	return hr;
}
STDMETHODIMP CComponentObject::InitializeControl()
{
	HRESULT hr;
	Check(CheckAlive()); CheckIf(m_state != QuadooForm::Binding,QuadooForm::EInvalidState);
	m_state = QuadooForm::Initializing;
	Check(Dispatch(L"OnCreate")); Check(CheckAlive()); m_state = QuadooForm::Ready;
Cleanup:
	return hr;
}
STDMETHODIMP CComponentObject::ActivateControl() { HRESULT hr; Check(CheckAlive()); CheckIf(m_state != QuadooForm::Ready,QuadooForm::EInvalidState); Check(Activated()); Cleanup: return hr; }
STDMETHODIMP CComponentObject::AbortConstruction() { HRESULT hr; Check(CheckThread()); m_abort = TRUE; Check(Destroy()); Cleanup: return hr; }
STDMETHODIMP CComponentObject::Destroy()
{
	HRESULT hr;
	TStackRef<CComponentObject> keepAlive(this);
	Check(CheckThread()); CheckIfIgnore(m_state >= QuadooForm::Destroying,S_FALSE);
	m_state = QuadooForm::Destroying;
	hr = DestroyObject();
	{ HRESULT callback = Dispatch(L"OnDestroy"); if(FAILED(callback) && SUCCEEDED(hr)) hr = callback; }
	ClearBindings();
	for(sysint n = 0; n < m_observers.Length(); n++) m_observers[n]->Release(); m_observers.Clear();
	if(m_owner) { IQuadooComponentCollection* owner = m_owner; m_owner = NULL; HRESULT removed = owner->UnregisterComponent(static_cast<IQuadooComponent*>(this)); if(FAILED(removed) && removed != HRESULT_FROM_WIN32(ERROR_NOT_FOUND) && SUCCEEDED(hr)) hr = removed; }
	m_state = QuadooForm::Destroyed;
	SafeRelease(m_context);
	m_destroyedObservers.Notify(static_cast<IQuadooComponent*>(this));
Cleanup:
	return hr;
}
STDMETHODIMP CComponentObject::AddDestroyedObserver(IQuadooComponentDestroyedObserver* observer)
{
	HRESULT hr;
	Check(CheckThread());
	CheckIf(m_state >= QuadooForm::Destroying, QuadooForm::EDestroyed);
	Check(m_destroyedObservers.Add(observer));
Cleanup:
	return hr;
}

STDMETHODIMP CComponentObject::RemoveDestroyedObserver(IQuadooComponentDestroyedObserver* observer)
{
	HRESULT hr;
	Check(CheckThread());
	Check(m_destroyedObservers.Remove(observer));
Cleanup:
	return hr;
}

STDMETHODIMP CComponentObject::GetDefinitionTemplate(IJSONObject** definition)
{
	HRESULT hr; TStackRef<IJSONObject> result, properties, events; TStackRef<IJSONValue> value; CRString id;
	CheckIf(!definition,E_POINTER); *definition = NULL;
	Check(CheckThread()); Check(CloneObject(m_definition,&result));
	if(result->HasField(RSTRING_CAST(L"properties"))) { Check(result->FindNonNullValueW(L"properties",&value)); Check(value->GetObject(&properties)); value.Release(); }
	else { Check(JSONCreateObject(&properties)); }
	if(result->HasField(RSTRING_CAST(L"events"))) { Check(result->FindNonNullValueW(L"events",&value)); Check(value->GetObject(&events)); value.Release(); }
	else { Check(JSONCreateObject(&events)); }
	Check(GuidString(m_id,&id)); Check(PutString(result,L"id",id)); Check(PutString(result,L"name",m_name));
	for(sysint n = 0; n < m_values.Length(); n++) if(m_values[n].edited && !SameString(m_values[n].name,RSTRING_CAST(L"Name")))
	{
		IJSONObject* target = m_values[n].type == QuadooProperty::Event ? static_cast<IJSONObject*>(events) : static_cast<IJSONObject*>(properties);
		Check(JSONClone(m_values[n].value,&value,TRUE)); Check(target->AddValue(m_values[n].name,value)); value.Release();
	}
	if(properties->Count()) { Check(PutObject(result,L"properties",properties)); }
	if(events->Count()) { Check(PutObject(result,L"events",events)); }
	*definition = result.Detach();
Cleanup:
	return hr;
}
}
