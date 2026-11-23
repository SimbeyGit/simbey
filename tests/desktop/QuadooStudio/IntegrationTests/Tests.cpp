#include <winsock2.h>
#include "..\..\..\TestPaths.h"
#include <windows.h>
#include <commctrl.h>
#include <dbghelp.h>
#pragma comment(lib,"dbghelp.lib")
#include "Library\DPI.h"
#include "Library\Util\TextHelpers.h"
#include "..\..\..\..\desktop\QuadooStudio\QuadooStudio\QuadooStudio.h"
#include "..\..\..\..\desktop\QuadooStudio\QuadooStudio\QuadooProject.h"
#include "..\..\..\..\desktop\QuadooStudio\QuadooStudio\Splitter.h"
#include "..\..\..\..\desktop\QuadooStudio\QuadooStudio\QuadooCallStackDisplay.h"
#include "..\..\..\..\desktop\QuadooStudio\QuadooStudio\ApplicationProject.h"
#include "..\..\..\..\desktop\QuadooStudio\QuadooStudio\resource.h"
#include "..\..\..\platform\QuadooForm\TestExtension\ExternalControl.h"
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

static UINT g_checks;
static VOID Log(PCWSTR text)
{
	CHAR buffer[4096]; DWORD written;
	INT length = WideCharToMultiByte(CP_UTF8,0,text,-1,buffer,ARRAYSIZE(buffer),NULL,NULL);
	if(length) WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),buffer,length - 1,&written,NULL);
}
static LONG WINAPI TraceCrash(EXCEPTION_POINTERS* exception)
{
	HANDLE process = GetCurrentProcess();
	CONTEXT context = *exception->ContextRecord;
	STACKFRAME64 frame = {0};
#ifdef _WIN64
	DWORD machine = IMAGE_FILE_MACHINE_AMD64;
	frame.AddrPC.Offset = context.Rip; frame.AddrFrame.Offset = context.Rbp; frame.AddrStack.Offset = context.Rsp;
#else
	DWORD machine = IMAGE_FILE_MACHINE_I386;
	frame.AddrPC.Offset = context.Eip; frame.AddrFrame.Offset = context.Ebp; frame.AddrStack.Offset = context.Esp;
#endif
	frame.AddrPC.Mode = frame.AddrFrame.Mode = frame.AddrStack.Mode = AddrModeFlat;
	SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME); SymInitialize(process,NULL,TRUE);
	for(UINT n = 0; n < 24; n++)
	{
		BYTE buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {0};
		SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer); symbol->SizeOfStruct = sizeof(*symbol); symbol->MaxNameLen = MAX_SYM_NAME;
		DWORD64 displacement = 0; IMAGEHLP_LINE64 line = {sizeof(line)}; DWORD offset;
		CRString message;
		SymFromAddr(process,frame.AddrPC.Offset,&displacement,symbol);
		SymGetLineFromAddr64(process,frame.AddrPC.Offset,&offset,&line);
		if(SUCCEEDED(RStrFormatW(&message,L"crash %p %S at %S:%u\r\n",reinterpret_cast<PVOID>(static_cast<UINT_PTR>(frame.AddrPC.Offset)),symbol->Name,line.FileName ? line.FileName : "?",line.LineNumber))) Log(message.c_wstr());
		if(!StackWalk64(machine,process,GetCurrentThread(),&frame,&context,NULL,SymFunctionTableAccess64,SymGetModuleBase64,NULL)) break;
	}
	SymCleanup(process);
	return EXCEPTION_EXECUTE_HANDLER;
}
#ifdef _DEBUG
static VOID WINAPI TraceCheck(HRESULT hr, PCSTR file, UINT line)
{
	CRString message;
	if(SUCCEEDED(RStrFormatW(&message,L"check 0x%.8x at %S:%u\r\n",hr,file,line))) Log(message.c_wstr());
}
#endif
static BOOL Expect(BOOL condition, PCWSTR text, UINT line)
{
	g_checks++;
	if(!condition) { CRString message; if(SUCCEEDED(RStrFormatW(&message,L"FAILED %u: %ls\r\n",line,text))) Log(message.c_wstr()); }
	return condition;
}
#define WIDE_(s) L##s
#define WIDE(s) WIDE_(s)
#define REQUIRE(s) CheckIf(!Expect((s),WIDE(#s),__LINE__),E_FAIL)
#include "..\..\..\platform\QuadooForm\DesignerTests\ClipboardTests.h"
#include "Library\MenuUtil.h"
static VOID Pump()
{
	MSG message;
	while(PeekMessageW(&message,NULL,0,0,PM_REMOVE)) { if(message.message != WM_QUIT) { TranslateMessage(&message); DispatchMessageW(&message); } }
}
static VOID PumpInput()
{
	DWORD start = GetTickCount(), duration = GetDoubleClickTime() + 100;
	for(;;)
	{
		Pump(); DWORD elapsed = GetTickCount() - start;
		if(elapsed >= duration) break;
		MsgWaitForMultipleObjects(0,NULL,FALSE,duration - elapsed,QS_ALLINPUT);
	}
}
static INT ListRow(HWND list, PCWSTR text)
{
	LVFINDINFOW find = {0}; find.flags = LVFI_STRING; find.psz = text;
	return ListView_FindItem(list,-1,&find);
}
static HRESULT DesignerArrow(CQuadooStudio* studio, HWND target, UINT key, BOOL expected, BOOL resize = FALSE)
{
	HRESULT hr = S_OK;
	BYTE saved[256];
	BYTE keyboard[256] = {0};
	MSG message = {0};
	BOOL restore = FALSE;
	CheckIf(!GetKeyboardState(saved),HrEnsureLastError());
	keyboard[VK_CONTROL] = resize ? 0 : 0x80;
	keyboard[VK_SHIFT] = resize ? 0x80 : 0;
	CheckIf(!SetKeyboardState(keyboard),HrEnsureLastError());
	restore = TRUE;
	message.hwnd = target;
	message.message = WM_KEYDOWN;
	message.wParam = key;
	message.lParam = 1;
	REQUIRE(studio->PreTranslate(&message) == expected);
Cleanup:
	if(restore)
	{
		SetKeyboardState(saved);
	}
	return hr;
}
static HRESULT ReadJSON(PCWSTR path, IJSONObject** object)
{
	HRESULT hr;
	PWSTR text = NULL; INT length = 0;
	Check(Text::LoadFromFile(path,&text,&length,CP_UTF8));
	Check(QFD::Parse(text,length,object));
Cleanup:
	SafeDeleteArray(text);
	return hr;
}
static HRESULT ReadText(PCWSTR path, CRString& text)
{
	HRESULT hr;
	PWSTR loaded = NULL; INT length = 0;
	Check(Text::LoadFromFile(path,&loaded,&length,CP_UTF8));
	text = static_cast<RSTRING>(NULL);
	Check(RStrCreateW(length,loaded,&text));
Cleanup:
	SafeDeleteArray(loaded);
	return hr;
}
static BOOL Enabled(CQuadooProject* project, UINT command)
{
	OLECMD cmd = { command,0 };
	return SUCCEEDED(project->QueryStatus(NULL,1,&cmd,NULL)) && (cmd.cmdf & OLECMDF_ENABLED);
}
static HTREEITEM FindComponent(HWND tree, CProjectFile* file, REFGUID id)
{
	HTREEITEM item = TreeView_GetChild(tree,file->m_hItem);
	TArray<HTREEITEM> pending;
	while(item)
	{
		TVITEMW value = {0}; value.hItem = item; value.mask = TVIF_PARAM;
		if(TreeView_GetItem(tree,&value) && value.lParam)
		{
			CProjectTreeNode* node = reinterpret_cast<CProjectTreeNode*>(value.lParam);
			if(!node->source && node->file == file && IsEqualGUID(node->id,id)) return item;
		}
		HTREEITEM child = TreeView_GetChild(tree,item), sibling = TreeView_GetNextSibling(tree,item);
		if(sibling && FAILED(pending.Append(sibling))) return NULL;
		if(child) item = child;
		else if(pending.Length()) pending.Remove(pending.Length() - 1,&item);
		else item = NULL;
	}
	return NULL;
}
static HRESULT SetProperty(QFD::CDesignerEditor* editor, IQuadooComponent* component, PCWSTR name, IJSONValue* value)
{
	HRESULT hr;
	GUID id; TStackRef<IQuadooProperties> properties; UINT index;
	Check(component->GetID(&id));
	Check(editor->SelectID(id));
	Check(QFD::Property(component,name,&properties,&index));
	Check(editor->SetSelectedProperty(index,value));
Cleanup:
	return hr;
}
static HWND g_previewOwner, g_preview;
static BOOL CALLBACK ClosePreview(HWND window, LPARAM)
{
	WCHAR type[64]; GetClassNameW(window,type,ARRAYSIZE(type));
	if(!lstrcmpW(type,L"QuadooForm.Native.v2") && GetWindow(window,GW_OWNER) == g_previewOwner)
	{
		g_preview = window; SendMessageW(window,WM_CLOSE,0,0); return FALSE;
	}
	return TRUE;
}
static VOID CALLBACK PreviewTimer(HWND, UINT, UINT_PTR, DWORD) { EnumThreadWindows(GetCurrentThreadId(),ClosePreview,0); }
#include "ProjectCreationTests.h"
static HRESULT Integration(PCWSTR folder, BOOL keysOnly = FALSE)
{
	HRESULT hr;
	TStackRef<CQuadooStudio> studio;
	TStackRef<IJSONObject> project, saved, before, after;
	TStackRef<IQuadooControlContainer> root, panel;
	TStackRef<IQuadooControl> control, button;
	TStackRef<IQuadooControlFactory> external;
	TStackRef<IQuadooComponentFactory> service;
	TStackRef<IQuadooComponentRegistry> components;
	TStackRef<IJSONValue> value;
	TStackRef<IJSONArray> files;
	CRString projectPath, framePath, startupPath, beforeSource, afterSource, text, beforeDefinition, afterDefinition;
	HWND window = NULL, tree, status, palette;
	HANDLE locked = INVALID_HANDLE_VALUE;
	CQuadooProject* ide;
	CProjectFile *formFile, *frameFile;
	GUID panelID, buttonID, timerID, selected, rootID, externalID, serviceID;
	HTREEITEM formItem, panelItem;
	QuadooForm::Rect bounds = { 20,25,280,150 };
	Check(RStrFormatW(&projectPath,L"%ls\\Application.qsproj",folder));
	Check(RStrFormatW(&framePath,L"%ls\\DetailsFrame.quadoo",folder));
	Check(JSONCreateObject(&project));
	Check(QFD::PutString(project,L"target",RSTRING_CAST(L"application")));
	Check(QFD::PutString(project,L"args",RSTRING_CAST(L"")));
	Check(QFD::PutString(project,L"startDir",RSTRING_CAST(L".\\")));
	Check(CreateApplicationFixture(project,projectPath.c_wstr()));
	Check(RStrFormatW(&startupPath,L"%ls\\Main.quadoo",folder));
	REQUIRE(GetFileAttributesW(startupPath.c_wstr()) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND);
	{
		TStackRef<IJSONValue> arrayValue, tag, defaultValue;
		bool defaultForm = false;
		TStackRef<IJSONArray> entries;
		TStackRef<IJSONObject> entry;
		Check(project->FindNonNullValueW(L"files",&arrayValue));
		Check(arrayValue->GetArray(&entries));
		REQUIRE(entries->Count() == 1);
		Check(entries->GetObject(0,&entry));
		Check(entry->FindNonNullValueW(L"default",&defaultValue));
		Check(defaultValue->GetBoolean(&defaultForm));
		REQUIRE(defaultForm);
		{ CRString source; TStackRef<IJSONValue> path;
			Check(entry->FindNonNullValueW(L"path",&path));
			Check(path->GetString(&source));
			REQUIRE(source == RSTRING_CAST(L".\\MainForm.quadoo"));
		}
		Check(JSONCreateInteger(77,&tag));
		Check(entry->AddValueW(L"customMetadata",tag));
		Check(SaveStudioProject(project,projectPath.c_wstr()));
	}
	studio.Attach(__new CQuadooStudio(GetModuleHandle(NULL)));
	CheckAlloc(studio);
	{ CRString packageDirectory; Check(RStrFormatW(&packageDirectory,L"%ls\\Packages",folder));
	Check(studio->Initialize(L"",1260,820,SW_SHOWNOACTIVATE,packageDirectory.c_wstr())); }
	Check(studio->GetWindow(&window));
	Check(studio->OpenProject(projectPath.c_wstr()));
	Pump(); ide = studio->Project();
	REQUIRE(ide && ide->IsApplication() && ide->DesignerActive());
	tree = GetDlgItem(window,IDC_PROJECT_FILES); status = GetDlgItem(window,ID_STATUS_BAR);
	REQUIRE(tree && status && TreeView_GetCount(tree) == 3);
	{ CProjectFile* startup = NULL;
		REQUIRE(ide->FindSource(RSTRING_CAST(L".\\Main.quadoo"),&startup) == HRESULT_FROM_WIN32(ERROR_NOT_FOUND) && !startup);
	}
	Check(ide->FindSource(RSTRING_CAST(L".\\MainForm.quadoo"),&formFile));
	REQUIRE(formFile->m_fDefault && formFile->m_form && !lstrcmpW(formFile->m_form->name.c_wstr(),L"MainForm"));
	value.Release();
	Check(formFile->m_form->metadata->FindNonNullValueW(L"class",&value));
	text = static_cast<RSTRING>(NULL);
	Check(value->GetString(&text));
	REQUIRE(text == RSTRING_CAST(L"MainForm"));
	value.Release();
	Check(formFile->m_form->metadata->FindNonNullValueW(L"eventsInterface",&value));
	text = static_cast<RSTRING>(NULL);
	Check(value->GetString(&text));
	REQUIRE(text == RSTRING_CAST(L"IMainForm"));
	value.Release();
	Check(ReadText(formFile->m_pwzAbsolutePath,text));
	REQUIRE(wcsstr(text.c_wstr(),L"class MainForm : extern(Form), IMainForm") != NULL);
	QFD::CDesignerEditor* editor = ide->ActiveDesigner();
	Check(editor->Document().Root()->GetID(&rootID));
	palette = editor->PaletteWindow();
	REQUIRE(!editor->TreeWindow() && GetParent(palette) == window && IsWindowVisible(palette));
	REQUIRE(GetParent(editor->ToolsWindow()) == palette);
	{ RECT item; PumpInput();
	REQUIRE(ListView_GetItemRect(editor->ToolsWindow(),0,&item,LVIR_BOUNDS) && item.top >= 0);
	}
	formItem = formFile->m_hItem;
	REQUIRE(TreeView_GetParent(tree,FindComponent(tree,formFile,rootID)) == formItem);
	REQUIRE(Enabled(ide,ID_RUN_SCRIPT) && Enabled(ide,ID_DEBUG_SCRIPT) && !Enabled(ide,ID_PROJECT_COMPILE));
	REQUIRE(wcsstr(text.c_wstr(),L"Form = Form.Create(this);") && wcsstr(text.c_wstr(),L"property __QuadooRoot"));
	REQUIRE(ide->Exec(NULL,ID_PROJECT_COMPILE,0,NULL,NULL) == E_NOTIMPL);
	REQUIRE(Enabled(ide,QFD::Preview) && !Enabled(ide,QFD::DeleteSelection));
	Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root)));
	Check(editor->AddControl(RSTRING_CAST(L"Panel"),root,&bounds,&panelID));
	Check(editor->Document().Find(panelID,&control));
	Check(control->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&panel)));
	control.Release();
	bounds.Left = 12; bounds.Top = 18; bounds.Width = 90; bounds.Height = 26;
	Check(editor->AddControl(RSTRING_CAST(L"Button"),panel,&bounds,&buttonID));
	Check(editor->Document().Find(buttonID,&button));
	Check(editor->AddComponent(RSTRING_CAST(L"Timer"),&timerID));
	Pump();
	panelItem = FindComponent(tree,formFile,panelID);
	REQUIRE(TreeView_GetCount(tree) == 6 && formFile->m_hItem == formItem);
	REQUIRE(TreeView_GetParent(tree,FindComponent(tree,formFile,buttonID)) == panelItem);
	REQUIRE(TreeView_GetParent(tree,FindComponent(tree,formFile,timerID)) == FindComponent(tree,formFile,rootID));
	REQUIRE(TreeView_GetItemState(tree,FindComponent(tree,formFile,rootID),TVIS_EXPANDED) != 0);
	// Exercise the relocated palette's real notification route and delayed
	// list-view scrolling, including a scrolled nonvisual selection.
	{
		HWND tools = editor->ToolsWindow(); RECT position, before, after;
		INT row = ListRow(tools,L"Timer");
		GetWindowRect(palette,&position); MapWindowPoints(NULL,window,reinterpret_cast<POINT*>(&position),2);
		MoveWindow(palette,position.left,position.top,position.right - position.left,145,TRUE);
		SetFocus(tools);
		REQUIRE(row >= 0 && ListView_EnsureVisible(tools,row,FALSE));
		REQUIRE(ListView_GetItemRect(tools,0,&before,LVIR_BOUNDS) && before.top < 0);
		INT top = ListView_GetTopIndex(tools);
		RECT item;
		REQUIRE(ListView_GetItemRect(tools,row,&item,LVIR_BOUNDS));
		LPARAM point = MAKELPARAM((item.left + item.right) / 2,(item.top + item.bottom) / 2);
		REQUIRE(PostMessageW(tools,WM_LBUTTONDOWN,MK_LBUTTON,point));
		REQUIRE(PostMessageW(tools,WM_LBUTTONUP,0,point));
		PumpInput();
		REQUIRE(ListView_GetItemCount(editor->TrayWindow()) == 2 && TreeView_GetCount(tree) == 7);
		REQUIRE(ListView_GetItemRect(tools,0,&after,LVIR_BOUNDS) && before.top == after.top && top == ListView_GetTopIndex(tools));
		REQUIRE(ListView_GetNextItem(tools,-1,LVNI_SELECTED) == 0 && ListView_GetNextItem(tools,-1,LVNI_FOCUSED) == row);
		Check(editor->Delete());
		Pump();
		REQUIRE(ListView_GetItemCount(editor->TrayWindow()) == 1 && TreeView_GetCount(tree) == 6);
		MoveWindow(palette,position.left,position.top,position.right - position.left,position.bottom - position.top,TRUE);
	}
	TreeView_SelectItem(tree,FindComponent(tree,formFile,buttonID)); Pump();
	Check(editor->GetSelectedID(&selected));
	REQUIRE(IsEqualGUID(selected,buttonID) && Enabled(ide,QFD::DeleteSelection));
	{
		QuadooForm::Rect before, after;
		Check(button->GetBounds(&before));
		Check(studio->Exec(NULL,ID_FILE_SAVEFILE,0,NULL,NULL));
		REQUIRE(!editor->Document().Dirty());
		Check(DesignerArrow(studio,editor->OverlayWindow(),VK_RIGHT,TRUE));
		Check(button->GetBounds(&after));
		REQUIRE(after.Left == before.Left + 1 && after.Top == before.Top && editor->Document().Dirty());
		Check(DesignerArrow(studio,tree,VK_DOWN,TRUE));
		Check(button->GetBounds(&after));
		REQUIRE(after.Left == before.Left + 1 && after.Top == before.Top + 1);
		Check(DesignerArrow(studio,editor->ToolsWindow(),VK_LEFT,FALSE));
		Check(button->GetBounds(&after));
		REQUIRE(after.Left == before.Left + 1 && after.Top == before.Top + 1);
		Check(DesignerArrow(studio,editor->OverlayWindow(),VK_RIGHT,TRUE,TRUE));
		Check(DesignerArrow(studio,tree,VK_DOWN,TRUE,TRUE));
		Check(DesignerArrow(studio,editor->ToolsWindow(),VK_LEFT,FALSE,TRUE));
		Check(button->GetBounds(&after));
		REQUIRE(after.Width == before.Width + 1 && after.Height == before.Height + 1 && after.Left == before.Left + 1 && after.Top == before.Top + 1);
		Pump();
		REQUIRE(formFile->IsModified() && TreeView_GetSelection(tree) == FindComponent(tree,formFile,buttonID));
	}
	if(keysOnly)
	{
		QuadooForm::Rect previous;
		QuadooForm::Rect unchanged;
		Check(button->GetBounds(&previous));
		Check(studio->Exec(NULL,ID_FILE_SAVEFILE,0,NULL,NULL));
		Check(ide->SetDesignView(FALSE));
		Pump();
		REQUIRE(!ide->DesignerActive() && ide->ActiveSourceEditor());
		Check(DesignerArrow(studio,editor->OverlayWindow(),VK_RIGHT,FALSE,TRUE));
		Check(DesignerArrow(studio,editor->OverlayWindow(),VK_DOWN,FALSE));
		Check(ide->SetDesignView(TRUE));
		Pump();
		Check(button->GetBounds(&unchanged));
		REQUIRE(previous.Left == unchanged.Left && previous.Top == unchanged.Top && previous.Width == unchanged.Width && previous.Height == unchanged.Height);
		hr = S_OK;
		goto Cleanup;
	}
	// Shared command IDs, clipboard shortcuts and tree creation order must work
	// through Studio's command target, including the popup checked states.
	{
		CTestClipboard clipboard;
		GUID pasted;
		Check(clipboard.Save(window));
		Check(editor->SelectID(buttonID));
		HMENU popup = QFD::CreateControlMenu();
		REQUIRE(popup != NULL);
		HRESULT state = MenuUtil::EnableMenuItems(NULL,popup,ide);
		BOOL checked = (GetMenuState(popup,QFD::ToggleEnabled,MF_BYCOMMAND) & MF_CHECKED) != 0;
		BOOL tabDisabled = (GetMenuState(popup,QFD::TabOrder,MF_BYCOMMAND) & MF_GRAYED) != 0;
		DestroyMenu(popup);
		Check(state);
		REQUIRE(checked && tabDisabled);
		Check(studio->Exec(NULL,ID_FORM_ENABLED,0,NULL,NULL));
		REQUIRE(!editor->IsCommandChecked(QFD::ToggleEnabled));
		Check(studio->Exec(NULL,ID_FORM_ENABLED,0,NULL,NULL));
		Check(DesignerArrow(studio,tree,'C',TRUE));
		button.Release(); panel.Release(); root.Release();
		Check(studio->Exec(NULL,ID_FORM_PASTE,0,NULL,NULL));
		Check(editor->GetSelectedID(&pasted));
		Pump();
		REQUIRE(!IsEqualGUID(pasted,buttonID));
		REQUIRE(TreeView_GetParent(tree,FindComponent(tree,formFile,pasted)) == FindComponent(tree,formFile,panelID));
		Check(studio->Exec(NULL,ID_FORM_BACK,0,NULL,NULL));
		Pump();
		REQUIRE(TreeView_GetChild(tree,FindComponent(tree,formFile,panelID)) == FindComponent(tree,formFile,pasted));
		Check(studio->Exec(NULL,ID_FORM_FRONT,0,NULL,NULL));
		Pump();
		REQUIRE(TreeView_GetNextSibling(tree,FindComponent(tree,formFile,pasted)) == NULL);
		Check(studio->Exec(NULL,ID_FORM_CUT,0,NULL,NULL));
		Pump();
		REQUIRE(!FindComponent(tree,formFile,pasted));
		Check(DesignerArrow(studio,tree,'V',TRUE));
		Pump();
		Check(editor->GetSelectedID(&pasted));
		REQUIRE(FindComponent(tree,formFile,pasted) != NULL);
		Check(studio->Exec(NULL,ID_FORM_DELETE,0,NULL,NULL));
		Pump();
		Check(editor->SelectID(buttonID));
		Check(editor->Document().Find(buttonID,&button));
		Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root)));
		Check(editor->Document().Find(panelID,&control));
		Check(control->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&panel)));
		control.Release();
		panelItem = FindComponent(tree,formFile,panelID);
		REQUIRE(formFile->m_hItem == formItem);
	}
	Check(JSONCreateString(RSTRING_CAST(L"RenamedButton"),&value));
	Check(SetProperty(editor,button,L"Name",value));
	value.Release(); Pump();
	REQUIRE(panelItem == FindComponent(tree,formFile,panelID));
	{
		HWND list = GetDlgItem(editor->Window(),104);
		NMITEMACTIVATE click = {0}; click.hdr.hwndFrom = list; click.hdr.idFrom = 104; click.hdr.code = NM_DBLCLK; click.iItem = ListRow(list,L"Name");
		REQUIRE(click.iItem >= 0);
		SendMessageW(editor->Window(),WM_NOTIFY,104,reinterpret_cast<LPARAM>(&click));
		HWND edit = GetDlgItem(list,120);
		REQUIRE(IsWindow(edit));
		SetWindowTextW(edit,L"Panel1");
		REQUIRE(ide->SetDesignView(FALSE) == QuadooForm::EDuplicateName);
		REQUIRE(ide->DesignerActive() && IsWindowVisible(palette) && !ide->ActiveSourceEditor());
		SendMessageW(edit,WM_KEYDOWN,VK_ESCAPE,0); Pump();
	}
	Check(studio->Exec(NULL,ID_FILE_SAVEFILE,0,NULL,NULL));
	REQUIRE(!editor->Document().Dirty());
	Check(ide->SetDesignView(FALSE));
	Pump();
	REQUIRE(!ide->DesignerActive() && !ide->ActivePalette() && !IsWindowVisible(palette) && ide->ActiveSourceEditor());
	Check(DesignerArrow(studio,editor->OverlayWindow(),VK_RIGHT,FALSE));
	REQUIRE(!Enabled(ide,QFD::Preview) && !Enabled(ide,QFD::Grid) && !Enabled(ide,QFD::Paste) && !Enabled(ide,QFD::Copy) && Enabled(ide,ID_VIEW_DESIGN));
	{ RECT rect, client, bar; GetWindowRect(tree,&rect); GetClientRect(window,&client); GetWindowRect(status,&bar);
	REQUIRE(rect.bottom - rect.top == client.bottom - (bar.bottom - bar.top));
	}
	{ ITextDocument* source = ide->ActiveSourceEditor()->GetTextDocument();
		Check(source->Insert(source->Size(),SLP(L"\r\n// unsaved source survives Design\r\n")));
	}
	Check(ide->SetDesignView(TRUE));
	Pump();
	REQUIRE(ide->ActiveDesigner() == editor && IsWindowVisible(palette));
	Check(JSONCreateInteger(123,&value));
	Check(SetProperty(editor,button,L"Width",value));
	value.Release(); Pump();
	Check(ReadText(formFile->m_pwzAbsolutePath,beforeSource));
	Check(ReadText(formFile->m_form->definition.c_wstr(),beforeDefinition));
	locked = CreateFileW(formFile->m_form->definition.c_wstr(),GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
	REQUIRE(locked != INVALID_HANDLE_VALUE);
	REQUIRE(FAILED(studio->Exec(NULL,ID_FILE_SAVEFILE,0,NULL,NULL)));
	REQUIRE(editor->Document().Dirty() && formFile->IsModified());
	Check(ReadText(formFile->m_pwzAbsolutePath,afterSource));
	Check(ReadText(formFile->m_form->definition.c_wstr(),afterDefinition));
	REQUIRE(!lstrcmpW(beforeSource.c_wstr(),afterSource.c_wstr()) && !lstrcmpW(beforeDefinition.c_wstr(),afterDefinition.c_wstr()));
	CloseHandle(locked); locked = INVALID_HANDLE_VALUE;
	Check(studio->Exec(NULL,ID_FILE_SAVEFILE,0,NULL,NULL));
	REQUIRE(!editor->Document().Dirty() && !formFile->IsModified());
	Check(ReadText(formFile->m_pwzAbsolutePath,text));
	REQUIRE(wcsstr(text.c_wstr(),L"unsaved source survives Design") != NULL);
	Check(editor->Document().Serialize(&before));
	Check(ide->NewFormUnit(FALSE,framePath.c_wstr()));
	Pump();
	Check(ide->FindSource(RSTRING_CAST(L".\\DetailsFrame.quadoo"),&frameFile));
	REQUIRE(ide->DesignerActive() && !ide->ActiveDesigner()->Document().IsForm() && !Enabled(ide,QFD::Preview));
	Check(frameFile->m_form->metadata->FindNonNullValueW(L"class",&value));
	text = static_cast<RSTRING>(NULL);
	Check(value->GetString(&text));
	value.Release();
	REQUIRE(text == RSTRING_CAST(L"DetailsFrame"));
	REQUIRE(FAILED(ide->NewFormUnit(FALSE,framePath.c_wstr())));
	{
		CRString subdirectory, duplicate;
		Check(RStrFormatW(&subdirectory,L"%ls\\Other",folder));
		CheckIf(!CreateDirectoryW(subdirectory.c_wstr(),NULL),HrEnsureLastError());
		Check(RStrFormatW(&duplicate,L"%ls\\DetailsFrame.quadoo",subdirectory.c_wstr()));
		REQUIRE(ide->NewFormUnit(FALSE,duplicate.c_wstr()) == E_INVALIDARG);
		REQUIRE(GetFileAttributesW(duplicate.c_wstr()) == INVALID_FILE_ATTRIBUTES);
	}
	REQUIRE(formItem == formFile->m_hItem && panelItem == FindComponent(tree,formFile,panelID));
	TreeView_SelectItem(tree,FindComponent(tree,formFile,buttonID)); Pump();
	REQUIRE(ide->ActiveDesigner() == editor && !IsWindowVisible(frameFile->m_form->editor->PaletteWindow()));
	{
		UINT_PTR oldSession = frameFile->m_form->session;
		frameFile->m_form->Close();
		HWND projectWindow;
		Check(ide->GetWindow(&projectWindow));
		PostMessageW(projectWindow,QFD::ChangedMessage,oldSession,0); Pump();
		REQUIRE(frameFile->m_form->session != oldSession && ide->ActiveDesigner() == editor);
	}
	{ WCHAR statusText[128]; SendMessageW(status,SB_GETTEXTW,3,reinterpret_cast<LPARAM>(statusText));
	REQUIRE(!lstrcmpW(statusText,L"Zoom 100%"));
	}
	Check(editor->SelectID(rootID));
	Check(JSONCreateString(RSTRING_CAST(L"NoScriptAdapterRequired"),&value));
	Check(SetProperty(editor,root,L"OnCreate",value));
	value.Release();
	g_previewOwner = window; g_preview = NULL;
	REQUIRE(SetTimer(window,5001,20,PreviewTimer) != 0);
	Check(ide->Exec(NULL,QFD::Preview,0,NULL,NULL));
	KillTimer(window,5001);
	REQUIRE(g_preview && !IsWindow(g_preview) && IsWindowEnabled(window));
	Check(studio->Exec(NULL,ID_FILE_SAVEALL,0,NULL,NULL));
	// External factories use the same outline and artwork path as built-ins.
	Check(CreateExternalFactory(&external));
	Check(CreateExternalComponentFactory(&service));
	Check(ide->FormRegistry()->RegisterFactory(external));
	Check(ide->FormRegistry()->QueryInterface(__uuidof(IQuadooComponentRegistry),reinterpret_cast<PVOID*>(&components)));
	Check(components->RegisterComponentFactory(service));
	Check(editor->AddControl(RSTRING_CAST(L"ExternalMeter"),root,&bounds,&externalID));
	Check(editor->AddComponent(RSTRING_CAST(L"ExternalService"),&serviceID));
	Pump();
	REQUIRE(FindComponent(tree,formFile,externalID) && FindComponent(tree,formFile,serviceID));
	{
		TVITEMW item = {0}; item.mask = TVIF_IMAGE; item.hItem = FindComponent(tree,formFile,externalID);
		REQUIRE(TreeView_GetItem(tree,&item) && item.iImage >= 3);
		item.hItem = FindComponent(tree,formFile,serviceID);
		REQUIRE(TreeView_GetItem(tree,&item) && item.iImage >= 3);
	}
	Check(editor->SelectID(externalID));
	Check(editor->Delete());
	Check(editor->SelectID(serviceID));
	Check(editor->Delete());
	Pump();
	REQUIRE(!FindComponent(tree,formFile,externalID) && !FindComponent(tree,formFile,serviceID));
	Check(studio->Exec(NULL,ID_FILE_SAVEALL,0,NULL,NULL));
	// The host-wide package registry survives project closure. Registrations
	// made by this fixture must be released explicitly before unloading its DLL.
	Check(components->UnregisterComponentFactory(RSTRING_CAST(L"ExternalService")));
	Check(ide->FormRegistry()->UnregisterFactory(RSTRING_CAST(L"ExternalMeter")));
	components.Release(); external.Release(); service.Release();
	panel.Release(); root.Release(); button.Release();
	Check(studio->Exec(NULL,ID_FILE_CLOSEPROJECT,0,NULL,NULL));
	Pump();
	REQUIRE(!studio->Project() && !IsWindow(palette) && TreeView_GetCount(tree) == 0);
	Check(UnloadExternalExtension());
	Check(studio->OpenProject(projectPath.c_wstr()));
	Pump(); ide = studio->Project();
	Check(ide->FindSource(RSTRING_CAST(L".\\MainForm.quadoo"),&formFile));
	REQUIRE(formFile->m_fDefault);
	formFile->m_form->design = TRUE;
	Check(ide->SwitchToFile(formFile));
	Pump(); editor = ide->ActiveDesigner();
	Check(editor->Document().Find(buttonID,&button));
	REQUIRE(QFD::IntProperty(button,L"Width") == 123);
	text = static_cast<RSTRING>(NULL);
	Check(QFD::StringProperty(button,L"Name",&text));
	REQUIRE(text == RSTRING_CAST(L"RenamedButton"));
	Check(ReadJSON(projectPath.c_wstr(),&saved));
	Check(saved->FindNonNullValueW(L"files",&value));
	Check(value->GetArray(&files));
	value.Release();
	REQUIRE(files->Count() == 2);
	{
		TStackRef<IJSONObject> entry;
		BOOL retained = FALSE;
		for(sysint n = 0; n < files->Count(); n++)
		{
			entry.Release(); value.Release();
			Check(files->GetObject(n,&entry));
			if(SUCCEEDED(entry->FindNonNullValueW(L"customMetadata",&value)))
			{
				INT tag;
				Check(value->GetInteger(&tag));
				retained = tag == 77;
			}
		}
		value.Release();
		REQUIRE(retained);
	}
	// A second writer must never be silently overwritten by a paired save.
	{
		CRString externalText, originalSource, disk;
		Check(JSONCreateInteger(124,&value));
		Check(SetProperty(editor,button,L"Width",value));
		value.Release();
		Check(ReadText(formFile->m_form->definition.c_wstr(),beforeDefinition));
		Check(RStrFormatW(&externalText,L"%ls\r\n",beforeDefinition.c_wstr()));
		Check(Text::SaveToFile(externalText.c_wstr(),RStrLen(externalText),formFile->m_form->definition.c_wstr()));
		REQUIRE(studio->Exec(NULL,ID_FILE_SAVEALL,0,NULL,NULL) == HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH) && editor->Document().Dirty());
		Check(ReadText(formFile->m_form->definition.c_wstr(),disk));
		REQUIRE(disk == static_cast<RSTRING>(externalText));
		Check(Text::SaveToFile(beforeDefinition.c_wstr(),RStrLen(beforeDefinition),formFile->m_form->definition.c_wstr()));
		Check(ReadText(formFile->m_pwzAbsolutePath,originalSource));
		externalText = static_cast<RSTRING>(NULL);
		Check(RStrFormatW(&externalText,L"%ls\r\n// External edit\r\n",originalSource.c_wstr()));
		Check(Text::SaveToFile(externalText.c_wstr(),RStrLen(externalText),formFile->m_pwzAbsolutePath));
		REQUIRE(studio->Exec(NULL,ID_FILE_SAVEALL,0,NULL,NULL) == HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH) && editor->Document().Dirty());
		Check(ReadText(formFile->m_pwzAbsolutePath,disk));
		REQUIRE(disk == static_cast<RSTRING>(externalText));
		Check(Text::SaveToFile(originalSource.c_wstr(),RStrLen(originalSource),formFile->m_pwzAbsolutePath));
		Check(studio->Exec(NULL,ID_FILE_SAVEALL,0,NULL,NULL));
		REQUIRE(!editor->Document().Dirty());
	}
	Check(ide->SetDesignView(FALSE));
	Pump();
	Check(ReadText(formFile->m_form->definition.c_wstr(),beforeDefinition));
	formFile->m_form->Close();
	Check(Text::SaveToFile(L"{ invalid",9,formFile->m_form->definition.c_wstr()));
	REQUIRE(FAILED(ide->SetDesignView(TRUE)) && !ide->DesignerActive() && ide->ActiveSourceEditor());
	Check(Text::SaveToFile(beforeDefinition.c_wstr(),RStrLen(beforeDefinition),formFile->m_form->definition.c_wstr()));
	Check(ide->SetDesignView(TRUE));
	Pump();
	button.Release();
	// Remove retains both files. The project must not acquire a redundant
	// application.mainForm cache when the associated Form is removed.
	{
		VARIANT selection;
		CRString sourcePath, definitionPath;
		Check(RStrCreateW(TStrLenAssert(formFile->m_pwzAbsolutePath),formFile->m_pwzAbsolutePath,&sourcePath));
		definitionPath = formFile->m_form->definition;
		VariantInit(&selection);
		selection.vt = VT_BYREF;
		selection.byref = formFile->m_hItem;
		Check(ide->Exec(NULL,ID_CONTEXT_REMOVE,0,&selection,NULL));
		Pump();
		saved.Release();
		Check(ReadJSON(projectPath.c_wstr(),&saved));
		TStackRef<IJSONObject> application;
		Check(saved->FindNonNullValueW(L"application",&value));
		Check(value->GetObject(&application));
		value.Release();
		REQUIRE(FAILED(application->FindNonNullValueW(L"mainForm",&value)));
		REQUIRE(GetFileAttributesW(sourcePath.c_wstr()) != INVALID_FILE_ATTRIBUTES && GetFileAttributesW(definitionPath.c_wstr()) != INVALID_FILE_ATTRIBUTES);
		for(UINT n = 0; n < 8 && Enabled(ide,ID_CLOSE_TAB); n++)
		{
			Check(ide->Exec(NULL,ID_CLOSE_TAB,0,NULL,NULL));
			Pump();
		}
		WCHAR zoom[128];
		SendMessageW(status,SB_GETTEXTW,3,reinterpret_cast<LPARAM>(zoom));
		REQUIRE(!ide->ActivePalette() && !zoom[0]);
		REQUIRE(ide->Exec(NULL,ID_FILE_SAVEFILE,0,NULL,NULL) == OLECMDERR_E_DISABLED);
	}
	Check(studio->Exec(NULL,ID_FILE_CLOSEPROJECT,0,NULL,NULL));
	Pump();
	// Ordinary project types retain source-only trees and their existing command
	// availability. Opening them cannot leave a designer palette attached.
	{
		CRString legacySource;
		static const WCHAR source[] = L"var legacy = 1;\r\n";
		Check(RStrFormatW(&legacySource,L"%ls\\LegacyMain.quadoo",folder));
		Check(Text::SaveToFile(source,ARRAYSIZE(source) - 1,legacySource.c_wstr()));
	}
	for(UINT n = 0; n < 3; n++)
	{
		TStackRef<IJSONObject> legacy, entry;
		TStackRef<IJSONArray> entries;
		TStackRef<IJSONValue> json;
		CRString path;
		Check(JSONCreateObject(&legacy));
		Check(QFD::PutString(legacy,L"target",RSTRING_CAST(n == 0 ? L"qvm" : n == 1 ? L"wqvm" : L"web")));
		Check(JSONCreateObject(&entry));
		Check(QFD::PutString(entry,L"path",RSTRING_CAST(L".\\LegacyMain.quadoo")));
		Check(JSONCreateBool(true,&json));
		Check(entry->AddValueW(L"default",json));
		json.Release();
		Check(JSONCreateArray(&entries));
		Check(JSONWrapObject(entry,&json));
		Check(entries->Add(json));
		json.Release();
		Check(JSONWrapArray(entries,&json));
		Check(legacy->AddValueW(L"files",json));
		Check(RStrFormatW(&path,L"%ls\\Legacy%u.qsproj",folder,n));
		Check(SaveStudioProject(legacy,path.c_wstr()));
		Check(studio->OpenProject(path.c_wstr()));
		Pump(); ide = studio->Project();
		REQUIRE(!ide->IsApplication() && !ide->DesignerActive() && !ide->ActivePalette() && ide->ActiveSourceEditor());
		REQUIRE(TreeView_GetCount(tree) == 2 && !Enabled(ide,QFD::Preview) && !Enabled(ide,QFD::Grid));
		REQUIRE(Enabled(ide,ID_RUN_SCRIPT));
		REQUIRE(Enabled(ide,ID_PROJECT_COMPILE) == (n != 2));
		Check(studio->Exec(NULL,ID_FILE_CLOSEPROJECT,0,NULL,NULL));
		Pump();
	}
	hr = S_OK;
