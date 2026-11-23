static HRESULT ResizeTestBounds(QFD::CDesignerEditor* editor, REFGUID id, QuadooForm::Rect* bounds)
{
	HRESULT hr;
	TStackRef<IQuadooControl> control;
	Check(editor->Document().Find(id,&control));
	Check(control->GetBounds(bounds));
Cleanup:
	return hr;
}

static HRESULT ResizeTestProperty(QFD::CDesignerEditor* editor, REFGUID id, PCWSTR name, INT number)
{
	HRESULT hr;
	TStackRef<IQuadooControl> control;
	TStackRef<IJSONValue> value;
	Check(editor->Document().Find(id,&control));
	Check(JSONCreateInteger(number,&value));
	Check(Edit(editor,control,name,value));
Cleanup:
	return hr;
}

static HRESULT DesignerResizing()
{
	HRESULT hr;
	HWND host = Host();
	TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONObject> before;
	TStackRef<IJSONObject> after;
	GUID a;
	GUID b;
	GUID holder;
	GUID combo;
	GUID label;
	GUID docked;
	GUID page;
	GUID root;
	GUID timer;
	GUID selected;
	GUID ids[2];
	QuadooForm::Rect initial;
	QuadooForm::Rect bounds;
	QuadooForm::Rect other;
	QuadooForm::Rect expected;
	WCHAR text[64];
	static const WCHAR source[] =
		L"{\"type\":\"Form\",\"name\":\"Resizing\",\"properties\":{\"Width\":560,\"Height\":400},\"children\":["
		L"{\"type\":\"Panel\",\"name\":\"holder\",\"properties\":{\"Left\":20,\"Top\":20,\"Width\":270,\"Height\":240},\"children\":["
		L"{\"type\":\"Button\",\"name\":\"a\",\"properties\":{\"Left\":13,\"Top\":19,\"Width\":80,\"Height\":26}},"
		L"{\"type\":\"Button\",\"name\":\"b\",\"properties\":{\"Left\":110,\"Top\":70,\"Width\":95,\"Height\":32}},"
		L"{\"type\":\"ComboBox\",\"name\":\"entry\",\"properties\":{\"Left\":8,\"Top\":110,\"Width\":140,\"Height\":24}},"
		L"{\"type\":\"Label\",\"name\":\"label\",\"properties\":{\"Left\":20,\"Top\":150,\"AutoSize\":true,\"Caption\":\"Auto label\"}}]},"
		L"{\"type\":\"Panel\",\"name\":\"docked\",\"properties\":{\"Align\":\"Bottom\",\"Height\":25}},"
		L"{\"type\":\"TabControl\",\"name\":\"tabs\",\"properties\":{\"Left\":310,\"Top\":20,\"Width\":180,\"Height\":200},"
		L"\"children\":[{\"type\":\"TabPage\",\"name\":\"page\"}]}]}";
	CheckIf(!host,HrEnsureLastError());
	Check(CreateEditor(host,&editor));
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_LEFT,FALSE,TRUE,FALSE,TRUE));
	Check(QFD::Parse(SLP(source),&definition));
	Check(editor->LoadDefinition(definition));
	Check(NamedID(editor,L"a",&a));
	Check(NamedID(editor,L"b",&b));
	Check(NamedID(editor,L"holder",&holder));
	Check(NamedID(editor,L"entry",&combo));
	Check(NamedID(editor,L"label",&label));
	Check(NamedID(editor,L"docked",&docked));
	Check(NamedID(editor,L"page",&page));
	Check(editor->Document().Root()->GetID(&root));
	Check(editor->SelectID(a));
	Check(ResizeTestBounds(editor,a,&initial));
	Check(ResizeTestBounds(editor,holder,&other));
	editor->SetSnap(TRUE);
	for(UINT dpi = 96; dpi <= 192; dpi += 48)
	{
		editor->SetDPI(dpi);
		for(UINT zoom = 50; zoom <= 200; zoom += 50)
		{
			editor->SetZoom(zoom);
			static const UINT keys[] = {VK_RIGHT,VK_DOWN,VK_LEFT,VK_UP};
			expected = initial;
			for(UINT n = 0; n < ARRAYSIZE(keys); n++)
			{
				editor->Document().SetDirty(FALSE);
				Check(NudgeKey(editor,editor->OverlayWindow(),keys[n],FALSE,TRUE,FALSE,TRUE));
				if(n == 0)
				{
					expected.Width++;
				}
				else if(n == 1)
				{
					expected.Height++;
				}
				else if(n == 2)
				{
					expected.Width--;
				}
				else
				{
					expected.Height--;
				}
				Check(ResizeTestBounds(editor,a,&bounds));
				REQUIRE(EqualBounds(bounds,expected) && editor->Document().Dirty());
				Check(editor->GetSelectedID(&selected));
				REQUIRE(IsEqualGUID(a,selected));
			}
		}
	}
	Check(ResizeTestBounds(editor,holder,&bounds));
	REQUIRE(EqualBounds(bounds,other));
	editor->SetDPI(96);
	editor->SetZoom(100);
	editor->Document().SetDirty(FALSE);
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_RIGHT,FALSE,FALSE,FALSE,FALSE));
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_RIGHT,TRUE,TRUE,FALSE,FALSE));
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_RIGHT,FALSE,TRUE,TRUE,FALSE));
	Check(NudgeKey(editor,editor->ToolsWindow(),VK_RIGHT,FALSE,TRUE,FALSE,FALSE));
	Check(NudgeKey(editor,GetDlgItem(editor->Window(),104),VK_RIGHT,FALSE,TRUE,FALSE,FALSE));
	{
		HWND field = StartInline(editor,L"Caption");
		REQUIRE(field != NULL);
		SetWindowTextW(field,L"Select this text");
		Check(NudgeKey(editor,field,VK_LEFT,FALSE,TRUE,FALSE,TRUE));
		REQUIRE(IsWindowVisible(field) && !editor->Document().Dirty());
		SendMessageW(field,WM_KEYDOWN,VK_ESCAPE,0);
	}
	Check(ResizeTestBounds(editor,a,&bounds));
	REQUIRE(EqualBounds(bounds,initial) && !editor->Document().Dirty());
	for(UINT n = 0; n < 3; n++)
	{
		Check(NudgeKey(editor,editor->TreeWindow(),VK_RIGHT,FALSE,TRUE,FALSE,TRUE));
	}
	Check(ResizeTestBounds(editor,a,&bounds));
	REQUIRE(bounds.Width == initial.Width + 3 && bounds.Height == initial.Height && bounds.Left == initial.Left && bounds.Top == initial.Top);
	ListView_GetItemText(GetDlgItem(editor->Window(),104),InspectorRow(editor,L"Width"),1,text,ARRAYSIZE(text));
	REQUIRE(!lstrcmpW(text,L"83"));
	{
		RECT rectangle;
		Check(editor->SelectionRect(&rectangle));
		POINT point = {(rectangle.left + rectangle.right) / 2,(rectangle.top + rectangle.bottom) / 2};
		SendMessageW(editor->OverlayWindow(),WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(point.x,point.y));
		REQUIRE(GetCapture() == editor->OverlayWindow());
		Check(NudgeKey(editor,editor->OverlayWindow(),VK_DOWN,FALSE,TRUE,FALSE,TRUE));
		SendMessageW(editor->OverlayWindow(),WM_KEYDOWN,VK_ESCAPE,0);
		Check(ResizeTestBounds(editor,a,&expected));
		REQUIRE(EqualBounds(bounds,expected));
	}
	// Multi-selection applies the same increment to each sibling's own size.
	Check(editor->SelectID(b,TRUE));
	Check(ResizeTestBounds(editor,a,&initial));
	Check(ResizeTestBounds(editor,b,&other));
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_RIGHT,FALSE,TRUE,FALSE,TRUE));
	Check(NudgeKey(editor,editor->TreeWindow(),VK_DOWN,FALSE,TRUE,FALSE,TRUE));
	Check(ResizeTestBounds(editor,a,&bounds));
	expected = initial;
	expected.Width++;
	expected.Height++;
	REQUIRE(EqualBounds(bounds,expected));
	Check(ResizeTestBounds(editor,b,&bounds));
	expected = other;
	expected.Width++;
	expected.Height++;
	REQUIRE(EqualBounds(bounds,expected) && editor->SelectionCount() == 2 && editor->IsSelected(a) && editor->IsSelected(b));
	Check(ResizeTestProperty(editor,b,L"MaxWidth",bounds.Width));
	Check(editor->SelectID(a));
	Check(editor->SelectID(b,TRUE));
	Check(editor->Document().Serialize(&before));
	editor->Document().SetDirty(FALSE);
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_RIGHT,FALSE,TRUE,FALSE,TRUE));
	Check(editor->Document().Serialize(&after));
	REQUIRE(!JSONCompareObjects(before,after) && !editor->Document().Dirty());
	before.Release();
	after.Release();
	Check(ResizeTestProperty(editor,b,L"MaxWidth",0));
	Check(editor->SelectID(a));
	Check(editor->SelectID(b,TRUE));
	ids[0] = a;
	ids[1] = b;
	{
		struct RejectResize : QFD::ISourceSite
		{
			virtual HRESULT PrepareDefinition(IJSONObject*, QFD::ISourceEdit** edit)
			{
				*edit = NULL;
				return E_ACCESSDENIED;
			}
			virtual HRESULT OpenHandler(REFGUID, RSTRING)
			{
				return E_NOTIMPL;
			}
		} reject;
		Check(editor->Document().Serialize(&before));
		editor->Document().SetDirty(FALSE);
		editor->Document().SetSourceSite(&reject);
		HRESULT rejected = editor->Document().ResizeControls(ids,2,1,0);
		editor->Document().SetSourceSite(NULL);
		REQUIRE(rejected == E_ACCESSDENIED);
		Check(editor->Document().Serialize(&after));
		REQUIRE(!JSONCompareObjects(before,after) && !editor->Document().Dirty());
		REQUIRE(editor->Document().ResizeControls(ids,2,INT_MAX,0) == E_INVALIDARG && !editor->Document().Dirty());
	}
	Check(editor->LoadDefinition(after));
	Check(ResizeTestBounds(editor,a,&bounds));
	REQUIRE(bounds.Width == initial.Width + 1 && bounds.Height == initial.Height + 1);
	Check(ResizeTestBounds(editor,b,&bounds));
	REQUIRE(bounds.Width == other.Width + 1 && bounds.Height == other.Height + 1);
	Check(editor->SelectID(a));
	Check(editor->SelectID(label,TRUE));
	editor->Document().SetDirty(FALSE);
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_RIGHT,FALSE,TRUE,FALSE,TRUE));
	REQUIRE(!editor->Document().Dirty());
	Check(editor->SelectID(combo));
	Check(ResizeTestBounds(editor,combo,&initial));
	editor->Document().SetDirty(FALSE);
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_DOWN,FALSE,TRUE,FALSE,TRUE));
	REQUIRE(!editor->Document().Dirty());
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_RIGHT,FALSE,TRUE,FALSE,TRUE));
	Check(ResizeTestBounds(editor,combo,&bounds));
	REQUIRE(bounds.Width == initial.Width + 1 && bounds.Height == initial.Height);
	Check(editor->SelectID(docked));
	Check(ResizeTestBounds(editor,docked,&initial));
	editor->Document().SetDirty(FALSE);
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_RIGHT,FALSE,TRUE,FALSE,TRUE));
	REQUIRE(!editor->Document().Dirty());
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_DOWN,FALSE,TRUE,FALSE,TRUE));
	Check(ResizeTestBounds(editor,docked,&bounds));
	REQUIRE(bounds.Height == initial.Height + 1 && bounds.Width == initial.Width);
	Check(editor->SelectID(page));
	editor->Document().SetDirty(FALSE);
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_RIGHT,FALSE,TRUE,FALSE,TRUE));
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_DOWN,FALSE,TRUE,FALSE,TRUE));
	REQUIRE(!editor->Document().Dirty());
	Check(editor->AddComponent(RSTRING_CAST(L"Timer"),&timer));
	editor->Document().SetDirty(FALSE);
	Check(NudgeKey(editor,editor->TreeWindow(),VK_RIGHT,FALSE,TRUE,FALSE,TRUE));
	REQUIRE(!editor->Document().Dirty());
	// Lower/upper limits are no-ops; negative dimensions are never stored.
	Check(editor->SelectID(a));
	Check(ResizeTestBounds(editor,a,&bounds));
	bounds.Width = 0;
	bounds.Height = 0;
	Check(editor->SetSelectionBounds(&bounds));
	editor->Document().SetDirty(FALSE);
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_LEFT,FALSE,TRUE,FALSE,TRUE));
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_UP,FALSE,TRUE,FALSE,TRUE));
	REQUIRE(!editor->Document().Dirty());
	Check(ResizeTestProperty(editor,a,L"MaxHeight",1));
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_DOWN,FALSE,TRUE,FALSE,TRUE));
	editor->Document().SetDirty(FALSE);
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_DOWN,FALSE,TRUE,FALSE,TRUE));
	REQUIRE(!editor->Document().Dirty());
	Check(ResizeTestProperty(editor,a,L"MaxHeight",0));
	Check(ResizeTestProperty(editor,a,L"MinHeight",20));
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_RIGHT,FALSE,TRUE,FALSE,TRUE));
	Check(ResizeTestBounds(editor,a,&bounds));
	REQUIRE(bounds.Width == 1 && bounds.Height == 1);
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_DOWN,FALSE,TRUE,FALSE,TRUE));
	Check(ResizeTestBounds(editor,a,&bounds));
	REQUIRE(bounds.Width == 1 && bounds.Height == 21);
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_UP,FALSE,TRUE,FALSE,TRUE));
	editor->Document().SetDirty(FALSE);
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_UP,FALSE,TRUE,FALSE,TRUE));
	REQUIRE(!editor->Document().Dirty());
	Check(ResizeTestProperty(editor,a,L"Width",30));
	Check(ResizeTestProperty(editor,a,L"MaxWidth",10));
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_LEFT,FALSE,TRUE,FALSE,TRUE));
	Check(ResizeTestBounds(editor,a,&bounds));
	REQUIRE(bounds.Width == 9 && bounds.Height == 20);
	Check(editor->SelectID(root));
	Check(ResizeTestBounds(editor,root,&initial));
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_RIGHT,FALSE,TRUE,FALSE,TRUE));
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_DOWN,FALSE,TRUE,FALSE,TRUE));
	Check(ResizeTestBounds(editor,root,&bounds));
	REQUIRE(bounds.Width == initial.Width + 1 && bounds.Height == initial.Height + 1 && bounds.Left == initial.Left && bounds.Top == initial.Top);
	Check(editor->NewDocument(FALSE));
	Check(editor->Document().Root()->GetID(&root));
	Check(ResizeTestBounds(editor,root,&initial));
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_RIGHT,FALSE,TRUE,FALSE,TRUE));
	Check(NudgeKey(editor,editor->OverlayWindow(),VK_DOWN,FALSE,TRUE,FALSE,TRUE));
	Check(ResizeTestBounds(editor,root,&bounds));
	REQUIRE(bounds.Width == initial.Width + 1 && bounds.Height == initial.Height + 1 && bounds.Left == initial.Left && bounds.Top == initial.Top);
Cleanup:
	if(host)
	{
		DestroyWindow(host);
	}
	return hr;
}
