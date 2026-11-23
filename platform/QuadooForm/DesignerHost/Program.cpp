#include "..\Designer\Designer.h"
#include "..\Packages\Packages.h"
#include "Library\ChooseFile.h"
#include <shellapi.h>

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

class CDesignerHost : public QFP::IHost
{
	HWND m_window, m_status;
	HMENU m_menu;
	TStackRef<QFD::CDesignerEditor> m_editor;
	TStackRef<QFP::CCatalog> m_packages;
	BOOL m_packagesBusy;

	VOID Report(HRESULT hr, PCWSTR operation)
	{
		if(FAILED(hr)) { CRString text; if(SUCCEEDED(RStrFormatW(&text,L"%ls failed (0x%.8x). The document has not been replaced. Verify the JSON, property formats and file permissions.",operation,hr))) MessageBoxW(m_window,text.c_wstr(),L"QuadooForm Designer",MB_OK | MB_ICONERROR); }
	}
	HRESULT Save(BOOL saveAs)
	{
		HRESULT hr;
		CChooseFile chooser;
		CRString path, initial;
		if(!saveAs && m_editor->Document().Path())
			return m_editor->SaveDocument(RStrToWide(m_editor->Document().Path()));
		Check(chooser.Initialize());
		if(m_editor->Document().Path()) initial = m_editor->Document().Path();
		else { CRString name; Check(m_editor->Document().Root()->GetName(&name)); Check(RStrFormatW(&initial,L"%ls.%ls.json",name.c_wstr(),m_editor->Document().IsForm() ? L"qform" : L"qframe")); }
		CheckIfIgnore(!chooser.SaveFile(m_window,L"Save QuadooForm definition",initial.c_wstr(),L"QuadooForm JSON (*.json)\0*.json\0All files\0*.*\0\0"),S_FALSE);
		PCWSTR selected = chooser.GetFile(0); INT length = TStrLenAssert(selected);
		if(length < 5 || lstrcmpiW(selected + length - 5,L".json")) { Check(RStrFormatW(&path,L"%ls.%ls.json",selected,m_editor->Document().IsForm() ? L"qform" : L"qframe")); }
		else { Check(RStrCreateW(length,selected,&path)); }
		Check(m_editor->SaveDocument(path.c_wstr()));
Cleanup:
		return hr;
	}
	BOOL ConfirmChange()
	{
		if(!m_editor || !m_editor->Document().Dirty()) return TRUE;
		INT answer = MessageBoxW(m_window,L"Save changes to the current definition?",L"QuadooForm Designer",MB_YESNOCANCEL | MB_ICONQUESTION);
		if(answer == IDNO) return TRUE;
		if(answer == IDYES) { HRESULT hr = Save(FALSE); Report(hr,L"Save"); return hr == S_OK; }
		return FALSE;
	}
	VOID Preferences(BOOL save)
	{
		HKEY key = NULL; DWORD disposition;
		if(save)
		{
			if(RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Simbey\\QuadooFormDesigner",0,NULL,0,KEY_WRITE,NULL,&key,&disposition) != ERROR_SUCCESS) return;
			INT left, right, tree; m_editor->GetLayout(&left,&right,&tree); DWORD values[] = { left,right,tree,m_editor->Zoom(),m_editor->GridVisible(),m_editor->SnapEnabled() };
			static PCWSTR const names[] = { L"ObjectsWidth",L"InspectorWidth",L"TreeHeight",L"Zoom",L"Grid",L"Snap" };
			for(UINT n = 0; n < ARRAYSIZE(values); n++) RegSetValueExW(key,names[n],0,REG_DWORD,reinterpret_cast<const BYTE*>(&values[n]),sizeof(DWORD));
		}
		else
		{
			if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Simbey\\QuadooFormDesigner",0,KEY_READ,&key) != ERROR_SUCCESS) return;
			DWORD values[] = { 190,300,285,100,TRUE,FALSE }; static PCWSTR const names[] = { L"ObjectsWidth",L"InspectorWidth",L"TreeHeight",L"Zoom",L"Grid",L"Snap" };
			for(UINT n = 0; n < ARRAYSIZE(values); n++) { DWORD size = sizeof(DWORD), type; RegQueryValueExW(key,names[n],NULL,&type,reinterpret_cast<PBYTE>(&values[n]),&size); }
			m_editor->SetLayout(values[0],values[1],values[2]); m_editor->SetZoom(values[3]); m_editor->SetGrid(values[4]); m_editor->SetSnap(values[5]);
		}
		RegCloseKey(key);
	}
	HMENU Menu()
	{
		HMENU menu = CreateMenu(), file = CreatePopupMenu(), create = CreatePopupMenu(), edit = CreatePopupMenu(), view = CreatePopupMenu(), form = CreatePopupMenu(), help = CreatePopupMenu();
		AppendMenuW(create,MF_STRING,QFD::NewForm,L"&Form"); AppendMenuW(create,MF_STRING,QFD::NewFrame,L"F&rame"); AppendMenuW(file,MF_POPUP,reinterpret_cast<UINT_PTR>(create),L"&New");
		AppendMenuW(file,MF_STRING,QFD::Open,L"&Open...\tCtrl+O"); AppendMenuW(file,MF_STRING,QFD::Save,L"&Save\tCtrl+S"); AppendMenuW(file,MF_STRING,QFD::SaveAs,L"Save &As..."); AppendMenuW(file,MF_SEPARATOR,0,NULL); AppendMenuW(file,MF_STRING,QFD::Exit,L"E&xit");
		AppendMenuW(edit,MF_STRING,QFD::Cut,L"Cu&t\tCtrl+X"); AppendMenuW(edit,MF_STRING,QFD::Copy,L"&Copy\tCtrl+C"); AppendMenuW(edit,MF_STRING,QFD::Paste,L"&Paste\tCtrl+V"); AppendMenuW(edit,MF_SEPARATOR,0,NULL);
		AppendMenuW(edit,MF_STRING,QFD::DeleteSelection,L"&Delete\tDelete"); AppendMenuW(edit,MF_STRING,QFD::SelectParent,L"Select &Parent"); AppendMenuW(edit,MF_STRING,QFD::SelectRoot,L"Select &Root");
		AppendMenuW(view,MF_STRING,QFD::Zoom50,L"50%"); AppendMenuW(view,MF_STRING,QFD::Zoom75,L"75%"); AppendMenuW(view,MF_STRING,QFD::Zoom100,L"100%"); AppendMenuW(view,MF_STRING,QFD::Zoom150,L"150%"); AppendMenuW(view,MF_STRING,QFD::Zoom200,L"200%"); AppendMenuW(view,MF_SEPARATOR,0,NULL); AppendMenuW(view,MF_STRING,QFD::Grid,L"Workspace &Grid"); AppendMenuW(view,MF_STRING,QFD::Snap,L"&Snap to 8 logical units"); AppendMenuW(view,MF_STRING,QFD::ResetLayout,L"&Reset pane layout");
		AppendMenuW(form,MF_STRING,QFP::PackagesCommand,L"&Packages...");
		AppendMenuW(form,MF_SEPARATOR,0,NULL);
		AppendMenuW(form,MF_STRING,QFD::Preview,L"&Preview\tF5");
		AppendMenuW(form,MF_SEPARATOR,0,NULL);
		AppendMenuW(form,MF_STRING,QFD::TabOrder,L"Tab &Order...");
		AppendMenuW(form,MF_STRING,QFD::MenuEditor,L"&Menu Editor...");
		AppendMenuW(form,MF_STRING,QFD::ImageListEditor,L"&Image List Editor...");
		AppendMenuW(form,MF_STRING,QFD::TreeNodesEditor,L"Tree &Nodes Editor...");
		AppendMenuW(form,MF_STRING,QFD::ListColumnsEditor,L"ListView &Columns Editor...");
		AppendMenuW(form,MF_STRING,QFD::AddStatusPanel,L"Add Status &Panel");
		AppendMenuW(form,MF_STRING,QFD::MoveStatusPanelLeft,L"Move Panel &Left");
		AppendMenuW(form,MF_STRING,QFD::MoveStatusPanelRight,L"Move Panel &Right");
		AppendMenuW(form,MF_STRING,QFD::SendToBack,L"Send to &Back"); AppendMenuW(form,MF_STRING,QFD::SendToFront,L"Send to &Front"); AppendMenuW(form,MF_STRING,QFD::ToggleEnabled,L"&Enabled");
		AppendMenuW(help,MF_STRING,QFD::About,L"&About");
		AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(file),L"&File"); AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(edit),L"&Edit"); AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(view),L"&View"); AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(form),L"F&orm"); AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(help),L"&Help");
		return menu;
	}
	HRESULT Initialize()
	{
		HRESULT hr;
		m_menu = Menu(); CheckIf(!m_menu,E_OUTOFMEMORY); SetMenu(m_window,m_menu);
		m_status = CreateWindowW(STATUSCLASSNAMEW,L"",WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,0,0,1,1,m_window,NULL,GetModuleHandle(NULL),NULL); CheckIf(!m_status,HrEnsureLastError());
		m_packages.Attach(__new QFP::CCatalog); CheckAlloc(m_packages); Check(m_packages->Initialize());
		m_editor.Attach(__new QFD::CDesignerEditor); CheckAlloc(m_editor); Check(m_editor->Create(m_window,m_packages->Registry()));
		{ HDC dc = GetDC(m_window); if(dc) { m_editor->SetDPI(GetDeviceCaps(dc,LOGPIXELSX)); ReleaseDC(m_window,dc); } }
		Check(m_editor->NewDocument(TRUE)); Preferences(FALSE); Layout(); Update();
Cleanup:
		return hr;
	}
	VOID Layout()
	{
		if(!m_editor || !m_status) return;
		RECT client, status; GetClientRect(m_window,&client); SendMessageW(m_status,WM_SIZE,0,0); GetWindowRect(m_status,&status);
		INT parts[] = { max(0,client.right - 110),-1 }; SendMessageW(m_status,SB_SETPARTS,ARRAYSIZE(parts),reinterpret_cast<LPARAM>(parts));
		SetWindowPos(m_editor->Window(),NULL,0,0,client.right,max(1,client.bottom - (status.bottom - status.top)),SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
		RedrawWindow(m_window,NULL,NULL,RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
	}
	VOID Update()
	{
		if(!m_editor || !m_editor->Document().Root()) return;
		CRString name, title; m_editor->Document().Root()->GetName(&name);
		if(SUCCEEDED(RStrFormatW(&title,L"%ls%ls - QuadooForm Designer",m_editor->Document().Path() ? RStrToWide(m_editor->Document().Path()) : name.c_wstr(),m_editor->Document().Dirty() ? L" *" : L""))) SetWindowTextW(m_window,title.c_wstr());
		SendMessageW(m_status,SB_SETTEXTW,0,reinterpret_cast<LPARAM>(RStrToWide(m_editor->Status())));
		{ CRString zoom; if(SUCCEEDED(RStrFormatW(&zoom,L"Zoom %u%%",m_editor->Zoom()))) SendMessageW(m_status,SB_SETTEXTW,1,reinterpret_cast<LPARAM>(zoom.c_wstr())); }
		BOOL preview = m_editor->Document().IsForm(); EnableMenuItem(m_menu,QFD::Preview,MF_BYCOMMAND | (preview ? MF_ENABLED : MF_GRAYED));
		GUID selected, root; m_editor->GetSelectedID(&selected); m_editor->Document().Root()->GetID(&root); BOOL child = !IsEqualGUID(selected,root);
		EnableMenuItem(m_menu,QFD::DeleteSelection,MF_BYCOMMAND | (child ? MF_ENABLED : MF_GRAYED)); EnableMenuItem(m_menu,QFD::SelectParent,MF_BYCOMMAND | (child ? MF_ENABLED : MF_GRAYED));
		UINT zooms[] = { 50,75,100,150,200 }; for(UINT n = 0; n < ARRAYSIZE(zooms); n++) CheckMenuItem(m_menu,QFD::Zoom50 + n,MF_BYCOMMAND | (m_editor->Zoom() == zooms[n] ? MF_CHECKED : MF_UNCHECKED));
		CheckMenuItem(m_menu,QFD::Grid,MF_BYCOMMAND | (m_editor->GridVisible() ? MF_CHECKED : MF_UNCHECKED)); CheckMenuItem(m_menu,QFD::Snap,MF_BYCOMMAND | (m_editor->SnapEnabled() ? MF_CHECKED : MF_UNCHECKED));
	}
	VOID Command(UINT command)
	{
		HRESULT hr = S_OK;
		hr = m_editor->CommitPropertyEdit();
		if(FAILED(hr)) { Report(hr,L"Edit property"); return; }
		switch(command)
		{
		case QFP::PackagesCommand: m_packagesBusy=TRUE; hr=QFP::ShowPackages(m_window,GetModuleHandle(NULL),m_packages,this); m_packagesBusy=FALSE; break;
		case QFD::NewForm: case QFD::NewFrame: if(ConfirmChange()) hr = m_editor->NewDocument(command == QFD::NewForm); break;
		case QFD::Open:
			if(ConfirmChange()) { CChooseFile chooser; hr = chooser.Initialize(); if(SUCCEEDED(hr) && chooser.OpenSingleFile(m_window,L"Open a Form or Frame",L"Form and Frame JSON (*.qform.json;*.qframe.json;*.json)\0*.qform.json;*.qframe.json;*.json\0All files\0*.*\0\0")) hr = m_editor->OpenDocument(chooser.GetFile(0)); } break;
		case QFD::Save: case QFD::SaveAs: hr = Save(command == QFD::SaveAs); break;
		case QFD::Exit: SendMessageW(m_window,WM_CLOSE,0,0); return;
		case QFD::DeleteSelection: hr = m_editor->Delete(); break;
		case QFD::SelectRoot: m_editor->SelectAncestor(TRUE); break;
		case QFD::SelectParent: m_editor->SelectAncestor(FALSE); break;
		case QFD::Preview:
			{
				LRESULT result; if(m_editor->Document().IsForm()) hr = m_editor->PreviewForm(m_window,&result);
				if(FAILED(hr)) { MessageBoxW(m_window,RStrToWide(m_editor->Status()),L"Preview failed",MB_OK | MB_ICONERROR); Update(); return; }
			}
			break;
		case QFD::ImageListEditor:
		case QFD::TreeNodesEditor:
		case QFD::AddStatusPanel:
		case QFD::MoveStatusPanelLeft:
		case QFD::MoveStatusPanelRight:
		case QFD::ListColumnsEditor:
		case QFD::MenuEditor: case QFD::TabOrder: case QFD::SendToBack: case QFD::SendToFront: case QFD::Cut: case QFD::Copy: case QFD::Paste: case QFD::ToggleEnabled:
			if(m_editor->CanExecute(command)) hr = m_editor->ExecuteCommand(command,m_window); break;
		case QFD::Zoom50: case QFD::Zoom75: case QFD::Zoom100: case QFD::Zoom150: case QFD::Zoom200: { UINT zooms[] = { 50,75,100,150,200 }; m_editor->SetZoom(zooms[command - QFD::Zoom50]); } break;
		case QFD::Grid: m_editor->SetGrid(!m_editor->GridVisible()); break;
		case QFD::Snap: m_editor->SetSnap(!m_editor->SnapEnabled()); break;
		case QFD::ResetLayout: m_editor->ResetPanes(); break;
		case QFD::About: MessageBoxW(m_window,L"QuadooForm Designer\nNative Form and Frame editor\nVS2008 / Windows XP compatible\nDouble-click a property or event to edit it.",L"About QuadooForm Designer",MB_OK | MB_ICONINFORMATION); break;
		}
		Report(hr,L"Command"); Update(); if(m_packages) m_packages->UpdateUsage(this);
	}
	LRESULT Message(UINT msg, WPARAM wp, LPARAM lp)
	{
		if(msg==QFP::CatalogChangedMessage() || (msg==WM_ACTIVATE && LOWORD(wp)!=WA_INACTIVE))
		{
			if(m_packages && !m_packagesBusy) { m_packagesBusy=TRUE; m_packages->Refresh(this); m_packages->UpdateUsage(this); m_packagesBusy=FALSE; }
			if(msg==QFP::CatalogChangedMessage()) return 0;
		}
		switch(msg)
		{
		case WM_CREATE: { HRESULT hr = Initialize(); Report(hr,L"Initialize"); return SUCCEEDED(hr) ? 0 : -1; }
		case WM_INITMENUPOPUP: if(m_editor) m_editor->UpdateCommandMenu(reinterpret_cast<HMENU>(wp)); return 0;
		case WM_COMMAND: Command(LOWORD(wp)); return 0;
		case QFD::ChangedMessage: Update(); if(m_packages && !m_packagesBusy) m_packages->UpdateUsage(this); return 0;
		case WM_SIZE: Layout(); return 0;
		case WM_GETMINMAXINFO: reinterpret_cast<MINMAXINFO*>(lp)->ptMinTrackSize.x = 850; reinterpret_cast<MINMAXINFO*>(lp)->ptMinTrackSize.y = 500; return 0;
		case 0x02e0: if(m_editor) m_editor->SetDPI(LOWORD(wp)); { RECT* rect = reinterpret_cast<RECT*>(lp); SetWindowPos(m_window,NULL,rect->left,rect->top,rect->right - rect->left,rect->bottom - rect->top,SWP_NOACTIVATE | SWP_NOZORDER); } return 0;
		case WM_THEMECHANGED: case WM_SETTINGCHANGE: case WM_SYSCOLORCHANGE: if(m_editor) RedrawWindow(m_editor->Window(),NULL,NULL,RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN); return 0;
		case WM_CLOSE: { HRESULT hr = m_editor->CommitPropertyEdit(); if(FAILED(hr)) Report(hr,L"Edit property"); else if(ConfirmChange()) DestroyWindow(m_window); } return 0;
		case WM_DESTROY: if(m_editor) Preferences(TRUE); if(m_menu) { SetMenu(m_window,NULL); DestroyMenu(m_menu); m_menu = NULL; } PostQuitMessage(0); return 0;
		}
		return DefWindowProcW(m_window,msg,wp,lp);
	}
	static LRESULT CALLBACK Procedure(HWND window, UINT msg, WPARAM wp, LPARAM lp)
	{
		CDesignerHost* host = reinterpret_cast<CDesignerHost*>(GetWindowLongPtrW(window,GWLP_USERDATA));
		if(msg == WM_NCCREATE) { host = static_cast<CDesignerHost*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams); host->m_window = window; SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(host)); }
		return host ? host->Message(msg,wp,lp) : DefWindowProcW(window,msg,wp,lp);
	}
