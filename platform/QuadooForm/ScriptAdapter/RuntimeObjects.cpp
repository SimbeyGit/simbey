#include "RuntimeObjects.h"
#include "TreeObjects.h"
#include "ListViewObjects.h"
#include "StatusObjects.h"
#include "CanvasBindings.h"
#include "Published\SimbeyCore.h"
#include "Library\Core\StringCore.h"

using namespace AQVM;

const WCHAR c_wzAsync[] = L"Async";

BOOL AQVM::SameIdentity(IUnknown* first, IUnknown* second)
{
	TStackRef<IUnknown> a, b;
	return first && second && SUCCEEDED(first->QueryInterface(IID_IUnknown,reinterpret_cast<PVOID*>(&a))) &&
		SUCCEEDED(second->QueryInterface(IID_IUnknown,reinterpret_cast<PVOID*>(&b))) && static_cast<IUnknown*>(a) == static_cast<IUnknown*>(b);
}

HRESULT AQVM::VariantInteger(const QuadooVM::QVARIANT* value, LONGLONG* number)
{
	HRESULT hr = S_OK;
	CheckIf(!value || !number,E_POINTER);
	if(value->eType == QuadooVM::I4)
	{
		*number = value->lVal;
	}
	else if(value->eType == QuadooVM::I8)
	{
		*number = value->llVal;
	}
	else
	{
		hr = DISP_E_TYPEMISMATCH;
	}
Cleanup:
	return hr;
}

CInstanceLink::CInstanceLink() : instance(NULL), asyncOpen(TRUE), listCursor(0)
{
}

CInstanceLink::~CInstanceLink()
{
	ClearTreeObjects(this);
	ClearListObjects(this);
}

AssignedProperty::AssignedProperty()
{
	ZeroMemory(&previous,sizeof(previous));
	previous.eType = QuadooVM::Null;
}

AssignedProperty::~AssignedProperty()
{
	QVMClearVariant(&previous);
}

CComponentObject::CComponentObject (CInstanceLink* link, IQuadooComponent* component) :
	m_link(link),
	m_component(component)
{
	if(component)
	{
		component->QueryInterface(&m_srNativeHandler);	// Support is optional
	}
}

CComponentObject::~CComponentObject()
{
}

VOID CComponentObject::AttachComponent(IQuadooComponent* component)
{
	m_component = component;

	m_srNativeHandler.Release();
	if(component)
		component->QueryInterface(&m_srNativeHandler);	// Support is optional.
}

STDMETHODIMP CComponentObject::GetComponent(IQuadooComponent** component)
{
	HRESULT hr;
	CheckIf(!component,E_POINTER);
	*component = NULL;
	CheckIf(!m_component,QuadooForm::EInvalidState);
	*component = m_component;
	(*component)->AddRef();
	hr = S_OK;
Cleanup:
	return hr;
}

HRESULT CComponentObject::Alive()
{
	HRESULT hr;
	QuadooForm::ComponentState state;
	CheckIf(!m_component,QuadooForm::EInvalidState);
	Check(m_component->GetState(&state));
	CheckIf(state >= QuadooForm::Destroying,QuadooForm::EDestroyed);
Cleanup:
	return hr;
}

HRESULT CComponentObject::Wrap(IQuadooComponent* component, QuadooVM::QVARIANT* result)
{
	HRESULT hr;
	CComponentObject* wrapper = NULL;
	result->eType = QuadooVM::Null;
	if(!component)
	{
		hr = S_OK;
		goto Cleanup;
	}
	if(m_link && m_link->instance)
	{
		Check(m_link->instance->Wrap(component,&result->pObject));
	}
	else
	{
		Check(NewComponentObject(NULL,component,&wrapper));
		result->pObject = wrapper;
		wrapper = NULL;
	}
	result->eType = QuadooVM::Object;

Cleanup:
	SafeRelease(wrapper);
	return hr;
}

HRESULT CComponentObject::ResolveMenuReference(IJSONValue* value, QuadooVM::QVARIANT* result)
{
	HRESULT hr;
	TStackRef<IQuadooComponent> root;
	TStackRef<IQuadooComponentHost> host;
	TStackRef<IQuadooComponentCollection> components;
	GUID id;
	UINT count;
	result->eType = QuadooVM::Null;
	CheckIfIgnore(!value,S_OK);
	CheckIf(!m_link || !m_link->instance,QuadooForm::EInvalidState);
	Check(value->GetStringAsGuid(id));
	Check(m_link->instance->Root()->GetComponent(&root));
	Check(root->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&host)));
	Check(host->GetComponents(&components));
	Check(components->GetCount(&count));
	for(UINT n = 0; n < count; n++)
	{
		TStackRef<IQuadooComponent> component;
		GUID current;
		Check(components->GetAt(n,&component));
		Check(component->GetID(&current));
		if(IsEqualGUID(id,current))
		{
			Check(Wrap(component,result));
			goto Cleanup;
		}
	}
	hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

Cleanup:
	return hr;
}

