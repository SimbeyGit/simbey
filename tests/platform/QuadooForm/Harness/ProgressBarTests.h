class CProgressObserver : public CBaseUnknown, public IQuadooPropertyObserver
{
public:
	UINT calls;
	BOOL reenter;
	BOOL destroy;
	CProgressObserver() : calls(0), reenter(FALSE), destroy(FALSE)
	{
	}
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooPropertyObserver)
	END_UNK_MAP
	STDMETHOD(OnPropertyChanged)(IQuadooComponent* component, UINT)
	{
		HRESULT hr;
		TStackRef<IQuadooProgressBar> progress;
		INT minimum;
		INT maximum;
		INT position;
		Check(component->QueryInterface(&progress));
		Check(progress->GetMin(&minimum));
		Check(progress->GetMax(&maximum));
		Check(progress->GetPosition(&position));
		REQUIRE(minimum <= position && position <= maximum);
		calls++;
		if(reenter)
		{
			reenter = FALSE;
			Check(progress->SetPosition(maximum));
		}
		if(destroy)
		{
			destroy = FALSE;
			Check(component->Destroy());
		}
	Cleanup:
		return hr;
	}
	STDMETHOD(OnBoundsChanged)(IQuadooControl*, const QuadooForm::Rect*)
	{
		return S_OK;
	}
};

