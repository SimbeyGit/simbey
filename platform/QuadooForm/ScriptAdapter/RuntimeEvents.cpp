#include "RuntimeObjects.h"
#include "TreeObjects.h"
#include "ListViewObjects.h"
#include "Library\Core\StringCore.h"

using namespace AQVM;

class CEventArguments : public CBaseUnknown, public CQuadooObjectImpl
{
	TStackRef<CInstanceLink> m_link;
	TStackRef<IQuadooEventArgs> m_args;
	BOOL m_active;

	HRESULT Argument(RSTRING name, UINT* index, QuadooForm::EventValueType* type)
	{
		HRESULT hr;
		UINT count;
		Check(m_args->GetArgumentCount(&count));
		for(UINT n = 0; n < count; n++)
		{
			CRString candidate;
			Check(m_args->GetArgumentName(n,&candidate));
			if(candidate == name)
			{
				*index = n;
				Check(m_args->GetArgumentType(n,type));
				goto Cleanup;
			}
		}
		hr = DISP_E_MEMBERNOTFOUND;
	Cleanup:
		return hr;
	}

	HRESULT WrapComponent(IQuadooComponent* component, QuadooVM::QVARIANT* result)
	{
		HRESULT hr = S_OK;
		CComponentObject* wrapper = NULL;
		result->eType = QuadooVM::Null;
		if(component)
		{
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
		}
	Cleanup:
		SafeRelease(wrapper);
		return hr;
	}

public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooObject)
	END_UNK_MAP

	CEventArguments(CInstanceLink* link, IQuadooEventArgs* args) : m_link(link), m_args(args), m_active(TRUE)
	{
	}

	VOID Complete()
	{
		m_active = FALSE;
	}

	STDMETHOD(GetProperty)(IQuadooVM*, RSTRING name, QuadooVM::QVARIANT* result)
	{
		HRESULT hr;
		UINT index;
		QuadooForm::EventValueType type;
		result->eType = QuadooVM::Null;
		if(!lstrcmpW(RStrToWide(name),L"EventName"))
		{
			Check(m_args->GetEventName(&result->rstrVal));
			result->eType = QuadooVM::String;
			goto Cleanup;
		}
		if(!lstrcmpW(RStrToWide(name),L"Sender"))
		{
			TStackRef<IQuadooComponent> sender;
			Check(m_args->GetSender(&sender));
			Check(WrapComponent(sender,result));
			goto Cleanup;
		}
		Check(Argument(name,&index,&type));
		switch(type)
		{
		case QuadooForm::ValueInt:
			{
				INT integer;
				Check(m_args->GetInt(index,&integer));
				result->lVal = integer;
				result->eType = QuadooVM::I4;
			}
			break;
		case QuadooForm::ValueLong:
			Check(m_args->GetLong(index,&result->llVal));
			result->eType = QuadooVM::I8;
			break;
		case QuadooForm::ValueBool:
			{
				BOOL value;
				Check(m_args->GetBool(index,&value));
				result->fVal = value != FALSE;
				result->eType = QuadooVM::Bool;
			}
			break;
		case QuadooForm::ValueString:
			Check(m_args->GetString(index,&result->rstrVal));
			result->eType = QuadooVM::String;
			break;
		case QuadooForm::ValueComponent:
			{
				TStackRef<IQuadooComponent> component;
				Check(m_args->GetComponent(index,&component));
				Check(WrapComponent(component,result));
			}
			break;
		case QuadooForm::ValueObject:
			{
				TStackRef<IQuadooEventObjectArgs> objects;
				TStackRef<IUnknown> object;
				TStackRef<IQuadooComponent> sender;
				Check(m_args->QueryInterface(__uuidof(IQuadooEventObjectArgs),reinterpret_cast<PVOID*>(&objects)));
				Check(objects->GetObject(index,&object));
				if(object)
				{
					Check(m_args->GetSender(&sender));
					Check(WrapValueObject(object,sender,m_link,result));
				}
			}
			break;
		default:
			hr = DISP_E_TYPEMISMATCH;
		}
	Cleanup:
		return hr;
	}

	STDMETHOD(SetProperty)(IQuadooVM*, RSTRING name, QuadooVM::QVARIANT* value)
	{
		HRESULT hr;
		UINT index;
		QuadooForm::EventValueType type;
		LONGLONG integer;
		CheckIf(!m_active,QuadooForm::EInvalidState);
		Check(Argument(name,&index,&type));
		switch(type)
		{
		case QuadooForm::ValueInt:
			Check(VariantInteger(value,&integer));
			CheckIf(integer < INT_MIN || integer > INT_MAX,DISP_E_OVERFLOW);
			Check(m_args->SetInt(index,static_cast<INT>(integer)));
			break;
		case QuadooForm::ValueLong:
			Check(VariantInteger(value,&integer));
			Check(m_args->SetLong(index,integer));
			break;
		case QuadooForm::ValueBool:
			CheckIf(value->eType != QuadooVM::Bool,DISP_E_TYPEMISMATCH);
			Check(m_args->SetBool(index,value->fVal));
			break;
		case QuadooForm::ValueString:
			CheckIf(value->eType != QuadooVM::String,DISP_E_TYPEMISMATCH);
			Check(m_args->SetString(index,value->rstrVal));
			break;
		case QuadooForm::ValueComponent:
			{
				TStackRef<IQuadooComponent> component;
				if(value->eType != QuadooVM::Null)
				{
					CheckIf(value->eType != QuadooVM::Object || !m_link || !m_link->instance,DISP_E_TYPEMISMATCH);
					Check(m_link->instance->Runtime()->Unwrap(value->pObject,&component));
				}
				Check(m_args->SetComponent(index,component));
			}
			break;
		default:
			hr = DISP_E_TYPEMISMATCH;
		}
	Cleanup:
		return hr;
	}
};

