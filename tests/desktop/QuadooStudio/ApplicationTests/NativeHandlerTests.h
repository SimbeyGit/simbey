// Exercises a published-interface control in QuadooFormTestExtension.dll through
// the production adapter and a real VM. No internal native control base is used.
static HRESULT NativeHandlerTests()
{
	HRESULT hr;
	HRESULT registered;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooControlFactory> factory;
	TStackRef<IQuadooFactoryLifetime> lifetime;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IJSONValue> json;
	TStackRef<IJSONObject> definition;
	TStackRef<IQuadooControl> root, control, plain;
	TStackRef<IQuadooControlContainer> container;
	TStackRef<AQVM::CComponentObject> wrapper;
	TStackRef<AQVM::CFormObject> unattached;
	TStackRef<IQuadooObject> native, payload;
	TStackRef<IUnknown> identity;
	TStackRef<IUnknown> nativeIdentity;
	TStackRef<IQuadooInstanceLoader> loader;
	TStackRef<IQuadooVM> vm;
	TStackRef<IQuadooObject> exception;
	CMemoryStream code;
	CCompilerStatus compiler;
	QuadooForm::ContextOptions options = {sizeof(options)};
	QuadooVM::QVARIANT value = {QuadooVM::I4};
	QuadooVM::QVARIANT index = {QuadooVM::I4};
	QuadooVM::QVARIANT result = {QuadooVM::Null};
	QuadooVM::QVARIANT argument = {QuadooVM::Object};
	QuadooVM::QVARIANT* member;
	QuadooVM::QVPARAMS parameters = {0};
	ULONG ordinal;
	DWORD arguments;
	UINT count;
	BOOL installed = FALSE;
	static const WCHAR formJSON[] = L"{\"schemaVersion\":1,\"type\":\"Form\",\"name\":\"NativeHandlerForm\"}";
	static const WCHAR controlJSON[] = L"{\"type\":\"ExternalScriptMeter\",\"name\":\"Native1\"}";
	static const WCHAR buttonJSON[] = L"{\"type\":\"Button\",\"name\":\"Plain\",\"properties\":{\"Caption\":\"Descriptor fallback\"}}";
	static const WCHAR script[] =
		L"function Probe(control)\n"
		L"{\n"
		L"\tcontrol.NativeValue = 17;\n"
		L"\tif(control.NativeValue != 17)\n\t{\n\t\treturn -1;\n\t}\n"
		L"\tcontrol.Slots[1] = 29;\n"
		L"\tif(control.Slots[1] != 29)\n\t{\n\t\treturn -2;\n\t}\n"
		L"\tcontrol[2] = 37;\n"
		L"\tif(control[2] != 37)\n\t{\n\t\treturn -3;\n\t}\n"
		L"\treturn control.Echo(control.NativeValue + control.Slots[1] + control[2]);\n"
		L"}\n";

	Stage("Independent native IQuadooObject forwarding...\r\n");
	Check(QuadooGetControlRegistry(&registry));
	Check(CreateExternalScriptFactory(&factory));
	Check(factory->QueryInterface(&lifetime));
	Check(registry->RegisterFactory(factory));
	installed = TRUE;
	options.Mode = QuadooForm::Runtime;
	options.DPI = 96;
	options.pRegistry = registry;
	Check(QuadooCreateConstructionContext(&options,&context));
	Check(JSONParse(NULL,SLP(formJSON),&json));
	Check(json->GetObject(&definition));
	Check(QuadooCreateControl(context,NULL,definition,&root));
	Check(root->QueryInterface(&container));
	definition.Release();
	json.Release();
	Check(JSONParse(NULL,SLP(controlJSON),&json));
	Check(json->GetObject(&definition));
	Check(QuadooCreateControl(context,container,definition,&control));
	Check(container->GetChildCount(&count));
	REQUIRE(count == 1);
	Check(control->QueryInterface(&native));
	Check(control->QueryInterface(IID_IUnknown,reinterpret_cast<PVOID*>(&identity)));
	Check(native->QueryInterface(IID_IUnknown,reinterpret_cast<PVOID*>(&nativeIdentity)));
	REQUIRE(identity == static_cast<IUnknown*>(nativeIdentity));
	Check(AQVM::NewComponentObject(NULL,control,&wrapper));
	// Form/frame wrappers start unattached before their construction is complete.
	unattached.Attach(__new AQVM::CFormObject(NULL));
	CheckAlloc(unattached);
	REQUIRE(unattached->GetProperty(NULL,RSTRING_CAST(L"NativeValue"),&result) == QuadooForm::EInvalidState);
	unattached.Release();

	Check(QuadooParseTextToStream(SLP(L"NativeHandlerProbe"),SLP(script),0,&code,NULL,&compiler));
	Check(QVMCreateLoader(NULL,&loader));
	Check(loader->AddInstance(RSTRING_CAST(L"NativeHandlerProbe"),NULL,&code,code.DataRemaining(),NULL));
	Check(loader->LoadVM(RSTRING_CAST(L"NativeHandlerProbe"),&registered,&vm));
	Check(vm->RunConstructor(&exception));
	REQUIRE(!exception);
	Check(vm->FindFunction(L"Probe",&ordinal,&arguments));
	REQUIRE(arguments == 1);
	argument.pObject = wrapper;
	Check(vm->PushValue(&argument));
	Check(vm->RunFunction(ordinal,&result));
	REQUIRE(result.eType == QuadooVM::I4 && result.lVal == 83);
	QVMClearVariant(&result);
	Check(wrapper->GetProperty(NULL,RSTRING_CAST(L"SawVM"),&result));
	REQUIRE(result.eType == QuadooVM::Bool && result.fVal);
	QVMClearVariant(&result);

	value.lVal = 61;
	Check(wrapper->SetProperty(NULL,RSTRING_CAST(L"NativeValue"),&value));
	Check(wrapper->GetProperty(NULL,RSTRING_CAST(L"NativeValue"),&result));
	REQUIRE(result.eType == QuadooVM::I4 && result.lVal == 61);
	QVMClearVariant(&result);
	Check(wrapper->GetProperty(NULL,RSTRING_CAST(L"SawVM"),&result));
	REQUIRE(result.eType == QuadooVM::Bool && !result.fVal);
	QVMClearVariant(&result);
	Check(wrapper->GetDefaultValue(&result));
	REQUIRE(result.eType == QuadooVM::I4 && result.lVal == 61);
	QVMClearVariant(&result);
	Check(wrapper->GetMemberVariable(0,&member));
	REQUIRE(member && member->eType == QuadooVM::I4 && member->lVal == 61);
	member->lVal = 62;
	Check(wrapper->GetDefaultValue(&result));
	REQUIRE(result.lVal == 62);
	QVMClearVariant(&result);
	REQUIRE(wrapper->GetMemberVariable(1,&member) == DISP_E_BADINDEX);
	REQUIRE(member == NULL);
	for(INT slot = 0; slot < 3; slot++)
	{
		index.lVal = slot;
		value.lVal = 70 + slot;
		Check(wrapper->SetIndexedProperty(vm,NULL,&index,&value));
		Check(wrapper->GetIndexedProperty(vm,RSTRING_CAST(L"Slots"),&index,&result));
		REQUIRE(result.eType == QuadooVM::I4 && result.lVal == 70 + slot);
		QVMClearVariant(&result);
	}
	index.lVal = 3;
	REQUIRE(wrapper->GetIndexedProperty(vm,NULL,&index,&result) == DISP_E_BADINDEX);
	REQUIRE(wrapper->SetIndexedProperty(vm,NULL,&index,&value) == DISP_E_BADINDEX);
	REQUIRE(wrapper->GetProperty(vm,RSTRING_CAST(L"Pending"),&result) == E_PENDING);
	// The handler owns dispatch; descriptor properties must not silently fallback.
	REQUIRE(wrapper->GetProperty(vm,RSTRING_CAST(L"Name"),&result) == DISP_E_MEMBERNOTFOUND);
	REQUIRE(wrapper->SetProperty(vm,RSTRING_CAST(L"Width"),&value) == DISP_E_MEMBERNOTFOUND);
	REQUIRE(wrapper->Invoke(vm,RSTRING_CAST(L"SetFocus"),&parameters,&result) == DISP_E_MEMBERNOTFOUND);
	Check(wrapper->GetProperty(NULL,RSTRING_CAST(L"State"),&result));
	REQUIRE(result.eType == QuadooVM::I4 && result.lVal == QuadooForm::Ready);
	QVMClearVariant(&result);
	definition.Release();
	json.Release();
	Check(JSONParse(NULL,SLP(buttonJSON),&json));
	Check(json->GetObject(&definition));
	Check(QuadooCreateControl(context,container,definition,&plain));
	{
		TStackRef<AQVM::CComponentObject> object;
		Check(AQVM::NewComponentObject(NULL,plain,&object));
		payload = object;
	}
	argument.pObject = payload;
	Check(wrapper->SetProperty(vm,RSTRING_CAST(L"Payload"),&argument));
	Check(wrapper->GetProperty(vm,RSTRING_CAST(L"Payload"),&result));
	REQUIRE(result.eType == QuadooVM::Object && AQVM::SameIdentity(result.pObject,payload));
	QVMClearVariant(&result);
	REQUIRE(wrapper->DeleteProperty(RSTRING_CAST(L"NativeValue"),&result) == S_FALSE);
	REQUIRE(result.eType == QuadooVM::I4 && result.lVal == 62);
	QVMClearVariant(&result);
	REQUIRE(wrapper->DeleteProperty(RSTRING_CAST(L"NativeValue"),NULL) == S_FALSE);
	REQUIRE(wrapper->DeleteProperty(RSTRING_CAST(L"Missing"),NULL) == DISP_E_MEMBERNOTFOUND);
	REQUIRE(wrapper->GetIndexedProperty(vm,NULL,NULL,&result) == E_POINTER);
	REQUIRE(wrapper->GetIndexedProperty(vm,NULL,&index,NULL) == E_POINTER);
	REQUIRE(wrapper->SetProperty(vm,RSTRING_CAST(L"NativeValue"),NULL) == E_POINTER);
	REQUIRE(wrapper->SetIndexedProperty(vm,NULL,NULL,&value) == E_POINTER);
	REQUIRE(wrapper->SetIndexedProperty(vm,NULL,&index,NULL) == E_POINTER);
	REQUIRE(wrapper->GetDefaultValue(NULL) == E_POINTER);
	REQUIRE(wrapper->GetMemberVariable(0,NULL) == E_POINTER);

	// Replacing/clearing the native component refreshes the cached capability.
	wrapper->AttachComponent(plain);
	Check(wrapper->GetProperty(NULL,RSTRING_CAST(L"Caption"),&result));
	REQUIRE(result.eType == QuadooVM::String && !lstrcmpW(RStrToWide(result.rstrVal),L"Descriptor fallback"));
	QVMClearVariant(&result);
	REQUIRE(wrapper->GetDefaultValue(&result) == E_NOTIMPL);
	REQUIRE(wrapper->GetMemberVariable(0,&member) == E_NOTIMPL);
	REQUIRE(wrapper->DeleteProperty(RSTRING_CAST(L"Caption"),NULL) == E_NOTIMPL);
	REQUIRE(wrapper->SetIndexedProperty(NULL,NULL,&index,&value) == E_NOTIMPL);
	wrapper->AttachComponent(NULL);
	REQUIRE(wrapper->GetDefaultValue(&result) == QuadooForm::EInvalidState);
	wrapper->AttachComponent(control);
	Check(wrapper->GetDefaultValue(&result));
	REQUIRE(result.eType == QuadooVM::I4 && result.lVal == 0);
	QVMClearVariant(&result);
	Check(wrapper->Invoke(vm,RSTRING_CAST(L"Destroy"),&parameters,&result));
	REQUIRE(wrapper->GetProperty(vm,RSTRING_CAST(L"NativeValue"),&result) == QuadooForm::EDestroyed);
	REQUIRE(wrapper->SetProperty(vm,RSTRING_CAST(L"NativeValue"),&value) == QuadooForm::EDestroyed);
	REQUIRE(wrapper->GetIndexedProperty(vm,NULL,&index,&result) == QuadooForm::EDestroyed);
	REQUIRE(wrapper->SetIndexedProperty(vm,NULL,&index,&value) == QuadooForm::EDestroyed);
	REQUIRE(wrapper->GetMemberVariable(0,&member) == QuadooForm::EDestroyed);
	REQUIRE(wrapper->GetDefaultValue(&result) == QuadooForm::EDestroyed);
	REQUIRE(wrapper->DeleteProperty(RSTRING_CAST(L"NativeValue"),NULL) == QuadooForm::EDestroyed);
	REQUIRE(wrapper->Invoke(vm,RSTRING_CAST(L"Echo"),&parameters,&result) == QuadooForm::EDestroyed);
	Check(wrapper->GetProperty(NULL,RSTRING_CAST(L"State"),&result));
	REQUIRE(result.eType == QuadooVM::I4 && result.lVal == QuadooForm::Destroyed);
	QVMClearVariant(&result);
	Check(lifetime->GetLiveCount(&count));
	REQUIRE(count == 1);
	REQUIRE(ExternalCanUnload() == S_FALSE);
	wrapper.Release();
	native.Release();
	identity.Release();
	nativeIdentity.Release();
	control.Release();
	Check(lifetime->GetLiveCount(&count));
	REQUIRE(count == 0);
	Check(registry->UnregisterFactory(RSTRING_CAST(L"ExternalScriptMeter")));
	installed = FALSE;
	lifetime.Release();
	factory.Release();
	REQUIRE(ExternalCanUnload() == S_OK);
	Check(UnloadExternalExtension());
	Stage("Independent native handler forwarding passed.\r\n");
Cleanup:
	QVMClearVariant(&result);
	if(vm)
	{
		vm->Unload();
	}
	if(root)
	{
		root->Destroy();
	}
	if(installed)
	{
		registry->UnregisterFactory(RSTRING_CAST(L"ExternalScriptMeter"));
	}
	return hr;
}
