#include "..\..\..\..\desktop\QuadooStudio\QuadooStudio\ApplicationSettingsDlg.h"
#include "Library\Window\DialogHost.h"
#include "..\..\..\platform\QuadooForm\Harness\TestIcon.h"
#include "..\..\..\platform\QuadooForm\Harness\IconRepairData.h"

static HRESULT FileFingerprint(PCWSTR path, ULONGLONG* fingerprint);

static HRESULT ProjectTestIcon(IJSONObject* project, BOOL unmasked = FALSE)
{
	HRESULT hr;
	TStackRef<IJSONValue> icon;
	TStackRef<IJSONValue> data;
	TStackRef<IJSONObject> object;
	if(unmasked)
	{
		CMemoryStream bytes;
		Check(TestUnmaskedIcon(bytes));
		Check(TestRawIconValue(bytes.GetReadPtr(),bytes.DataRemaining(),&icon));
	}
	else
	{
		Check(TestIconValue(&icon));
	}
	Check(icon->GetObject(&object));
	Check(object->FindNonNullValueW(L"data",&data));
	Check(project->AddValueW(L"icon",data));
Cleanup:
	return hr;
}

static CApplicationSettingsDlg* s_settingsDialog;
static PCWSTR s_settingsIcon;
static PCWSTR s_settingsModule;
static PCWSTR s_settingsOtherModule;
static UINT s_settingsAction;
static HRESULT s_settingsResult;
static BOOL s_settingsVisited;

static HRESULT CheckModuleLists(IJSONValue* value, sysint x86, sysint x64)
{
	HRESULT hr;
	TStackRef<IJSONObject> settings;
	TStackRef<IJSONValue> field;
	TStackRef<IJSONArray> modules;
	Check(value->GetObject(&settings));
	Check(settings->FindNonNullValueW(L"x86",&field));
	Check(field->GetArray(&modules));
	REQUIRE(modules->Count() == x86);
	field.Release();
	modules.Release();
	Check(settings->FindNonNullValueW(L"x64",&field));
	Check(field->GetArray(&modules));
	REQUIRE(modules->Count() == x64);
Cleanup:
	return hr;
}

static HRESULT ExerciseSettingsWindow(HWND window)
{
	HRESULT hr = S_OK;
	HWND list = GetDlgItem(window,IDC_APPLICATION_MODULES);
	UINT native = sizeof(void*) == 8 ? IDC_MODULES_X64 : IDC_MODULES_X86;
	UINT other = native == IDC_MODULES_X64 ? IDC_MODULES_X86 : IDC_MODULES_X64;
	REQUIRE(list != NULL);
	REQUIRE(Header_GetItemCount(ListView_GetHeader(list)) == 2);
	REQUIRE(IsDlgButtonChecked(window,native) == BST_CHECKED && IsDlgButtonChecked(window,other) == BST_UNCHECKED);
	if(s_settingsDialog)
	{
		TStackRef<IJSONValue> settings;
		WCHAR path[MAX_PATH];
		Check(s_settingsDialog->LoadIconFile(s_settingsIcon));
		Check(s_settingsDialog->AddModule(s_settingsModule));
		REQUIRE(s_settingsDialog->AddModule(s_settingsModule) == HRESULT_FROM_WIN32(ERROR_DUP_NAME));
		REQUIRE(s_settingsDialog->Modules()->Count() == 1);
		REQUIRE(RStrLen(s_settingsDialog->IconData()) > 0);
		SendMessageW(GetDlgItem(window,other),BM_CLICK,0,0);
		REQUIRE(IsDlgButtonChecked(window,other) == BST_CHECKED && IsDlgButtonChecked(window,native) == BST_UNCHECKED);
		REQUIRE(ListView_GetItemCount(list) == 0);
		Check(s_settingsDialog->AddModule(s_settingsOtherModule));
		REQUIRE(ListView_GetItemCount(list) == 1);
		ListView_GetItemText(list,0,1,path,ARRAYSIZE(path));
		REQUIRE(!lstrcmpiW(path,L".\\Other\\Plugin.qbc"));
		Check(s_settingsDialog->WrapModules(&settings));
		Check(CheckModuleLists(settings,1,1));
		SendMessageW(window,WM_COMMAND,IDC_REMOVE_MODULE,0);
		REQUIRE(!ListView_GetItemCount(list));
		SendMessageW(GetDlgItem(window,native),BM_CLICK,0,0);
		ListView_GetItemText(list,0,1,path,ARRAYSIZE(path));
		REQUIRE(!lstrcmpiW(path,L".\\Plugin.qbc"));
	}
	else
	{
		// The saved legacy array is an x86 list on either host.
		REQUIRE(ListView_GetItemCount(list) == (native == IDC_MODULES_X86 ? 1 : 0));
		SendMessageW(GetDlgItem(window,IDC_MODULES_X86),BM_CLICK,0,0);
		REQUIRE(IsDlgButtonChecked(window,IDC_MODULES_X86) == BST_CHECKED && ListView_GetItemCount(list) == 1);
		SendMessageW(GetDlgItem(window,IDC_MODULES_X64),BM_CLICK,0,0);
		REQUIRE(IsDlgButtonChecked(window,IDC_MODULES_X64) == BST_CHECKED && ListView_GetItemCount(list) == 0);
		SendMessageW(GetDlgItem(window,IDC_MODULES_X86),BM_CLICK,0,0);
	}
	REQUIRE(SendMessageW(GetDlgItem(window,IDC_EMBED_ICON),STM_GETICON,0,0) != 0);
	REQUIRE(ListView_GetItemCount(list) == 1);
	if(s_settingsAction >= 2)
	{
		SendMessageW(window,WM_COMMAND,IDC_CLEAR_ICON,0);
		ListView_SetItemState(list,0,LVIS_SELECTED,LVIS_SELECTED);
		SendMessageW(window,WM_COMMAND,IDC_REMOVE_MODULE,0);
		REQUIRE(!SendMessageW(GetDlgItem(window,IDC_EMBED_ICON),STM_GETICON,0,0));
		REQUIRE(!ListView_GetItemCount(list));
	}
	else
	{
		REQUIRE(IsWindowEnabled(GetDlgItem(window,IDC_CLEAR_ICON)));
	}
Cleanup:
	return hr;
}