Cleanup:
	if(locked != INVALID_HANDLE_VALUE) CloseHandle(locked);
	if(window) KillTimer(window,5001);
	components.Release(); external.Release(); service.Release(); panel.Release(); root.Release(); control.Release(); button.Release();
	if(studio && studio->Project()) { studio->Project()->CloseProject(FALSE); studio->Exec(NULL,ID_FILE_CLOSEPROJECT,0,NULL,NULL); }
	if(studio) studio->Destroy(); studio.Release(); Pump();
	return hr;
}
#include "PackageTests.h"
#include "SourceSyncTests.h"
#include "SettingsTests.h"
#include "ApplicationTests.h"
#include "MenuTests.h"
#include "ImageListTests.h"
#include "TreeTests.h"
#include "ListViewTests.h"
#include "ImageButtonTests.h"
#include "ProgressBarTests.h"
#include "DateTimePickerTests.h"
#include "CalendarTests.h"
#include "StatusBarTests.h"
#include "CanvasTests.h"
#include "MultiSelectionTests.h"
int wmain(int count, WCHAR** arguments)
{
	HRESULT hr;
	#ifdef _DEBUG
	for(INT traceArg = 1; traceArg < count; traceArg++) if(!lstrcmpW(arguments[traceArg],L"--trace")) _SetAssertCallback(TraceCheck);
	#else
	UNREFERENCED_PARAMETER(count);
	UNREFERENCED_PARAMETER(arguments);
	#endif
	SetUnhandledExceptionFilter(TraceCrash); SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
	HINSTANCE instance = GetModuleHandle(NULL);
	WCHAR temp[MAX_PATH], folder[MAX_PATH] = {0};
	CRString message;
	INITCOMMONCONTROLSEX controls = { sizeof(controls),ICC_WIN95_CLASSES };
	DPI::SetDPIAware(); DPI::Initialize(); OleInitialize(NULL); InitCommonControlsEx(&controls);
	CodeEditRegister(); CSplitter::Register(instance); CQuadooProject::Register(instance); CQuadooStudio::Register(instance); CQuadooCallStackDisplay::Register(instance);
	Check(TestScratchDirectory(L"QuadooStudio",temp,ARRAYSIZE(temp)));
	CheckIf(!GetTempFileNameW(temp,L"QSI",0,folder),HrEnsureLastError());
	CheckIf(!DeleteFileW(folder) || !CreateDirectoryW(folder,NULL),HrEnsureLastError());
	Log(L"Studio Application embedding, shared tree/status, views and persistence...\r\n");
	if(count > 1 && !lstrcmpW(arguments[1],L"--creation-only"))
	{
		Check(ApplicationProjectCreation(folder));
	}
	else if(count > 1 && !lstrcmpW(arguments[1],L"--canvas-only"))
	{
		Check(StudioCanvases(folder));
		Check(StudioCanvasExample(folder));
	}
	else if(count > 1 && !lstrcmpW(arguments[1],L"--status-bar-only"))
	{
		Check(StudioStatusBars(folder));
	}
	else if(count > 1 && !lstrcmpW(arguments[1],L"--progress-bar-only"))
	{
		Check(StudioProgressBars(folder));
	}
	else if(count > 1 && !lstrcmpW(arguments[1],L"--calendar-only"))
	{
		Check(StudioCalendars(folder));
	}
	else if(count > 1 && !lstrcmpW(arguments[1],L"--datetime-picker-only"))
	{
		Check(StudioDateTimePickers(folder));
	}
	else if(count > 1 && !lstrcmpW(arguments[1],L"--image-button-only"))
	{
		Check(StudioImageButtons(folder));
	}
	else if(count > 1 && !lstrcmpW(arguments[1],L"--selection-only"))
	{
		Check(StudioMultiSelection(folder));
	}
	else if(count > 1 && !lstrcmpW(arguments[1],L"--designer-keys-only"))
	{
		Check(StudioMultiSelection(folder));
		Check(Integration(folder,TRUE));
	}
	else if(count > 1 && !lstrcmpW(arguments[1],L"--listview-only"))
	{
		Check(StudioListViews(folder));
	}
	else if(count > 1 && !lstrcmpW(arguments[1],L"--tree-only"))
	{
		Check(StudioTrees(folder));
	}
	else if(count > 1 && !lstrcmpW(arguments[1],L"--image-list-only"))
	{
		Check(StudioImageLists(folder));
	}
	else if(count > 1 && !lstrcmpW(arguments[1],L"--menus-only"))
	{
		Check(StudioMenuIntegration(folder));
	}
	else if(count > 1 && !lstrcmpW(arguments[1],L"--settings-only"))
	{
		Check(ApplicationSettingsTests(folder));
	}
	else if(count > 1 && !lstrcmpW(arguments[1],L"--application-only"))
	{
		Check(ApplicationBuildAndRun(folder));
		Check(ApplicationBuildAndRun(folder,TRUE));
	}
	else if(count > 1 && !lstrcmpW(arguments[1],L"--application-debug-only"))
	{
		Check(ApplicationBuildAndRun(folder,FALSE,TRUE));
	}
	else if(count > 1 && !lstrcmpW(arguments[1],L"--source-only"))
	{
		Check(SourceSynchronization(folder));
	}
	else if(count > 1 && !lstrcmpW(arguments[1],L"--packages-only"))
	{
		Check(StudioPackages(folder));
	}
	else
	{
		Check(ApplicationProjectCreation(folder));
		Check(ApplicationSettingsTests(folder));
		Check(StudioMultiSelection(folder));
		Check(StudioListViews(folder));
		Check(Integration(folder));
		Check(StudioImageButtons(folder));
		Check(StudioCanvases(folder));
		Check(StudioCanvasExample(folder));
		Check(StudioStatusBars(folder));
		Check(StudioProgressBars(folder));
		Check(StudioMenuIntegration(folder));
		Check(StudioPackages(folder));
		Check(SourceSynchronization(folder));
		Check(ApplicationBuildAndRun(folder));
		Check(ApplicationBuildAndRun(folder,TRUE));
	}
Cleanup:
	if(SUCCEEDED(RStrFormatW(&message,L"Studio integration: %u assertions; result 0x%.8x\r\nScratch files: %ls\r\n",g_checks,hr,folder))) Log(message.c_wstr());
	CodeEditUnregister(); CQuadooProject::Unregister(instance); CQuadooStudio::Unregister(instance); CQuadooCallStackDisplay::Unregister(instance); CSplitter::Unregister(instance); OleUninitialize();
	return FAILED(hr) ? 1 : 0;
}
