static HRESULT DesignerDateTimePickers()
{
	HRESULT hr;
	HWND host = Host();
	TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IQuadooControlContainer> root;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooDateTimePicker> picker;
	TStackRef<IJSONObject> saved;
	TStackRef<IJSONObject> again;
	TStackRef<IJSONValue> value;
	QFD::CSelectionProperties properties;
	GUID id;
	UINT index;
	DWORD flags;
	SYSTEMTIME time;
	QuadooForm::Rect bounds = {12,18,170,24};
	CheckIf(!host,HrEnsureLastError());
	Check(CreateEditor(host,&editor));
	Check(editor->NewDocument(FALSE));
	HWND tools = editor->ToolsWindow();
	INT row = FindTool(tools,L"DateTimePicker");
	REQUIRE(row > 0 && PaletteRowImage(tools,row) >= 4);
	Check(editor->Document().Root()->QueryInterface(&root));
	Check(editor->AddControl(RSTRING_CAST(L"DateTimePicker"),root,&bounds,&id));
	Check(editor->Document().Find(id,&control));
	Check(control->QueryInterface(&picker));
	Check(picker->GetDateTime(&time));
	REQUIRE(time.wYear >= 1601 && time.wMonth >= 1 && time.wDay >= 1);
	REQUIRE(InspectorRow(editor,L"DateTime") >= 0 && InspectorRow(editor,L"Kind") >= 0 && InspectorRow(editor,L"ShowCheckBox") >= 0);
	{
		HWND field = StartInline(editor,L"DateTime");
		REQUIRE(field != NULL);
		SetWindowTextW(field,L"2031-04-05T06:07:08.009");
		SendMessageW(field,WM_KEYDOWN,VK_RETURN,0);
		Check(picker->GetDateTime(&time));
		REQUIRE(time.wYear == 2031 && time.wMonth == 4 && time.wDay == 5 && time.wHour == 6 && time.wMilliseconds == 9);
		editor->Document().SetDirty(FALSE);
		field = StartInline(editor,L"DateTime");
		REQUIRE(field != NULL);
		SetWindowTextW(field,L"2031-02-29");
		SendMessageW(field,WM_KEYDOWN,VK_RETURN,0);
		REQUIRE(IsWindowVisible(field) && !editor->Document().Dirty());
		SendMessageW(field,WM_KEYDOWN,VK_ESCAPE,0);
	}
	Check(properties.Initialize(&editor->Document(),&id,1));
	Check(properties.Find(RSTRING_CAST(L"Kind"),&index,&flags));
	Check(JSONCreateString(RSTRING_CAST(L"Time"),&value));
	Check(editor->SetSelectedProperty(index,value));
	value.Release();
	Check(properties.Find(RSTRING_CAST(L"OnChange"),&index,&flags));
	Check(JSONCreateString(RSTRING_CAST(L"Picker_Changed"),&value));
	Check(editor->SetSelectedProperty(index,value));
	editor->SetDPI(144);
	INT cx;
	INT cy;
	REQUIRE(ImageList_GetIconSize(ListView_GetImageList(tools,LVSIL_SMALL),&cx,&cy) && cx == 30 && cy == 30);
	Check(editor->Document().Serialize(&saved));
	Check(editor->LoadDefinition(saved));
	Check(editor->Document().Serialize(&again));
	REQUIRE(!JSONCompareObjects(saved,again));
	control.Release();
	picker.Release();
	Check(editor->Document().Find(id,&control));
	Check(control->QueryInterface(&picker));
	Check(picker->GetDateTime(&time));
	REQUIRE(time.wYear == 2031 && time.wDay == 5 && time.wMilliseconds == 9);
	hr = S_OK;
Cleanup:
	editor.Release();
	if(host)
	{
		DestroyWindow(host);
	}
	return hr;
}