static HRESULT Property(IQuadooComponent* component, RSTRING name, IQuadooProperties** properties,
	IQuadooPropertyDescriptor** descriptor, UINT* index)
{
	HRESULT hr;
	TStackRef<IQuadooProperties> values;
	TStackRef<IQuadooPropertyDescriptors> descriptors;
	UINT count;
	Check(component->GetProperties(&values));
	Check(values->GetDescriptors(&descriptors));
	Check(descriptors->GetCount(&count));
	for(UINT n = 0; n < count; n++)
	{
		TStackRef<IQuadooPropertyDescriptor> item;
		CRString candidate;
		Check(descriptors->GetAt(n,&item));
		Check(item->GetName(&candidate));
		if(candidate == name)
		{
			*properties = values.Detach();
			*descriptor = item.Detach();
			*index = n;
			hr = S_OK;
			goto Cleanup;
		}
	}
	hr = DISP_E_MEMBERNOTFOUND;
Cleanup:
	return hr;
}

static HRESULT PropertyJSON(QuadooVM::QVARIANT* value, IJSONValue** json, UINT depth = 0)
{
	HRESULT hr;
	TStackRef<IQuadooArray> array;
	TStackRef<IJSONArray> copied;
	QuadooVM::QVARIANT item = {QuadooVM::Null};
	CheckIf(depth > 128,E_INVALIDARG);
	if(value->eType == QuadooVM::Array)
	{
		array = value->pArray;
	}
	else if(value->eType == QuadooVM::Object)
	{
		hr = value->pObject->QueryInterface(&array);
		CheckIf(FAILED(hr) && hr != E_NOINTERFACE,hr);
	}
	if(array)
	{
		CheckIf(array->Length() > 65536,E_INVALIDARG);
		Check(JSONCreateArray(&copied));
		for(sysint n = 0; n < array->Length(); n++)
		{
			TStackRef<IJSONValue> element;
			Check(array->GetItem(n,&item));
			Check(PropertyJSON(&item,&element,depth + 1));
			Check(copied->Add(element));
			QVMClearVariant(&item);
		}
		Check(JSONWrapArray(copied,json));
	}
	else
	{
		Check(QVMConvertToJSON(value,json));
	}
Cleanup:
	QVMClearVariant(&item);
	return hr;
}

