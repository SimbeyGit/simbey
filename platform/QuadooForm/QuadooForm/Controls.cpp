#include "Runtime.h"

namespace QF
{
BOOL CControl::AcceptParent(IQuadooControlContainer* parent)
{
	TStackRef<IQuadooTabControl> tabs;
	return parent && parent->QueryInterface(__uuidof(IQuadooTabControl),reinterpret_cast<PVOID*>(&tabs)) == E_NOINTERFACE;
}
STDMETHODIMP CControl::GetDesignCapabilities(DWORD* capabilities)
{
	HRESULT hr;
	CheckIf(!capabilities,E_POINTER); *capabilities = 0; Check(CheckAlive());
	*capabilities = QuadooForm::DesignResizeWidth | QuadooForm::DesignResizeHeight;
	if(m_parent) *capabilities |= QuadooForm::DesignMoveX | QuadooForm::DesignMoveY;
	if(Boolean(L"AutoSize")) *capabilities &= ~(QuadooForm::DesignResizeWidth | QuadooForm::DesignResizeHeight);
	switch(Integer(L"Align"))
	{
	case QuadooForm::AlignTop: case QuadooForm::AlignBottom: *capabilities &= QuadooForm::DesignResizeHeight; break;
	case QuadooForm::AlignLeft: case QuadooForm::AlignRight: *capabilities &= QuadooForm::DesignResizeWidth; break;
	case QuadooForm::AlignClient: *capabilities = 0; break;
	}
Cleanup:
	return hr;
}
CControl::CControl(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) :
	m_thread(GetCurrentThreadId()), m_parent(parent), m_factory(factory)
{
	SetInterface(m_context, context); m_factory->AddRef(); m_factory->AddInstance();
	m_definition = NULL; m_eventNames = NULL;
	ZeroMemory(&m_id, sizeof(m_id)); ZeroMemory(&m_bounds, sizeof(m_bounds)); m_effective = m_bounds;
	m_font = NULL; m_dpi = 96; m_updates = 1;
	m_boundsDirty = m_layoutBusy = m_notifying = m_abort = m_autoDPI = FALSE;
	m_callbackError = S_OK; m_state = QuadooForm::Constructing;
}
CControl::~CControl()
{
	ClearBindings();
	for(sysint n = 0; n < m_values.Length(); n++) SafeRelease(m_values[n].value);
	for(sysint n = 0; n < m_observers.Length(); n++) m_observers[n]->Release();
	SafeRelease(m_definition); SafeRelease(m_eventNames); SafeRelease(m_context);
	if(m_font) DeleteObject(m_font);
	m_factory->RemoveInstance(); m_factory->Release();
}
UINT CControl::FindProperty(PCWSTR name) const
{
	UINT index;
	if(!name) return static_cast<UINT>(m_values.Length());
	for(index = 0; index < static_cast<UINT>(m_values.Length()); index++)
		if(m_values[index].spec && m_values[index].spec->name && 0 == TStrCmpAssert(m_values[index].spec->name, name)) break;
	return index;
}
HRESULT CControl::StoredValue(PCWSTR name, IJSONValue** value)
{
	HRESULT hr = S_OK;
	UINT index = FindProperty(name);
	CheckIf(!value, E_POINTER); *value = NULL;
	CheckIf(index >= static_cast<UINT>(m_values.Length()), DISP_E_MEMBERNOTFOUND);
	CheckIf(!m_values[index].value && m_values[index].spec->type != QuadooProperty::Icon && m_values[index].spec->type != QuadooProperty::Picture && m_values[index].spec->type != QuadooProperty::ObjectReference, QuadooForm::EInvalidState);
	*value = m_values[index].value; if(*value) (*value)->AddRef();
Cleanup:
	return hr;
}
HRESULT CControl::StoredString(PCWSTR name, RSTRING* value)
{
	HRESULT hr;
	TStackRef<IJSONValue> json;
	CheckIf(!value, E_POINTER); *value = NULL;
	Check(StoredValue(name, &json)); Check(json->GetString(value));
Cleanup:
	return hr;
}
HRESULT CControl::SetStoredValue(PCWSTR name, IJSONValue* value)
{
	HRESULT hr = S_OK;
	UINT index = FindProperty(name);
	CheckIf(index >= static_cast<UINT>(m_values.Length()), DISP_E_MEMBERNOTFOUND);
	CheckIf(!value, E_INVALIDARG);
	ReplaceInterface(m_values[index].value, value);
Cleanup:
	return hr;
}
HRESULT CControl::Dirty(PCWSTR name)
{
	HRESULT hr = S_OK;
	UINT index = FindProperty(name);
	CheckIf(index >= static_cast<UINT>(m_values.Length()), DISP_E_MEMBERNOTFOUND);
	m_values[index].dirty = TRUE;
Cleanup:
	return hr;
}
INT CControl::Integer(PCWSTR name) const
{
	INT value = 0; UINT index = FindProperty(name);
	if(index < static_cast<UINT>(m_values.Length()) && m_values[index].value) m_values[index].value->GetInteger(&value);
	return value;
}
BOOL CControl::Boolean(PCWSTR name) const
{
	bool value = false; UINT index = FindProperty(name);
	if(index < static_cast<UINT>(m_values.Length()) && m_values[index].value) m_values[index].value->GetBoolean(&value);
	return value;
}
HRESULT CControl::Insets(PCWSTR name, QuadooForm::Insets* insets) const
{
	HRESULT hr = S_OK;
	TStackRef<IJSONObject> object;
	UINT index = FindProperty(name);
	ZeroMemory(insets, sizeof(*insets));
	if(index < static_cast<UINT>(m_values.Length()))
	{
		CheckIf(!m_values[index].value, QuadooForm::EInvalidState);
		Check(m_values[index].value->GetObject(&object));
		Check(ReadInt(object,L"Left",0,&insets->Left)); Check(ReadInt(object,L"Top",0,&insets->Top));
		Check(ReadInt(object,L"Right",0,&insets->Right)); Check(ReadInt(object,L"Bottom",0,&insets->Bottom));
	}
Cleanup:
	return hr;
}
HRESULT CControl::ResolvePropertyIndex(UINT index, UINT* property, const EventSpec** event) const
{
	HRESULT hr = S_OK;
	*property = index; *event = NULL;
	if(index >= static_cast<UINT>(m_values.Length()))
	{
		index -= static_cast<UINT>(m_values.Length());
		if(index < static_cast<UINT>(m_factory->m_eventSpecs.Length()))
			*event = m_factory->m_eventSpecs[index];
		else
			hr = HRESULT_FROM_WIN32(ERROR_INVALID_INDEX);
	}
	return hr;
}

HRESULT CControl::CheckThread() const { return m_thread == GetCurrentThreadId() ? S_OK : QuadooForm::EWrongThread; }
HRESULT CControl::CheckAlive() const
{
	HRESULT hr = CheckThread();
	if(SUCCEEDED(hr) && m_state >= QuadooForm::Destroying)
		hr = QuadooForm::EDestroyed;
	return hr;
}

STDMETHODIMP CControl::GetDefinitionTemplate(IJSONObject** definition)
{
	HRESULT hr;
	CheckIf(!definition, E_POINTER); *definition = NULL; Check(CheckThread());
	Check(CloneObject(m_definition, definition));
Cleanup:
	return hr;
}

HRESULT CControl::Construct(IJSONObject* definition)
{
	HRESULT hr;
	TStackRef<IJSONValue> id;
	TStackRef<IJSONValue> name;
	CRString type, nameText;
	INT version;
	CheckIf(!definition, E_INVALIDARG);
	Check(ReadString(definition, L"type", L"", &type));
	CheckIf(!SameString(type, RSTRING_CAST(m_factory->m_type->name)), QuadooForm::EInvalidDefinition);
	Check(ReadInt(definition, L"schemaVersion", 1, &version)); CheckIf(version != 1, QuadooForm::EInvalidDefinition);
	if(definition->HasField(RSTRING_CAST(L"id")))
	{
		Check(definition->FindNonNullValueW(L"id", &id));
		Check(id->GetStringAsGuid(m_id));
	}
	else
	{
		Check(CoCreateGuid(&m_id));
	}
	CheckIf(IsEqualGUID(m_id, GUID_NULL), QuadooForm::EInvalidDefinition);
	Check(ReadString(definition, L"name", L"", &nameText));
	if(!RStrLen(nameText))
	{
		nameText = static_cast<RSTRING>(NULL);
		Check(RStrFormatW(&nameText, L"%ls_%.8x", m_factory->m_type->name, m_id.Data1));
	}
	CheckIf(!RStrIsWide(static_cast<RSTRING>(nameText)) || RStrLen(nameText) > 256, E_INVALIDARG);
	m_name = nameText;
	if(m_parent)
	{
		TStackRef<IQuadooFrame> frame;
		Check(ValidateCandidate(m_parent, m_name, m_id, SUCCEEDED(Control()->QueryInterface(__uuidof(IQuadooFrame), reinterpret_cast<PVOID*>(&frame)))));
	}
	Check(m_context->GetDPI(&m_dpi)); m_autoDPI = m_dpi == 0;
	if(m_autoDPI)
	{
		HWND host;
		if(m_parent)
			m_dpi = ControlDPI(m_parent);
		else
		{
			Check(m_context->GetHostHandle(&host));
			m_dpi = SystemDPI(host);
		}
	}
	CheckIf(m_dpi < 48 || m_dpi > 768, E_INVALIDARG);
	Check(CloneObject(definition, &m_definition));
	Check(LoadProperties(definition));
	Check(CreateNative());
	if(m_parent) { Check(m_parent->RegisterControl(Control())); }
Cleanup:
	if(FAILED(hr))
	{
		m_abort = TRUE;
		Destroy();
	}
	return hr;
}

HRESULT CControl::LoadProperties(IJSONObject* definition)
{
	HRESULT hr;
	TStackRef<IJSONValue> json;
	TStackRef<IJSONObject> properties, events;
	QuadooForm::ContextMode mode;
	for(sysint n = 0; n < m_factory->m_specs.Length(); n++)
	{
		PropertySlot slot; TStackRef<IJSONValue> value;
		slot.spec = m_factory->m_specs[n]; slot.dirty = FALSE;
		Check(DefaultValue(slot.spec, &value)); slot.value = value;
		Check(m_values.Append(slot)); value.Detach();
	}
	for(sysint n = 0; n < m_factory->m_eventSpecs.Length(); n++)
	{
		Check(m_eventDirty.Append(FALSE));
	}
	Check(JSONCreateString(m_name, &json));
	Check(SetStoredValue(L"Name", json));
	json.Release();

	Check(m_context->GetMode(&mode));
	if(definition->HasField(RSTRING_CAST(L"properties")))
	{
		Check(definition->FindNonNullValueW(L"properties", &json)); Check(json->GetObject(&properties)); json.Release();
		for(sysint n = 0; n < properties->Count(); n++)
		{
			CRString propertyName; TStackRef<IJSONValue> value;
			Check(properties->GetValueName(n, &propertyName));
			UINT index = FindProperty(propertyName.c_wstr());
			if(index == static_cast<UINT>(m_values.Length()) && mode == QuadooForm::Design) continue;
			CheckIf(index == static_cast<UINT>(m_values.Length()), DISP_E_MEMBERNOTFOUND);
			CheckIf(SameString(propertyName, RSTRING_CAST(L"Name")) || (m_values[index].spec->flags & (QuadooForm::PropertyRuntimeOnly | QuadooForm::PropertyReadOnly)), E_INVALIDARG);
			Check(properties->GetValueByIndex(n, &json)); Check(NormalizeValue(m_values[index].spec, json, &value));
			SafeRelease(m_values[index].value); m_values[index].value = value.Detach(); json.Release();
		}
	}
	m_bounds.Left = Integer(L"Left"); m_bounds.Top = Integer(L"Top"); m_bounds.Width = Integer(L"Width"); m_bounds.Height = Integer(L"Height"); m_effective = m_bounds;
	CheckIf((Integer(L"MaxWidth") && Integer(L"MaxWidth") < Integer(L"MinWidth")) || (Integer(L"MaxHeight") && Integer(L"MaxHeight") < Integer(L"MinHeight")), E_INVALIDARG);
	if(definition->HasField(RSTRING_CAST(L"events")))
	{
		Check(definition->FindNonNullValueW(L"events", &json)); Check(json->GetObject(&events));
		for(sysint n = 0; n < events->Count(); n++)
		{
			CRString eventName, handler; TStackRef<IJSONValue> eventValue; sysint e;
			Check(events->GetValueName(n, &eventName)); Check(events->GetValueByIndex(n, &eventValue)); CheckIf(!eventValue, E_INVALIDARG);
			Check(eventValue->GetString(&handler)); CheckIf(!RStrIsWide(static_cast<RSTRING>(handler)), E_INVALIDARG);
			for(e = 0; e < m_factory->m_eventSpecs.Length(); e++) if(SameString(eventName, RSTRING_CAST(m_factory->m_eventSpecs[e]->name))) break;
			CheckIf(e == m_factory->m_eventSpecs.Length() && mode != QuadooForm::Design, DISP_E_MEMBERNOTFOUND);
		}
		Check(CloneObject(events, &m_eventNames));
	}
	else
	{
		Check(JSONCreateObject(&m_eventNames));
	}

Cleanup:
	return hr;
}

STDMETHODIMP CControl::GetID(GUID* id) { HRESULT hr = CheckThread(); if(SUCCEEDED(hr)) { if(id) *id = m_id; else hr = E_POINTER; } return hr; }

STDMETHODIMP CControl::GetName(RSTRING* name)
{
	HRESULT hr;
	CheckIf(!name, E_POINTER); *name = NULL;
	Check(CheckThread()); RStrSet(*name, m_name);
Cleanup:
	return hr;
}

STDMETHODIMP CControl::GetTypeName (RSTRING* name)
{
	HRESULT hr = CheckThread();
	if(SUCCEEDED(hr))
		hr = name ? RStrCreateW(TStrLenAssert(m_factory->m_type->name), m_factory->m_type->name, name) : E_POINTER;
	return hr;
}

STDMETHODIMP CControl::GetProperties (IQuadooProperties** properties)
{
	HRESULT hr;
	CheckIf(!properties, E_POINTER); *properties = NULL; Check(CheckThread());
	*properties = static_cast<IQuadooProperties*>(this); AddRef();
Cleanup:
	return hr;
}

STDMETHODIMP CControl::GetState(QuadooForm::ComponentState* state) { HRESULT hr = CheckThread(); if(SUCCEEDED(hr)) { if(state) *state = m_state; else hr = E_POINTER; } return hr; }

STDMETHODIMP CControl::GetParent(IQuadooControlContainer** parent)
{
	HRESULT hr;
	CheckIf(!parent, E_POINTER); *parent = NULL; Check(CheckThread());
	*parent = m_parent; if(*parent) (*parent)->AddRef();
Cleanup:
	return hr;
}

STDMETHODIMP CControl::GetBounds(QuadooForm::Rect* bounds) { HRESULT hr = CheckThread(); if(SUCCEEDED(hr)) { if(bounds) *bounds = m_bounds; else hr = E_POINTER; } return hr; }

STDMETHODIMP CControl::GetEffectiveBounds(QuadooForm::Rect* bounds) { HRESULT hr = CheckThread(); if(SUCCEEDED(hr)) { if(bounds) *bounds = m_effective; else hr = E_POINTER; } return hr; }

VOID CControl::ClearBindings()
{
	for(sysint n = 0; n < m_bindings.Length(); n++) SafeRelease(m_bindings[n].delegate);
	m_bindings.Clear();
}

STDMETHODIMP CControl::BindEvents(IQuadooConstructionContext* context)
{
	HRESULT hr;
	TStackRef<IQuadooEventsBinder> binder;
	QuadooForm::ContextMode mode;
	CRString bindingName;
	PCWSTR eventName = L"";
	Check(CheckAlive()); CheckIf(m_state != QuadooForm::Constructing, QuadooForm::EInvalidState);
	CheckIf(!context || !SameObject(context, m_context), E_INVALIDARG);
	Check(context->GetMode(&mode)); Check(context->GetBinder(&binder));
	m_state = QuadooForm::Binding;
	for(sysint n = 0; n < m_factory->m_eventSpecs.Length(); n++)
	{
		TStackRef<IQuadooDelegate> delegate;
		CRString name;
		Binding binding;
		Check(ReadString(m_eventNames, m_factory->m_eventSpecs[n]->name, L"", &name));
		bindingName = name; eventName = m_factory->m_eventSpecs[n]->name;
		if(mode == QuadooForm::Runtime && RStrLen(name))
		{
			CheckIf(!binder, QuadooForm::EHandlerNotFound);
			hr = binder->BindEvent(name, &delegate);
			CheckIf(hr == S_FALSE || (SUCCEEDED(hr) && !delegate), QuadooForm::EHandlerNotFound); Check(hr);
		}
		binding.spec = m_factory->m_eventSpecs[n]; binding.delegate = delegate;
		Check(m_bindings.Append(binding)); delegate.Detach();
	}
	hr = S_OK;
Cleanup:
	if(FAILED(hr))
	{
		CRString detail; if(SUCCEEDED(RStrFormatW(&detail, L"%ls: %ls handler '%ls' could not be bound", m_name.c_wstr(), eventName, bindingName.c_wstr()))) context->ReportError(hr, m_name, detail);
		ClearBindings();
	}
	return hr;
}

HRESULT CControl::Dispatch(PCWSTR name, BOOL* cancelled, LONGLONG value)
{
	HRESULT hr = S_OK;
	TStackRef<CControl> keepAlive(this);
	if(cancelled) *cancelled = FALSE;
	if(m_abort || m_state == QuadooForm::Constructing || m_state == QuadooForm::Binding || m_state == QuadooForm::Destroyed) goto Cleanup;
	for(sysint n = 0; n < m_bindings.Length(); n++)
		if(0 == TStrCmpAssert(m_bindings[n].spec->name, name) && m_bindings[n].delegate)
		{
			TStackRef<IQuadooDelegate> delegate(m_bindings[n].delegate);
			TStackRef<CEventArgs> args;
			args.Attach(__new CEventArgs(m_bindings[n].spec, Control())); CheckAlloc(args); Check(args->Initialize());
			if(m_bindings[n].spec->count && m_bindings[n].spec->args[0].type == QuadooForm::ValueInt) args->SetNativeValue(0, value);
			Check(delegate->Invoke(args));
			if(cancelled) *cancelled = args->Cancelled();
			break;
		}
Cleanup:
	return hr;
}

STDMETHODIMP CControl::InitializeControl()
{
	HRESULT hr;
	Check(CheckAlive()); CheckIf(m_state != QuadooForm::Binding, QuadooForm::EInvalidState);
	m_state = QuadooForm::Initializing;
	Check(Dispatch(L"OnCreate")); Check(CheckAlive());
	m_state = QuadooForm::Ready; m_updates = 0;
	hr = S_OK;
Cleanup:
	return hr;
}

STDMETHODIMP CControl::ActivateControl()
{
	HRESULT hr;
	Check(CheckAlive()); CheckIf(m_state != QuadooForm::Ready, QuadooForm::EInvalidState);
	Check(ApplyNative(L"Visible")); Check(CheckAlive()); Check(FlushNotifications());
Cleanup:
	return hr;
}

STDMETHODIMP CControl::AbortConstruction()
{
	HRESULT hr;
	Check(CheckThread()); m_abort = TRUE; Check(Destroy());
Cleanup:
	return hr;
}

STDMETHODIMP CControl::Destroy()
{
	HRESULT hr;
	TStackRef<CControl> keepAlive(this);
	Check(CheckThread());
	CheckIfIgnore(m_state >= QuadooForm::Destroying, S_FALSE);
	Check(BeforeDestroy());
	m_state = QuadooForm::Destroying;
	hr = Dispatch(L"OnDestroy");
	ReportCallback(hr, L"OnDestroy");
	{ HRESULT children = DestroyChildren(); if(FAILED(children) && SUCCEEDED(hr)) hr = children; }
	{ HRESULT native = DestroyNative(); if(FAILED(native) && SUCCEEDED(hr)) hr = native; }
	ClearBindings();
	for(sysint n = 0; n < m_observers.Length(); n++) m_observers[n]->Release(); m_observers.Clear();
	if(m_parent)
	{
		IQuadooControlContainer* parent = m_parent; m_parent = NULL;
		HRESULT unregisterResult = parent->UnregisterControl(Control());
		if(FAILED(unregisterResult) && unregisterResult != HRESULT_FROM_WIN32(ERROR_NOT_FOUND) && SUCCEEDED(hr)) hr = unregisterResult;
	}
	m_state = QuadooForm::Destroyed;
	SafeRelease(m_context);
	m_destroyedObservers.Notify(Control());
Cleanup:
	return hr;
}

STDMETHODIMP CControl::AddDestroyedObserver(IQuadooComponentDestroyedObserver* observer)
{
	HRESULT hr;
	Check(CheckThread());
	CheckIf(m_state >= QuadooForm::Destroying, QuadooForm::EDestroyed);
	Check(m_destroyedObservers.Add(observer));
Cleanup:
	return hr;
}

STDMETHODIMP CControl::RemoveDestroyedObserver(IQuadooComponentDestroyedObserver* observer)
{
	HRESULT hr;
	Check(CheckThread());
	Check(m_destroyedObservers.Remove(observer));
Cleanup:
	return hr;
}

HRESULT CControl::CreateNative() { return RecreateFont(); }
HRESULT CControl::DestroyNative() { return S_OK; }
HRESULT CControl::DestroyChildren() { return S_OK; }
HRESULT CControl::PropertyChanged(PCWSTR name) { return S_OK; }
// A staged observer is inert until the child entry is committed. Its owner is
// weak and detachable, so even a broken RemoveObserver cannot retain a parent.
class CChildObserver : public CBaseUnknown, public IQuadooPropertyObserver
{
	CContainer* m_owner;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooPropertyObserver)
	END_UNK_MAP

public:
	CChildObserver() : m_owner(NULL) {}
	VOID Activate(CContainer* owner) { m_owner = owner; }
	VOID Detach() { m_owner = NULL; }
	STDMETHOD(OnPropertyChanged)(IQuadooComponent* component, UINT index)
	{
		TStackRef<CContainer> owner(m_owner);
		return owner ? owner->OnPropertyChanged(component, index) : S_OK;
	}
	STDMETHOD(OnBoundsChanged)(IQuadooControl* control, const QuadooForm::Rect* bounds) { return S_OK; }
};
CContainer::CContainer(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) :
	CWinControl(factory, context, parent), m_names(RStrCompareRStr), m_layoutDirty(FALSE), m_registering(FALSE)
{
	m_viewportHeight = 0;
	ZeroMemory(&m_contentSize,sizeof(m_contentSize));
}
CContainer::~CContainer() { Assert(m_children.Length() == 0); }

