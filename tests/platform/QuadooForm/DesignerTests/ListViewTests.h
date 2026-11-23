static HRESULT DesignerListViewReferences()
{
	HRESULT hr;
	HWND host = Host();
	TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONObject> child;
	TStackRef<IJSONObject> serialized;
	TStackRef<IJSONObject> repeated;
	TStackRef<IJSONArray> children;
	TStackRef<IJSONValue> wrapped;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooControlContainer> root;
	TStackRef<IQuadooComponentHost> componentHost;
	TStackRef<IQuadooComponentCollection> components;
	TStackRef<IQuadooComponent> images;
	GUID id;
	GUID imageID;
	CheckIf(!host,E_FAIL);
	Check(CreateEditor(host,&editor));
	Check(Parse(ImageListNativeDefinition,&definition));
	Check(Parse(L"{\"type\":\"ListView\",\"name\":\"Results\",\"properties\":{\"Columns\":[{\"Caption\":\"Name\"},{\"Caption\":\"Details\"}],\"Items\":[{\"Caption\":\"row\",\"SubItems\":[\"detail\"]}]}}",&child));
	Check(JSONCreateArray(&children));
	Check(JSONWrapObject(child,&wrapped));
	Check(children->Add(wrapped));
	wrapped.Release();
	Check(JSONWrapArray(children,&wrapped));
	Check(definition->AddValueW(L"children",wrapped));
	Check(editor->LoadDefinition(definition));
	Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root)));
	Check(root->FindControl(RSTRING_CAST(L"Results"),&control));
	Check(control->GetID(&id));
	Check(editor->SelectID(id));
	REQUIRE(FindTool(editor->ToolsWindow(),L"ListView") >= 0);
	REQUIRE(InspectorRow(editor,L"Items") < 0 && InspectorRow(editor,L"Columns") >= 0);
	Check(ChooseProperty(editor,L"ViewStyle",4,0));
	Check(ChooseProperty(editor,L"ViewStyle",4,3));
	Check(root->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&componentHost)));
	Check(componentHost->GetComponents(&components));
	Check(components->FindComponent(RSTRING_CAST(L"ImageList1"),&images));
	Check(images->GetID(&imageID));
	static PCWSTR const names[] = {L"LargeImages",L"SmallImages",L"StateImages"};
	for(UINT n = 0; n < ARRAYSIZE(names); n++)
	{
		HWND combo;
		Check(ReferenceDropdown(editor,names[n],2,&combo));
		REQUIRE(SendMessageW(combo,CB_FINDSTRINGEXACT,static_cast<WPARAM>(-1),reinterpret_cast<LPARAM>(L"ImageList1")) == 1);
		SendMessageW(combo,CB_SETCURSEL,1,0);
		SendMessageW(combo,WM_KEYDOWN,VK_RETURN,0);
		control.Release();
		Check(editor->Document().Find(id,&control));
		Check(ReferenceAssignment(control,names[n],imageID));
	}
	Check(editor->Document().Serialize(&serialized));
	Check(editor->LoadDefinition(serialized));
	Check(editor->Document().Serialize(&repeated));
	REQUIRE(JSONCompareObjects(serialized,repeated) == 0);
	{
		TStackRef<IJSONValue> copied;
		TStackRef<IQuadooListView> view;
		TStackRef<IQuadooListItems> items;
		TStackRef<IQuadooListColumns> columns;
		TStackRef<IQuadooListItem> item;
		TStackRef<IQuadooListColumn> column;
		GUID rowID;
		GUID columnID;
		GUID pasted;
		CRString text;
		control.Release();
		Check(editor->Document().Find(id,&control));
		Check(control->QueryInterface(__uuidof(IQuadooListView),reinterpret_cast<PVOID*>(&view)));
		Check(view->GetItems(&items));
		Check(view->GetColumns(&columns));
		Check(items->GetAt(0,&item));
		Check(columns->GetAt(0,&column));
		Check(item->GetID(&rowID));
		Check(column->GetID(&columnID));
		Check(editor->Document().CopySelectionDefinition(id,&copied));
		Check(editor->Document().PasteDefinition(id,copied,&pasted));
		REQUIRE(!IsEqualGUID(pasted,id));
		control.Release();
		view.Release();
		items.Release();
		columns.Release();
		item.Release();
		column.Release();
		Check(editor->Document().Find(pasted,&control));
		Check(control->QueryInterface(__uuidof(IQuadooListView),reinterpret_cast<PVOID*>(&view)));
		Check(view->GetItems(&items));
		Check(view->GetColumns(&columns));
		Check(items->GetAt(0,&item));
		Check(columns->GetAt(0,&column));
		GUID candidate;
		Check(item->GetID(&candidate));
		REQUIRE(!IsEqualGUID(candidate,rowID));
		Check(column->GetID(&candidate));
		REQUIRE(!IsEqualGUID(candidate,columnID));
		Check(item->GetSubItem(0,&text));
		REQUIRE(text == RSTRING_CAST(L"detail"));
	}
	Cleanup:
	editor.Release();
	if(host)
	{
		DestroyWindow(host);
	}
	return hr;
}
