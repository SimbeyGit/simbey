#include "Runtime.h"

namespace QF
{
CEventArgs::CEventArgs(const EventSpec* spec, IQuadooComponent* sender) : m_spec(spec), m_thread(GetCurrentThreadId())
{
	SetInterface(m_sender, sender);
}
HRESULT CEventArgs::Initialize()
{
	HRESULT hr = S_OK;
	for(UINT n = 0; n < m_spec->count; n++) { Value value; ZeroMemory(&value, sizeof(value)); Check(m_values.Append(value)); }
Cleanup:
	return hr;
}
	CEventArgs::~CEventArgs()
	{
		for(sysint n = 0; n < m_values.Length(); n++)
		{
			RStrRelease(m_values[n].text);
			SafeRelease(m_values[n].component);
			SafeRelease(m_values[n].object);
		}
		SafeRelease(m_sender);
	}
VOID CEventArgs::SetNativeValue(UINT index, LONGLONG value) { if(index < m_spec->count) m_values[index].number = value; }
BOOL CEventArgs::Cancelled() const
{
	BOOL cancelled = FALSE;
	for(UINT n = 0; n < m_spec->count; n++)
		if(0 == TStrCmpAssert(m_spec->args[n].name, L"Cancel") || 0 == TStrCmpAssert(m_spec->args[n].name, L"Handled"))
			cancelled = m_values[n].number != 0;
	return cancelled;
}
HRESULT CEventArgs::Validate(UINT index, QuadooForm::EventValueType type, BOOL write)
{
	HRESULT hr = S_OK;
	CheckIf(m_thread != GetCurrentThreadId(), QuadooForm::EWrongThread);
	CheckIf(index >= m_spec->count, HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
	CheckIf(m_spec->args[index].type != type, DISP_E_TYPEMISMATCH);
	CheckIf(write && !m_spec->args[index].writable, E_ACCESSDENIED);
Cleanup:
	return hr;
}
STDMETHODIMP CEventArgs::GetEventName(RSTRING* name) { return name ? RStrCreateW(TStrLenAssert(m_spec->name), m_spec->name, name) : E_POINTER; }
STDMETHODIMP CEventArgs::GetSender(IQuadooComponent** sender)
{
	HRESULT hr = sender ? S_OK : E_POINTER;
	if(sender) { *sender = m_sender; (*sender)->AddRef(); }
	return hr;
}
STDMETHODIMP CEventArgs::GetArgumentCount(UINT* count) { if(count) *count = m_spec->count; return count ? S_OK : E_POINTER; }
STDMETHODIMP CEventArgs::GetArgumentName(UINT index, RSTRING* name)
{
	HRESULT hr;
	CheckIf(!name, E_POINTER); *name = NULL;
	CheckIf(index >= m_spec->count, HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
	Check(RStrCreateW(TStrLenAssert(m_spec->args[index].name), m_spec->args[index].name, name));
Cleanup:
	return hr;
}
STDMETHODIMP CEventArgs::GetArgumentType(UINT index, QuadooForm::EventValueType* type)
{
	HRESULT hr = S_OK;
	CheckIf(!type, E_POINTER);
	CheckIf(index >= m_spec->count, HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
	*type = m_spec->args[index].type;
Cleanup:
	return hr;
}
STDMETHODIMP CEventArgs::GetString(UINT index, RSTRING* value)
{
	HRESULT hr;
	CheckIf(!value, E_POINTER); *value = NULL;
	Check(Validate(index, QuadooForm::ValueString, FALSE)); RStrSet(*value, m_values[index].text);
Cleanup:
	return hr;
}
STDMETHODIMP CEventArgs::SetString(UINT index, RSTRING value)
{
	HRESULT hr;
	Check(Validate(index, QuadooForm::ValueString, TRUE));
	RStrReplace(&m_values[index].text, value);
Cleanup:
	return hr;
}
STDMETHODIMP CEventArgs::GetComponent(UINT index, IQuadooComponent** value)
{
	HRESULT hr;
	CheckIf(!value, E_POINTER); *value = NULL;
	Check(Validate(index, QuadooForm::ValueComponent, FALSE));
	*value = m_values[index].component; if(*value) (*value)->AddRef();
Cleanup:
	return hr;
}
STDMETHODIMP CEventArgs::SetComponent(UINT index, IQuadooComponent* value)
{
	HRESULT hr;
	Check(Validate(index, QuadooForm::ValueComponent, TRUE));
	ReplaceInterface(m_values[index].component, value);
Cleanup:
	return hr;
}

BOOL SameObject(IUnknown* a, IUnknown* b)
{
	TStackRef<IUnknown> first, second;
	return a && b && SUCCEEDED(a->QueryInterface(IID_IUnknown, reinterpret_cast<PVOID*>(&first))) &&
		SUCCEEDED(b->QueryInterface(IID_IUnknown, reinterpret_cast<PVOID*>(&second))) && static_cast<IUnknown*>(first) == static_cast<IUnknown*>(second);
}
UINT ControlDPI(IQuadooControl* control)
{
	TStackRef<IUpdateParticipant> participant;
	UINT dpi;
	if(SUCCEEDED(control->QueryInterface(__uuidof(IUpdateParticipant), reinterpret_cast<PVOID*>(&participant))) && SUCCEEDED(participant->GetLayoutDPI(&dpi))) return dpi;
	TStackRef<IQuadooWinControl> window; HWND hwnd = NULL;
	if(SUCCEEDED(control->QueryInterface(__uuidof(IQuadooWinControl), reinterpret_cast<PVOID*>(&window)))) window->GetHandle(&hwnd);
	return SystemDPI(hwnd);
}
HRESULT GetNamespace(IQuadooControl* control, IQuadooControlContainer** root)
{
	HRESULT hr;
	TStackRef<IQuadooControl> current;
	TStackRef<IQuadooControlContainer> parent;
	TStackRef<IQuadooForm> form;
	TStackRef<IQuadooFrame> frame;
	CheckIf(!root, E_POINTER); *root = NULL;
	CheckIf(!control, E_INVALIDARG);
	current = control;
	for(UINT depth = 0; depth < 256; depth++)
	{
		Check(QueryOptional(current, &form)); Check(QueryOptional(current, &frame));
		if(form || frame)
		{ Check(current->QueryInterface(__uuidof(IQuadooControlContainer), reinterpret_cast<PVOID*>(root))); goto Cleanup; }
		Check(current->GetParent(&parent)); CheckIf(!parent, QuadooForm::EInvalidState);
		current.Release(); Check(parent->QueryInterface(__uuidof(IQuadooControl), reinterpret_cast<PVOID*>(&current))); parent.Release();
	}
	hr = QuadooForm::EInvalidState;
Cleanup:
	return hr;
}
HRESULT IndexTree(IQuadooControl* root, TRStrMap<IQuadooControl*>& names, TArray<GUID>& ids,
	IQuadooControl* substitute, RSTRING newName, BOOL isRoot, UINT depth)
{
	HRESULT hr;
	GUID id;
	CRString name;
	TStackRef<IQuadooFrame> frame;
	TStackRef<IQuadooControlContainer> container;
	BOOL boundary;
	CheckIf(!root || depth >= 128, QuadooForm::EInvalidDefinition);
	Check(QueryOptional(root, &frame)); boundary = !isRoot && frame != NULL;
	Check(root->GetName(&name));
	CheckIf(!RStrIsWide(static_cast<RSTRING>(name)) || !RStrLen(name) || RStrLen(name) > 256, QuadooForm::EInvalidDefinition);
	if(substitute && SameObject(root, substitute)) name = newName;
	if(RStrLen(name))
	{
		hr = names.Add(name, root);
		CheckIf(hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS), QuadooForm::EDuplicateName); Check(hr);
	}
	if(!boundary)
	{
		Check(root->GetID(&id));
		CheckIf(IsEqualGUID(id, GUID_NULL), QuadooForm::EInvalidDefinition);
		for(sysint n = 0; n < ids.Length(); n++) { CheckIf(IsEqualGUID(id, ids[n]), QuadooForm::EDuplicateID); }
		Check(ids.Append(id));
	}
	if(!boundary) { Check(QueryOptional(root, &container)); }
	if(container)
	{
		UINT count; Check(container->GetChildCount(&count)); CheckIf(count > 4096, QuadooForm::EInvalidDefinition);
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IQuadooControl> child;
			Check(container->GetChild(n, &child)); Check(IndexTree(child, names, ids, substitute, newName, FALSE, depth + 1));
		}
	}
	hr = S_OK;
Cleanup:
	return hr;
}
HRESULT ValidateCandidate(IQuadooControlContainer* parent, RSTRING name, REFGUID id, BOOL frame)
{
	HRESULT hr;
	TStackRef<IQuadooControlContainer> root;
	TStackRef<IQuadooControl> control;
	TRStrMap<IQuadooControl*> names(RStrCompareRStr);
	TArray<GUID> ids;
	IQuadooControl* match;
	Check(GetNamespace(parent, &root)); Check(root->QueryInterface(__uuidof(IQuadooControl), reinterpret_cast<PVOID*>(&control)));
	Check(IndexTree(control, names, ids));
	CheckIf(RStrLen(name) && SUCCEEDED(names.Find(name, &match)), QuadooForm::EDuplicateName);
	Check(ValidateVisualComponentIdentity(root,name,id,frame));
	if(!frame) for(sysint n = 0; n < ids.Length(); n++) { CheckIf(IsEqualGUID(id, ids[n]), QuadooForm::EDuplicateID); }
	hr = S_OK;
Cleanup:
	return hr;
}
}

HRESULT WINAPI QuadooValidateControlName(IQuadooControl* control, RSTRING name)
{
	HRESULT hr;
	TStackRef<IQuadooControlContainer> root;
	TStackRef<IQuadooControl> rootControl;
	TStackRef<IQuadooFrame> frame;
	TStackRef<IQuadooControlContainer> parent;
	TRStrMap<IQuadooControl*> names(RStrCompareRStr);
	TArray<GUID> ids;
	CheckIf(!control || !name || !RStrIsWide(name) || RStrLen(name) == 0, E_INVALIDARG);
	Check(QF::GetNamespace(control, &root));
	Check(root->QueryInterface(__uuidof(IQuadooControl), reinterpret_cast<PVOID*>(&rootControl)));
	Check(QF::IndexTree(rootControl, names, ids, control, name));
	{ GUID id; Check(control->GetID(&id)); Check(QF::ValidateVisualComponentIdentity(root,name,id)); }
	if(SUCCEEDED(control->QueryInterface(__uuidof(IQuadooFrame), reinterpret_cast<PVOID*>(&frame))))
	{
		Check(control->GetParent(&parent));
		if(parent)
		{
			root.Release(); rootControl.Release(); names.Clear(); ids.Clear();
			Check(QF::GetNamespace(parent, &root)); Check(root->QueryInterface(__uuidof(IQuadooControl), reinterpret_cast<PVOID*>(&rootControl)));
			Check(QF::IndexTree(rootControl, names, ids, control, name));
			{ GUID id; Check(control->GetID(&id)); Check(QF::ValidateVisualComponentIdentity(root,name,id,TRUE)); }
		}
	}
Cleanup:
	return hr;
}

namespace QF {
STDMETHODIMP CEventArgs::GetInt(UINT index, INT* value)
{
	HRESULT hr;
	CheckIf(!value, E_POINTER); *value = 0;
	Check(Validate(index, QuadooForm::ValueInt, FALSE));
	*value = static_cast<INT>(m_values[index].number);
Cleanup:
	return hr;
}
STDMETHODIMP CEventArgs::SetInt(UINT index, INT value)
{
	HRESULT hr;
	Check(Validate(index, QuadooForm::ValueInt, TRUE));
	m_values[index].number = value;
Cleanup:
	return hr;
}
}

namespace QF {
STDMETHODIMP CEventArgs::GetLong(UINT index, LONGLONG* value)
{
	HRESULT hr;
	CheckIf(!value, E_POINTER); *value = 0;
	Check(Validate(index, QuadooForm::ValueLong, FALSE));
	*value = static_cast<LONGLONG>(m_values[index].number);
Cleanup:
	return hr;
}
STDMETHODIMP CEventArgs::SetLong(UINT index, LONGLONG value)
{
	HRESULT hr;
	Check(Validate(index, QuadooForm::ValueLong, TRUE));
	m_values[index].number = value;
Cleanup:
	return hr;
}
}

namespace QF {
STDMETHODIMP CEventArgs::GetBool(UINT index, BOOL* value)
{
	HRESULT hr;
	CheckIf(!value, E_POINTER); *value = 0;
	Check(Validate(index, QuadooForm::ValueBool, FALSE));
	*value = m_values[index].number != 0;
Cleanup:
	return hr;
}
STDMETHODIMP CEventArgs::SetBool(UINT index, BOOL value)
{
	HRESULT hr;
	Check(Validate(index, QuadooForm::ValueBool, TRUE));
	m_values[index].number = value != 0;
Cleanup:
	return hr;
}
}

namespace QF
{
	VOID CEventArgs::SetNativeObject(UINT index, IUnknown* object)
	{
		if(index < m_spec->count)
		{
			ReplaceInterface(m_values[index].object,object);
		}
	}
	VOID CEventArgs::SetNativeString(UINT index, RSTRING text)
	{
		if(index < m_spec->count)
		{
			RStrReplace(&m_values[index].text,text);
		}
	}
	STDMETHODIMP CEventArgs::GetObject(UINT index, IUnknown** object)
	{
		HRESULT hr;
		CheckIf(!object,E_POINTER);
		*object = NULL;
		Check(Validate(index,QuadooForm::ValueObject,FALSE));
		*object = m_values[index].object;
		if(*object)
		{
			(*object)->AddRef();
		}
	Cleanup:
		return hr;
	}
	STDMETHODIMP CEventArgs::SetObject(UINT index, IUnknown* object)
	{
		HRESULT hr;
		Check(Validate(index,QuadooForm::ValueObject,TRUE));
		ReplaceInterface(m_values[index].object,object);
	Cleanup:
		return hr;
	}
}