STDMETHODIMP CContainer::GetChildCount (UINT* count)
{
	HRESULT hr = CheckThread();
	if(SUCCEEDED(hr))
	{
		if(count)
			*count = static_cast<UINT>(m_children.Length());
		else
			hr = E_POINTER;
	}
	return hr;
}

STDMETHODIMP CContainer::GetChild(UINT index, IQuadooControl** control)
{
	HRESULT hr;
	CheckIf(!control, E_POINTER); *control = NULL; Check(CheckThread());
	CheckIf(index >= static_cast<UINT>(m_children.Length()), HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
	*control = m_children[index].control; (*control)->AddRef();
Cleanup:
	return hr;
}

HRESULT CContainer::Reindex(IQuadooControl* substitute, RSTRING name)
{
	HRESULT hr;
	TRStrMap<IQuadooControl*> names(RStrCompareRStr);
	TArray<GUID> ids;

	Check(IndexTree(Control(), names, ids, substitute, name));
	m_names.Swap(names);

Cleanup:
	return hr;
}

STDMETHODIMP CContainer::FindControl(RSTRING name, IQuadooControl** control)
{
	HRESULT hr;
	CheckIf(!control, E_POINTER); *control = NULL; Check(CheckThread());
	CheckIf(!name || !RStrIsWide(name), E_INVALIDARG);
	Check(Reindex()); Check(m_names.Find(name, control)); (*control)->AddRef();
Cleanup:
	return hr;
}

STDMETHODIMP CContainer::RegisterControl(IQuadooControl* control)
{
	HRESULT hr;
	TStackRef<CContainer> keepAlive(this);
	TStackRef<IQuadooControl> candidate(control), namespaceControl;
	TStackRef<IUnknown> identity, parentIdentity, ownIdentity;
	TStackRef<IQuadooControlContainer> parent, namespaceRoot;
	TStackRef<IQuadooWinControl> window;
	TStackRef<IQuadooPropertyNotifications> notifications;
	TStackRef<CChildObserver> observer;
	TRStrMap<IQuadooControl*> names(RStrCompareRStr);
	TArray<GUID> ids;
	CRString name, verifiedName;
	GUID id, verifiedID;
	QuadooForm::ComponentState state;
	Child child;
	BOOL entered = FALSE, subscribed = FALSE;

	Check(CheckAlive()); CheckIf(!control, E_INVALIDARG); CheckIf(m_registering, QuadooForm::EInvalidState);
	m_registering = TRUE; entered = TRUE;
	Check(control->QueryInterface(IID_IUnknown, reinterpret_cast<PVOID*>(&identity))); CheckIf(!identity, E_UNEXPECTED);
	Check(Container()->QueryInterface(IID_IUnknown, reinterpret_cast<PVOID*>(&ownIdentity))); CheckIf(!ownIdentity, E_UNEXPECTED);
	CheckIf(static_cast<IUnknown*>(identity) == static_cast<IUnknown*>(ownIdentity), E_INVALIDARG);
	for(sysint n = 0; n < m_children.Length(); n++) { CheckIf(identity == m_children[n].identity, HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)); }
	Check(control->GetParent(&parent)); CheckIf(!parent, E_INVALIDARG);
	Check(parent->QueryInterface(IID_IUnknown, reinterpret_cast<PVOID*>(&parentIdentity))); CheckIf(!parentIdentity, E_UNEXPECTED);
	CheckIf(static_cast<IUnknown*>(parentIdentity) != static_cast<IUnknown*>(ownIdentity), E_INVALIDARG);
	Check(control->GetState(&state)); CheckIf(state >= QuadooForm::Destroying, QuadooForm::EDestroyed);
	Check(control->GetName(&name)); Check(control->GetID(&id));
	Check(ValidateControlMetadata(control));
	Check(GetNamespace(Container(), &namespaceRoot));
	Check(ValidateUniqueComponent(namespaceRoot,control));
	Check(namespaceRoot->QueryInterface(__uuidof(IQuadooControl), reinterpret_cast<PVOID*>(&namespaceControl))); CheckIf(!namespaceControl, E_UNEXPECTED);
	Check(IndexTree(namespaceControl, names, ids)); Check(IndexTree(control, names, ids, NULL, NULL, FALSE));
	for(sysint n = 0; n < names.Length(); n++)
	{
		IQuadooControl* member = *names.GetValuePtr(n); TStackRef<IQuadooFrame> frame; CRString memberName; GUID memberID;
		Check(member->GetName(&memberName)); Check(member->GetID(&memberID));
		BOOL alias = !SameObject(member,namespaceControl) && SUCCEEDED(member->QueryInterface(__uuidof(IQuadooFrame),reinterpret_cast<PVOID*>(&frame)));
		Check(ValidateVisualComponentIdentity(namespaceRoot,memberName,memberID,alias));
	}
	Check(QueryOptional(control, &window));
	if(window)
	{
		HWND hwnd, expected;
		TStackRef<IQuadooViewportDock> dock;
		Check(window->GetHandle(&hwnd));
		Check(QueryOptional(control,&dock));
		if(dock)
		{
			Check(GetHandle(&expected));
		}
		else
		{
			Check(GetChildHostHandle(&expected));
		}
		CheckIf(!hwnd || ::GetParent(hwnd) != expected, E_INVALIDARG);
	}
	Check(QueryOptional(control, &notifications));
	// Reserve before calling extension code. Append cannot allocate after this.
	Check(m_children.Reserve(1));
	if(notifications)
	{
		observer.Attach(__new CChildObserver); CheckAlloc(observer);
		subscribed = TRUE; // Also undo an implementation that subscribes then fails.
		hr = notifications->AddObserver(observer);
		CheckIf(hr != S_OK, FAILED(hr) ? hr : E_UNEXPECTED);
	}
	// Observer registration may call back into the candidate. Reject identity
	// changes or destruction before publishing it; reads still see the old tree.
	Check(control->GetState(&state)); CheckIf(state >= QuadooForm::Destroying, QuadooForm::EDestroyed);
	Check(control->GetName(&verifiedName)); Check(control->GetID(&verifiedID));
	CheckIf(!SameString(name, verifiedName) || !IsEqualGUID(id, verifiedID), QuadooForm::EInvalidState);
	parent.Release(); parentIdentity.Release();
	Check(control->GetParent(&parent)); CheckIf(!parent, E_INVALIDARG);
	Check(parent->QueryInterface(IID_IUnknown, reinterpret_cast<PVOID*>(&parentIdentity))); CheckIf(!parentIdentity, E_UNEXPECTED);
	CheckIf(static_cast<IUnknown*>(parentIdentity) != static_cast<IUnknown*>(ownIdentity), QuadooForm::EInvalidState);
	Check(ValidateControlMetadata(control));
	ZeroMemory(&child, sizeof(child)); child.control = candidate; child.identity = identity;
	child.notifications = notifications; child.observer = observer;
	Check(m_children.Append(child)); candidate.Detach(); notifications.Detach(); observer.Detach();
	if(child.observer) child.observer->Activate(this);
	m_names.Clear(); m_layoutDirty = TRUE;

Cleanup:
	if(FAILED(hr) && subscribed)
	{
		if(observer) observer->Detach();
		notifications->RemoveObserver(observer);
	}
	if(entered) m_registering = FALSE;
	return hr;
}

