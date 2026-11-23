#include <dlgs.h>

// Existing fixtures choose their source explicitly, through the same project
// method used by the interactive first-form and Form-menu paths.
static HRESULT CreateApplicationFixture(IJSONObject* project, PCWSTR path)
{
	HRESULT hr;
	TStackRef<CQuadooStudio> studio;
	TStackRef<IJSONObject> saved;
	TStackRef<IJSONValue> value;
	CRString directory;
	CRString source;
	CRString packages;
	PCWSTR leaf = TStrRChr(path,L'\\');
	CheckIf(!leaf,E_INVALIDARG);
	Check(RStrCreateW(static_cast<INT>(leaf - path),path,&directory));
	Check(RStrFormatW(&source,L"%ls\\MainForm.quadoo",directory.c_wstr()));
	Check(RStrFormatW(&packages,L"%ls\\Fixture Packages",directory.c_wstr()));
	Check(CreateApplicationProject(project,path));
	studio.Attach(__new CQuadooStudio(GetModuleHandle(NULL)));
	CheckAlloc(studio);
	Check(studio->Initialize(L"",1100,740,SW_HIDE,packages.c_wstr()));
	Check(studio->OpenProject(path));
	Check(studio->Project()->NewFormUnit(TRUE,source.c_wstr()));
	Check(ReadJSON(path,&saved));
	Check(saved->FindNonNullValueW(L"files",&value));
	Check(project->AddValueW(L"files",value));
	value.Release();
	Check(saved->FindNonNullValueW(L"application",&value));
	Check(project->AddValueW(L"application",value));
Cleanup:
	if(studio)
	{
		if(studio->Project())
		{
			studio->Project()->CloseProject(FALSE);
		}
		studio->Destroy();
	}
	Pump();
	return hr;
}

struct ProjectCreationDriver
{
	PCWSTR project;
	PCWSTR source;
	BOOL cancel;
	BOOL formSeen;
	UINT phase;
	DWORD start;
	HRESULT error;
};
static ProjectCreationDriver s_creation;

