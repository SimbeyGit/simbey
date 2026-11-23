static HRESULT DesignerProgressBars()
{
	HRESULT hr;
	HWND host = Host();
	TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IQuadooControlContainer> root;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooProgressBar> progress;
	TStackRef<IJSONObject> saved;
	TStackRef<IJSONObject> again;
	TStackRef<IJSONValue> value;
	GUID id;
	UINT index;
	DWORD flags;
	INT position;
	QuadooForm::Rect bounds = {10,10,150,20};
	CheckIf(!host,E_FAIL);
	Check(CreateEditor(host,&editor));
	Check(editor->NewDocument(FALSE));
	HWND tools = GetDlgItem(editor->Window(),102);
	INT row = FindTool(tools,L"ProgressBar");
	REQUIRE(row > 0 && PaletteRowImage(tools,row) >= 4);
	Check(editor->Document().Root()->QueryInterface(&root));
	Check(editor->AddControl(RSTRING_CAST(L"ProgressBar"),root,&bounds,&id));
	Check(editor->Document().Find(id,&control));
	Check(control->QueryInterface(&progress));
	REQUIRE(InspectorRow(editor,L"Min") >= 0 && InspectorRow(editor,L"Max") >= 0 && InspectorRow(editor,L"Position") >= 0 && InspectorRow(editor,L"Step") >= 0);
	{
		QFD::CSelectionProperties properties;
		Check(properties.Initialize(&editor->Document(),&id,1));
		Check(properties.Find(RSTRING_CAST(L"Position"),&index,&flags));
		Check(JSONCreateInteger(65,&value));
		Check(editor->SetSelectedProperty(index,value));
	}
	Check(progress->GetPosition(&position));
	REQUIRE(position == 65);
	Check(editor->Document().Serialize(&saved));
	Check(editor->LoadDefinition(saved));
	Check(editor->Document().Serialize(&again));
	REQUIRE(!JSONCompareObjects(saved,again));
	control.Release();
	progress.Release();
	Check(editor->Document().Find(id,&control));
	Check(control->QueryInterface(&progress));
	Check(progress->GetPosition(&position));
	REQUIRE(position == 65);
	hr = S_OK;
Cleanup:
	editor.Release();
	if(host)
	{
		DestroyWindow(host);
	}
	return hr;
}
