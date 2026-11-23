#include "COMIdentityTests.h"

static HRESULT ComponentProperty(IQuadooComponent* component, PCWSTR name, IQuadooProperties** values, UINT* index)
{
	HRESULT hr; TStackRef<IQuadooProperties> properties; TStackRef<IQuadooPropertyDescriptors> descriptors; UINT count;
	CheckIf(!component || !values || !index,E_INVALIDARG); *values = NULL;
	Check(component->GetProperties(&properties)); Check(properties->GetDescriptors(&descriptors)); Check(descriptors->GetCount(&count));
	CheckIf(count > 4096,E_INVALIDARG);
	for(UINT n = 0; n < count; n++) { TStackRef<IQuadooPropertyDescriptor> descriptor; CRString current; Check(descriptors->GetAt(n,&descriptor)); Check(descriptor->GetName(&current)); if(current == RSTRING_CAST(name)) { *values = properties.Detach(); *index = n; hr = S_OK; goto Cleanup; } }
	hr = DISP_E_MEMBERNOTFOUND;
Cleanup:
	return hr;
}
static INT ComponentIntProperty(IQuadooComponent* component, PCWSTR name)
{
	TStackRef<IQuadooProperties> values; UINT index; INT result = 0;
	if(SUCCEEDED(ComponentProperty(component,name,&values,&index))) values->GetValueInt(index,&result); return result;
}
static HRESULT PumpComponentTimers(HWND window, UINT milliseconds)
{
	HRESULT hr = S_OK; DWORD started = GetTickCount(); MSG message;
	do
	{
		while(IsWindow(window) && PeekMessageW(&message,window,WM_TIMER,WM_TIMER,PM_REMOVE)) DispatchMessageW(&message);
		if(GetTickCount() - started >= milliseconds) break;
		CheckIf(MsgWaitForMultipleObjects(0,NULL,FALSE,2,QS_TIMER) == WAIT_FAILED,HrEnsureLastError());
	} while(IsWindow(window));
Cleanup:
	return hr;
}
class CComponentTestBinder : public CBaseUnknown, public IQuadooEventsBinder, public IQuadooDelegate
{
public:
	UINT binds, creates, accepts, cancels, pulses, finalizes;
	BOOL postBound, destroyPulse, destroyAccept;
	IQuadooEventArgs* retained;
	UINT ticks, tickDepth, maxTickDepth;
	BOOL reenterTimer, disableTimer, destroyTimer, destroyTimerRoot, pumpCreate, retainTimer;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooEventsBinder)
		UNK_INTERFACE(IQuadooDelegate)
	END_UNK_MAP
	CComponentTestBinder() : binds(0), creates(0), accepts(0), cancels(0), pulses(0), finalizes(0), postBound(FALSE), destroyPulse(FALSE), destroyAccept(FALSE), retained(NULL), ticks(0), tickDepth(0), maxTickDepth(0), reenterTimer(FALSE), disableTimer(FALSE), destroyTimer(FALSE), destroyTimerRoot(FALSE), pumpCreate(FALSE), retainTimer(FALSE) {}
	~CComponentTestBinder() { SafeRelease(retained); }
	STDMETHOD(BindEvent)(RSTRING name, IQuadooDelegate** delegate)
	{ if(!delegate) return E_POINTER; *delegate = NULL; if(!lstrcmpW(RStrToWide(name),L"Missing")) return S_FALSE; binds++; *delegate = static_cast<IQuadooDelegate*>(this); AddRef(); return S_OK; }
	STDMETHOD(PostBind)(IQuadooControl* root)
	{
		HRESULT hr; TStackRef<IQuadooComponentHost> host; TStackRef<IQuadooComponentCollection> components; UINT count;
		Check(root->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&host)));
		Check(host->GetComponents(&components)); Check(components->GetCount(&count));
		postBound = count > 0 && binds > 0; hr = S_OK;
	Cleanup:
		return hr;
	}
	STDMETHOD(Invoke)(IQuadooEventArgs* args)
	{
		HRESULT hr; CRString event; TStackRef<IQuadooComponent> sender; BOOL enteredTick = FALSE;
		Check(args->GetEventName(&event)); Check(args->GetSender(&sender));
		if(event == RSTRING_CAST(L"OnCreate"))
		{
			CheckIf(!postBound,E_FAIL); creates++;
			if(pumpCreate) { TStackRef<IQuadooWinControl> window; HWND handle; Check(sender->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&window))); Check(window->GetHandle(&handle)); Check(PumpComponentTimers(handle,30)); CheckIf(ticks,E_FAIL); }
		}
		if(event == RSTRING_CAST(L"OnAccept")) { accepts++; if(destroyAccept) { Check(sender->Destroy()); } }
		if(event == RSTRING_CAST(L"OnCancel")) cancels++;
		if(event == RSTRING_CAST(L"OnPulse"))
		{ INT value; pulses++; Check(args->GetInt(0,&value)); Check(args->SetInt(0,value + 10)); ReplaceInterface(retained,args); if(destroyPulse) { Check(sender->Destroy()); } }
		if(event == RSTRING_CAST(L"OnFinalizing")) finalizes++;
		if(event == RSTRING_CAST(L"OnTimer"))
		{
			ticks++; tickDepth++; maxTickDepth = max(maxTickDepth,tickDepth); enteredTick = TRUE;
			TStackRef<IQuadooTimer> timer; Check(sender->QueryInterface(__uuidof(IQuadooTimer),reinterpret_cast<PVOID*>(&timer)));
			if(retainTimer) ReplaceInterface(retained,args);
			if(reenterTimer || destroyTimerRoot)
			{
				TStackRef<IQuadooNonvisualComponent> nonvisual; TStackRef<IQuadooComponentCollection> owner; TStackRef<IQuadooControlContainer> root; HWND window;
				Check(sender->QueryInterface(__uuidof(IQuadooNonvisualComponent),reinterpret_cast<PVOID*>(&nonvisual))); Check(nonvisual->GetOwner(&owner)); Check(owner->GetRoot(&root));
				if(reenterTimer) { Check(root->GetHandle(&window)); Check(PumpComponentTimers(window,30)); }
				if(destroyTimerRoot) { Check(root->Destroy()); }
			}
			if(disableTimer) { Check(timer->SetEnabled(FALSE)); }
			if(destroyTimer) { Check(sender->Destroy()); }
		}
		hr = S_OK;
	Cleanup:
		if(enteredTick) tickDepth--;
		return hr;
	}
};
static HRESULT ComponentValue(IQuadooComponent* component, PCWSTR name, IJSONValue* value)
{
	HRESULT hr; TStackRef<IQuadooProperties> values; UINT index;
	Check(ComponentProperty(component,name,&values,&index)); Check(values->SetValueJSON(index,value));
Cleanup:
	return hr;
}
static HRESULT TestComponents(IQuadooComponent* root, IQuadooComponentCollection** components)
{
	HRESULT hr; TStackRef<IQuadooComponentHost> host;
	Check(root->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&host))); Check(host->GetComponents(components));