STDMETHODIMP CContainer::UnregisterControl(IQuadooControl* control)
{
	HRESULT hr;
	TStackRef<CContainer> keepAlive(this);
	TStackRef<IUnknown> identity;
	QuadooForm::ComponentState state;
	Check(CheckThread()); CheckIf(!control, E_INVALIDARG); CheckIf(m_registering, QuadooForm::EInvalidState);
	Check(control->GetState(&state)); CheckIf(state < QuadooForm::Destroying, QuadooForm::EInvalidState);
	Check(control->QueryInterface(IID_IUnknown, reinterpret_cast<PVOID*>(&identity))); CheckIf(!identity, E_UNEXPECTED);
	hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	for(sysint n = 0; n < m_children.Length(); n++)
		if(identity == m_children[n].identity)
		{
			Child child = m_children[n];
			if(child.observer) child.observer->Detach();
			m_children.Remove(n, NULL); m_names.Clear(); m_layoutDirty = TRUE;
			if(child.notifications) child.notifications->RemoveObserver(child.observer);
			SafeRelease(child.notifications); SafeRelease(child.observer); child.control->Release();
			hr = m_state < QuadooForm::Destroying ? RequestLayout() : S_OK; break;
		}
Cleanup:
	return hr;
}

HRESULT CContainer::DestroyChildren()
{
	HRESULT hr = S_OK;
	while(m_children.Length())
	{
		TStackRef<IQuadooControl> child(m_children[m_children.Length() - 1].control);
		HRESULT childResult = m_abort ? S_OK : child->Destroy();
		if(m_abort)
		{
			TStackRef<IQuadooConstructionParticipant> participant;
			if(SUCCEEDED(child->QueryInterface(__uuidof(IQuadooConstructionParticipant), reinterpret_cast<PVOID*>(&participant)))) childResult = participant->AbortConstruction();
			else childResult = child->Destroy();
		}
		if(FAILED(childResult) && SUCCEEDED(hr)) hr = childResult;
		// A third-party child may not unregister itself. Parent teardown still owns cleanup.
		if(m_children.Length() && m_children[m_children.Length() - 1].control == static_cast<IQuadooControl*>(child))
		{
			Child entry = m_children[m_children.Length() - 1];
			if(entry.observer) entry.observer->Detach();
			m_children.Remove(m_children.Length() - 1, NULL);
			if(entry.notifications) entry.notifications->RemoveObserver(entry.observer);
			SafeRelease(entry.notifications); SafeRelease(entry.observer); child->Release();
		}
	}
	m_names.Clear(); return hr;
}

