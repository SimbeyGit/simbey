#include "Runtime.h"
#include "ImageButton.h"
#include "ProgressBar.h"
#include "StatusBar.h"
#include "Canvas.h"
#include "DateTimePicker.h"
#include "Calendar.h"
#include "..\Diagnostics.h"

namespace QF
{
	CFactory::CFactory(const TypeSpec* type) : m_type(type), m_thread(GetCurrentThreadId()), m_instances(0), m_properties(NULL), m_events(NULL) {}
	CFactory::~CFactory() { SafeRelease(m_properties); SafeRelease(m_events); }

	HRESULT CFactory::Initialize()
	{
		HRESULT hr;
		CheckIf(!m_type || !m_type->name || !*m_type->name, QuadooForm::EInvalidDefinition);
		Check(ComposeMetadata(m_type, m_specs, m_eventSpecs));
		Check(CreateDescriptors(m_specs, m_eventSpecs, &m_properties)); Check(CreateEventDescriptors(m_eventSpecs, &m_events));
		Check(ValidateDescriptors(m_properties, m_events));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CFactory::GetTypeName(RSTRING* name) { return name ? RStrCreateW(TStrLenAssert(m_type->name), m_type->name, name) : E_POINTER; }
	STDMETHODIMP CFactory::GetProperties(IQuadooPropertyDescriptors** properties)
	{
		HRESULT hr = S_OK;
		CheckIf(!properties, E_POINTER); *properties = NULL; CheckIf(m_thread != GetCurrentThreadId(), QuadooForm::EWrongThread);
		CheckIf(!m_properties, QuadooForm::EInvalidState);
		*properties = m_properties; (*properties)->AddRef();
	Cleanup:
		return hr;
	}
	STDMETHODIMP CFactory::GetEvents(IQuadooEventDescriptors** events)
	{
		HRESULT hr = S_OK;
		CheckIf(!events, E_POINTER); *events = NULL; CheckIf(m_thread != GetCurrentThreadId(), QuadooForm::EWrongThread);
		CheckIf(!m_events, QuadooForm::EInvalidState);
		*events = m_events; (*events)->AddRef();
	Cleanup:
		return hr;
	}
	STDMETHODIMP CFactory::GetLiveCount(UINT* count)
	{
		HRESULT hr = S_OK;
		CheckIf(!count, E_POINTER); *count = 0; CheckIf(m_thread != GetCurrentThreadId(), QuadooForm::EWrongThread); *count = m_instances;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CFactory::CanCreate(IQuadooControlContainer* parent, BOOL* allowed)
	{
		HRESULT hr = S_OK;
		CheckIf(!allowed,E_POINTER); *allowed = FALSE; CheckIf(m_thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
		*allowed = AcceptParent(parent);
	Cleanup:
		return hr;
	}
	STDMETHODIMP CFactory::CreateControl(IQuadooConstructionContext* context, IQuadooControlContainer* parent,
		IJSONObject* definition, IQuadooControl** control)
	{
		HRESULT hr;
		TStackRef<CControl> object;
		CheckIf(!control, E_POINTER); *control = NULL;
		CheckIf(m_thread != GetCurrentThreadId(), QuadooForm::EWrongThread);
		CheckIf(!context || !definition || !AcceptParent(parent), E_INVALIDARG);
		object.Attach(Allocate(context, parent)); CheckAlloc(object);
		Check(object->Construct(definition));
		*control = object->Control(); (*control)->AddRef();
	Cleanup:
		return hr;
	}

	class CRegistry :
		public CBaseUnknown,
		public IQuadooControlRegistry,
		public IQuadooFactoryCatalog,
		public IQuadooComponentRegistry
	{
		DWORD m_thread;
		TRStrMap<IQuadooControlFactory*> m_factories;
		TRStrMap<IQuadooComponentFactory*> m_components;
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooControlRegistry)
			UNK_INTERFACE(IQuadooFactoryCatalog)
			UNK_INTERFACE(IQuadooComponentRegistry)
		END_UNK_MAP

		CRegistry() : m_thread(GetCurrentThreadId()), m_factories(RStrCompareRStr), m_components(RStrCompareRStr) {}
		~CRegistry() { for(sysint n = 0; n < m_factories.Length(); n++) (*m_factories.GetValuePtr(n))->Release(); for(sysint n = 0; n < m_components.Length(); n++) (*m_components.GetValuePtr(n))->Release(); }

		STDMETHOD(RegisterComponentFactory)(IQuadooComponentFactory* factory)
		{
			HRESULT hr; CRString name; IQuadooControlFactory* visual;
			CheckIf(m_thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
			CheckIf(!factory,E_INVALIDARG);
			Check(factory->GetTypeName(&name));
			CheckIf(!RStrIsWide(static_cast<RSTRING>(name)) || !RStrLen(name),E_INVALIDARG);
			CheckIf(SUCCEEDED(m_factories.Find(name,&visual)),HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS));
			Check(ValidateComponentFactory(factory));
			Check(m_components.Add(name,factory)); factory->AddRef();
		Cleanup:
			return hr;
		}

		STDMETHOD(UnregisterComponentFactory)(RSTRING name)
		{
			HRESULT hr; IQuadooComponentFactory* factory; TStackRef<IQuadooFactoryLifetime> lifetime; UINT count;
			CheckIf(m_thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
			CheckIf(!name || !RStrIsWide(name),E_INVALIDARG);
			Check(m_components.Find(name,&factory));
			Check(factory->QueryInterface(__uuidof(IQuadooFactoryLifetime),reinterpret_cast<PVOID*>(&lifetime)));
			Check(lifetime->GetLiveCount(&count));
			CheckIf(count,HRESULT_FROM_WIN32(ERROR_BUSY));
			Check(m_components.Remove(name,NULL)); factory->Release();
		Cleanup:
			return hr;
		}

		STDMETHOD(FindComponentFactory)(RSTRING name, IQuadooComponentFactory** factory)
		{
			HRESULT hr;
			CheckIf(!factory,E_POINTER); *factory = NULL;
			CheckIf(m_thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
			CheckIf(!name || !RStrIsWide(name),E_INVALIDARG);
			Check(m_components.Find(name,factory)); (*factory)->AddRef();
		Cleanup:
			return hr;
		}

		STDMETHOD(GetComponentFactoryCount)(UINT* count)
		{
			if(!count) return E_POINTER; *count = 0;
			if(m_thread != GetCurrentThreadId()) return QuadooForm::EWrongThread;
			*count = static_cast<UINT>(m_components.Length()); return S_OK;
		}

		STDMETHOD(GetComponentFactory)(UINT index, IQuadooComponentFactory** factory)
		{
			HRESULT hr = S_OK;
			CheckIf(!factory,E_POINTER); *factory = NULL;
			CheckIf(m_thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
			CheckIf(index >= static_cast<UINT>(m_components.Length()),HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
			*factory = *m_components.GetValuePtr(index); (*factory)->AddRef();
		Cleanup:
			return hr;
		}

		STDMETHOD(GetFactoryCount)(UINT* count)
		{
			HRESULT hr = S_OK;
			CheckIf(!count,E_POINTER); *count = 0; CheckIf(m_thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
			*count = static_cast<UINT>(m_factories.Length());
		Cleanup:
			return hr;
		}

		STDMETHOD(GetFactory)(UINT index, IQuadooControlFactory** factory)
		{
			HRESULT hr = S_OK;
			CheckIf(!factory,E_POINTER); *factory = NULL; CheckIf(m_thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
			CheckIf(index >= static_cast<UINT>(m_factories.Length()),HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
			*factory = *m_factories.GetValuePtr(index); (*factory)->AddRef();
		Cleanup:
			return hr;
		}

		HRESULT Initialize()
		{
			HRESULT hr = S_OK;
			{ TStackRef<CFactory> factory; factory.Attach(__new TFactory<CForm>); CheckAlloc(factory); Check(factory->Initialize()); Check(RegisterFactory(factory)); }
			{ TStackRef<CFactory> factory; factory.Attach(__new TFactory<CFrame>); CheckAlloc(factory); Check(factory->Initialize()); Check(RegisterFactory(factory)); }
			{ TStackRef<CFactory> factory; factory.Attach(__new TFactory<CPanel>); CheckAlloc(factory); Check(factory->Initialize()); Check(RegisterFactory(factory)); }
			{ TStackRef<CFactory> factory; factory.Attach(__new TFactory<CScrollingPanel>); CheckAlloc(factory); Check(factory->Initialize()); Check(RegisterFactory(factory)); }
			{ TStackRef<CFactory> factory; factory.Attach(__new TFactory<CLabel>); CheckAlloc(factory); Check(factory->Initialize()); Check(RegisterFactory(factory)); }
			{
				TStackRef<CFactory> factory;
				factory.Attach(__new TFactory<CColorSwatch>);
				CheckAlloc(factory);
				Check(factory->Initialize());
				Check(RegisterFactory(factory));
			}
			{
				TStackRef<CFactory> factory;
				factory.Attach(__new TFactory<CImage>);
				CheckAlloc(factory);
				Check(factory->Initialize());
				Check(RegisterFactory(factory));
			}
			{ TStackRef<CFactory> factory; factory.Attach(__new TFactory<CButton>); CheckAlloc(factory); Check(factory->Initialize()); Check(RegisterFactory(factory)); }
			{ TStackRef<CFactory> factory; factory.Attach(__new TFactory<CTextBox>); CheckAlloc(factory); Check(factory->Initialize()); Check(RegisterFactory(factory)); }
			{ TStackRef<CFactory> factory; factory.Attach(__new TFactory<CMemo>); CheckAlloc(factory); Check(factory->Initialize()); Check(RegisterFactory(factory)); }
			{ TStackRef<CFactory> factory; factory.Attach(__new TFactory<CCheckBox>); CheckAlloc(factory); Check(factory->Initialize()); Check(RegisterFactory(factory)); }
			{ TStackRef<CFactory> factory; factory.Attach(__new TFactory<CRadioButton>); CheckAlloc(factory); Check(factory->Initialize()); Check(RegisterFactory(factory)); }
			{ TStackRef<CFactory> factory; factory.Attach(__new TFactory<CGroupBox>); CheckAlloc(factory); Check(factory->Initialize()); Check(RegisterFactory(factory)); }
			{ TStackRef<CFactory> factory; factory.Attach(__new TFactory<CComboBox>); CheckAlloc(factory); Check(factory->Initialize()); Check(RegisterFactory(factory)); }
			{ TStackRef<CFactory> factory; factory.Attach(__new TFactory<CListBox>); CheckAlloc(factory); Check(factory->Initialize()); Check(RegisterFactory(factory)); }
			{ TStackRef<CFactory> factory; factory.Attach(__new TFactory<CTabControl>); CheckAlloc(factory); Check(factory->Initialize()); Check(RegisterFactory(factory)); }
			{ TStackRef<CFactory> factory; factory.Attach(__new TFactory<CTabPage>); CheckAlloc(factory); Check(factory->Initialize()); Check(RegisterFactory(factory)); }
			Check(RegisterTreeViewFactory(this));
			Check(RegisterListViewFactory(this));
			Check(RegisterImageButtonFactory(this));
			Check(RegisterProgressBarFactory(this));
			Check(RegisterStatusBarFactory(this));
			Check(RegisterCanvasFactory(this));
			Check(RegisterDateTimePickerFactory(this));
			Check(RegisterCalendarFactory(this));
			Check(RegisterNonvisualFactories(this));
			Check(RegisterTimerFactory(this));
			Check(RegisterMenuFactories(this));
	Cleanup:
			return hr;
		}
		STDMETHOD(RegisterFactory)(IQuadooControlFactory* factory)
		{
			HRESULT hr;
			CRString name;
			CheckIf(m_thread != GetCurrentThreadId(), QuadooForm::EWrongThread); CheckIf(!factory, E_INVALIDARG);
			Check(factory->GetTypeName(&name)); CheckIf(!RStrIsWide(static_cast<RSTRING>(name)) || !RStrLen(name), E_INVALIDARG);
			{ IQuadooComponentFactory* component; CheckIf(SUCCEEDED(m_components.Find(name,&component)),HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)); }
			Check(ValidateFactory(factory));
			Check(m_factories.Add(name, factory)); factory->AddRef();
	Cleanup:
			return hr;
		}
		STDMETHOD(UnregisterFactory)(RSTRING name)
		{
			HRESULT hr;
			IQuadooControlFactory* factory;
			TStackRef<IQuadooFactoryLifetime> lifetime;
			UINT count;
			CheckIf(m_thread != GetCurrentThreadId(), QuadooForm::EWrongThread); CheckIf(!name || !RStrIsWide(name), E_INVALIDARG);
			Check(m_factories.Find(name, &factory));
			Check(factory->QueryInterface(__uuidof(IQuadooFactoryLifetime), reinterpret_cast<PVOID*>(&lifetime)));
			Check(lifetime->GetLiveCount(&count)); CheckIf(count, HRESULT_FROM_WIN32(ERROR_BUSY));
			Check(m_factories.Remove(name, NULL)); factory->Release();
	Cleanup:
			return hr;
		}
		STDMETHOD(FindFactory)(RSTRING name, IQuadooControlFactory** factory)
		{
			HRESULT hr;
			CheckIf(!factory, E_POINTER); *factory = NULL;
			CheckIf(m_thread != GetCurrentThreadId(), QuadooForm::EWrongThread); CheckIf(!name || !RStrIsWide(name), E_INVALIDARG);
			Check(m_factories.Find(name, factory)); (*factory)->AddRef();
	Cleanup:
			return hr;
		}
	};

	class CContext : public CBaseUnknown, public IQuadooConstructionContext, public QuadooPrivate::IDiagnosticReport
	{
		DWORD m_thread;
		QuadooForm::ContextMode m_mode;
		UINT m_dpi;
		HWND m_host;
		IQuadooControlRegistry* m_registry;
		IQuadooEventsBinder* m_binder;
		IQuadooConstructionReport* m_report;
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooConstructionContext)
			UNK_INTERFACE(QuadooPrivate::IDiagnosticReport)
		END_UNK_MAP

		CContext (const QuadooForm::ContextOptions* options) :
			m_thread(GetCurrentThreadId()),
			m_mode(options->Mode),
			m_dpi(options->DPI),
			m_host(options->hwndHost)
		{
			SetInterface(m_registry, options->pRegistry);
			SetInterface(m_binder, options->pBinder);
			SetInterface(m_report, options->pReport);
		}

		~CContext ()
		{
			SafeRelease(m_registry);
			SafeRelease(m_binder);
			SafeRelease(m_report);
		}

		HRESULT Initialize ()
		{
			HRESULT hr = S_OK;
			if(!m_registry)
			{
				Check(QuadooGetControlRegistry(&m_registry));
			}
	Cleanup:
			return hr;
		}

		STDMETHOD(GetMode)(QuadooForm::ContextMode* mode) { HRESULT hr = m_thread == GetCurrentThreadId() ? S_OK : QuadooForm::EWrongThread; if(SUCCEEDED(hr)) { if(mode) *mode = m_mode; else hr = E_POINTER; } return hr; }
		STDMETHOD(GetDPI)(UINT* dpi) { HRESULT hr = m_thread == GetCurrentThreadId() ? S_OK : QuadooForm::EWrongThread; if(SUCCEEDED(hr)) { if(dpi) *dpi = m_dpi; else hr = E_POINTER; } return hr; }
		STDMETHOD(GetHostHandle)(HWND* hwnd) { HRESULT hr = m_thread == GetCurrentThreadId() ? S_OK : QuadooForm::EWrongThread; if(SUCCEEDED(hr)) { if(hwnd) *hwnd = m_host; else hr = E_POINTER; } return hr; }
		STDMETHOD(GetRegistry)(IQuadooControlRegistry** registry)
		{
			HRESULT hr = S_OK;
			CheckIf(!registry, E_POINTER);
			*registry = NULL;
			CheckIf(m_thread != GetCurrentThreadId(), QuadooForm::EWrongThread);
			*registry = m_registry;
			(*registry)->AddRef();
	Cleanup:
			return hr;
		}

		STDMETHOD(GetBinder)(IQuadooEventsBinder** binder)
		{
			HRESULT hr = S_OK;
			CheckIf(!binder, E_POINTER);
			*binder = NULL;
			CheckIf(m_thread != GetCurrentThreadId(), QuadooForm::EWrongThread);
			SetInterface(*binder, m_binder);
	Cleanup:
			return hr;
		}

		STDMETHOD(ReportError)(HRESULT error, RSTRING path, RSTRING detail)
		{
			HRESULT hr = m_thread == GetCurrentThreadId() ? S_OK : QuadooForm::EWrongThread;
			if(SUCCEEDED(hr) && m_report)
				hr = m_report->OnError(error, path, detail);
			return hr;
		}
		STDMETHOD(ReportDiagnostic)(HRESULT error, RSTRING path, RSTRING detail)
		{
			HRESULT hr;
			TStackRef<QuadooPrivate::IDiagnosticReport> report;
			CheckIf(m_thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
			if(m_report)
			{
				Check(QueryOptional(m_report,&report));
				if(report)
				{
					Check(report->ReportDiagnostic(error,path,detail));
				}
				else
				{
					Check(m_report->OnError(error,path,detail));
				}
			}
		Cleanup:
			return hr;
		}
	};
}

HRESULT WINAPI QuadooGetControlRegistry (IQuadooControlRegistry** registry)
{
	HRESULT hr;
	TStackRef<QF::CRegistry> object;

	CheckIf(!registry, E_POINTER); *registry = NULL;
	object.Attach(__new QF::CRegistry);
	CheckAlloc(object);
	Check(object->Initialize());
	*registry = object.Detach();

Cleanup:
	return hr;
}

HRESULT WINAPI QuadooCreateConstructionContext (const QuadooForm::ContextOptions* options, IQuadooConstructionContext** context)
{
	HRESULT hr;
	TStackRef<QF::CContext> object;

	CheckIf(!context, E_POINTER); *context = NULL;
	CheckIf(!options || options->cbSize != sizeof(*options) || options->Mode < QuadooForm::Runtime || options->Mode > QuadooForm::Preview, E_INVALIDARG);
	CheckIf(options->DPI && (options->DPI < 48 || options->DPI > 768), E_INVALIDARG);
	CheckIf(options->hwndHost && (!IsWindow(options->hwndHost) || GetWindowThreadProcessId(options->hwndHost, NULL) != GetCurrentThreadId()), E_INVALIDARG);
	object.Attach(__new QF::CContext(options));
	CheckAlloc(object);
	Check(object->Initialize());
	*context = object.Detach();

Cleanup:
	return hr;
}
