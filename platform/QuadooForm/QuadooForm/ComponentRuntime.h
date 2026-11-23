#pragma once

namespace QF
{
// Optional internal test seam, never required of third-party factories. Native
// tests wrap the construction context to exercise modal results without opening
// an OS picker; production contexts use CChooseFile.
interface __declspec(uuid("BEBAC027-72BA-418D-938B-4A12F4512710")) IFileDialogDriver : IUnknown
{
	STDMETHOD(Choose)(HWND owner, BOOL open, RSTRING title, RSTRING filename, RSTRING filter,
		RSTRING directory, RSTRING extension, UINT index, DWORD options, RSTRING* selected, UINT* selectedIndex) = 0;
};
interface __declspec(uuid("BEBAC027-72BA-418D-938B-4A12F4512711")) ITimerStartCheck : IUnknown
{
	STDMETHOD(BeforeStart)(HWND window, UINT interval) = 0;
};
// Shared identity, storage and lifecycle for native nonvisual objects and design
// representations. Visual behavior is supplied only by the visual placeholder.
class CComponentObject : public CBaseUnknown, public IQuadooComponent, public IQuadooProperties,
	public IQuadooPropertyNotifications, public IQuadooConstructionParticipant, public IQuadooDefinitionSource,
	public IQuadooComponentLifecycle
{
protected:
	struct Slot { RSTRING name; QuadooProperty::Type type; DWORD flags; IJSONValue* value; BOOL edited; };
	struct Binding { RSTRING name; IQuadooDelegate* delegate; };
	DWORD m_thread;
	GUID m_id;
	CRString m_name, m_type;
	QuadooForm::ComponentState m_state;
	QuadooForm::ContextMode m_mode;
	IQuadooConstructionContext* m_context;
	IQuadooComponentCollection* m_owner; // Weak; root destruction calls Destroy.
	IUnknown* m_factory;
	UINT* m_live;
	IQuadooPropertyDescriptors* m_descriptors;
	IQuadooEventDescriptors* m_events;
	IJSONObject* m_definition;
	TArray<Slot> m_values;
	TArray<Binding> m_bindings;
	TArray<IQuadooPropertyObserver*> m_observers;
	CDestroyedObservers m_destroyedObservers;
	BOOL m_abort, m_notifying;
	HRESULT CheckThread() const;
	HRESULT CheckAlive() const;
	UINT Find(PCWSTR name) const;
	INT Integer(PCWSTR name) const;
	BOOL Boolean(PCWSTR name) const;
	HRESULT String(PCWSTR name, RSTRING* value);
	HRESULT SetNamed(PCWSTR name, IJSONValue* value);
	HRESULT TypedValue(UINT index, QuadooProperty::Type type, IJSONValue** value);
	HRESULT SetTyped(UINT index, QuadooProperty::Type type, IJSONValue* value);
	HRESULT Notify(UINT index);
	HRESULT Dispatch(PCWSTR event);
	VOID ClearBindings();
	virtual HRESULT ValidateProperty(PCWSTR name, IJSONValue* value) { return S_OK; }
	virtual HRESULT ChangedProperty(PCWSTR name) { return S_OK; }
	virtual HRESULT ValidateName(RSTRING name);
	virtual HRESULT Activated() { return S_OK; }
	virtual HRESULT DestroyObject() { return S_OK; }
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooComponent)
		UNK_INTERFACE(IQuadooProperties)
		UNK_INTERFACE(IQuadooPropertyNotifications)
		UNK_INTERFACE(IQuadooComponentLifecycle)
		UNK_INTERFACE(IQuadooConstructionParticipant)
		UNK_INTERFACE(IQuadooDefinitionSource)
	END_UNK_MAP
	CComponentObject(IQuadooConstructionContext* context, IQuadooComponentCollection* owner, IUnknown* factory, UINT* live = NULL);
	virtual ~CComponentObject();
	HRESULT Construct(IJSONObject* definition, IQuadooPropertyDescriptors* properties, IQuadooEventDescriptors* events);
	STDMETHOD(GetID)(GUID* id);
	STDMETHOD(GetName)(RSTRING* name);
	STDMETHOD(SetName)(RSTRING name);
	STDMETHOD(GetTypeName)(RSTRING* name);
	STDMETHOD(GetProperties)(IQuadooProperties** properties);
	STDMETHOD(GetState)(QuadooForm::ComponentState* state);
	STDMETHOD(Destroy)(VOID);
	STDMETHOD(GetDefinitionTemplate)(IJSONObject** definition);
	STDMETHOD(GetDescriptors)(IQuadooPropertyDescriptors** descriptors);
	STDMETHOD(GetValueInt)(UINT index, INT* value);
	STDMETHOD(GetValueLong)(UINT index, LONGLONG* value);
	STDMETHOD(GetValueString)(UINT index, RSTRING* value);
	STDMETHOD(GetValueBool)(UINT index, BOOL* value);
	STDMETHOD(GetValueFloat)(UINT index, DOUBLE* value);
	STDMETHOD(GetValueEvent)(UINT index, RSTRING* value);
	STDMETHOD(GetValueJSON)(UINT index, IJSONValue** value);
	STDMETHOD(SetValueInt)(UINT index, INT value);
	STDMETHOD(SetValueLong)(UINT index, LONGLONG value);
	STDMETHOD(SetValueString)(UINT index, RSTRING value);
	STDMETHOD(SetValueBool)(UINT index, BOOL value);
	STDMETHOD(SetValueFloat)(UINT index, DOUBLE value);
	STDMETHOD(SetValueEvent)(UINT index, RSTRING value);
	STDMETHOD(SetValueJSON)(UINT index, IJSONValue* value);
	STDMETHOD(AddObserver)(IQuadooPropertyObserver* observer);
	STDMETHOD(RemoveObserver)(IQuadooPropertyObserver* observer);
	STDMETHOD(AddDestroyedObserver)(IQuadooComponentDestroyedObserver* observer);
	STDMETHOD(RemoveDestroyedObserver)(IQuadooComponentDestroyedObserver* observer);
	STDMETHOD(BindEvents)(IQuadooConstructionContext* context);
	STDMETHOD(InitializeControl)(VOID);
	STDMETHOD(ActivateControl)(VOID);
	STDMETHOD(AbortConstruction)(VOID);
};
extern const TypeSpec NonvisualType;
class CNonvisual : public CComponentObject, public IQuadooNonvisualComponent
{
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooNonvisualComponent)
		UNK_CHAIN_TO(CComponentObject)
	END_UNK_MAP
	CNonvisual(IQuadooConstructionContext* context, IQuadooComponentCollection* owner, IUnknown* factory, UINT* live) : CComponentObject(context,owner,factory,live) {}
	STDMETHOD(GetOwner)(IQuadooComponentCollection** owner)
	{
		HRESULT hr;
		CheckIf(!owner,E_POINTER); *owner = NULL;
		Check(CheckThread()); *owner = m_owner; if(*owner) (*owner)->AddRef();
	Cleanup:
		return hr;
	}
};
template<class T> class TComponentFactory : public CBaseUnknown, public IQuadooComponentFactory, public IQuadooFactoryLifetime, public IQuadooPaletteIcon
{
	DWORD m_thread;
	UINT m_live;
	IQuadooPropertyDescriptors* m_properties;
	IQuadooEventDescriptors* m_events;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooComponentFactory)
		UNK_INTERFACE(IQuadooFactoryLifetime)
		UNK_INTERFACE(IQuadooPaletteIcon)
	END_UNK_MAP
	TComponentFactory() : m_thread(GetCurrentThreadId()), m_live(0), m_properties(NULL), m_events(NULL) {}
	~TComponentFactory() { SafeRelease(m_properties); SafeRelease(m_events); }
	HRESULT Initialize()
	{
		HRESULT hr; TArray<const PropertySpec*> properties; TArray<const EventSpec*> events;
		Check(ComposeMetadata(T::Type(),properties,events,FALSE));
		Check(CreateDescriptors(properties,events,&m_properties));
		Check(CreateEventDescriptors(events,&m_events));
		Check(ValidateDescriptors(m_properties,m_events,FALSE));
	Cleanup:
		return hr;
	}
	STDMETHOD(GetPaletteIcon)(UINT cx, UINT cy, HICON* icon)
	{
		HRESULT hr;
		CheckIf(!icon,E_POINTER);
		*icon = NULL;
		CheckIf(m_thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
		Check(LoadPaletteResource(T::Type()->paletteIcon,cx,cy,icon));
	Cleanup:
		return hr;
	}
	STDMETHOD(GetTypeName)(RSTRING* name)
	{
		HRESULT hr; CheckIf(!name,E_POINTER); *name = NULL;
		CheckIf(m_thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
		Check(RStrCreateW(TStrLenAssert(T::Type()->name),T::Type()->name,name));
	Cleanup:
		return hr;
	}
	STDMETHOD(GetProperties)(IQuadooPropertyDescriptors** properties)
	{
		HRESULT hr = S_OK; CheckIf(!properties,E_POINTER); *properties = NULL;
		CheckIf(m_thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
		*properties = m_properties; if(*properties) (*properties)->AddRef();
	Cleanup:
		return hr;
	}
	STDMETHOD(GetEvents)(IQuadooEventDescriptors** events)
	{
		HRESULT hr = S_OK; CheckIf(!events,E_POINTER); *events = NULL;
		CheckIf(m_thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
		*events = m_events; if(*events) (*events)->AddRef();
	Cleanup:
		return hr;
	}
	STDMETHOD(GetLiveCount)(UINT* count) { if(!count) return E_POINTER; *count = 0; if(m_thread != GetCurrentThreadId()) return QuadooForm::EWrongThread; *count = m_live; return S_OK; }
	STDMETHOD(CreateComponent)(IQuadooConstructionContext* context, IQuadooComponentCollection* owner, IJSONObject* definition, IQuadooComponent** component)
	{
		HRESULT hr; TStackRef<T> object; CRString type;
		CheckIf(!component,E_POINTER); *component = NULL;
		CheckIf(!context || !owner || !definition,E_INVALIDARG);
		CheckIf(m_thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
		Check(ReadString(definition,L"type",L"",&type));
		CheckIf(!SameString(type,RSTRING_CAST(T::Type()->name)),QuadooForm::EInvalidDefinition);
		object.Attach(__new T(context,owner,static_cast<IQuadooComponentFactory*>(this),&m_live)); CheckAlloc(object);
		Check(object->Construct(definition,m_properties,m_events));
		Check(owner->RegisterComponent(object));
		*component = object.Detach();
	Cleanup:
		if(FAILED(hr) && object) object->AbortConstruction();
		return hr;
	}
};
}
