#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include "resource.h"
#include "QuadooProject.h"
#include "ApplicationProject.h"
#include "Library\ChooseFile.h"
#include "Library\Window\DialogHost.h"
#include "Library\Util\Formatting.h"
#include "Published\SimbeyCore.h"
#include "..\..\..\platform\QuadooForm\Designer\Designer.h"
#include "ApplicationSettingsDlg.h"

HRESULT StudioApplication::DecodeApplicationIcon(IJSONObject* project, CMemoryStream& bytes)
{
	HRESULT hr = S_FALSE;
	TStackRef<IJSONValue> value;
	TStackRef<IJSONValue> validated;
	TStackRef<IJSONObject> object;
	CRString data;
	CRString ascii;
	CRString canonical;
	CMemoryStream encoded;
	CheckIf(!project,E_POINTER);
	if(project->HasField(RSTRING_CAST(L"icon")))
	{
		Check(project->FindNonNullValueW(L"icon",&value));
		Check(value->GetString(&data));
		if(!data.Length())
		{
			hr = S_FALSE;
			goto Cleanup;
		}
		CheckIf(data.Length() > (1024 * 1024 + 2) / 3 * 4,E_INVALIDARG);
		if(RStrIsWide(static_cast<RSTRING>(data)))
		{
			Check(RStrConvertToA(data.Length(),data.c_wstr(),&ascii));
		}
		else
		{
			ascii = data;
		}
		Check(ScDecodeBase64A(RStrToAnsi(static_cast<RSTRING>(ascii)),ascii.Length(),&bytes,NULL));
		// Check the original encoding before the runtime repairs legacy ICO data.
		Check(ScEncodeBase64A(bytes.GetReadPtr(),bytes.DataRemaining(),&encoded,NULL));
		CheckIf(encoded.DataRemaining() != ascii.Length() || memcmp(encoded.GetReadPtr(),RStrToAnsi(static_cast<RSTRING>(ascii)),ascii.Length()),E_INVALIDARG);
		Check(QuadooCreateIconValue(bytes.GetReadPtr(),bytes.DataRemaining(),&validated));
		Check(validated->GetObject(&object));
		value.Release();
		Check(object->FindNonNullValueW(L"data",&value));
		Check(value->GetString(&canonical));
		ascii = NULL;
		Check(RStrConvertToA(canonical.Length(),canonical.c_wstr(),&ascii));
		// Executable RT_ICON resources must receive the normalized image bytes.
		bytes.Reset();
		Check(ScDecodeBase64A(RStrToAnsi(static_cast<RSTRING>(ascii)),ascii.Length(),&bytes,NULL));
	}
Cleanup:
	return hr;
}

StudioApplication::ModuleArchitecture StudioApplication::CurrentModuleArchitecture()
{
#ifdef _WIN64
	return ModulesX64;
#else
	return ModulesX86;
#endif
}

HRESULT StudioApplication::ReadModuleSettings(IJSONObject* project, ModuleArchitecture architecture, IJSONArray** modules)
{
	HRESULT hr;
	TStackRef<IJSONValue> value;
	TStackRef<IJSONObject> settings;
	TStackRef<IJSONArray> source;
	TStackRef<IJSONArray> copy;
	CheckIf(!project || !modules,E_POINTER);
	*modules = NULL;
	Check(JSONCreateArray(&copy));
	if(project->HasField(RSTRING_CAST(L"modules")))
	{
		Check(project->FindNonNullValueW(L"modules",&value));
		if(value->GetType() == JSON::Array)
		{
			if(architecture == ModulesX86)
			{
				Check(value->GetArray(&source));
			}
		}
		else
		{
			Check(value->GetObject(&settings));
			value.Release();
			PCWSTR name = architecture == ModulesX64 ? L"x64" : L"x86";
			if(settings->HasField(RSTRING_CAST(name)))
			{
				Check(settings->FindNonNullValueW(name,&value));
				Check(value->GetArray(&source));
			}
		}
		value.Release();
	}
	if(source)
	{
		CheckIf(source->Count() > 4096,E_INVALIDARG);
		for(sysint n = 0; n < source->Count(); n++)
		{
			CRString path;
			Check(source->GetString(n,&path));
			CheckIf(!path.Length() || !RStrIsWide(static_cast<RSTRING>(path)),E_INVALIDARG);
			Check(JSONCreateString(path,&value));
			Check(copy->Add(value));
			value.Release();
		}
	}
	*modules = copy.Detach();
Cleanup:
	return hr;
}