public:
	CDesignerHost() : m_window(NULL), m_status(NULL), m_menu(NULL), m_packagesBusy(FALSE) {}
	HRESULT VisitDefinitions(QFP::IDefinitionVisitor* visitor)
	{
		HRESULT hr=S_OK; TStackRef<IJSONObject> definition;
		if(m_editor && m_editor->Document().Root()) { Check(m_editor->CommitPropertyEdit()); Check(m_editor->Document().Serialize(&definition)); Check(visitor->Visit(definition,m_editor->Document().Path() ? m_editor->Document().Path() : RSTRING_CAST(L"(unsaved document)"))); }
	Cleanup: return hr;
	}
	HRESULT GetEditors(TArray<QFD::CDesignerEditor*>& editors) { return m_editor ? editors.Append(static_cast<QFD::CDesignerEditor*>(m_editor)) : S_OK; }
	HRESULT Run(PCWSTR path)
	{
		HRESULT hr;
		WNDCLASSEXW type; ZeroMemory(&type,sizeof(type)); type.cbSize = sizeof(type); type.hInstance = GetModuleHandle(NULL); type.lpfnWndProc = Procedure; type.hCursor = LoadCursor(NULL,IDC_ARROW); type.hIcon = LoadIcon(NULL,IDI_APPLICATION); type.hbrBackground = GetSysColorBrush(COLOR_BTNFACE); type.lpszClassName = L"QuadooForm.Designer.Host.1";
		HACCEL shortcuts = NULL; ACCEL keys[] = { { FVIRTKEY | FCONTROL,'O',QFD::Open },{ FVIRTKEY | FCONTROL,'S',QFD::Save },{ FVIRTKEY,VK_F5,QFD::Preview },{ FVIRTKEY,VK_DELETE,QFD::DeleteSelection } };
		RECT work; CheckIf(!SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0),HrEnsureLastError());
		CheckIf(!RegisterClassExW(&type),HrEnsureLastError());
		m_window = CreateWindowExW(0,type.lpszClassName,L"QuadooForm Designer",WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,work.left,work.top,min(1250,work.right - work.left),min(800,work.bottom - work.top),NULL,NULL,type.hInstance,this); CheckIf(!m_window,HrEnsureLastError());
		if(path && *path) { hr = m_editor->OpenDocument(path); Report(hr,L"Open"); }
		shortcuts = CreateAcceleratorTableW(keys,ARRAYSIZE(keys)); CheckIf(!shortcuts,HrEnsureLastError()); ShowWindow(m_window,SW_SHOW); UpdateWindow(m_window);
		MSG message;
		while(TRUE)
		{
			INT received = GetMessageW(&message,NULL,0,0);
			if(!received)
				break;
			CheckIfGetLastError(received < 0);
			if(m_editor->TranslateEditorMessage(&message))
				continue;
			if(!TranslateAcceleratorW(m_window,shortcuts,&message) && !IsDialogMessageW(m_window,&message))
			{
				TranslateMessage(&message);
				DispatchMessageW(&message);
			}
		}
		hr = S_OK;
Cleanup:
		if(shortcuts) DestroyAcceleratorTable(shortcuts); if(m_window && IsWindow(m_window)) DestroyWindow(m_window); m_editor.Release();
		return hr;
	}
};
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, INT)
{
	HMODULE user = GetModuleHandleW(L"user32.dll"); typedef BOOL (WINAPI* AWARE)(); AWARE aware = reinterpret_cast<AWARE>(GetProcAddress(user,"SetProcessDPIAware")); if(aware) aware();
	INITCOMMONCONTROLSEX controls = { sizeof(controls),ICC_WIN95_CLASSES }; InitCommonControlsEx(&controls);
	HRESULT initialized = CoInitialize(NULL); if(FAILED(initialized)) return 1;
	INT count; PWSTR* args = CommandLineToArgvW(GetCommandLineW(),&count); CDesignerHost host; HRESULT hr = host.Run(args && count > 1 ? args[1] : NULL);
	if(args) LocalFree(args); CoUninitialize(); return FAILED(hr) ? 1 : 0;
}