STDMETHODIMP CComponentObject::GetProperty(IQuadooVM* pVM, RSTRING name, QuadooVM::QVARIANT* result)
{
	HRESULT hr;
	PCWSTR pcwzName = RStrToWide(name);
	TStackRef<IQuadooProperties> properties;
	TStackRef<IQuadooPropertyDescriptor> descriptor;
	TStackRef<IJSONValue> value;
	UINT index;

	CheckIf(NULL == pcwzName, E_INVALIDARG);
	CheckIf(!result,E_POINTER);
	result->eType = QuadooVM::Null;

	if(0 == TStrCmpAssert(pcwzName,L"State"))
	{
		QuadooForm::ComponentState state;
		CheckIf(!m_component,QuadooForm::EInvalidState);
		Check(m_component->GetState(&state));
		result->eType = QuadooVM::I4;
		result->lVal = state;
		goto Cleanup;
	}
	Check(Alive());

	if(m_srNativeHandler)
	{
		TStackRef<IQuadooObject> handler(m_srNativeHandler);
		Check(handler->GetProperty(pVM, name, result));
	}
	else
	{
		if(0 == TStrCmpAssert(pcwzName,L"Panels") || 0 == TStrCmpAssert(pcwzName,L"Images"))
		{
			TStackRef<IQuadooStatusBar> bar;
			if(SUCCEEDED(m_component->QueryInterface(&bar)))
			{
				if(0 == TStrCmpAssert(pcwzName,L"Panels"))
				{
					TStackRef<IQuadooStatusPanels> panels;
					Check(bar->GetPanels(&panels));
					Check(WrapStatusPanels(panels,result));
				}
				else
				{
					TStackRef<IQuadooComponent> images;
					Check(bar->GetImages(&images));
					Check(Wrap(images,result));
				}
				goto Cleanup;
			}
		}
		TStackRef<IQuadooListView> list;
		HRESULT supported = m_component->QueryInterface(&list);
		CheckIf(FAILED(supported) && supported != E_NOINTERFACE,supported);
		if(list)
		{
			supported = ListGet(list,m_link,name,result);
			CheckIf(supported != DISP_E_MEMBERNOTFOUND,supported);
		}
		if(0 == TStrCmpAssert(pcwzName,L"OnDestroy") && m_link && m_link->instance && m_link->instance->Root() == this)
		{
			RSTRING handler = m_link->instance->DestroyHandler();
			Check(RStrCreateW(RStrLen(handler),handler ? RStrToWide(handler) : L"",&result->rstrVal));
			result->eType = QuadooVM::String;
			goto Cleanup;
		}
		if(0 == TStrCmpAssert(pcwzName,L"MainMenu"))
		{
			TStackRef<IQuadooFormMenu> menus;
			TStackRef<IQuadooComponent> menu;
			Check(m_component->QueryInterface(&menus));
			Check(menus->GetMainMenu(&menu));
			Check(Wrap(menu,result));
			goto Cleanup;
		}
		if(0 == TStrCmpAssert(pcwzName,L"Items") || 0 == TStrCmpAssert(pcwzName,L"Nodes") || 0 == TStrCmpAssert(pcwzName,L"Selected") ||
			0 == TStrCmpAssert(pcwzName,L"SelectedNode") || 0 == TStrCmpAssert(pcwzName,L"Images"))
		{
			TStackRef<IQuadooTreeView> tree;
			if(SUCCEEDED(m_component->QueryInterface(&tree)))
			{
				TStackRef<IQuadooTreeNodes> nodes;
				Check(tree->GetNodes(&nodes));

				if(0 == TStrCmpAssert(pcwzName,L"Images"))
				{
					TStackRef<IQuadooComponent> images;
					Check(tree->GetImages(&images));
					Check(Wrap(images,result));
				}
				else if(0 == TStrCmpAssert(pcwzName,L"Items") || 0 == TStrCmpAssert(pcwzName,L"Nodes"))
				{
					Check(WrapTreeNodes(nodes,NULL,result,m_link));
				}
				else
				{
					TStackRef<IQuadooTreeNode> node;
					Check(tree->GetSelected(&node));
					Check(WrapTreeNode(node,nodes,result,m_link));
				}
				goto Cleanup;
			}
		}
		if(0 == TStrCmpAssert(pcwzName,L"PopupMenu"))
		{
			Check(Property(m_component,name,&properties,&descriptor,&index));
			Check(properties->GetValueJSON(index,&value));
			Check(ResolveMenuReference(value,result));
			goto Cleanup;
		}
		if(0 == TStrCmpAssert(pcwzName,L"Handle"))
		{
			TStackRef<IQuadooWinControl> window;
			HWND handle;
			Check(m_component->QueryInterface(&window));
			Check(window->GetHandle(&handle));
			QVMSetQWord(result,reinterpret_cast<UINT_PTR>(handle));
			goto Cleanup;
		}
		if(0 == TStrCmpAssert(pcwzName,L"Parent"))
		{
			TStackRef<IQuadooControl> control;
			TStackRef<IQuadooControlContainer> parent;
			Check(m_component->QueryInterface(&control));
			Check(control->GetParent(&parent));
			Check(Wrap(parent,result));
			goto Cleanup;
		}
		if(0 == TStrCmpAssert(pcwzName,L"Items"))
		{
			TStackRef<IQuadooComboBox> combo;
			TStackRef<IQuadooListBox> list;
			TStackRef<IRStringArray> items;
			if(SUCCEEDED(m_component->QueryInterface(&combo)))
			{
				Check(combo->GetItems(&items));
			}
			else if(SUCCEEDED(m_component->QueryInterface(&list)))
			{
				Check(list->GetItems(&items));
			}
			if(items)
			{
				Check(QVMWrapStringArray(items,result));
				goto Cleanup;
			}
		}
		if(0 == TStrCmpAssert(pcwzName,L"ActivePage"))
		{
			TStackRef<IQuadooTabControl> tabs;
			TStackRef<IQuadooTabPage> page;
			Check(m_component->QueryInterface(&tabs));
			Check(tabs->GetActivePage(&page));
			Check(Wrap(page,result));
			goto Cleanup;
		}
		Check(Property(m_component,name,&properties,&descriptor,&index));
		Check(properties->GetValueJSON(index,&value));
		Check(QVMConvertFromJSON(value,result));
	}

Cleanup:
	return hr;
}

STDMETHODIMP CComponentObject::GetIndexedProperty(IQuadooVM* vm, RSTRING name, QuadooVM::QVARIANT* index, QuadooVM::QVARIANT* result)
{
	HRESULT hr;
	QuadooVM::QVARIANT collection = {QuadooVM::Null};
	TStackRef<IQuadooListView> view;
	CheckIf(!index || !result,E_POINTER);
	result->eType = QuadooVM::Null;
	Check(Alive());
	if(m_srNativeHandler)
	{
		TStackRef<IQuadooObject> handler(m_srNativeHandler);
		Check(handler->GetIndexedProperty(vm,name,index,result));
	}
	else
	{
		PCWSTR pcwzProperty = RStrToWide(name);
		CheckIf(NULL == pcwzProperty, DISP_E_MEMBERNOTFOUND);

		if(0 != TStrCmpAssert(pcwzProperty, L"Panels"))
		{
			Check(m_component->QueryInterface(&view));
			CheckIf(TStrCmpAssert(pcwzProperty, L"Items") && TStrCmpAssert(pcwzProperty, L"Columns"), DISP_E_MEMBERNOTFOUND);
		}
		Check(GetProperty(vm,name,&collection));
		CheckIf(collection.eType != QuadooVM::Object,DISP_E_TYPEMISMATCH);
		Check(collection.pObject->GetIndexedProperty(vm,NULL,index,result));
	}

Cleanup:
	QVMClearVariant(&collection);
	return hr;
}

