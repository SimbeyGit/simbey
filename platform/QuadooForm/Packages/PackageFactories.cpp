#include "Packages.h"
#include <commctrl.h>

namespace QFP
{
class CFactoryBase : public CBaseUnknown, public IQuadooFactoryLifetime, public IQuadooPaletteIcon
{
public:
	TStackRef<CDefinition> definition;
	CRString usageDirectory;
	DWORD thread;
	UINT live;
	BOOL active;
	HRESULT initialization;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooFactoryLifetime)
		UNK_INTERFACE(IQuadooPaletteIcon)
	END_UNK_MAP
	CFactoryBase(CDefinition* value,PCWSTR directory) : definition(value),thread(GetCurrentThreadId()),live(0),active(TRUE) { initialization=RStrCreateW(TStrLenAssert(directory),directory,&usageDirectory); }
	HRESULT Thread() const { return thread==GetCurrentThreadId() ? S_OK : QuadooForm::EWrongThread; }
	STDMETHOD(GetLiveCount)(UINT* count) { if(count) *count=live; return !count ? E_POINTER : Thread(); }
	STDMETHOD(GetPaletteIcon)(UINT cx,UINT cy,HICON* icon) { HRESULT hr; CheckIf(!icon,E_POINTER); *icon=NULL; Check(Thread()); Check(definition->GetIcon(cx,cy,icon)); Cleanup: return hr; }
	HRESULT Enter(IQuadooConstructionContext* context,HANDLE* lease)
	{
		HRESULT hr; QuadooForm::ContextMode mode;
		Check(Thread()); CheckIf(!context || !lease,E_INVALIDARG); *lease=NULL;
		Check(context->GetMode(&mode));
		if(mode!=QuadooForm::Design) { context->ReportError(QuadooForm::EDesignUnsupported,definition->type,RSTRING_CAST(L"This is a design-time package. Register the real control factory before loading the form.")); Check(QuadooForm::EDesignUnsupported); }
		CheckIf(!active,HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
		Check(AcquirePackageUsage(usageDirectory.c_wstr(),definition,lease));
	Cleanup: return hr;
	}
};
class CObject : public CBaseUnknown, public IQuadooComponent, public IQuadooProperties,
	public IQuadooPropertyNotifications, public IQuadooConstructionParticipant, public IQuadooDefinitionSource
{
protected:
	TStackRef<CFactoryBase> m_factory;
	CDefinition* m_definition;
	TArray<IJSONValue*> m_values;
	TArray<IQuadooPropertyObserver*> m_observers;
	TStackRef<IJSONObject> m_template;
	GUID m_id;
	QuadooForm::ComponentState m_state;
	HANDLE m_lease;
	virtual HRESULT Detach() = 0;
	virtual HRESULT ValidateName(RSTRING name) = 0;
	virtual HRESULT Apply() { return S_OK; }
	virtual IQuadooComponent* Component() { return static_cast<IQuadooComponent*>(this); }
	HRESULT Alive() { HRESULT hr=m_factory->Thread(); return FAILED(hr) ? hr : m_state>=QuadooForm::Destroying ? QuadooForm::EDestroyed : S_OK; }
	UINT Index(PCWSTR name) const { return m_definition->Find(name); }
	HRESULT Notify(UINT index)
	{
		HRESULT hr=S_OK; TArray<IQuadooPropertyObserver*> observers;
		TStackRef<CObject> keep(this);
		for(sysint n=0;n<m_observers.Length();n++) { Check(observers.Append(m_observers[n])); m_observers[n]->AddRef(); }
		for(sysint n=0;n<observers.Length();n++) { Check(observers[n]->OnPropertyChanged(Component(),index)); if(m_state>=QuadooForm::Destroying) break; }
	Cleanup:
		for(sysint n=0;n<observers.Length();n++) observers[n]->Release();
		return hr;
	}
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooComponent)
		UNK_INTERFACE(IQuadooProperties)
		UNK_INTERFACE(IQuadooPropertyNotifications)
		UNK_INTERFACE(IQuadooConstructionParticipant)
		UNK_INTERFACE(IQuadooDefinitionSource)
	END_UNK_MAP
	CObject(CFactoryBase* factory,HANDLE lease) : m_factory(factory),m_definition(factory->definition),m_state(QuadooForm::Constructing),m_lease(lease) { ZeroMemory(&m_id,sizeof(m_id)); factory->live++; }
	virtual ~CObject()
	{
		for(sysint n=0;n<m_values.Length();n++) SafeRelease(m_values[n]);
		for(sysint n=0;n<m_observers.Length();n++) m_observers[n]->Release();
		if(m_lease) CloseHandle(m_lease); m_factory->live--;
	}
	HRESULT Construct(IJSONObject* definition)
	{
		HRESULT hr; TStackRef<IJSONValue> value; TStackRef<IJSONObject> properties,events; CRString name,type;
		Check(ReadString(definition,L"type",type)); CheckIf(!(type==static_cast<RSTRING>(m_definition->type)),E_INVALIDARG);
		Check(JSONCloneObject(definition,&value,TRUE)); Check(value->GetObject(&m_template)); value.Release();
		Check(ReadString(definition,L"name",name)); CheckIf(!RStrLen(name),E_INVALIDARG);
		if(definition->HasField(RSTRING_CAST(L"id"))) { Check(definition->FindNonNullValueW(L"id",&value)); Check(value->GetStringAsGuid(m_id)); value.Release(); CheckIf(IsEqualGUID(m_id,GUID_NULL),E_INVALIDARG); }
		else { Check(CoCreateGuid(&m_id)); }
		if(definition->HasField(RSTRING_CAST(L"properties"))) { Check(definition->FindNonNullValueW(L"properties",&value)); Check(value->GetObject(&properties)); value.Release(); }
		if(definition->HasField(RSTRING_CAST(L"events"))) { Check(definition->FindNonNullValueW(L"events",&value)); Check(value->GetObject(&events)); value.Release(); }
		for(sysint n=0;n<m_definition->properties.Length();n++)
		{
			Property* property=m_definition->properties[n]; TStackRef<IJSONValue> input,normalized;
			if(property->name==RSTRING_CAST(L"Name")) { Check(JSONCreateString(name,&input)); }
			else
			{
				IJSONObject* source=property->type==QuadooProperty::Event ? events : properties;
				if(source && source->HasField(property->name)) { Check(source->FindValue(property->name,&input)); }
				else { Check(JSONClone(property->initial,&input,TRUE)); }
			}
			Check(Normalize(property,input,&normalized)); Check(m_values.Append(static_cast<IJSONValue*>(normalized))); normalized.Detach();
		}
		Check(ValidateName(name));
	Cleanup: return hr;
	}
	STDMETHOD(GetID)(GUID* value) { if(value) *value=m_id; return value ? m_factory->Thread() : E_POINTER; }
	STDMETHOD(GetName)(RSTRING* value) { return GetValueString(Index(L"Name"),value); }
	STDMETHOD(SetName)(RSTRING value) { return SetValueString(Index(L"Name"),value); }
	STDMETHOD(GetTypeName)(RSTRING* value) { return CopyString(m_definition->type,value); }
	STDMETHOD(GetProperties)(IQuadooProperties** value) { if(!value) return E_POINTER; *value=static_cast<IQuadooProperties*>(this); AddRef(); return S_OK; }
	STDMETHOD(GetState)(QuadooForm::ComponentState* state) { if(state) *state=m_state; return state ? m_factory->Thread() : E_POINTER; }
	STDMETHOD(Destroy)(VOID)
	{
		HRESULT hr; TStackRef<CObject> keep(this);
		Check(m_factory->Thread()); CheckIfIgnore(m_state>=QuadooForm::Destroying,S_FALSE);
		m_state=QuadooForm::Destroying; hr=Detach(); m_state=QuadooForm::Destroyed;
		for(sysint n=0;n<m_observers.Length();n++) m_observers[n]->Release(); m_observers.Clear();
	Cleanup: return hr;
	}
	STDMETHOD(BindEvents)(IQuadooConstructionContext* context) { HRESULT hr; QuadooForm::ContextMode mode; Check(Alive()); CheckIf(!context || m_state!=QuadooForm::Constructing,QuadooForm::EInvalidState); Check(context->GetMode(&mode)); CheckIf(mode!=QuadooForm::Design,QuadooForm::EDesignUnsupported); m_state=QuadooForm::Binding; Cleanup: return hr; }
	STDMETHOD(InitializeControl)(VOID) { HRESULT hr; Check(Alive()); CheckIf(m_state!=QuadooForm::Binding,QuadooForm::EInvalidState); m_state=QuadooForm::Initializing; Check(Apply()); Cleanup: return hr; }
	STDMETHOD(ActivateControl)(VOID) { HRESULT hr; Check(Alive()); CheckIf(m_state!=QuadooForm::Initializing,QuadooForm::EInvalidState); m_state=QuadooForm::Ready; Cleanup: return hr; }
	STDMETHOD(AbortConstruction)(VOID) { return Destroy(); }
	STDMETHOD(GetDefinitionTemplate)(IJSONObject** value) { HRESULT hr; TStackRef<IJSONValue> clone; CheckIf(!value,E_POINTER); *value=NULL; Check(JSONCloneObject(m_template,&clone,TRUE)); Check(clone->GetObject(value)); Cleanup: return hr; }
	STDMETHOD(GetDescriptors)(IQuadooPropertyDescriptors** value) { return m_definition->GetProperties(value); }
	STDMETHOD(GetValueJSON)(UINT index,IJSONValue** value)
	{
		HRESULT hr; CheckIf(!value,E_POINTER); *value=NULL; Check(m_factory->Thread()); CheckIf(index>=static_cast<UINT>(m_values.Length()),HRESULT_FROM_WIN32(ERROR_INVALID_INDEX)); Check(JSONClone(m_values[index],value,TRUE)); Cleanup: return hr;
	}
	STDMETHOD(SetValueJSON)(UINT index,IJSONValue* input)
	{
		HRESULT hr; TStackRef<IJSONValue> next,previous; CRString name; TStackRef<CObject> keep(this);
		Check(Alive()); CheckIf(index>=static_cast<UINT>(m_values.Length()),HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
		CheckIf(m_definition->properties[index]->flags&QuadooForm::PropertyReadOnly,E_ACCESSDENIED);
		Check(Normalize(m_definition->properties[index],input,&next));
		if(index==Index(L"Name")) { Check(next->GetString(&name)); Check(ValidateName(name)); }
		CheckIfIgnore(!JSONCompare(m_values[index],next),S_FALSE);
		previous.Attach(m_values[index]); m_values[index]=next.Detach();
		hr=Apply(); if(SUCCEEDED(hr) && m_state==QuadooForm::Ready) hr=Notify(index);
		if(FAILED(hr) && m_state<QuadooForm::Destroying) { SafeRelease(m_values[index]); m_values[index]=previous.Detach(); Apply(); }
	Cleanup: return hr;
	}
	STDMETHOD(GetValueInt)(UINT index,INT* value) { HRESULT hr; TStackRef<IJSONValue> item; CheckIf(!value,E_POINTER); Check(GetValueJSON(index,&item)); CheckIf(!item,DISP_E_TYPEMISMATCH); Check(item->GetInteger(value)); Cleanup: return hr; }
	STDMETHOD(GetValueLong)(UINT index,LONGLONG* value) { HRESULT hr; TStackRef<IJSONValue> item; CheckIf(!value,E_POINTER); Check(GetValueJSON(index,&item)); CheckIf(!item,DISP_E_TYPEMISMATCH); if(item->GetType()==JSON::Integer) { INT integer; Check(item->GetInteger(&integer)); *value=integer; } else { Check(item->GetLongInteger(value)); } Cleanup: return hr; }
	STDMETHOD(GetValueFloat)(UINT index,DOUBLE* value) { HRESULT hr; TStackRef<IJSONValue> item; CheckIf(!value,E_POINTER); Check(GetValueJSON(index,&item)); CheckIf(!item,DISP_E_TYPEMISMATCH); Check(item->GetDouble(value)); Cleanup: return hr; }
	STDMETHOD(GetValueBool)(UINT index,BOOL* value) { HRESULT hr; TStackRef<IJSONValue> item; bool flag; CheckIf(!value,E_POINTER); Check(GetValueJSON(index,&item)); CheckIf(!item,DISP_E_TYPEMISMATCH); Check(item->GetBoolean(&flag)); *value=flag; Cleanup: return hr; }
	STDMETHOD(GetValueString)(UINT index,RSTRING* value) { HRESULT hr; TStackRef<IJSONValue> item; CheckIf(!value,E_POINTER); *value=NULL; Check(GetValueJSON(index,&item)); CheckIf(!item,DISP_E_TYPEMISMATCH); Check(item->GetString(value)); Cleanup: return hr; }
	STDMETHOD(GetValueEvent)(UINT index,RSTRING* value) { return GetValueString(index,value); }
	STDMETHOD(SetValueInt)(UINT index,INT value) { HRESULT hr; TStackRef<IJSONValue> item; Check(JSONCreateInteger(value,&item)); Check(SetValueJSON(index,item)); Cleanup: return hr; }
	STDMETHOD(SetValueLong)(UINT index,LONGLONG value) { HRESULT hr; TStackRef<IJSONValue> item; Check(JSONCreateLongInteger(value,&item)); Check(SetValueJSON(index,item)); Cleanup: return hr; }
	STDMETHOD(SetValueFloat)(UINT index,DOUBLE value) { HRESULT hr; TStackRef<IJSONValue> item; Check(JSONCreateDouble(value,&item)); Check(SetValueJSON(index,item)); Cleanup: return hr; }
	STDMETHOD(SetValueBool)(UINT index,BOOL value) { HRESULT hr; TStackRef<IJSONValue> item; Check(JSONCreateBool(value!=FALSE,&item)); Check(SetValueJSON(index,item)); Cleanup: return hr; }
	STDMETHOD(SetValueString)(UINT index,RSTRING value) { HRESULT hr; TStackRef<IJSONValue> item; Check(JSONCreateString(value,&item)); Check(SetValueJSON(index,item)); Cleanup: return hr; }
	STDMETHOD(SetValueEvent)(UINT index,RSTRING value) { return SetValueString(index,value); }
	STDMETHOD(AddObserver)(IQuadooPropertyObserver* value) { HRESULT hr; Check(Alive()); CheckIf(!value,E_POINTER); for(sysint n=0;n<m_observers.Length();n++) if(QFD::SameObject(value,m_observers[n])) return S_FALSE; Check(m_observers.Append(value)); value->AddRef(); Cleanup: return hr; }
	STDMETHOD(RemoveObserver)(IQuadooPropertyObserver* value) { HRESULT hr; Check(m_factory->Thread()); CheckIf(!value,E_POINTER); for(sysint n=0;n<m_observers.Length();n++) if(QFD::SameObject(value,m_observers[n])) { m_observers[n]->Release(); m_observers.Remove(n,NULL); return S_OK; } hr=HRESULT_FROM_WIN32(ERROR_NOT_FOUND); Cleanup: return hr; }
};
class CNonvisual : public CObject, public IQuadooNonvisualComponent
{
	IQuadooComponentCollection* m_owner;
protected:
	HRESULT ValidateName(RSTRING name) { return m_owner ? m_owner->ValidateIdentity(Component(),name,m_id) : QuadooForm::EDestroyed; }
	HRESULT Detach() { IQuadooComponentCollection* owner=m_owner; m_owner=NULL; return owner ? owner->UnregisterComponent(Component()) : S_OK; }
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooNonvisualComponent)
		UNK_CHAIN_TO(CObject)
	END_UNK_MAP
	CNonvisual(CFactoryBase* factory,HANDLE lease,IQuadooComponentCollection* owner) : CObject(factory,lease),m_owner(owner) {}
	STDMETHOD(GetOwner)(IQuadooComponentCollection** value) { if(!value) return E_POINTER; *value=m_owner; if(*value) (*value)->AddRef(); return m_factory->Thread(); }
};
#define QFP_COMPONENT_FORWARD \
	STDMETHOD(GetID)(GUID* value) { return CObject::GetID(value); } \
	STDMETHOD(GetName)(RSTRING* value) { return CObject::GetName(value); } \
	STDMETHOD(SetName)(RSTRING value) { return CObject::SetName(value); } \
	STDMETHOD(GetTypeName)(RSTRING* value) { return CObject::GetTypeName(value); } \
	STDMETHOD(GetProperties)(IQuadooProperties** value) { return CObject::GetProperties(value); } \
	STDMETHOD(GetState)(QuadooForm::ComponentState* value) { return CObject::GetState(value); } \
	STDMETHOD(Destroy)(VOID) { return CObject::Destroy(); }

