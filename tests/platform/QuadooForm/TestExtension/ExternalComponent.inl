// Independent nonvisual example. Uses only published QuadooForm interfaces and
// this extension's own metadata/event helpers; no runtime implementation headers.
static const UINT serviceIndexes[] = { 13,0,1,11,12 };
class CServiceProperties : public CModuleObject, public IQuadooPropertyDescriptors
{
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooPropertyDescriptors)
	END_UNK_MAP
	STDMETHOD(GetCount)(UINT* count) { if(count) *count = ARRAYSIZE(serviceIndexes); return count ? S_OK : E_POINTER; }
	STDMETHOD(GetAt)(UINT index, IQuadooPropertyDescriptor** value)
	{
		HRESULT hr = S_OK; CheckIf(!value,E_POINTER); *value = NULL;
		CheckIf(index >= ARRAYSIZE(serviceIndexes),HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
		*value = __new CProperty(serviceIndexes[index],ExternalValidSchema); CheckAlloc(*value);
	Cleanup:
		return hr;
	}
	STDMETHOD(Find)(RSTRING name, IQuadooPropertyDescriptor** value)
	{
		HRESULT hr = DISP_E_MEMBERNOTFOUND; CheckIf(!value,E_POINTER); *value = NULL;
		for(UINT n = 0; n < ARRAYSIZE(serviceIndexes); n++) if(Equal(name,RSTRING_CAST(specs[serviceIndexes[n]].name))) { Check(GetAt(n,value)); break; }
	Cleanup:
		return hr;
	}
};
class CServiceFactory;
class CService : public CModuleObject, public IQuadooComponent, public IQuadooNonvisualComponent,
	public IQuadooProperties, public IQuadooConstructionParticipant, public IQuadooDefinitionSource, public IQuadooPropertyNotifications, public IExternalService, public IExternalComponentTest
{
	IQuadooComponentFactory* m_factory;
	UINT* m_live;
	IQuadooConstructionContext* m_context;
	IQuadooComponentCollection* m_owner; // Weak, following the published contract.
	IJSONObject* m_definition;
	IJSONValue* m_values[ARRAYSIZE(serviceIndexes)];
	IQuadooDelegate* m_pulse;
	IQuadooDelegate* m_final;
	TArray<IQuadooPropertyObserver*> m_observers;
	CRString m_name;
	GUID m_id;
	DWORD m_thread;
	QuadooForm::ComponentState m_state;
	BOOL m_abort;
	ExternalFailure m_failure;
	HRESULT m_error;
	HRESULT Fault(ExternalFailure point) { return point == m_failure ? m_error : S_OK; }
	HRESULT Thread() { return m_thread == GetCurrentThreadId() ? S_OK : QuadooForm::EWrongThread; }
	HRESULT Alive() { HRESULT hr = Thread(); return FAILED(hr) ? hr : m_state >= QuadooForm::Destroying ? QuadooForm::EDestroyed : S_OK; }
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		if((riid == __uuidof(IQuadooDefinitionSource) && m_failure == ExternalDefinitionQI) || (riid == __uuidof(IQuadooComponentHost) && m_failure == ExternalComponentHostQI)) { hr = m_error; goto exit; }
		UNK_INTERFACE(IQuadooComponent)
		UNK_INTERFACE(IQuadooNonvisualComponent)
		UNK_INTERFACE(IQuadooProperties)
		UNK_INTERFACE(IQuadooConstructionParticipant)
		UNK_INTERFACE(IQuadooDefinitionSource)
		UNK_INTERFACE(IQuadooPropertyNotifications)
		UNK_INTERFACE(IExternalService)
		UNK_INTERFACE(IExternalComponentTest)
	END_UNK_MAP
	CService(IQuadooComponentFactory* factory, UINT* live, IQuadooConstructionContext* context, IQuadooComponentCollection* owner) : m_live(live), m_owner(owner), m_definition(NULL), m_pulse(NULL), m_final(NULL), m_id(GUID_NULL), m_thread(GetCurrentThreadId()), m_state(QuadooForm::Constructing), m_abort(FALSE), m_failure(ExternalNoFailure), m_error(E_ABORT)
	{ SetInterface(m_factory,factory); SetInterface(m_context,context); ++*m_live; ZeroMemory(m_values,sizeof(m_values)); ++g_externalCounts.objects; }
	~CService()
	{ for(UINT n = 0; n < ARRAYSIZE(m_values); n++) SafeRelease(m_values[n]); for(sysint n = 0; n < m_observers.Length(); n++) m_observers[n]->Release(); SafeRelease(m_pulse); SafeRelease(m_final); SafeRelease(m_context); SafeRelease(m_definition); --*m_live; m_factory->Release(); --g_externalCounts.objects; }
	STDMETHOD(SetFailure)(ExternalFailure failure, HRESULT error) { m_failure = failure; m_error = error; return S_OK; }
	HRESULT Construct(IJSONObject* definition, BOOL registerComponent = TRUE)
	{
		HRESULT hr; TStackRef<IJSONValue> value, clone; TStackRef<IJSONObject> properties, events;
		Check(CoCreateGuid(&m_id));
		Check(JSONWrapObject(definition,&value)); Check(JSONClone(value,&clone,TRUE)); Check(clone->GetObject(&m_definition)); value.Release(); clone.Release();
		if(definition->HasField(RSTRING_CAST(L"id"))) { Check(definition->FindNonNullValueW(L"id",&value)); Check(value->GetStringAsGuid(m_id)); value.Release(); }
		Check(definition->FindNonNullValueW(L"name",&value)); Check(value->GetString(&m_name)); value.Release();
		if(definition->HasField(RSTRING_CAST(L"properties"))) { Check(definition->FindNonNullValueW(L"properties",&value)); Check(value->GetObject(&properties)); value.Release(); }
		if(definition->HasField(RSTRING_CAST(L"events"))) { Check(definition->FindNonNullValueW(L"events",&value)); Check(value->GetObject(&events)); value.Release(); }
		for(UINT n = 0; n < ARRAYSIZE(serviceIndexes); n++)
		{
			Check(Default(serviceIndexes[n],&m_values[n]));
			IJSONObject* source = n >= 3 ? static_cast<IJSONObject*>(events) : static_cast<IJSONObject*>(properties);
			if(n && source && source->HasField(RSTRING_CAST(specs[serviceIndexes[n]].name)))
			{ Check(source->FindNonNullValueW(specs[serviceIndexes[n]].name,&value)); Check(SetValueJSON(n,value)); value.Release(); }
		}
		SafeRelease(m_values[0]); Check(JSONCreateString(m_name,&m_values[0]));
		if(registerComponent) { Check(m_owner->RegisterComponent(this)); }
	Cleanup:
		return hr;
	}
	STDMETHOD(GetID)(GUID* id) { HRESULT hr = Fault(ExternalID); if(FAILED(hr)) return hr; if(id) *id = m_id; return id ? Thread() : E_POINTER; }
	STDMETHOD(GetName)(RSTRING* name) { HRESULT hr = Fault(ExternalName); if(FAILED(hr)) return hr; if(name) RStrSet(*name,m_name); return name ? Thread() : E_POINTER; }
	STDMETHOD(GetTypeName)(RSTRING* type) { return CopyText(L"ExternalService",type); }
	STDMETHOD(GetProperties)(IQuadooProperties** values) { HRESULT hr = Fault(ExternalProperties); if(FAILED(hr)) return hr; if(!values) return E_POINTER; *values = NULL; if(m_failure != ExternalNullProperties) { *values = this; AddRef(); } return Thread(); }
	STDMETHOD(GetState)(QuadooForm::ComponentState* state) { HRESULT hr = Fault(ExternalState); if(FAILED(hr)) return hr; if(state) *state = m_state; return state ? Thread() : E_POINTER; }
	STDMETHOD(GetOwner)(IQuadooComponentCollection** owner)
	{
		HRESULT hr; CheckIf(!owner,E_POINTER); *owner = NULL; Check(Thread()); Check(Fault(ExternalParent));
		if(m_failure == ExternalObserverReentrant) { TStackRef<IQuadooControlContainer> root; Check(m_owner->GetRoot(&root)); CheckIf(root->Destroy() != QuadooForm::EInvalidState,E_UNEXPECTED); Check(m_error); }
		*owner = m_owner; if(*owner) (*owner)->AddRef();
	Cleanup:
		return hr;
	}
	STDMETHOD(SetName)(RSTRING name)
	{ HRESULT hr; TStackRef<IJSONValue> value; Check(Alive()); Check(JSONCreateString(name,&value)); Check(m_owner->ValidateIdentity(this,name,m_id)); ReplaceInterface(m_values[0],static_cast<IJSONValue*>(value)); m_name = name; Check(Notify(0)); Cleanup: return hr; }
	STDMETHOD(GetDefinitionTemplate)(IJSONObject** definition)
	{ HRESULT hr; TStackRef<IJSONValue> value, clone; CheckIf(!definition,E_POINTER); *definition = NULL; Check(Thread()); Check(JSONWrapObject(m_definition,&value)); Check(JSONClone(value,&clone,TRUE)); Check(clone->GetObject(definition)); Cleanup: return hr; }
	STDMETHOD(GetDescriptors)(IQuadooPropertyDescriptors** descriptors) { HRESULT hr = Fault(ExternalDescriptors); if(FAILED(hr)) return hr; if(m_failure == ExternalNullDescriptors) { if(descriptors) *descriptors = NULL; return descriptors ? S_OK : E_POINTER; } return m_factory->GetProperties(descriptors); }
	STDMETHOD(GetValueJSON)(UINT index, IJSONValue** value)
	{ HRESULT hr; CheckIf(!value,E_POINTER); *value = NULL; Check(Thread()); CheckIf(index >= ARRAYSIZE(m_values),HRESULT_FROM_WIN32(ERROR_INVALID_INDEX)); Check(JSONClone(m_values[index],value,TRUE)); Cleanup: return hr; }
	STDMETHOD(SetValueJSON)(UINT index, IJSONValue* value)
	{
		HRESULT hr; TStackRef<IJSONValue> copy;
		Check(Alive()); CheckIf(index >= ARRAYSIZE(m_values),HRESULT_FROM_WIN32(ERROR_INVALID_INDEX)); CheckIf(!value,E_INVALIDARG);
		if(!index) { CRString name; Check(value->GetString(&name)); Check(SetName(name)); goto Cleanup; }
		if(index == 1) { INT level; Check(value->GetInteger(&level)); }
		else { CRString text; Check(value->GetString(&text)); }
		Check(JSONClone(value,&copy,TRUE)); ReplaceInterface(m_values[index],static_cast<IJSONValue*>(copy)); Check(Notify(index));
	Cleanup:
		return hr;
	}
	STDMETHOD(GetValueInt)(UINT index, INT* value) { HRESULT hr; TStackRef<IJSONValue> json; CheckIf(!value,E_POINTER); CheckIf(index != 1,DISP_E_TYPEMISMATCH); Check(GetValueJSON(index,&json)); Check(json->GetInteger(value)); Cleanup: return hr; }
	STDMETHOD(GetValueString)(UINT index, RSTRING* value) { HRESULT hr; TStackRef<IJSONValue> json; CheckIf(!value,E_POINTER); *value = NULL; CheckIf(index != 0 && index != 2,DISP_E_TYPEMISMATCH); Check(GetValueJSON(index,&json)); Check(json->GetString(value)); Cleanup: return hr; }
	STDMETHOD(GetValueEvent)(UINT index, RSTRING* value) { HRESULT hr; TStackRef<IJSONValue> json; CheckIf(!value,E_POINTER); *value = NULL; CheckIf(index < 3 || index > 4,DISP_E_TYPEMISMATCH); Check(GetValueJSON(index,&json)); Check(json->GetString(value)); Cleanup: return hr; }
	STDMETHOD(SetValueInt)(UINT index, INT value) { HRESULT hr; TStackRef<IJSONValue> json; CheckIf(index != 1,DISP_E_TYPEMISMATCH); Check(JSONCreateInteger(value,&json)); Check(SetValueJSON(index,json)); Cleanup: return hr; }
	STDMETHOD(SetValueString)(UINT index, RSTRING value) { HRESULT hr; TStackRef<IJSONValue> json; CheckIf(index != 0 && index != 2,DISP_E_TYPEMISMATCH); Check(JSONCreateString(value,&json)); Check(SetValueJSON(index,json)); Cleanup: return hr; }
	STDMETHOD(SetValueEvent)(UINT index, RSTRING value) { HRESULT hr; TStackRef<IJSONValue> json; CheckIf(index < 3 || index > 4,DISP_E_TYPEMISMATCH); Check(JSONCreateString(value,&json)); Check(SetValueJSON(index,json)); Cleanup: return hr; }
	STDMETHOD(GetValueLong)(UINT index,LONGLONG* value) { return DISP_E_TYPEMISMATCH; }
	STDMETHOD(GetValueBool)(UINT index,BOOL* value) { return DISP_E_TYPEMISMATCH; }
	STDMETHOD(GetValueFloat)(UINT index,DOUBLE* value) { return DISP_E_TYPEMISMATCH; }
	STDMETHOD(SetValueLong)(UINT index,LONGLONG value) { return DISP_E_TYPEMISMATCH; }
	STDMETHOD(SetValueBool)(UINT index,BOOL value) { return DISP_E_TYPEMISMATCH; }
	STDMETHOD(SetValueFloat)(UINT index,DOUBLE value) { return DISP_E_TYPEMISMATCH; }
	STDMETHOD(AddObserver)(IQuadooPropertyObserver* observer) { HRESULT hr; CheckIf(!observer,E_INVALIDARG); Check(m_observers.Append(observer)); observer->AddRef(); Cleanup: return hr; }
	STDMETHOD(RemoveObserver)(IQuadooPropertyObserver* observer) { for(sysint n = 0; n < m_observers.Length(); n++) if(m_observers[n] == observer) { m_observers.Remove(n,NULL); observer->Release(); return S_OK; } return S_FALSE; }
	HRESULT Notify(UINT index)
	{
		HRESULT hr = S_OK; TStackRef<CService> self(this); TArray<IQuadooPropertyObserver*> observers;
		if(m_state == QuadooForm::Ready)
		{
			for(sysint n = 0; n < m_observers.Length(); n++) { Check(observers.Append(m_observers[n])); m_observers[n]->AddRef(); }
			for(sysint n = 0; n < observers.Length() && m_state == QuadooForm::Ready; n++) { Check(observers[n]->OnPropertyChanged(this,index)); }
		}
	Cleanup:
		for(sysint n = 0; n < observers.Length(); n++) observers[n]->Release(); return hr;
	}
	STDMETHOD(BindEvents)(IQuadooConstructionContext* context)
	{
		HRESULT hr; QuadooForm::ContextMode mode; TStackRef<IQuadooEventsBinder> binder;
		Check(Alive()); CheckIf(m_state != QuadooForm::Constructing,QuadooForm::EInvalidState);
		Check(context->GetMode(&mode)); Check(context->GetBinder(&binder));
		if(mode == QuadooForm::Runtime) for(UINT n = 3; n < 5; n++)
		{
			CRString handler; Check(m_values[n]->GetString(&handler));
			if(RStrLen(handler)) { CheckIf(!binder,QuadooForm::EHandlerNotFound); IQuadooDelegate** target = n == 3 ? &m_pulse : &m_final; hr = binder->BindEvent(handler,target); CheckIf(hr == S_FALSE || (SUCCEEDED(hr) && !*target),QuadooForm::EHandlerNotFound); Check(hr); }
		}
		m_state = QuadooForm::Binding; hr = S_OK;
	Cleanup:
		return hr;
	}
	STDMETHOD(InitializeControl)(VOID) { if(m_state != QuadooForm::Binding) return QuadooForm::EInvalidState; m_state = QuadooForm::Ready; return S_OK; }
	STDMETHOD(ActivateControl)(VOID) { return Alive(); }
	STDMETHOD(AbortConstruction)(VOID) { m_abort = TRUE; return Destroy(); }
	STDMETHOD(Pulse)(INT value, INT* accepted)
	{
		HRESULT hr; TStackRef<CService> self(this); TStackRef<IQuadooDelegate> delegate(m_pulse); TStackRef<CArgs> args;
		CheckIf(!accepted,E_POINTER); *accepted = value; Check(Alive()); CheckIf(m_state != QuadooForm::Ready,QuadooForm::EInvalidState);
		if(delegate) { args.Attach(__new CArgs(this,FALSE,value)); CheckAlloc(args); Check(delegate->Invoke(args)); Check(args->GetInt(0,accepted)); }
		hr = S_OK;
	Cleanup:
		return hr;
	}
	STDMETHOD(Destroy)(VOID)
	{
		HRESULT hr; TStackRef<CService> self(this);
		Check(Thread()); CheckIfIgnore(m_state >= QuadooForm::Destroying,S_FALSE); m_state = QuadooForm::Destroying;
		hr = S_OK;
		if(m_final && !m_abort) { TStackRef<CArgs> args; args.Attach(__new CArgs(this,TRUE,0)); hr = args ? m_final->Invoke(args) : E_OUTOFMEMORY; }
		if(m_owner) { IQuadooComponentCollection* owner = m_owner; m_owner = NULL; owner->UnregisterComponent(this); }
		SafeRelease(m_pulse); SafeRelease(m_final); SafeRelease(m_context); for(sysint n = 0; n < m_observers.Length(); n++) m_observers[n]->Release(); m_observers.Clear();
		m_state = QuadooForm::Destroyed; ++g_externalCounts.destroyed;
	Cleanup:
		return hr;
	}
};
class CServiceFactory : public CIconFactory, public IQuadooComponentFactory, public IQuadooFactoryLifetime, public IExternalDesignPolicy, public IExternalComponentTestFactory
{
	UINT m_live;
	HRESULT m_designResult;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooComponentFactory)
		UNK_INTERFACE(IQuadooFactoryLifetime)
		UNK_INTERFACE(IExternalDesignPolicy)
		UNK_INTERFACE(IExternalComponentTestFactory)
		UNK_CHAIN_TO(CIconFactory)
	END_UNK_MAP
	CServiceFactory() : CIconFactory(102), m_live(0), m_designResult(S_OK) {}
	STDMETHOD(SetDesignResult)(HRESULT result) { m_designResult = result; return S_OK; }
	STDMETHOD(GetTypeName)(RSTRING* name) { return CopyText(L"ExternalService",name); }
	STDMETHOD(GetLiveCount)(UINT* count) { if(count) *count = m_live; return count ? S_OK : E_POINTER; }
	STDMETHOD(GetProperties)(IQuadooPropertyDescriptors** properties) { if(properties) *properties = __new CServiceProperties; return !properties ? E_POINTER : *properties ? S_OK : E_OUTOFMEMORY; }
	STDMETHOD(GetEvents)(IQuadooEventDescriptors** events) { if(events) *events = __new CEvents(ExternalValidSchema); return !events ? E_POINTER : *events ? S_OK : E_OUTOFMEMORY; }
	STDMETHOD(CreateComponent)(IQuadooConstructionContext* context, IQuadooComponentCollection* owner, IJSONObject* definition, IQuadooComponent** component)
	{ return Create(context,owner,definition,component,TRUE); }
	STDMETHOD(CreateCandidate)(IQuadooConstructionContext* context, IQuadooComponentCollection* owner, IJSONObject* definition, IQuadooComponent** component)
	{ return Create(context,owner,definition,component,FALSE); }
	HRESULT Create(IQuadooConstructionContext* context, IQuadooComponentCollection* owner, IJSONObject* definition, IQuadooComponent** component, BOOL registerComponent)
	{
		HRESULT hr; TStackRef<CService> service; QuadooForm::ContextMode mode;
		CheckIf(!component,E_POINTER); *component = NULL; CheckIf(!context || !owner || !definition,E_INVALIDARG);
		Check(context->GetMode(&mode)); if(mode == QuadooForm::Design) { Check(m_designResult); }
		service.Attach(__new CService(this,&m_live,context,owner)); CheckAlloc(service);
		Check(service->Construct(definition,registerComponent)); *component = service.Detach();
	Cleanup:
		if(FAILED(hr) && service) service->AbortConstruction();
		return hr;
	}
};