STDMETHODIMP CComponentObject::SetIndexedProperty(IQuadooVM* vm, RSTRING name, QuadooVM::QVARIANT* index, QuadooVM::QVARIANT* value)
{
	HRESULT hr;
	CheckIf(!index || !value,E_POINTER);
	Check(Alive());
	if(m_srNativeHandler)
	{
		TStackRef<IQuadooObject> handler(m_srNativeHandler);
		Check(handler->SetIndexedProperty(vm,name,index,value));
	}
	else
	{
		hr = CQuadooObjectImpl::SetIndexedProperty(vm,name,index,value);
	}
Cleanup:
	return hr;
}

STDMETHODIMP CComponentObject::GetMemberVariable(DWORD index, QuadooVM::QVARIANT** value)
{
	HRESULT hr;
	CheckIf(!value,E_POINTER);
	*value = NULL;
	Check(Alive());
	if(m_srNativeHandler)
	{
		TStackRef<IQuadooObject> handler(m_srNativeHandler);
		Check(handler->GetMemberVariable(index,value));
	}
	else
	{
		hr = CQuadooObjectImpl::GetMemberVariable(index,value);
	}
Cleanup:
	return hr;
}

STDMETHODIMP CComponentObject::GetDefaultValue(QuadooVM::QVARIANT* result)
{
	HRESULT hr;
	CheckIf(!result,E_POINTER);
	result->eType = QuadooVM::Null;
	Check(Alive());
	if(m_srNativeHandler)
	{
		TStackRef<IQuadooObject> handler(m_srNativeHandler);
		Check(handler->GetDefaultValue(result));
	}
	else
	{
		hr = CQuadooObjectImpl::GetDefaultValue(result);
	}
Cleanup:
	return hr;
}

STDMETHODIMP CComponentObject::DeleteProperty(RSTRING name, QuadooVM::QVARIANT* result)
{
	HRESULT hr;
	CheckIf(!name,E_INVALIDARG);
	if(result)
	{
		result->eType = QuadooVM::Null;
	}
	Check(Alive());
	if(m_srNativeHandler)
	{
		TStackRef<IQuadooObject> handler(m_srNativeHandler);
		Check(handler->DeleteProperty(name,result));
	}
	else
	{
		hr = CQuadooObjectImpl::DeleteProperty(name,result);
	}
Cleanup:
	return hr;
}

