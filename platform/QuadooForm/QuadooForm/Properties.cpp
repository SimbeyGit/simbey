#include "Runtime.h"

namespace QF
{
HRESULT CControl::Value(UINT index, QuadooProperty::Type type, IJSONValue** value)
{
	HRESULT hr;
	UINT property;
	const EventSpec* event;
	CheckIf(!value, E_POINTER); *value = NULL;
	Check(CheckThread()); Check(ResolvePropertyIndex(index, &property, &event));
	CheckIf(event, DISP_E_TYPEMISMATCH);
	CheckIf(type != m_values[property].spec->type && !(type == QuadooProperty::String && m_values[property].spec->type == QuadooProperty::Memo) && !(type == QuadooProperty::Int &&
		(m_values[property].spec->type == QuadooProperty::Enum || m_values[property].spec->type == QuadooProperty::Flags || m_values[property].spec->type == QuadooProperty::Color)), DISP_E_TYPEMISMATCH);
	*value = m_values[property].value; (*value)->AddRef();
Cleanup:
	return hr;
}

STDMETHODIMP CControl::GetDescriptors(IQuadooPropertyDescriptors** descriptors)
{
	HRESULT hr;
	CheckIf(!descriptors, E_POINTER); *descriptors = NULL; Check(CheckThread());
	Check(m_factory->GetProperties(descriptors));
Cleanup:
	return hr;
}

HRESULT CControl::SetValue(UINT property, QuadooProperty::Type type, IJSONValue* value)
{
	HRESULT hr;
	TStackRef<IJSONValue> normalized, old;
	TStackRef<CControl> keepAlive(this);
	Check(CheckAlive()); CheckIf(property >= static_cast<UINT>(m_values.Length()), DISP_E_MEMBERNOTFOUND);
	PCWSTR name = m_values[property].spec->name;
	CheckIf(m_values[property].spec->flags & QuadooForm::PropertyReadOnly,E_ACCESSDENIED);
	CheckIf(type != m_values[property].spec->type && !(type == QuadooProperty::String && m_values[property].spec->type == QuadooProperty::Memo) && !(type == QuadooProperty::Int &&
		(m_values[property].spec->type == QuadooProperty::Enum || m_values[property].spec->type == QuadooProperty::Flags || m_values[property].spec->type == QuadooProperty::Color)), DISP_E_TYPEMISMATCH);
	Check(NormalizeValue(m_values[property].spec, value, &normalized));
	Check(ValidateMenuReference(Control(),name,normalized,m_state >= QuadooForm::Binding));
	if(0 == TStrCmpAssert(name, L"Name"))
	{
		CRString name;
		Check(normalized->GetString(&name));
		Check(SetName(name));
		goto Cleanup;
	}
	if((0 == TStrCmpAssert(name,L"MinWidth") || 0 == TStrCmpAssert(name,L"MinHeight") || 0 == TStrCmpAssert(name,L"MaxWidth") || 0 == TStrCmpAssert(name,L"MaxHeight")))
	{
		INT number, minW = Integer(L"MinWidth"), minH = Integer(L"MinHeight"), maxW = Integer(L"MaxWidth"), maxH = Integer(L"MaxHeight");
		Check(normalized->GetInteger(&number));
		if(0 == TStrCmpAssert(name, L"MinWidth")) minW = number; else if(0 == TStrCmpAssert(name, L"MinHeight")) minH = number;
		else if(0 == TStrCmpAssert(name, L"MaxWidth")) maxW = number; else maxH = number;
		CheckIf((maxW && maxW < minW) || (maxH && maxH < minH), E_INVALIDARG);
	}
	if(0 == TStrCmpAssert(name, L"Align") && m_parent)
	{
		INT number; Check(normalized->GetInteger(&number));
		if(number == QuadooForm::AlignClient)
		{
			UINT count; Check(m_parent->GetChildCount(&count));
			for(UINT n = 0; n < count; n++)
			{
				TStackRef<IQuadooControl> sibling; Check(m_parent->GetChild(n, &sibling));
				CheckIf(!SameObject(sibling, Control()) && LayoutInt(sibling, L"Align", 0) == QuadooForm::AlignClient, E_INVALIDARG);
			}
		}
	}
	if(0 == JSONCompare(m_values[property].value, normalized)) { hr = S_FALSE; goto Cleanup; }
	old.Attach(m_values[property].value); m_values[property].value = normalized.Detach();
	if(0 == TStrCmpAssert(name, L"Left")) m_bounds.Left = Integer(L"Left"); if(0 == TStrCmpAssert(name, L"Top")) m_bounds.Top = Integer(L"Top");
	if(0 == TStrCmpAssert(name, L"Width")) m_bounds.Width = Integer(L"Width"); if(0 == TStrCmpAssert(name, L"Height")) m_bounds.Height = Integer(L"Height");
	hr = ApplyNative(name);
	if(FAILED(hr))
	{
		SafeRelease(m_values[property].value); m_values[property].value = old.Detach();
		m_bounds.Left = Integer(L"Left"); m_bounds.Top = Integer(L"Top"); m_bounds.Width = Integer(L"Width"); m_bounds.Height = Integer(L"Height");
		goto Cleanup;
	}
	Check(CheckAlive());
	m_values[property].dirty = TRUE;
	if(!m_parent && (0 == TStrCmpAssert(name,L"Left") || 0 == TStrCmpAssert(name,L"Top") || 0 == TStrCmpAssert(name,L"Width") || 0 == TStrCmpAssert(name,L"Height") || 0 == TStrCmpAssert(name,L"MinWidth") || 0 == TStrCmpAssert(name,L"MinHeight") || 0 == TStrCmpAssert(name,L"MaxWidth") || 0 == TStrCmpAssert(name,L"MaxHeight")))
	{
		m_updates++; hr = Arrange(&m_bounds, m_dpi); m_updates--; Check(hr);
	}
	if(m_values[property].spec->flags & QuadooForm::PropertyAffectsLayout) { Check(RequestLayout()); }
	Check(FlushNotifications());
	Check(PropertyChanged(name));
	hr = S_OK;
Cleanup:
	return hr;
}

HRESULT CControl::SetNamedValue(PCWSTR name, QuadooProperty::Type type, IJSONValue* value)
{
	HRESULT hr;
	Check(CheckAlive()); Check(SetValue(FindProperty(name), type, value));
Cleanup:
	return hr;
}

STDMETHODIMP CControl::SetName(RSTRING name)
{
	HRESULT hr;
	TStackRef<IJSONValue> value;
	Check(CheckAlive()); CheckIf(!name || !RStrIsWide(name) || RStrLen(name) == 0 || RStrLen(name) > 256, E_INVALIDARG);
	CheckIfIgnore(SameString(name, m_name), S_FALSE);
	Check(JSONCreateString(name, &value));
	Check(QuadooValidateControlName(Control(), name));
	Check(SetStoredValue(L"Name", value)); m_name = name;
	Check(QueueChange(L"Name"));
Cleanup:
	return hr;
}

STDMETHODIMP CControl::SetBounds(const QuadooForm::Rect* bounds)
{
	HRESULT hr;
	TStackRef<IJSONValue> values[4];
	UINT indexes[4];
	const INT* fields = reinterpret_cast<const INT*>(bounds);
	static PCWSTR const names[] = { L"Left", L"Top", L"Width", L"Height" };
	Check(CheckAlive()); CheckIf(!bounds, E_POINTER);
	for(UINT n = 0; n < 4; n++)
	{
		TStackRef<IJSONValue> input;
		indexes[n] = FindProperty(names[n]);
		CheckIf(indexes[n] >= static_cast<UINT>(m_values.Length()), DISP_E_MEMBERNOTFOUND);
		Check(JSONCreateInteger(fields[n], &input));
		Check(NormalizeValue(m_values[indexes[n]].spec, input, &values[n]));
	}
	CheckIfIgnore(0 == memcmp(&m_bounds, bounds, sizeof(m_bounds)), S_FALSE);
	for(UINT n = 0; n < 4; n++)
	{
		PropertySlot& slot = m_values[indexes[n]];
		if(0 != JSONCompare(slot.value, values[n])) slot.dirty = TRUE;
		SafeRelease(slot.value); slot.value = values[n].Detach();
	}
	m_bounds = *bounds;
	if(!m_parent) { Check(Arrange(&m_bounds, m_dpi)); }
	Check(RequestLayout()); Check(FlushNotifications());
Cleanup:
	return hr;
}

STDMETHODIMP CControl::GetValueJSON(UINT index, IJSONValue** value)
{
	HRESULT hr;
	UINT property;
	const EventSpec* event;
	CheckIf(!value, E_POINTER); *value = NULL; Check(CheckThread());
	Check(ResolvePropertyIndex(index, &property, &event));
	if(event)
	{ CRString handler; Check(ReadString(m_eventNames, event->name, L"", &handler)); Check(JSONCreateString(handler, value)); }
	else { Check(JSONClone(m_values[property].value, value, TRUE)); }
Cleanup:
	return hr;
}

STDMETHODIMP CControl::SetValueJSON(UINT index, IJSONValue* value)
{
	HRESULT hr;
	UINT property;
	const EventSpec* event;
	Check(CheckAlive());
	Check(ResolvePropertyIndex(index, &property, &event));
	if(event) { CRString handler; CheckIf(!value, E_INVALIDARG); Check(value->GetString(&handler)); Check(SetValueEvent(index, handler)); }
	else { Check(SetValue(property, m_values[property].spec->type, value)); }
Cleanup:
	return hr;
}

STDMETHODIMP CControl::GetValueEvent(UINT index, RSTRING* value)
{
	HRESULT hr;
	UINT property;
	const EventSpec* event;
	CheckIf(!value, E_POINTER); *value = NULL; Check(CheckThread());
	Check(ResolvePropertyIndex(index, &property, &event)); CheckIf(!event, DISP_E_TYPEMISMATCH);
	Check(ReadString(m_eventNames, event->name, L"", value));
Cleanup:
	return hr;
}

STDMETHODIMP CControl::SetValueEvent(UINT index, RSTRING value)
{
	HRESULT hr;
	UINT property;
	const EventSpec* event;
	TStackRef<IQuadooEventsBinder> binder;
	TStackRef<IQuadooDelegate> delegate;
	QuadooForm::ContextMode mode;
	Check(CheckAlive()); CheckIf(!value || !RStrIsWide(value), E_INVALIDARG);
	Check(ResolvePropertyIndex(index, &property, &event)); CheckIf(!event, DISP_E_TYPEMISMATCH);
	Check(m_context->GetMode(&mode));
	if(m_state != QuadooForm::Constructing && mode == QuadooForm::Runtime && RStrLen(value))
	{
		Check(m_context->GetBinder(&binder)); CheckIf(!binder, QuadooForm::EHandlerNotFound);
		hr = binder->BindEvent(value, &delegate); CheckIf(hr == S_FALSE || (SUCCEEDED(hr) && !delegate), QuadooForm::EHandlerNotFound); Check(hr);
	}
	Check(PutString(m_eventNames, event->name, value));
	for(sysint n = 0; n < m_bindings.Length(); n++)
		if(m_bindings[n].spec == event) { SafeRelease(m_bindings[n].delegate); m_bindings[n].delegate = delegate.Detach(); break; }
	m_eventDirty[index - static_cast<UINT>(m_values.Length())] = TRUE;
	Check(FlushNotifications());
Cleanup:
	return hr;
}

STDMETHODIMP CControl::AddObserver(IQuadooPropertyObserver* observer)
{
	HRESULT hr;

	Check(CheckAlive());
	CheckIf(!observer, E_INVALIDARG);
	for(sysint n = 0; n < m_observers.Length(); n++)
	{
		CheckIfIgnore(SameObject(observer, m_observers[n]), S_FALSE);
	}
	Check(m_observers.Append(observer));
	observer->AddRef();
Cleanup:
	return hr;
}

STDMETHODIMP CControl::RemoveObserver(IQuadooPropertyObserver* observer)
{
	HRESULT hr = CheckThread();
	Check(hr); CheckIf(!observer, E_INVALIDARG);
	hr = S_FALSE;
	for(sysint n = 0; n < m_observers.Length(); n++)
		if(SameObject(observer, m_observers[n])) { IQuadooPropertyObserver* old = m_observers[n]; m_observers.Remove(n, NULL); old->Release(); hr = S_OK; break; }
Cleanup:
	return hr;
}

STDMETHODIMP CControl::IsUpdatePending(BOOL* pending)
{
	HRESULT hr = CheckThread();
	if(SUCCEEDED(hr))
	{
		if(pending)
			*pending = m_updates || m_layoutBusy || m_state < QuadooForm::Ready;
		else
			hr = E_POINTER;
	}
	return hr;
}

BOOL CControl::Updating(BOOL includeLayout) const
{
	if(m_updates || (includeLayout && m_layoutBusy) || m_state < QuadooForm::Ready) return TRUE;
	TStackRef<IQuadooControlContainer> current(m_parent);
	for(UINT depth = 0; current && depth < 256; depth++)
	{
		TStackRef<IUpdateParticipant> participant;
		BOOL pending = FALSE;
		if(SUCCEEDED(current->QueryInterface(__uuidof(IUpdateParticipant), reinterpret_cast<PVOID*>(&participant))))
		{
			HRESULT hr = includeLayout ? participant->IsUpdatePending(&pending) : participant->IsBatchPending(&pending);
			if(SUCCEEDED(hr) && pending) return TRUE;
		}
		TStackRef<IQuadooControlContainer> parent;
		if(FAILED(current->GetParent(&parent))) break;
		current = parent;
	}
	return FALSE;
}
HRESULT CControl::QueueChange(PCWSTR name)
{
	HRESULT hr;
	Check(Dirty(name)); Check(FlushNotifications());
Cleanup:
	return hr;
}

HRESULT CControl::FlushNotifications()
{
	HRESULT hr = S_OK;
	TStackRef<CControl> keepAlive(this);
	TArray<IQuadooPropertyObserver*> observers;
	BOOL entered = FALSE;
	if(m_notifying || Updating() || m_state >= QuadooForm::Destroying) goto Cleanup;
	m_notifying = TRUE; entered = TRUE;
	for(sysint n = 0; n < m_observers.Length(); n++)
	{ Check(observers.Append(m_observers[n])); m_observers[n]->AddRef(); }
	for(UINT pass = 0; pass < 64; pass++)
	{
		BOOL pending = FALSE;
		UINT index = 0;
		for(sysint p = 0; p < m_values.Length(); p++)
		{
			if(m_values[p].dirty)
			{
				pending = TRUE; m_values[p].dirty = FALSE;
				for(sysint n = 0; n < observers.Length(); n++)
					for(sysint o = 0; o < m_observers.Length(); o++) if(SameObject(observers[n], m_observers[o]))
					{ HRESULT notify = observers[n]->OnPropertyChanged(Control(), index); if(FAILED(notify) && SUCCEEDED(hr)) hr = notify; break; }
			}
			index++;
		}
		for(sysint e = 0; e < m_eventDirty.Length(); e++)
		{
			if(m_eventDirty[e])
			{
				pending = TRUE; m_eventDirty[e] = FALSE;
				for(sysint n = 0; n < observers.Length(); n++)
					for(sysint o = 0; o < m_observers.Length(); o++) if(SameObject(observers[n], m_observers[o]))
					{ HRESULT notify = observers[n]->OnPropertyChanged(Control(), index); if(FAILED(notify) && SUCCEEDED(hr)) hr = notify; break; }
			}
			index++;
		}
		if(m_boundsDirty)
		{
			QuadooForm::Rect bounds = m_effective; pending = TRUE; m_boundsDirty = FALSE;
			for(sysint n = 0; n < observers.Length(); n++)
				for(sysint o = 0; o < m_observers.Length(); o++) if(SameObject(observers[n], m_observers[o]))
				{ HRESULT notify = observers[n]->OnBoundsChanged(Control(), &bounds); if(FAILED(notify) && SUCCEEDED(hr)) hr = notify; break; }
		}
		if(!pending || m_state >= QuadooForm::Destroying) break;
		if(pass == 63 && SUCCEEDED(hr)) hr = E_UNEXPECTED;
	}
Cleanup:
	for(sysint n = 0; n < observers.Length(); n++) observers[n]->Release();
	if(entered) m_notifying = FALSE;
	return hr;
}

STDMETHODIMP CContainer::BeginUpdate()
{
	HRESULT hr;
	Check(CheckAlive()); CheckIf(m_registering, QuadooForm::EInvalidState); CheckIf(m_updates == UINT_MAX, E_UNEXPECTED); m_updates++;
Cleanup:
	return hr;
}

STDMETHODIMP CContainer::EndUpdate()
{
	HRESULT hr;
	Check(CheckAlive()); CheckIf(m_registering, QuadooForm::EInvalidState); CheckIf(!m_updates, QuadooForm::EInvalidState); m_updates--;
	if(!m_updates)
	{
		Check(RequestLayout()); Check(FlushNotifications());
		Check(FlushTree(Control()));
	}
Cleanup:
	return hr;
}

HRESULT FlushTree(IQuadooControl* control)
{
	HRESULT hr = S_OK;
	TStackRef<IUpdateParticipant> pending;
	TStackRef<IQuadooControlContainer> container;
	if(SUCCEEDED(control->QueryInterface(__uuidof(IUpdateParticipant), reinterpret_cast<PVOID*>(&pending)))) { Check(pending->FlushPending()); }
	if(SUCCEEDED(control->QueryInterface(__uuidof(IQuadooControlContainer), reinterpret_cast<PVOID*>(&container))))
	{
		UINT count; Check(container->GetChildCount(&count));
		for(UINT n = 0; n < count; n++) { TStackRef<IQuadooControl> child; Check(container->GetChild(n, &child)); Check(FlushTree(child)); }
	}
Cleanup:
	return hr;
}

STDMETHODIMP CControl::GetValueInt(UINT index, INT* value)
{
	HRESULT hr;
	TStackRef<IJSONValue> json;
	CheckIf(!value, E_POINTER); *value = 0;
	Check(Value(index, QuadooProperty::Int, &json));
	Check(json->GetInteger(value));
Cleanup:
	return hr;
}

STDMETHODIMP CControl::SetValueInt(UINT index, INT value)
{
	HRESULT hr;
	TStackRef<IJSONValue> json;
	UINT property; const EventSpec* event;
	Check(CheckAlive());
	Check(ResolvePropertyIndex(index, &property, &event)); CheckIf(event, DISP_E_TYPEMISMATCH);
	Check(JSONCreateInteger(value, &json));
	Check(SetValue(property, QuadooProperty::Int, json));
Cleanup:
	return hr;
}

STDMETHODIMP CControl::GetValueLong(UINT index, LONGLONG* value)
{
	HRESULT hr;
	TStackRef<IJSONValue> json;
	CheckIf(!value, E_POINTER); *value = 0;
	Check(Value(index, QuadooProperty::Long, &json));
	Check(json->GetLongInteger(value));
Cleanup:
	return hr;
}

STDMETHODIMP CControl::SetValueLong(UINT index, LONGLONG value)
{
	HRESULT hr;
	TStackRef<IJSONValue> json;
	UINT property; const EventSpec* event;
	Check(CheckAlive());
	Check(ResolvePropertyIndex(index, &property, &event)); CheckIf(event, DISP_E_TYPEMISMATCH);
	Check(JSONCreateLongInteger(value, &json));
	Check(SetValue(property, QuadooProperty::Long, json));
Cleanup:
	return hr;
}

STDMETHODIMP CControl::GetValueBool(UINT index, BOOL* value)
{
	HRESULT hr;
	TStackRef<IJSONValue> json;
	bool boolean;
	CheckIf(!value, E_POINTER); *value = 0;
	Check(Value(index, QuadooProperty::Boolean, &json));
	Check(json->GetBoolean(&boolean));
	*value = boolean;
Cleanup:
	return hr;
}

STDMETHODIMP CControl::SetValueBool(UINT index, BOOL value)
{
	HRESULT hr;
	TStackRef<IJSONValue> json;
	UINT property; const EventSpec* event;
	Check(CheckAlive());
	Check(ResolvePropertyIndex(index, &property, &event)); CheckIf(event, DISP_E_TYPEMISMATCH);
	Check(JSONCreateBool(value != 0, &json));
	Check(SetValue(property, QuadooProperty::Boolean, json));
Cleanup:
	return hr;
}

STDMETHODIMP CControl::GetValueFloat(UINT index, DOUBLE* value)
{
	HRESULT hr;
	TStackRef<IJSONValue> json;
	CheckIf(!value, E_POINTER); *value = 0;
	Check(Value(index, QuadooProperty::Float, &json));
	Check(json->GetDouble(value));
Cleanup:
	return hr;
}

STDMETHODIMP CControl::SetValueFloat(UINT index, DOUBLE value)
{
	HRESULT hr;
	TStackRef<IJSONValue> json;
	UINT property; const EventSpec* event;
	Check(CheckAlive());
	Check(ResolvePropertyIndex(index, &property, &event)); CheckIf(event, DISP_E_TYPEMISMATCH);
	Check(JSONCreateDouble(value, &json));
	Check(SetValue(property, QuadooProperty::Float, json));
Cleanup:
	return hr;
}

STDMETHODIMP CControl::GetValueString(UINT index, RSTRING* value)
{
	HRESULT hr;
	TStackRef<IJSONValue> json;
	CheckIf(!value, E_POINTER); *value = NULL;
	Check(Value(index, QuadooProperty::String, &json));
	Check(json->GetString(value));
Cleanup:
	return hr;
}

STDMETHODIMP CControl::SetValueString(UINT index, RSTRING value)
{
	HRESULT hr;
	TStackRef<IJSONValue> json;
	UINT property; const EventSpec* event;
	Check(CheckAlive());
	Check(ResolvePropertyIndex(index, &property, &event)); CheckIf(event, DISP_E_TYPEMISMATCH);
	Check(JSONCreateString(value, &json));
	Check(SetValue(property, QuadooProperty::String, json));
Cleanup:
	return hr;
}

STDMETHODIMP CControl::GetVisible(BOOL* value)
{
	HRESULT hr = CheckThread();
	if(SUCCEEDED(hr)) { if(value) *value = Boolean(L"Visible"); else hr = E_POINTER; }
	return hr;
}

STDMETHODIMP CControl::SetVisible(BOOL value)
{
	HRESULT hr;
	TStackRef<IJSONValue> json;
	Check(JSONCreateBool(value != 0, &json));
	Check(SetNamedValue(L"Visible", QuadooProperty::Boolean, json));
Cleanup:
	return hr;
}

STDMETHODIMP CControl::GetEnabled(BOOL* value)
{
	HRESULT hr = CheckThread();
	if(SUCCEEDED(hr)) { if(value) *value = Boolean(L"Enabled"); else hr = E_POINTER; }
	return hr;
}

STDMETHODIMP CControl::SetEnabled(BOOL value)
{
	HRESULT hr;
	TStackRef<IJSONValue> json;
	Check(JSONCreateBool(value != 0, &json));
	Check(SetNamedValue(L"Enabled", QuadooProperty::Boolean, json));
Cleanup:
	return hr;
}

STDMETHODIMP CWinControl::GetTabOrder(INT* value)
{
	HRESULT hr = CheckThread();
	if(SUCCEEDED(hr)) { if(value) *value = Integer(L"TabOrder"); else hr = E_POINTER; }
	return hr;
}

STDMETHODIMP CWinControl::SetTabOrder(INT value)
{
	HRESULT hr;
	TStackRef<IJSONValue> json;
	Check(JSONCreateInteger(value, &json));
	Check(SetNamedValue(L"TabOrder", QuadooProperty::Int, json));
Cleanup:
	return hr;
}

STDMETHODIMP CScrollingContainer::GetAutoScroll(BOOL* value)
{
	HRESULT hr = CheckThread();
	if(SUCCEEDED(hr)) { if(value) *value = Boolean(L"AutoScroll"); else hr = E_POINTER; }
	return hr;
}

STDMETHODIMP CScrollingContainer::SetAutoScroll(BOOL value)
{
	HRESULT hr;
	TStackRef<IJSONValue> json;
	Check(JSONCreateBool(value != 0, &json));
	Check(SetNamedValue(L"AutoScroll", QuadooProperty::Boolean, json));
Cleanup:
	return hr;
}
}