static BOOL CALLBACK FindSettingsWindow(HWND window, LPARAM)
{
	if(GetDlgItem(window,IDC_APPLICATION_MODULES))
	{
		s_settingsVisited = TRUE;
		s_settingsResult = ExerciseSettingsWindow(window);
		PostMessageW(window,WM_COMMAND,SUCCEEDED(s_settingsResult) && (s_settingsAction & 1) ? IDOK : IDCANCEL,0);
		return FALSE;
	}
	return TRUE;
}

static VOID CALLBACK SettingsTimer(HWND owner, UINT, UINT_PTR identity, DWORD)
{
	if(!s_settingsVisited)
	{
		EnumThreadWindows(GetCurrentThreadId(),FindSettingsWindow,0);
	}
	if(s_settingsVisited)
	{
		KillTimer(owner,identity);
	}
}

static HRESULT BeginSettingsDriver(HWND owner, UINT action)
{
	HRESULT hr = S_OK;
	s_settingsAction = action;
	s_settingsVisited = FALSE;
	s_settingsResult = E_PENDING;
	CheckIf(!SetTimer(owner,5902,50,SettingsTimer),HrEnsureLastError());
Cleanup:
	return hr;
}

static HRESULT ApplicationSettingsTests(PCWSTR scratch)
{
	HRESULT hr;
	TStackRef<IJSONObject> project;
	TStackRef<IJSONObject> saved;
	TStackRef<IJSONValue> value;
	TStackRef<IJSONArray> modules;
	TStackRef<CQuadooStudio> studio;
	CRString directory;
	CRString projectPath;
	CRString iconPath;
	CRString modulePath;
	CRString otherDirectory;
	CRString otherModulePath;
	CRString store;
	CRString text;
	CMemoryStream bytes;
	HANDLE file = INVALID_HANDLE_VALUE;
	DWORD written;
	HWND owner = NULL;
	HWND window = NULL;
	ULONGLONG before;
	ULONGLONG after;
	OLECMD command = {ID_PROJECT_SETTINGS,0};
	Check(RStrFormatW(&directory,L"%ls\\Settings",scratch));
	CheckIf(!CreateDirectoryW(directory.c_wstr(),NULL),HrEnsureLastError());
	Check(RStrFormatW(&projectPath,L"%r\\Settings.qsproj",static_cast<RSTRING>(directory)));
	Check(RStrFormatW(&iconPath,L"%r\\Application.ico",static_cast<RSTRING>(directory)));
	Check(RStrFormatW(&modulePath,L"%r\\Plugin.qbc",static_cast<RSTRING>(directory)));
	Check(RStrFormatW(&otherDirectory,L"%r\\Other",static_cast<RSTRING>(directory)));
	CheckIf(!CreateDirectoryW(otherDirectory.c_wstr(),NULL),HrEnsureLastError());
	Check(RStrFormatW(&otherModulePath,L"%r\\Plugin.qbc",static_cast<RSTRING>(otherDirectory)));
	Check(RStrFormatW(&store,L"%r\\Packages",static_cast<RSTRING>(directory)));
	Check(JSONCreateObject(&project));
	Check(ProjectTestIcon(project,TRUE));
	Check(StudioApplication::DecodeApplicationIcon(project,bytes));
	REQUIRE(bytes.DataRemaining() == 38 + 1128 + 4264);
	ICONDIRENTRY repairedEntry;
	CopyMemory(&repairedEntry,bytes.GetReadPtr() + sizeof(ICONHEAD),sizeof(repairedEntry));
	REQUIRE(repairedEntry.dwImageOffset == 38 && repairedEntry.dwBytesInRes == 1128);
	// File selection must repair the same original ICO, independently of builds.
	Check(TestUnmaskedIcon(bytes));
	file = CreateFileW(iconPath.c_wstr(),GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
	CheckIf(file == INVALID_HANDLE_VALUE,HrEnsureLastError());
	CheckIf(!WriteFile(file,bytes.GetReadPtr(),bytes.DataRemaining(),&written,NULL),HrEnsureLastError());
	SafeCloseFileHandle(file);
	Check(Text::SaveToFile(SLP(L"Module fixture"),modulePath.c_wstr()));
	Check(Text::SaveToFile(SLP(L"Other architecture fixture"),otherModulePath.c_wstr()));
	project->RemoveValueW(L"icon");
	s_settingsIcon = iconPath.c_wstr();
	s_settingsModule = modulePath.c_wstr();
	s_settingsOtherModule = otherModulePath.c_wstr();
	owner = CreateWindowExW(0,L"STATIC",L"Settings test",WS_OVERLAPPEDWINDOW,0,0,100,100,NULL,NULL,GetModuleHandle(NULL),NULL);
	CheckIf(!owner,HrEnsureLastError());
	for(UINT action = 0; action < 2; action++)
	{
		CApplicationSettingsDlg dialog(project,directory);
		CDialogHost host(GetModuleHandle(NULL));
		s_settingsDialog = &dialog;
		Check(BeginSettingsDriver(owner,action));
		Check(host.Display(owner,&dialog));
		KillTimer(owner,5902);
		Check(dialog.Error());
		Check(s_settingsResult);
		REQUIRE(s_settingsVisited && host.GetReturnValue() == (action ? IDOK : IDCANCEL));
		REQUIRE(!project->HasField(RSTRING_CAST(L"icon")) && !project->HasField(RSTRING_CAST(L"modules")));
	}
	s_settingsDialog = NULL;
	Check(QFD::PutString(project,L"target",RSTRING_CAST(L"application")));
	Check(CreateApplicationFixture(project,projectPath.c_wstr()));
	Check(ProjectTestIcon(project));
	Check(JSONCreateArray(&modules));
	Check(JSONCreateString(RSTRING_CAST(L".\\Plugin.qbc"),&value));
	Check(modules->Add(value));
	value.Release();
	Check(JSONWrapArray(modules,&value));
	Check(project->AddValueW(L"modules",value));
	value.Release();
	{
		CMemoryStream legacy;
		Check(JSONSerializeObject(project,&legacy));
		Check(Text::SaveToFile(legacy.TGetReadPtr<WCHAR>(),legacy.TDataRemaining<WCHAR>(),projectPath.c_wstr()));
	}
	studio.Attach(__new CQuadooStudio(GetModuleHandle(NULL)));
	CheckAlloc(studio);
	Check(studio->Initialize(L"",1100,740,SW_SHOWNOACTIVATE,store.c_wstr()));
	Check(studio->GetWindow(&window));
	Check(studio->QueryStatus(NULL,1,&command,NULL));
	REQUIRE(!(command.cmdf & OLECMDF_ENABLED));
	REQUIRE(GetMenuState(GetMenu(window),ID_PROJECT_SETTINGS,MF_BYCOMMAND) != static_cast<UINT>(-1));
	Check(studio->OpenProject(projectPath.c_wstr()));
	Pump();
	Check(studio->QueryStatus(NULL,1,&command,NULL));
	REQUIRE(command.cmdf & OLECMDF_ENABLED);
	Check(FileFingerprint(projectPath.c_wstr(),&before));
	Check(BeginSettingsDriver(window,2));
	REQUIRE(studio->Exec(NULL,ID_PROJECT_SETTINGS,0,NULL,NULL) == S_FALSE);
	KillTimer(window,5902);
	Check(s_settingsResult);
	Check(FileFingerprint(projectPath.c_wstr(),&after));
	REQUIRE(before == after);
	file = CreateFileW(projectPath.c_wstr(),GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
	CheckIf(file == INVALID_HANDLE_VALUE,HrEnsureLastError());
	Check(BeginSettingsDriver(window,3));
	REQUIRE(FAILED(studio->Exec(NULL,ID_PROJECT_SETTINGS,0,NULL,NULL)));
	KillTimer(window,5902);
	Check(s_settingsResult);
	SafeCloseFileHandle(file);
	Check(FileFingerprint(projectPath.c_wstr(),&after));
	REQUIRE(before == after);
	// Failed publication restored the live settings, including the icon preview.
	{
		CProjectFile* source;
		CMemoryStream snapshot;
		CRString snapshotPath;
		TStackRef<IJSONObject> paired;
		TStackRef<IJSONObject> ordinary;
		TStackRef<IJSONValue> settings;
		Check(studio->Project()->FindSource(RSTRING_CAST(L".\\MainForm.quadoo"),&source));
		Check(studio->Project()->PrepareUnitSave(source,&snapshot,&snapshotPath));
		Check(QFD::Parse(snapshot.TGetReadPtr<WCHAR>(),static_cast<INT>(snapshot.TDataRemaining<WCHAR>()),&paired));
		Check(paired->FindNonNullValueW(L"modules",&settings));
		Check(CheckModuleLists(settings,1,0));
		Check(studio->Project()->Save());
		Check(ReadJSON(projectPath.c_wstr(),&ordinary));
		settings.Release();
		Check(ordinary->FindNonNullValueW(L"modules",&settings));
		Check(CheckModuleLists(settings,1,0));
	}
	Check(BeginSettingsDriver(window,3));
	Check(studio->Exec(NULL,ID_PROJECT_SETTINGS,0,NULL,NULL));
	KillTimer(window,5902);
	Check(s_settingsResult);
	Check(ReadText(projectPath.c_wstr(),text));
	Check(QFD::Parse(text.c_wstr(),text.Length(),&saved));
	REQUIRE(!saved->HasField(RSTRING_CAST(L"icon")));
	Check(saved->FindNonNullValueW(L"modules",&value));
	Check(CheckModuleLists(value,0,0));
	Check(studio->Exec(NULL,ID_FILE_CLOSEPROJECT,0,NULL,NULL));
	project.Release();
	Check(JSONCreateObject(&project));
	Check(QFD::PutString(project,L"target",RSTRING_CAST(L"qvm")));
	Check(QFD::PutString(project,L"args",RSTRING_CAST(L"")));
	Check(QFD::PutString(project,L"startDir",RSTRING_CAST(L".\\")));
	modules.Release();
	Check(JSONCreateArray(&modules));
	value.Release();
	Check(JSONWrapArray(modules,&value));
	Check(project->AddValueW(L"files",value));
	Check(SaveStudioProject(project,projectPath.c_wstr()));
	Check(studio->OpenProject(projectPath.c_wstr()));
	Check(studio->QueryStatus(NULL,1,&command,NULL));
	REQUIRE(!(command.cmdf & OLECMDF_ENABLED));
	REQUIRE(studio->Exec(NULL,ID_PROJECT_SETTINGS,0,NULL,NULL) == OLECMDERR_E_DISABLED);
Cleanup:
	s_settingsDialog = NULL;
	SafeCloseFileHandle(file);
	if(owner)
	{
		KillTimer(owner,5902);
		DestroyWindow(owner);
	}
	if(window)
	{
		KillTimer(window,5902);
	}
	if(studio && studio->Project())
	{
		studio->Project()->CloseProject(FALSE);
		studio->Exec(NULL,ID_FILE_CLOSEPROJECT,0,NULL,NULL);
	}
	if(studio)
	{
		studio->Destroy();
	}
	Pump();
	return hr;
}
