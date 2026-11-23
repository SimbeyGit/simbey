static HRESULT StudioDateTimePickers(PCWSTR scratch)
{
	HRESULT hr;
	TStackRef<CQuadooStudio> studio;
	TStackRef<IJSONObject> project;
	TStackRef<IQuadooControlContainer> root;
	TStackRef<IQuadooComponent> component;
	TStackRef<IQuadooDateTimePicker> picker;
	TStackRef<IJSONValue> value;
	TStackRef<ITextDocument> document;
	CRString folder;
	CRString path;
	CRString packages;
	CRString source;
	CProjectFile* file;
	SYSTEMTIME time;
	GUID id;
	QuadooForm::Rect bounds = {12,18,170,24};
	Check(RStrFormatW(&folder,L"%ls\\DateTimePickers",scratch));
	CheckIf(!CreateDirectoryW(folder.c_wstr(),NULL),HrEnsureLastError());
	Check(RStrFormatW(&path,L"%r\\Pickers.qsproj",static_cast<RSTRING>(folder)));
	Check(RStrFormatW(&packages,L"%r\\Packages",static_cast<RSTRING>(folder)));
	Check(JSONCreateObject(&project));
	Check(QFD::PutString(project,L"target",RSTRING_CAST(L"application")));
	Check(CreateApplicationFixture(project,path.c_wstr()));
	studio.Attach(__new CQuadooStudio(GetModuleHandleW(NULL)));
	CheckAlloc(studio);
	Check(studio->Initialize(L"",1200,800,SW_SHOWNOACTIVATE,packages.c_wstr()));
	Check(studio->OpenProject(path.c_wstr()));
	Pump();
	CQuadooProject* ide = studio->Project();
	QFD::CDesignerEditor* editor = ide->ActiveDesigner();
	REQUIRE(editor != NULL);
	Check(editor->Document().Root()->QueryInterface(&root));
	{
		HWND tools = editor->ToolsWindow();
		BOOL found = FALSE;
		for(INT n = 0; n < ListView_GetItemCount(tools); n++)
		{
			WCHAR text[64];
			ListView_GetItemText(tools,n,0,text,ARRAYSIZE(text));
			found |= !lstrcmpW(text,L"DateTimePicker");
		}
		REQUIRE(found);
	}
	Check(editor->AddControl(RSTRING_CAST(L"DateTimePicker"),root,&bounds,&id));
	Check(editor->Document().FindComponent(id,&component));
	Check(JSONCreateString(RSTRING_CAST(L"2032-06-07T08:09:10.011"),&value));
	Check(SetProperty(editor,component,L"DateTime",value));
	Check(ide->FindSource(RSTRING_CAST(L".\\MainForm.quadoo"),&file));
	Check(ide->SourceDocument(file,&document));
	Check(StudioSource::ReadDocument(document,&source));
	REQUIRE(wcsstr(source.c_wstr(),L"property var DateTimePicker1;") != NULL);
	Check(studio->Exec(NULL,ID_FILE_SAVEALL,0,NULL,NULL));
	Check(studio->Exec(NULL,ID_FILE_CLOSEPROJECT,0,NULL,NULL));
	Check(studio->OpenProject(path.c_wstr()));
	Pump();
	editor = studio->Project()->ActiveDesigner();
	component.Release();
	Check(editor->Document().FindComponent(id,&component));
	Check(component->QueryInterface(&picker));
	Check(picker->GetDateTime(&time));
	REQUIRE(time.wYear == 2032 && time.wMonth == 6 && time.wDay == 7 && time.wHour == 8 && time.wMilliseconds == 11);
	hr = S_OK;
Cleanup:
	if(studio && studio->Project())
	{
		studio->Project()->CloseProject(FALSE);
	}
	if(studio)
	{
		studio->Destroy();
	}
	Pump();
	return hr;
}