HRESULT StudioApplication::WrapModuleSettings(IJSONArray* x86, IJSONArray* x64, IJSONValue** modules)
{
	HRESULT hr;
	TStackRef<IJSONArray> empty;
	TStackRef<IJSONObject> object;
	TStackRef<IJSONValue> value;
	if(!x64)
	{
		Check(JSONCreateArray(&empty));
		x64 = empty;
	}
	Check(JSONCreateObject(&object));
	Check(JSONWrapArray(x86,&value));
	Check(object->AddValueW(L"x86",value));
	value.Release();
	Check(JSONWrapArray(x64,&value));
	Check(object->AddValueW(L"x64",value));
	Check(JSONWrapObject(object,modules));
Cleanup:
	return hr;
}

HRESULT StudioApplication::ResolveModulePath(RSTRING directory, RSTRING path, CRString& absolute, CRString& name)
{
	HRESULT hr;
	PWSTR combined = NULL;
	INT length;
	WCHAR resolved[MAX_PATH];
	PWSTR leaf = NULL;
	DWORD count;
	CheckIf(!directory || !path || !RStrIsWide(path) || !RStrLen(path),E_INVALIDARG);
	for(INT n = 0; n < RStrLen(path); n++)
	{
		CheckIf(!RStrToWide(path)[n],E_INVALIDARG);
	}
	Check(Formatting::TBuildDirectory(RStrToWide(directory),RStrLen(directory),RStrToWide(path),RStrLen(path),&combined,&length));
	count = GetFullPathNameW(combined,ARRAYSIZE(resolved),resolved,&leaf);
	CheckIf(!count,HrEnsureLastError());
	CheckIf(count >= ARRAYSIZE(resolved),HRESULT_FROM_WIN32(ERROR_FILENAME_EXCED_RANGE));
	CheckIf(!leaf || !*leaf,E_INVALIDARG);
	INT nameLength = static_cast<INT>(count - (leaf - resolved));
	CheckIf(leaf[nameLength - 1] == L'.' || leaf[nameLength - 1] == L' ',E_INVALIDARG);
	for(INT n = 0; n < nameLength; n++)
	{
		CheckIf(leaf[n] < L' ' || TStrChr(L"<>:\"/\\|?*",leaf[n]),E_INVALIDARG);
	}
	Check(RStrCreateW(count,resolved,&absolute));
	Check(RStrCreateW(nameLength,leaf,&name));
Cleanup:
	SafeDeleteArray(combined);
	return hr;
}

CApplicationSettingsDlg::CApplicationSettingsDlg(IJSONObject* project, RSTRING directory) :
	CBaseDialog(IDD_APPLICATION_SETTINGS), m_project(project), m_architecture(StudioApplication::CurrentModuleArchitecture()),
	m_directory(directory), m_icon(NULL), m_error(S_OK)
{
}

CApplicationSettingsDlg::~CApplicationSettingsDlg()
{
	if(m_icon)
	{
		DestroyIcon(m_icon);
	}
}

RSTRING CApplicationSettingsDlg::IconData() const
{
	return m_iconData;
}

IJSONArray* CApplicationSettingsDlg::Modules() const
{
	return m_modules[m_architecture];
}

HRESULT CApplicationSettingsDlg::WrapModules(IJSONValue** modules) const
{
	return StudioApplication::WrapModuleSettings(m_modules[StudioApplication::ModulesX86],m_modules[StudioApplication::ModulesX64],modules);
}

HRESULT CApplicationSettingsDlg::Error() const
{
	return m_error;
}

