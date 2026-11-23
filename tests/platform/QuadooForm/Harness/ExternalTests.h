#pragma once

class CExternalBinder : public CBaseUnknown, public IQuadooEventsBinder, public IQuadooDelegate
{
public:
	UINT binds, posts, initializing, ready, finalizing;
	BOOL failReady;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooEventsBinder)
		UNK_INTERFACE(IQuadooDelegate)
	END_UNK_MAP
	CExternalBinder() : binds(0), posts(0), initializing(0), ready(0), finalizing(0), failReady(FALSE) {}
	STDMETHOD(BindEvent)(RSTRING name, IQuadooDelegate** delegate)
	{
		HRESULT hr = S_OK;
		CheckIf(!delegate,E_POINTER); *delegate = NULL;
		CheckIf(CRString(name) == RSTRING_CAST(L"Missing"), QuadooForm::EHandlerNotFound);
		*delegate = static_cast<IQuadooDelegate*>(this); AddRef(); binds++;
	Cleanup:
		return hr;
	}
	STDMETHOD(PostBind)(IQuadooControl* control)
	{
		HRESULT hr;
		TStackRef<IQuadooControlContainer> container; TStackRef<IQuadooControl> meter;
		QuadooForm::ComponentState state;
		Check(control->GetState(&state)); REQUIRE(state == QuadooForm::Binding);
		Check(control->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&container)));
		Check(container->GetChild(0,&meter)); Check(meter->GetState(&state)); REQUIRE(state == QuadooForm::Binding);
		REQUIRE(initializing == 0); posts++;
	Cleanup:
		return hr;
	}
	STDMETHOD(Invoke)(IQuadooEventArgs* args)
	{
		HRESULT hr;
		TStackRef<IQuadooComponent> sender; TStackRef<IExternalMeter> meter;
		CRString eventName; QuadooForm::ComponentState state; UINT count; INT value;
		Check(args->GetSender(&sender)); Check(sender->GetState(&state));
		Check(sender->QueryInterface(__uuidof(IExternalMeter),reinterpret_cast<PVOID*>(&meter)));
		Check(args->GetEventName(&eventName)); Check(args->GetArgumentCount(&count));
		if(eventName == RSTRING_CAST(L"OnFinalizing")) { REQUIRE(state == QuadooForm::Destroying && count == 0); finalizing++; }
		else
		{
			REQUIRE(eventName == RSTRING_CAST(L"OnPulse") && count == 1);
			Check(args->GetInt(0,&value)); Check(args->SetInt(0,value + 7));
			if(state == QuadooForm::Initializing) initializing++; else { REQUIRE(state == QuadooForm::Ready); ready++; CheckIf(failReady, E_FAIL); }
		}
	Cleanup:
		return hr;
	}
};

