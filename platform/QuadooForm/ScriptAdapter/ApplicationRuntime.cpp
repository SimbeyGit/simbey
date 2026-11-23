#include "RuntimeObjects.h"
#include "AsyncCalls.h"
#include "Library\Core\StringCore.h"
#include "Library\Util\Formatting.h"

using namespace AQVM;

class CDefinitionObject : public CBaseUnknown, public CQuadooObjectImpl
{
	TStackRef<CApplicationRuntime> m_runtime;
	TStackRef<IJSONObject> m_unit;
	TStackRef<IJSONObject> m_definition;
	BOOL m_creating;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooObject)
	END_UNK_MAP

	CDefinitionObject(CApplicationRuntime* runtime, IJSONObject* unit, IJSONObject* definition) :
		m_runtime(runtime), m_unit(unit), m_definition(definition), m_creating(FALSE)
	{
	}

	STDMETHOD(Invoke)(IQuadooVM*, RSTRING method, QuadooVM::QVPARAMS* parameters, QuadooVM::QVARIANT* result)
	{
		HRESULT hr;
		TStackRef<CDefinitionObject> keepAlive(this);
		IQuadooObject* owner = NULL;
		BOOL entered = FALSE;
		CheckIf(!parameters || !result,E_POINTER);
		result->eType = QuadooVM::Null;
		Check(m_runtime->CheckThread());
		CheckIf(!method || lstrcmpW(RStrToWide(method),L"Create"),DISP_E_MEMBERNOTFOUND);
		CheckIf(m_creating,QuadooForm::EInvalidState);
		CheckIf(parameters->cArgs < 1 || parameters->cArgs > 2,DISP_E_BADPARAMCOUNT);
		CheckIf(parameters->pqvArgs[parameters->cArgs - 1].eType != QuadooVM::Object,DISP_E_TYPEMISMATCH);
		if(parameters->cArgs == 2 && parameters->pqvArgs[0].eType != QuadooVM::Null)
		{
			CheckIf(parameters->pqvArgs[0].eType != QuadooVM::Object,DISP_E_TYPEMISMATCH);
			owner = parameters->pqvArgs[0].pObject;
		}
		m_creating = TRUE;
		entered = TRUE;
		Check(m_runtime->Create(m_unit,m_definition,parameters->pqvArgs[parameters->cArgs - 1].pObject,owner,&result->pObject));
		result->eType = QuadooVM::Object;
	Cleanup:
		if(entered)
		{
			m_creating = FALSE;
		}
		return hr;
	}
};

CApplicationRuntime::CApplicationRuntime() :
	m_module(NULL), m_definitions(NULL), m_thread(GetCurrentThreadId()), m_lifetimeLink(NULL), m_async(NULL), m_dispatch(NULL), m_checkRetry(FALSE),
	m_pumping(FALSE), m_stopping(FALSE), m_entryActive(FALSE), m_started(FALSE),
	m_cleaning(FALSE), m_shuttingDown(FALSE), m_checkPending(FALSE), m_exitCommitted(FALSE),
	m_creationDepth(0), m_callbackDepth(0), m_leases(0), m_generation(0), m_collectOrdinal(0), m_hrError(S_OK)
{
}

CApplicationRuntime::~CApplicationRuntime()
{
	Shutdown();
}

HRESULT CApplicationRuntime::CheckThread()
{
	return m_thread == GetCurrentThreadId() ? S_OK : QuadooForm::EWrongThread;
}

HRESULT CApplicationRuntime::Initialize(HINSTANCE module, IJSONObject* catalog, IQuadooControlRegistry* registry,
	IDefinitionSource* definitions)
{
	HRESULT hr;
	Check(CheckThread());
	CheckIf(m_catalog,QuadooForm::EInvalidState);
	Check(ValidateCatalog(catalog));
	Check(CopyObject(catalog,&m_catalog));
	m_module = module;
	m_definitions = definitions;
	if(registry)
	{
		m_registry = registry;
	}
	else
	{
		Check(QuadooGetControlRegistry(&m_registry));
	}
	Check(InitializeLifetime());
	m_async = __new CAsyncCalls(this);
	CheckAlloc(m_async);
Cleanup:
	return hr;
}

HRESULT CApplicationRuntime::LoadDefinition(RSTRING className, IQuadooObject* script, IQuadooObject** loader)
{
	HRESULT hr;
	TStackRef<IJSONObject> unit, definition;
	CRString kind, nativeType, resourceName;
	CheckIf(!loader,E_POINTER);
	*loader = NULL;
	Check(CheckThread());
	CheckIf(!m_catalog || !script || m_shuttingDown,QuadooForm::EInvalidState);
	Check(FindUnit(m_catalog,className,&unit));
	Check(StringField(unit,L"kind",&kind));
	if(m_definitions)
	{
		Check(m_definitions->Lookup(className,&definition));
	}
	else
	{
		Check(ClassResourceName(className,&resourceName));
		Check(ResourceObject(m_module,kind == RSTRING_CAST(L"form") ? FormType : FrameType,resourceName.c_wstr(),&definition));
	}
	Check(StringField(definition,L"type",&nativeType));
	CheckIf(lstrcmpW(nativeType.c_wstr(),kind == RSTRING_CAST(L"form") ? L"Form" : L"Frame"),QuadooForm::EInvalidDefinition);
	*loader = __new CDefinitionObject(this,unit,definition);
	CheckAlloc(*loader);
Cleanup:
	return hr;
}

