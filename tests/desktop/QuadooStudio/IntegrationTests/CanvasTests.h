static HRESULT StudioCanvases(PCWSTR scratch)
{
	HRESULT hr;
	TStackRef<CQuadooStudio> studio;
	TStackRef<IJSONObject> project;
	TStackRef<IQuadooControlContainer> root;
	TStackRef<IQuadooComponent> component;
	TStackRef<IJSONValue> value;
	TStackRef<ITextDocument> document;
	TStackRef<IQuadooCanvas> canvas;
	CRString folder;
	CRString path;
	CRString packages;
	CRString source;
	HWND window = NULL;
	GUID id;
	CProjectFile* file;
	SIZE size;
	QuadooForm::Rect bounds = {10,10,150,100};
	Check(RStrFormatW(&folder,L"%ls\\Canvases",scratch));
	CheckIf(!CreateDirectoryW(folder.c_wstr(),NULL),HrEnsureLastError());
	Check(RStrFormatW(&path,L"%r\\Canvases.qsproj",static_cast<RSTRING>(folder)));
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
	Check(editor->AddControl(RSTRING_CAST(L"Canvas"),root,&bounds,&id));
	Check(editor->Document().FindComponent(id,&component));
	Check(JSONCreateBool(true,&value));
	Check(SetProperty(editor,component,L"ClearOnResize",value));
	Check(ide->FindSource(RSTRING_CAST(L".\\MainForm.quadoo"),&file));
	Check(ide->SourceDocument(file,&document));
	Check(StudioSource::ReadDocument(document,&source));
	REQUIRE(wcsstr(source.c_wstr(),L"property var Canvas1;") != NULL);
	Check(studio->Exec(NULL,ID_FILE_SAVEALL,0,NULL,NULL));
	Check(studio->Exec(NULL,ID_FILE_CLOSEPROJECT,0,NULL,NULL));
	Check(studio->OpenProject(path.c_wstr()));
	Pump();
	editor = studio->Project()->ActiveDesigner();
	component.Release();
	Check(editor->Document().FindComponent(id,&component));
	Check(component->QueryInterface(&canvas));
	Check(canvas->MeasureText(RSTRING_CAST(L"Studio Canvas"),&size));
	REQUIRE(size.cx > 0 && size.cy > 0 && QFD::BoolProperty(component,L"ClearOnResize"));
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

static HRESULT StudioCanvasExample(PCWSTR scratch)
{
	HRESULT hr;
	WCHAR executablePath[MAX_PATH];
	CRString runtime;
	CRString sourceDirectory;
	CRString directory;
	CRString projectPath;
	CRString projectText;
	CRString executable;
	CRString diagnostic;
	TStackRef<IJSONObject> project;
	HINSTANCE image = NULL;
	CheckIf(!GetModuleFileNameW(NULL,executablePath,ARRAYSIZE(executablePath)),HrEnsureLastError());
	PCWSTR leaf = TStrRChr(executablePath,L'\\');
	CheckIf(!leaf,E_FAIL);
	Check(RStrCreateW(static_cast<INT>(leaf - executablePath),executablePath,&runtime));
	Check(RStrFormatW(&sourceDirectory,L"%r\\DesignerExamples\\CanvasApplication",static_cast<RSTRING>(runtime)));
	Check(RStrFormatW(&directory,L"%ls\\CanvasExample",scratch));
	CheckIf(!CreateDirectoryW(directory.c_wstr(),NULL),HrEnsureLastError());
	static PCWSTR const files[] = {L"CanvasApplication.qsproj",L"CanvasForm.quadoo",L"CanvasForm.qform.json",L"Sprite.png"};
	for(UINT n = 0; n < ARRAYSIZE(files); n++)
	{
		CRString from;
		CRString to;
		Check(RStrFormatW(&from,L"%r\\%ls",static_cast<RSTRING>(sourceDirectory),files[n]));
		Check(RStrFormatW(&to,L"%r\\%ls",static_cast<RSTRING>(directory),files[n]));
		CheckIf(!CopyFileW(from.c_wstr(),to.c_wstr(),TRUE),HrEnsureLastError());
	}
	Check(RStrFormatW(&projectPath,L"%r\\CanvasApplication.qsproj",static_cast<RSTRING>(directory)));
	Check(ReadText(projectPath.c_wstr(),projectText));
	Check(QFD::Parse(projectText.c_wstr(),RStrLen(projectText),&project));
	hr = StudioApplication::Build(project,projectPath,runtime,executable,diagnostic);
	if(FAILED(hr) && RStrLen(diagnostic))
	{
		Log(diagnostic.c_wstr());
	}
	Check(hr);
	REQUIRE(GetFileAttributesW(executable.c_wstr()) != INVALID_FILE_ATTRIBUTES);
	image = reinterpret_cast<HINSTANCE>(LoadLibraryExW(executable.c_wstr(),NULL,LOAD_LIBRARY_AS_DATAFILE));
	CheckIf(!image,HrEnsureLastError());
	{
		TStackRef<IJSONObject> catalog;
		TStackRef<IJSONObject> definition;
		CRString startup;
		Check(AQVM::ResourceObject(image,AQVM::CatalogType,MAKEINTRESOURCEW(AQVM::CatalogResource),&catalog));
		Check(AQVM::StringField(catalog,L"startupClass",&startup));
		REQUIRE(startup == RSTRING_CAST(L"CanvasForm"));
		Check(AQVM::ResourceObject(image,L"QFORM",L"CanvasForm",&definition));
		REQUIRE(definition->HasField(RSTRING_CAST(L"children")));
	}
	hr = S_OK;
Cleanup:
	if(image)
	{
		FreeLibrary(image);
	}
	return hr;
}