class CScriptDelegate : public CBaseUnknown, public IQuadooDelegate
{
	TStackRef<CInstanceLink> m_link;
	ULONG m_ordinal;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooDelegate)
	END_UNK_MAP

	CScriptDelegate(ULONG ordinal, CInstanceLink* link) : m_link(link), m_ordinal(ordinal)
	{
	}

	STDMETHOD(Invoke)(IQuadooEventArgs* args)
	{
		HRESULT hr;
		TStackRef<CEventArguments> arguments;
		TStackRef<CFormInstance> instance(m_link->instance);
		QuadooVM::QVARIANT input = {QuadooVM::Null}, result = {QuadooVM::Null};
		QuadooVM::QVPARAMS parameters = {0};
		CRString event;
		BOOL diagnostic = FALSE;
		BOOL entered = FALSE;
		CheckIf(!instance,QuadooForm::EDestroyed);
		Check(instance->Runtime()->EnterCallback());
		entered = TRUE;
		Check(args->GetEventName(&event));
		diagnostic = event == RSTRING_CAST(L"OnData") || event == RSTRING_CAST(L"OnDataUpdate") ||
			event == RSTRING_CAST(L"OnDataHint") || event == RSTRING_CAST(L"OnDataFind") ||
			event == RSTRING_CAST(L"OnDataStateChange");
		if(diagnostic)
		{
			TStackRef<IQuadooComponent> sender;
			TStackRef<IQuadooListView> list;
			ULONGLONG generation = 0;
			diagnostic = SUCCEEDED(args->GetSender(&sender)) && sender &&
				SUCCEEDED(sender->QueryInterface(__uuidof(IQuadooListView),reinterpret_cast<PVOID*>(&list))) &&
				SUCCEEDED(list->GetDataGeneration(&generation)) && generation != 0;
		}
		arguments.Attach(__new CEventArguments(m_link,args));
		CheckAlloc(arguments);
		input.eType = QuadooVM::Object;
		input.pObject = arguments;
		parameters.cArgs = 1;
		parameters.pqvArgs = &input;
		Check(instance->InvokeResolved(m_ordinal, &parameters, &result));
		if(result.eType != QuadooVM::Bool && result.eType != QuadooVM::Null)
		{
			if(result.eType == QuadooVM::Object)
			{
				QuadooVM::QVARIANT description = {QuadooVM::Null}, code = {QuadooVM::Null};
				QuadooVM::QVPARAMS empty = {0};
				HRESULT failure = E_FAIL;
				if(SUCCEEDED(result.pObject->GetProperty(NULL,RSTRING_CAST(L"Code"),&code)) && code.eType == QuadooVM::I4 && FAILED(code.lVal))
				{
					failure = code.lVal;
				}
				if(SUCCEEDED(result.pObject->Invoke(NULL,RSTRING_CAST(L"ToString"),&empty,&description)) && description.eType == QuadooVM::String)
				{
					if(diagnostic)
					{
						instance->Runtime()->ReportDiagnostic(failure,event,description.rstrVal);
					}
					else
					{
						instance->Runtime()->OnError(failure,event,description.rstrVal);
					}
				}
				QVMClearVariant(&description);
				QVMClearVariant(&code);
				Check(failure);
			}
			else
				CheckIf(QuadooVM::I4 != result.eType, DISP_E_TYPEMISMATCH);
		}
	Cleanup:
		if(arguments)
		{
			arguments->Complete();
		}
		QVMClearVariant(&result);
		if(FAILED(hr) && instance)
		{
			if(diagnostic)
			{
				instance->Runtime()->ReportDiagnostic(hr,event,RSTRING_CAST(L"Script event invocation failed."));
			}
			else
			{
				instance->Runtime()->OnError(hr,event,RSTRING_CAST(L"Script event invocation failed."));
			}
		}
		arguments.Release();
		if(entered)
		{
			instance->Runtime()->LeaveCallback();
		}
		return hr;
	}
};

