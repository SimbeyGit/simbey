static HRESULT DriveColorDialog(QFD::CDesignerEditor* editor, HWND host, COLORREF initial, BOOL accept)
{
	HRESULT hr;
	TStackRef<IQuadooComponent> component;
	TStackRef<IJSONValue> value;
	GUID id;
	INT raw;
	Check(editor->GetSelectedID(&id));
	Check(editor->Document().FindComponent(id,&component));
	Check(QFD::ReadProperty(component,L"BackColor",&value));
	Check(value->GetInteger(&raw));
	REQUIRE(ResolveQuadooColor(raw) == initial);
	Check(StartPicker(host,accept ? PickerCustom : PickerCustomCancel,raw));
	StartInline(editor,L"BackColor");
	Check(g_picker.error);
	REQUIRE(g_picker.seen && g_picker.standardSeen && g_picker.standardCorrect);
Cleanup:
	KillTimer(host,5430);
	return hr;
}
static HRESULT EditColor(QFD::CDesignerEditor* editor, IQuadooControl* control, PCWSTR name, INT color)
{
	HRESULT hr;
	TStackRef<IJSONValue> value;
	Check(JSONCreateInteger(color,&value));
	Check(Edit(editor,control,name,value));
Cleanup:
	return hr;
}
static HRESULT CheckColorSwatch(QFD::CDesignerEditor* editor, PCWSTR name, COLORREF expected, BOOL selected)
{
	HRESULT hr;
	HWND list = GetDlgItem(editor->Window(),104);
	INT row = InspectorRow(editor,name);
	RECT cell, client;
	CImageTestCanvas canvas;
	CheckIf(row < 0,E_FAIL);
	ListView_EnsureVisible(list,row,FALSE);
	ListView_SetItemState(list,-1,0,LVIS_SELECTED | LVIS_FOCUSED);
	if(selected) ListView_SetItemState(list,row,LVIS_SELECTED | LVIS_FOCUSED,LVIS_SELECTED | LVIS_FOCUSED);
	SetFocus(list);
	REQUIRE(ListView_GetSubItemRect(list,row,1,LVIR_BOUNDS,&cell));
	GetClientRect(list,&client);
	Check(canvas.Create(client.right,client.bottom));
	canvas.Fill(RGB(241,1,242));
	SendMessageW(list,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(canvas.DC()),PRF_CLIENT);
	REQUIRE(canvas.Pixel(cell.left + 8,(cell.top + cell.bottom) / 2) == expected);
Cleanup:
	return hr;
}
static HRESULT DesignerColors()
{
	HRESULT hr;
	HWND host = Host();
	TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IQuadooControlContainer> root, parent;
	TStackRef<IQuadooControl> container, child;
	TStackRef<IJSONObject> saved;
	GUID id;
	QuadooForm::Rect bounds = {10,10,200,140}, childBounds = {5,5,70,22};
	const INT system = static_cast<INT>(0x80000000 | COLOR_HIGHLIGHT);
	const INT rgb = RGB(19,71,139);
	CheckIf(!host,HrEnsureLastError());
	Check(CreateEditor(host,&editor));
	for(UINT frame = 0; frame < 2; frame++)
	{
		root.Release(); parent.Release(); container.Release(); child.Release();
		Check(editor->NewDocument(!frame));
		Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root)));
		Check(EditColor(editor,root,L"BackColor",rgb));
		Check(EditColor(editor,root,L"ForeColor",system));
		Check(editor->AddControl(RSTRING_CAST(L"TextBox"),root,&childBounds,&id));
		Check(editor->Document().Find(id,&child));
		REQUIRE(ImageInteger(child,L"BackColor") == rgb && ImageInteger(child,L"ForeColor") == system);
		static PCWSTR const containers[] = {L"Panel",L"GroupBox",L"ScrollingPanel",L"TabControl"};
		for(UINT n = 0; n < ARRAYSIZE(containers); n++)
		{
			container.Release(); parent.Release(); child.Release();
			Check(editor->AddControl(RSTRING_CAST(containers[n]),root,&bounds,&id));
			Check(editor->Document().Find(id,&container));
			REQUIRE(ImageInteger(container,L"BackColor") == rgb && ImageInteger(container,L"ForeColor") == system);
			Check(container->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&parent)));
			if(n == 3)
			{
				Check(editor->AddControl(RSTRING_CAST(L"TabPage"),parent,&bounds,&id));
				container.Release();
				Check(editor->Document().Find(id,&container));
				REQUIRE(ImageInteger(container,L"BackColor") == rgb && ImageInteger(container,L"ForeColor") == system);
				parent.Release();
				Check(container->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&parent)));
			}
			Check(EditColor(editor,parent,L"BackColor",system));
			Check(EditColor(editor,parent,L"ForeColor",rgb));
			Check(editor->AddControl(RSTRING_CAST(L"TextBox"),parent,&childBounds,&id));
			Check(editor->Document().Find(id,&child));
			REQUIRE(ImageInteger(child,L"BackColor") == system && ImageInteger(child,L"ForeColor") == rgb);
			Check(EditColor(editor,parent,L"BackColor",rgb));
			REQUIRE(ImageInteger(child,L"BackColor") == system);
			Check(child->GetID(&id));
			Check(editor->SelectID(id));
			Check(CheckColorSwatch(editor,L"BackColor",GetSysColor(COLOR_HIGHLIGHT),FALSE));
			Check(CheckColorSwatch(editor,L"BackColor",GetSysColor(COLOR_HIGHLIGHT),TRUE));
			Check(CheckColorSwatch(editor,L"ForeColor",rgb,FALSE));
		}
		Check(editor->Document().Serialize(&saved));
		Check(editor->LoadDefinition(saved));
		saved.Release(); child.Release();
		Check(editor->Document().Find(id,&child));
		REQUIRE(ImageInteger(child,L"BackColor") == system && ImageInteger(child,L"ForeColor") == rgb);
		Check(editor->SelectID(id));
		Check(CheckColorSwatch(editor,L"BackColor",GetSysColor(COLOR_HIGHLIGHT),FALSE));
		SendMessageW(editor->Window(),WM_SYSCOLORCHANGE,0,0);
		REQUIRE(ImageInteger(child,L"BackColor") == system);
		REQUIRE(!editor->Document().Dirty());
		Check(DriveColorDialog(editor,host,GetSysColor(COLOR_HIGHLIGHT),FALSE));
		REQUIRE(ImageInteger(child,L"BackColor") == system && !editor->Document().Dirty());
		Check(DriveColorDialog(editor,host,GetSysColor(COLOR_HIGHLIGHT),TRUE));
		REQUIRE(ImageInteger(child,L"BackColor") == RGB(41,82,123) && editor->Document().Dirty());
		Check(CheckColorSwatch(editor,L"BackColor",RGB(41,82,123),FALSE));
	}
	hr = S_OK;
Cleanup:
	editor.Release();
	if(host) { KillTimer(host,5430); DestroyWindow(host); }
	return hr;
}
