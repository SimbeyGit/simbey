static HRESULT StudioProgressBars(PCWSTR scratch)
{
	HRESULT hr;
	TStackRef<CQuadooStudio> studio;
	TStackRef<IJSONObject> project;
	TStackRef<IQuadooControlContainer> root;
	TStackRef<IQuadooComponent> component;
	TStackRef<IJSONValue> value;
	TStackRef<ITextDocument> document;
	TStackRef<IQuadooProgressBar> progress;
	CRString folder;
	CRString path;
	CRString packages;
	CRString source;
	HWND window = NULL;
	GUID id;
	CProjectFile* file;
	INT position;
	QuadooForm::Rect bounds = {10,10,150,20};
	Check(RStrFormatW(&folder,L"%ls\\ProgressBars",scratch));
	CheckIf(!CreateDirectoryW(folder.c_wstr(),NULL),HrEnsureLastError());
	Check(RStrFormatW(&path,L"%r\\ProgressBars.qsproj",static_cast<RSTRING>(folder)));
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
	Check(editor->AddControl(RSTRING_CAST(L"ProgressBar"),root,&bounds,&id));
	Check(editor->Document().FindComponent(id,&component));
	Check(JSONCreateInteger(70,&value));
	Check(SetProperty(editor,component,L"Position",value));
	Check(ide->FindSource(RSTRING_CAST(L".\\MainForm.quadoo"),&file));
	Check(ide->SourceDocument(file,&document));
	Check(StudioSource::ReadDocument(document,&source));
	REQUIRE(wcsstr(source.c_wstr(),L"property var ProgressBar1;") != NULL);
	Check(studio->Exec(NULL,ID_FILE_SAVEALL,0,NULL,NULL));
	Check(studio->Exec(NULL,ID_FILE_CLOSEPROJECT,0,NULL,NULL));
	Check(studio->OpenProject(path.c_wstr()));
	Pump();
	editor = studio->Project()->ActiveDesigner();
	component.Release();
	Check(editor->Document().FindComponent(id,&component));
	Check(component->QueryInterface(&progress));
	Check(progress->GetPosition(&position));
	REQUIRE(position == 70);
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
