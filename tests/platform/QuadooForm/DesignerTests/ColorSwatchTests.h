static HRESULT DesignerColorSwatches()
{
	HRESULT hr;
	HWND host = Host();
	TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IQuadooControlContainer> root;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooStaticControl> painted;
	TStackRef<IQuadooMessageHandler> messages;
	TStackRef<IQuadooProperties> properties;
	TStackRef<IQuadooPropertyDescriptors> descriptors;
	TStackRef<IQuadooPropertyDescriptor> descriptor;
	TStackRef<IJSONValue> value, read;
	TStackRef<IJSONObject> serialized;
	GUID id;
	UINT choices;
	WCHAR path[MAX_PATH] = {0};
	CImageTestCanvas canvas;
	CheckIf(!host,HrEnsureLastError());
	Check(CreateEditor(host,&editor));
	Check(editor->NewDocument(TRUE));
	Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root)));
	HWND tools = GetDlgItem(editor->Window(),102);
	INT row = FindTool(tools,L"ColorSwatch");
	REQUIRE(row > 0 && PaletteRowImage(tools,row) >= 4);
	Check(PickTool(editor,L"ColorSwatch"));
	{
		POINT point = {30,60};
		MapWindowPoints(editor->Document().Host(),editor->CanvasWindow(),&point,1);
		SendMessageW(editor->OverlayWindow(),WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(point.x,point.y));
		SendMessageW(editor->OverlayWindow(),WM_LBUTTONUP,0,MAKELPARAM(point.x,point.y));
	}
	Check(root->FindControl(RSTRING_CAST(L"ColorSwatch1"),&control));
	Check(control->GetID(&id));
	REQUIRE(FindTree(editor->TreeWindow(),TreeView_GetRoot(editor->TreeWindow()),L"ColorSwatch1 (ColorSwatch)") != NULL);
	Check(control->GetProperties(&properties));
	Check(properties->GetDescriptors(&descriptors));
	Check(descriptors->Find(RSTRING_CAST(L"Mode"),&descriptor));
	Check(descriptor->GetEnumCount(&choices));
	REQUIRE(choices == 2);
	Check(DrivePropertyDialog(editor,host,L"Colors",L"[255,65280,16711680]",TRUE,FALSE));
	Check(DrivePropertyDialog(editor,host,L"Labels",L"[\"Red\",\"Green\",\"Blue\"]",TRUE,FALSE));
	Check(ChooseProperty(editor,L"Mode",2,0));
	Check(JSONCreateInteger(1,&value));
	Check(Edit(editor,control,L"Selected",value));
	value.Release();
	Check(JSONCreateString(RSTRING_CAST(L"ColorSwatch1_Click"),&value));
	Check(Edit(editor,control,L"OnClick",value));
	value.Release();
	InspectorEvents(editor,TRUE);
	{
		HWND field = StartInline(editor,L"OnClick");
		REQUIRE(field != NULL);
		SetWindowTextW(field,L"SwatchClick");
		SendMessageW(field,WM_KEYDOWN,VK_RETURN,0);
	}
	InspectorEvents(editor,FALSE);
	Check(ImageRead(control,L"OnClick",&read));
	{
		CRString text;
		Check(read->GetString(&text));
		REQUIRE(text == RSTRING_CAST(L"SwatchClick"));
	}
	Check(control->QueryInterface(__uuidof(IQuadooStaticControl),reinterpret_cast<PVOID*>(&painted)));
	Check(canvas.Create(160,60));
	{
		RECT bounds = {0,0,150,50};
		canvas.Fill(RGB(99,88,77));
		Check(painted->Paint(canvas.DC(),&bounds));
		REQUIRE(canvas.Pixel(75,5) == RGB(0,255,0) && canvas.Pixel(25,5) == RGB(255,0,0));
	}
	Check(editor->Document().Serialize(&serialized));
	Check(NewTemporary(path,ARRAYSIZE(path)));
	Check(editor->SaveDocument(path));
	properties.Release(); descriptors.Release(); descriptor.Release(); painted.Release(); control.Release(); root.Release();
	Check(editor->OpenDocument(path));
	Check(editor->Document().Find(id,&control));
	REQUIRE(ImageInteger(control,L"Selected") == 1);
	read.Release();
	Check(ImageRead(control,L"Colors",&read));
	Check(JSONParse(NULL,L"[255,65280,16711680]",TStrLenAssert(L"[255,65280,16711680]"),&value));
	REQUIRE(!JSONCompare(read,value));
	value.Release(); read.Release();
	Check(editor->SelectID(id));
	Check(DrivePropertyDialog(editor,host,L"Colors",L"[0]",FALSE,FALSE));
	REQUIRE(ImageInteger(control,L"Selected") == 1);
	Check(DrivePropertyDialog(editor,host,L"Colors",L"[0]",TRUE,FALSE));
	REQUIRE(ImageInteger(control,L"Selected") == -1);
	Check(ChooseProperty(editor,L"Mode",2,1));
	// Even a direct message to a Design-mode static control must not open a picker.
	Check(control->QueryInterface(__uuidof(IQuadooMessageHandler),reinterpret_cast<PVOID*>(&messages)));
	BOOL handled;
	LRESULT result;
	REQUIRE(messages->HandleMessage(editor->Document().Host(),WM_LBUTTONUP,0,MAKELPARAM(35,65),&handled,&result) == S_FALSE && !handled);
	REQUIRE(!editor->TrayWindow() || ListView_GetItemCount(editor->TrayWindow()) == 0);
Cleanup:
	if(host) DestroyWindow(host);
	if(path[0]) DeleteFileW(path);
	return hr;
}