class CRootDestroyDelegate : public CBaseUnknown, public IQuadooDelegate
{
	TStackRef<CInstanceLink> m_link;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooDelegate)
	END_UNK_MAP

	CRootDestroyDelegate(CInstanceLink* link) : m_link(link)
	{
	}

	STDMETHOD(Invoke)(IQuadooEventArgs* args)
	{
		HRESULT hr = S_OK;
		TStackRef<CFormInstance> instance(m_link->instance);
		if(instance)
		{
			hr = instance->InvokeDestroy(args);
		}
		return hr;
	}
};

CFormInstance::CFormInstance(CApplicationRuntime* runtime, IQuadooObject* script) :
	m_runtime(runtime), m_script(script), m_root(NULL), m_committed(FALSE), m_detached(FALSE)
{
}

CFormInstance::~CFormInstance()
{
	Detach();
}

HRESULT CFormInstance::Initialize(IJSONObject* unit, IJSONObject* definition)
{
	HRESULT hr;
	CRString kind, interfaceName;
	TStackRef<IJSONObject> events;
	TStackRef<IJSONValue> value;
	m_link.Attach(__new CInstanceLink);
	CheckAlloc(m_link);
	m_link->instance = this;
	Check(StringField(unit,L"kind",&kind));
	Check(StringField(unit,L"eventsInterface",&interfaceName));
	Check(m_script->GetInterface(interfaceName,&m_methods));
	if(kind == RSTRING_CAST(L"form"))
	{
		m_root = __new CFormObject(m_link);
	}
	else
	{
		m_root = __new CFrameObject(m_link);
	}
	CheckAlloc(m_root);
	if(definition->HasField(RSTRING_CAST(L"events")))
	{
		Check(definition->FindNonNullValueW(L"events",&value));
		Check(value->GetObject(&events));
		value.Release();
		if(events->HasField(RSTRING_CAST(L"OnDestroy")))
		{
			Check(StringField(events,L"OnDestroy",&m_destroyHandler));
		}
	}
	else
	{
		Check(JSONCreateObject(&events));
		Check(JSONWrapObject(events,&value));
		Check(definition->AddValueW(L"events",value));
	}
	Check(PutString(events,L"OnDestroy",RSTRING_CAST(DestroyBinding)));
Cleanup:
	return hr;
}

HRESULT CFormInstance::Resolve(RSTRING name, IQuadooDelegate** delegate)
{
	HRESULT hr;
	ULONG ordinal;
	*delegate = NULL;
	Check(m_methods->ResolveMethod(name,&ordinal));
	*delegate = __new CScriptDelegate(ordinal,m_link);
	CheckAlloc(*delegate);
Cleanup:
	return hr;
}

