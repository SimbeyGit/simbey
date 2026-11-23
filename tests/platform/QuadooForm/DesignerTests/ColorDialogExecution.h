class CColorDialogProbe : public CBaseUnknown, public IQuadooEventsBinder, public IQuadooDelegate, public IQuadooPropertyObserver
{
public:
	UINT accepts, cancels, notices;
	HRESULT callback, observer, recursive;
	BOOL destroyAccepted;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooEventsBinder)
		UNK_INTERFACE(IQuadooDelegate)
		UNK_INTERFACE(IQuadooPropertyObserver)
	END_UNK_MAP
	CColorDialogProbe() : accepts(0), cancels(0), notices(0), callback(S_OK), observer(S_OK), recursive(S_OK), destroyAccepted(FALSE) {}
	STDMETHOD(BindEvent)(RSTRING, IQuadooDelegate** handler)
	{
		if(!handler) return E_POINTER;
		*handler = static_cast<IQuadooDelegate*>(this); AddRef(); return S_OK;
	}
	STDMETHOD(PostBind)(IQuadooControl*) { return S_OK; }
	STDMETHOD(Invoke)(IQuadooEventArgs* args)
	{
		HRESULT hr;
		CRString event;
		TStackRef<IQuadooComponent> sender;
		TStackRef<IQuadooColorDialog> dialog;
		BOOL accepted = TRUE;
		Check(args->GetEventName(&event));
		Check(args->GetSender(&sender));
		if(event == RSTRING_CAST(L"OnAccept")) accepts++;
		if(event == RSTRING_CAST(L"OnCancel")) cancels++;
		Check(sender->QueryInterface(__uuidof(IQuadooColorDialog),reinterpret_cast<PVOID*>(&dialog)));
		recursive = dialog->Execute(NULL,&accepted);
		REQUIRE(recursive == QuadooForm::EInvalidState && !accepted);
		if(destroyAccepted && event == RSTRING_CAST(L"OnAccept"))
		{
			Check(sender->Destroy());
		}
		hr = callback;
	Cleanup:
		return hr;
	}
	STDMETHOD(OnPropertyChanged)(IQuadooComponent*, UINT) { notices++; return observer; }
	STDMETHOD(OnBoundsChanged)(IQuadooControl*, const QuadooForm::Rect*) { return E_UNEXPECTED; }
};
static HRESULT ColorDialogExecution()
{
	HRESULT hr;
	HWND host = Host(), owner;
	TStackRef<IQuadooControl> root;
	TStackRef<IQuadooComponentCollection> components;
	TStackRef<IQuadooComponent> component;
	TStackRef<IQuadooColorDialog> dialog;
	TStackRef<IQuadooPropertyNotifications> notifications;
	TStackRef<IQuadooProperties> values;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IQuadooControlContainer> window;
	TStackRef<IJSONObject> definition;
	TStackRef<CColorDialogProbe> binder;
	QuadooForm::ContextOptions options = {sizeof(options)};
	UINT index;
	INT color;
	BOOL accepted;
	CheckIf(!host,HrEnsureLastError());
	binder.Attach(__new CColorDialogProbe);
	CheckAlloc(binder);
	options.Mode = QuadooForm::Runtime; options.hwndHost = host; options.pBinder = binder;
	Check(QuadooCreateConstructionContext(&options,&context));
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"DialogOwner\",\"components\":[{\"type\":\"ColorDialog\",\"name\":\"Colors\",\"properties\":{\"Color\":-2147483635},\"events\":{\"OnAccept\":\"Accept\",\"OnCancel\":\"Cancel\"}}]}",&definition));
	Check(QuadooCreateControl(context,NULL,definition,&root));
	Check(root->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&window)));
	Check(window->GetHandle(&owner));
	Check(TestComponents(root,&components));
	Check(components->FindComponent(RSTRING_CAST(L"Colors"),&component));
	Check(component->QueryInterface(__uuidof(IQuadooColorDialog),reinterpret_cast<PVOID*>(&dialog)));
	Check(component->QueryInterface(__uuidof(IQuadooPropertyNotifications),reinterpret_cast<PVOID*>(&notifications)));
	Check(ComponentProperty(component,L"Color",&values,&index));
	Check(notifications->AddObserver(binder));
	for(UINT n = 0; n < 7; n++)
	{
		const COLORREF initial = 0x80000000 | COLOR_HIGHLIGHT;
		Check(values->SetValueInt(index,initial));
		binder->notices = 0;
		binder->callback = n == 2 || n == 5 ? E_FAIL : S_OK;
		binder->observer = n == 3 ? E_OUTOFMEMORY : S_OK;
		binder->destroyAccepted = n == 6;
		PickerTest test = n == 0 || n == 5 ? PickerCancel : n == 4 ? PickerKeep : PickerHex;
		BOOL useRoot = n == 4;
		Check(StartPicker(useRoot ? owner : host,test,initial));
		HRESULT shown = dialog->Execute(useRoot ? NULL : host,&accepted);
		KillTimer(useRoot ? owner : host,5430);
		Check(g_picker.error);
		Check(PickerResourcesReleased());
		if(n == 0) REQUIRE(shown == S_FALSE && !accepted && binder->cancels == 1 && !binder->notices);
		else if(n == 2 || n == 3 || n == 5) REQUIRE(shown == (n == 2 || n == 5 ? E_FAIL : E_OUTOFMEMORY) && !accepted);
		else REQUIRE(shown == S_OK && accepted);
		if(n != 6)
		{
			Check(values->GetValueInt(index,&color));
			REQUIRE(static_cast<COLORREF>(color) == (n == 1 ? RGB(30,144,255) : initial));
		}
		if(n == 1 || n == 3) REQUIRE(binder->notices == 1);
		if(n == 2 || n == 4 || n == 5 || n == 6) REQUIRE(binder->notices == 0);
		binder->callback = binder->observer = S_OK;
	}
	REQUIRE(binder->accepts == 5 && binder->cancels == 2);
	REQUIRE(dialog->Execute(host,&accepted) == QuadooForm::EDestroyed && !accepted);
	hr = S_OK;
Cleanup:
	if(root) root->Destroy();
	if(host) { KillTimer(host,5430); DestroyWindow(host); }
	return hr;
}
