#include "Runtime.h"
#include "ComponentRuntime.h"

namespace QF
{
class CPlaceholderObject : public CComponentObject, public IQuadooDesignPlaceholder
{
	QuadooForm::PlaceholderReason m_reason;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooDesignPlaceholder)
		UNK_CHAIN_TO(CComponentObject)
	END_UNK_MAP
	CPlaceholderObject(IQuadooConstructionContext* context, IQuadooComponentCollection* owner, IUnknown* factory, QuadooForm::PlaceholderReason reason) : CComponentObject(context,owner,factory), m_reason(reason) {}
	STDMETHOD(GetReason)(QuadooForm::PlaceholderReason* reason, RSTRING* detail)
	{
		HRESULT hr;
		CheckIf(!reason || !detail,E_POINTER); *detail = NULL;
		Check(CheckThread()); *reason = m_reason;
		PCWSTR text = m_reason == QuadooForm::MissingFactory ? L"Factory is not registered; original definition is preserved." : m_reason == QuadooForm::ParentDesignUnavailable ? L"Parent is a design representation; native child layout is unavailable." : L"Factory explicitly does not support Design construction; original definition is preserved.";
		Check(RStrCreateW(TStrLenAssert(text),text,detail));
	Cleanup:
		return hr;
	}
};
class CNonvisualPlaceholder : public CPlaceholderObject, public IQuadooNonvisualComponent
{
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooNonvisualComponent)
		UNK_CHAIN_TO(CPlaceholderObject)
	END_UNK_MAP
	CNonvisualPlaceholder(IQuadooConstructionContext* context, IQuadooComponentCollection* owner, IUnknown* factory, QuadooForm::PlaceholderReason reason) : CPlaceholderObject(context,owner,factory,reason) {}
	STDMETHOD(GetOwner)(IQuadooComponentCollection** owner)
	{
		HRESULT hr; CheckIf(!owner,E_POINTER); *owner = NULL;
		Check(CheckThread()); *owner = m_owner; if(*owner) (*owner)->AddRef();
	Cleanup:
		return hr;
	}
};
class CVisualPlaceholder : public CPlaceholderObject, public IQuadooControlContainer, public IQuadooChildWindowHost, public IQuadooDesignCapabilities, public IQuadooLayoutControl
{
	IQuadooControlContainer* m_parent;
	HWND m_window;
	TArray<IQuadooControl*> m_children;
	HRESULT Position()
	{
		HRESULT hr = S_OK;
		UINT dpi = ControlDPI(m_parent);
		if(m_window) { CheckIf(!MoveWindow(m_window,MulDiv(Integer(L"Left"),dpi,96),MulDiv(Integer(L"Top"),dpi,96),MulDiv(Integer(L"Width"),dpi,96),MulDiv(Integer(L"Height"),dpi,96),TRUE),HrEnsureLastError()); }
	Cleanup:
		return hr;
	}
protected:
	virtual HRESULT ChangedProperty(PCWSTR name) { return Position(); }
	virtual HRESULT ValidateName(RSTRING name) { return m_parent ? QuadooValidateControlName(static_cast<IQuadooControlContainer*>(this),name) : S_OK; }
	virtual HRESULT DestroyObject()
	{
		HRESULT hr = S_OK;
		while(m_children.Length())
		{
			TStackRef<IQuadooControl> child(m_children[m_children.Length() - 1]);
			HRESULT destroy = child->Destroy(); if(FAILED(destroy) && SUCCEEDED(hr)) hr = destroy;
			for(sysint n = m_children.Length(); n > 0; n--) if(SameObject(child,m_children[n - 1])) { m_children[n - 1]->Release(); m_children.Remove(n - 1,NULL); }
		}
		if(m_window) { DestroyWindow(m_window); m_window = NULL; }
		if(m_parent) { IQuadooControlContainer* parent = m_parent; m_parent = NULL; HRESULT removed = parent->UnregisterControl(this); if(FAILED(removed) && removed != HRESULT_FROM_WIN32(ERROR_NOT_FOUND) && SUCCEEDED(hr)) hr = removed; }
		return hr;
	}
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE_PATH(IQuadooComponent,IQuadooControlContainer)
		UNK_INTERFACE_PATH(IQuadooControl,IQuadooControlContainer)
		UNK_INTERFACE_PATH(IQuadooWinControl,IQuadooControlContainer)
		UNK_INTERFACE(IQuadooControlContainer)
		UNK_INTERFACE(IQuadooChildWindowHost)
		UNK_INTERFACE(IQuadooDesignCapabilities)
		UNK_INTERFACE(IQuadooLayoutControl)
		UNK_CHAIN_TO(CPlaceholderObject)
	END_UNK_MAP
	CVisualPlaceholder(IQuadooConstructionContext* context, IQuadooControlContainer* parent, IUnknown* factory, QuadooForm::PlaceholderReason reason) : CPlaceholderObject(context,NULL,factory,reason), m_parent(parent), m_window(NULL) {}
	~CVisualPlaceholder() { DestroyObject(); }
	HRESULT CreateRepresentation()
	{
		HRESULT hr; HWND parent; TStackRef<IQuadooChildWindowHost> host; CRString label;
		CheckIf(!m_parent,E_INVALIDARG);
		if(SUCCEEDED(m_parent->QueryInterface(__uuidof(IQuadooChildWindowHost),reinterpret_cast<PVOID*>(&host)))) { Check(host->GetChildHostHandle(&parent)); }
		else { Check(m_parent->GetHandle(&parent)); }
		Check(RStrFormatW(&label,L"%ls (%ls)\r\nDesign representation",m_name.c_wstr(),m_type.c_wstr()));
		m_window = CreateWindowExW(0,L"STATIC",label.c_wstr(),WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SS_CENTER,0,0,1,1,parent,NULL,g_hInstance,NULL);
		CheckIf(!m_window,HrEnsureLastError());
		SendMessageW(m_window,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),FALSE);
		Check(Position());
		Check(m_parent->RegisterControl(this));
	Cleanup:
		return hr;
	}
	STDMETHOD(GetID)(GUID* id) { return CComponentObject::GetID(id); }
	STDMETHOD(GetName)(RSTRING* name) { return CComponentObject::GetName(name); }
	STDMETHOD(SetName)(RSTRING name) { return CComponentObject::SetName(name); }
	STDMETHOD(GetTypeName)(RSTRING* name) { return CComponentObject::GetTypeName(name); }
	STDMETHOD(GetProperties)(IQuadooProperties** properties) { return CComponentObject::GetProperties(properties); }
	STDMETHOD(GetState)(QuadooForm::ComponentState* state) { return CComponentObject::GetState(state); }
	STDMETHOD(Destroy)(VOID) { return CComponentObject::Destroy(); }
	STDMETHOD(GetParent)(IQuadooControlContainer** parent) { HRESULT hr; CheckIf(!parent,E_POINTER); *parent = NULL; Check(CheckThread()); *parent = m_parent; if(*parent) (*parent)->AddRef(); Cleanup: return hr; }
	STDMETHOD(GetBounds)(QuadooForm::Rect* bounds) { HRESULT hr; CheckIf(!bounds,E_POINTER); Check(CheckThread()); bounds->Left = Integer(L"Left"); bounds->Top = Integer(L"Top"); bounds->Width = Integer(L"Width"); bounds->Height = Integer(L"Height"); Cleanup: return hr; }
	STDMETHOD(GetEffectiveBounds)(QuadooForm::Rect* bounds) { return GetBounds(bounds); }
	STDMETHOD(SetBounds)(const QuadooForm::Rect* bounds)
	{
		HRESULT hr;
		TStackRef<IJSONValue> values[4]; const PCWSTR names[] = { L"Left",L"Top",L"Width",L"Height" };
		Check(CheckAlive()); CheckIf(!bounds,E_POINTER);
		for(UINT n = 0; n < 4; n++)
		{
			TStackRef<IJSONValue> input; PropertySpec spec = { names[n],QuadooProperty::Int,QuadooForm::PropertySerializable | QuadooForm::PropertyAffectsLayout,0,0 };
			Check(JSONCreateInteger(reinterpret_cast<const INT*>(bounds)[n],&input));
			Check(NormalizeValue(&spec,input,&values[n]));
			CheckIf(Find(names[n]) >= static_cast<UINT>(m_values.Length()),DISP_E_MEMBERNOTFOUND);
		}
		for(UINT n = 0; n < 4; n++) { UINT index = Find(names[n]); ReplaceInterface(m_values[index].value,static_cast<IJSONValue*>(values[n])); m_values[index].edited = TRUE; }
		Check(Position());
		for(UINT n = 0; n < 4; n++) { Check(Notify(Find(names[n]))); }
	Cleanup:
		return hr;
	}
	STDMETHOD(GetVisible)(BOOL* visible) { HRESULT hr; CheckIf(!visible,E_POINTER); Check(CheckThread()); *visible = Boolean(L"Visible"); Cleanup: return hr; }
	STDMETHOD(GetEnabled)(BOOL* enabled) { HRESULT hr; CheckIf(!enabled,E_POINTER); Check(CheckThread()); *enabled = Boolean(L"Enabled"); Cleanup: return hr; }
	STDMETHOD(SetVisible)(BOOL visible) { return SetValueBool(Find(L"Visible"),visible); }
	STDMETHOD(SetEnabled)(BOOL enabled) { return SetValueBool(Find(L"Enabled"),enabled); }
	STDMETHOD(GetHandle)(HWND* window) { HRESULT hr; CheckIf(!window,E_POINTER); *window = NULL; Check(CheckAlive()); *window = m_window; Cleanup: return hr; }
	STDMETHOD(GetChildHostHandle)(HWND* window) { return GetHandle(window); }
	STDMETHOD(SetFocus)(VOID) { return E_NOTIMPL; }
	STDMETHOD(GetTabOrder)(INT* order) { HRESULT hr; CheckIf(!order,E_POINTER); Check(CheckThread()); *order = Integer(L"TabOrder"); Cleanup: return hr; }
	STDMETHOD(SetTabOrder)(INT order) { return SetValueInt(Find(L"TabOrder"),order); }
	STDMETHOD(GetChildCount)(UINT* count) { HRESULT hr; CheckIf(!count,E_POINTER); Check(CheckThread()); *count = static_cast<UINT>(m_children.Length()); Cleanup: return hr; }
	STDMETHOD(GetChild)(UINT index, IQuadooControl** control) { HRESULT hr; CheckIf(!control,E_POINTER); *control = NULL; Check(CheckThread()); CheckIf(index >= static_cast<UINT>(m_children.Length()),HRESULT_FROM_WIN32(ERROR_INVALID_INDEX)); *control = m_children[index]; (*control)->AddRef(); Cleanup: return hr; }
	STDMETHOD(FindControl)(RSTRING name, IQuadooControl** control)
	{
		HRESULT hr; TRStrMap<IQuadooControl*> names(RStrCompareRStr); TArray<GUID> ids;
		CheckIf(!control,E_POINTER); *control = NULL; Check(CheckAlive());
		Check(IndexTree(this,names,ids)); Check(names.Find(name,control)); (*control)->AddRef();
	Cleanup:
		return hr;
	}
	STDMETHOD(RegisterControl)(IQuadooControl* control)
	{
		HRESULT hr; CRString name; GUID id; TStackRef<IQuadooControlContainer> parent;
		Check(CheckAlive()); CheckIf(!control,E_INVALIDARG);
		Check(control->GetParent(&parent)); CheckIf(!SameObject(parent,static_cast<IQuadooControlContainer*>(this)),E_INVALIDARG);
		Check(control->GetName(&name)); Check(control->GetID(&id)); Check(ValidateCandidate(this,name,id,FALSE));
		Check(m_children.Append(control)); control->AddRef();
	Cleanup:
		return hr;
	}
	STDMETHOD(UnregisterControl)(IQuadooControl* control)
	{
		HRESULT hr; Check(CheckThread());
		for(sysint n = 0; n < m_children.Length(); n++) if(SameObject(control,m_children[n])) { IQuadooControl* previous = m_children[n]; m_children.Remove(n,NULL); previous->Release(); hr = S_OK; goto Cleanup; }
		hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	Cleanup:
		return hr;
	}
	STDMETHOD(BeginUpdate)(VOID) { return CheckAlive(); }
	STDMETHOD(EndUpdate)(VOID) { return CheckAlive(); }
	STDMETHOD(UpdateLayout)(VOID) { return CheckAlive(); }
	STDMETHOD(Measure)(QuadooForm::Size* size) { if(!size) return E_POINTER; size->Width = Integer(L"Width"); size->Height = Integer(L"Height"); return CheckAlive(); }
	STDMETHOD(Arrange)(const QuadooForm::Rect* bounds, UINT dpi) { return CheckAlive(); } // No inferred unavailable layout.
	STDMETHOD(GetDesignCapabilities)(DWORD* flags)
	{
		HRESULT hr; CheckIf(!flags,E_POINTER); *flags = 0; Check(CheckAlive());
		// Only known integer geometry is exposed, never specialized capabilities.
		if(Find(L"Left") < static_cast<UINT>(m_values.Length()) && Find(L"Top") < static_cast<UINT>(m_values.Length())) *flags |= QuadooForm::DesignMoveX | QuadooForm::DesignMoveY;
		if(Find(L"Width") < static_cast<UINT>(m_values.Length())) *flags |= QuadooForm::DesignResizeWidth;
		if(Find(L"Height") < static_cast<UINT>(m_values.Length())) *flags |= QuadooForm::DesignResizeHeight;
	Cleanup:
		return hr;
	}
};

