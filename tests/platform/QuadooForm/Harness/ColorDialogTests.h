// Tests use only published QuadooForm contracts; shared by the native and designer suites.
struct ColorDialogThreadCall { IQuadooColorDialog* dialog; HRESULT result; BOOL accepted; };
static DWORD WINAPI ColorDialogWrongThread(PVOID data)
{
	ColorDialogThreadCall* call = static_cast<ColorDialogThreadCall*>(data);
	call->result = call->dialog->Execute(NULL,&call->accepted);
	return 0;
}
static HRESULT ColorDialogMetadata()
{
	HRESULT hr;
	HWND host = CreateWindowExW(0,L"STATIC",L"ColorDialog test owner",WS_OVERLAPPEDWINDOW,0,0,600,400,NULL,NULL,GetModuleHandle(NULL),NULL);
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooComponentRegistry> factories;
	TStackRef<IQuadooComponentFactory> factory;
	TStackRef<IQuadooFactoryLifetime> lifetime;
	TStackRef<IJSONObject> definition, saved;
	TStackRef<IQuadooFormSerializer> serializer;
	TStackRef<IQuadooControl> root, reopened;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IQuadooComponentCollection> components;
	TStackRef<IQuadooComponent> component;
	TStackRef<IQuadooColorDialog> dialog;
	TStackRef<IQuadooProperties> values;
	TStackRef<IQuadooControl> visual;
	QuadooForm::ContextOptions options = {sizeof(options)};
	QuadooProperty::Type type;
	UINT index, live;
	INT color;
	DWORD flags;
	BOOL accepted = TRUE;
	HANDLE thread = NULL;
	ColorDialogThreadCall call;
	const IID* paths[] = {&__uuidof(IQuadooComponent),&__uuidof(IQuadooColorDialog),&__uuidof(IQuadooNonvisualComponent),&__uuidof(IQuadooProperties),&__uuidof(IQuadooPropertyNotifications),&__uuidof(IQuadooConstructionParticipant),&__uuidof(IQuadooDefinitionSource)};
	CheckIf(!host,HrEnsureLastError());
	Check(QuadooGetControlRegistry(&registry));
	Check(registry->QueryInterface(__uuidof(IQuadooComponentRegistry),reinterpret_cast<PVOID*>(&factories)));
	Check(factories->FindComponentFactory(RSTRING_CAST(L"ColorDialog"),&factory));
	Check(factory->QueryInterface(__uuidof(IQuadooFactoryLifetime),reinterpret_cast<PVOID*>(&lifetime)));
	Check(lifetime->GetLiveCount(&live)); REQUIRE(live == 0);
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"ColorOwner\",\"components\":[{\"type\":\"ColorDialog\",\"name\":\"Colors\",\"properties\":{\"Color\":-2147483648}}]}",&definition));
	options.Mode = QuadooForm::Runtime; options.pRegistry = registry; options.hwndHost = host;
	Check(QuadooCreateConstructionContext(&options,&context));
	Check(QuadooCreateControl(context,NULL,definition,&root));
	Check(TestComponents(root,&components));
	Check(components->FindComponent(RSTRING_CAST(L"Colors"),&component));
	Check(component->QueryInterface(__uuidof(IQuadooColorDialog),reinterpret_cast<PVOID*>(&dialog)));
	Check(CheckCOMIdentity(component,paths,ARRAYSIZE(paths)));
	REQUIRE(component->QueryInterface(__uuidof(IQuadooControl),reinterpret_cast<PVOID*>(&visual)) == E_NOINTERFACE && !visual);
	Check(ComponentProperty(component,L"Color",&values,&index));
	{
		TStackRef<IQuadooPropertyDescriptors> descriptors;
		TStackRef<IQuadooPropertyDescriptor> descriptor;
		Check(values->GetDescriptors(&descriptors));
		Check(descriptors->GetAt(index,&descriptor));
		Check(descriptor->GetType(&type));
		Check(descriptor->GetFlags(&flags));
		REQUIRE(type == QuadooProperty::Color && (flags & QuadooForm::PropertySerializable));
	}
	Check(values->GetValueInt(index,&color)); REQUIRE(color == INT_MIN);
	REQUIRE(dialog->Execute(NULL,NULL) == E_POINTER);
	REQUIRE(dialog->Execute(reinterpret_cast<HWND>(1),&accepted) == E_INVALIDARG && !accepted);
	Check(values->SetValueInt(index,0x80000000 | COLOR_BTNFACE));
	REQUIRE(values->SetValueInt(index,static_cast<INT>(0x80000019)) == E_INVALIDARG);
	REQUIRE(values->SetValueInt(index,0x01000000) == E_INVALIDARG);
	Check(values->GetValueInt(index,&color)); REQUIRE(color == static_cast<INT>(0x80000000 | COLOR_BTNFACE));
	call.dialog = dialog; call.result = S_OK; call.accepted = TRUE;
	thread = CreateThread(NULL,0,ColorDialogWrongThread,&call,0,NULL);
	CheckIf(!thread,HrEnsureLastError());
	REQUIRE(WaitForSingleObject(thread,10000) == WAIT_OBJECT_0);
	REQUIRE(call.result == QuadooForm::EWrongThread && !call.accepted);
	Check(QuadooGetFormSerializer(&serializer));
	Check(serializer->Serialize(root,&saved));
	Check(QuadooCreateControl(context,NULL,saved,&reopened));
	{
		TStackRef<IQuadooComponentCollection> secondOwner;
		TStackRef<IQuadooComponent> second;
		TStackRef<IQuadooProperties> properties;
		UINT slot;
		Check(TestComponents(reopened,&secondOwner));
		Check(secondOwner->FindComponent(RSTRING_CAST(L"Colors"),&second));
		Check(ComponentProperty(second,L"Color",&properties,&slot));
		Check(properties->GetValueInt(slot,&color)); REQUIRE(color == static_cast<INT>(0x80000000 | COLOR_BTNFACE));
	}
	Check(lifetime->GetLiveCount(&live)); REQUIRE(live == 2);
	REQUIRE(factories->UnregisterComponentFactory(RSTRING_CAST(L"ColorDialog")) == HRESULT_FROM_WIN32(ERROR_BUSY));
	Check(root->Destroy()); Check(reopened->Destroy());
	REQUIRE(dialog->Execute(NULL,&accepted) == QuadooForm::EDestroyed && !accepted);
	Check(lifetime->GetLiveCount(&live)); REQUIRE(live == 1); // Retained interfaces keep the factory alive.
	values.Release(); dialog.Release(); component.Release();
	Check(lifetime->GetLiveCount(&live)); REQUIRE(live == 0);
	Check(factories->UnregisterComponentFactory(RSTRING_CAST(L"ColorDialog")));
	Check(factories->RegisterComponentFactory(factory));
	root.Release(); reopened.Release(); context.Release(); components.Release();
	for(UINT mode = QuadooForm::Design; mode <= QuadooForm::Preview; mode++)
	{
		TStackRef<IQuadooComponentCollection> owner;
		TStackRef<IQuadooComponent> item;
		TStackRef<IQuadooColorDialog> operation;
		options.Mode = static_cast<QuadooForm::ContextMode>(mode);
		Check(QuadooCreateConstructionContext(&options,&context));
		Check(QuadooCreateControl(context,NULL,definition,&root));
		Check(TestComponents(root,&owner));
		Check(owner->FindComponent(RSTRING_CAST(L"Colors"),&item));
		Check(item->QueryInterface(__uuidof(IQuadooColorDialog),reinterpret_cast<PVOID*>(&operation)));
		accepted = TRUE;
		REQUIRE(operation->Execute(NULL,&accepted) == QuadooForm::EInvalidState && !accepted);
		Check(root->Destroy()); root.Release(); context.Release();
	}
	hr = S_OK;
Cleanup:
	if(thread) CloseHandle(thread);
	if(root) root->Destroy();
	if(reopened) reopened->Destroy();
	if(host) DestroyWindow(host);
	return hr;
}
