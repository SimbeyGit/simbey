struct TimerThreadCall { IQuadooTimer* timer; HRESULT result; };
static DWORD WINAPI TimerThread(PVOID value) { TimerThreadCall* call = static_cast<TimerThreadCall*>(value); call->result = call->timer->SetEnabled(FALSE); return 0; }
static HRESULT TimerComponents()
{
	HRESULT hr;
	TStackRef<CComponentTestBinder> binder; TStackRef<IQuadooControlRegistry> registry; TStackRef<IQuadooComponentRegistry> componentRegistry;
	TStackRef<IQuadooComponentFactory> factory; TStackRef<IQuadooFactoryLifetime> lifetime;
	TStackRef<IQuadooConstructionContext> context; TStackRef<IJSONObject> definition, serialized, candidate;
	TStackRef<IQuadooControl> root, failed; TStackRef<IQuadooComponentCollection> collection;
	TStackRef<IQuadooComponent> component; TStackRef<IQuadooTimer> timer; TStackRef<IQuadooWinControl> native;
	TStackRef<IQuadooProperties> properties; UINT index, interval, count, live, before; BOOL enabled;
	QuadooForm::ContextOptions options; HWND window = NULL, designHost = NULL; HANDLE thread = NULL; TimerThreadCall call;
	binder.Attach(__new CComponentTestBinder); CheckAlloc(binder); binder->pumpCreate = TRUE;
	Check(QuadooGetControlRegistry(&registry)); Check(registry->QueryInterface(__uuidof(IQuadooComponentRegistry),reinterpret_cast<PVOID*>(&componentRegistry)));
	Check(componentRegistry->FindComponentFactory(RSTRING_CAST(L"Timer"),&factory)); Check(factory->QueryInterface(__uuidof(IQuadooFactoryLifetime),reinterpret_cast<PVOID*>(&lifetime)));
	ZeroMemory(&options,sizeof(options)); options.cbSize = sizeof(options); options.pRegistry = registry; options.pBinder = binder;
	Check(QuadooCreateConstructionContext(&options,&context));
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"Timers\",\"events\":{\"OnCreate\":\"Create\"},\"components\":[{\"type\":\"Timer\",\"name\":\"Timer1\",\"properties\":{\"Interval\":2},\"events\":{\"OnTimer\":\"Tick\"}}]}",&definition));
	Check(QuadooCreateControl(context,NULL,definition,&root)); REQUIRE(binder->creates == 1 && binder->binds == 2 && binder->ticks == 0);
	Check(TestComponents(root,&collection)); Check(collection->FindComponent(RSTRING_CAST(L"Timer1"),&component));
	Check(component->QueryInterface(__uuidof(IQuadooTimer),reinterpret_cast<PVOID*>(&timer)));
	{ const IID* paths[] = { &__uuidof(IQuadooComponent),&__uuidof(IQuadooNonvisualComponent),&__uuidof(IQuadooProperties),&__uuidof(IQuadooPropertyNotifications),&__uuidof(IQuadooConstructionParticipant),&__uuidof(IQuadooDefinitionSource),&__uuidof(IQuadooTimer),&__uuidof(IQuadooMessageHandler) }; Check(CheckCOMIdentity(component,paths,ARRAYSIZE(paths))); }
	REQUIRE(component->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native)) == E_NOINTERFACE && !native);
	Check(timer->GetInterval(&interval)); Check(timer->GetEnabled(&enabled)); REQUIRE(interval == 2 && enabled);
	Check(root->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native))); Check(native->GetHandle(&window)); native.Release();
	Check(timer->SetEnabled(FALSE)); Check(PumpComponentTimers(window,40)); REQUIRE(binder->ticks == 0);
	Check(timer->SetInterval(0)); Check(timer->SetEnabled(TRUE)); Check(PumpComponentTimers(window,40)); REQUIRE(binder->ticks == 0);
	Check(timer->SetInterval(INT_MAX)); Check(PumpComponentTimers(window,30)); REQUIRE(binder->ticks == 0);
	REQUIRE(timer->SetInterval(0xffffffff) == E_INVALIDARG);
	Check(ComponentProperty(component,L"Interval",&properties,&index)); REQUIRE(properties->SetValueInt(index,-1) == E_INVALIDARG); properties.Release();
	Check(timer->GetInterval(&interval)); REQUIRE(interval == INT_MAX);
	call.timer = timer; call.result = S_OK; thread = CreateThread(NULL,0,TimerThread,&call,0,NULL); CheckIf(!thread,HrEnsureLastError());
	REQUIRE(WaitForSingleObject(thread,5000) == WAIT_OBJECT_0 && call.result == QuadooForm::EWrongThread); CloseHandle(thread); thread = NULL;
	binder->reenterTimer = TRUE; binder->disableTimer = TRUE; binder->retainTimer = TRUE;
	Check(timer->SetInterval(2)); Check(PumpComponentTimers(window,250));
	REQUIRE(binder->ticks == 1 && binder->maxTickDepth == 1 && binder->retained);
	Check(PumpComponentTimers(window,40)); REQUIRE(binder->ticks == 1);
	binder->reenterTimer = FALSE; binder->disableTimer = FALSE; before = binder->ticks;
	Check(timer->SetInterval(5)); Check(timer->SetEnabled(TRUE)); Check(PumpComponentTimers(window,80)); REQUIRE(binder->ticks > before);
	Check(timer->SetEnabled(FALSE)); before = binder->ticks; Check(PumpComponentTimers(window,30)); REQUIRE(binder->ticks == before);
	{
		TStackRef<IQuadooFormSerializer> serializer; Check(QuadooGetFormSerializer(&serializer)); Check(serializer->Serialize(root,&serialized));
		TStackRef<IQuadooControl> preview; TStackRef<IQuadooConstructionContext> previewContext; TStackRef<CComponentTestBinder> suppressed;
		suppressed.Attach(__new CComponentTestBinder); CheckAlloc(suppressed); options.Mode = QuadooForm::Preview; options.pBinder = suppressed;
		Check(QuadooCreateConstructionContext(&options,&previewContext)); Check(QuadooCreateControl(previewContext,NULL,definition,&preview));
		TStackRef<IQuadooWinControl> previewWindow; HWND handle; Check(preview->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&previewWindow))); Check(previewWindow->GetHandle(&handle));
		Check(PumpComponentTimers(handle,40)); REQUIRE(suppressed->binds == 0 && suppressed->creates == 0 && suppressed->ticks == 0);
		Check(preview->Destroy());
	}
	// Frame timers are routed through their own native root, with an independent binder.
	{
		TStackRef<IQuadooFrame> frame; TStackRef<IQuadooControlContainer> parent; TStackRef<IJSONObject> frameDefinition; TStackRef<CComponentTestBinder> frameBinder;
		frameBinder.Attach(__new CComponentTestBinder); CheckAlloc(frameBinder); frameBinder->disableTimer = TRUE;
		Check(root->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&parent)));
		Check(Parse(L"{\"type\":\"Frame\",\"name\":\"TimerFrame\",\"components\":[{\"type\":\"Timer\",\"name\":\"Timer1\",\"properties\":{\"Interval\":2},\"events\":{\"OnTimer\":\"FrameTick\"}}]}",&frameDefinition));
		Check(QuadooCreateFrame(frameBinder,parent,frameDefinition,&frame)); HWND frameWindow; Check(frame->GetHandle(&frameWindow)); Check(PumpComponentTimers(frameWindow,100)); REQUIRE(frameBinder->ticks == 1 && binder->ticks == before); Check(frame->Destroy());
	}
	binder->destroyTimer = TRUE; Check(timer->SetEnabled(TRUE)); Check(PumpComponentTimers(window,100));
	QuadooForm::ComponentState state; Check(component->GetState(&state)); Check(collection->GetCount(&count)); REQUIRE(state == QuadooForm::Destroyed && count == 0);
	REQUIRE(timer->SetEnabled(TRUE) == QuadooForm::EDestroyed);
	timer.Release(); component.Release(); Check(root->Destroy()); root.Release(); collection.Release();
	Check(lifetime->GetLiveCount(&live)); REQUIRE(live == 1 && componentRegistry->UnregisterComponentFactory(RSTRING_CAST(L"Timer")) == HRESULT_FROM_WIN32(ERROR_BUSY));
	SafeRelease(binder->retained); Check(lifetime->GetLiveCount(&live)); REQUIRE(live == 0);
	// A tick can destroy its owning Form safely.
	binder->destroyTimer = FALSE; binder->destroyTimerRoot = TRUE; binder->retainTimer = FALSE; binder->pumpCreate = FALSE;
	options.Mode = QuadooForm::Runtime; options.pBinder = binder; context.Release(); Check(QuadooCreateConstructionContext(&options,&context));
	Check(QuadooCreateControl(context,NULL,definition,&root)); Check(root->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native))); Check(native->GetHandle(&window)); native.Release();
	Check(PumpComponentTimers(window,100)); REQUIRE(!IsWindow(window)); root.Release();
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"BadTimer\",\"components\":[{\"type\":\"Timer\",\"name\":\"Timer1\",\"events\":{\"OnTimer\":\"Missing\"}}]}",&candidate)); REQUIRE(QuadooCreateControl(context,NULL,candidate,&failed) == QuadooForm::EHandlerNotFound && !failed); candidate.Release();
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"BadTimer\",\"components\":[{\"type\":\"Timer\",\"name\":\"Timer1\",\"properties\":{\"Interval\":-1}}]}",&candidate)); REQUIRE(FAILED(QuadooCreateControl(context,NULL,candidate,&failed)) && !failed); candidate.Release();
	for(UINT cycle = 0; cycle < 32; cycle++)
	{
		TStackRef<CComponentTestBinder> cycleBinder; TStackRef<IQuadooConstructionContext> cycleContext; TStackRef<IQuadooControl> cycleRoot; TStackRef<IQuadooWinControl> cycleNative;
		cycleBinder.Attach(__new CComponentTestBinder); CheckAlloc(cycleBinder); cycleBinder->disableTimer = TRUE; options.pBinder = cycleBinder;
		Check(QuadooCreateConstructionContext(&options,&cycleContext)); Check(QuadooCreateControl(cycleContext,NULL,definition,&cycleRoot));
		HWND handle; Check(cycleRoot->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&cycleNative))); Check(cycleNative->GetHandle(&handle));
		Check(PumpComponentTimers(handle,40)); REQUIRE(cycleBinder->ticks == 1);
		Check(cycleRoot->Destroy()); cycleNative.Release(); cycleRoot.Release();
		Check(lifetime->GetLiveCount(&live)); REQUIRE(!IsWindow(handle) && live == 0);
	}
	Check(componentRegistry->UnregisterComponentFactory(RSTRING_CAST(L"Timer")));
Cleanup:
	if(thread) { WaitForSingleObject(thread,5000); CloseHandle(thread); }
	if(root) root->Destroy(); if(failed) failed->Destroy(); if(binder) SafeRelease(binder->retained); if(designHost) DestroyWindow(designHost);
	return hr;
}
