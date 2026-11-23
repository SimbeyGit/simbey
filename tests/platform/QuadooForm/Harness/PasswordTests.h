static HRESULT TextBoxPasswords()
{
	HRESULT hr;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONObject> saved;
	TStackRef<IJSONObject> again;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<CBinder> binder;
	TStackRef<IQuadooControl> form;
	TStackRef<IQuadooControl> copy;
	TStackRef<IQuadooControlContainer> container;
	TStackRef<IQuadooControl> edit;
	TStackRef<IQuadooControl> masked;
	TStackRef<IQuadooControl> memo;
	TStackRef<IQuadooWinControl> window;
	TStackRef<IQuadooProperties> properties;
	TStackRef<IQuadooPropertyDescriptors> descriptors;
	TStackRef<IQuadooPropertyDescriptor> descriptor;
	TStackRef<IJSONValue> value;
	TStackRef<IQuadooFormSerializer> serializer;
	CRString text;
	UINT index;
	UINT textIndex;
	DWORD flags;
	DWORD start;
	DWORD end;
	HWND handle;
	HWND same;
	BOOL password;
	bool defaultPassword;
	QuadooProperty::Type type;
	WCHAR native[64];
	binder.Attach(__new CBinder);
	CheckAlloc(binder);
	Check(Context(96,binder,&context));
	Check(Parse(L"{\"type\":\"Form\",\"children\":["
		L"{\"type\":\"TextBox\",\"name\":\"Plain\",\"properties\":{\"Text\":\"secret\"},\"events\":{\"OnChange\":\"Changed\"}},"
		L"{\"type\":\"TextBox\",\"name\":\"Masked\",\"properties\":{\"Text\":\"initial\",\"Password\":true}},"
		L"{\"type\":\"Memo\",\"name\":\"Memo\"}]}",&definition));
	Check(QuadooCreateControl(context,NULL,definition,&form));
	Check(form->QueryInterface(&container));
	Check(container->FindControl(RSTRING_CAST(L"Plain"),&edit));
	Check(container->FindControl(RSTRING_CAST(L"Masked"),&masked));
	Check(container->FindControl(RSTRING_CAST(L"Memo"),&memo));
	Check(Property(edit,L"Password",&properties,&index));
	Check(properties->GetDescriptors(&descriptors));
	Check(descriptors->Find(RSTRING_CAST(L"Password"),&descriptor));
	Check(descriptor->GetType(&type));
	Check(descriptor->GetFlags(&flags));
	Check(descriptor->GetDefaultValue(&value));
	Check(value->GetBoolean(&defaultPassword));
	REQUIRE(type == QuadooProperty::Boolean && !defaultPassword && (flags & QuadooForm::PropertySerializable));
	Check(properties->GetValueBool(index,&password));
	Check(edit->QueryInterface(&window));
	Check(window->GetHandle(&handle));
	REQUIRE(!password && !(GetWindowLongW(handle,GWL_STYLE) & ES_PASSWORD) && !SendMessageW(handle,EM_GETPASSWORDCHAR,0,0));
	{
		TStackRef<IQuadooWinControl> other;
		TStackRef<IQuadooProperties> otherProperties;
		HWND otherHandle;
		UINT otherIndex;
		Check(masked->QueryInterface(&other));
		Check(other->GetHandle(&otherHandle));
		REQUIRE((GetWindowLongW(otherHandle,GWL_STYLE) & ES_PASSWORD) && SendMessageW(otherHandle,EM_GETPASSWORDCHAR,0,0));
		REQUIRE(Property(memo,L"Password",&otherProperties,&otherIndex) == DISP_E_MEMBERNOTFOUND);
	}
	// Native typing still publishes real Text, and masking preserves edit state.
	SendMessageW(handle,EM_SETSEL,1,3);
	SendMessageW(handle,EM_REPLACESEL,TRUE,reinterpret_cast<LPARAM>(L"XX"));
	REQUIRE(binder->changed == 1 && SendMessageW(handle,EM_CANUNDO,0,0));
	SendMessageW(handle,EM_SETSEL,1,4);
	SendMessageW(handle,EM_SETMODIFY,TRUE,0);
	for(UINT n = 0; n < 8; n++)
	{
		Check(properties->SetValueBool(index,TRUE));
		Check(window->GetHandle(&same));
		REQUIRE(same == handle && (GetWindowLongW(handle,GWL_STYLE) & ES_PASSWORD) && SendMessageW(handle,EM_GETPASSWORDCHAR,0,0));
		REQUIRE(properties->SetValueBool(index,TRUE) == S_FALSE);
		SendMessageW(handle,EM_GETSEL,reinterpret_cast<WPARAM>(&start),reinterpret_cast<LPARAM>(&end));
		REQUIRE(start == 1 && end == 4 && SendMessageW(handle,EM_GETMODIFY,0,0) && SendMessageW(handle,EM_CANUNDO,0,0));
		REQUIRE(GetWindowTextW(handle,native,ARRAYSIZE(native)) == 6 && !lstrcmpW(native,L"sXXret") && binder->changed == 1);
		Check(properties->SetValueBool(index,FALSE));
		REQUIRE(!(GetWindowLongW(handle,GWL_STYLE) & ES_PASSWORD) && !SendMessageW(handle,EM_GETPASSWORDCHAR,0,0) && binder->changed == 1);
	}
	REQUIRE(properties->SetValueInt(index,1) == DISP_E_TYPEMISMATCH);
	REQUIRE(SendMessageW(handle,EM_UNDO,0,0));
	REQUIRE(GetWindowTextW(handle,native,ARRAYSIZE(native)) == 6 && !lstrcmpW(native,L"secret") && binder->changed == 2);
	{
		TStackRef<IQuadooProperties> textProperties;
		Check(Property(edit,L"Text",&textProperties,&textIndex));
		Check(textProperties->GetValueString(textIndex,&text));
		REQUIRE(text == RSTRING_CAST(L"secret"));
	}
	Check(properties->SetValueBool(index,TRUE));
	Check(QuadooGetFormSerializer(&serializer));
	Check(serializer->Serialize(form,&saved));
	Check(QuadooCreateControl(context,NULL,saved,&copy));
	Check(serializer->Serialize(copy,&again));
	REQUIRE(!JSONCompareObjects(saved,again));
	{
		TStackRef<IQuadooControlContainer> other;
		TStackRef<IQuadooControl> control;
		TStackRef<IQuadooProperties> copied;
		TStackRef<IQuadooWinControl> otherWindow;
		UINT copiedIndex;
		Check(copy->QueryInterface(&other));
		Check(other->FindControl(RSTRING_CAST(L"Plain"),&control));
		Check(Property(control,L"Password",&copied,&copiedIndex));
		Check(copied->GetValueBool(copiedIndex,&password));
		Check(control->QueryInterface(&otherWindow));
		Check(otherWindow->GetHandle(&same));
		REQUIRE(password && (GetWindowLongW(same,GWL_STYLE) & ES_PASSWORD) && SendMessageW(same,EM_GETPASSWORDCHAR,0,0));
	}
	Check(copy->Destroy());
	copy.Release();
	saved.Release();
	again.Release();
	Check(properties->SetValueBool(index,FALSE));
	Check(serializer->Serialize(form,&saved));
	Check(QuadooCreateControl(context,NULL,saved,&copy));
	Check(serializer->Serialize(copy,&again));
	REQUIRE(!JSONCompareObjects(saved,again));
	Check(copy->Destroy());
	Check(form->Destroy());
	REQUIRE(!IsWindow(handle) && properties->SetValueBool(index,FALSE) == QuadooForm::EDestroyed);
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
