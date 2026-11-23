static HRESULT StudioImageButtons(PCWSTR scratch)
{
	HRESULT hr;
	TStackRef<CQuadooStudio> studio;
	TStackRef<IJSONObject> project;
	TStackRef<IQuadooControlContainer> root;
	TStackRef<IQuadooComponent> component;
	TStackRef<IJSONValue> value;
	TStackRef<ITextDocument> document;
	CRString folder;
	CRString path;
	CRString packages;
	CRString source;
	HWND window = NULL;
	GUID id;
	CProjectFile* file;
	QuadooForm::Rect bounds = {10,10,120,60};
	Check(RStrFormatW(&folder,L"%ls\\ImageButtons",scratch));
	CheckIf(!CreateDirectoryW(folder.c_wstr(),NULL),HrEnsureLastError());
	Check(RStrFormatW(&path,L"%r\\ImageButtons.qsproj",static_cast<RSTRING>(folder)));
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
	Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root)));
	Check(editor->AddControl(RSTRING_CAST(L"ImageButton"),root,&bounds,&id));
	Check(editor->Document().FindComponent(id,&component));
	Check(JSONCreateString(RSTRING_CAST(L"Top"),&value));
	Check(SetProperty(editor,component,L"ImagePosition",value));
	value.Release();
	Check(ide->FindSource(RSTRING_CAST(L".\\MainForm.quadoo"),&file));
	Check(ide->SourceDocument(file,&document));
	Check(StudioSource::ReadDocument(document,&source));
	REQUIRE(wcsstr(source.c_wstr(),L"property var ImageButton1;") != NULL);
	REQUIRE(!wcsstr(source.c_wstr(),L"property var Picture;") && !wcsstr(source.c_wstr(),L"property var DisabledPicture;"));
	Check(studio->Exec(NULL,ID_FILE_SAVEALL,0,NULL,NULL));
	Check(studio->Exec(NULL,ID_FILE_CLOSEPROJECT,0,NULL,NULL));
	Check(studio->OpenProject(path.c_wstr()));
	Pump();
	editor = studio->Project()->ActiveDesigner();
	component.Release();
	Check(editor->Document().FindComponent(id,&component));
	Check(QFD::ReadProperty(component,L"ImagePosition",&value));
	INT position;
	Check(value->GetInteger(&position));
	REQUIRE(position == 1);
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