HRESULT CApplicationSettingsDlg::Initialize()
{
	HRESULT hr;
	CMemoryStream bytes;
	TStackRef<IJSONValue> icon;
	TStackRef<IJSONValue> data;
	TStackRef<IJSONObject> object;
	HWND list = GetDlgItem(IDC_APPLICATION_MODULES);
	RECT bounds;
	LVCOLUMNW column = {0};
	for(UINT architecture = 0; architecture < StudioApplication::ModuleArchitectureCount; architecture++)
	{
		Check(StudioApplication::ReadModuleSettings(m_project,static_cast<StudioApplication::ModuleArchitecture>(architecture),&m_modules[architecture]));
	}
	Check(StudioApplication::DecodeApplicationIcon(m_project,bytes));
	if(bytes.DataRemaining())
	{
		Check(QuadooCreateIconValue(bytes.GetReadPtr(),bytes.DataRemaining(),&icon));
		Check(QuadooLoadIconValue(icon,48,48,&m_icon));
		Check(icon->GetObject(&object));
		Check(object->FindNonNullValueW(L"data",&data));
		Check(data->GetString(&m_iconData));
		SendMessageW(GetDlgItem(IDC_EMBED_ICON),STM_SETICON,reinterpret_cast<WPARAM>(m_icon),0);
	}
	ListView_SetExtendedListViewStyle(list,LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
	GetClientRect(list,&bounds);
	column.mask = LVCF_TEXT | LVCF_WIDTH;
	column.pszText = const_cast<PWSTR>(L"Module name");
	column.cx = (bounds.right - bounds.left) / 3;
	CheckIf(ListView_InsertColumn(list,0,&column) < 0,E_FAIL);
	column.pszText = const_cast<PWSTR>(L"Module path");
	column.cx = (bounds.right - bounds.left) * 2 / 3 - GetSystemMetrics(SM_CXVSCROLL);
	CheckIf(ListView_InsertColumn(list,1,&column) < 0,E_FAIL);
	Check(ShowModules(m_architecture));
Cleanup:
	return hr;
}

HRESULT CApplicationSettingsDlg::ShowModules(StudioApplication::ModuleArchitecture architecture)
{
	HRESULT hr = S_OK;
	HWND list = GetDlgItem(IDC_APPLICATION_MODULES);
	INT previousCount = ListView_GetItemCount(list);
	IJSONArray* modules = m_modules[architecture];
	// Keep the current rows until the replacement list has loaded successfully.
	SendMessageW(list,WM_SETREDRAW,FALSE,0);
	for(sysint n = 0; n < modules->Count(); n++)
	{
		CRString path;
		Check(modules->GetString(n,&path));
		Check(InsertModule(path,previousCount + static_cast<INT>(n)));
	}
	for(INT row = previousCount - 1; row >= 0; row--)
	{
		ListView_DeleteItem(list,row);
	}
	m_architecture = architecture;
Cleanup:
	if(FAILED(hr))
	{
		for(INT row = ListView_GetItemCount(list) - 1; row >= previousCount; row--)
		{
			ListView_DeleteItem(list,row);
		}
	}
	CheckRadioButton(GetFrame(),IDC_MODULES_X86,IDC_MODULES_X64,m_architecture == StudioApplication::ModulesX64 ? IDC_MODULES_X64 : IDC_MODULES_X86);
	SendMessageW(list,WM_SETREDRAW,TRUE,0);
	InvalidateRect(list,NULL,TRUE);
	UpdateButtons();
	return hr;
}

HRESULT CApplicationSettingsDlg::InsertModule(RSTRING path, INT index)
{
	HRESULT hr;
	CRString absolute;
	CRString name;
	HWND list = GetDlgItem(IDC_APPLICATION_MODULES);
	LVITEMW item = {0};
	Check(StudioApplication::ResolveModulePath(m_directory,path,absolute,name));
	item.mask = LVIF_TEXT;
	item.iItem = index;
	item.pszText = const_cast<PWSTR>(name.c_wstr());
	CheckIf(ListView_InsertItem(list,&item) < 0,E_OUTOFMEMORY);
	item.iSubItem = 1;
	item.pszText = const_cast<PWSTR>(RStrToWide(path));
	if(!ListView_SetItem(list,&item))
	{
		ListView_DeleteItem(list,index);
		Check(E_OUTOFMEMORY);
	}
Cleanup:
	return hr;
}

HRESULT CApplicationSettingsDlg::LoadIconFile(PCWSTR path)
{
	HRESULT hr;
	TStackRef<IJSONValue> value;
	TStackRef<IJSONValue> data;
	TStackRef<IJSONObject> object;
	CRString encoded;
	HICON replacement = NULL;
	Check(QFD::LoadIconFile(path,&value));
	Check(QuadooLoadIconValue(value,48,48,&replacement));
	Check(value->GetObject(&object));
	Check(object->FindNonNullValueW(L"data",&data));
	Check(data->GetString(&encoded));
	SendMessageW(GetDlgItem(IDC_EMBED_ICON),STM_SETICON,reinterpret_cast<WPARAM>(replacement),0);
	if(m_icon)
	{
		DestroyIcon(m_icon);
	}
	m_icon = replacement;
	replacement = NULL;
	m_iconData = encoded;
	UpdateButtons();
Cleanup:
	if(replacement)
	{
		DestroyIcon(replacement);
	}
	return hr;
}

VOID CApplicationSettingsDlg::ClearIcon()
{
	SendMessageW(GetDlgItem(IDC_EMBED_ICON),STM_SETICON,0,0);
	if(m_icon)
	{
		DestroyIcon(m_icon);
		m_icon = NULL;
	}
	m_iconData = static_cast<RSTRING>(NULL);
	UpdateButtons();
}

HRESULT CApplicationSettingsDlg::AddModule(PCWSTR path)
{
	HRESULT hr;
	CRString absolute;
	CRString name;
	CRString stored;
	TStackRef<IJSONValue> value;
	WCHAR relative[MAX_PATH];
	IJSONArray* modules = Modules();
	INT index = static_cast<INT>(modules->Count());
	Check(StudioApplication::ResolveModulePath(m_directory,RSTRING_CAST(path),absolute,name));
	DWORD attributes = GetFileAttributesW(absolute.c_wstr());
	CheckIf(attributes == INVALID_FILE_ATTRIBUTES,HrEnsureLastError());
	CheckIf(attributes & FILE_ATTRIBUTE_DIRECTORY,E_INVALIDARG);
	for(sysint n = 0; n < modules->Count(); n++)
	{
		CRString previous;
		CRString previousPath;
		CRString previousName;
		Check(modules->GetString(n,&previous));
		Check(StudioApplication::ResolveModulePath(m_directory,previous,previousPath,previousName));
		CheckIf(!lstrcmpiW(name.c_wstr(),previousName.c_wstr()),HRESULT_FROM_WIN32(ERROR_DUP_NAME));
	}
	if(PathRelativePathToW(relative,m_directory.c_wstr(),FILE_ATTRIBUTE_DIRECTORY,absolute.c_wstr(),FILE_ATTRIBUTE_NORMAL))
	{
		Check(RStrCreateW(TStrLenAssert(relative),relative,&stored));
	}
	else
	{
		stored = absolute;
	}
	Check(JSONCreateString(stored,&value));
	Check(InsertModule(stored,index));
	hr = modules->Add(value);
	if(FAILED(hr))
	{
		ListView_DeleteItem(GetDlgItem(IDC_APPLICATION_MODULES),index);
		goto Cleanup;
	}
	ListView_SetItemState(GetDlgItem(IDC_APPLICATION_MODULES),index,LVIS_SELECTED | LVIS_FOCUSED,LVIS_SELECTED | LVIS_FOCUSED);
	ListView_EnsureVisible(GetDlgItem(IDC_APPLICATION_MODULES),index,FALSE);
	UpdateButtons();
Cleanup:
	return hr;
}

HRESULT CApplicationSettingsDlg::RemoveModule()
{
	HRESULT hr = S_FALSE;
	HWND list = GetDlgItem(IDC_APPLICATION_MODULES);
	IJSONArray* modules = Modules();
	INT index = ListView_GetNextItem(list,-1,LVNI_SELECTED);
	if(index >= 0)
	{
		Check(modules->Remove(index));
		ListView_DeleteItem(list,index);
		if(modules->Count())
		{
			index = min(index,static_cast<INT>(modules->Count()) - 1);
			ListView_SetItemState(list,index,LVIS_SELECTED | LVIS_FOCUSED,LVIS_SELECTED | LVIS_FOCUSED);
		}
		UpdateButtons();
	}
Cleanup:
	return hr;
}

HRESULT CApplicationSettingsDlg::BrowseIcon()
{
	HRESULT hr;
	CChooseFile chooser;
	UINT filter;
	Check(chooser.Initialize());
	Check(chooser.SetInitialDirectory(m_directory.c_wstr()));
	hr = chooser.Choose(GetFrame(),TRUE,L"Select Application Icon",NULL,L"Icon images (*.ico;*.png)\0*.ico;*.png\0\0",L"ico",1,OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,&filter);
	CheckIfIgnore(hr == S_FALSE,S_FALSE);
	Check(hr);
	Check(LoadIconFile(chooser.GetFile(0)));
Cleanup:
	return hr;
}

HRESULT CApplicationSettingsDlg::BrowseModules()
{
	HRESULT hr;
	CChooseFile chooser;
	UINT filter;
	Check(chooser.Initialize());
	Check(chooser.SetInitialDirectory(m_directory.c_wstr()));
	hr = chooser.Choose(GetFrame(),TRUE,L"Add Module Dependencies",NULL,L"Modules (*.dll;*.qbc;*.quadoo)\0*.dll;*.qbc;*.quadoo\0All files (*.*)\0*.*\0\0",NULL,1,OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,&filter);
	CheckIfIgnore(hr == S_FALSE,S_FALSE);
	Check(hr);
	for(INT n = 0; n < chooser.GetFileCount(); n++)
	{
		if(chooser.GetDir())
		{
			CRString path;
			Check(RStrFormatW(&path,L"%ls\\%ls",chooser.GetDir(),chooser.GetFile(n)));
			Check(AddModule(path.c_wstr()));
		}
		else
		{
			Check(AddModule(chooser.GetFile(n)));
		}
	}
Cleanup:
	return hr;
}

VOID CApplicationSettingsDlg::UpdateButtons()
{
	EnableWindow(GetDlgItem(IDC_CLEAR_ICON),m_icon != NULL);
	EnableWindow(GetDlgItem(IDC_REMOVE_MODULE),ListView_GetNextItem(GetDlgItem(IDC_APPLICATION_MODULES),-1,LVNI_SELECTED) >= 0);
}

VOID CApplicationSettingsDlg::ReportError(HRESULT error)
{
	CRString text;
	if(SUCCEEDED(RStrFormatW(&text,L"The settings could not be updated (0x%.8x). Check the icon or module path. Each module must have a unique file name.",error)))
	{
		MessageBoxW(GetFrame(),text.c_wstr(),L"Application Settings",MB_OK | MB_ICONERROR);
	}
}

BOOL CApplicationSettingsDlg::DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result)
{
	HRESULT hr = S_OK;
	switch(message)
	{
	case WM_INITDIALOG:
		m_error = Initialize();
		if(FAILED(m_error))
		{
			End(IDCANCEL);
		}
		else
		{
			CenterHost();
		}
		break;
	case WM_NOTIFY:
		if(reinterpret_cast<NMHDR*>(lParam)->idFrom == IDC_APPLICATION_MODULES)
		{
			UpdateButtons();
		}
		break;
	case WM_CLOSE:
		End(IDCANCEL);
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDC_SELECT_ICON:
			hr = BrowseIcon();
			break;
		case IDC_CLEAR_ICON:
			ClearIcon();
			break;
		case IDC_ADD_MODULE:
			hr = BrowseModules();
			break;
		case IDC_REMOVE_MODULE:
			hr = RemoveModule();
			break;
		case IDC_MODULES_X86:
		case IDC_MODULES_X64:
			if(HIWORD(wParam) == BN_CLICKED)
			{
				StudioApplication::ModuleArchitecture architecture = LOWORD(wParam) == IDC_MODULES_X64 ? StudioApplication::ModulesX64 : StudioApplication::ModulesX86;
				if(architecture != m_architecture)
				{
					hr = ShowModules(architecture);
				}
			}
			break;
		case IDOK:
		case IDCANCEL:
			End(LOWORD(wParam));
			break;
		}
		break;
	}
	if(FAILED(hr))
	{
		ReportError(hr);
	}
	return FALSE;
}