HRESULT CApplicationRuntime::Create(IJSONObject* unit, IJSONObject* definition, IQuadooObject* script,
	IQuadooObject* owner, IQuadooObject** root)
{
	HRESULT hr;
	TStackRef<CFormInstance> instance;
	TStackRef<IJSONObject> snapshot;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IQuadooControl> created;
	TStackRef<IQuadooComponent> ownerComponent;
	TStackRef<IQuadooControlContainer> parent;
	TStackRef<IQuadooWinControl> ownerWindow;
	CRString kind;
	QuadooForm::ContextOptions options = {sizeof(options),QuadooForm::Runtime};
	BOOL registered = FALSE;
	BOOL constructing = FALSE;
	CheckIf(!root,E_POINTER);
	*root = NULL;
	Check(CheckThread());
	CheckIf(!m_catalog || m_stopping,QuadooForm::EInvalidState);
	if(!m_creationDepth)
	{
		m_diagnostic = static_cast<RSTRING>(NULL);
	}
	m_creationDepth++;
	constructing = TRUE;
	for(sysint n = 0; n < m_instances.Length(); n++)
	{
		CheckIf(SameIdentity(script,m_instances[n]->Script()),QuadooForm::EInvalidState);
	}
	Check(StringField(unit,L"kind",&kind));
	if(owner)
	{
		Check(Unwrap(owner,&ownerComponent));
		Check(ownerComponent->QueryInterface(&ownerWindow));
		Check(ownerWindow->GetHandle(&options.hwndHost));
	}
	if(kind == RSTRING_CAST(L"frame"))
	{
		CheckIf(!ownerComponent,E_INVALIDARG);
		Check(ownerComponent->QueryInterface(&parent));
	}
	Check(CopyObject(definition,&snapshot));
	if(parent)
	{
		GUID instanceID;
		WCHAR guid[40];
		CRString className, runtimeName;
		Check(CoCreateGuid(&instanceID));
		CheckIf(!StringFromGUID2(instanceID,guid,ARRAYSIZE(guid)),E_FAIL);
		Check(StringField(unit,L"class",&className));
		Check(RStrFormatW(&runtimeName,L"%.180ls_%ls",className.c_wstr(),guid));
		Check(PutString(snapshot,L"name",runtimeName));
	}
	instance.Attach(__new CFormInstance(this,script));
	CheckAlloc(instance);
	Check(instance->Initialize(unit,snapshot));
	Check(m_instances.Append(instance));
	instance->AddRef();
	registered = TRUE;
	options.pRegistry = m_registry;
	options.pBinder = instance;
	options.pReport = this;
	Check(QuadooCreateConstructionContext(&options,&context));
	Check(QuadooCreateControl(context,parent,snapshot,&created));
	Check(instance->Commit());
	*root = instance->Root();
	(*root)->AddRef();
Cleanup:
	if(constructing)
	{
		if(FAILED(hr) && !RStrLen(m_diagnostic))
		{
			OnError(hr,RSTRING_CAST(L"Create"),RSTRING_CAST(L"Form/frame construction failed; check its properties and event bindings."));
		}
		if(SUCCEEDED(hr) && m_creationDepth == 1)
		{
			m_diagnostic = static_cast<RSTRING>(NULL);
		}
	}
	if(FAILED(hr) && instance)
	{
		if(created)
		{
			created->Destroy();
		}
		if(registered)
		{
			for(sysint n = 0; n < m_instances.Length(); n++)
			{
				if(m_instances[n] == static_cast<CFormInstance*>(instance))
				{
					m_instances.Remove(n,NULL);
					instance->Release();
					break;
				}
			}
		}
		{
			TStackRef<IQuadooComponent> component;
			if(instance->Root() && SUCCEEDED(instance->Root()->GetComponent(&component)))
			{
				UnregisterRoot(component);
			}
		}
		instance->Detach();
	}
	if(constructing)
	{
		m_creationDepth--;
		QueueLifetimeCheck();
	}
	return hr;
}

HRESULT CApplicationRuntime::Unwrap(IQuadooObject* object, IQuadooComponent** component)
{
	HRESULT hr;
	TStackRef<IComponentObject> wrapper;
	CheckIf(!component,E_POINTER);
	*component = NULL;
	Check(CheckThread());
	CheckIf(!object,E_INVALIDARG);
	if(SUCCEEDED(object->QueryInterface(&wrapper)))
	{
		Check(wrapper->GetComponent(component));
	}
	else
	{
		for(sysint n = 0; n < m_instances.Length(); n++)
		{
			if(SameIdentity(object,m_instances[n]->Script()))
			{
				Check(m_instances[n]->Root()->GetComponent(component));
				break;
			}
		}
		CheckIf(!*component,E_NOINTERFACE);
	}
Cleanup:
	return hr;
}