template<class Interface> class TVisual : public CObject, public Interface,
	public IQuadooLayoutControl, public IQuadooDesignCapabilities
{
protected:
	IQuadooControlContainer* m_parent;
	HWND m_window;
	UINT m_dpi;
	HFONT m_font;
	HICON m_icon;
	TStackRef<IJSONValue> m_fontValue;
	QuadooForm::Rect m_effective;
	HRESULT Resources(UINT dpi)
	{
		HRESULT hr; TStackRef<IJSONValue> value; TStackRef<IJSONObject> object; CRString face; INT size; BOOL bold,italic;
		Check(GetValueJSON(Index(L"Font"),&value)); Check(value->GetObject(&object));
		if(!m_font || dpi!=m_dpi || JSONCompare(value,m_fontValue))
		{
			Check(ReadString(object,L"Name",face,L"Segoe UI")); Check(ReadInt(object,L"Size",&size,9));
			Check(ReadBool(object,L"Bold",&bold)); Check(ReadBool(object,L"Italic",&italic));
			LOGFONTW font; ZeroMemory(&font,sizeof(font)); font.lfHeight=-MulDiv(size,dpi,72); font.lfWeight=bold ? FW_BOLD : FW_NORMAL; font.lfItalic=static_cast<BYTE>(italic); font.lfCharSet=DEFAULT_CHARSET;
			Check(TStrCchCpy(font.lfFaceName,ARRAYSIZE(font.lfFaceName),face.c_wstr()));
			HFONT next=CreateFontIndirectW(&font); CheckIf(!next,HrEnsureLastError()); if(m_font) DeleteObject(m_font); m_font=next; m_fontValue=value;
		}
		if(dpi!=m_dpi || !m_icon)
		{
			HICON next=NULL; hr=m_definition->GetIcon(MulDiv(20,dpi,96),MulDiv(20,dpi,96),&next); Check(hr);
			if(m_icon) DestroyIcon(m_icon); m_icon=next;
		}
	Cleanup: return hr;
	}
	HRESULT ValidateName(RSTRING name) { return QuadooValidateControlName(static_cast<Interface*>(this),name); }
	virtual IQuadooComponent* Component() { return static_cast<Interface*>(this); }
	HRESULT Detach()
	{
		HRESULT hr=S_OK;
		if(m_window) { RemoveWindowSubclass(m_window,Procedure,1); DestroyWindow(m_window); m_window=NULL; }
		if(m_font) { DeleteObject(m_font); m_font=NULL; } if(m_icon) { DestroyIcon(m_icon); m_icon=NULL; }
		IQuadooControlContainer* parent=m_parent; m_parent=NULL;
		if(parent) hr=parent->UnregisterControl(static_cast<Interface*>(this));
		return hr==HRESULT_FROM_WIN32(ERROR_NOT_FOUND) ? S_OK : hr;
	}
	virtual HRESULT Apply()
	{
		HRESULT hr=S_OK;
		if(m_window)
		{
			QuadooForm::Rect bounds; Check(GetBounds(&bounds));
			Check(Arrange(&bounds,m_dpi)); InvalidateRect(m_window,NULL,FALSE);
			if(m_parent && m_state>=QuadooForm::Initializing) { Check(m_parent->UpdateLayout()); }
		}
	Cleanup: return hr;
	}
	virtual HRESULT PaintChildren(HDC) { return S_OK; }
	VOID PaintSurface(HDC dc)
	{
		RECT rect; GetClientRect(m_window,&rect);
		HBRUSH brush=CreateSolidBrush(ResolveQuadooColor(QFD::IntProperty(static_cast<Interface*>(this),L"BackColor",0x8000000f)));
		FillRect(dc,&rect,brush); DeleteObject(brush); FrameRect(dc,&rect,GetSysColorBrush(COLOR_BTNSHADOW));
		UINT pixels=MulDiv(20,m_dpi,96);
		if(m_icon) DrawIconEx(dc,4,4,m_icon,pixels,pixels,0,NULL,DI_NORMAL);
		CRString name,label; GetName(&name); RStrFormatW(&label,L"%ls (%ls)",name.c_wstr(),m_definition->type.c_wstr());
		SetBkMode(dc,TRANSPARENT); SetTextColor(dc,ResolveQuadooColor(QFD::IntProperty(static_cast<Interface*>(this),L"ForeColor",0x80000008)));
		HGDIOBJ previous=m_font ? SelectObject(dc,m_font) : NULL;
		rect.left+=pixels+8; rect.top+=4; DrawTextW(dc,label.c_wstr(),RStrLen(label),&rect,DT_TOP | DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX); if(previous) SelectObject(dc,previous);
		PaintChildren(dc);
	}
	static LRESULT CALLBACK Procedure(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR data)
	{
		TVisual* self=reinterpret_cast<TVisual*>(data);
		TStackRef<TVisual> keep(self);
		if(message==WM_ERASEBKGND) return 1;
		if(message==WM_PRINTCLIENT) { self->PaintSurface(reinterpret_cast<HDC>(wp)); return 0; }
		if(message==WM_PAINT)
		{
			PAINTSTRUCT paint; HDC dc=BeginPaint(window,&paint); self->PaintSurface(dc); EndPaint(window,&paint); return 0;
		}
		return DefSubclassProc(window,message,wp,lp);
	}
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE_PATH(IQuadooComponent,Interface)
		UNK_INTERFACE_PATH(IQuadooControl,Interface)
		UNK_INTERFACE_PATH(IQuadooWinControl,Interface)
		UNK_INTERFACE(Interface)
		UNK_INTERFACE(IQuadooLayoutControl)
		UNK_INTERFACE(IQuadooDesignCapabilities)
		UNK_CHAIN_TO(CObject)
	END_UNK_MAP
	QFP_COMPONENT_FORWARD
	TVisual(CFactoryBase* factory,HANDLE lease,IQuadooControlContainer* parent) : CObject(factory,lease),m_parent(parent),m_window(NULL),m_dpi(96),m_font(NULL),m_icon(NULL) { ZeroMemory(&m_effective,sizeof(m_effective)); }
	~TVisual() { if(m_window) { RemoveWindowSubclass(m_window,Procedure,1); DestroyWindow(m_window); } if(m_font) DeleteObject(m_font); if(m_icon) DestroyIcon(m_icon); }
	HRESULT Attach(IQuadooConstructionContext* context)
	{
		HRESULT hr; HWND parent; TStackRef<IQuadooChildWindowHost> host;
		Check(context->GetDPI(&m_dpi)); if(!m_dpi) m_dpi=96;
		hr=m_parent->QueryInterface(__uuidof(IQuadooChildWindowHost),reinterpret_cast<PVOID*>(&host));
		if(hr==E_NOINTERFACE) { Check(m_parent->GetHandle(&parent)); } else { Check(hr); Check(host->GetChildHostHandle(&parent)); }
		m_window=CreateWindowExW(WS_EX_CONTROLPARENT,L"STATIC",L"",WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,0,0,1,1,parent,NULL,GetModuleHandle(NULL),NULL); CheckIf(!m_window,HrEnsureLastError());
		CheckIf(!SetWindowSubclass(m_window,Procedure,1,reinterpret_cast<DWORD_PTR>(this)),HrEnsureLastError()); Check(Apply()); Check(m_parent->RegisterControl(static_cast<Interface*>(this)));
	Cleanup: return hr;
	}
	STDMETHOD(GetParent)(IQuadooControlContainer** value) { if(!value) return E_POINTER; *value=m_parent; if(*value) (*value)->AddRef(); return m_factory->Thread(); }
	STDMETHOD(GetBounds)(QuadooForm::Rect* value) { HRESULT hr; CheckIf(!value,E_POINTER); Check(GetValueInt(Index(L"Left"),&value->Left)); Check(GetValueInt(Index(L"Top"),&value->Top)); Check(GetValueInt(Index(L"Width"),&value->Width)); Check(GetValueInt(Index(L"Height"),&value->Height)); Cleanup: return hr; }
	STDMETHOD(SetBounds)(const QuadooForm::Rect* value)
	{
		HRESULT hr; TStackRef<IJSONValue> next[4],old[4]; UINT indices[4]; static PCWSTR const names[]={L"Left",L"Top",L"Width",L"Height"};
		Check(Alive()); CheckIf(!value,E_POINTER);
		for(UINT n=0;n<4;n++) { indices[n]=Index(names[n]); CheckIf(indices[n]>=static_cast<UINT>(m_values.Length()),E_UNEXPECTED); TStackRef<IJSONValue> input; Check(JSONCreateInteger(reinterpret_cast<const INT*>(value)[n],&input)); Check(Normalize(m_definition->properties[indices[n]],input,&next[n])); }
		for(UINT n=0;n<4;n++) { old[n].Attach(m_values[indices[n]]); m_values[indices[n]]=next[n].Detach(); }
		hr=Apply(); if(SUCCEEDED(hr) && m_state==QuadooForm::Ready) for(UINT n=0;n<4 && SUCCEEDED(hr);n++) hr=Notify(indices[n]);
		if(FAILED(hr) && m_state<QuadooForm::Destroying) { for(UINT n=0;n<4;n++) { SafeRelease(m_values[indices[n]]); m_values[indices[n]]=old[n].Detach(); } Apply(); }
	Cleanup: return hr;
	}
	STDMETHOD(GetEffectiveBounds)(QuadooForm::Rect* value) { if(value) *value=m_effective; return value ? m_factory->Thread() : E_POINTER; }
	STDMETHOD(GetVisible)(BOOL* value) { return GetValueBool(Index(L"Visible"),value); }
	STDMETHOD(SetVisible)(BOOL value) { return SetValueBool(Index(L"Visible"),value); }
	STDMETHOD(GetEnabled)(BOOL* value) { return GetValueBool(Index(L"Enabled"),value); }
	STDMETHOD(SetEnabled)(BOOL value) { return SetValueBool(Index(L"Enabled"),value); }
	STDMETHOD(GetHandle)(HWND* value) { if(!value) return E_POINTER; *value=NULL; HRESULT hr=Alive(); if(SUCCEEDED(hr)) *value=m_window; return hr; }
	STDMETHOD(SetFocus)(VOID) { HRESULT hr=Alive(); if(SUCCEEDED(hr) && m_window) ::SetFocus(m_window); return hr; }
	STDMETHOD(GetTabOrder)(INT* value) { return GetValueInt(Index(L"TabOrder"),value); }
	STDMETHOD(SetTabOrder)(INT value) { return SetValueInt(Index(L"TabOrder"),value); }
	STDMETHOD(Measure)(QuadooForm::Size* value) { HRESULT hr; QuadooForm::Rect bounds; CheckIf(!value,E_POINTER); Check(GetBounds(&bounds)); value->Width=bounds.Width; value->Height=bounds.Height; Cleanup: return hr; }
	STDMETHOD(Arrange)(const QuadooForm::Rect* value,UINT dpi)
	{
		HRESULT hr; Check(Alive()); CheckIf(!value || dpi<48 || dpi>768 || value->Width<0 || value->Height<0,E_INVALIDARG);
		Check(Resources(dpi)); m_dpi=dpi; m_effective=*value;
		if(m_window) { CheckIf(!SetWindowPos(m_window,NULL,MulDiv(value->Left,dpi,96),MulDiv(value->Top,dpi,96),MulDiv(value->Width,dpi,96),MulDiv(value->Height,dpi,96),SWP_NOZORDER | SWP_NOACTIVATE),HrEnsureLastError()); }
	Cleanup: return hr;
	}
	STDMETHOD(GetDesignCapabilities)(DWORD* value) { if(value) *value=15; return value ? m_factory->Thread() : E_POINTER; }
};
typedef TVisual<IQuadooWinControl> CLeaf;
static QuadooForm::Insets Insets(IQuadooComponent* control,PCWSTR name);
class CContainer;
class CChildObserver : public CBaseUnknown, public IQuadooPropertyObserver
{
	CContainer* m_owner;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooPropertyObserver)
	END_UNK_MAP
	CChildObserver(CContainer* owner) : m_owner(owner) {}
	VOID Detach() { m_owner=NULL; }
	STDMETHOD(OnPropertyChanged)(IQuadooComponent*,UINT);
	STDMETHOD(OnBoundsChanged)(IQuadooControl*,const QuadooForm::Rect*);
};
static HRESULT ValidateID(IQuadooControl* control,REFGUID id,IUnknown* candidate,UINT depth=0)
{
	HRESULT hr; GUID existing; TStackRef<IQuadooControlContainer> container;
	CheckIf(depth>128,QuadooForm::EInvalidDefinition);
	if(QFD::SameObject(control,candidate)) return S_OK;
	Check(control->GetID(&existing)); CheckIf(IsEqualGUID(existing,id),QuadooForm::EDuplicateID);
	hr=control->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&container));
	if(hr==E_NOINTERFACE) hr=S_OK;
	else
	{
		Check(hr); UINT count; Check(container->GetChildCount(&count));
		for(UINT n=0;n<count;n++) { TStackRef<IQuadooControl> child; Check(container->GetChild(n,&child)); Check(ValidateID(child,id,candidate,depth+1)); }
	}
	if(!depth)
	{
		TStackRef<IQuadooComponentHost> host; TStackRef<IQuadooComponentCollection> components;
		hr=control->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&host));
		if(hr==E_NOINTERFACE) hr=S_OK;
		else { Check(hr); UINT count; Check(host->GetComponents(&components)); Check(components->GetCount(&count)); for(UINT n=0;n<count;n++) { TStackRef<IQuadooComponent> component; Check(components->GetAt(n,&component)); Check(component->GetID(&existing)); CheckIf(IsEqualGUID(existing,id),QuadooForm::EDuplicateID); } }
	}
