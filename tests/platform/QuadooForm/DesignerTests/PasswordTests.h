static HRESULT DesignerPasswords()
{
	HRESULT hr;
	HWND host = Host();
	TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IQuadooControlContainer> root;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooWinControl> window;
	TStackRef<IJSONObject> saved;
	TStackRef<IJSONObject> again;
	GUID id;
	HWND handle;
	QuadooForm::Rect bounds = {12,18,160,24};
	CheckIf(!host,HrEnsureLastError());
	Check(CreateEditor(host,&editor));
	Check(editor->NewDocument(TRUE));
	Check(editor->Document().Root()->QueryInterface(&root));
	Check(editor->AddControl(RSTRING_CAST(L"TextBox"),root,&bounds,&id));
	Check(editor->Document().Find(id,&control));
	Check(control->QueryInterface(&window));
	Check(window->GetHandle(&handle));
	REQUIRE(InspectorRow(editor,L"Password") >= 0 && !(GetWindowLongW(handle,GWL_STYLE) & ES_PASSWORD));
	// The existing Boolean choice editor is shared with Quadoo Studio.
	Check(ChooseProperty(editor,L"Password",2,1));
	REQUIRE((GetWindowLongW(handle,GWL_STYLE) & ES_PASSWORD) && SendMessageW(handle,EM_GETPASSWORDCHAR,0,0));
	Check(ChooseProperty(editor,L"Password",2,0));
	REQUIRE(!(GetWindowLongW(handle,GWL_STYLE) & ES_PASSWORD) && !SendMessageW(handle,EM_GETPASSWORDCHAR,0,0));
	Check(ChooseProperty(editor,L"Password",2,1));
	Check(editor->Document().Serialize(&saved));
	Check(editor->LoadDefinition(saved));
	Check(editor->Document().Serialize(&again));
	REQUIRE(!JSONCompareObjects(saved,again));
	control.Release();
	window.Release();
	Check(editor->Document().Find(id,&control));
	Check(control->QueryInterface(&window));
	Check(window->GetHandle(&handle));
	REQUIRE((GetWindowLongW(handle,GWL_STYLE) & ES_PASSWORD) && SendMessageW(handle,EM_GETPASSWORDCHAR,0,0));
	hr = S_OK;
Cleanup:
	editor.Release();
	if(host)
	{
		DestroyWindow(host);
	}
	return hr;
}