static HRESULT ExternalComponent()
{
	HRESULT hr;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooControlFactory> factory;
	TStackRef<IQuadooFactoryLifetime> lifetime;
	TStackRef<IQuadooPropertyDescriptors> descriptors;
	TStackRef<IQuadooPropertyDescriptor> descriptor;
	TStackRef<IQuadooEventDescriptors> events;
	TStackRef<IQuadooEventDescriptor> event;
	TStackRef<IQuadooConstructionContext> context, copyContext, designContext;
	TStackRef<CExternalBinder> binder, copyBinder;
	TStackRef<IQuadooControl> root, copy, meter, other, failed, design;
	TStackRef<IQuadooForm> form;
	TStackRef<IExternalMeter> extension;
	TStackRef<IQuadooStaticControl> painted;
	TStackRef<IQuadooLayoutControl> layout;
	TStackRef<IQuadooProperties> properties;
	TStackRef<IQuadooPropertyNotifications> notifications;
	TStackRef<CObserver> observer;
	TStackRef<IQuadooFormSerializer> serializer;
	TStackRef<IJSONObject> definition, serialized, copySerialized, child, values, childEvents;
	TStackRef<IJSONValue> json, firstJSON, secondJSON;
	TStackRef<IJSONArray> children;
	CRString text, argument;
	QuadooForm::EventValueType argumentType;
	QuadooForm::ComponentState state;
	QuadooForm::Rect stored, effective, bounds;
	UINT count, index, dpi, arranges, initialFactories = ExternalStatistics().factories;
	INT number, accepted; BOOL writable; HWND hwnd;
	LRESULT modalResult;
	IUnknown* unsupported = NULL;

	binder.Attach(__new CExternalBinder);
	copyBinder.Attach(__new CExternalBinder);
	observer.Attach(__new CObserver);
	CheckAlloc(binder);
	CheckAlloc(copyBinder);
	CheckAlloc(observer);

	Check(QuadooGetControlRegistry(&registry)); Check(CreateExternalFactory(&factory)); Check(registry->RegisterFactory(factory));
	REQUIRE(FAILED(registry->RegisterFactory(factory)));
	Check(factory->GetProperties(&descriptors)); Check(descriptors->Find(RSTRING_CAST(L"Level"),&descriptor)); Check(descriptor->GetDefaultValue(&json)); Check(json->GetInteger(&number)); REQUIRE(number == 17); json.Release(); descriptor.Release();
	Check(factory->GetEvents(&events)); Check(events->GetCount(&count)); REQUIRE(count == 2);
	Check(events->Find(RSTRING_CAST(L"OnPulse"),&event)); Check(event->GetArgument(0,&argument,&argumentType,&writable)); REQUIRE(argument == RSTRING_CAST(L"Value") && argumentType == QuadooForm::ValueInt && writable);
	Check(factory->QueryInterface(__uuidof(IQuadooFactoryLifetime),reinterpret_cast<PVOID*>(&lifetime))); Check(lifetime->GetLiveCount(&count)); REQUIRE(count == 0);
	Check(Context(96,binder,&context,registry));
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"ExternalHost\",\"properties\":{\"Width\":400,\"Height\":300,\"Padding\":{\"Left\":10,\"Top\":10,\"Right\":10,\"Bottom\":10}},\"children\":["
		L"{\"type\":\"ExternalMeter\",\"name\":\"meter\",\"properties\":{\"Level\":31,\"Legend\":\"independent\",\"Align\":1,\"AutoSize\":true},\"events\":{\"OnPulse\":\"Pulse\",\"OnFinalizing\":\"Final\"}},"
		L"{\"type\":\"Button\",\"name\":\"other\",\"properties\":{\"Align\":5}}]}",&definition));
	Check(QuadooCreateControl(context,NULL,definition,&root)); Check(root->QueryInterface(__uuidof(IQuadooForm),reinterpret_cast<PVOID*>(&form)));
	REQUIRE(binder->binds == 2 && binder->posts == 1 && binder->initializing == 1);
	Check(form->FindControl(RSTRING_CAST(L"meter"),&meter)); Check(form->FindControl(RSTRING_CAST(L"other"),&other));
	Check(meter->QueryInterface(__uuidof(IExternalMeter),reinterpret_cast<PVOID*>(&extension))); Check(meter->QueryInterface(__uuidof(IQuadooStaticControl),reinterpret_cast<PVOID*>(&painted)));
	unsupported = reinterpret_cast<IUnknown*>(1); REQUIRE(meter->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&unsupported)) == E_NOINTERFACE && unsupported == NULL);
	Check(meter->GetBounds(&stored)); Check(meter->GetEffectiveBounds(&effective)); REQUIRE(stored.Width == 73 && stored.Height == 19); REQUIRE(effective.Left == 10 && effective.Top == 10 && effective.Width == 380 && effective.Height == 23);
	Check(extension->GetArrangeCount(&arranges,&dpi)); REQUIRE(arranges > 0 && dpi == 96);
	Check(meter->QueryInterface(__uuidof(IQuadooPropertyNotifications),reinterpret_cast<PVOID*>(&notifications))); Check(notifications->AddObserver(observer));
	Check(SetInt(meter,L"Level",47)); REQUIRE(observer->properties == 1); Check(extension->Pulse(5,&accepted)); REQUIRE(accepted == 12 && binder->ready == 1);
	Check(Property(meter,L"OnPulse",&properties,&index)); REQUIRE(properties->SetValueEvent(index,RSTRING_CAST(L"Missing")) == QuadooForm::EHandlerNotFound);
	Check(properties->GetValueEvent(index,&text)); REQUIRE(text == RSTRING_CAST(L"Pulse")); Check(properties->SetValueEvent(index,RSTRING_CAST(L"PulseAgain"))); properties.Release();
	Check(form->GetHandle(&hwnd)); Check(form->ShowForm(SW_SHOWNOACTIVATE)); InvalidateRect(hwnd,NULL,TRUE); UpdateWindow(hwnd); REQUIRE(ExternalStatistics().paints > 0);
	{ TStackRef<IQuadooChildWindowHost> host; HWND content; Check(form->QueryInterface(__uuidof(IQuadooChildWindowHost),reinterpret_cast<PVOID*>(&host))); Check(host->GetChildHostHandle(&content)); SendMessageW(content,WM_LBUTTONUP,0,MAKELPARAM(15,15)); }
	REQUIRE(ExternalStatistics().messages == 1 && binder->ready == 2); Check(form->HideForm());
	Check(SetInt(meter,L"Align",0)); Check(Property(meter,L"AutoSize",&properties,&index)); Check(properties->SetValueBool(index,FALSE)); properties.Release();
	bounds.Left = 20; bounds.Top = 40; bounds.Width = 80; bounds.Height = 20; Check(meter->SetBounds(&bounds)); Check(SetInt(meter,L"Anchors",12));
	Check(root->QueryInterface(__uuidof(IQuadooLayoutControl),reinterpret_cast<PVOID*>(&layout))); Check(root->GetBounds(&bounds)); bounds.Width += 100; bounds.Height += 60; Check(layout->Arrange(&bounds,144));
	Check(meter->GetEffectiveBounds(&effective)); REQUIRE(effective.Left == 120 && effective.Top == 100 && effective.Width == 80 && effective.Height == 20);
	Check(extension->GetArrangeCount(&count,&dpi)); REQUIRE(count > arranges && dpi == 144);
	Check(SetInt(other,L"Align",0)); Check(SetInt(meter,L"Align",5)); REQUIRE(SetInt(other,L"Align",5) == E_INVALIDARG); Check(SetInt(meter,L"Align",0));
	REQUIRE(meter->SetName(RSTRING_CAST(L"other")) == QuadooForm::EDuplicateName); Check(meter->SetName(RSTRING_CAST(L"renamedMeter"))); REQUIRE(other->SetName(RSTRING_CAST(L"renamedMeter")) == QuadooForm::EDuplicateName);
	Check(QuadooGetFormSerializer(&serializer)); Check(serializer->Serialize(root,&serialized)); Check(serialized->FindNonNullValueW(L"children",&json)); Check(json->GetArray(&children)); json.Release();
	Check(children->GetObject(0,&child)); Check(child->FindNonNullValueW(L"properties",&json)); Check(json->GetObject(&values)); json.Release();
	Check(values->FindNonNullValueW(L"Level",&json)); Check(json->GetInteger(&number)); REQUIRE(number == 47); json.Release();
	Check(child->FindNonNullValueW(L"events",&json)); Check(json->GetObject(&childEvents)); json.Release(); Check(childEvents->FindNonNullValueW(L"OnPulse",&json)); text = static_cast<RSTRING>(NULL); Check(json->GetString(&text)); REQUIRE(text == RSTRING_CAST(L"PulseAgain")); json.Release();
	Check(JSONCreateInteger(999,&json)); Check(values->AddValueW(L"Level",json)); json.Release(); Check(Property(meter,L"Level",&properties,&index)); Check(properties->GetValueInt(index,&number)); REQUIRE(number == 47); properties.Release();
	serialized.Release(); Check(serializer->Serialize(root,&serialized));
	Check(Context(96,copyBinder,&copyContext,registry)); Check(QuadooCreateControl(copyContext,NULL,serialized,&copy)); Check(serializer->Serialize(copy,&copySerialized));
	Check(JSONWrapObject(serialized,&firstJSON)); Check(JSONWrapObject(copySerialized,&secondJSON)); REQUIRE(JSONCompare(firstJSON,secondJSON) == 0);
	Check(lifetime->GetLiveCount(&count)); REQUIRE(count == 2); REQUIRE(registry->UnregisterFactory(RSTRING_CAST(L"ExternalMeter")) == HRESULT_FROM_WIN32(ERROR_BUSY));
	Check(copy->Destroy()); copy.Release(); Check(lifetime->GetLiveCount(&count)); REQUIRE(count == 1 && copyBinder->finalizing == 1);
	// Fail an external event through native hit testing while the modal loop runs.
	// The error reaches the form through public ancestry, without native casts.
	Check(meter->GetEffectiveBounds(&effective)); binder->failReady = TRUE;
	{ TStackRef<IQuadooChildWindowHost> host; HWND content; Check(form->QueryInterface(__uuidof(IQuadooChildWindowHost),reinterpret_cast<PVOID*>(&host))); Check(host->GetChildHostHandle(&content)); PostMessageW(content,WM_LBUTTONUP,0,MAKELPARAM(MulDiv(effective.Left + 1,144,96),MulDiv(effective.Top + 1,144,96))); }
	REQUIRE(form->ShowModal(&modalResult) == E_FAIL); binder->failReady = FALSE;
	Check(root->Destroy()); REQUIRE(binder->finalizing == 1); Check(meter->GetState(&state)); REQUIRE(state == QuadooForm::Destroyed); REQUIRE(meter->Destroy() == S_FALSE); REQUIRE(extension->Pulse(0,&accepted) == QuadooForm::EDestroyed);
	{
		TStackRef<IQuadooControlContainer> parent;
		Check(meter->GetParent(&parent));
		REQUIRE(!parent);
	}
	Check(lifetime->GetLiveCount(&count)); REQUIRE(count == 1); REQUIRE(registry->UnregisterFactory(RSTRING_CAST(L"ExternalMeter")) == HRESULT_FROM_WIN32(ERROR_BUSY));
	notifications.Release(); painted.Release(); extension.Release(); meter.Release(); Check(lifetime->GetLiveCount(&count)); REQUIRE(count == 0 && ExternalStatistics().objects == 0);
	definition.Release(); Check(Parse(L"{\"type\":\"ExternalMeter\",\"name\":\"failed\",\"events\":{\"OnPulse\":\"Missing\",\"OnFinalizing\":\"Final\"}}",&definition));
	REQUIRE(QuadooCreateControl(context,NULL,definition,&failed) == QuadooForm::EHandlerNotFound && !failed); Check(lifetime->GetLiveCount(&count)); REQUIRE(count == 0 && binder->finalizing == 1);
	Check(Context(96,binder,&designContext,registry,QuadooForm::Design)); Check(QuadooCreateControl(designContext,NULL,definition,&design)); Check(design->Destroy()); design.Release();
	Check(lifetime->GetLiveCount(&count)); REQUIRE(count == 0 && binder->posts == 1 && binder->finalizing == 1);
	Check(registry->UnregisterFactory(RSTRING_CAST(L"ExternalMeter")));
	REQUIRE(ExternalStatistics().factories == initialFactories); lifetime.Release(); factory.Release(); REQUIRE(ExternalStatistics().factories == initialFactories + 1);
	// Metadata may outlive the factory; the independent descriptors remain usable.
	descriptor.Release(); Check(descriptors->Find(RSTRING_CAST(L"Legend"),&descriptor));
	{
		TStackRef<IQuadooControlRegistry> isolatedRegistry;
		TStackRef<IQuadooControlFactory> isolatedFactory;
		TStackRef<IQuadooConstructionContext> isolatedContext;
		TStackRef<IQuadooControl> isolated;
		UINT before = ExternalStatistics().factories;
		definition.Release(); Check(Parse(L"{\"type\":\"ExternalMeter\",\"name\":\"isolated\"}",&definition));
		Check(QuadooGetControlRegistry(&isolatedRegistry)); Check(CreateExternalFactory(&isolatedFactory)); Check(isolatedRegistry->RegisterFactory(isolatedFactory));
		Check(Context(96,NULL,&isolatedContext,isolatedRegistry)); Check(QuadooCreateControl(isolatedContext,NULL,definition,&isolated));
		isolatedFactory.Release(); isolatedRegistry.Release(); isolatedContext.Release();
		REQUIRE(ExternalStatistics().factories == before);
		Check(isolated->Destroy()); REQUIRE(ExternalStatistics().factories == before);
		isolated.Release(); REQUIRE(ExternalStatistics().factories == before + 1 && ExternalStatistics().objects == 0);
	}