STDMETHODIMP CComponentObject::SetProperty(IQuadooVM* vm, RSTRING name, QuadooVM::QVARIANT* value)
{
	HRESULT hr;
	PCWSTR pcwzName = RStrToWide(name);
	TStackRef<IQuadooProperties> properties;
	TStackRef<IQuadooPropertyDescriptor> descriptor;
	TStackRef<IJSONValue> json;
	UINT index;
	QuadooProperty::Type type;

	CheckIf(NULL == pcwzName, E_INVALIDARG);
	CheckIf(!value,E_POINTER);

	Check(Alive());
	if(m_srNativeHandler)
	{
		TStackRef<IQuadooObject> handler(m_srNativeHandler);
		Check(handler->SetProperty(vm,name,value));
		goto Cleanup;
	}
	{
		TStackRef<IQuadooListView> list;
		HRESULT supported = m_component->QueryInterface(&list);
		CheckIf(FAILED(supported) && supported != E_NOINTERFACE,supported);
		if(list)
		{
			supported = ListSet(list,name,value);
			CheckIf(supported != DISP_E_MEMBERNOTFOUND,supported);
		}
	}
	if(0 == TStrCmpAssert(pcwzName,L"OnDestroy") && m_link && m_link->instance && m_link->instance->Root() == this)
	{
		CheckIf(value->eType != QuadooVM::String,DISP_E_TYPEMISMATCH);
		Check(m_link->instance->SetDestroyHandler(value->rstrVal));
		goto Cleanup;
	}
	if(0 == TStrCmpAssert(pcwzName,L"Images"))
	{
		TStackRef<IQuadooStatusBar> bar;
		if(SUCCEEDED(m_component->QueryInterface(&bar)))
		{
			TStackRef<IQuadooComponent> images;
			if(value->eType != QuadooVM::Null)
			{
				CheckIf(value->eType != QuadooVM::Object || !m_link || !m_link->instance,DISP_E_TYPEMISMATCH);
				Check(m_link->instance->Runtime()->Unwrap(value->pObject,&images));
			}
			Check(bar->SetImages(images));
			goto Cleanup;
		}
	}
	if(0 == TStrCmpAssert(pcwzName,L"Selected") || 0 == TStrCmpAssert(pcwzName,L"SelectedNode") || 0 == TStrCmpAssert(pcwzName,L"Images"))
	{
		TStackRef<IQuadooTreeView> tree;
		if(SUCCEEDED(m_component->QueryInterface(&tree)))
		{
			if(0 == TStrCmpAssert(pcwzName,L"Images"))
			{
				TStackRef<IQuadooComponent> images;
				if(value->eType != QuadooVM::Null)
				{
					CheckIf(value->eType != QuadooVM::Object || !m_link || !m_link->instance,DISP_E_TYPEMISMATCH);
					Check(m_link->instance->Runtime()->Unwrap(value->pObject,&images));
				}
				Check(tree->SetImages(images));
			}
			else
			{
				TStackRef<IQuadooTreeNode> node;
				Check(UnwrapTreeNode(value,&node));
				Check(tree->SetSelected(node));
			}
			goto Cleanup;
		}
	}
	if(0 == TStrCmpAssert(pcwzName,L"MainMenu"))
	{
		TStackRef<IQuadooFormMenu> menus;
		TStackRef<IQuadooComponent> menu;
		Check(m_component->QueryInterface(&menus));
		if(value->eType != QuadooVM::Null)
		{
			CheckIf(value->eType != QuadooVM::Object || !m_link || !m_link->instance,DISP_E_TYPEMISMATCH);
			Check(m_link->instance->Runtime()->Unwrap(value->pObject,&menu));
		}
		Check(menus->SetMainMenu(menu));
		goto Cleanup;
	}
	if(0 == TStrCmpAssert(pcwzName,L"ActivePage"))
	{
		TStackRef<IQuadooTabControl> tabs;
		TStackRef<IQuadooComponent> component;
		TStackRef<IQuadooTabPage> page;
		Check(m_component->QueryInterface(&tabs));
		if(value->eType != QuadooVM::Null)
		{
			CheckIf(value->eType != QuadooVM::Object || !m_link || !m_link->instance,DISP_E_TYPEMISMATCH);
			Check(m_link->instance->Runtime()->Unwrap(value->pObject,&component));
			Check(component->QueryInterface(&page));
		}
		Check(tabs->SetActivePage(page));
		goto Cleanup;
	}
	if(0 == TStrCmpAssert(pcwzName,L"PopupMenu") && value->eType == QuadooVM::Object)
	{
		TStackRef<IQuadooComponent> menu;
		TStackRef<IQuadooPopupMenu> popup;
		TStackRef<IQuadooNonvisualComponent> nonvisual;
		TStackRef<IQuadooComponentCollection> owner;
		TStackRef<IQuadooControlContainer> ownerRoot;
		TStackRef<IQuadooComponent> root;
		GUID id;
		WCHAR text[40];
		CheckIf(!m_link || !m_link->instance,QuadooForm::EInvalidState);
		Check(m_link->instance->Runtime()->Unwrap(value->pObject,&menu));
		Check(menu->QueryInterface(&popup));
		Check(menu->QueryInterface(&nonvisual));
		Check(nonvisual->GetOwner(&owner));
		CheckIf(!owner,QuadooForm::EDestroyed);
		Check(owner->GetRoot(&ownerRoot));
		Check(m_link->instance->Root()->GetComponent(&root));
		CheckIf(!SameIdentity(ownerRoot,root),E_INVALIDARG);
		Check(menu->GetID(&id));
		INT length = StringFromGUID2(id,text,ARRAYSIZE(text));
		CheckIf(!length,E_FAIL);
		Check(Property(m_component,name,&properties,&descriptor,&index));
		Check(JSONCreateStringW(text,length - 1,&json));
		Check(properties->SetValueJSON(index,json));
		goto Cleanup;
	}
	Check(Property(m_component,name,&properties,&descriptor,&index));
	Check(descriptor->GetType(&type));
	if(type == QuadooProperty::Color)
	{
		LONGLONG raw;
		Check(VariantInteger(value,&raw));
		CheckIf(raw < INT_MIN || raw > UINT_MAX,DISP_E_OVERFLOW);
		Check(JSONCreateInteger(static_cast<INT>(raw),&json));
	}
	else
	{
		Check(PropertyJSON(value,&json));
	}
	Check(properties->SetValueJSON(index,json));
Cleanup:
	return hr;
}

