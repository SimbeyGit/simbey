static HRESULT DesignerCalendars()
{
	HRESULT hr;
	HWND host = Host();
	TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IQuadooChildWindowHost> childHost;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooWinControl> window;
	TStackRef<IJSONObject> saved;
	TStackRef<IJSONObject> again;
	CRString type;
	GUID id;
	HWND parent;
	HWND handle;
	SYSTEMTIME date;
	POINT point = {18,22};
	INT cx;
	INT cy;
	CheckIf(!host,HrEnsureLastError());
	Check(CreateEditor(host,&editor));
	Check(editor->NewDocument(TRUE));
	editor->SetDPI(96);
	HWND tools = editor->ToolsWindow();
	INT row = FindTool(tools,L"Calendar");
	REQUIRE(row > 0 && PaletteRowImage(tools,row) >= 4);
	REQUIRE(ImageList_GetIconSize(ListView_GetImageList(tools,LVSIL_SMALL),&cx,&cy) && cx == 20 && cy == 20);
	Check(editor->Document().Root()->QueryInterface(&childHost));
	Check(childHost->GetChildHostHandle(&parent));
	MapWindowPoints(parent,editor->CanvasWindow(),&point,1);
	Check(PickTool(editor,L"Calendar"));
	SendMessageW(editor->OverlayWindow(),WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(point.x,point.y));
	SendMessageW(editor->OverlayWindow(),WM_LBUTTONUP,0,MAKELPARAM(point.x,point.y));
	Check(editor->GetSelectedID(&id));
	Check(editor->Document().Find(id,&control));
	Check(control->GetTypeName(&type));
	REQUIRE(type == RSTRING_CAST(L"Calendar"));
	Check(control->QueryInterface(&window));
	Check(window->GetHandle(&handle));
	REQUIRE(MonthCal_GetCurSel(handle,&date));
	{
		HWND field = StartInline(editor,L"Date");
		REQUIRE(field != NULL);
		SetWindowTextW(field,L"2032-02-29");
		SendMessageW(field,WM_KEYDOWN,VK_RETURN,0);
		REQUIRE(MonthCal_GetCurSel(handle,&date) && date.wYear == 2032 && date.wMonth == 2 && date.wDay == 29);
		editor->Document().SetDirty(FALSE);
		field = StartInline(editor,L"Date");
		REQUIRE(field != NULL);
		SetWindowTextW(field,L"2031-02-29");
		SendMessageW(field,WM_KEYDOWN,VK_RETURN,0);
		REQUIRE(IsWindowVisible(field) && !editor->Document().Dirty());
		SendMessageW(field,WM_KEYDOWN,VK_ESCAPE,0);
	}
	Check(ChooseProperty(editor,L"ShowWeekNumbers",2,1));
	Check(window->GetHandle(&handle));
	REQUIRE((GetWindowLongW(handle,GWL_STYLE) & MCS_WEEKNUMBERS) != 0);
	InspectorEvents(editor,TRUE);
	REQUIRE(InspectorRow(editor,L"OnChange") >= 0 && InspectorRow(editor,L"OnSelect") >= 0);
	editor->SetDPI(144);
	REQUIRE(ImageList_GetIconSize(ListView_GetImageList(tools,LVSIL_SMALL),&cx,&cy) && cx == 30 && cy == 30);
	Check(editor->Document().Serialize(&saved));
	Check(editor->LoadDefinition(saved));
	Check(editor->Document().Serialize(&again));
	REQUIRE(!JSONCompareObjects(saved,again));
	control.Release();
	window.Release();
	Check(editor->Document().Find(id,&control));
	Check(control->QueryInterface(&window));
	Check(window->GetHandle(&handle));
	REQUIRE(MonthCal_GetCurSel(handle,&date) && date.wYear == 2032 && date.wMonth == 2 && date.wDay == 29);
	hr = S_OK;
Cleanup:
	editor.Release();
	if(host)
	{
		DestroyWindow(host);
	}
	return hr;
}
