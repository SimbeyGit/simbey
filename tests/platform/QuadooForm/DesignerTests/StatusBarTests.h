static HRESULT DesignerStatusBars()
{
	HRESULT hr;
	HWND host = Host();
	TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IQuadooControlContainer> root;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooStatusBar> bar;
	TStackRef<IQuadooStatusPanels> panels;
	TStackRef<IQuadooStatusPanel> panel;
	TStackRef<IQuadooComponent> entry;
	TStackRef<IQuadooComponentOutline> outline;
	TStackRef<IJSONObject> saved;
	TStackRef<IJSONObject> again;
	TStackRef<IJSONValue> value;
	CRString text;
	QuadooForm::Rect bounds = {30,30,120,22};
	GUID id;
	GUID panelID;
	GUID groupID;
	UINT count;
	UINT index;
	DWORD flags;
	CheckIf(!host,E_FAIL);
	Check(CreateEditor(host,&editor));
	Check(editor->NewDocument(TRUE));
	INT row = FindTool(editor->ToolsWindow(),L"StatusBar");
	REQUIRE(row > 0 && PaletteRowImage(editor->ToolsWindow(),row) >= 4);
	Check(editor->Document().Root()->QueryInterface(&root));
	Check(editor->AddControl(RSTRING_CAST(L"StatusBar"),root,&bounds,&id));
	Check(editor->Document().Find(id,&control));
	Check(control->QueryInterface(&bar));
	Check(bar->GetPanels(&panels));
	REQUIRE(editor->CanExecute(QFD::AddStatusPanel));
	Check(editor->ExecuteCommand(QFD::AddStatusPanel,host));
	Check(panels->GetCount(&count));
	REQUIRE(count == 1);
	Check(editor->GetSelectedID(&panelID));
	Check(editor->Document().FindComponent(panelID,&entry));
	Check(entry->QueryInterface(&panel));
	REQUIRE(InspectorRow(editor,L"Text") >= 0 && InspectorRow(editor,L"Width") >= 0 && InspectorRow(editor,L"Icon") >= 0);
	REQUIRE(!editor->CanExecute(QFD::Copy) && !editor->CanExecute(QFD::Cut));
	REQUIRE(!editor->CanExecute(QFD::MoveStatusPanelLeft) && !editor->CanExecute(QFD::MoveStatusPanelRight));
	{
		HWND field = StartInline(editor,L"Text");
		REQUIRE(field && !(GetWindowLongW(field,GWL_STYLE) & ES_MULTILINE));
		SetWindowTextW(field,L"Inline ready");
		SendMessageW(field,WM_KEYDOWN,VK_RETURN,0);
		Check(panel->GetText(&text));
		REQUIRE(text == RSTRING_CAST(L"Inline ready"));
	}
	{
		QFD::CSelectionProperties properties;
		Check(properties.Initialize(&editor->Document(),&panelID,1));
		Check(properties.Find(RSTRING_CAST(L"Text"),&index,&flags));
		Check(JSONCreateString(RSTRING_CAST(L"Ready"),&value));
		Check(editor->SetSelectedProperty(index,value));
	}
	{
		INT width;
		HWND field = StartInline(editor,L"Width");
		REQUIRE(field && !(GetWindowLongW(field,GWL_STYLE) & ES_MULTILINE));
		SetWindowTextW(field,L"150");
		SendMessageW(field,WM_KEYDOWN,VK_RETURN,0);
		Check(panel->GetWidth(&width));
		REQUIRE(width == 150 && !IsWindowVisible(field));
		editor->Document().SetDirty(FALSE);
		field = StartInline(editor,L"Width");
		REQUIRE(field != NULL);
		SendMessageW(field,WM_KEYDOWN,VK_RETURN,0);
		REQUIRE(!editor->Document().Dirty());
		static PCWSTR const invalid[] = {L"-1",L"1000001",L"abc",L"123 extra",L"123 456",L"123]trailing",L"12.5",L"\"150\"",L"",L"null"};
		for(UINT n = 0; n < ARRAYSIZE(invalid); n++)
		{
			field = StartInline(editor,L"Width");
			REQUIRE(field != NULL);
			SetWindowTextW(field,invalid[n]);
			REQUIRE(FAILED(editor->CommitPropertyEdit()) && IsWindowVisible(field));
			Check(panel->GetWidth(&width));
			REQUIRE(width == 150 && !editor->Document().Dirty());
			SendMessageW(field,WM_KEYDOWN,VK_ESCAPE,0);
		}
		field = StartInline(editor,L"Width");
		REQUIRE(field != NULL);
		SetWindowTextW(field,L"200");
		SendMessageW(field,WM_KEYDOWN,VK_ESCAPE,0);
		Check(panel->GetWidth(&width));
		REQUIRE(width == 150 && !editor->Document().Dirty());
		Check(editor->SelectID(id));
		field = StartInline(editor,L"Height");
		REQUIRE(field != NULL);
		SetWindowTextW(field,L"28");
		SendMessageW(field,WM_KEYDOWN,VK_RETURN,0);
		REQUIRE(QFD::IntProperty(control,L"Height") == 28);
		REQUIRE(StartInline(editor,L"Width") == NULL);
		Check(editor->SelectID(panelID));
	}
	Check(entry->QueryInterface(&outline));
	Check(outline->GetOutlineParentID(&groupID));
	Check(editor->SelectID(groupID));
	REQUIRE(!editor->CanExecute(QFD::DeleteSelection));
	Check(editor->ExecuteCommand(QFD::AddStatusPanel,host));
	Check(panels->GetCount(&count));
	REQUIRE(count == 2);
	Check(editor->SelectID(panelID));
	Check(editor->ExecuteCommand(QFD::MoveStatusPanelRight,host));
	Check(panel->GetIndex(&index));
	REQUIRE(index == 1);
	Check(editor->ExecuteCommand(QFD::MoveStatusPanelLeft,host));
	Check(panel->GetIndex(&index));
	REQUIRE(index == 0);
	Check(editor->Document().Serialize(&saved));
	Check(editor->LoadDefinition(saved));
	Check(editor->Document().Serialize(&again));
	REQUIRE(!JSONCompareObjects(saved,again));
	Check(editor->SelectID(panelID));
	Check(editor->ExecuteCommand(QFD::DeleteSelection,host));
	entry.Release();
	REQUIRE(editor->Document().FindComponent(panelID,&entry) == HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
	Check(editor->SelectID(groupID));
	REQUIRE(editor->CanExecute(QFD::AddStatusPanel));
	Check(editor->SelectID(id));
	Check(editor->ExecuteCommand(QFD::DeleteSelection,host));
	REQUIRE(editor->Document().FindComponent(groupID,&entry) == HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
	hr = S_OK;
Cleanup:
	editor.Release();
	if(host)
	{
		DestroyWindow(host);
	}
	return hr;
}
