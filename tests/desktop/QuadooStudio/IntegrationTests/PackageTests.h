#include "..\..\..\..\platform\QuadooForm\Packages\PackagesResource.h"
static BOOL g_packagesDialogVerified;
static BOOL CALLBACK ClosePackagesDialog(HWND window,LPARAM owner)
{
	if(GetWindow(window,GW_OWNER)!=reinterpret_cast<HWND>(owner)) return TRUE;
	WCHAR name[64]; GetWindowTextW(window,name,ARRAYSIZE(name));
	if(lstrcmpW(name,L"Control Packages")) return TRUE;
	HWND list=GetDlgItem(window,IDC_QF_PACKAGE_LIST); INT cx=0,cy=0;
	g_packagesDialogVerified=list && ListView_GetItemCount(list)==1 &&
		ImageList_GetIconSize(ListView_GetImageList(list,LVSIL_SMALL),&cx,&cy) && cx>=20 && cy>=20;
	PostMessageW(window,WM_COMMAND,IDOK,0); return FALSE;
}
static VOID CALLBACK PackageDialogTimer(HWND owner,UINT,UINT_PTR,DWORD)
{
	EnumThreadWindows(GetCurrentThreadId(),ClosePackagesDialog,reinterpret_cast<LPARAM>(owner));
}
static HRESULT StudioPackages(PCWSTR folder)
{
	HRESULT hr; TStackRef<CQuadooStudio> studio; TStackRef<IJSONObject> project,definition;
	TStackRef<IJSONValue> value; TStackRef<IQuadooComponent> component;
	TStackRef<IQuadooProperties> properties; TStackRef<IJSONArray> files;
	CRString directory,store,path,source,definitionPath,detail,text;
	WCHAR executable[MAX_PATH]; HWND window=NULL; CQuadooProject* ide=NULL; CProjectFile* file=NULL;
	QFD::CDesignerEditor* editor=NULL; GUID id; UINT property; HANDLE locked=INVALID_HANDLE_VALUE;
	Check(RStrFormatW(&directory,L"%ls\\PackageProject",folder)); CheckIf(!CreateDirectoryW(directory.c_wstr(),NULL),HrEnsureLastError());
	Check(RStrFormatW(&store,L"%ls\\PackageStore",folder)); Check(RStrFormatW(&path,L"%ls\\PackageApplication.qsproj",directory.c_wstr()));
	CheckIf(!GetModuleFileNameW(NULL,executable,ARRAYSIZE(executable)),HrEnsureLastError());
	PWSTR slash=const_cast<PWSTR>(TStrRChr(executable,L'\\')); CheckIf(!slash,E_UNEXPECTED); *slash=0;
	Check(RStrFormatW(&source,L"%ls\\DesignerExamples\\QuadooPlugin.json",executable));
	studio.Attach(__new CQuadooStudio(GetModuleHandle(NULL))); CheckAlloc(studio);
	Check(studio->Initialize(L"",1260,820,SW_SHOWNOACTIVATE,store.c_wstr()));
	Check(studio->GetWindow(&window));
	REQUIRE(GetMenuState(GetMenu(window),QFP::PackagesCommand,MF_BYCOMMAND) != static_cast<UINT>(-1));
	{ OLECMD command={QFP::PackagesCommand,0}; Check(studio->QueryStatus(NULL,1,&command,NULL)); REQUIRE(command.cmdf & OLECMDF_ENABLED); }
	Check(studio->Packages()->Install(source.c_wstr(),studio));
	g_packagesDialogVerified=FALSE; CheckIf(!SetTimer(window,5901,50,PackageDialogTimer),HrEnsureLastError());
	Check(studio->Exec(NULL,QFP::PackagesCommand,0,NULL,NULL)); KillTimer(window,5901); REQUIRE(g_packagesDialogVerified);
	Check(JSONCreateObject(&project)); Check(QFD::PutString(project,L"target",RSTRING_CAST(L"application")));
	Check(CreateApplicationFixture(project,path.c_wstr())); Check(studio->OpenProject(path.c_wstr())); Pump();
	ide=studio->Project(); editor=ide->ActiveDesigner(); REQUIRE(editor && editor->Document().Registry()==studio->Packages()->Registry());
	Check(ide->FindSource(RSTRING_CAST(L".\\MainForm.quadoo"),&file)); definitionPath=file->m_form->definition;
	{ LVFINDINFOW find; ZeroMemory(&find,sizeof(find)); find.flags=LVFI_STRING; find.psz=L"QuadooPlugin"; REQUIRE(ListView_FindItem(editor->ToolsWindow(),-1,&find)>=0); }
	Check(editor->AddComponent(RSTRING_CAST(L"QuadooPlugin"),&id)); Check(editor->Document().FindComponent(id,&component));
	Check(QFD::Property(component,L"Module",&properties,&property)); Check(JSONCreateString(RSTRING_CAST(L"AppModule.dll"),&value)); Check(editor->SetSelectedProperty(property,value)); value.Release(); properties.Release(); component.Release();
	REQUIRE(studio->Packages()->CanRemove(0,studio,detail)==S_FALSE); REQUIRE(studio->Packages()->Remove(0,studio)==HRESULT_FROM_WIN32(ERROR_BUSY));
	Check(ide->SetDesignView(FALSE)); REQUIRE(!ide->DesignerActive());
	{
		TStackRef<QFP::CDefinition> package; TStackRef<IJSONValue> copy,wrapped; TStackRef<IJSONObject> json,control;
		GUID identity; WCHAR guid[40]; CRString additional; CMemoryStream content;
		Check(QFP::LoadPackage(source.c_wstr(),TRUE,&package)); Check(JSONCloneObject(package->json,&copy,TRUE)); Check(copy->GetObject(&json));
		Check(CoCreateGuid(&identity)); StringFromGUID2(identity,guid,ARRAYSIZE(guid)); Check(QFD::PutString(json,L"packageId",RSTRING_CAST(guid)));
		Check(json->FindNonNullValueW(L"control",&wrapped)); Check(wrapped->GetObject(&control)); Check(QFD::PutString(control,L"type",RSTRING_CAST(L"CodeViewPackage")));
		Check(QFD::PutString(json,L"name",RSTRING_CAST(L"CodeViewPackage"))); Check(RStrFormatW(&additional,L"%ls\\CodeViewPackage.json",folder));
		Check(JSONSerializeObject(json,&content)); Check(Text::SaveToFile(content.TGetReadPtr<WCHAR>(),content.DataRemaining()/sizeof(WCHAR),additional.c_wstr()));
		Check(studio->Packages()->Install(additional.c_wstr(),studio)); Check(ide->SetDesignView(TRUE));
		REQUIRE(IsWindowVisible(editor->PaletteWindow()) && IsWindowVisible(editor->ToolsWindow()));
		UINT added=0; while(added<studio->Packages()->Count() && !(studio->Packages()->At(added)->type==RSTRING_CAST(L"CodeViewPackage"))) added++;
		REQUIRE(added<studio->Packages()->Count()); Check(studio->Packages()->Remove(added,studio)); Check(ide->SetDesignView(FALSE));
	}
	{ OLECMD command={QFP::PackagesCommand,0}; Check(studio->QueryStatus(NULL,1,&command,NULL)); REQUIRE(command.cmdf & OLECMDF_ENABLED); }
	REQUIRE(studio->Packages()->CanRemove(0,studio,detail)==S_FALSE); // unsaved definition in inactive Design view
	Check(studio->Exec(NULL,ID_FILE_SAVEALL,0,NULL,NULL)); Check(studio->Exec(NULL,ID_FILE_CLOSEPROJECT,0,NULL,NULL)); Pump();
	REQUIRE(studio->Packages()->CanRemove(0,studio,detail)==S_OK);
	// Persist a code-active project and reopen: the MainForm definition is used
	// by the project even when the GUI editor has not been instantiated.
	Check(studio->OpenProject(path.c_wstr())); Pump(); ide=studio->Project();
	Check(ide->FindSource(RSTRING_CAST(L".\\MainForm.quadoo"),&file)); file->m_form->Close();
	REQUIRE(!file->m_form->editor); REQUIRE(studio->Packages()->CanRemove(0,studio,detail)==S_FALSE);
	locked=CreateFileW(definitionPath.c_wstr(),GENERIC_READ,0,NULL,OPEN_EXISTING,0,NULL); CheckIf(locked==INVALID_HANDLE_VALUE,HrEnsureLastError());
	REQUIRE(FAILED(studio->Packages()->CanRemove(0,studio,detail))); REQUIRE(RStrLen(detail)>0);
	CloseHandle(locked); locked=INVALID_HANDLE_VALUE;
	Check(studio->Exec(NULL,ID_FILE_CLOSEPROJECT,0,NULL,NULL)); Pump();
	Check(studio->Packages()->Remove(0,studio)); REQUIRE(studio->Packages()->Count()==0);
	{ OLECMD command={QFP::PackagesCommand,0}; Check(studio->QueryStatus(NULL,1,&command,NULL)); REQUIRE(command.cmdf & OLECMDF_ENABLED); }
Cleanup:
	if(locked!=INVALID_HANDLE_VALUE) CloseHandle(locked);
	if(window) KillTimer(window,5901);
	if(studio && studio->Project()) { studio->Project()->CloseProject(FALSE); studio->Exec(NULL,ID_FILE_CLOSEPROJECT,0,NULL,NULL); }
	if(studio) studio->Destroy(); studio.Release(); Pump();
	return hr;
}