HRESULT CFormInstance::InvokeResolved(ULONG ordinal, QuadooVM::QVPARAMS* parameters, QuadooVM::QVARIANT* result)
{
	HRESULT hr;
	TStackRef<IQuadooInterface> methods(m_methods);
	CheckIf(m_detached || !methods, QuadooForm::EDestroyed);
	Check(methods->Invoke(ordinal, parameters, result));
Cleanup:
	return hr;
}

STDMETHODIMP CFormInstance::BindEvent(RSTRING name, IQuadooDelegate** delegate)
{
	HRESULT hr = S_OK;
	CheckIf(!delegate,E_POINTER);
	*delegate = NULL;
	if(!lstrcmpW(RStrToWide(name),DestroyBinding))
	{
		Check(SetDestroyHandler(m_destroyHandler));
		*delegate = __new CRootDestroyDelegate(m_link);
		CheckAlloc(*delegate);
	}
	else
	{
		Check(Resolve(name,delegate));
	}
Cleanup:
	return hr;
}

HRESULT CFormInstance::SetDestroyHandler(RSTRING name)
{
	HRESULT hr = S_OK;
	TStackRef<IQuadooDelegate> delegate;
	if(RStrLen(name))
	{
		Check(Resolve(name,&delegate));
	}
	m_destroyHandler = name;
	m_userDestroy = delegate;
Cleanup:
	return hr;
}

RSTRING CFormInstance::DestroyHandler()
{
	return m_destroyHandler;
}

HRESULT CFormInstance::InvokeDestroy(IQuadooEventArgs* args)
{
	TStackRef<IQuadooDelegate> user(m_userDestroy);
	return user ? user->Invoke(args) : S_OK;
}

HRESULT CFormInstance::Wrap(IQuadooComponent* component, IQuadooObject** object)
{
	HRESULT hr;
	TStackRef<IQuadooComponent> current;
	CComponentObject* created = NULL;
	*object = NULL;
	if(m_root && SUCCEEDED(m_root->GetComponent(&current)) && SameIdentity(component,current))
	{
		*object = m_root;
	}
	else
	{
		for(sysint n = 0; n < m_objects.Length(); n++)
		{
			current.Release();
			Check(m_objects[n]->GetComponent(&current));
			if(SameIdentity(component,current))
			{
				*object = m_objects[n];
				break;
			}
		}
	}
	if(!*object)
	{
		Check(NewComponentObject(m_link,component,&created));
		Check(m_objects.Append(created));
		*object = created;
		created = NULL;
	}
	(*object)->AddRef();
	hr = S_OK;
Cleanup:
	SafeRelease(created);
	return hr;
}

HRESULT CFormInstance::Assign(RSTRING name, IQuadooObject* object)
{
	HRESULT hr;
	AssignedProperty* saved = __new AssignedProperty;
	QuadooVM::QVARIANT value = {QuadooVM::Object};
	CheckAlloc(saved);
	saved->name = name;
	saved->assigned = object;
	Check(m_script->GetProperty(NULL,name,&saved->previous));
	Check(m_assigned.Append(saved));
	saved = NULL;
	value.pObject = object;
	Check(m_script->SetProperty(NULL,name,&value));
Cleanup:
	__delete saved;
	return hr;
}

HRESULT CFormInstance::Populate(IQuadooControl* control, BOOL root, UINT depth)
{
	HRESULT hr;
	TStackRef<IQuadooControlContainer> container;
	TStackRef<IQuadooFrame> frame;
	CheckIf(depth > 128,QuadooForm::EInvalidDefinition);
	if(!root)
	{
		CRString name;
		TStackRef<IQuadooObject> object;
		Check(control->GetName(&name));
		Check(Wrap(control,&object));
		Check(Assign(name,object));
		if(SUCCEEDED(control->QueryInterface(&frame)))
		{
			goto Cleanup;
		}
	}
	if(SUCCEEDED(control->QueryInterface(&container)))
	{
		UINT count;
		Check(container->GetChildCount(&count));
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IQuadooControl> child;
			Check(container->GetChild(n,&child));
			Check(Populate(child,FALSE,depth + 1));
		}
	}
Cleanup:
	return hr;
}

