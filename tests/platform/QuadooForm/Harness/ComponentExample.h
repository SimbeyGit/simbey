class CComponentExampleBinder : public CBaseUnknown, public IQuadooEventsBinder, public IQuadooDelegate
{
	IQuadooControl* m_root; // Weak; OnDestroy clears it. No delegate/root cycle.
	UINT m_ticks;
	HRESULT Caption(PCWSTR name, RSTRING text)
	{
		HRESULT hr; TStackRef<IQuadooControlContainer> root; TStackRef<IQuadooControl> label; TStackRef<IQuadooProperties> properties; UINT index;
		CheckIf(!m_root,QuadooForm::EDestroyed);
		Check(m_root->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root))); Check(root->FindControl(RSTRING_CAST(name),&label));
		Check(ComponentProperty(label,L"Caption",&properties,&index)); Check(properties->SetValueString(index,text));
	Cleanup:
		return hr;
	}
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooEventsBinder)
		UNK_INTERFACE(IQuadooDelegate)
	END_UNK_MAP
	CComponentExampleBinder() : m_root(NULL), m_ticks(0) {}
	STDMETHOD(BindEvent)(RSTRING, IQuadooDelegate** delegate) { if(!delegate) return E_POINTER; *delegate = static_cast<IQuadooDelegate*>(this); AddRef(); return S_OK; }
	STDMETHOD(PostBind)(IQuadooControl* root) { m_root = root; return S_OK; }
	STDMETHOD(Invoke)(IQuadooEventArgs* args)
	{
		HRESULT hr; CRString event, name; TStackRef<IQuadooComponent> sender;
		Check(args->GetEventName(&event)); Check(args->GetSender(&sender)); Check(sender->GetName(&name));
		if(event == RSTRING_CAST(L"OnDestroy")) { m_root = NULL; hr = S_OK; goto Cleanup; }
		CheckIf(!m_root,QuadooForm::EDestroyed);
		if(event == RSTRING_CAST(L"OnTimer")) { CRString text; Check(RStrFormatW(&text,L"Timer ticks: %u",++m_ticks)); Check(Caption(L"Clock",text)); }
		if(event == RSTRING_CAST(L"OnClick"))
		{
			TStackRef<IQuadooComponentCollection> components; TStackRef<IQuadooComponent> component; TStackRef<IQuadooFileDialog> dialog; BOOL accepted;
			Check(TestComponents(m_root,&components)); Check(components->FindComponent(name == RSTRING_CAST(L"OpenButton") ? RSTRING_CAST(L"OpenFile1") : RSTRING_CAST(L"SaveFile1"),&component));
			Check(component->QueryInterface(__uuidof(IQuadooFileDialog),reinterpret_cast<PVOID*>(&dialog)));
			Check(dialog->Execute(NULL,&accepted));
		}
		if(event == RSTRING_CAST(L"OnAccept") || event == RSTRING_CAST(L"OnCancel"))
		{
			CRString filename, text; TStackRef<IQuadooProperties> properties; UINT index;
			Check(ComponentProperty(sender,L"FileName",&properties,&index)); Check(properties->GetValueString(index,&filename));
			Check(RStrFormatW(&text,L"%ls: %ls",event == RSTRING_CAST(L"OnAccept") ? L"Selected (no file I/O)" : L"Cancelled; retained filename",filename.c_wstr())); Check(Caption(L"Status",text));
		}
		hr = S_OK;
	Cleanup:
		return hr;
	}
};
static HRESULT ShowComponentExample()
{
	HRESULT hr; TStackRef<IJSONObject> definition; TStackRef<IQuadooForm> form; TStackRef<CComponentExampleBinder> binder; LRESULT result;
	binder.Attach(__new CComponentExampleBinder); CheckAlloc(binder);
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"ComponentExample\",\"properties\":{\"Caption\":\"QuadooForm components - native example\",\"Width\":640,\"Height\":240},\"events\":{\"OnDestroy\":\"Destroyed\"},\"children\":[{\"type\":\"Button\",\"name\":\"OpenButton\",\"properties\":{\"Caption\":\"Open...\",\"Left\":20,\"Top\":20},\"events\":{\"OnClick\":\"Open\"}},{\"type\":\"Button\",\"name\":\"SaveButton\",\"properties\":{\"Caption\":\"Save...\",\"Left\":140,\"Top\":20},\"events\":{\"OnClick\":\"Save\"}},{\"type\":\"Label\",\"name\":\"Clock\",\"properties\":{\"Caption\":\"Timer ticks: 0\",\"Left\":20,\"Top\":70,\"Width\":580}},{\"type\":\"Label\",\"name\":\"Status\",\"properties\":{\"Caption\":\"Choose a file; this example performs no file I/O.\",\"Left\":20,\"Top\":110,\"Width\":600,\"Height\":100}}],\"components\":[{\"type\":\"OpenFile\",\"name\":\"OpenFile1\",\"properties\":{\"Title\":\"QuadooForm OpenFile test\",\"Filter\":\"Text files|*.txt|All files|*.*\",\"DefaultExt\":\"txt\",\"FileName\":\"previous.txt\"},\"events\":{\"OnAccept\":\"Accepted\",\"OnCancel\":\"Cancelled\"}},{\"type\":\"SaveFile\",\"name\":\"SaveFile1\",\"properties\":{\"Title\":\"QuadooForm SaveFile test\",\"DefaultExt\":\"txt\",\"FileName\":\"QuadooForm-example.txt\"},\"events\":{\"OnAccept\":\"Accepted\",\"OnCancel\":\"Cancelled\"}},{\"type\":\"Timer\",\"name\":\"Timer1\",\"properties\":{\"Interval\":250},\"events\":{\"OnTimer\":\"Tick\"}}]}",&definition));
	Check(QuadooCreateForm(binder,NULL,definition,&form)); Check(form->ShowModal(&result));
Cleanup:
	if(form) form->Destroy();
	return hr;
}
