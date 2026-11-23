// A non-ListView wrapper with a private native child hierarchy. Keyboard routing
// must discover the capability through public interfaces rather than HWND classes.
class CKeyboardInputProbe : public QF::CTextBox, public IQuadooKeyboardInput
{
public:
	HWND target;
	BOOL claims;
	UINT queries;
	HRESULT answer;
	IQuadooFormKeyboard* reenter;
	BOOL nestedHandled;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooKeyboardInput)
		UNK_CHAIN_TO(QF::CTextBox)
	END_UNK_MAP
	CKeyboardInputProbe(QF::CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) :
		CTextBox(factory,context,parent), target(NULL), claims(FALSE), queries(0), answer(S_OK),
		reenter(NULL), nestedHandled(FALSE)
	{
	}
	static const QF::TypeSpec* Type()
	{
		static const QF::TypeSpec type = {L"KeyboardInputProbe",QF::CTextBox::Type(),NULL,0,NULL,0,0};
		return &type;
	}
	STDMETHOD(OwnsKeyboardMessage)(const MSG* message, BOOL* owns)
	{
		HRESULT hr;
		CheckIf(!owns,E_POINTER);
		*owns = FALSE;
		CheckIf(!message,E_INVALIDARG);
		Check(CheckAlive());
		queries++;
		if(reenter)
		{
			MSG nested = *message;
			Check(reenter->PreprocessMessage(&nested,&nestedHandled));
		}
		Check(answer);
		*owns = claims && message->hwnd == target;
	Cleanup:
		return hr;
	}
};

class CKeyboardInputFactory : public QF::TFactory<CKeyboardInputProbe>
{
	virtual QF::CControl* Allocate(IQuadooConstructionContext* context, IQuadooControlContainer* parent)
	{
		last = __new CKeyboardInputProbe(this,context,parent);
		return last;
	}
public:
	CKeyboardInputProbe* last;
	CKeyboardInputFactory() : last(NULL)
	{
	}
};

class CKeyboardInputBinder : public CBaseUnknown, public IQuadooEventsBinder, public IQuadooDelegate
{
public:
	UINT previews;
	UINT clicks;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooEventsBinder)
		UNK_INTERFACE(IQuadooDelegate)
	END_UNK_MAP
	CKeyboardInputBinder() : previews(0), clicks(0)
	{
	}
	STDMETHOD(BindEvent)(RSTRING, IQuadooDelegate** delegate)
	{
		HRESULT hr = S_OK;
		CheckIf(!delegate,E_POINTER);
		*delegate = this;
		AddRef();
	Cleanup:
		return hr;
	}
	STDMETHOD(PostBind)(IQuadooControl*)
	{
		return S_OK;
	}
	STDMETHOD(Invoke)(IQuadooEventArgs* args)
	{
		HRESULT hr;
		CRString name;
		Check(args->GetEventName(&name));
		if(name == RSTRING_CAST(L"OnClick"))
		{
			clicks++;
		}
		else
		{
			previews++;
			Check(args->SetBool(1,TRUE));
		}
	Cleanup:
		return hr;
	}
};

