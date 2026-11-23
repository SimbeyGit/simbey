#include "..\..\..\..\platform\QuadooForm\Designer\ListColumnsEditor.h"

static BOOL s_studioColumnsSeen;
static UINT s_studioColumnsTicks;
static VOID CALLBACK StudioColumnsTimer(HWND owner, UINT, UINT_PTR timer, DWORD)
{
	HWND dialog = FindWindowW(L"#32770",L"ListView Columns Editor");
	if(!dialog && ++s_studioColumnsTicks < 100)
	{
		return;
	}
	KillTimer(owner,timer);
	if(dialog)
	{
		s_studioColumnsSeen = TRUE;
		SetDlgItemTextW(dialog,812,L"155");
		SendMessageW(dialog,WM_COMMAND,IDOK,0);
	}
}

static HRESULT StudioListViews(PCWSTR scratch)
{
	HRESULT hr;
	TStackRef<CQuadooStudio> studio;
	TStackRef<IJSONObject> project;
	TStackRef<IJSONObject> baseline;
	TStackRef<IJSONObject> current;
	TStackRef<IQuadooControlContainer> parent;
	TStackRef<IQuadooComponent> component;
	TStackRef<IQuadooComponent> images;
	TStackRef<IQuadooListView> view;
	TStackRef<IQuadooListColumns> columns;
	TStackRef<IQuadooListColumn> first;
	TStackRef<IQuadooListColumn> second;
	TStackRef<ITextDocument> document;
	TStackRef<IJSONValue> value;
	CRString folder;
	CRString projectPath;
	CRString packages;
	CRString sourcePath;
	CRString source;
	CRString before;
	HWND window = NULL;
	GUID id;
	GUID imageID;
	GUID popupID;
	GUID columnID;
	CProjectFile* file;
	UINT count;
	INT width;
	QuadooForm::Rect bounds = {12,12,360,220};
	Check(RStrFormatW(&folder,L"%ls\\ListViews",scratch));
	CheckIf(!CreateDirectoryW(folder.c_wstr(),NULL),E_FAIL);
	Check(RStrFormatW(&projectPath,L"%r\\ListViews.qsproj",static_cast<RSTRING>(folder)));
	Check(RStrFormatW(&packages,L"%r\\Packages",static_cast<RSTRING>(folder)));
	Check(RStrFormatW(&sourcePath,L"%r\\MainForm.quadoo",static_cast<RSTRING>(folder)));
	Check(JSONCreateObject(&project));
	Check(QFD::PutString(project,L"target",RSTRING_CAST(L"application")));
	Check(CreateApplicationFixture(project,projectPath.c_wstr()));
	studio.Attach(__new CQuadooStudio(GetModuleHandleW(NULL)));
	CheckAlloc(studio);
	Check(studio->Initialize(L"",1260,820,SW_SHOWNOACTIVATE,packages.c_wstr()));
	Check(studio->GetWindow(&window));
	Check(studio->OpenProject(projectPath.c_wstr()));
	Pump();
	CQuadooProject* ide = studio->Project();
	QFD::CDesignerEditor* editor = ide->ActiveDesigner();
	REQUIRE(editor != NULL);
	REQUIRE(GetMenuState(GetMenu(window),ID_FORM_LISTCOLUMNSEDITOR,MF_BYCOMMAND) != static_cast<UINT>(-1));
	REQUIRE(!Enabled(ide,QFD::ListColumnsEditor));
	Check(editor->AddComponent(RSTRING_CAST(L"ImageList"),&imageID));
	Check(editor->AddComponent(RSTRING_CAST(L"PopupMenu"),&popupID));
	Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&parent)));
	Check(editor->AddControl(RSTRING_CAST(L"ListView"),parent,&bounds,&id));
	Check(editor->Document().FindComponent(id,&component));
	REQUIRE(Enabled(ide,QFD::ListColumnsEditor) && !Enabled(ide,QFD::TreeNodesEditor));
	{
		WCHAR text[40];
		INT length = StringFromGUID2(imageID,text,ARRAYSIZE(text));
		CheckIf(!length,E_FAIL);
		Check(JSONCreateStringW(text,length - 1,&value));
		Check(SetProperty(editor,component,L"SmallImages",value));
		value.Release();
		component.Release();
		Check(editor->Document().FindComponent(id,&component));
		length = StringFromGUID2(popupID,text,ARRAYSIZE(text));
		CheckIf(!length,E_FAIL);
		Check(JSONCreateStringW(text,length - 1,&value));
		Check(SetProperty(editor,component,L"PopupMenu",value));
		value.Release();
	}
	Check(ide->FindSource(RSTRING_CAST(L".\\MainForm.quadoo"),&file));
	Check(ide->SourceDocument(file,&document));
	Check(StudioSource::ReadDocument(document,&before));
	Check(editor->Document().Serialize(&baseline));
	{
		QFD::CListColumnsEditSession session;
		Check(session.Initialize(editor,id));
		Check(session.columns->Insert(0,RSTRING_CAST(L"Name"),&first));
		Check(first->SetWidth(125));
		Check(first->GetID(&columnID));
		Check(session.columns->Insert(1,RSTRING_CAST(L"Detail"),&second));
		Check(second->SetWidth(173));
		Check(second->SetAlignment(QuadooForm::ListColumnRight));
		StudioSource::FailAfter(0);
		hr = session.Apply();
		StudioSource::FailAfter(-1);
		REQUIRE(FAILED(hr));
		Check(editor->Document().Serialize(&current));
		REQUIRE(JSONCompareObjects(baseline,current) == 0);
		current.Release();
		Check(StudioSource::ReadDocument(document,&source));
		REQUIRE(source == static_cast<RSTRING>(before));
		Check(session.Apply());
	}
	Pump();
	HWND outline = GetDlgItem(window,IDC_PROJECT_FILES);
	REQUIRE(FindComponent(outline,file,id) != NULL && FindComponent(outline,file,columnID) == NULL);
	source = NULL;
	Check(StudioSource::ReadDocument(document,&source));
	REQUIRE(source == static_cast<RSTRING>(before));
	REQUIRE(wcsstr(source.c_wstr(),L"property var ListView1;") != NULL);
	REQUIRE(!wcsstr(source.c_wstr(),L"property var Name;") && !wcsstr(source.c_wstr(),L"property var Detail;"));
	Check(editor->SelectID(id));
	editor->SetDPI(144);
	s_studioColumnsSeen = FALSE;
	s_studioColumnsTicks = 0;
	CheckIf(!SetTimer(window,8152,30,StudioColumnsTimer),E_FAIL);
	Check(studio->Exec(NULL,ID_FORM_LISTCOLUMNSEDITOR,0,NULL,NULL));
	KillTimer(window,8152);
	REQUIRE(s_studioColumnsSeen);
	Check(studio->Exec(NULL,ID_FILE_SAVEALL,0,NULL,NULL));
	Check(ReadText(sourcePath.c_wstr(),source));
	REQUIRE(wcsstr(source.c_wstr(),L"property var ListView1;") != NULL);
	Check(ide->SetDesignView(FALSE));
	REQUIRE(!Enabled(ide,QFD::ListColumnsEditor));
	Check(ide->SetDesignView(TRUE));
	Check(studio->Exec(NULL,ID_FILE_CLOSEPROJECT,0,NULL,NULL));
	Check(studio->OpenProject(projectPath.c_wstr()));
	Pump();
	editor = studio->Project()->ActiveDesigner();
	component.Release();
	Check(editor->Document().FindComponent(id,&component));
	Check(component->QueryInterface(__uuidof(IQuadooListView),reinterpret_cast<PVOID*>(&view)));
	Check(view->GetColumns(&columns));
	Check(columns->GetCount(&count));
	REQUIRE(count == 2);
	{
		TStackRef<IQuadooWinControl> native;
		HWND list;
		RECT bounds;
		Check(component->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native)));
		Check(native->GetHandle(&list));
		HWND header = ListView_GetHeader(list);
		REQUIRE(Header_GetItemCount(header) == 2);
		REQUIRE(GetWindowLongW(header,GWL_STYLE) & WS_VISIBLE);
		REQUIRE(GetWindowRect(header,&bounds) && bounds.bottom > bounds.top && bounds.right > bounds.left);
	}
	first.Release();
	Check(columns->Find(columnID,&first));
	Check(first->GetWidth(&width));
	REQUIRE(width == 155);
	Check(view->GetImages(QuadooForm::ListSmallImages,&images));
	REQUIRE(images != NULL);
	Check(QFD::ReadProperty(component,L"PopupMenu",&value));
	GUID reference;
	Check(value->GetStringAsGuid(reference));
	REQUIRE(IsEqualGUID(reference,popupID));
	hr = S_OK;
Cleanup:
	StudioSource::FailAfter(-1);
	if(window)
	{
		KillTimer(window,8152);
	}
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