STDMETHODIMP CComponentObject::Invoke (IQuadooVM* pVM, RSTRING method, QuadooVM::QVPARAMS* parameters, QuadooVM::QVARIANT* result)
{
	HRESULT hr;
	PCWSTR pcwzMethod = RStrToWide(method);

	CheckIf(NULL == pcwzMethod, E_INVALIDARG);
	CheckIf(!parameters || !result,E_POINTER);
	result->eType = QuadooVM::Null;

	if(0 == TStrCmpAssert(pcwzMethod,L"Destroy"))
	{
		CheckIf(parameters->cArgs,DISP_E_BADPARAMCOUNT);
		CheckIf(!m_component,QuadooForm::EInvalidState);
		Check(m_component->Destroy());
		goto Cleanup;
	}
	Check(Alive());
	if(m_srNativeHandler)
	{
		TStackRef<IQuadooObject> handler(m_srNativeHandler);
		Check(handler->Invoke(pVM, method, parameters, result));
	}
	else
	{
		TStackRef<IQuadooCanvas> canvas;
		HRESULT supported = m_component->QueryInterface(&canvas);
		CheckIf(FAILED(supported) && supported != E_NOINTERFACE,supported);
		if(canvas)
		{
			supported = CanvasInvoke(canvas,method,parameters,result);
			CheckIf(supported != DISP_E_MEMBERNOTFOUND,supported);
		}
		TStackRef<IQuadooListView> list;
		supported = m_component->QueryInterface(&list);
		CheckIf(FAILED(supported) && supported != E_NOINTERFACE,supported);
		if(list)
		{
			supported = ListInvoke(list,m_link,method,parameters,result);
			CheckIf(supported != DISP_E_MEMBERNOTFOUND,supported);
		}
		if(0 == TStrCmpAssert(pcwzMethod,L"BeginLabelEdit"))
		{
			TStackRef<IQuadooTreeView> tree;
			TStackRef<IQuadooTreeNode> node;
			CheckIf(parameters->cArgs != 1,DISP_E_BADPARAMCOUNT);
			Check(m_component->QueryInterface(&tree));
			Check(UnwrapTreeNode(parameters->pqvArgs,&node));
			Check(tree->BeginLabelEdit(node));
			goto Cleanup;
		}
		if(0 == TStrCmpAssert(pcwzMethod,L"StepIt"))
		{
			TStackRef<IQuadooProgressBar> progress;
			CheckIf(parameters->cArgs,DISP_E_BADPARAMCOUNT);
			Check(m_component->QueryInterface(&progress));
			Check(progress->StepIt());
		}
		else if(0 == TStrCmpAssert(pcwzMethod,L"SetFocus"))
		{
			TStackRef<IQuadooWinControl> window;
			CheckIf(parameters->cArgs,DISP_E_BADPARAMCOUNT);
			Check(m_component->QueryInterface(&window));
			Check(window->SetFocus());
		}
		else if(0 == TStrCmpAssert(pcwzMethod,L"Click"))
		{
			TStackRef<IQuadooButton> button;
			TStackRef<IQuadooMenuItem> menu;
			CheckIf(parameters->cArgs,DISP_E_BADPARAMCOUNT);
			if(SUCCEEDED(m_component->QueryInterface(&menu)))
			{
				Check(menu->Click());
			}
			else
			{
				Check(m_component->QueryInterface(&button));
				Check(button->Click());
			}
		}
		else if(0 == TStrCmpAssert(pcwzMethod,L"Toggle") || 0 == TStrCmpAssert(pcwzMethod,L"SetEnabled") || 0 == TStrCmpAssert(pcwzMethod,L"SetChecked") ||
			0 == TStrCmpAssert(pcwzMethod,L"GetEnabled") || 0 == TStrCmpAssert(pcwzMethod,L"GetChecked"))
		{
			TStackRef<IQuadooMenuItem> item;
			Check(m_component->QueryInterface(&item));
			if(0 == TStrCmpAssert(pcwzMethod,L"Toggle"))
			{
				CheckIf(parameters->cArgs,DISP_E_BADPARAMCOUNT);
				Check(item->Toggle());
			}
			else if(pcwzMethod[0] == L'S')
			{
				CheckIf(parameters->cArgs != 1,DISP_E_BADPARAMCOUNT);
				CheckIf(parameters->pqvArgs[0].eType != QuadooVM::Bool,DISP_E_TYPEMISMATCH);
				if(0 == TStrCmpAssert(pcwzMethod,L"SetEnabled"))
				{
					Check(item->SetEnabled(parameters->pqvArgs[0].fVal));
				}
				else
				{
					Check(item->SetChecked(parameters->pqvArgs[0].fVal));
				}
			}
			else
			{
				BOOL flag;
				CheckIf(parameters->cArgs,DISP_E_BADPARAMCOUNT);
				if(0 == TStrCmpAssert(pcwzMethod,L"GetEnabled"))
				{
					Check(item->GetEnabled(&flag));
				}
				else
				{
					Check(item->GetChecked(&flag));
				}
				result->eType = QuadooVM::Bool;
				result->fVal = flag != FALSE;
			}
		}
		else if(0 == TStrCmpAssert(pcwzMethod,L"Popup"))
		{
			TStackRef<IQuadooPopupMenu> menu;
			LONGLONG x;
			LONGLONG y;
			CheckIf(parameters->cArgs != 2,DISP_E_BADPARAMCOUNT);
			Check(VariantInteger(&parameters->pqvArgs[0],&x));
			Check(VariantInteger(&parameters->pqvArgs[1],&y));
			CheckIf(x < INT_MIN || x > INT_MAX || y < INT_MIN || y > INT_MAX,DISP_E_OVERFLOW);
			Check(m_component->QueryInterface(&menu));
			Check(menu->Popup(NULL,static_cast<INT>(x),static_cast<INT>(y)));
		}
		else if(0 == TStrCmpAssert(pcwzMethod,L"Execute"))
		{
			TStackRef<IQuadooFileDialog> file;
			TStackRef<IQuadooColorDialog> color;
			TStackRef<IQuadooComponent> owner;
			TStackRef<IQuadooWinControl> window;
			BOOL accepted;
			HWND handle = NULL;
			CheckIf(parameters->cArgs > 1,DISP_E_BADPARAMCOUNT);
			if(parameters->cArgs)
			{
				CheckIf(parameters->pqvArgs[0].eType != QuadooVM::Object || !m_link || !m_link->instance,DISP_E_TYPEMISMATCH);
				Check(m_link->instance->Runtime()->Unwrap(parameters->pqvArgs[0].pObject,&owner));
			}
			else if(m_link && m_link->instance)
			{
				Check(m_link->instance->Root()->GetComponent(&owner));
			}
			if(owner && SUCCEEDED(owner->QueryInterface(&window)))
			{
				Check(window->GetHandle(&handle));
			}
			if(SUCCEEDED(m_component->QueryInterface(&file)))
			{
				Check(file->Execute(handle,&accepted));
			}
			else
			{
				Check(m_component->QueryInterface(&color));
				Check(color->Execute(handle,&accepted));
			}
			result->eType = QuadooVM::Bool;
			result->fVal = accepted != FALSE;
		}
		else if(0 == TStrCmpAssert(pcwzMethod,L"SelectNextPage"))
		{
			TStackRef<IQuadooTabControl> tabs;
			CheckIf(parameters->cArgs != 1 || parameters->pqvArgs[0].eType != QuadooVM::Bool,DISP_E_BADPARAMCOUNT);
			Check(m_component->QueryInterface(&tabs));
			Check(tabs->SelectNextPage(parameters->pqvArgs[0].fVal));
		}
		else
		{
			hr = DISP_E_MEMBERNOTFOUND;
		}
	}
Cleanup:
	return hr;
}