STDMETHODIMP CFormInstance::PostBind(IQuadooControl* control)
{
	HRESULT hr;
	TStackRef<IQuadooComponentHost> host;
	TStackRef<IQuadooComponentCollection> components;
	UINT count;
	m_root->AttachComponent(control);
	Check(m_runtime->RegisterRoot(control));
	Check(Assign(RSTRING_CAST(RootProperty),m_root));
	Check(Populate(control,TRUE));
	Check(control->QueryInterface(&host));
	Check(host->GetComponents(&components));
	Check(components->GetCount(&count));
	for(UINT n = 0; n < count; n++)
	{
		TStackRef<IQuadooComponent> component;
		TStackRef<IQuadooObject> object;
		TStackRef<IScriptComponent> provider;
		CRString name;
		Check(components->GetAt(n,&component));
		Check(component->GetName(&name));
		hr = component->QueryInterface(&provider);
		CheckIf(FAILED(hr) && hr != E_NOINTERFACE,hr);
		if(provider)
		{
			Check(provider->GetScriptObject(&object));
		}
		else
		{
			Check(Wrap(component,&object));
		}
		Check(Assign(name,object));
	}
Cleanup:
	return hr;
}

HRESULT CFormInstance::Commit()
{
	HRESULT hr;
	TStackRef<IQuadooComponent> component;
	Check(m_root->GetComponent(&component));
	Check(m_runtime->CommitRoot(component));
	m_committed = TRUE;
	for(sysint n = 0; n < m_assigned.Length(); n++)
	{
		QVMClearVariant(&m_assigned[n]->previous);
	}
Cleanup:
	return hr;
}

BOOL CFormInstance::Committed()
{
	return m_committed;
}

VOID CFormInstance::Detach()
{
	if(m_detached)
	{
		return;
	}
	m_detached = TRUE;
	if(m_link)
	{
		m_link->asyncOpen = FALSE;
		m_runtime->CancelAsync(m_link);
	}
	ClearTreeObjects(m_link);
	ClearListObjects(m_link);
	if(!m_committed)
	{
		for(sysint n = m_assigned.Length(); n > 0; n--)
		{
			AssignedProperty* property = m_assigned[n - 1];
			m_script->SetProperty(NULL,property->name,&property->previous);
		}
	}
	else if(m_script)
	{
		for(sysint n = 0; n < m_assigned.Length(); n++)
		{
			QuadooVM::QVARIANT value = {QuadooVM::Null};
			AssignedProperty* property = m_assigned[n];
			if(SUCCEEDED(m_script->GetProperty(NULL,property->name,&value)))
			{
				BOOL owned = value.eType == QuadooVM::Object && SameIdentity(value.pObject,property->assigned);
				if(owned)
				{
					QuadooVM::QVARIANT empty = {QuadooVM::Null};
					m_script->SetProperty(NULL,property->name,&empty);
				}
				QVMClearVariant(&value);
			}
		}
	}
	if(m_link)
	{
		m_link->instance = NULL;
	}
	m_assigned.DeleteAll();
	for(sysint n = 0; n < m_objects.Length(); n++)
	{
		m_objects[n]->Release();
	}
	m_objects.Clear();
	SafeRelease(m_root);
	m_methods.Release();
	m_userDestroy.Release();
	if(m_script)
	{
		// Native ownership must use the VM's object-release protocol as well:
		// the final Release queues a script destructor before freeing the object.
		QVMReleaseObject(m_script.Detach());
	}
}

BOOL CFormInstance::Destroyed()
{
	TStackRef<IQuadooComponent> component;
	QuadooForm::ComponentState state;
	return m_root && SUCCEEDED(m_root->GetComponent(&component)) && SUCCEEDED(component->GetState(&state)) && state == QuadooForm::Destroyed;
}

STDMETHODIMP CFormInstance::HandleMessage(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result)
{
	HRESULT hr = S_OK;
	TStackRef<CFormInstance> keepAlive(this);
	CheckIf(!handled || !result,E_POINTER);
	*handled = FALSE;
	*result = 0;
	if(!m_detached)
	{
		Check(m_runtime->HandleContainerMessage(window,message,wp,lp,handled,result));
	}
Cleanup:
	return hr;
}

IQuadooObject* CFormInstance::Script()
{
	return m_script;
}

CContainerObject* CFormInstance::Root()
{
	return m_root;
}

CApplicationRuntime* CFormInstance::Runtime()
{
	return m_runtime;
}
