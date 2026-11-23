static BOOL s_studioMenuVisited;
static BOOL CALLBACK DriveStudioMenu(HWND window, LPARAM)
{
	if(!GetDlgItem(window,610) || !GetDlgItem(window,625))
	{
		return TRUE;
	}
	s_studioMenuVisited = TRUE;
	SendMessageW(window,WM_COMMAND,621,0);
	SetDlgItemTextW(window,611,L"FileMenu");
	SetDlgItemTextW(window,612,L"&File");
	SendMessageW(window,WM_COMMAND,625,0);
	SendMessageW(window,WM_COMMAND,621,0);
	SetDlgItemTextW(window,611,L"OpenItem");
	SetDlgItemTextW(window,612,L"&Open");
	SetDlgItemTextW(window,613,L"Ctrl+O");
	SetDlgItemTextW(window,614,L"Menu_Open");
	SendMessageW(window,WM_COMMAND,625,0);
	SendMessageW(window,WM_COMMAND,620,0);
	SetDlgItemTextW(window,611,L"QuitItem");
	SetDlgItemTextW(window,612,L"E&xit");
	SetDlgItemTextW(window,613,L"Alt+F4");
	SetDlgItemTextW(window,614,L"Menu_Quit");
	PostMessageW(window,WM_COMMAND,IDOK,0);
	return FALSE;
}
static VOID CALLBACK StudioMenuTimer(HWND owner, UINT, UINT_PTR identity, DWORD)
{
	EnumThreadWindows(GetCurrentThreadId(),DriveStudioMenu,0);
	if(s_studioMenuVisited)
	{
		KillTimer(owner,identity);
	}
}
static HRESULT StudioChoiceEditing(QFD::CDesignerEditor* editor)
{
	HRESULT hr;
	HWND list = GetDlgItem(editor->Window(),104);
	GUID rootID;
	static PCWSTR const names[] = { L"BorderStyle",L"WindowState",L"FormStyle",L"KeyPreview" };
	const INT counts[] = {4,3,2,2};
	const INT selections[] = {1,2,1,1};
	Check(editor->Document().Root()->GetID(&rootID));
	Check(editor->SelectID(rootID));
	for(UINT n = 0; n < ARRAYSIZE(names); n++)
	{
		WCHAR type[32];
		NMITEMACTIVATE click = {0};
		click.hdr.hwndFrom = list;
		click.hdr.idFrom = 104;
		click.hdr.code = NM_DBLCLK;
		click.iItem = ListRow(list,names[n]);
		click.iSubItem = 1;
		REQUIRE(click.iItem >= 0);
		SendMessageW(editor->Window(),WM_NOTIFY,104,reinterpret_cast<LPARAM>(&click));
		HWND combo = GetDlgItem(list,120);
		REQUIRE(combo && IsWindowVisible(combo));
		GetClassNameW(combo,type,ARRAYSIZE(type));
		REQUIRE(!lstrcmpiW(type,L"ComboBox") && SendMessageW(combo,CB_GETCOUNT,0,0) == counts[n]);
		SendMessageW(combo,CB_SETCURSEL,selections[n],0);
		SendMessageW(combo,WM_KEYDOWN,VK_RETURN,0);
		REQUIRE(!IsWindowVisible(combo));
		if(n == 3)
		{
			REQUIRE(QFD::BoolProperty(editor->Document().Root(),names[n]));
		}
		else
		{
			REQUIRE(QFD::IntProperty(editor->Document().Root(),names[n]) == selections[n]);
		}
	}
Cleanup:
	return hr;
}
static HRESULT StudioMenuReference(QFD::CDesignerEditor* editor, REFGUID mainID)
{
	HRESULT hr;
	GUID rootID;
	GUID assignedID;
	TStackRef<IQuadooFormMenu> form;
	TStackRef<IQuadooComponent> assigned;
	HWND list = GetDlgItem(editor->Window(),104);
	HWND combo;
	WCHAR type[32];
	NMITEMACTIVATE click = {0};
	Check(editor->Document().Root()->GetID(&rootID));
	Check(editor->SelectID(rootID));
	click.hdr.hwndFrom = list;
	click.hdr.idFrom = 104;
	click.hdr.code = NM_DBLCLK;
	click.iItem = ListRow(list,L"MainMenu");
	click.iSubItem = 1;
	REQUIRE(click.iItem >= 0);
	SendMessageW(editor->Window(),WM_NOTIFY,104,reinterpret_cast<LPARAM>(&click));
	combo = GetDlgItem(list,120);
	REQUIRE(combo && IsWindowVisible(combo));
	GetClassNameW(combo,type,ARRAYSIZE(type));
	REQUIRE(!lstrcmpiW(type,L"ComboBox") && SendMessageW(combo,CB_GETCOUNT,0,0) == 2);
	REQUIRE(SendMessageW(combo,CB_GETCURSEL,0,0) == 1);
	SendMessageW(combo,CB_SETCURSEL,0,0);
	SendMessageW(list,WM_COMMAND,MAKEWPARAM(120,CBN_SELENDOK),reinterpret_cast<LPARAM>(combo));
	Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooFormMenu),reinterpret_cast<PVOID*>(&form)));
	Check(form->GetMainMenu(&assigned));
	REQUIRE(!assigned);
	// Reopen through the same inspector notification and reconnect the menu.
	SendMessageW(editor->Window(),WM_NOTIFY,104,reinterpret_cast<LPARAM>(&click));
	combo = GetDlgItem(list,120);
	REQUIRE(combo && IsWindowVisible(combo));
	SendMessageW(combo,CB_SETCURSEL,1,0);
	SendMessageW(combo,WM_KEYDOWN,VK_RETURN,0);
	Check(form->GetMainMenu(&assigned));
	REQUIRE(assigned != NULL);
	Check(assigned->GetID(&assignedID));
	REQUIRE(IsEqualGUID(assignedID,mainID));
	Check(editor->SelectID(mainID));