HRESULT CApplicationRuntime::ShowForm(IQuadooObject* script, INT command)
{
	HRESULT hr;
	TStackRef<IQuadooComponent> component;
	TStackRef<IQuadooForm> form;
	BOOL entered = FALSE;
	Check(CheckThread());
	CheckIf(!script || command < SW_HIDE || command > SW_FORCEMINIMIZE, E_INVALIDARG);
	CheckIf(m_stopping, QuadooForm::EInvalidState);
	Check(EnterCallback());
	entered = TRUE;
	for(sysint n = 0; n < m_instances.Length(); n++)
	{
		if(SameIdentity(script,m_instances[n]->Script()))
		{
			Check(m_instances[n]->Root()->GetComponent(&component));
			Check(component->QueryInterface(&form));
			if(m_instances[n]->Destroyed())
			{
				hr = S_FALSE;
				goto Cleanup;
			}
			break;
		}
	}
	CheckIf(!form, QuadooForm::EInvalidState);
	Check(form->ShowForm(command));
Cleanup:
	if(entered)
	{
		LeaveCallback();
	}
	return hr;
}

HRESULT CApplicationRuntime::Preprocess(MSG* message, BOOL* handled)
{
	HRESULT hr;
	BOOL entered = FALSE;
	Check(CheckThread());
	CheckIf(!message || !handled,E_POINTER);
	*handled = FALSE;
	Sweep();
	Check(EnterCallback());
	entered = TRUE;
	for(sysint n = m_instances.Length(); n > 0; n--)
	{
		TStackRef<CFormInstance> instance(m_instances[n - 1]);
		TStackRef<IQuadooComponent> component;
		TStackRef<IQuadooFormKeyboard> keyboard;
		TStackRef<IQuadooWinControl> window;
		HWND handle;
		if(!instance->Committed() || instance->Destroyed())
		{
			continue;
		}
		Check(instance->Root()->GetComponent(&component));
		if(FAILED(component->QueryInterface(&keyboard)))
		{
			continue;
		}
		Check(component->QueryInterface(&window));
		Check(window->GetHandle(&handle));
		if(message->hwnd == handle || IsChild(handle,message->hwnd))
		{
			Check(keyboard->PreprocessMessage(message,handled));
			break;
		}
	}
	hr = S_OK;
Cleanup:
	if(entered)
	{
		LeaveCallback();
	}
	return hr;
}

VOID CApplicationRuntime::Shutdown()
{
	TStackRef<CAsyncCalls> calls;
	m_pumping = FALSE;
	m_stopping = TRUE;
	m_shuttingDown = TRUE;
	m_definitions = NULL;
	m_entryActive = FALSE;
	calls.Attach(m_async);
	m_async = NULL;
	if(calls)
	{
		calls->Shutdown();
		calls.Release();
	}
	ShutdownLifetime();
	while(m_instances.Length())
	{
		TStackRef<CFormInstance> instance;
		instance.Attach(m_instances[m_instances.Length() - 1]);
		m_instances.Remove(m_instances.Length() - 1, NULL);
		instance->Detach();
	}
}

HRESULT CApplicationRuntime::Error() const
{
	return m_hrError;
}

RSTRING CApplicationRuntime::Diagnostic() const
{
	return m_diagnostic;
}

STDMETHODIMP CApplicationRuntime::OnError(HRESULT error, RSTRING path, RSTRING detail)
{
	if(FAILED(error) && m_creationDepth)
	{
		if(!RStrLen(m_diagnostic))
		{
			RStrFormatW(&m_diagnostic,L"%r: %r (0x%.8x)",path,detail,error);
		}
		return S_OK;
	}
	if(FAILED(error) && SUCCEEDED(m_hrError))
	{
		m_hrError = error;
		m_diagnostic = static_cast<RSTRING>(NULL);
		RStrFormatW(&m_diagnostic,L"%r: %r (0x%.8x)",path,detail,error);
		m_stopping = TRUE;
		QueueLifetimeCheck();
	}
	return S_OK;
}

STDMETHODIMP CApplicationRuntime::ReportDiagnostic(HRESULT error, RSTRING path, RSTRING detail)
{
	HRESULT hr;
	Check(CheckThread());
	if(FAILED(error) && SUCCEEDED(m_hrError) && !RStrLen(m_diagnostic))
	{
		// Preserve the useful script exception instead of overwriting it with
		// the native notification's subsequent generic error report.
		Check(RStrFormatW(&m_diagnostic,L"%r: %r (0x%.8x)",path,detail,error));
	}
Cleanup:
	return hr;
}
