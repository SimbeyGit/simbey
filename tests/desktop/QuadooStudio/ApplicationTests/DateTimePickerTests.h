static HRESULT DateTimePickerScriptTests()
{
	HRESULT hr;
	TStackRef<IJSONObject> definition;
	TStackRef<IQuadooControl> form;
	TStackRef<IQuadooControlContainer> container;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooDateTimePicker> picker;
	TStackRef<AQVM::CComponentObject> wrapper;
	TStackRef<IQuadooInstanceLoader> loader;
	TStackRef<IQuadooVM> vm;
	TStackRef<IQuadooObject> exception;
	CMemoryStream code;
	CCompilerStatus compiler;
	QuadooVM::QVARIANT result = {QuadooVM::Null};
	HRESULT registered;
	SYSTEMTIME time;
	static const WCHAR script[] =
		L"function PickerChecks()\n{\n"
		L"\tPicker.ShowCheckBox = true;\n\tPicker.Checked = false;\n"
		L"\tPicker.Kind = \"Time\";\n\tPicker.Format = \"HH:mm:ss\";\n"
		L"\tPicker.DateTime = \"2033-02-03T04:05:06.007\";\n"
		L"\tif(Picker.Checked || Picker.DateTime != \"2033-02-03T04:05:06.007\")\n\t{\n\t\treturn false;\n\t}\n"
		L"\tPicker.Checked = true;\n\tPicker.MinDate = \"2033-02-01\";\n\tPicker.MaxDate = \"2033-02-28\";\n"
		L"\tvar failed = false;\n\ttry\n\t{\n\t\tPicker.DateTime = \"2033-02-29\";\n\t}\n\tcatch(error)\n\t{\n\t\tfailed = true;\n\t}\n"
		L"\treturn failed && Picker.Checked && Picker.DateTime == \"2033-02-03T04:05:06.007\";\n}\n";
	Check(AQVM::ParseObject(SLP(L"{\"type\":\"Form\",\"name\":\"PickerScripts\",\"children\":[{\"type\":\"DateTimePicker\",\"name\":\"Picker\"}]}"),&definition));
	{
		TStackRef<IQuadooForm> created;
		Check(QuadooCreateForm(NULL,NULL,definition,&created));
		Check(created->QueryInterface(&form));
	}
	Check(form->QueryInterface(&container));
	Check(container->FindControl(RSTRING_CAST(L"Picker"),&control));
	Check(control->QueryInterface(&picker));
	Check(AQVM::NewComponentObject(NULL,control,&wrapper));
	Check(QuadooParseTextToStream(SLP(L"PickerTests.quadoo"),SLP(script),0,&code,NULL,&compiler));
	Check(QVMCreateLoader(NULL,&loader));
	Check(loader->AddInstance(RSTRING_CAST(L"PickerTests"),NULL,&code,code.DataRemaining(),NULL));
	Check(loader->LoadVM(RSTRING_CAST(L"PickerTests"),&registered,&vm));
	Check(vm->AddGlobal(RSTRING_CAST(L"Picker"),wrapper));
	Check(vm->RunConstructor(&exception));
	REQUIRE(!exception);
	Check(Call(vm,L"PickerChecks",&result));
	REQUIRE(result.eType == QuadooVM::Bool && result.fVal);
	Check(picker->GetDateTime(&time));
	REQUIRE(time.wYear == 2033 && time.wDay == 3 && time.wHour == 4 && time.wMilliseconds == 7);
	Check(form->Destroy());
	QVMClearVariant(&result);
	REQUIRE(wrapper->GetProperty(NULL,RSTRING_CAST(L"DateTime"),&result) == QuadooForm::EDestroyed);
	hr = S_OK;
Cleanup:
	QVMClearVariant(&result);
	if(vm)
	{
		vm->Unload();
	}
	if(form)
	{
		form->Destroy();
	}
	return hr;
}
