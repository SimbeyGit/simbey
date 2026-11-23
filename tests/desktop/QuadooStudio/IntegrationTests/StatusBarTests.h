static HRESULT StudioStatusBars(PCWSTR scratch)
{
	HRESULT hr;
	TStackRef<CQuadooStudio> studio;
	TStackRef<IJSONObject> project;
	TStackRef<IQuadooControlContainer> root;
	TStackRef<IQuadooComponent> component;
	TStackRef<IQuadooStatusPanel> panel;
	TStackRef<IJSONValue> value;
	TStackRef<ITextDocument> document;
	CRString folder;
	CRString path;
	CRString packages;
	CRString source;
	CRString text;
	HWND window = NULL;
	GUID id;
	GUID panelID;
	CProjectFile* file;
	QuadooForm::Rect bounds = {0,0,200,22};
	Check(RStrFormatW(&folder,L"%ls\\StatusBars",scratch));
	CheckIf(!CreateDirectoryW(folder.c_wstr(),NULL),HrEnsureLastError());
	Check(RStrFormatW(&path,L"%r\\StatusBars.qsproj",static_cast<RSTRING>(folder)));
	Check(RStrFormatW(&packages,L"%r\\Packages",static_cast<RSTRING>(folder)));
	Check(JSONCreateObject(&project));
	Check(QFD::PutString(project,L"target",RSTRING_CAST(L"application")));
	Check(CreateApplicationFixture(project,path.c_wstr()));
	studio.Attach(__new CQuadooStudio(GetModuleHandleW(NULL)));
	CheckAlloc(studio);
	Check(studio->Initialize(L"",1200,800,SW_SHOWNOACTIVATE,packages.c_wstr()));
	Check(studio->GetWindow(&window));
	Check(studio->OpenProject(path.c_wstr()));
	Pump();
	CQuadooProject* ide = studio->Project();
	QFD::CDesignerEditor* editor = ide->ActiveDesigner();
	REQUIRE(editor != NULL);
	Check(editor->Document().Root()->QueryInterface(&root));
	Check(editor->AddControl(RSTRING_CAST(L"StatusBar"),root,&bounds,&id));
	REQUIRE(Enabled(ide,QFD::AddStatusPanel));
	Check(editor->ExecuteCommand(QFD::AddStatusPanel,window));
	Check(editor->GetSelectedID(&panelID));
	Check(editor->Document().FindComponent(panelID,&component));
	Check(component->QueryInterface(&panel));
	Check(JSONCreateString(RSTRING_CAST(L"Studio ready"),&value));
	Check(SetProperty(editor,component,L"Text",value));
	Pump();
	BOOL found = FALSE;
	for(UINT n = 0; n < editor->OutlineCount(); n++)
	{
		QFD::OutlineEntry entry;
		Check(editor->GetOutlineEntry(n,&entry));
		if(IsEqualGUID(entry.id,panelID))
		{
			REQUIRE(!IsEqualGUID(entry.parent,id));
			found = TRUE;
		}
	}
	REQUIRE(found);
	Check(ide->FindSource(RSTRING_CAST(L".\\MainForm.quadoo"),&file));
	Check(ide->SourceDocument(file,&document));
	Check(StudioSource::ReadDocument(document,&source));
	REQUIRE(wcsstr(source.c_wstr(),L"property var StatusBar1;") != NULL);
	REQUIRE(wcsstr(source.c_wstr(),L"property var StatusPanel") == NULL);
	REQUIRE(wcsstr(source.c_wstr(),L"property var Panels") == NULL);
	Check(studio->Exec(NULL,ID_FILE_SAVEALL,0,NULL,NULL));
	Check(studio->Exec(NULL,ID_FILE_CLOSEPROJECT,0,NULL,NULL));
	Check(studio->OpenProject(path.c_wstr()));
	Pump();
	editor = studio->Project()->ActiveDesigner();
	Check(editor->SelectID(panelID));
	component.Release();
	panel.Release();
	Check(editor->Document().FindComponent(panelID,&component));
	Check(component->QueryInterface(&panel));
	Check(panel->GetText(&text));
	REQUIRE(text == RSTRING_CAST(L"Studio ready"));
	Check(editor->ExecuteCommand(QFD::DeleteSelection,window));
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