CContainerObject::CContainerObject(CInstanceLink* link, IQuadooComponent* component) : CComponentObject(link,component)
{
}

STDMETHODIMP CContainerObject::GetProperty(IQuadooVM* vm, RSTRING name, QuadooVM::QVARIANT* result)
{
	HRESULT hr;
	PCWSTR pcwzName = RStrToWide(name);

	CheckIf(NULL == pcwzName, E_INVALIDARG);

	if(0 == TStrCmpAssert(pcwzName,L"ControlCount"))
	{
		TStackRef<IQuadooControlContainer> container;
		UINT count;
		Check(Alive());
		Check(m_component->QueryInterface(&container));
		Check(container->GetChildCount(&count));
		result->eType = QuadooVM::I4;
		result->lVal = count;
	}
	else
	{
		hr = CComponentObject::GetProperty(vm,name,result);
	}
Cleanup:
	return hr;
}

STDMETHODIMP CContainerObject::Invoke(IQuadooVM* vm, RSTRING method, QuadooVM::QVPARAMS* parameters, QuadooVM::QVARIANT* result)
{
	HRESULT hr;
	PCWSTR pcwzMethod = RStrToWide(method);
	TStackRef<IQuadooControlContainer> container;
	TStackRef<IQuadooControl> control;

	CheckIf(!parameters || !result,E_POINTER);
	CheckIf(NULL == pcwzMethod, E_INVALIDARG);
	result->eType = QuadooVM::Null;

	if(0 == TStrCmpNAssert(pcwzMethod, SLP(c_wzAsync)))
	{
		pcwzMethod += StaticLength(c_wzAsync);

		if(L'\0' == *pcwzMethod)							// Async
		{
			Check(Alive());
			CheckIf(parameters->cArgs != 1,DISP_E_BADPARAMCOUNT);
			CheckIf(!parameters->pqvArgs,E_POINTER);
			CheckIf(parameters->pqvArgs[0].eType != QuadooVM::Object,DISP_E_TYPEMISMATCH);
			CheckIf(!parameters->pqvArgs[0].pObject,E_INVALIDARG);
			CheckIf(!m_link || !m_link->instance || !m_link->asyncOpen,QuadooForm::EDestroyed);
			Check(m_link->instance->Runtime()->QueueAsync(m_link,m_component,parameters->pqvArgs[0].pObject));
		}
		else if(0 == TStrCmpAssert(pcwzMethod, L"Wait"))	// AsyncWait
		{
			Check(Alive());
			CheckIf(parameters->cArgs != 2,DISP_E_BADPARAMCOUNT);
			CheckIf(!parameters->pqvArgs,E_POINTER);
			CheckIf(parameters->pqvArgs[0].eType != QuadooVM::Object || parameters->pqvArgs[1].eType != QuadooVM::Object,DISP_E_TYPEMISMATCH);
			CheckIf(!parameters->pqvArgs[0].pObject || !parameters->pqvArgs[1].pObject,E_INVALIDARG);
			CheckIf(!m_link || !m_link->instance || !m_link->asyncOpen,QuadooForm::EDestroyed);
			// Native argument storage is in reverse script-argument order.
			Check(m_link->instance->Runtime()->QueueAsyncWait(m_link,m_component,
				parameters->pqvArgs[1].pObject,parameters->pqvArgs[0].pObject));
		}
		else
			hr = CComponentObject::Invoke(vm,method,parameters,result);
	}
	else if(0 == TStrCmpAssert(pcwzMethod,L"FindControl") || 0 == TStrCmpAssert(pcwzMethod,L"GetControl"))
	{
		Check(Alive());
		CheckIf(parameters->cArgs != 1,DISP_E_BADPARAMCOUNT);
		Check(m_component->QueryInterface(&container));
		if(0 == TStrCmpAssert(pcwzMethod,L"FindControl"))
		{
			CheckIf(parameters->pqvArgs[0].eType != QuadooVM::String,DISP_E_TYPEMISMATCH);
			Check(container->FindControl(parameters->pqvArgs[0].rstrVal,&control));
		}
		else
		{
			LONGLONG index;
			Check(VariantInteger(parameters->pqvArgs,&index));
			CheckIf(index < 0 || index > UINT_MAX,DISP_E_BADINDEX);
			Check(container->GetChild(static_cast<UINT>(index),&control));
		}
		Check(Wrap(control,result));
	}
	else if(0 == TStrCmpAssert(pcwzMethod,L"BeginUpdate") || 0 == TStrCmpAssert(pcwzMethod,L"EndUpdate") || 0 == TStrCmpAssert(pcwzMethod,L"UpdateLayout"))
	{
		Check(Alive());
		CheckIf(parameters->cArgs,DISP_E_BADPARAMCOUNT);
		Check(m_component->QueryInterface(&container));
		if(0 == TStrCmpAssert(pcwzMethod,L"BeginUpdate"))
		{
			Check(container->BeginUpdate());
		}
		else if(0 == TStrCmpAssert(pcwzMethod,L"EndUpdate"))
		{
			Check(container->EndUpdate());
		}
		else
		{
			Check(container->UpdateLayout());
		}
	}
	else
	{
		hr = CComponentObject::Invoke(vm,method,parameters,result);
	}
Cleanup:
	return hr;
}