Cleanup:
	return hr;
}
static HRESULT RuntimeComponents()
{
	HRESULT hr;
	TStackRef<IQuadooControlRegistry> registry; TStackRef<IQuadooComponentRegistry> componentRegistry; TStackRef<IQuadooComponentFactory> external;
	TStackRef<IQuadooConstructionContext> context; TStackRef<IQuadooControl> root, impossible;
	TStackRef<IQuadooComponentCollection> components; TStackRef<IQuadooComponent> service, open, save, found;
	TStackRef<IQuadooFileDialog> dialog; TStackRef<IQuadooWinControl> native; TStackRef<IExternalService> operation;
	TStackRef<IQuadooFactoryLifetime> lifetime; TStackRef<IQuadooFormSerializer> serializer; TStackRef<IQuadooProperties> values;
	TStackRef<IJSONObject> definition, serialized, candidate; TStackRef<IJSONValue> value;
	TStackRef<CComponentTestBinder> binder; QuadooForm::ContextOptions options; UINT count, live, index; INT accepted;
	Check(QuadooGetControlRegistry(&registry));
	Check(registry->QueryInterface(__uuidof(IQuadooComponentRegistry),reinterpret_cast<PVOID*>(&componentRegistry)));
	Check(CreateExternalComponentFactory(&external)); Check(componentRegistry->RegisterComponentFactory(external));
	Check(external->QueryInterface(__uuidof(IQuadooFactoryLifetime),reinterpret_cast<PVOID*>(&lifetime)));
	binder.Attach(__new CComponentTestBinder); CheckAlloc(binder);
	ZeroMemory(&options,sizeof(options)); options.cbSize = sizeof(options); options.Mode = QuadooForm::Runtime; options.pRegistry = registry; options.pBinder = binder;
	Check(QuadooCreateConstructionContext(&options,&context));
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"Root\",\"events\":{\"OnCreate\":\"Create\"},\"children\":[{\"type\":\"Button\",\"name\":\"Action\"}],\"components\":[{\"type\":\"OpenFile\",\"name\":\"OpenFile1\",\"properties\":{\"Filter\":\"Text|*.txt|All|*.*\",\"FilterIndex\":2,\"DefaultExt\":\"txt\"},\"events\":{\"OnAccept\":\"Accept\",\"OnCancel\":\"Cancel\"}},{\"type\":\"SaveFile\",\"name\":\"SaveFile1\"},{\"type\":\"ExternalService\",\"name\":\"Service\",\"properties\":{\"Level\":42,\"Legend\":\"independent\"},\"events\":{\"OnPulse\":\"Pulse\",\"OnFinalizing\":\"Final\"},\"extensionData\":{\"kept\":[1,2,3]}}]}",&definition));
	Check(QuadooCreateControl(context,NULL,definition,&root)); Check(TestComponents(root,&components)); Check(components->GetCount(&count)); REQUIRE(count == 3 && binder->creates == 1 && binder->postBound && binder->binds == 5);
	Check(components->FindComponent(RSTRING_CAST(L"Service"),&service)); Check(components->FindComponent(RSTRING_CAST(L"OpenFile1"),&open)); Check(components->FindComponent(RSTRING_CAST(L"SaveFile1"),&save));
	{
		const IID* registryPaths[] = { &__uuidof(IQuadooControlRegistry),&__uuidof(IQuadooFactoryCatalog),&__uuidof(IQuadooComponentRegistry) };
		const IID* factoryPaths[] = { &__uuidof(IQuadooComponentFactory),&__uuidof(IQuadooFactoryLifetime) };
		const IID* rootPaths[] = { &__uuidof(IQuadooComponent),&__uuidof(IQuadooControlContainer),&__uuidof(IQuadooComponentHost) };
		const IID* collectionPaths[] = { &__uuidof(IQuadooComponentCollection) };
		const IID* componentPaths[] = { &__uuidof(IQuadooComponent),&__uuidof(IQuadooNonvisualComponent),&__uuidof(IQuadooProperties),&__uuidof(IQuadooPropertyNotifications),&__uuidof(IQuadooConstructionParticipant),&__uuidof(IQuadooDefinitionSource),&__uuidof(IQuadooFileDialog) };
		Check(CheckCOMIdentity(registry,registryPaths,ARRAYSIZE(registryPaths)));
		Check(CheckCOMIdentity(external,factoryPaths,ARRAYSIZE(factoryPaths)));
		Check(CheckCOMIdentity(root,rootPaths,ARRAYSIZE(rootPaths)));
		Check(CheckCOMIdentity(components,collectionPaths,ARRAYSIZE(collectionPaths)));
		Check(CheckCOMIdentity(open,componentPaths,ARRAYSIZE(componentPaths)));
		Check(CheckCOMIdentity(save,componentPaths,ARRAYSIZE(componentPaths)));
		componentPaths[ARRAYSIZE(componentPaths) - 1] = &__uuidof(IExternalService);
		Check(CheckCOMIdentity(service,componentPaths,ARRAYSIZE(componentPaths)));
	}
	REQUIRE(open->QueryInterface(__uuidof(IQuadooControl),reinterpret_cast<PVOID*>(&impossible)) == E_NOINTERFACE && !impossible);
	REQUIRE(open->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native)) == E_NOINTERFACE && !native);
	Check(open->QueryInterface(__uuidof(IQuadooFileDialog),reinterpret_cast<PVOID*>(&dialog)));
	{ BOOL chosen = TRUE; REQUIRE(dialog->Execute(reinterpret_cast<HWND>(1),&chosen) == E_INVALIDARG && !chosen); }
	Check(components->FindComponent(RSTRING_CAST(L"Action"),&found)); REQUIRE(found != NULL); found.Release();
	REQUIRE(service->SetName(RSTRING_CAST(L"Action")) == QuadooForm::EDuplicateName);
	REQUIRE(root->SetName(RSTRING_CAST(L"Service")) == QuadooForm::EDuplicateName);
	{ TStackRef<IQuadooControlContainer> visualTree; Check(root->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&visualTree))); Check(visualTree->GetChildCount(&count)); REQUIRE(count == 1); }
	Check(ComponentProperty(service,L"Level",&values,&index)); Check(values->GetValueInt(index,&accepted)); REQUIRE(accepted == 42);
	Check(values->SetValueInt(index,56)); REQUIRE(ComponentIntProperty(service,L"Level") == 56); values.Release();
	Check(service->QueryInterface(__uuidof(IExternalService),reinterpret_cast<PVOID*>(&operation)));
	Check(operation->Pulse(7,&accepted)); REQUIRE(accepted == 17 && binder->pulses == 1 && binder->retained);
	Check(lifetime->GetLiveCount(&live)); REQUIRE(live == 1 && componentRegistry->UnregisterComponentFactory(RSTRING_CAST(L"ExternalService")) == HRESULT_FROM_WIN32(ERROR_BUSY));
	Check(QuadooGetFormSerializer(&serializer)); Check(serializer->Serialize(root,&serialized));
	{
		TStackRef<IExternalComponentTest> fault; TStackRef<IJSONObject> failedSnapshot;
		Check(service->QueryInterface(__uuidof(IExternalComponentTest),reinterpret_cast<PVOID*>(&fault)));
		Check(fault->SetFailure(ExternalDefinitionQI,E_OUTOFMEMORY));
		REQUIRE(serializer->Serialize(root,&failedSnapshot) == E_OUTOFMEMORY && !failedSnapshot);
		Check(fault->SetFailure(ExternalComponentHostQI,E_OUTOFMEMORY));
		REQUIRE(serializer->Serialize(root,&failedSnapshot) == E_OUTOFMEMORY && !failedSnapshot);
		Check(fault->SetFailure(ExternalNoFailure,S_OK));
	}
	{
		TStackRef<IExternalComponentTestFactory> candidates; TStackRef<IJSONObject> candidateDefinition, before, after;
		Check(external->QueryInterface(__uuidof(IExternalComponentTestFactory),reinterpret_cast<PVOID*>(&candidates)));
		Check(Parse(L"{\"type\":\"ExternalService\",\"name\":\"Candidate\"}",&candidateDefinition));
		const ExternalFailure failures[] = { ExternalParent,ExternalState,ExternalName,ExternalID,ExternalProperties,ExternalDescriptors,ExternalNullProperties,ExternalNullDescriptors,ExternalObserverReentrant };
		Check(serializer->Serialize(root,&before));
		for(UINT n = 0; n < ARRAYSIZE(failures); n++)
		{
			TStackRef<IQuadooComponent> item; TStackRef<IExternalComponentTest> fault;
			Check(candidates->CreateCandidate(context,components,candidateDefinition,&item));
			Check(item->QueryInterface(__uuidof(IExternalComponentTest),reinterpret_cast<PVOID*>(&fault)));
			Check(fault->SetFailure(failures[n],E_ABORT));
			REQUIRE(FAILED(components->RegisterComponent(item)));
			Check(fault->SetFailure(ExternalNoFailure,S_OK));
			Check(components->GetCount(&count)); REQUIRE(count == 3);
			Check(serializer->Serialize(root,&after)); REQUIRE(JSONCompareObjects(before,after) == 0); after.Release();
			Check(item->Destroy());
		}
	}
	Check(JSONCreateInteger(0,&value)); REQUIRE(FAILED(ComponentValue(open,L"FilterIndex",value))); value.Release();
	Check(JSONCreateStringW(L"Incomplete",10,&value)); REQUIRE(FAILED(ComponentValue(open,L"Filter",value))); value.Release();
	Check(JSONCreateInteger(32,&value)); REQUIRE(FAILED(ComponentValue(save,L"Options",value))); value.Release();
	// Reject mixed categories, duplicate identities, and unresolved handlers.
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"Bad\",\"children\":[{\"type\":\"OpenFile\",\"name\":\"BadChild\"}]}",&candidate)); REQUIRE(FAILED(QuadooCreateControl(context,NULL,candidate,&impossible)) && !impossible); candidate.Release();
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"Bad\",\"components\":[{\"type\":\"Button\",\"name\":\"BadComponent\"}]}",&candidate)); REQUIRE(FAILED(QuadooCreateControl(context,NULL,candidate,&impossible)) && !impossible); candidate.Release();
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"Bad\",\"children\":[{\"type\":\"Label\",\"name\":\"Duplicate\"}],\"components\":[{\"type\":\"SaveFile\",\"name\":\"Duplicate\"}]}",&candidate)); REQUIRE(QuadooCreateControl(context,NULL,candidate,&impossible) == QuadooForm::EDuplicateName && !impossible); candidate.Release();
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"Bad\",\"components\":[{\"type\":\"OpenFile\",\"name\":\"Picker\",\"events\":{\"OnAccept\":\"Missing\"}}]}",&candidate)); REQUIRE(QuadooCreateControl(context,NULL,candidate,&impossible) == QuadooForm::EHandlerNotFound && !impossible); candidate.Release();
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"Bad\",\"children\":[{\"type\":\"Button\",\"name\":\"Visual\",\"id\":\"{22000000-0000-0000-0000-000000000001}\"}],\"components\":[{\"type\":\"SaveFile\",\"name\":\"Nonvisual\",\"id\":\"{22000000-0000-0000-0000-000000000001}\"}]}",&candidate)); REQUIRE(QuadooCreateControl(context,NULL,candidate,&impossible) == QuadooForm::EDuplicateID && !impossible); candidate.Release();
	{
		TStackRef<IQuadooControlContainer> parent; TStackRef<IQuadooFrame> first, second; TStackRef<IJSONObject> frameDefinition;
		TStackRef<IQuadooComponentCollection> frameComponents; TStackRef<IQuadooComponent> frameItem;
		Check(root->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&parent)));
		Check(Parse(L"{\"type\":\"Frame\",\"name\":\"FrameA\",\"components\":[{\"type\":\"OpenFile\",\"name\":\"OpenFile1\",\"id\":\"{33000000-0000-0000-0000-000000000001}\"}]}",&frameDefinition));
		Check(QuadooCreateFrame(NULL,parent,frameDefinition,&first)); frameDefinition.Release();
		Check(Parse(L"{\"type\":\"Frame\",\"name\":\"FrameB\",\"components\":[{\"type\":\"OpenFile\",\"name\":\"OpenFile1\",\"id\":\"{33000000-0000-0000-0000-000000000001}\"}]}",&frameDefinition));
		Check(QuadooCreateFrame(NULL,parent,frameDefinition,&second));
		Check(TestComponents(first,&frameComponents)); Check(frameComponents->FindComponent(RSTRING_CAST(L"OpenFile1"),&frameItem));
		TStackRef<IUnknown> rootIdentity, frameIdentity; Check(open->QueryInterface(IID_IUnknown,reinterpret_cast<PVOID*>(&rootIdentity))); Check(frameItem->QueryInterface(IID_IUnknown,reinterpret_cast<PVOID*>(&frameIdentity))); REQUIRE(rootIdentity != static_cast<IUnknown*>(frameIdentity));
		TStackRef<IJSONObject> after; Check(serializer->Serialize(root,&after)); REQUIRE(JSONCompareObjects(serialized,after) == 0);
	}
	// A delegate may destroy its sender. Retained event arguments keep the code
	// and component alive until released even after root teardown.
	binder->destroyPulse = TRUE; Check(operation->Pulse(9,&accepted)); REQUIRE(accepted == 19 && binder->finalizes == 1);
	operation.Release(); service.Release();
	Check(root->Destroy()); root.Release(); dialog.Release(); open.Release(); save.Release();
	REQUIRE(components->GetCount(&count) == QuadooForm::EDestroyed);
	Check(lifetime->GetLiveCount(&live)); REQUIRE(live == 1);
	SafeRelease(binder->retained); Check(lifetime->GetLiveCount(&live)); REQUIRE(live == 0);
	Check(componentRegistry->UnregisterComponentFactory(RSTRING_CAST(L"ExternalService")));
	hr = S_OK;
Cleanup:
	if(root) root->Destroy(); if(binder) SafeRelease(binder->retained);
	return hr;
}
