static HRESULT StudioCalendars(PCWSTR scratch)
{
	HRESULT hr;
	TStackRef<CQuadooStudio> studio;
	TStackRef<IJSONObject> project;
	TStackRef<IQuadooControlContainer> root;
	TStackRef<IQuadooComponent> component;
	TStackRef<IQuadooWinControl> calendar;
	TStackRef<IJSONValue> value;
	TStackRef<ITextDocument> document;
	CRString folder;
	CRString path;
	CRString packages;
	CRString source;
	CProjectFile* file;
	SYSTEMTIME date;
	HWND window;
	GUID id;
	QuadooForm::Rect bounds = {12,18,260,205};
	Check(RStrFormatW(&folder,L"%ls\\Calendars",scratch));
	CheckIf(!CreateDirectoryW(folder.c_wstr(),NULL),HrEnsureLastError());
	Check(RStrFormatW(&path,L"%r\\Calendars.qsproj",static_cast<RSTRING>(folder)));
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
		LVFINDINFOW find = {LVFI_STRING,L"Calendar"};
		LVITEMW item = {0};
		item.mask = LVIF_IMAGE;
		item.iItem = ListView_FindItem(tools,-1,&find);
		REQUIRE(item.iItem > 0 && ListView_GetItem(tools,&item) && item.iImage >= 4);
	}
	Check(editor->AddControl(RSTRING_CAST(L"Calendar"),root,&bounds,&id));
	Check(editor->Document().FindComponent(id,&component));
	Check(JSONCreateString(RSTRING_CAST(L"2032-06-07"),&value));
	Check(SetProperty(editor,component,L"Date",value));
	Check(ide->FindSource(RSTRING_CAST(L".\\MainForm.quadoo"),&file));
	Check(ide->SourceDocument(file,&document));
	Check(StudioSource::ReadDocument(document,&source));
	REQUIRE(wcsstr(source.c_wstr(),L"property var Calendar1;") != NULL);
	Check(studio->Exec(NULL,ID_FILE_SAVEALL,0,NULL,NULL));
	Check(studio->Exec(NULL,ID_FILE_CLOSEPROJECT,0,NULL,NULL));
	Check(studio->OpenProject(path.c_wstr()));
	Pump();
	editor = studio->Project()->ActiveDesigner();
	REQUIRE(editor != NULL);
	component.Release();
	Check(editor->Document().FindComponent(id,&component));
	Check(component->QueryInterface(&calendar));
	Check(calendar->GetHandle(&window));
	REQUIRE(MonthCal_GetCurSel(window,&date) && date.wYear == 2032 && date.wMonth == 6 && date.wDay == 7);
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