CFormObject::CFormObject(CInstanceLink* link) : CContainerObject(link,NULL)
{
}

CFrameObject::CFrameObject(CInstanceLink* link) : CContainerObject(link,NULL)
{
}

STDMETHODIMP CFormObject::Invoke(IQuadooVM* vm, RSTRING method, QuadooVM::QVPARAMS* parameters, QuadooVM::QVARIANT* result)
{
	HRESULT hr;
	PCWSTR pcwzMethod = RStrToWide(method);
	TStackRef<IQuadooForm> form;

	CheckIf(NULL == pcwzMethod, E_INVALIDARG);

	if(0 == TStrCmpAssert(pcwzMethod,L"Show") || 0 == TStrCmpAssert(pcwzMethod,L"ShowForm") || 0 == TStrCmpAssert(pcwzMethod,L"Hide") || 0 == TStrCmpAssert(pcwzMethod,L"HideForm") || 0 == TStrCmpAssert(pcwzMethod,L"Close") || 0 == TStrCmpAssert(pcwzMethod,L"ShowModal"))
	{
		result->eType = QuadooVM::Null;
		Check(Alive());
		Check(m_component->QueryInterface(&form));
		if(0 == TStrCmpAssert(pcwzMethod,L"Show") || 0 == TStrCmpAssert(pcwzMethod,L"ShowForm"))
		{
			LONGLONG command = SW_SHOW;
			CheckIf(parameters->cArgs > 1,DISP_E_BADPARAMCOUNT);
			if(parameters->cArgs)
			{
				Check(VariantInteger(parameters->pqvArgs,&command));
			}
			CheckIf(command < SW_HIDE || command > SW_FORCEMINIMIZE,E_INVALIDARG);
			Check(form->ShowForm(static_cast<INT>(command)));
		}
		else
		{
			CheckIf(parameters->cArgs,DISP_E_BADPARAMCOUNT);
			if(0 == TStrCmpAssert(pcwzMethod,L"Close"))
			{
				hr = form->Close();
				Check(hr);
				result->eType = QuadooVM::Bool;
				result->fVal = hr == S_OK;
			}
			else if(0 == TStrCmpAssert(pcwzMethod,L"ShowModal"))
			{
				LRESULT modal;
				Check(form->ShowModal(&modal));
				result->eType = QuadooVM::I8;
				result->llVal = modal;
			}
			else
			{
				Check(form->HideForm());
			}
		}
	}
	else
	{
		hr = CContainerObject::Invoke(vm,method,parameters,result);
	}
Cleanup:
	return hr;
}

HRESULT AQVM::NewComponentObject(CInstanceLink* link, IQuadooComponent* component, CComponentObject** object)
{
	HRESULT hr = S_OK;
	TStackRef<IQuadooControlContainer> container;
	CheckIf(!object,E_POINTER);
	*object = NULL;
	if(SUCCEEDED(component->QueryInterface(&container)))
	{
		*object = __new CContainerObject(link,component);
	}
	else
	{
		*object = __new CComponentObject(link,component);
	}
	CheckAlloc(*object);
Cleanup:
	return hr;
}
