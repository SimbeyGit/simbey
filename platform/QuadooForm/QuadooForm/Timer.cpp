#include "Runtime.h"
#include "ComponentRuntime.h"

namespace QF
{
static const PropertySpec timerProperties[] =
{
	{ L"Enabled",QuadooProperty::Boolean,QuadooForm::PropertySerializable,1,1 },
	{ L"Interval",QuadooProperty::Int,QuadooForm::PropertySerializable,1000,2 }
};
static const EventSpec timerEvents[] = { { L"OnTimer",0,0,NULL,2 } };
static const TypeSpec timerType = { L"Timer",&NonvisualType,timerProperties,ARRAYSIZE(timerProperties),timerEvents,ARRAYSIZE(timerEvents),IDI_PALETTE_TIMER };
static volatile LONG nextTimerID = 0x51460000;

class CTimer : public CNonvisual, public IQuadooTimer, public IQuadooMessageHandler
{
	HWND m_timerWindow;
	UINT_PTR m_timerID;
	BOOL m_active, m_ticking;
	VOID Stop()
	{
		if(m_timerID && m_timerWindow) KillTimer(m_timerWindow,m_timerID);
		m_timerID = 0; m_timerWindow = NULL;
	}
	HRESULT Configure()
	{
		HRESULT hr;
		TStackRef<IQuadooControlContainer> root; HWND window; UINT_PTR id;
		TStackRef<ITimerStartCheck> startCheck;
		if(!m_active || m_mode != QuadooForm::Runtime || !Boolean(L"Enabled") || !Integer(L"Interval")) { Stop(); hr = S_OK; goto Cleanup; }
		CheckIf(!m_owner,QuadooForm::EDestroyed);
		Check(m_owner->GetRoot(&root)); Check(root->GetHandle(&window));
		CheckIf(!window || !IsWindow(window),QuadooForm::EDestroyed);
		Check(QueryOptional(m_context,&startCheck));
		if(startCheck) { Check(startCheck->BeforeStart(window,Integer(L"Interval"))); Check(CheckAlive()); }
		id = static_cast<UINT_PTR>(static_cast<DWORD>(InterlockedIncrement(&nextTimerID)));
		if(!id) id = static_cast<UINT_PTR>(static_cast<DWORD>(InterlockedIncrement(&nextTimerID)));
		// A new ID on every restart makes already-queued old WM_TIMER messages
		// harmless. Create before retiring the old timer for property rollback.
		CheckIf(!SetTimer(window,id,Integer(L"Interval"),NULL),E_OUTOFMEMORY);
		Stop(); m_timerWindow = window; m_timerID = id;
		hr = S_OK;
Cleanup:
		return hr;
	}
protected:
	virtual HRESULT ValidateProperty(PCWSTR name, IJSONValue* value)
	{
		HRESULT hr = S_OK;
		if(!TStrCmpAssert(name,L"Interval")) { INT number; Check(value->GetInteger(&number)); CheckIf(number < 0,E_INVALIDARG); }
	Cleanup:
		return hr;
	}
	virtual HRESULT ChangedProperty(PCWSTR name) { return m_state == QuadooForm::Ready && (!TStrCmpAssert(name,L"Enabled") || !TStrCmpAssert(name,L"Interval")) ? Configure() : S_OK; }
	virtual HRESULT Activated() { m_active = TRUE; return Configure(); }
	virtual HRESULT DestroyObject() { m_active = FALSE; Stop(); return S_OK; }
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooTimer)
		UNK_INTERFACE(IQuadooMessageHandler)
		UNK_CHAIN_TO(CNonvisual)
	END_UNK_MAP
	static const TypeSpec* Type() { return &timerType; }
	CTimer(IQuadooConstructionContext* context, IQuadooComponentCollection* owner, IUnknown* factory, UINT* live) : CNonvisual(context,owner,factory,live), m_timerWindow(NULL), m_timerID(0), m_active(FALSE), m_ticking(FALSE) {}
	~CTimer() { Stop(); }
	STDMETHOD(GetEnabled)(BOOL* enabled) { return GetValueBool(Find(L"Enabled"),enabled); }
	STDMETHOD(SetEnabled)(BOOL enabled) { return SetValueBool(Find(L"Enabled"),enabled); }
	STDMETHOD(GetInterval)(UINT* interval)
	{
		HRESULT hr; INT number;
		CheckIf(!interval,E_POINTER); *interval = 0;
		Check(GetValueInt(Find(L"Interval"),&number)); *interval = number;
	Cleanup:
		return hr;
	}
	STDMETHOD(SetInterval)(UINT interval) { HRESULT hr; Check(CheckAlive()); CheckIf(interval > INT_MAX,E_INVALIDARG); Check(SetValueInt(Find(L"Interval"),static_cast<INT>(interval))); Cleanup: return hr; }
	STDMETHOD(HandleMessage)(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result)
	{
		HRESULT hr = S_OK; TStackRef<CTimer> keepAlive(this); BOOL entered = FALSE;
		CheckIf(!handled || !result,E_POINTER); *handled = FALSE; *result = 0;
		Check(CheckThread());
		if(message != WM_TIMER || !m_timerID || wp != m_timerID || window != m_timerWindow) goto Cleanup;
		*handled = TRUE;
		if(m_ticking || m_state != QuadooForm::Ready || m_mode != QuadooForm::Runtime || !Boolean(L"Enabled") || !Integer(L"Interval")) goto Cleanup;
		m_ticking = TRUE; entered = TRUE;
		Check(Dispatch(L"OnTimer"));
	Cleanup:
		if(entered) m_ticking = FALSE;
		return hr;
	}
};
HRESULT RegisterTimerFactory(IQuadooComponentRegistry* registry)
{
	HRESULT hr; TStackRef<TComponentFactory<CTimer> > factory;
	factory.Attach(__new TComponentFactory<CTimer>); CheckAlloc(factory);
	Check(factory->Initialize()); Check(registry->RegisterComponentFactory(factory));
Cleanup:
	return hr;
}
}
