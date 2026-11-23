static HRESULT NudgeKey(QFD::CDesignerEditor* editor, HWND target, UINT key, BOOL control, BOOL shift, BOOL alt, BOOL expected)
{
	HRESULT hr = S_OK;
	BYTE saved[256], keyboard[256] = {0};
	MSG message = {0};
	BOOL restore = FALSE;
	CheckIf(!GetKeyboardState(saved),HrEnsureLastError());
	keyboard[VK_CONTROL] = control ? 0x80 : 0;
	keyboard[VK_SHIFT] = shift ? 0x80 : 0;
	keyboard[VK_MENU] = alt ? 0x80 : 0;
	CheckIf(!SetKeyboardState(keyboard),HrEnsureLastError());
	restore = TRUE;
	message.hwnd = target; message.message = WM_KEYDOWN; message.wParam = key; message.lParam = 1;
	REQUIRE(editor->TranslateEditorMessage(&message) == expected);
Cleanup:
	if(restore) SetKeyboardState(saved);
	return hr;
}
static HRESULT DesignerNudging()
{
	HRESULT hr;
	HWND host = Host(), field = NULL;
	TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IJSONObject> definition, saved;
	TStackRef<IQuadooControlContainer> root;
	TStackRef<IQuadooControl> button, label, panel, docked, tabs, page;
	TStackRef<IJSONValue> value;
	GUID id, selected, timer;
	QuadooForm::Rect initial, bounds, parentBounds;
	IQuadooControl* immovable[3];
	WCHAR inspector[32];
	RECT rectangle;
	POINT point = {0,0};
	CheckIf(!host,HrEnsureLastError());
	Check(CreateEditor(host,&editor));
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_LEFT,TRUE,FALSE,FALSE,TRUE));
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"Nudging\",\"properties\":{\"Width\":500,\"Height\":350},\"children\":[{\"type\":\"Panel\",\"name\":\"panel\",\"properties\":{\"Left\":35,\"Top\":50,\"Width\":200,\"Height\":180},\"children\":[{\"type\":\"Button\",\"name\":\"button\",\"properties\":{\"Left\":13,\"Top\":19,\"Width\":80,\"Height\":26}},{\"type\":\"Label\",\"name\":\"label\",\"properties\":{\"Left\":5,\"Top\":65,\"Caption\":\"Label\"}}]},{\"type\":\"Panel\",\"name\":\"docked\",\"properties\":{\"Align\":\"Bottom\",\"Height\":25}},{\"type\":\"TabControl\",\"name\":\"tabs\",\"properties\":{\"Left\":250,\"Top\":20,\"Width\":200,\"Height\":200},\"children\":[{\"type\":\"TabPage\",\"name\":\"page\"}]}]}",&definition));
	Check(editor->LoadDefinition(definition));
	Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root)));
	Check(root->FindControl(RSTRING_CAST(L"button"),&button));
	Check(root->FindControl(RSTRING_CAST(L"label"),&label));
	Check(root->FindControl(RSTRING_CAST(L"panel"),&panel));
	Check(root->FindControl(RSTRING_CAST(L"docked"),&docked));
	Check(root->FindControl(RSTRING_CAST(L"tabs"),&tabs));
	Check(root->FindControl(RSTRING_CAST(L"page"),&page));
	Check(button->GetID(&id));
	Check(editor->SelectID(id));
	Check(button->GetBounds(&initial));
	Check(panel->GetBounds(&parentBounds));
	editor->SetSnap(TRUE);
	for(UINT dpi = 96; dpi <= 192; dpi += 48)
	{
		editor->SetDPI(dpi);
		for(UINT zoom = 50; zoom <= 200; zoom += 50)
		{
			editor->SetZoom(zoom);
			static const UINT keys[] = {VK_LEFT,VK_UP,VK_RIGHT,VK_DOWN};
			bounds = initial;
			for(UINT n = 0; n < ARRAYSIZE(keys); n++)
			{
				editor->Document().SetDirty(FALSE);
				Check(NudgeKey(editor,editor->OverlayWindow(),keys[n],TRUE,FALSE,FALSE,TRUE));
				if(n == 0) bounds.Left--;
				else if(n == 1) bounds.Top--;
				else if(n == 2) bounds.Left++;
				else bounds.Top++;
				QuadooForm::Rect actual;
				Check(button->GetBounds(&actual));
				REQUIRE(EqualBounds(bounds,actual) && editor->Document().Dirty());
				Check(editor->GetSelectedID(&selected));
				REQUIRE(IsEqualGUID(id,selected));
			}
		}
	}
	Check(panel->GetBounds(&bounds));
	REQUIRE(EqualBounds(bounds,parentBounds));
	editor->SetDPI(96); editor->SetZoom(100);
	// Unmodified arrows and other keyboard contexts keep their native behavior.
	editor->Document().SetDirty(FALSE);
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_LEFT,FALSE,FALSE,FALSE,FALSE));
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_RIGHT,TRUE,TRUE,FALSE,FALSE));
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_DOWN,TRUE,FALSE,TRUE,FALSE));
	Check(NudgeKey(editor,editor->ToolsWindow(),VK_RIGHT,TRUE,FALSE,FALSE,FALSE));
	Check(NudgeKey(editor,GetDlgItem(editor->Window(),104),VK_RIGHT,TRUE,FALSE,FALSE,FALSE));
	field = StartInline(editor,L"Caption");
	REQUIRE(field != NULL);
	SetWindowTextW(field,L"two words");
	Check(NudgeKey(editor,field,VK_LEFT,TRUE,FALSE,FALSE,TRUE));
	REQUIRE(IsWindowVisible(field) && !editor->Document().Dirty());
	SendMessageW(field,WM_KEYDOWN,VK_ESCAPE,0);
	Check(button->GetBounds(&bounds));
	REQUIRE(EqualBounds(bounds,initial) && !editor->Document().Dirty());
	// A held arrow is delivered as repeated key messages, one design pixel each.
	for(UINT n = 0; n < 3; n++)
	{
		Check(NudgeKey(editor,editor->TreeWindow(),VK_RIGHT,TRUE,FALSE,FALSE,TRUE));
	}
	Check(button->GetBounds(&bounds));
	REQUIRE(bounds.Left == initial.Left + 3 && bounds.Top == initial.Top && bounds.Width == initial.Width && bounds.Height == initial.Height);
	ListView_GetItemText(GetDlgItem(editor->Window(),104),InspectorRow(editor,L"Left"),1,inspector,ARRAYSIZE(inspector));
	REQUIRE(!lstrcmpW(inspector,L"16"));
	Check(editor->Document().Serialize(&saved));
	Check(editor->LoadDefinition(saved));
	button.Release(); root.Release();
	Check(editor->Document().Find(id,&button));
	Check(button->GetBounds(&bounds));
	REQUIRE(bounds.Left == 16 && bounds.Top == 19);
	Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root)));
	label.Release(); docked.Release(); tabs.Release(); page.Release();
	Check(root->FindControl(RSTRING_CAST(L"label"),&label));
	Check(root->FindControl(RSTRING_CAST(L"docked"),&docked));
	Check(root->FindControl(RSTRING_CAST(L"tabs"),&tabs));
	Check(root->FindControl(RSTRING_CAST(L"page"),&page));
	Check(label->GetID(&selected));
	Check(editor->SelectID(selected));
	Check(label->GetBounds(&initial));
	Check(NudgeKey(editor,editor->CanvasWindow(),VK_DOWN,TRUE,FALSE,FALSE,TRUE));
	Check(label->GetBounds(&bounds));
	REQUIRE(bounds.Top == initial.Top + 1 && bounds.Left == initial.Left);
	immovable[0] = root; immovable[1] = docked; immovable[2] = page;
	for(UINT n = 0; n < ARRAYSIZE(immovable); n++)
	{
		Check(immovable[n]->GetID(&selected));
		Check(editor->SelectID(selected));
		Check(immovable[n]->GetBounds(&initial));
		editor->Document().SetDirty(FALSE);
		Check(NudgeKey(editor,editor->OverlayWindow(),VK_LEFT,TRUE,FALSE,FALSE,TRUE));
		Check(NudgeKey(editor,editor->OverlayWindow(),VK_DOWN,TRUE,FALSE,FALSE,TRUE));
		Check(immovable[n]->GetBounds(&bounds));
		REQUIRE(EqualBounds(bounds,initial) && !editor->Document().Dirty());
	}
	Check(editor->AddComponent(RSTRING_CAST(L"Timer"),&timer));
	editor->Document().SetDirty(FALSE);
	Check(NudgeKey(editor,editor->TreeWindow(),VK_LEFT,TRUE,FALSE,FALSE,TRUE));
	REQUIRE(!editor->Document().Dirty());
	// An in-progress mouse gesture owns the bounds until it ends or is canceled.
	Check(editor->SelectID(id));
	Check(button->GetBounds(&initial));
	Check(editor->SelectionRect(&rectangle));
	point.x = (rectangle.left + rectangle.right) / 2;
	point.y = (rectangle.top + rectangle.bottom) / 2;
	SendMessageW(editor->OverlayWindow(),WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(point.x,point.y));
	REQUIRE(GetCapture() == editor->OverlayWindow());
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_RIGHT,TRUE,FALSE,FALSE,TRUE));
	SendMessageW(editor->OverlayWindow(),WM_KEYDOWN,VK_ESCAPE,0);
	Check(button->GetBounds(&bounds));
	REQUIRE(EqualBounds(bounds,initial) && !editor->Document().Dirty());
	// Moving a container keeps its children in the same parent-relative positions.
	panel.Release();
	Check(root->FindControl(RSTRING_CAST(L"panel"),&panel));
	Check(panel->GetID(&selected));
	Check(editor->SelectID(selected));
	Check(panel->GetBounds(&parentBounds));
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_RIGHT,TRUE,FALSE,FALSE,TRUE));
	Check(panel->GetBounds(&bounds));
	REQUIRE(bounds.Left == parentBounds.Left + 1 && bounds.Top == parentBounds.Top);
	Check(button->GetBounds(&bounds));
	REQUIRE(EqualBounds(bounds,initial));
	// Zero is not a movement boundary; native controls may have negative bounds.
	Check(editor->SelectID(id));
	bounds = initial; bounds.Left = bounds.Top = 0;
	Check(editor->SetSelectionBounds(&bounds));
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_LEFT,TRUE,FALSE,FALSE,TRUE));
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_UP,TRUE,FALSE,FALSE,TRUE));
	Check(button->GetBounds(&bounds));
	REQUIRE(bounds.Left == -1 && bounds.Top == -1 && bounds.Width == initial.Width && bounds.Height == initial.Height);
	// Layout constraints can change effective width without changing the stored
	// width. Moving must not turn that into a resize or reject the stored width.
	Check(JSONCreateInteger(120,&value));
	Check(Edit(editor,button,L"MinWidth",value));
	value.Release();
	Check(button->GetBounds(&initial));
	Check(button->GetEffectiveBounds(&bounds));
	REQUIRE(initial.Width == 80 && bounds.Width == 120);
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_RIGHT,TRUE,FALSE,FALSE,TRUE));
	Check(button->GetBounds(&bounds));
	REQUIRE(bounds.Left == initial.Left + 1 && bounds.Width == initial.Width && bounds.Height == initial.Height);
	Check(JSONCreateInteger(0,&value));
	Check(Edit(editor,button,L"MinWidth",value));
	value.Release();
	Check(JSONCreateInteger(40,&value));
	Check(Edit(editor,button,L"MaxWidth",value));
	value.Release();
	Check(button->GetBounds(&initial));
	Check(button->GetEffectiveBounds(&bounds));
	REQUIRE(initial.Width == 80 && bounds.Width == 40);
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_UP,TRUE,FALSE,FALSE,TRUE));
	Check(button->GetBounds(&bounds));
	REQUIRE(bounds.Top == initial.Top - 1 && bounds.Width == initial.Width && bounds.Height == initial.Height);
Cleanup:
	if(host) DestroyWindow(host);
	return hr;
}
