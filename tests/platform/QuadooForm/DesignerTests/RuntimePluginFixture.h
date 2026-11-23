// Independent real-runtime fixture: only published QuadooForm interfaces.
// Descriptors are supplied by the test; this class never reads a package store.
class CRealPluginFactory;
class CRealPlugin : public CBaseUnknown, public IQuadooComponent, public IQuadooNonvisualComponent,
	public IQuadooProperties, public IQuadooConstructionParticipant
{
	TStackRef<IQuadooPropertyDescriptors> m_descriptors;
	TStackRef<IQuadooFactoryLifetime> m_factory;
	IQuadooComponentCollection* m_owner;
	TArray<IJSONValue*> m_values;
	GUID m_id;
	QuadooForm::ComponentState m_state;
	UINT m_name;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooComponent)
		UNK_INTERFACE(IQuadooNonvisualComponent)
		UNK_INTERFACE(IQuadooProperties)
		UNK_INTERFACE(IQuadooConstructionParticipant)
	END_UNK_MAP
	CRealPlugin(IQuadooPropertyDescriptors* descriptors,IQuadooFactoryLifetime* factory,IQuadooComponentCollection* owner) : m_descriptors(descriptors),m_factory(factory),m_owner(owner),m_state(QuadooForm::Constructing),m_name(UINT_MAX) {}
	~CRealPlugin();
	HRESULT Construct(IJSONObject* input)
	{
		HRESULT hr; UINT count; TStackRef<IJSONValue> value; TStackRef<IJSONObject> properties,events; CRString name;
		Check(CoCreateGuid(&m_id)); if(input->HasField(RSTRING_CAST(L"id"))) { Check(input->FindNonNullValueW(L"id",&value)); Check(value->GetStringAsGuid(m_id)); value.Release(); }
		Check(input->FindNonNullValueW(L"name",&value)); Check(value->GetString(&name)); value.Release();
		if(input->HasField(RSTRING_CAST(L"properties"))) { Check(input->FindNonNullValueW(L"properties",&value)); Check(value->GetObject(&properties)); value.Release(); }
		if(input->HasField(RSTRING_CAST(L"events"))) { Check(input->FindNonNullValueW(L"events",&value)); Check(value->GetObject(&events)); value.Release(); }
		Check(m_descriptors->GetCount(&count));
		for(UINT n=0;n<count;n++)
		{
			TStackRef<IQuadooPropertyDescriptor> descriptor; CRString key; QuadooProperty::Type type;
			Check(m_descriptors->GetAt(n,&descriptor)); Check(descriptor->GetName(&key)); Check(descriptor->GetType(&type));
			if(key==RSTRING_CAST(L"Name")) { m_name=n; Check(JSONCreateString(name,&value)); }
			else if(type==QuadooProperty::Event && events && events->HasField(key)) { Check(events->FindNonNullValue(key,&value)); }
			else if(properties && properties->HasField(key)) { Check(properties->FindNonNullValue(key,&value)); }
			else { Check(descriptor->GetDefaultValue(&value)); }
			Check(m_values.Append(static_cast<IJSONValue*>(value))); value.Detach();
		}
		Check(m_owner->ValidateIdentity(this,name,m_id)); Check(m_owner->RegisterComponent(this));
	Cleanup: return hr;
	}
	STDMETHOD(GetID)(GUID* id) { if(id) *id=m_id; return id ? S_OK : E_POINTER; }
	STDMETHOD(GetName)(RSTRING* name) { return GetValueString(m_name,name); }
	STDMETHOD(SetName)(RSTRING name) { HRESULT hr; Check(m_owner->ValidateIdentity(this,name,m_id)); Check(SetValueString(m_name,name)); Cleanup: return hr; }
	STDMETHOD(GetTypeName)(RSTRING* name) { return RStrCreateW(12,L"QuadooPlugin",name); }
	STDMETHOD(GetProperties)(IQuadooProperties** values) { if(!values) return E_POINTER; *values=this; AddRef(); return S_OK; }
	STDMETHOD(GetState)(QuadooForm::ComponentState* state) { if(state) *state=m_state; return state ? S_OK : E_POINTER; }
	STDMETHOD(Destroy)(VOID) { TStackRef<CRealPlugin> keep(this); if(m_state>=QuadooForm::Destroying) return S_FALSE; m_state=QuadooForm::Destroyed; IQuadooComponentCollection* owner=m_owner; m_owner=NULL; return owner ? owner->UnregisterComponent(this) : S_OK; }
	STDMETHOD(GetOwner)(IQuadooComponentCollection** owner) { if(!owner) return E_POINTER; *owner=m_owner; if(*owner) (*owner)->AddRef(); return S_OK; }
	STDMETHOD(GetDescriptors)(IQuadooPropertyDescriptors** value) { if(!value) return E_POINTER; *value=m_descriptors; (*value)->AddRef(); return S_OK; }
	STDMETHOD(GetValueJSON)(UINT index,IJSONValue** value) { if(!value) return E_POINTER; *value=NULL; return index<static_cast<UINT>(m_values.Length()) ? JSONClone(m_values[index],value,TRUE) : E_INVALIDARG; }
	STDMETHOD(SetValueJSON)(UINT index,IJSONValue* value) { HRESULT hr; TStackRef<IJSONValue> copy; CheckIf(index>=static_cast<UINT>(m_values.Length()),E_INVALIDARG); Check(JSONClone(value,&copy,TRUE)); SafeRelease(m_values[index]); m_values[index]=copy.Detach(); Cleanup: return hr; }
	STDMETHOD(GetValueString)(UINT index,RSTRING* value) { HRESULT hr; TStackRef<IJSONValue> item; CheckIf(!value,E_POINTER); *value=NULL; Check(GetValueJSON(index,&item)); Check(item->GetString(value)); Cleanup: return hr; }
	STDMETHOD(SetValueString)(UINT index,RSTRING value) { HRESULT hr; TStackRef<IJSONValue> item; Check(JSONCreateString(value,&item)); Check(SetValueJSON(index,item)); Cleanup: return hr; }
	STDMETHOD(GetValueEvent)(UINT index,RSTRING* value) { return GetValueString(index,value); }
	STDMETHOD(SetValueEvent)(UINT index,RSTRING value) { return SetValueString(index,value); }
	STDMETHOD(GetValueInt)(UINT,INT*) { return DISP_E_TYPEMISMATCH; }
	STDMETHOD(SetValueInt)(UINT,INT) { return DISP_E_TYPEMISMATCH; }
	STDMETHOD(GetValueLong)(UINT,LONGLONG*) { return DISP_E_TYPEMISMATCH; }
	STDMETHOD(SetValueLong)(UINT,LONGLONG) { return DISP_E_TYPEMISMATCH; }
	STDMETHOD(GetValueBool)(UINT,BOOL*) { return DISP_E_TYPEMISMATCH; }
	STDMETHOD(SetValueBool)(UINT,BOOL) { return DISP_E_TYPEMISMATCH; }
	STDMETHOD(GetValueFloat)(UINT,DOUBLE*) { return DISP_E_TYPEMISMATCH; }
	STDMETHOD(SetValueFloat)(UINT,DOUBLE) { return DISP_E_TYPEMISMATCH; }
	STDMETHOD(BindEvents)(IQuadooConstructionContext* context)
	{
		HRESULT hr; TStackRef<IQuadooEventsBinder> binder; Check(context->GetBinder(&binder));
		for(sysint n=0;n<m_values.Length();n++)
		{
			TStackRef<IQuadooPropertyDescriptor> descriptor; QuadooProperty::Type type;
			Check(m_descriptors->GetAt(static_cast<UINT>(n),&descriptor)); Check(descriptor->GetType(&type));
			if(type==QuadooProperty::Event) { CRString name; Check(m_values[n]->GetString(&name)); if(RStrLen(name)) { TStackRef<IQuadooDelegate> delegate; CheckIf(!binder,E_FAIL); Check(binder->BindEvent(name,&delegate)); CheckIf(!delegate,E_FAIL); } }
		}
		m_state=QuadooForm::Binding;
	Cleanup: return hr;
	}
	STDMETHOD(InitializeControl)(VOID) { m_state=QuadooForm::Initializing; return S_OK; }
	STDMETHOD(ActivateControl)(VOID) { m_state=QuadooForm::Ready; return S_OK; }
	STDMETHOD(AbortConstruction)(VOID) { return Destroy(); }
};
class CRealPluginFactory : public CBaseUnknown, public IQuadooComponentFactory, public IQuadooFactoryLifetime
{
public:
	TStackRef<IQuadooPropertyDescriptors> properties;
	TStackRef<IQuadooEventDescriptors> events;
	UINT live;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooComponentFactory)
		UNK_INTERFACE(IQuadooFactoryLifetime)
	END_UNK_MAP
	CRealPluginFactory(IQuadooPropertyDescriptors* p,IQuadooEventDescriptors* e) : properties(p),events(e),live(0) {}
	STDMETHOD(GetLiveCount)(UINT* count) { if(count) *count=live; return count ? S_OK : E_POINTER; }
	STDMETHOD(GetTypeName)(RSTRING* name) { return RStrCreateW(12,L"QuadooPlugin",name); }
	STDMETHOD(GetProperties)(IQuadooPropertyDescriptors** value) { if(!value) return E_POINTER; *value=properties; (*value)->AddRef(); return S_OK; }
	STDMETHOD(GetEvents)(IQuadooEventDescriptors** value) { if(!value) return E_POINTER; *value=events; (*value)->AddRef(); return S_OK; }
	STDMETHOD(CreateComponent)(IQuadooConstructionContext*,IQuadooComponentCollection* owner,IJSONObject* input,IQuadooComponent** output)
	{
		HRESULT hr; TStackRef<CRealPlugin> object; CheckIf(!output,E_POINTER); *output=NULL;
		object.Attach(__new CRealPlugin(properties,this,owner)); CheckAlloc(object); live++; Check(object->Construct(input)); *output=static_cast<IQuadooComponent*>(object.Detach());
	Cleanup: if(FAILED(hr) && object) object->AbortConstruction(); return hr;
	}
};
inline CRealPlugin::~CRealPlugin()
{
	for(sysint n=0;n<m_values.Length();n++) SafeRelease(m_values[n]);
	static_cast<CRealPluginFactory*>(static_cast<IQuadooFactoryLifetime*>(m_factory))->live--;
}