STDMETHODIMP CContainer::OnPropertyChanged(IQuadooComponent* component, UINT index)
{
	HRESULT hr;
	TStackRef<IQuadooProperties> properties;
	TStackRef<IQuadooPropertyDescriptors> descriptors;
	TStackRef<IQuadooPropertyDescriptor> descriptor;
	DWORD flags;
	CRString name;
	TStackRef<IUnknown> identity;
	UINT count;
	BOOL registered = FALSE;
	Check(CheckAlive()); CheckIfIgnore(m_registering, S_FALSE); CheckIf(!component, E_INVALIDARG);
	Check(component->QueryInterface(IID_IUnknown, reinterpret_cast<PVOID*>(&identity))); CheckIf(!identity, E_UNEXPECTED);
	for(sysint n = 0; n < m_children.Length(); n++) if(identity == m_children[n].identity) { registered = TRUE; break; }
	CheckIf(!registered, E_INVALIDARG);
	Check(component->GetProperties(&properties)); CheckIf(!properties, QuadooForm::EInvalidDefinition);
	Check(properties->GetDescriptors(&descriptors)); CheckIf(!descriptors, QuadooForm::EInvalidDefinition);
	Check(descriptors->GetCount(&count)); CheckIf(index >= count || count > 4096, HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
	Check(descriptors->GetAt(index, &descriptor)); CheckIf(!descriptor, QuadooForm::EInvalidDefinition); Check(descriptor->GetFlags(&flags));
	Check(descriptor->GetName(&name));
	if(SameString(name, RSTRING_CAST(L"Left")) || SameString(name, RSTRING_CAST(L"Top")) ||
		SameString(name, RSTRING_CAST(L"Width")) || SameString(name, RSTRING_CAST(L"Height")) || SameString(name, RSTRING_CAST(L"Anchors")))
	{
		for(sysint n = 0; n < m_children.Length(); n++) if(SameObject(component, m_children[n].control)) m_children[n].anchored = FALSE;
	}
	if(flags & QuadooForm::PropertyAffectsLayout) { Check(RequestLayout()); }
Cleanup:
	return hr;
}
}