Cleanup: return hr;
}
class CContainer : public TVisual<IQuadooControlContainer>, public IQuadooChildWindowHost, public IQuadooPropertyObserver
{
	struct Child { IQuadooControl* control; CChildObserver* observer; QuadooForm::Rect stored,anchor; QuadooForm::Size size; INT anchors; BOOL anchored; };
	TArray<Child> m_children;
	UINT m_updates;
	BOOL m_layout,m_registering;
protected:
	HRESULT PaintChildren(HDC dc)
	{
		HRESULT hr=S_OK; TArray<IQuadooControl*> children;
		for(sysint n=0;n<m_children.Length();n++) { Check(children.Append(m_children[n].control)); m_children[n].control->AddRef(); }
		for(sysint n=0;n<children.Length();n++)
		{
			TStackRef<IQuadooStaticControl> painted;
			hr=children[n]->QueryInterface(__uuidof(IQuadooStaticControl),reinterpret_cast<PVOID*>(&painted));
			if(hr==E_NOINTERFACE) continue;
			Check(hr); QuadooForm::Rect bounds; Check(children[n]->GetEffectiveBounds(&bounds));
			RECT physical={MulDiv(bounds.Left,m_dpi,96),MulDiv(bounds.Top,m_dpi,96),MulDiv(bounds.Left+bounds.Width,m_dpi,96),MulDiv(bounds.Top+bounds.Height,m_dpi,96)};
			INT saved=SaveDC(dc); CheckIf(!saved,HrEnsureLastError()); hr=painted->Paint(dc,&physical); RestoreDC(dc,saved); Check(hr);
			if(m_state>=QuadooForm::Destroying) break;
		}
		hr=S_OK;
	Cleanup: for(sysint n=0;n<children.Length();n++) children[n]->Release(); return hr;
	}
	HRESULT Detach()
	{
		HRESULT hr=S_OK;
		while(m_children.Length()) { TStackRef<IQuadooControl> child(m_children[m_children.Length()-1].control); HRESULT result=child->Destroy(); if(FAILED(result) && SUCCEEDED(hr)) hr=result; UnregisterControl(child); }
		HRESULT result=TVisual<IQuadooControlContainer>::Detach(); return FAILED(hr) ? hr : result;
	}
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooChildWindowHost)
		UNK_INTERFACE(IQuadooPropertyObserver)
		UNK_CHAIN_TO(TVisual<IQuadooControlContainer>)
	END_UNK_MAP
	CContainer(CFactoryBase* factory,HANDLE lease,IQuadooControlContainer* parent) : TVisual<IQuadooControlContainer>(factory,lease,parent),m_updates(0),m_layout(FALSE),m_registering(FALSE) {}
	STDMETHOD(GetChildHostHandle)(HWND* value) { return GetHandle(value); }
	STDMETHOD(GetChildCount)(UINT* value) { if(value) *value=static_cast<UINT>(m_children.Length()); return value ? m_factory->Thread() : E_POINTER; }
	STDMETHOD(GetChild)(UINT index,IQuadooControl** value) { HRESULT hr; CheckIf(!value,E_POINTER); *value=NULL; Check(m_factory->Thread()); CheckIf(index>=static_cast<UINT>(m_children.Length()),HRESULT_FROM_WIN32(ERROR_INVALID_INDEX)); *value=m_children[index].control; (*value)->AddRef(); Cleanup: return hr; }
	STDMETHOD(FindControl)(RSTRING name,IQuadooControl** value)
	{
		HRESULT hr=HRESULT_FROM_WIN32(ERROR_NOT_FOUND); CRString own;
		CheckIf(!value,E_POINTER); *value=NULL; Check(m_factory->Thread()); Check(GetName(&own));
		if(own==name) { *value=static_cast<IQuadooControlContainer*>(this); AddRef(); goto Cleanup; }
		for(sysint n=0;n<m_children.Length();n++)
		{
			CRString text; Check(m_children[n].control->GetName(&text));
			if(text==name) { *value=m_children[n].control; (*value)->AddRef(); hr=S_OK; goto Cleanup; }
			TStackRef<IQuadooControlContainer> container;
			if(SUCCEEDED(m_children[n].control->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&container)))) { hr=container->FindControl(name,value); if(hr!=HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) goto Cleanup; }
		}
		hr=HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	Cleanup: return hr;
	}
	STDMETHOD(RegisterControl)(IQuadooControl* control)
	{
		HRESULT hr; TStackRef<IQuadooControlContainer> parent; TStackRef<IQuadooPropertyNotifications> notifications;
		CRString name; GUID id; QuadooForm::ComponentState state; Child child; BOOL observing=FALSE,entered=FALSE;
		TStackRef<CChildObserver> observer; TStackRef<IQuadooControl> root;
		Check(Alive()); CheckIf(!control || m_registering,QuadooForm::EInvalidState); m_registering=TRUE; entered=TRUE;
		Check(control->GetParent(&parent)); CheckIf(!QFD::SameObject(parent,static_cast<IQuadooControlContainer*>(this)),E_INVALIDARG);
		Check(control->GetState(&state)); CheckIf(state>=QuadooForm::Destroying,QuadooForm::EDestroyed);
		Check(control->GetName(&name)); Check(QuadooValidateControlName(control,name));
		Check(control->GetID(&id)); CheckIf(IsEqualGUID(id,GUID_NULL),E_INVALIDARG);
		root=static_cast<IQuadooControlContainer*>(this);
		for(UINT depth=0;;depth++) { TStackRef<IQuadooControlContainer> owner; CheckIf(depth>128,QuadooForm::EInvalidDefinition); Check(root->GetParent(&owner)); if(!owner) break; root=owner; }
		Check(ValidateID(root,id,control));
		Check(control->GetBounds(&child.stored)); child.anchor=child.stored; child.size.Width=child.size.Height=0; child.anchors=0; child.anchored=FALSE; child.control=control;
		for(sysint n=0;n<m_children.Length();n++) { CheckIf(QFD::SameObject(control,m_children[n].control),HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)); }
		hr=control->QueryInterface(__uuidof(IQuadooPropertyNotifications),reinterpret_cast<PVOID*>(&notifications));
		if(hr!=E_NOINTERFACE) { Check(hr); observer.Attach(__new CChildObserver(this)); CheckAlloc(observer); observing=TRUE; Check(notifications->AddObserver(observer)); }
		child.observer=observer;
		Check(Alive()); Check(m_children.Append(child)); control->AddRef(); observer.Detach(); observing=FALSE;
	Cleanup:
		if(observing) { observer->Detach(); notifications->RemoveObserver(observer); } if(entered) m_registering=FALSE;
		return hr;
	}
	STDMETHOD(UnregisterControl)(IQuadooControl* control)
	{
		HRESULT hr; Check(m_factory->Thread()); CheckIf(m_registering,QuadooForm::EInvalidState);
		for(sysint n=0;n<m_children.Length();n++) if(QFD::SameObject(control,m_children[n].control))
		{
			TStackRef<IQuadooControl> keep(control); TStackRef<IQuadooPropertyNotifications> notifications;
			CChildObserver* observer=m_children[n].observer; if(observer) observer->Detach();
			m_children.Remove(n,NULL); if(observer && SUCCEEDED(control->QueryInterface(__uuidof(IQuadooPropertyNotifications),reinterpret_cast<PVOID*>(&notifications)))) notifications->RemoveObserver(observer);
			SafeRelease(observer);
			control->Release(); hr=S_OK; goto Cleanup;
		}
		hr=HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	Cleanup: return hr;
	}
	STDMETHOD(BeginUpdate)(VOID) { HRESULT hr=Alive(); if(SUCCEEDED(hr)) m_updates++; return hr; }
	STDMETHOD(EndUpdate)(VOID) { HRESULT hr; Check(Alive()); CheckIf(!m_updates,QuadooForm::EInvalidState); m_updates--; Check(UpdateLayout()); Cleanup: return hr; }
	STDMETHOD(OnPropertyChanged)(IQuadooComponent*,UINT) { return UpdateLayout(); }
	STDMETHOD(OnBoundsChanged)(IQuadooControl*,const QuadooForm::Rect*) { return UpdateLayout(); }
	STDMETHOD(Arrange)(const QuadooForm::Rect* value,UINT dpi) { HRESULT hr; Check(TVisual<IQuadooControlContainer>::Arrange(value,dpi)); Check(UpdateLayout()); Cleanup: return hr; }
	STDMETHOD(Measure)(QuadooForm::Size* size)
	{
		HRESULT hr; QuadooForm::Insets padding; CheckIf(!size,E_POINTER); Check(Alive());
		padding=Insets(Component(),L"Padding"); size->Width=padding.Left+padding.Right; size->Height=padding.Top+padding.Bottom;
		for(sysint n=0;n<m_children.Length();n++)
		{
			TStackRef<IQuadooControl> child(m_children[n].control); QuadooForm::Rect bounds; BOOL visible;
			Check(child->GetVisible(&visible)); if(!visible) continue; Check(child->GetBounds(&bounds));
			QuadooForm::Insets margins=Insets(child,L"Margins");
			if(QFD::BoolProperty(child,L"AutoSize"))
			{
				TStackRef<IQuadooLayoutControl> layout;
				if(SUCCEEDED(child->QueryInterface(__uuidof(IQuadooLayoutControl),reinterpret_cast<PVOID*>(&layout)))) { QuadooForm::Size measured; Check(layout->Measure(&measured)); bounds.Width=measured.Width; bounds.Height=measured.Height; }
			}
			size->Width=max(size->Width,bounds.Left+bounds.Width+margins.Right+padding.Right);
			size->Height=max(size->Height,bounds.Top+bounds.Height+margins.Bottom+padding.Bottom);
		}
		size->Width=max(size->Width,QFD::IntProperty(Component(),L"MinWidth")); size->Height=max(size->Height,QFD::IntProperty(Component(),L"MinHeight"));
		{ INT maximum=QFD::IntProperty(Component(),L"MaxWidth"); if(maximum) size->Width=min(size->Width,maximum); maximum=QFD::IntProperty(Component(),L"MaxHeight"); if(maximum) size->Height=min(size->Height,maximum); }
	Cleanup: return hr;
	}
	STDMETHOD(UpdateLayout)(VOID);
};
STDMETHODIMP CChildObserver::OnPropertyChanged(IQuadooComponent*,UINT) { TStackRef<CContainer> keep(m_owner); return keep ? keep->UpdateLayout() : S_OK; }
STDMETHODIMP CChildObserver::OnBoundsChanged(IQuadooControl*,const QuadooForm::Rect*) { TStackRef<CContainer> keep(m_owner); return keep ? keep->UpdateLayout() : S_OK; }
static QuadooForm::Insets Insets(IQuadooComponent* control,PCWSTR name)
{
	QuadooForm::Insets result={0,0,0,0}; TStackRef<IJSONValue> value; TStackRef<IJSONObject> object;
	if(SUCCEEDED(QFD::ReadProperty(control,name,&value)) && value && SUCCEEDED(value->GetObject(&object))) { ReadInt(object,L"Left",&result.Left); ReadInt(object,L"Top",&result.Top); ReadInt(object,L"Right",&result.Right); ReadInt(object,L"Bottom",&result.Bottom); }
	return result;
}
STDMETHODIMP CContainer::UpdateLayout()
{
	HRESULT hr=S_OK; BOOL entered=FALSE; RECT client; QuadooForm::Insets padding; QuadooForm::Rect available; QuadooForm::Size size; UINT clients=0;
	Check(Alive()); CheckIfIgnore(m_updates || m_layout || !m_window,S_FALSE); m_layout=TRUE; entered=TRUE;
	GetClientRect(m_window,&client); size.Width=MulDiv(client.right,96,m_dpi); size.Height=MulDiv(client.bottom,96,m_dpi); padding=Insets(Component(),L"Padding");
	available.Left=padding.Left; available.Top=padding.Top; available.Width=max(0,size.Width-padding.Left-padding.Right); available.Height=max(0,size.Height-padding.Top-padding.Bottom);
	for(sysint n=0;n<m_children.Length();n++)
	{
		Child& entry=m_children[n]; TStackRef<IQuadooControl> child(entry.control); QuadooForm::Rect bounds; BOOL visible;
		Check(child->GetBounds(&bounds)); Check(child->GetVisible(&visible)); QuadooForm::Insets margin=Insets(child,L"Margins");
		INT align=QFD::IntProperty(child,L"Align"),anchors=QFD::IntProperty(child,L"Anchors",3);
		if(entry.anchored && (memcmp(&entry.stored,&bounds,sizeof(bounds)) || entry.anchors!=anchors)) entry.anchored=FALSE;
		entry.stored=bounds; entry.anchors=anchors;
		if(align==QuadooForm::AlignClient) { CheckIf(++clients>1,E_INVALIDARG); }
		if(QFD::BoolProperty(child,L"AutoSize")) { TStackRef<IQuadooLayoutControl> layout; if(SUCCEEDED(child->QueryInterface(__uuidof(IQuadooLayoutControl),reinterpret_cast<PVOID*>(&layout)))) { QuadooForm::Size measured; Check(layout->Measure(&measured)); bounds.Width=measured.Width; bounds.Height=measured.Height; } }
		if(align && visible)
		{
			bounds.Left=available.Left+margin.Left; bounds.Top=available.Top+margin.Top;
			if(align==QuadooForm::AlignTop || align==QuadooForm::AlignBottom)
			{
				bounds.Width=max(0,available.Width-margin.Left-margin.Right); INT used=min(available.Height,bounds.Height+margin.Top+margin.Bottom);
				if(align==QuadooForm::AlignBottom) bounds.Top=available.Top+available.Height-margin.Bottom-bounds.Height; else available.Top+=used; available.Height-=used;
			}
			else if(align==QuadooForm::AlignLeft || align==QuadooForm::AlignRight)
			{
				bounds.Height=max(0,available.Height-margin.Top-margin.Bottom); INT used=min(available.Width,bounds.Width+margin.Left+margin.Right);
				if(align==QuadooForm::AlignRight) bounds.Left=available.Left+available.Width-margin.Right-bounds.Width; else available.Left+=used; available.Width-=used;
			}
			else { bounds.Width=max(0,available.Width-margin.Left-margin.Right); bounds.Height=max(0,available.Height-margin.Top-margin.Bottom); }
		}
		else
		{
			if(!entry.anchored) { entry.anchor=bounds; entry.size=size; entry.anchored=TRUE; }
			INT dx=size.Width-entry.size.Width,dy=size.Height-entry.size.Height; bounds=entry.anchor;
			if((anchors&5)==5) bounds.Width=max(0,bounds.Width+dx); else if(anchors&4) bounds.Left+=dx; else if(!(anchors&1)) bounds.Left+=dx/2;
			if((anchors&10)==10) bounds.Height=max(0,bounds.Height+dy); else if(anchors&8) bounds.Top+=dy; else if(!(anchors&2)) bounds.Top+=dy/2;
		}
		bounds.Width=max(bounds.Width,QFD::IntProperty(child,L"MinWidth")); bounds.Height=max(bounds.Height,QFD::IntProperty(child,L"MinHeight"));
		INT maximum=QFD::IntProperty(child,L"MaxWidth"); if(maximum) bounds.Width=min(bounds.Width,maximum); maximum=QFD::IntProperty(child,L"MaxHeight"); if(maximum) bounds.Height=min(bounds.Height,maximum);
		TStackRef<IQuadooLayoutControl> layout;
		hr=child->QueryInterface(__uuidof(IQuadooLayoutControl),reinterpret_cast<PVOID*>(&layout));
		if(hr==E_NOINTERFACE)
		{
			TStackRef<IQuadooWinControl> native; HWND handle;
			hr=child->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native));
			if(hr!=E_NOINTERFACE) { Check(hr); Check(native->GetHandle(&handle)); CheckIf(!SetWindowPos(handle,NULL,MulDiv(bounds.Left,m_dpi,96),MulDiv(bounds.Top,m_dpi,96),MulDiv(bounds.Width,m_dpi,96),MulDiv(bounds.Height,m_dpi,96),SWP_NOACTIVATE | SWP_NOZORDER),HrEnsureLastError()); }
		}
		else { Check(hr); Check(layout->Arrange(&bounds,m_dpi)); }
	}