static HRESULT KeyboardInputTests()
{
	HRESULT hr;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<CKeyboardInputFactory> factory;
	TStackRef<CKeyboardInputBinder> binder;
	TStackRef<CKeyboardInputProbe> probe;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IJSONObject> definition;
	TStackRef<IQuadooControl> root;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooForm> form;
	TStackRef<IQuadooFormKeyboard> keyboard;
	TStackRef<IQuadooKeyboardInput> input;
	TStackRef<IUnknown> identity;
	TStackRef<IUnknown> other;
	QuadooForm::ContextOptions options = {0};
	MSG message = {0};
	HWND window;
	HWND native;
	HWND child;
	HWND edit;
	BOOL handled;
	BOOL owns;
	BOOL changed = FALSE;
	BYTE saved[256];
	BYTE state[256];
	UINT live;
	factory.Attach(__new CKeyboardInputFactory);
	binder.Attach(__new CKeyboardInputBinder);
	CheckAlloc(factory);
	CheckAlloc(binder);
	Check(QuadooGetControlRegistry(&registry));
	Check(factory->Initialize());
	Check(registry->RegisterFactory(factory));
	options.cbSize = sizeof(options);
	options.DPI = 96;
	options.pRegistry = registry;
	options.pBinder = binder;
	Check(QuadooCreateConstructionContext(&options,&context));
	Check(NativeDefinition(
		L"{\"type\":\"Form\",\"properties\":{\"Width\":300,\"Height\":160,\"KeyPreview\":true,"
		L"\"MainMenu\":\"{982D3760-AE22-4851-AB59-BA5C57778001}\"},\"events\":{\"OnKeyDown\":\"Key\",\"OnKeyPress\":\"Key\",\"OnKeyUp\":\"Key\"},\"children\":["
		L"{\"type\":\"KeyboardInputProbe\",\"name\":\"Input\",\"properties\":{\"Width\":180,\"Height\":40}},"
		L"{\"type\":\"Button\",\"name\":\"Default\",\"properties\":{\"Top\":60,\"Default\":true,\"Cancel\":true},\"events\":{\"OnClick\":\"Run\"}}],"
		L"\"components\":[{\"type\":\"MainMenu\",\"name\":\"Menu\",\"id\":\"{982D3760-AE22-4851-AB59-BA5C57778001}\"},"
		L"{\"type\":\"MenuItem\",\"name\":\"Action\",\"menuParent\":\"{982D3760-AE22-4851-AB59-BA5C57778001}\","
		L"\"properties\":{\"Caption\":\"Action\",\"ShortCut\":\"Ctrl+K\"},\"events\":{\"OnClick\":\"Run\"}}]}",&definition));
	Check(QuadooCreateControl(context,NULL,definition,&root));
	Check(root->QueryInterface(__uuidof(IQuadooForm),reinterpret_cast<PVOID*>(&form)));
	Check(root->QueryInterface(__uuidof(IQuadooFormKeyboard),reinterpret_cast<PVOID*>(&keyboard)));
	Check(form->FindControl(RSTRING_CAST(L"Input"),&control));
	Check(control->QueryInterface(__uuidof(IQuadooKeyboardInput),reinterpret_cast<PVOID*>(&input)));
	Check(control->QueryInterface(__uuidof(IUnknown),reinterpret_cast<PVOID*>(&identity)));
	Check(input->QueryInterface(__uuidof(IUnknown),reinterpret_cast<PVOID*>(&other)));
	VERIFY(identity == static_cast<IUnknown*>(other));
	identity.Release();
	other.Release();
	probe = factory->last;
	Check(probe->GetHandle(&native));
	Check(form->GetHandle(&window));
	Check(form->ShowForm(SW_SHOW));
	child = CreateWindowExW(0,L"STATIC",L"",WS_CHILD | WS_VISIBLE,0,0,170,30,native,NULL,g_hInstance,NULL);
	CheckIf(!child,HrEnsureLastError());
	edit = CreateWindowExW(0,L"EDIT",L"",WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,0,0,160,25,child,NULL,g_hInstance,NULL);
	CheckIf(!edit,HrEnsureLastError());
	SetFocus(edit);
	VERIFY(GetFocus() == edit);
	probe->target = edit;
	probe->claims = TRUE;
	message.hwnd = edit;
	message.message = WM_KEYDOWN;
	message.wParam = VK_RETURN;
	VERIFY(input->OwnsKeyboardMessage(&message,NULL) == E_POINTER);
	VERIFY(input->OwnsKeyboardMessage(NULL,&owns) == E_INVALIDARG && !owns);
	const UINT messages[] = {WM_KEYDOWN,WM_KEYDOWN,WM_KEYDOWN,WM_CHAR,WM_KEYUP,WM_SYSKEYDOWN};
	const WPARAM keys[] = {VK_RETURN,VK_ESCAPE,VK_TAB,'a','A','X'};
	for(UINT n = 0; n < ARRAYSIZE(messages); n++)
	{
		message.message = messages[n];
		message.wParam = keys[n];
		Check(keyboard->PreprocessMessage(&message,&handled));
		VERIFY(!handled && !binder->clicks && !binder->previews && GetFocus() == edit);
		if(message.message == WM_CHAR)
		{
			DispatchMessageW(&message);
		}
	}
	WCHAR text[8];
	VERIFY(GetWindowTextW(edit,text,ARRAYSIZE(text)) == 1 && text[0] == L'a');
	CheckIf(!GetKeyboardState(saved),HrEnsureLastError());
	CopyMemory(state,saved,sizeof(state));
	state[VK_CONTROL] = 0x80;
	CheckIf(!SetKeyboardState(state),HrEnsureLastError());
	changed = TRUE;
	message.message = WM_KEYDOWN;
	message.wParam = 'K';
	Check(keyboard->PreprocessMessage(&message,&handled));
	VERIFY(!handled && !binder->clicks);
	probe->claims = FALSE;
	Check(keyboard->PreprocessMessage(&message,&handled));
	VERIFY(handled && binder->clicks == 1);
	SetKeyboardState(saved);
	changed = FALSE;
	message.wParam = 'B';
	Check(keyboard->PreprocessMessage(&message,&handled));
	VERIFY(handled && binder->previews == 1);
	message.wParam = VK_RETURN;
	Check(keyboard->PreprocessMessage(&message,&handled));
	VERIFY(handled && binder->clicks == 2);
	// An unrelated HWND is neither queried nor intercepted by this Form.
	UINT queries = probe->queries;
	message.hwnd = GetDesktopWindow();
	Check(keyboard->PreprocessMessage(&message,&handled));
	VERIFY(!handled && probe->queries == queries);
	message.hwnd = edit;
	probe->claims = TRUE;
	probe->reenter = keyboard;
	Check(keyboard->PreprocessMessage(&message,&handled));
	probe->reenter = NULL;
	VERIFY(!handled && probe->nestedHandled && probe->queries == queries + 1);
	probe->answer = E_ABORT;
	VERIFY(keyboard->PreprocessMessage(&message,&handled) == E_ABORT && handled);
	probe->answer = S_OK;
	Check(keyboard->PreprocessMessage(&message,&handled));
	VERIFY(!handled);
	Check(root->Destroy());
	VERIFY(!IsWindow(window) && !IsWindow(edit));
	VERIFY(input->OwnsKeyboardMessage(&message,&owns) == QuadooForm::EDestroyed && !owns);
	input.Release();
	probe.Release();
	control.Release();
	Check(factory->GetLiveCount(&live));
	VERIFY(live == 0);
	hr = S_OK;
Cleanup:
	if(changed)
	{
		SetKeyboardState(saved);
	}
	if(root)
	{
		root->Destroy();
	}
	return hr;
}