HRESULT CreateDesignPlaceholder(IQuadooConstructionContext* context, IQuadooControlContainer* parent, IQuadooComponentCollection* owner,
	IJSONObject* definition, IQuadooPropertyDescriptors* properties, IQuadooEventDescriptors* events, IUnknown* factory, QuadooForm::PlaceholderReason reason, IQuadooComponent** result)
{
	HRESULT hr;
	TStackRef<IQuadooPropertyDescriptors> fallbackProperties; TStackRef<IQuadooEventDescriptors> fallbackEvents;
	TStackRef<CNonvisualPlaceholder> nonvisual; TStackRef<CVisualPlaceholder> visual;
	CheckIf(!result,E_POINTER); *result = NULL;
	if(!properties || !events)
	{
		TArray<const PropertySpec*> specs; TArray<const EventSpec*> eventSpecs;
		static const PropertySpec name = { L"Name",QuadooProperty::String,QuadooForm::PropertySerializable,0,0 };
		if(owner) { Check(specs.Append(&name)); }
		else
		{
			static const DWORD stored = QuadooForm::PropertySerializable, layout = stored | QuadooForm::PropertyAffectsLayout;
			static const PropertySpec generic[] =
			{
				{ L"Name",QuadooProperty::String,stored,0,0 }, { L"Left",QuadooProperty::Int,layout,0,1 }, { L"Top",QuadooProperty::Int,layout,0,2 },
				{ L"Width",QuadooProperty::Int,layout,100,3 }, { L"Height",QuadooProperty::Int,layout,28,4 },
				{ L"Visible",QuadooProperty::Boolean,layout,1,5 }, { L"Enabled",QuadooProperty::Boolean,stored,1,6 }, { L"TabOrder",QuadooProperty::Int,layout,0,7 }
			};
			for(UINT n = 0; n < ARRAYSIZE(generic); n++) { Check(specs.Append(&generic[n])); }
		}
		Check(CreateDescriptors(specs,eventSpecs,&fallbackProperties)); Check(CreateEventDescriptors(eventSpecs,&fallbackEvents));
		properties = fallbackProperties; events = fallbackEvents;
	}
	if(owner)
	{
		nonvisual.Attach(__new CNonvisualPlaceholder(context,owner,factory,reason)); CheckAlloc(nonvisual);
		Check(nonvisual->Construct(definition,properties,events)); Check(owner->RegisterComponent(nonvisual));
		*result = nonvisual.Detach();
	}
	else
	{
		visual.Attach(__new CVisualPlaceholder(context,parent,factory,reason)); CheckAlloc(visual);
		Check(visual->Construct(definition,properties,events)); Check(visual->CreateRepresentation());
		Check(visual->QueryInterface(__uuidof(IQuadooComponent),reinterpret_cast<PVOID*>(result)));
	}
Cleanup:
	if(FAILED(hr)) { if(nonvisual) nonvisual->AbortConstruction(); if(visual) visual->AbortConstruction(); }
	return hr;
}
}