Cleanup:
	if(copy) copy->Destroy(); if(root) root->Destroy(); if(design) design->Destroy();
	return hr;
}

#include "COMIdentityTests.h"

static HRESULT ExtensionFailures()
{
	HRESULT hr;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooControlFactory> factory;
	TStackRef<IExternalTestFactory> testFactory;
	TStackRef<IQuadooFactoryLifetime> lifetime;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IQuadooControl> root, existing, candidate, failed;
	TStackRef<IQuadooForm> form;
	TStackRef<IExternalMeter> meter;
	TStackRef<IExternalTestControl> diagnostics;
	TStackRef<IQuadooFormSerializer> serializer;
	TStackRef<IJSONObject> definition, candidateDefinition, before, after;
	TStackRef<IJSONValue> beforeValue, afterValue;
	QuadooForm::Rect expectedBounds, actualBounds;
	UINT count, live, observers, baselineArranges, arranges, dpi;
	ULONG baselineRefs, references; BOOL isolated;
	Check(QuadooGetControlRegistry(&registry)); Check(CreateExternalFactory(&factory));
	Check(factory->QueryInterface(__uuidof(IExternalTestFactory),reinterpret_cast<PVOID*>(&testFactory)));
	Check(factory->QueryInterface(__uuidof(IQuadooFactoryLifetime),reinterpret_cast<PVOID*>(&lifetime)));
	for(UINT mode = ExternalMissingName; mode <= ExternalBadEnumValue; mode++)
	{
		if(mode == ExternalReorderedProperties) continue;
		TStackRef<IQuadooControlFactory> found;
		Check(testFactory->SetSchema(static_cast<ExternalSchema>(mode)));
		REQUIRE(FAILED(registry->RegisterFactory(factory)));
		REQUIRE(FAILED(registry->FindFactory(RSTRING_CAST(L"ExternalMeter"),&found)) && !found);
		Check(lifetime->GetLiveCount(&live)); REQUIRE(live == 0);
	}
	Check(testFactory->SetSchema(ExternalValidSchema)); Check(registry->RegisterFactory(factory));
	Check(Context(96,NULL,&context,registry));
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"Rollback\",\"children\":[{\"type\":\"ExternalMeter\",\"name\":\"existing\",\"properties\":{\"Align\":5}}]}",&definition));
	Check(QuadooCreateControl(context,NULL,definition,&root)); Check(root->QueryInterface(__uuidof(IQuadooForm),reinterpret_cast<PVOID*>(&form)));
	Check(form->FindControl(RSTRING_CAST(L"existing"),&existing)); Check(existing->QueryInterface(__uuidof(IExternalMeter),reinterpret_cast<PVOID*>(&meter)));
	Check(existing->GetEffectiveBounds(&expectedBounds)); Check(QuadooGetFormSerializer(&serializer));
	Check(Parse(L"{\"type\":\"ExternalMeter\",\"name\":\"candidate\"}",&candidateDefinition));
	Check(serializer->Serialize(root,&before)); Check(JSONWrapObject(before,&beforeValue));
	for(UINT failure = ExternalIdentity; failure <= ExternalInvalidWindowQI; failure++)
	{
		Check(testFactory->CreateCandidate(context,form,candidateDefinition,&candidate));
		Check(candidate->QueryInterface(__uuidof(IExternalTestControl),reinterpret_cast<PVOID*>(&diagnostics)));
		Check(diagnostics->GetTestState(&observers,&baselineRefs,&isolated)); REQUIRE(observers == 0);
		Check(meter->GetArrangeCount(&baselineArranges,&dpi));
		Check(diagnostics->SetFailure(static_cast<ExternalFailure>(failure),E_ABORT));
		HRESULT registration = form->RegisterControl(candidate);
		HRESULT expected = failure >= ExternalNullWindowQI ? E_UNEXPECTED : failure == ExternalNullProperties || failure == ExternalNullDescriptors ? QuadooForm::EInvalidDefinition : E_ABORT;
		if(registration != expected)
		{ CRString detail; if(SUCCEEDED(RStrFormatW(&detail,L"failure point %u returned 0x%.8x\r\n",failure,registration))) Log(detail.c_wstr()); }
		REQUIRE(registration == expected);
		Check(diagnostics->GetTestState(&observers,&references,&isolated));
		REQUIRE(references == baselineRefs && isolated);
		REQUIRE(observers == (failure == ExternalObserverRollbackFailure ? 1 : 0));
		Check(diagnostics->SetFailure(ExternalNoFailure,S_OK)); Check(diagnostics->SendObserverNotification());
		Check(form->GetChildCount(&count)); REQUIRE(count == 1);
		{ TStackRef<IQuadooControl> found; Check(form->FindControl(RSTRING_CAST(L"existing"),&found)); REQUIRE(found == static_cast<IQuadooControl*>(existing)); }
		Check(meter->GetArrangeCount(&arranges,&dpi)); REQUIRE(arranges == baselineArranges);
		Check(existing->GetEffectiveBounds(&actualBounds)); REQUIRE(EqualBounds(expectedBounds,actualBounds));
		Check(serializer->Serialize(root,&after)); Check(JSONWrapObject(after,&afterValue)); REQUIRE(JSONCompare(beforeValue,afterValue) == 0); afterValue.Release(); after.Release();
		if(failure != ExternalObserverRollbackFailure)
		{
			Check(form->RegisterControl(candidate)); Check(form->GetChildCount(&count)); REQUIRE(count == 2);
			Check(diagnostics->GetTestState(&observers,&references,&isolated)); REQUIRE(observers == 1);
		}
		Check(candidate->Destroy()); diagnostics.Release(); candidate.Release(); Check(lifetime->GetLiveCount(&live)); REQUIRE(live == 1);
		// Unregistration may perform layout; establish a fresh stable baseline.
		Check(existing->GetEffectiveBounds(&expectedBounds));
	}
	{
		TStackRef<IQuadooPropertyObserver> sink;
		Check(form->QueryInterface(__uuidof(IQuadooPropertyObserver),reinterpret_cast<PVOID*>(&sink)));
		REQUIRE(sink->OnPropertyChanged(existing,UINT_MAX) == HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
		REQUIRE(sink->OnPropertyChanged(NULL,0) == E_INVALIDARG); REQUIRE(sink->OnPropertyChanged(root,0) == E_INVALIDARG);
		Check(existing->QueryInterface(__uuidof(IExternalTestControl),reinterpret_cast<PVOID*>(&diagnostics)));
		Check(diagnostics->SetFailure(ExternalNullDescriptors,E_ABORT)); REQUIRE(diagnostics->SendObserverNotification() == QuadooForm::EInvalidDefinition);
		Check(diagnostics->SetFailure(ExternalNoFailure,S_OK)); diagnostics.Release();
	}
	Check(testFactory->CreateCandidate(context,form,candidateDefinition,&candidate)); Check(candidate->QueryInterface(__uuidof(IExternalTestControl),reinterpret_cast<PVOID*>(&diagnostics)));
	Check(diagnostics->SetFailure(ExternalObserverAfter,S_FALSE)); REQUIRE(form->RegisterControl(candidate) == E_UNEXPECTED);
	Check(diagnostics->GetTestState(&observers,&references,&isolated)); REQUIRE(observers == 0);
	Check(diagnostics->SetFailure(ExternalNoFailure,S_OK)); Check(candidate->Destroy()); candidate.Release(); diagnostics.Release();
	definition.Release(); Check(Parse(L"{\"type\":\"ExternalMeter\",\"name\":\"mismatch\",\"testInstanceSchema\":10}",&definition));
	REQUIRE(QuadooCreateControl(context,form,definition,&failed) == QuadooForm::EInvalidDefinition && !failed);
	Check(form->GetChildCount(&count)); REQUIRE(count == 1); Check(lifetime->GetLiveCount(&live)); REQUIRE(live == 1);
	// Mutating an advertised factory after registration is also rejected before allocation.
	Check(testFactory->SetSchema(ExternalWrongWidthType)); REQUIRE(FAILED(QuadooCreateControl(context,form,candidateDefinition,&failed)) && !failed);
	Check(lifetime->GetLiveCount(&live)); REQUIRE(live == 1); Check(testFactory->SetSchema(ExternalValidSchema));
	{
		const IID* paths[] = { &__uuidof(IQuadooControlFactory), &__uuidof(IQuadooFactoryLifetime), &__uuidof(IExternalTestFactory) };
		Check(CheckCOMIdentity(factory,paths,ARRAYSIZE(paths)));
		const IID* componentPaths[] = { &__uuidof(IQuadooComponent), &__uuidof(IQuadooControl), &__uuidof(IQuadooStaticControl), &__uuidof(IQuadooProperties), &__uuidof(IQuadooPropertyNotifications), &__uuidof(IQuadooConstructionParticipant), &__uuidof(IQuadooLayoutControl), &__uuidof(IQuadooMessageHandler), &__uuidof(IExternalMeter), &__uuidof(IExternalTestControl) };
		Check(CheckCOMIdentity(existing,componentPaths,ARRAYSIZE(componentPaths)));
	}
	// Parent teardown must progress even if an attached child's identity query
	// fails while it tries to unregister itself. Cleanup uses retained entries.
	Check(existing->QueryInterface(__uuidof(IExternalTestControl),reinterpret_cast<PVOID*>(&diagnostics)));
	Check(diagnostics->SetFailure(ExternalIdentity,E_ABORT)); Check(root->Destroy());
	Check(form->GetChildCount(&count)); REQUIRE(count == 0);
	Check(diagnostics->SetFailure(ExternalNoFailure,S_OK));
Cleanup:
	if(diagnostics) diagnostics->SetFailure(ExternalNoFailure,S_OK);
	if(candidate) candidate->Destroy(); if(root) root->Destroy();
	return hr;
}