static BOOL CALLBACK CreationFileNameEdit(HWND window, LPARAM data)
{
	WCHAR type[32];
	WCHAR parent[32];
	GetClassNameW(window,type,ARRAYSIZE(type));
	GetClassNameW(GetParent(window),parent,ARRAYSIZE(parent));
	if(!lstrcmpiW(type,L"Edit") && !lstrcmpiW(parent,L"ComboBox"))
	{
		*reinterpret_cast<HWND*>(data) = window;
		return FALSE;
	}
	return TRUE;
}
static BOOL ChooseCreationFile(HWND window, PCWSTR path)
{
	if(!IsWindowVisible(window) || !IsWindowEnabled(window))
	{
		return FALSE;
	}
	HWND edit = NULL;
	EnumChildWindows(window,CreationFileNameEdit,reinterpret_cast<LPARAM>(&edit));
	if(!edit)
	{
		edit = GetDlgItem(window,edt1);
	}
	if(edit)
	{
		SetFocus(edit);
		SendMessageW(edit,EM_SETSEL,0,-1);
		SendMessageW(edit,EM_REPLACESEL,TRUE,reinterpret_cast<LPARAM>(path));
		PostMessageW(GetDlgItem(window,IDOK),BM_CLICK,0,0);
	}
	return edit != NULL;
}
static BOOL CALLBACK ProjectCreationDialog(HWND window, LPARAM)
{
	WCHAR type[32];
	WCHAR title[80];
	GetClassNameW(window,type,ARRAYSIZE(type));
	if(lstrcmpW(type,L"#32770"))
	{
		return TRUE;
	}
	GetWindowTextW(window,title,ARRAYSIZE(title));
	BOOL project = !lstrcmpW(title,L"Create New Project");
	BOOL file = !lstrcmpW(title,L"Create Project File");
	BOOL form = !lstrcmpW(title,L"Add Form Source");
	if(!project && !file && !form)
	{
		return TRUE;
	}
	if(GetTickCount() - s_creation.start > 15000)
	{
		s_creation.error = HRESULT_FROM_WIN32(ERROR_TIMEOUT);
		PostMessageW(window,WM_COMMAND,IDCANCEL,0);
	}
	else if(project && IsWindowEnabled(window))
	{
		if(s_creation.phase == 0)
		{
			s_creation.phase = 1;
			SendDlgItemMessageW(window,IDC_APPLICATION,BM_CLICK,0,0);
			PostMessageW(window,WM_COMMAND,IDC_BROWSE,0);
		}
		else if(s_creation.phase == 2)
		{
			s_creation.phase = 3;
			PostMessageW(window,WM_COMMAND,IDOK,0);
		}
	}
	else if(file && s_creation.phase == 1)
	{
		if(ChooseCreationFile(window,s_creation.project))
		{
			s_creation.phase = 2;
		}
	}
	else if(form && s_creation.phase == 3)
	{
		if(s_creation.cancel || ChooseCreationFile(window,s_creation.source))
		{
			s_creation.phase = 4;
			s_creation.formSeen = TRUE;
			if(s_creation.cancel)
			{
				PostMessageW(window,WM_COMMAND,IDCANCEL,0);
			}
		}
	}
	return TRUE;
}
static VOID CALLBACK ProjectCreationTimer(HWND, UINT, UINT_PTR, DWORD)
{
	EnumThreadWindows(GetCurrentThreadId(),ProjectCreationDialog,0);
}
static HRESULT DriveProjectCreation(CQuadooStudio* studio, PCWSTR project, PCWSTR source, BOOL cancel)
{
	HRESULT hr;
	HWND window = NULL;
	Check(studio->GetWindow(&window));
	ZeroMemory(&s_creation,sizeof(s_creation));
	s_creation.project = project;
	s_creation.source = source;
	s_creation.cancel = cancel;
	s_creation.phase = project ? 0 : 3;
	s_creation.start = GetTickCount();
	CheckIf(!SetTimer(window,5903,20,ProjectCreationTimer),HrEnsureLastError());
	Check(studio->Exec(NULL,project ? ID_FILE_NEW_PROJECT : QFD::NewForm,0,NULL,NULL));
	Check(s_creation.error);
	REQUIRE(s_creation.formSeen);
Cleanup:
	if(FAILED(hr))
	{
		CRString line;
		if(SUCCEEDED(RStrFormatW(&line,L"Creation phase %u error 0x%.8x\r\n",s_creation.phase,s_creation.error)))
		{
			Log(line.c_wstr());
		}
	}
	if(window)
	{
		KillTimer(window,5903);
	}
	return hr;
}
static HRESULT VerifyCreatedDefault(CQuadooStudio* studio, PCWSTR project, PCWSTR relative, PCWSTR name, UINT count)
{
	HRESULT hr;
	CProjectFile* file;
	TStackRef<IJSONObject> saved;
	TStackRef<IJSONObject> application;
	TStackRef<IJSONArray> files;
	TStackRef<IJSONValue> value;
	CRString text;
	CRString expected;
	UINT defaults = 0;
	Check(studio->Project()->FindSource(RSTRING_CAST(relative),&file));
	REQUIRE(file->m_fDefault && file->m_form && file->m_form->form);
	REQUIRE(file->m_form->name == RSTRING_CAST(name));
	Check(ReadText(file->m_pwzAbsolutePath,text));
	Check(RStrFormatW(&expected,L"class %ls : extern(Form), I%ls",name,name));
	REQUIRE(TStrStr(text.c_wstr(),expected.c_wstr()) != NULL);
	Check(ReadJSON(project,&saved));
	Check(saved->FindNonNullValueW(L"files",&value));
	Check(value->GetArray(&files));
	value.Release();
	REQUIRE(files->Count() == count);
	for(sysint n = 0; n < files->Count(); n++)
	{
		TStackRef<IJSONObject> entry;
		Check(files->GetObject(n,&entry));
		if(entry->HasField(RSTRING_CAST(L"default")))
		{
			bool isDefault;
			Check(entry->FindNonNullValueW(L"default",&value));
			Check(value->GetBoolean(&isDefault));
			value.Release();
			if(isDefault)
			{
				defaults++;
				text = static_cast<RSTRING>(NULL);
				Check(entry->FindNonNullValueW(L"path",&value));
				Check(value->GetString(&text));
				value.Release();
				REQUIRE(text == RSTRING_CAST(relative));
			}
		}
	}
	REQUIRE(defaults == 1);
	Check(saved->FindNonNullValueW(L"application",&value));
	Check(value->GetObject(&application));
	value.Release();
	REQUIRE(!application->HasField(RSTRING_CAST(L"mainForm")));
Cleanup:
	return hr;
}
static HRESULT ApplicationProjectCreation(PCWSTR folder)
{
	HRESULT hr;
	TStackRef<CQuadooStudio> studio;
	TStackRef<IJSONObject> saved;
	TStackRef<IJSONObject> application;
	TStackRef<IJSONArray> files;
	TStackRef<IJSONValue> value;
	CRString packages;
	CRString project;
	CRString source;
	CRString forms;
	CRString frame;
	CRString failed;
	CProjectFile* file = NULL;
	HANDLE lock = INVALID_HANDLE_VALUE;
	Check(RStrFormatW(&packages,L"%ls\\Creation Packages",folder));
	Check(RStrFormatW(&forms,L"%ls\\Forms",folder));
	CheckIf(!CreateDirectoryW(forms.c_wstr(),NULL),HrEnsureLastError());
	studio.Attach(__new CQuadooStudio(GetModuleHandle(NULL)));
	CheckAlloc(studio);
	Check(studio->Initialize(L"",1100,740,SW_SHOWNOACTIVATE,packages.c_wstr()));
	Check(RStrFormatW(&project,L"%ls\\ChosenName.qsproj",folder));
	Check(RStrFormatW(&source,L"%ls\\Welcome.quadoo",forms.c_wstr()));
	Check(DriveProjectCreation(studio,project.c_wstr(),source.c_wstr(),FALSE));
	Check(VerifyCreatedDefault(studio,project.c_wstr(),L".\\Forms\\Welcome.quadoo",L"Welcome",1));
	REQUIRE(studio->Project()->DesignerActive());
	source = static_cast<RSTRING>(NULL);
	Check(RStrFormatW(&source,L"%ls\\Options.quadoo",forms.c_wstr()));
	Check(DriveProjectCreation(studio,NULL,source.c_wstr(),FALSE));
	Check(studio->Project()->FindSource(RSTRING_CAST(L".\\Forms\\Options.quadoo"),&file));
	REQUIRE(!file->m_fDefault);
	Check(VerifyCreatedDefault(studio,project.c_wstr(),L".\\Forms\\Welcome.quadoo",L"Welcome",2));
	Check(studio->Exec(NULL,ID_FILE_CLOSEPROJECT,0,NULL,NULL));
	Check(ReadJSON(project.c_wstr(), &saved));
	Check(saved->FindNonNullValueW(L"application", &value));
	Check(value->GetObject(&application));
	value.Release();
	Check(JSONCreateInteger(17, &value));
	Check(application->AddValueW(L"mainForm", value));
	value.Release();
	Check(SaveStudioProject(saved, project.c_wstr()));
	saved.Release();
	application.Release();
	Check(studio->OpenProject(project.c_wstr()));
	Check(ReadJSON(project.c_wstr(), &saved));
	Check(saved->FindNonNullValueW(L"application", &value));
	Check(value->GetObject(&application));
	value.Release();
	REQUIRE(application->HasField(RSTRING_CAST(L"mainForm")));
	Check(studio->Project()->Save());
	Check(VerifyCreatedDefault(studio,project.c_wstr(),L".\\Forms\\Welcome.quadoo",L"Welcome",2));
	Check(studio->Exec(NULL,ID_FILE_CLOSEPROJECT,0,NULL,NULL));
	saved.Release();
	application.Release();

	project = static_cast<RSTRING>(NULL);
	Check(RStrFormatW(&project,L"%ls\\CanceledFirstForm.qsproj",folder));
	Check(DriveProjectCreation(studio,project.c_wstr(),NULL,TRUE));
	REQUIRE(studio->Project() && !studio->Project()->DesignerActive());
	Check(ReadJSON(project.c_wstr(),&saved));
	Check(saved->FindNonNullValueW(L"files",&value));
	Check(value->GetArray(&files));
	value.Release();
	REQUIRE(files->Count() == 0);
	Check(saved->FindNonNullValueW(L"application",&value));
	Check(value->GetObject(&application));
	value.Release();
	REQUIRE(!application->HasField(RSTRING_CAST(L"mainForm")));
	source = static_cast<RSTRING>(NULL);
	Check(RStrFormatW(&source,L"%ls\\MainForm.quadoo",folder));
	REQUIRE(GetFileAttributesW(source.c_wstr()) == INVALID_FILE_ATTRIBUTES);
	source = static_cast<RSTRING>(NULL);
	Check(RStrFormatW(&source,L"%ls\\Form1.quadoo",folder));
	REQUIRE(GetFileAttributesW(source.c_wstr()) == INVALID_FILE_ATTRIBUTES);
	Check(RStrFormatW(&frame,L"%ls\\Part.quadoo",folder));
	Check(studio->Project()->NewFormUnit(FALSE,frame.c_wstr()));
	Check(studio->Project()->FindSource(RSTRING_CAST(L".\\Part.quadoo"),&file));
	REQUIRE(!file->m_fDefault);
	lock = CreateFileW(project.c_wstr(),GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
	CheckIf(lock == INVALID_HANDLE_VALUE,HrEnsureLastError());
	Check(RStrFormatW(&failed,L"%ls\\Failed.quadoo",folder));
	REQUIRE(FAILED(studio->Project()->NewFormUnit(TRUE,failed.c_wstr())));
	SafeCloseFileHandle(lock);
	REQUIRE(GetFileAttributesW(failed.c_wstr()) == INVALID_FILE_ATTRIBUTES);
	failed = static_cast<RSTRING>(NULL);
	Check(RStrFormatW(&failed,L"%ls\\Failed.qform.json",folder));
	REQUIRE(GetFileAttributesW(failed.c_wstr()) == INVALID_FILE_ATTRIBUTES);
	REQUIRE(studio->Project()->FindSource(RSTRING_CAST(L".\\Failed.quadoo"),&file) == HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
	source = static_cast<RSTRING>(NULL);
	Check(RStrFormatW(&source,L"%ls\\Entry.quadoo",folder));
	Check(DriveProjectCreation(studio,NULL,source.c_wstr(),FALSE));
	Check(VerifyCreatedDefault(studio,project.c_wstr(),L".\\Entry.quadoo",L"Entry",2));
Cleanup:
	SafeCloseFileHandle(lock);
	if(studio)
	{
		if(studio->Project())
		{
			studio->Project()->CloseProject(FALSE);
		}
		studio->Destroy();
	}
	Pump();
	return hr;
}