Cleanup: if(entered) m_layout=FALSE; return hr;
}
class CVisualFactory : public CFactoryBase, public IQuadooControlFactory, public IQuadooDesignFactory
{
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooControlFactory)
		UNK_INTERFACE(IQuadooDesignFactory)
		UNK_CHAIN_TO(CFactoryBase)
	END_UNK_MAP
	CVisualFactory(CDefinition* value,PCWSTR directory) : CFactoryBase(value,directory) {}
	STDMETHOD(GetTypeName)(RSTRING* value) { HRESULT hr; CheckIf(!value,E_POINTER); *value=NULL; Check(Thread()); Check(CopyString(definition->type,value)); Cleanup: return hr; }
	STDMETHOD(GetProperties)(IQuadooPropertyDescriptors** value) { HRESULT hr; CheckIf(!value,E_POINTER); *value=NULL; Check(Thread()); Check(definition->GetProperties(value)); Cleanup: return hr; }
	STDMETHOD(GetEvents)(IQuadooEventDescriptors** value) { HRESULT hr; CheckIf(!value,E_POINTER); *value=NULL; Check(Thread()); Check(definition->GetEvents(value)); Cleanup: return hr; }
	STDMETHOD(CanCreate)(IQuadooControlContainer* parent,BOOL* allowed) { if(!allowed) return E_POINTER; *allowed=parent!=NULL; TStackRef<IQuadooTabControl> tabs; if(parent && SUCCEEDED(parent->QueryInterface(__uuidof(IQuadooTabControl),reinterpret_cast<PVOID*>(&tabs)))) *allowed=FALSE; return Thread(); }
	STDMETHOD(CreateControl)(IQuadooConstructionContext* context,IQuadooControlContainer* parent,IJSONObject* input,IQuadooControl** result)
	{
		HRESULT hr; HANDLE lease=NULL; BOOL allowed; TStackRef<CObject> object;
		CheckIf(!result,E_POINTER); *result=NULL; CheckIf(!input,E_INVALIDARG); Check(CanCreate(parent,&allowed)); CheckIf(!allowed,E_INVALIDARG); Check(Enter(context,&lease));
		if(definition->container)
		{
			CContainer* instance=__new CContainer(this,lease,parent); CheckAlloc(instance); lease=NULL; object.Attach(instance); Check(instance->Construct(input)); Check(instance->Attach(context)); Check(instance->QueryInterface(__uuidof(IQuadooControl),reinterpret_cast<PVOID*>(result)));
		}
		else
		{
			CLeaf* instance=__new CLeaf(this,lease,parent); CheckAlloc(instance); lease=NULL; object.Attach(instance); Check(instance->Construct(input)); Check(instance->Attach(context)); Check(instance->QueryInterface(__uuidof(IQuadooControl),reinterpret_cast<PVOID*>(result)));
		}
	Cleanup: if(FAILED(hr) && object) object->AbortConstruction(); if(lease) CloseHandle(lease); return hr;
	}
};
class CComponentFactory : public CFactoryBase, public IQuadooComponentFactory
{
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooComponentFactory)
		UNK_CHAIN_TO(CFactoryBase)
	END_UNK_MAP
	CComponentFactory(CDefinition* value,PCWSTR directory) : CFactoryBase(value,directory) {}
	STDMETHOD(GetTypeName)(RSTRING* value) { HRESULT hr; CheckIf(!value,E_POINTER); *value=NULL; Check(Thread()); Check(CopyString(definition->type,value)); Cleanup: return hr; }
	STDMETHOD(GetProperties)(IQuadooPropertyDescriptors** value) { HRESULT hr; CheckIf(!value,E_POINTER); *value=NULL; Check(Thread()); Check(definition->GetProperties(value)); Cleanup: return hr; }
	STDMETHOD(GetEvents)(IQuadooEventDescriptors** value) { HRESULT hr; CheckIf(!value,E_POINTER); *value=NULL; Check(Thread()); Check(definition->GetEvents(value)); Cleanup: return hr; }
	STDMETHOD(CreateComponent)(IQuadooConstructionContext* context,IQuadooComponentCollection* owner,IJSONObject* input,IQuadooComponent** result)
	{
		HRESULT hr; HANDLE lease=NULL; TStackRef<CNonvisual> object;
		CheckIf(!result,E_POINTER); *result=NULL; CheckIf(!owner || !input,E_INVALIDARG); Check(Enter(context,&lease));
		object.Attach(__new CNonvisual(this,lease,owner)); CheckAlloc(object); lease=NULL; Check(object->Construct(input)); Check(owner->RegisterComponent(object)); *result=static_cast<IQuadooComponent*>(object); (*result)->AddRef();
	Cleanup: if(FAILED(hr) && object) object->AbortConstruction(); if(lease) CloseHandle(lease); return hr;
	}
};
HRESULT CreateFactory(CDefinition* definition,PCWSTR directory,IUnknown** factory)
{
	HRESULT hr=S_OK; CFactoryBase* object=NULL;
	CheckIf(!definition || !directory || !factory,E_POINTER); *factory=NULL;
	if(definition->visual) object=__new CVisualFactory(definition,directory); else object=__new CComponentFactory(definition,directory);
	CheckAlloc(object); Check(object->initialization); *factory=object->GetControllingUnknown(); object=NULL;
Cleanup: SafeRelease(object); return hr;
}
VOID RetireFactory(IUnknown* factory)
{
	// Every catalog-owned factory is allocated above; never cast unrelated factories.
	TStackRef<IQuadooFactoryLifetime> lifetime;
	if(factory && SUCCEEDED(factory->QueryInterface(__uuidof(IQuadooFactoryLifetime),reinterpret_cast<PVOID*>(&lifetime)))) static_cast<CFactoryBase*>(static_cast<IQuadooFactoryLifetime*>(lifetime))->active=FALSE;
}
}