static HRESULT ProgressBarNativeTests()
{
	HRESULT hr;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONObject> saved;
	TStackRef<IJSONObject> again;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IQuadooControl> form;
	TStackRef<IQuadooControl> copy;
	TStackRef<IQuadooControlContainer> container;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooProgressBar> progress;
	TStackRef<IQuadooWinControl> window;
	TStackRef<IQuadooPropertyNotifications> notifications;
	TStackRef<CProgressObserver> observer;
	TStackRef<IQuadooFormSerializer> serializer;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooControlFactory> factory;
	TStackRef<IQuadooFactoryLifetime> lifetime;
	HWND handle = NULL;
	UINT before;
	UINT after;
	INT value;
	PBRANGE range;
	Check(QuadooGetControlRegistry(&registry));
	Check(registry->FindFactory(RSTRING_CAST(L"ProgressBar"),&factory));
	Check(factory->QueryInterface(&lifetime));
	Check(lifetime->GetLiveCount(&before));
	Check(Context(96,NULL,&context));
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"ProgressTest\",\"children\":[{\"type\":\"ProgressBar\",\"name\":\"Progress\"}]}",&definition));
	Check(QuadooCreateControl(context,NULL,definition,&form));
	Check(form->QueryInterface(&container));
	Check(container->FindControl(RSTRING_CAST(L"Progress"),&control));
	Check(control->QueryInterface(&progress));
	Check(control->QueryInterface(&window));
	Check(window->GetHandle(&handle));
	{
		const IID* paths[] = {&__uuidof(IQuadooProgressBar),&__uuidof(IQuadooWinControl),&__uuidof(IQuadooProperties)};
		Check(CheckCOMIdentity(control,paths,ARRAYSIZE(paths)));
	}
	Check(progress->GetMin(&value));
	REQUIRE(value == 0);
	Check(progress->GetMax(&value));
	REQUIRE(value == 100);
	Check(progress->GetStep(&value));
	REQUIRE(value == 10);
	Check(progress->GetPosition(&value));
	REQUIRE(value == 0 && SendMessageW(handle,PBM_GETPOS,0,0) == 0);
	Check(SetInt(control,L"ForeColor",static_cast<INT>(0x80000000 | COLOR_HIGHLIGHT)));
	SendMessageW(handle,WM_SYSCOLORCHANGE,0,0);
	SendMessageW(handle,WM_THEMECHANGED,0,0);
	{
		TStackRef<IQuadooProperties> values;
		UINT index;
		Check(Property(control,L"ForeColor",&values,&index));
		Check(values->GetValueInt(index,&value));
		REQUIRE(value == static_cast<INT>(0x80000000 | COLOR_HIGHLIGHT));
	}
	REQUIRE(!(GetWindowLongW(handle,GWL_STYLE) & WS_TABSTOP));
	REQUIRE(progress->GetPosition(NULL) == E_POINTER);
	Check(progress->StepIt());
	Check(progress->GetPosition(&value));
	REQUIRE(value == 10 && SendMessageW(handle,PBM_GETPOS,0,0) == 10);
	Check(progress->SetPosition(1000));
	Check(progress->GetPosition(&value));
	REQUIRE(value == 100);
	Check(progress->StepIt());
	Check(progress->GetPosition(&value));
	REQUIRE(value == 10);
	// Compare boundary wrapping with the actual common control implementation.
	SendMessageW(handle,PBM_SETPOS,100,0);
	SendMessageW(handle,PBM_STEPIT,0,0);
	REQUIRE(SendMessageW(handle,PBM_GETPOS,0,0) == value);
	Check(progress->SetStep(-10));
	Check(progress->SetPosition(0));
	Check(progress->StepIt());
	Check(progress->GetPosition(&value));
	REQUIRE(value == 90 && SendMessageW(handle,PBM_GETPOS,0,0) == 90);
	Check(progress->SetStep(0));
	Check(progress->StepIt());
	Check(progress->GetPosition(&value));
	REQUIRE(value == 90);
	{
		const struct StepCase
		{
			INT position;
			INT step;
		} cases[] = {{0,-100},{0,-200},{50,-150},{0,250},{0,-250},{100,0},{100,100},{25,-125}};
		for(UINT n = 0; n < ARRAYSIZE(cases); n++)
		{
			Check(progress->SetPosition(cases[n].position));
			Check(progress->SetStep(cases[n].step));
			Check(progress->StepIt());
			Check(progress->GetPosition(&value));
			SendMessageW(handle,PBM_SETPOS,cases[n].position,0);
			SendMessageW(handle,PBM_SETSTEP,cases[n].step,0);
			SendMessageW(handle,PBM_STEPIT,0,0);
			REQUIRE(SendMessageW(handle,PBM_GETPOS,0,0) == value);
		}
	}
	Check(progress->SetPosition(90));
	REQUIRE(progress->SetMin(101) == E_INVALIDARG);
	REQUIRE(progress->SetMax(-1) == E_INVALIDARG);
	Check(progress->GetPosition(&value));
	REQUIRE(value == 90 && SendMessageW(handle,PBM_GETPOS,0,0) == 90);
	observer.Attach(__new CProgressObserver);
	CheckAlloc(observer);
	Check(control->QueryInterface(&notifications));
	Check(notifications->AddObserver(observer));
	Check(progress->SetMax(50));
	REQUIRE(observer->calls == 2);
	Check(progress->GetPosition(&value));
	REQUIRE(value == 50 && SendMessageW(handle,PBM_GETPOS,0,0) == 50);
	observer->reenter = TRUE;
	Check(progress->SetPosition(5));
	Check(progress->GetPosition(&value));
	REQUIRE(value == 50 && observer->calls >= 4);
	Check(notifications->RemoveObserver(observer));
	Check(progress->SetMin(50));
	Check(progress->SetStep(2147483647));
	Check(progress->StepIt());
	Check(progress->GetPosition(&value));
	REQUIRE(value == 50);
	Check(progress->SetMin(-2147483647 - 1));
	Check(progress->SetMax(2147483647));
	Check(progress->SetPosition(2147483647));
	Check(progress->SetStep(1));
	Check(progress->StepIt());
	Check(progress->GetPosition(&value));
	REQUIRE(value == (-2147483647));
	Check(progress->SetMax(250000));
	Check(progress->SetMin(100000));
	Check(progress->SetPosition(175000));
	Check(progress->SetStep(250));
	SendMessageW(handle,PBM_GETRANGE,FALSE,reinterpret_cast<LPARAM>(&range));
	REQUIRE(range.iLow == 100000 && range.iHigh == 250000 && SendMessageW(handle,PBM_GETPOS,0,0) == 175000);
	for(UINT dpi = 96; dpi <= 192; dpi += 48)
	{
		TStackRef<IQuadooLayoutControl> layout;
		QuadooForm::Rect bounds = {0,0,500,240};
		QuadooForm::Rect effective;
		RECT native;
		Check(form->QueryInterface(&layout));
		Check(layout->Arrange(&bounds,dpi));
		Check(control->GetEffectiveBounds(&effective));
		REQUIRE(GetWindowRect(handle,&native));
		REQUIRE(native.right - native.left == MulDiv(effective.Width,dpi,96));
		Check(progress->GetPosition(&value));
		REQUIRE(value == 175000);
	}
	Check(QuadooGetFormSerializer(&serializer));
	Check(serializer->Serialize(form,&saved));
	Check(QuadooCreateControl(context,NULL,saved,&copy));
	Check(serializer->Serialize(copy,&again));
	REQUIRE(!JSONCompareObjects(saved,again));
	Check(copy->Destroy());
	copy.Release();
	observer->destroy = TRUE;
	Check(notifications->AddObserver(observer));
	Check(progress->SetPosition(180000));
	REQUIRE(!IsWindow(handle));
	REQUIRE(progress->StepIt() == QuadooForm::EDestroyed);
	Check(form->Destroy());
	control.Release();
	progress.Release();
	window.Release();
	notifications.Release();
	observer.Release();
	Check(lifetime->GetLiveCount(&after));
	REQUIRE(after == before);
	// Invalid definitions fail before registering a partially created child.
	definition.Release();
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"Invalid\",\"children\":[{\"type\":\"ProgressBar\",\"properties\":{\"Min\":10,\"Max\":5}}]}",&definition));
	REQUIRE(QuadooCreateControl(context,NULL,definition,&copy) == E_INVALIDARG && !copy);
	definition.Release();
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"ParsedRange\",\"children\":[{\"type\":\"ProgressBar\",\"name\":\"Negative\",\"properties\":{\"Min\":-2147483648,\"Max\":2147483647,\"Position\":2147483647,\"Step\":-2147483648}},{\"type\":\"ProgressBar\",\"name\":\"Clamped\",\"properties\":{\"Min\":200,\"Max\":300,\"Position\":-1}}]}",&definition));
	Check(QuadooCreateControl(context,NULL,definition,&copy));
	container.Release();
	Check(copy->QueryInterface(&container));
	Check(container->FindControl(RSTRING_CAST(L"Negative"),&control));
	Check(control->QueryInterface(&progress));
	Check(progress->GetMin(&value));
	REQUIRE(value == LONG_MIN);
	Check(progress->StepIt());
	Check(progress->GetPosition(&value));
	REQUIRE(value == -1);
	control.Release();
	progress.Release();
	Check(container->FindControl(RSTRING_CAST(L"Clamped"),&control));
	Check(control->QueryInterface(&progress));
	Check(progress->GetPosition(&value));
	REQUIRE(value == 200);
	Check(copy->Destroy());
	hr = S_OK;
Cleanup:
	if(copy)
	{
		copy->Destroy();
	}
	if(form)
	{
		form->Destroy();
	}
	return hr;
}