class CRetainingBinder : public CBaseUnknown, public IQuadooEventsBinder, public IQuadooDelegate
{
public:
	IQuadooEventArgs* held;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		*ppvObj = NULL;
		UNK_INTERFACE(IQuadooEventsBinder)
		UNK_INTERFACE(IQuadooDelegate)
	END_UNK_MAP
	CRetainingBinder() : held(NULL) {}
	~CRetainingBinder() { SafeRelease(held); }
	STDMETHOD(BindEvent)(RSTRING name, IQuadooDelegate** delegate) { if(delegate) { *delegate = this; AddRef(); } return delegate ? S_OK : E_POINTER; }
	STDMETHOD(PostBind)(IQuadooControl* control) { return S_OK; }
	STDMETHOD(Invoke)(IQuadooEventArgs* args) { ReplaceInterface(held,args); return S_OK; }
};
static HRESULT ExtensionModuleLifetime()
{
	HRESULT hr;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooControlFactory> factory;
	TStackRef<IQuadooFactoryLifetime> lifetime;
	TStackRef<IQuadooPropertyDescriptors> descriptors;
	TStackRef<IQuadooEventDescriptors> events;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooComponent> sender;
	TStackRef<IJSONObject> definition;
	TStackRef<CRetainingBinder> binder;
	QuadooForm::ComponentState state;
	UINT live;
	REQUIRE(ExternalCanUnload() == S_OK);
	Check(CreateExternalFactory(&factory)); REQUIRE(ExternalCanUnload() == S_FALSE);
	Check(factory->GetProperties(&descriptors)); Check(factory->GetEvents(&events));
	Check(factory->QueryInterface(__uuidof(IQuadooFactoryLifetime),reinterpret_cast<PVOID*>(&lifetime)));
	Check(QuadooGetControlRegistry(&registry)); Check(registry->RegisterFactory(factory));
	binder.Attach(__new CRetainingBinder); CheckAlloc(binder);
	Check(Context(96,binder,&context,registry)); Check(Parse(L"{\"type\":\"ExternalMeter\",\"name\":\"retained\",\"events\":{\"OnPulse\":\"Hold\"}}",&definition));
	Check(QuadooCreateControl(context,NULL,definition,&control)); REQUIRE(binder->held != NULL);
	Check(control->Destroy()); control.Release(); Check(lifetime->GetLiveCount(&live)); REQUIRE(live == 1);
	Check(binder->held->GetSender(&sender)); Check(sender->GetState(&state)); REQUIRE(state == QuadooForm::Destroyed); sender.Release();
	REQUIRE(registry->UnregisterFactory(RSTRING_CAST(L"ExternalMeter")) == HRESULT_FROM_WIN32(ERROR_BUSY));
	SafeRelease(binder->held); Check(lifetime->GetLiveCount(&live)); REQUIRE(live == 0);
	Check(registry->UnregisterFactory(RSTRING_CAST(L"ExternalMeter"))); lifetime.Release(); factory.Release();
	context.Release(); registry.Release(); binder.Release(); REQUIRE(ExternalCanUnload() == S_FALSE);
	descriptors.Release(); REQUIRE(ExternalCanUnload() == S_FALSE); events.Release(); REQUIRE(ExternalCanUnload() == S_OK);
	Check(UnloadExternalExtension());
	// Load a fresh DLL instance, with fresh module counters, and unload it again.
	Check(CreateExternalFactory(&factory)); REQUIRE(ExternalStatistics().factories == 0); factory.Release(); REQUIRE(ExternalCanUnload() == S_OK);
Cleanup:
	if(binder) SafeRelease(binder->held); if(control) control->Destroy();
	return hr;
}