Cleanup:
	return hr;
}
static HRESULT StudioMenuIntegration(PCWSTR scratch)
{
	HRESULT hr;
	TStackRef<CQuadooStudio> studio;
	TStackRef<IJSONObject> project;
	TStackRef<IQuadooComponentHost> host;
	TStackRef<IQuadooComponentCollection> components;
	TStackRef<IQuadooComponent> file;
	TStackRef<IQuadooComponent> open;
	TStackRef<IQuadooProperties> properties;
	TStackRef<IJSONValue> value;
	CRString folder;
	CRString projectPath;
	CRString packageStore;
	CRString sourcePath;
	CRString source;
	CRString type;
	TStackRef<IQuadooFormMenu> formMenu;
	TStackRef<IQuadooComponent> assigned;
	GUID assignedID;
	HWND window = NULL;
	CProjectFile* unit;
	GUID mainID;
	GUID fileID;
	GUID openID;
	GUID selected;
	POINT point;
	Check(RStrFormatW(&folder,L"%ls\\Menus",scratch));
	CheckIf(!CreateDirectoryW(folder.c_wstr(),NULL),HrEnsureLastError());
	Check(RStrFormatW(&projectPath,L"%r\\Menus.qsproj",static_cast<RSTRING>(folder)));
	Check(RStrFormatW(&packageStore,L"%r\\Packages",static_cast<RSTRING>(folder)));
	Check(RStrFormatW(&sourcePath,L"%r\\MainForm.quadoo",static_cast<RSTRING>(folder)));
	Check(JSONCreateObject(&project));
	Check(QFD::PutString(project,L"target",RSTRING_CAST(L"application")));
	Check(CreateApplicationFixture(project,projectPath.c_wstr()));
	studio.Attach(__new CQuadooStudio(GetModuleHandle(NULL)));
	CheckAlloc(studio);
	Check(studio->Initialize(L"",1260,820,SW_SHOWNOACTIVATE,packageStore.c_wstr()));
	Check(studio->GetWindow(&window));
	Check(studio->OpenProject(projectPath.c_wstr()));
	Pump();
	CQuadooProject* ide = studio->Project();
	QFD::CDesignerEditor* editor = ide->ActiveDesigner();
	REQUIRE(editor != NULL);
	REQUIRE(GetMenuState(GetMenu(window),ID_FORM_MENUEDITOR,MF_BYCOMMAND) != static_cast<UINT>(-1));
	REQUIRE(!Enabled(ide,QFD::MenuEditor));
	Check(RStrCreateW(LSP(L"MainMenu"),&type));
	Check(editor->AddComponent(type,&mainID));
	Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooFormMenu),reinterpret_cast<PVOID*>(&formMenu)));
	Check(formMenu->GetMainMenu(&assigned));
	REQUIRE(assigned != NULL);
	Check(assigned->GetID(&assignedID));
	REQUIRE(IsEqualGUID(assignedID,mainID));
	Check(StudioMenuReference(editor,mainID));
	Check(StudioChoiceEditing(editor));
	Check(editor->SelectID(mainID));
	REQUIRE(Enabled(ide,QFD::MenuEditor));
	s_studioMenuVisited = FALSE;
	CheckIf(!SetTimer(window,6031,30,StudioMenuTimer),HrEnsureLastError());
	Check(studio->Exec(NULL,ID_FORM_MENUEDITOR,0,NULL,NULL));
	KillTimer(window,6031);
	REQUIRE(s_studioMenuVisited);
	Pump();
	Check(ide->FindSource(RSTRING_CAST(L".\\MainForm.quadoo"),&unit));
	Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&host)));
	Check(host->GetComponents(&components));
	Check(components->FindComponent(RSTRING_CAST(L"FileMenu"),&file));
	Check(components->FindComponent(RSTRING_CAST(L"OpenItem"),&open));
	Check(file->GetID(&fileID));
	Check(open->GetID(&openID));
	HWND tree = GetDlgItem(window,IDC_PROJECT_FILES);
	HTREEITEM mainNode = FindComponent(tree,unit,mainID);
	HTREEITEM fileNode = FindComponent(tree,unit,fileID);
	HTREEITEM openNode = FindComponent(tree,unit,openID);
	REQUIRE(mainNode && fileNode && openNode);
	REQUIRE(TreeView_GetParent(tree,fileNode) == mainNode && TreeView_GetParent(tree,openNode) == fileNode);
	TreeView_SelectItem(tree,openNode);
	Pump();
	Check(editor->GetSelectedID(&selected));
	REQUIRE(IsEqualGUID(selected,openID));
	Check(JSONCreateBool(false,&value));
	Check(SetProperty(editor,open,L"Enabled",value));
	REQUIRE(!QFD::BoolProperty(open,L"Enabled",TRUE));
	Check(editor->SelectID(fileID));
	RECT rect;
	Check(editor->MenuSelectionRect(&rect));
	point.x = (rect.left + rect.right) / 2;
	point.y = (rect.top + rect.bottom) / 2;
	Check(editor->SelectMenuAt(point));
	Pump();
	fileNode = FindComponent(tree,unit,fileID);
	REQUIRE(TreeView_GetSelection(tree) == fileNode);
	Check(ide->SetDesignView(FALSE));
	REQUIRE(!Enabled(ide,QFD::MenuEditor));
	Check(ide->SetDesignView(TRUE));
	REQUIRE(Enabled(ide,QFD::MenuEditor));
	Check(studio->Exec(NULL,ID_FILE_SAVEALL,0,NULL,NULL));
	Check(ReadText(sourcePath.c_wstr(),source));
	REQUIRE(wcsstr(source.c_wstr(),L"MainMenu1") && wcsstr(source.c_wstr(),L"OpenItem"));
	REQUIRE(wcsstr(source.c_wstr(),L"Menu_Open") && wcsstr(source.c_wstr(),L"Menu_Quit"));
	REQUIRE(ListView_GetItemCount(editor->TrayWindow()) == 1);
	Check(studio->Exec(NULL,ID_FILE_CLOSEPROJECT,0,NULL,NULL));
Cleanup:
	if(window)
	{
		KillTimer(window,6031);
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
