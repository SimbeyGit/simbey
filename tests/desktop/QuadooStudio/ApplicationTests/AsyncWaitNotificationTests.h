// Included after the independent native fixtures in AsyncWaitTests.h.
static HRESULT DrainWaitNotificationQueue()
{
	HRESULT hr = S_OK;
	MSG message;
	UINT count = 0;
	while(PeekMessageW(&message,NULL,0,0,PM_REMOVE))
	{
		if(message.message == WM_QUIT)
		{
			PostQuitMessage(static_cast<INT>(message.wParam));
			Check(E_UNEXPECTED);
		}
		CheckIf(++count > 200000,E_UNEXPECTED);
		DispatchMessageW(&message);
	}
Cleanup:
	return hr;
}

static HRESULT NativeAsyncWaitNotificationTests()
{
	HRESULT hr;
	AQVM::AsyncWaitTestHooks& hooks = AQVM::AsyncWaitTests();
	HANDLE notified = CreateEventW(NULL,TRUE,FALSE,NULL);
	HANDLE secondNotified = CreateEventW(NULL,TRUE,FALSE,NULL);
	HANDLE entered = CreateEventW(NULL,TRUE,FALSE,NULL);
	HANDLE finish = CreateEventW(NULL,TRUE,FALSE,NULL);
	HANDLE worker = NULL;
	WaitBarrier barrier = {entered,finish};
	const UINT fillerMessage = WM_APP + 0x675;
	CheckIf(!notified || !secondNotified || !entered || !finish,HrEnsureLastError());
	Stage("AsyncWait idle registrations schedule no maintenance notifications.\r\n");
	{
		WaitProbeCounts counts;
		CAsyncFixture fixture;
		TStackRef<CWaitProbe> probe;
		HWND window;
		UINT message;
		LONG notifications = InterlockedCompareExchange(&hooks.notifications,0,0);
		UINT receipts = hooks.maintenanceCalls;
		Check(fixture.Initialize());
		probe.Attach(__new CWaitProbe(&counts));
		CheckAlloc(probe);
		Check(probe->Initialize());
		Check(AsyncWaitEnqueue(fixture.wrapper,probe,probe));
		Check(fixture.runtime->GetAsyncWaitNotification(&window,&message));
		REQUIRE(message >= 0xc000);
		Check(DrainWaitNotificationQueue());
		// An obsolete timer message must no longer cause a maintenance pass.
		SendMessageW(window,WM_TIMER,0x56B,0);
		REQUIRE(!counts.calls && hooks.registrations == 1);
		REQUIRE(notifications == InterlockedCompareExchange(&hooks.notifications,0,0) && receipts == hooks.maintenanceCalls);
		SetEvent(probe->Handle());
		Check(PumpAsyncWaits());
		REQUIRE(counts.calls == 1 && counts.correctThread && NoWaitResources());
		REQUIRE(notifications == InterlockedCompareExchange(&hooks.notifications,0,0) && receipts == hooks.maintenanceCalls);
	}
	Stage("AsyncWait notification reaches UI before the native worker returns.\r\n");
	{
		WaitProbeCounts counts;
		CAsyncFixture fixture;
		TStackRef<CWaitProbe> probe;
		Check(fixture.Initialize());
		probe.Attach(__new CWaitProbe(&counts));
		CheckAlloc(probe);
		Check(probe->Initialize(TRUE,TRUE));
		hooks.postError = ERROR_ACCESS_DENIED;
		hooks.notified = notified;
		hooks.finishNotification = finish;
		hooks.drainEntered = entered;
		Check(AsyncWaitEnqueue(fixture.wrapper,probe,probe));
		hooks.postError = 0;
		probe.Release();
		CheckIf(WaitForSingleObject(notified,5000) != WAIT_OBJECT_0,E_FAIL);
		REQUIRE(!counts.deleted && !counts.calls);
		worker = CreateThread(NULL,0,FinishWaitWorker,&barrier,0,NULL);
		CheckIf(!worker,HrEnsureLastError());
		Check(PumpAsyncWaits());
		CheckIf(WaitForSingleObject(worker,5000) != WAIT_OBJECT_0,E_FAIL);
		DWORD result;
		CheckIfGetLastError(!GetExitCodeThread(worker,&result));
		REQUIRE(result == 0 && counts.deleted == 1 && counts.destructed == 1 && counts.correctThread);
		CloseHandle(worker);
		worker = NULL;
		hooks.finishNotification = NULL;
		hooks.drainEntered = NULL;
	}
	Stage("AsyncWait quota cleanup preserves Form and lease-only application lifetime.\r\n");
	for(UINT registerForm = 0; registerForm < 2; registerForm++)
	{
		WaitProbeCounts sourceCounts;
		WaitProbeCounts callbackCounts;
		CAsyncFixture fixture;
		TStackRef<CWaitProbe> source;
		TStackRef<CWaitProbe> callback;
		HWND window;
		HANDLE signal;
		BOOL run;
		UINT filled = 0;
		UINT quits = 0;
		INT quitCode = -1;
		Check(fixture.Initialize());
		if(registerForm)
		{
			Check(fixture.runtime->RegisterForm(fixture.container));
		}
		source.Attach(__new CWaitProbe(&sourceCounts));
		callback.Attach(__new CWaitProbe(&callbackCounts));
		CheckAlloc(source);
		CheckAlloc(callback);
		Check(source->Initialize());
		signal = source->Handle();
		ResetEvent(notified);
		hooks.notified = notified;
		Check(AsyncWaitEnqueue(fixture.wrapper,source,callback));
		source.Release();
		callback.Release();
		Check(fixture.runtime->BeginMessageLoop(&run));
		REQUIRE(run);
		Check(DrainWaitNotificationQueue());
		Check(fixture.container->GetHandle(&window));
		while(filled < 100000 && PostMessageW(window,fillerMessage,0,0))
		{
			filled++;
		}
		DWORD quota = GetLastError();
		REQUIRE(filled > 0 && quota == ERROR_NOT_ENOUGH_QUOTA);
		SetEvent(signal);
		CheckIf(WaitForSingleObject(notified,5000) != WAIT_OBJECT_0,E_FAIL);
		MSG pending;
		// Nonqueued delivery runs even though no posted message is removed.
		PeekMessageW(&pending,NULL,0,0,PM_NOREMOVE);
		REQUIRE(NoWaitResources() && !callbackCounts.calls);
		REQUIRE(sourceCounts.deleted == 1 && callbackCounts.deleted == 1 && sourceCounts.correctThread && callbackCounts.correctThread);
		BOOL posted = PostMessageW(window,fillerMessage,0,0);
		quota = posted ? ERROR_SUCCESS : GetLastError();
		REQUIRE(!posted && quota == ERROR_NOT_ENOUGH_QUOTA && SUCCEEDED(fixture.runtime->Error()));
		REQUIRE(RStrLen(fixture.runtime->Diagnostic()));
		REQUIRE(PeekMessageW(&pending,NULL,fillerMessage,fillerMessage,PM_REMOVE));
		BOOL handled;
		Check(fixture.runtime->Preprocess(&pending,&handled));
		if(!handled)
		{
			DispatchMessageW(&pending);
		}
		// Remove just the filler messages; leave lifetime work queued so
		// the lease-only case proves its final check was rescheduled.
		while(PeekMessageW(&pending,window,fillerMessage,fillerMessage,PM_REMOVE))
		{
			DispatchMessageW(&pending);
		}
		if(registerForm)
		{
			Check(DrainWaitNotificationQueue());
			Check(fixture.container->Destroy());
		}
		Check(DrainLifetime(&quits,&quitCode));
		REQUIRE(quits == 1 && quitCode == 0 && SUCCEEDED(fixture.runtime->Error()));
		fixture.runtime->EndMessageLoop();
	}
	Stage("AsyncWait double notification failure retains work until a sweep or destruction.\r\n");
	for(UINT destroy = 0; destroy < 2; destroy++)
	{
		WaitProbeCounts counts;
		CAsyncFixture fixture;
		TStackRef<CWaitProbe> probe;
		BOOL run;
		UINT quits = 0;
		INT code = -1;
		UINT receipts = hooks.maintenanceCalls;
		Check(fixture.Initialize());
		Check(fixture.runtime->RegisterForm(fixture.container));
		probe.Attach(__new CWaitProbe(&counts));
		CheckAlloc(probe);
		Check(probe->Initialize(TRUE,TRUE));
		ResetEvent(notified);
		hooks.postError = ERROR_NOT_ENOUGH_QUOTA;
		hooks.notificationError = ERROR_ACCESS_DENIED;
		Check(AsyncWaitEnqueue(fixture.wrapper,probe,probe));
		hooks.postError = 0;
		hooks.notificationError = 0;
		probe.Release();
		CheckIf(WaitForSingleObject(notified,5000) != WAIT_OBJECT_0,E_FAIL);
		REQUIRE(!counts.deleted && hooks.maintenanceCalls == receipts);
		if(destroy)
		{
			Check(fixture.container->Destroy());
		}
		// Beginning the application loop supplies an ordinary UI safe point.
		Check(fixture.runtime->BeginMessageLoop(&run));
		REQUIRE(run);
		if(!destroy)
		{
			fixture.runtime->Sweep();
		}
		REQUIRE(NoWaitResources() && counts.deleted == 1 && !counts.calls && counts.correctThread);
		REQUIRE(wcsstr(RStrToWide(fixture.runtime->Diagnostic()),L"80070718") &&
			wcsstr(RStrToWide(fixture.runtime->Diagnostic()),L"80070005"));
		if(!destroy)
		{
			Check(fixture.container->Destroy());
		}
		Check(DrainLifetime(&quits,&code));
		REQUIRE(quits == 1 && code == 0 && SUCCEEDED(fixture.runtime->Error()));
		fixture.runtime->EndMessageLoop();
	}
	Stage("AsyncWait a source destructor pumps a second failure notification during reaping.\r\n");
	{
		WaitProbeCounts firstCounts;
		WaitProbeCounts secondCounts;
		CAsyncFixture fixture;
		TStackRef<CWaitProbe> first;
		TStackRef<CWaitProbe> second;
		UINT receipts = hooks.maintenanceCalls;
		Check(fixture.Initialize());
		first.Attach(__new CWaitProbe(&firstCounts));
		second.Attach(__new CWaitProbe(&secondCounts));
		CheckAlloc(first);
		CheckAlloc(second);
		Check(first->Initialize());
		Check(second->Initialize());
		first->signalOnDestruct = second->Handle();
		first->waitOnDestruct = secondNotified;
		first->pumpOnDestruct = TRUE;
		ResetEvent(notified);
		hooks.postError = ERROR_ACCESS_DENIED;
		hooks.notified = notified;
		Check(AsyncWaitEnqueue(fixture.wrapper,first,first));
		hooks.notified = secondNotified;
		Check(AsyncWaitEnqueue(fixture.wrapper,second,second));
		hooks.postError = 0;
		SetEvent(first->Handle());
		first.Release();
		second.Release();
		Check(PumpAsyncWaits());
		REQUIRE(hooks.maintenanceCalls >= receipts + 2 && NoWaitResources());
		REQUIRE(firstCounts.deleted == 1 && secondCounts.deleted == 1 && !firstCounts.calls && !secondCounts.calls);
		REQUIRE(firstCounts.correctThread && secondCounts.correctThread);
	}
	Stage("AsyncWait failure notification is processed inside a native modal loop.\r\n");
	{
		WaitProbeCounts sourceCounts;
		WaitProbeCounts failedCounts;
		WaitProbeCounts signalCounts;
		AsyncProbeCounts shownCounts;
		AsyncProbeCounts closedCounts;
		CAsyncFixture fixture;
		TStackRef<CWaitProbe> source;
		TStackRef<CWaitProbe> failed;
		TStackRef<CWaitProbe> signal;
		TStackRef<CAsyncProbe> shown;
		TStackRef<CAsyncProbe> closed;
		TStackRef<IJSONObject> definition;
		TStackRef<IQuadooForm> owner;
		TStackRef<IQuadooForm> modal;
		TStackRef<IQuadooWinControl> ownerControl;
		HWND ownerWindow;
		BOOL ownerDisabled = FALSE;
		Check(fixture.Initialize());
		Check(AQVM::ParseObject(SLP(L"{\"type\":\"Form\",\"name\":\"AsyncWaitFailureModal\"}"),&definition));
		Check(QuadooCreateForm(NULL,NULL,definition,&owner));
		Check(owner->QueryInterface(&ownerControl));
		Check(owner->GetHandle(&ownerWindow));
		Check(owner->ShowForm(SW_SHOWNOACTIVATE));
		Check(QuadooCreateForm(NULL,ownerControl,definition,&modal));
		source.Attach(__new CWaitProbe(&sourceCounts));
		failed.Attach(__new CWaitProbe(&failedCounts));
		signal.Attach(__new CWaitProbe(&signalCounts));
		shown.Attach(__new CAsyncProbe(&shownCounts));
		closed.Attach(__new CAsyncProbe(&closedCounts));
		CheckAlloc(source);
		CheckAlloc(failed);
		CheckAlloc(signal);
		CheckAlloc(shown);
		CheckAlloc(closed);
		Check(source->Initialize());
		ResetEvent(notified);
		hooks.notified = notified;
		hooks.postError = ERROR_ACCESS_DENIED;
		Check(AsyncWaitEnqueue(fixture.wrapper,source,failed));
		hooks.postError = 0;
		signal->signalOnInvoke = source->Handle();
		signal->waitOnInvoke = notified;
		signal->pumpOnInvoke = TRUE;
		shown->modal = modal;
		closed->destroy = modal;
		closed->ownerWindow = ownerWindow;
		closed->ownerDisabled = &ownerDisabled;
		source.Release();
		failed.Release();
		Check(AsyncEnqueue(fixture.wrapper,shown));
		Check(AsyncEnqueue(fixture.wrapper,signal));
		Check(AsyncEnqueue(fixture.wrapper,closed));
		Check(DrainAsync());
		REQUIRE(shownCounts.calls == 1 && signalCounts.calls == 1 && closedCounts.calls == 1);
		REQUIRE(ownerDisabled && IsWindowEnabled(ownerWindow) && SUCCEEDED(fixture.runtime->Error()));
		REQUIRE(sourceCounts.deleted == 1 && failedCounts.deleted == 1 && !failedCounts.calls);
		REQUIRE(sourceCounts.correctThread && failedCounts.correctThread && signalCounts.correctThread && NoWaitResources());
		Check(owner->Destroy());
		Check(modal->Destroy());
	}
	Stage("AsyncWait bounded cleanup retry has no self-sustaining maintenance wake.\r\n");
	{
		WaitProbeCounts counts;
		CAsyncFixture fixture;
		TStackRef<CWaitProbe> probe;
		HWND window;
		UINT message;
		UINT receipts = hooks.maintenanceCalls;
		Check(fixture.Initialize());
		probe.Attach(__new CWaitProbe(&counts));
		CheckAlloc(probe);
		Check(probe->Initialize());
		Check(fixture.runtime->GetAsyncWaitNotification(&window,&message));
		Check(AsyncWaitEnqueue(fixture.wrapper,probe,probe));
		probe.Release();
		hooks.unregisterFailures = 6;
		Check(fixture.container->Destroy());
		REQUIRE(hooks.waits == 1 && hooks.unregisterFailures == 4 && counts.deleted == 1);
		SendMessageW(window,message,0,0);
		REQUIRE(hooks.unregisterFailures == 3 && hooks.waits == 1);
		Check(DrainWaitNotificationQueue());
		REQUIRE(hooks.unregisterFailures == 3 && hooks.maintenanceCalls == receipts + 1);
		hooks.unregisterFailures = 0;
		SendMessageW(window,message,0,0);
		REQUIRE(NoWaitResources() && !counts.calls && counts.correctThread);
	}
	hr = S_OK;
Cleanup:
	SetEvent(finish);
	if(worker)
	{
		WaitForSingleObject(worker,5000);
		CloseHandle(worker);
	}
	hooks.postError = 0;
	hooks.notificationError = 0;
	hooks.notified = NULL;
	hooks.finishNotification = NULL;
	hooks.drainEntered = NULL;
	hooks.unregisterFailures = 0;
	if(notified)
	{
		CloseHandle(notified);
	}
	if(secondNotified)
	{
		CloseHandle(secondNotified);
	}
	if(entered)
	{
		CloseHandle(entered);
	}
	if(finish)
	{
		CloseHandle(finish);
	}
	return hr;
}