static HRESULT NativeCapabilities()
{
	HRESULT hr;
	TStackRef<IJSONObject> definition; TStackRef<IQuadooControl> root; TStackRef<IQuadooConstructionContext> context;
	TStackRef<IQuadooForm> form; TStackRef<IQuadooFrame> frame;
	IQuadooControl* controls[14] = { NULL };
	const IID* interfaces[] = { &__uuidof(IQuadooForm), &__uuidof(IQuadooFrame), &__uuidof(IQuadooPanel), &__uuidof(IQuadooScrollingPanel), &__uuidof(IQuadooStaticControl), &__uuidof(IQuadooWinControl), &__uuidof(IQuadooWinControl), &__uuidof(IQuadooWinControl), &__uuidof(IQuadooRadioButton), &__uuidof(IQuadooControlContainer), &__uuidof(IQuadooComboBox), &__uuidof(IQuadooListBox), &__uuidof(IQuadooTabControl), &__uuidof(IQuadooTabPage) };
	const IID* specialized[] = { &__uuidof(IQuadooForm), &__uuidof(IQuadooFrame), &__uuidof(IQuadooPanel), &__uuidof(IQuadooScrollingPanel), &__uuidof(IQuadooStaticControl), &__uuidof(IQuadooScrollable), &__uuidof(IQuadooControlContainer), &__uuidof(IQuadooRadioButton), &__uuidof(IQuadooComboBox), &__uuidof(IQuadooListBox), &__uuidof(IQuadooTabControl), &__uuidof(IQuadooTabPage) };
	const UINT propertyCounts[] = { 43, 33, 33, 34, 25, 35, 33, 35, 34, 33, 37, 33, 37, 33 };
	const UINT eventCounts[] = { 13, 11, 11, 11, 5, 12, 12, 13, 13, 11, 13, 12, 13, 11 };

	Check(Parse(L"{\"type\":\"Form\",\"name\":\"Capabilities\",\"children\":[{\"type\":\"Panel\",\"name\":\"panel\"},{\"type\":\"ScrollingPanel\",\"name\":\"scroll\"},{\"type\":\"Label\",\"name\":\"label\"},{\"type\":\"Button\",\"name\":\"button\"},{\"type\":\"TextBox\",\"name\":\"edit\"},{\"type\":\"CheckBox\",\"name\":\"check\"},{\"type\":\"RadioButton\",\"name\":\"radio\"},{\"type\":\"GroupBox\",\"name\":\"group\"},{\"type\":\"ComboBox\",\"name\":\"combo\"},{\"type\":\"ListBox\",\"name\":\"list\"},{\"type\":\"TabControl\",\"name\":\"tabs\",\"properties\":{\"ActivePageIndex\":-1},\"children\":[{\"type\":\"TabPage\",\"name\":\"page\"}]}]}",&definition));
	Check(Context(96,NULL,&context));
	Check(QuadooCreateControl(context,NULL,definition,&root));
	Check(root->QueryInterface(__uuidof(IQuadooForm),reinterpret_cast<PVOID*>(&form)));
	controls[0] = root; controls[0]->AddRef();
	for(UINT n = 2; n < ARRAYSIZE(controls) - 1; n++) { Check(form->GetChild(n - 2,&controls[n])); }
	Check(form->FindControl(RSTRING_CAST(L"page"),&controls[13]));
	definition.Release(); Check(Parse(L"{\"type\":\"Frame\",\"name\":\"frame\"}",&definition));
	Check(QuadooCreateFrame(NULL,form,definition,&frame));
	Check(frame->QueryInterface(__uuidof(IQuadooControl),reinterpret_cast<PVOID*>(&controls[1])));
	for(UINT n = 0; n < ARRAYSIZE(controls); n++)
	{
		TStackRef<IUnknown> capability, identity;
		Check(controls[n]->QueryInterface(*interfaces[n],reinterpret_cast<PVOID*>(&capability))); Check(capability->QueryInterface(IID_IUnknown,reinterpret_cast<PVOID*>(&identity)));
		{
			const IID* paths[16] = { &__uuidof(IQuadooComponent), &__uuidof(IQuadooControl), &__uuidof(IQuadooProperties), &__uuidof(IQuadooPropertyNotifications), &__uuidof(IQuadooConstructionParticipant), &__uuidof(IQuadooLayoutControl), &__uuidof(IQuadooMessageHandler) };
			UINT pathCount = 7;
			if(n != 4) paths[pathCount++] = &__uuidof(IQuadooWinControl);
			if(n < 4 || n == 9 || n >= 12) { paths[pathCount++] = &__uuidof(IQuadooControlContainer); paths[pathCount++] = &__uuidof(IQuadooChildWindowHost); }
			paths[pathCount++] = interfaces[n];
			if(n == 3) { paths[pathCount++] = &__uuidof(IQuadooPanel); paths[pathCount++] = &__uuidof(IQuadooScrollable); }
			if(n == 0) { paths[pathCount++] = &__uuidof(IQuadooScrollable); paths[pathCount++] = &__uuidof(IQuadooModalDialog); }
			paths[pathCount++] = &__uuidof(IQuadooDesignCapabilities);
			Check(CheckCOMIdentity(controls[n],paths,pathCount));
		}
		for(UINT i = 0; i < ARRAYSIZE(specialized); i++)
		{
			BOOL expected = (n < 5 && i == n) || (i == 2 && n == 3) || (i == 5 && (n == 0 || n == 3)) || (i == 6 && (n < 4 || n == 9 || n >= 12)) || (i == 7 && n == 8) || (i == 8 && n == 10) || (i == 9 && n == 11) || (i == 10 && n == 12) || (i == 11 && n == 13);
			TStackRef<IUnknown> result, queriedIdentity;
			HRESULT query = controls[n]->QueryInterface(*specialized[i],reinterpret_cast<PVOID*>(&result));
			REQUIRE((expected && SUCCEEDED(query)) || (!expected && query == E_NOINTERFACE && !result));
			if(result) { Check(result->QueryInterface(IID_IUnknown,reinterpret_cast<PVOID*>(&queriedIdentity))); REQUIRE(static_cast<IUnknown*>(identity) == static_cast<IUnknown*>(queriedIdentity)); }
		}
		{
			TStackRef<IQuadooControlRegistry> registry;
			TStackRef<IQuadooControlFactory> factory;
			TStackRef<IQuadooPropertyDescriptors> factoryProperties, instanceProperties;
			TStackRef<IQuadooProperties> properties;
			TStackRef<IQuadooEventDescriptors> events;
			CRString type; UINT count, actualCount;
			Check(context->GetRegistry(&registry)); Check(controls[n]->GetTypeName(&type)); Check(registry->FindFactory(type,&factory));
			Check(factory->GetProperties(&factoryProperties)); Check(controls[n]->GetProperties(&properties)); Check(properties->GetDescriptors(&instanceProperties));
			Check(factoryProperties->GetCount(&count)); Check(instanceProperties->GetCount(&actualCount)); REQUIRE(count == propertyCounts[n] && count == actualCount);
			Check(factory->GetEvents(&events)); Check(events->GetCount(&count)); REQUIRE(count == eventCounts[n]);
			for(UINT p = 0; p < actualCount; p++)
			{
				TStackRef<IQuadooPropertyDescriptor> expected, actual;
				TStackRef<IJSONValue> defaultValue, value;
				CRString expectedName, actualName;
				DWORD expectedFlags, actualFlags;
				Check(factoryProperties->GetAt(p,&expected)); Check(instanceProperties->GetAt(p,&actual));
				Check(expected->GetName(&expectedName)); Check(actual->GetName(&actualName)); Check(expected->GetFlags(&expectedFlags)); Check(actual->GetFlags(&actualFlags));
				REQUIRE(expectedName == static_cast<RSTRING>(actualName) && expectedFlags == actualFlags);
				if(actualName == RSTRING_CAST(L"Name")) continue;
				Check(expected->GetDefaultValue(&defaultValue)); Check(properties->GetValueJSON(p,&value));
				REQUIRE(JSONCompare(defaultValue,value) == 0);
			}
		}
	}
Cleanup:
	if(root) root->Destroy();
	for(UINT n = 0; n < ARRAYSIZE(controls); n++) SafeRelease(controls[n]);
	return hr;
}
