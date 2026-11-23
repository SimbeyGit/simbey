static HRESULT DesignerImageButtons()
{
	HRESULT hr;
	HWND host = Host();
	TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IQuadooControlContainer> root;
	TStackRef<IQuadooControl> control;
	TStackRef<IJSONObject> saved;
	TStackRef<IJSONObject> again;
	TStackRef<IJSONValue> picture;
	TStackRef<IJSONValue> value;
	GUID imageID;
	GUID buttonID;
	UINT index;
	DWORD flags;
	QuadooForm::Rect bounds = {10,10,120,60};
	QFD::CSelectionProperties selection;
	GUID ids[2];
	CheckIf(!host,E_FAIL);
	Check(CreateEditor(host,&editor));
	Check(editor->NewDocument(FALSE));
	Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root)));
	Check(editor->AddControl(RSTRING_CAST(L"ImageButton"),root,&bounds,&imageID));
	Check(editor->Document().Find(imageID,&control));
	REQUIRE(InspectorRow(editor,L"Picture") >= 0 && InspectorRow(editor,L"DisabledPicture") >= 0 && InspectorRow(editor,L"ImagePosition") >= 0);
	Check(ImageFixtureValue(L"png",ImageRGBA,sizeof(ImageRGBA),&picture));
	{
		QFD::CSelectionProperties properties;
		Check(properties.Initialize(&editor->Document(),&imageID,1));
		Check(properties.Find(RSTRING_CAST(L"Picture"),&index,&flags));
		Check(editor->SetSelectedProperty(index,picture));
		Check(properties.Find(RSTRING_CAST(L"DisabledPicture"),&index,&flags));
		Check(editor->SetSelectedProperty(index,picture));
		Check(properties.Find(RSTRING_CAST(L"ImagePosition"),&index,&flags));
		Check(JSONCreateString(RSTRING_CAST(L"Top"),&value));
		Check(editor->SetSelectedProperty(index,value));
		value.Release();
	}
	Check(editor->Document().Serialize(&saved));
	Check(editor->LoadDefinition(saved));
	Check(editor->Document().Serialize(&again));
	REQUIRE(!JSONCompareObjects(saved,again));
	control.Release();
	Check(editor->Document().Find(imageID,&control));
	Check(QFD::ReadProperty(control,L"Picture",&value));
	REQUIRE(!JSONCompare(value,picture));
	value.Release();
	{
		TStackRef<IJSONValue> copied;
		GUID pasted;
		Check(editor->Document().CopySelectionDefinition(imageID,&copied));
		Check(editor->Document().PasteDefinition(imageID,copied,&pasted));
		REQUIRE(!IsEqualGUID(imageID,pasted));
		control.Release();
		Check(editor->Document().Find(pasted,&control));
		Check(QFD::ReadProperty(control,L"Picture",&value));
		REQUIRE(!JSONCompare(value,picture));
		value.Release();
		Check(QFD::ReadProperty(control,L"DisabledPicture",&value));
		REQUIRE(!JSONCompare(value,picture));
		value.Release();
		Check(editor->SelectID(pasted));
		Check(editor->Delete());
	}
	root.Release();
	Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root)));
	Check(editor->AddControl(RSTRING_CAST(L"Button"),root,&bounds,&buttonID));
	Check(editor->SelectID(imageID));
	Check(editor->SelectID(buttonID,TRUE));
	ids[0] = imageID;
	ids[1] = buttonID;
	Check(selection.Initialize(&editor->Document(),ids,ARRAYSIZE(ids)));
	Check(selection.Find(RSTRING_CAST(L"Caption"),&index,&flags));
	REQUIRE(FAILED(selection.Find(RSTRING_CAST(L"Picture"),&index,&flags)));
	hr = S_OK;
Cleanup:
	editor.Release();
	if(host)
	{
		DestroyWindow(host);
	}
	return hr;
}