static HRESULT CopySetting(IJSONObject* source, IJSONObject* target, PCWSTR name)
{
	HRESULT hr = S_OK;
	TStackRef<IJSONValue> value;
	if(source->HasField(RSTRING_CAST(name)))
	{
		Check(source->FindValueW(name,&value));
		Check(target->AddValueW(name,value));
	}
	else
	{
		target->RemoveValueW(name);
	}
Cleanup:
	return hr;
}

HRESULT CQuadooProject::EditApplicationSettings()
{
	HRESULT hr;
	CDialogHost host(m_hInstance);
	CApplicationSettingsDlg dialog(m_pProject,m_rstrProjectDir);
	TStackRef<IJSONValue> snapshot;
	TStackRef<IJSONObject> previous;
	TStackRef<IJSONValue> value;
	BOOL changed = FALSE;
	CheckIf(!IsApplication(),OLECMDERR_E_DISABLED);
	Check(host.Display(m_hwnd,&dialog));
	Check(dialog.Error());
	if(host.GetReturnValue() != IDOK)
	{
		hr = S_FALSE;
		goto Cleanup;
	}
	Check(JSONCloneObject(m_pProject,&snapshot,TRUE));
	Check(snapshot->GetObject(&previous));
	changed = TRUE;
	if(RStrLen(dialog.IconData()))
	{
		Check(JSONCreateString(dialog.IconData(),&value));
		Check(m_pProject->AddValueW(L"icon",value));
		value.Release();
	}
	else
	{
		m_pProject->RemoveValueW(L"icon");
	}
	Check(dialog.WrapModules(&value));
	Check(m_pProject->AddValueW(L"modules",value));
	Check(Save());
Cleanup:
	if(FAILED(hr) && changed)
	{
		CopySetting(previous,m_pProject,L"icon");
		CopySetting(previous,m_pProject,L"modules");
	}
	return hr;
}
