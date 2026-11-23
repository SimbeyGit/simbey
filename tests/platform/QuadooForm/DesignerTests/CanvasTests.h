static HRESULT DesignerCanvases()
{
	HRESULT hr;
	HWND host = Host();
	TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IQuadooControlContainer> root;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooCanvas> canvas;
	TStackRef<IJSONObject> saved;
	TStackRef<IJSONObject> again;
	GUID id;
	SIZE size;
	QuadooForm::Rect bounds = {10,12,200,100};
	CheckIf(!host,E_FAIL);
	Check(CreateEditor(host,&editor));
	for(UINT kind = 0; kind < 2; kind++)
	{
		Check(editor->NewDocument(kind == 0));
		INT row = FindTool(editor->ToolsWindow(),L"Canvas");
		REQUIRE(row > 0 && PaletteRowImage(editor->ToolsWindow(),row) >= 4);
		root.Release();
		control.Release();
		canvas.Release();
		Check(editor->Document().Root()->QueryInterface(&root));
		Check(editor->AddControl(RSTRING_CAST(L"Canvas"),root,&bounds,&id));
		Check(editor->Document().Find(id,&control));
		Check(control->QueryInterface(&canvas));
		REQUIRE(InspectorRow(editor,L"AutoRedraw") >= 0 && InspectorRow(editor,L"DoubleBuffered") >= 0 && InspectorRow(editor,L"ClearOnResize") >= 0);
		REQUIRE(InspectorRow(editor,L"Transparent") < 0);
		Check(ChooseProperty(editor,L"ClearOnResize",2,1));
		REQUIRE(QFD::BoolProperty(control,L"ClearOnResize"));
		HWND field = StartInline(editor,L"Width");
		REQUIRE(field != NULL);
		SetWindowTextW(field,L"210");
		SendMessageW(field,WM_KEYDOWN,VK_RETURN,0);
		REQUIRE(QFD::IntProperty(control,L"Width") == 210);
		InspectorEvents(editor,TRUE);
		field = StartInline(editor,L"OnPaint");
		REQUIRE(field != NULL);
		SetWindowTextW(field,L"CanvasPaint");
		SendMessageW(field,WM_KEYDOWN,VK_RETURN,0);
		InspectorEvents(editor,FALSE);
		saved.Release();
		again.Release();
		Check(editor->Document().Serialize(&saved));
		editor->Document().SetDirty(FALSE);
		Check(canvas->MeasureText(RSTRING_CAST(L"Measured only"),&size));
		REQUIRE(size.cx > 0 && size.cy > 0 && !editor->Document().Dirty());
		Check(canvas->Clear(RGB(10,20,30)));
		REQUIRE(!editor->Document().Dirty());
		Check(editor->Document().Serialize(&again));
		REQUIRE(!JSONCompareObjects(saved,again));
		Check(editor->LoadDefinition(saved));
		again.Release();
		Check(editor->Document().Serialize(&again));
		REQUIRE(!JSONCompareObjects(saved,again));
	}
	hr = S_OK;
Cleanup:
	editor.Release();
	if(host)
	{
		DestroyWindow(host);
	}
	return hr;
}
