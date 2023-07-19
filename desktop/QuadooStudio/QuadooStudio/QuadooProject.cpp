#include <windows.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <richedit.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Core\MemoryStream.h"
#include "Library\Util\Formatting.h"
#include "Library\Util\TextHelpers.h"
#include "Library\Util\Shell.h"
#include "Library\ChooseFile.h"
#include "Library\MenuUtil.h"
#include "Library\Window\DialogHost.h"
#include "Published\JSON.h"
#include "Tabs.h"
#include "RunParamsDlg.h"
#include "ProjectCompilerDlg.h"
#include "QuadooProject.h"

#ifndef ENM_CLIPFORMAT
	#define	ENM_CLIPFORMAT	0x00000080
#endif

const WCHAR c_wzQuadooProjectClass[] = L"QuadooProjectCls";

CQuadooProject::CQuadooProject (HINSTANCE hInstance, HWND hwndTree) :
	m_hInstance(hInstance),
	m_hwndTree(hwndTree),
	m_hCloseIcon(NULL),
	m_hDropDown(NULL),
	m_pTabs(NULL),
	m_rstrProject(NULL),
	m_rstrProjectDir(NULL),
	m_pProject(NULL),
	m_hProjectRoot(NULL),
	m_hRichEdit(NULL),
	m_hwndEditor(NULL)
{
	GetKeywords(&keywords, &keyword_count);
}

CQuadooProject::~CQuadooProject()
{
	SafeDelete(m_pTabs);

	DestroyIcon(m_hCloseIcon);
	DestroyIcon(m_hDropDown);

	SafeRelease(m_pProject);
	RStrRelease(m_rstrProjectDir);
	RStrRelease(m_rstrProject);

	if(m_hRichEdit)
		FreeLibrary(m_hRichEdit);
}

HRESULT CQuadooProject::Register(HINSTANCE hInstance)
{
	WNDCLASSEX wnd = {0};

	wnd.cbSize = sizeof(WNDCLASSEX);
	wnd.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	wnd.cbClsExtra = 0;
	wnd.cbWndExtra = 0;
	wnd.hInstance = hInstance;
	wnd.hIcon = NULL;
	wnd.hCursor = LoadCursor(NULL, IDC_ARROW);
	wnd.hbrBackground = NULL;
	wnd.lpszMenuName = NULL;
	wnd.lpszClassName = c_wzQuadooProjectClass;

	return RegisterClass(&wnd,NULL);
}

HRESULT CQuadooProject::Unregister (HINSTANCE hInstance)
{
	return UnregisterClass(c_wzQuadooProjectClass, hInstance);
}

HRESULT CQuadooProject::Initialize (HWND hwndParent, const RECT& rcSite, PCWSTR pcwzProject)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONArray> srFiles;
	RSTRING rstrLabel = NULL, rstrPath = NULL;
	TVINSERTSTRUCT tvis = {0};
	PCWSTR pcwzLabelPtr = TStrRChr(pcwzProject, L'\\'), pcwzExt;
	PWSTR pwzJSON = NULL;
	INT cchJSON;

	m_hRichEdit = LoadLibrary(L"riched20.dll");
	CheckIfGetLastError(NULL == m_hRichEdit);

	m_hCloseIcon = (HICON)LoadImage(m_hInstance, MAKEINTRESOURCE(IDI_CLOSE), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
	m_hDropDown = (HICON)LoadImage(m_hInstance, MAKEINTRESOURCE(IDI_DROP_DOWN), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);

	m_pTabs = __new CTabs(m_hCloseIcon, m_hDropDown);
	CheckAlloc(m_pTabs);
	Check(m_pTabs->LoadMetrics(hwndParent));

	Check(Create(0, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, c_wzQuadooProjectClass, NULL, rcSite.left, rcSite.top, rcSite.right - rcSite.left, rcSite.bottom - rcSite.top, hwndParent, SW_NORMAL));

	CheckIf(NULL == pcwzProject, E_UNEXPECTED);
	pcwzLabelPtr++;

	Check(RStrCreateW(static_cast<INT>(pcwzLabelPtr - pcwzProject), pcwzProject, &m_rstrProjectDir));

	pcwzExt = TStrRChr(pcwzLabelPtr, L'.');
	if(pcwzExt)
		Check(RStrCreateW(static_cast<INT>(pcwzExt - pcwzLabelPtr), pcwzLabelPtr, &rstrLabel));

	tvis.hParent        = TVI_ROOT;
	tvis.hInsertAfter   = TVI_LAST;
	tvis.item.mask      = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
	tvis.item.pszText   = const_cast<PWSTR>(rstrLabel ? RStrToWide(rstrLabel) : pcwzLabelPtr);
	tvis.item.iImage    = 0;
	tvis.item.iSelectedImage = 1;
	tvis.item.lParam    = (LPARAM)0;

	m_hProjectRoot = TreeView_InsertItem(m_hwndTree, &tvis);
	CheckIfGetLastError(NULL == m_hProjectRoot);

	Check(Text::LoadFromFile(pcwzProject, &pwzJSON, &cchJSON));
	Check(JSONParse(NULL, pwzJSON, cchJSON, &srv));
	Check(srv->GetObject(&m_pProject));

	srv.Release();
	Check(m_pProject->FindNonNullValueW(L"files", &srv));
	Check(srv->GetArray(&srFiles));

	for(sysint i = 0; i < srFiles->Count(); i++)
	{
		TStackRef<IJSONObject> srFile;
		CProjectFile* pFile;

		Check(srFiles->GetObject(i, &srFile));

		srv.Release();
		Check(srFile->FindNonNullValueW(L"path", &srv));
		Check(srv->GetString(&rstrPath));
		Check(AddFile(rstrPath, &pFile));

		srv.Release();
		if(SUCCEEDED(srFile->FindNonNullValueW(L"default", &srv)))
			Check(srv->GetBoolean(&pFile->m_fDefault));

		RStrRelease(rstrPath); rstrPath = NULL;
	}

	Check(RStrCreateW(TStrLenAssert(pcwzProject), pcwzProject, &m_rstrProject));

Cleanup:
	SafeDeleteArray(pwzJSON);
	RStrRelease(rstrPath);
	RStrRelease(rstrLabel);
	return hr;
}

HRESULT CQuadooProject::Save (VOID)
{
	HRESULT hr;
	CMemoryStream stmJSON, stmFormatted;

	Check(JSONSerializeObject(m_pProject, &stmJSON));
	Check(ReformatJSON(stmJSON.TGetReadPtr<WCHAR>(), stmJSON.TDataRemaining<WCHAR>(), &stmFormatted));
	Check(Text::SaveToFile(stmFormatted.TGetReadPtr<WCHAR>(), stmFormatted.TDataRemaining<WCHAR>(), RStrToWide(m_rstrProject)));

Cleanup:
	return hr;
}

HRESULT CQuadooProject::UpdateFiles (VOID)
{
	HRESULT hr;
	TStackRef<IJSONArray> srFiles;
	TStackRef<IJSONValue> srv;

	Check(JSONCreateArray(&srFiles));
	for(sysint i = 0; i < m_mapFiles.Length(); i++)
	{
		TStackRef<IJSONObject> srFile;
		CProjectFile* pFile = *m_mapFiles.GetValuePtr(i);

		Check(JSONCreateObject(&srFile));

		Check(JSONCreateString(pFile->m_rstrPath, &srv));
		Check(srFile->AddValueW(L"path", srv));
		srv.Release();

		if(pFile->m_fDefault)
		{
			Check(JSONCreateBool(pFile->m_fDefault, &srv));
			Check(srFile->AddValueW(L"default", srv));
			srv.Release();
		}

		Check(JSONWrapObject(srFile, &srv));
		Check(srFiles->Add(srv));
		srv.Release();
	}

	Check(JSONWrapArray(srFiles, &srv));
	Check(m_pProject->AddValueW(L"files", srv));

Cleanup:
	return hr;
}

HRESULT CQuadooProject::CloseProject (BOOL fPromptUserForSave)
{
	if(fPromptUserForSave)
	{
		for(sysint i = m_pTabs->Tabs() - 1; i >= 0; i--)
		{
			if(!CloseTab(i))
				return E_ABORT;
		}
	}

	m_mapFiles.DeleteAll();

	Destroy();
	return S_OK;
}

VOID CQuadooProject::ShowTreeContext (HTREEITEM hItem, const POINT& ptScreen)
{
	if(hItem == m_hProjectRoot)
	{
		HMENU hPopup = MenuUtil::LoadSubPopupMenu(m_hInstance, IDR_MENU, L"&Project");
		if(hPopup)
		{
			INT nCmd;

			MenuUtil::EnableMenuItems(NULL, hPopup, this);
			nCmd = TrackPopupMenu(hPopup, TPM_LEFTALIGN | TPM_RETURNCMD | TPM_NONOTIFY, ptScreen.x, ptScreen.y, 0, m_hwnd, NULL);
			if(0 != nCmd)
				Exec(NULL, nCmd, 0, NULL, NULL);
			DestroyMenu(hPopup);
		}
	}
	else
	{
		HMENU hPopup = MenuUtil::LoadSubPopupMenu(m_hInstance, IDR_FILE_MENU, L"&Context");
		if(hPopup)
		{
			INT nCmd;
			CProjectFile* pFile = GetProjectFromTreeItem(hItem);

			if(pFile->m_fDefault)
				CheckMenuItem(hPopup, ID_CONTEXT_SETDEFAULT, MF_BYCOMMAND | MF_CHECKED);

			nCmd = TrackPopupMenu(hPopup, TPM_LEFTALIGN | TPM_RETURNCMD | TPM_NONOTIFY, ptScreen.x, ptScreen.y, 0, m_hwnd, NULL);
			if(0 != nCmd)
			{
				VARIANT vItem;
				vItem.vt = VT_VOID;
				vItem.byref = (PVOID)hItem;
				Exec(NULL, nCmd, 0, &vItem, NULL);
			}

			DestroyMenu(hPopup);
		}
	}
}

VOID CQuadooProject::ActivateItem (HTREEITEM hItem)
{
	CProjectFile* pFile = GetProjectFromTreeItem(hItem);
	if(pFile)
		SwitchToFile(pFile);
}

VOID CQuadooProject::Move (const RECT& rcSite)
{
	__super::Move(rcSite.left, rcSite.top, rcSite.right - rcSite.left, rcSite.bottom - rcSite.top, TRUE);
}

// IOleCommandTarget

HRESULT STDMETHODCALLTYPE CQuadooProject::QueryStatus (
	const GUID* pguidCmdGroup,
	ULONG cCmds,
	OLECMD* prgCmds,
	OLECMDTEXT* pCmdText)
{
	for(ULONG i = 0; i < cCmds; i++)
	{
		switch(prgCmds[i].cmdID)
		{
		case ID_PROJECT_RUNPARAMETERS:
		case ID_PROJECT_ADDFILE:
		case ID_PROJECT_NEWFILE:
		case ID_CONTEXT_REMOVE:
			prgCmds[i].cmdf = OLECMDF_SUPPORTED | OLECMDF_ENABLED;
			break;
		case ID_RUN_SCRIPT:
		case ID_PROJECT_COMPILE:
			prgCmds[i].cmdf = OLECMDF_SUPPORTED;
			if(NULL != FindDefaultScript())
				prgCmds[i].cmdf |= OLECMDF_ENABLED;
			break;
		case ID_FILE_SAVEFILE:
			prgCmds[i].cmdf = OLECMDF_SUPPORTED;
			if(m_pTabs->GetActiveTabHighlight())
				prgCmds[i].cmdf |= OLECMDF_ENABLED;
			break;
		}
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CQuadooProject::Exec (
	const GUID* pguidCmdGroup,
	DWORD nCmdID,
	DWORD nCmdexecopt,
	VARIANT* pvaIn,
	VARIANT* pvaOut)
{
	HRESULT hr;

	switch(nCmdID)
	{
	case ID_PROJECT_RUNPARAMETERS:
		hr = EditRunParams();
		break;

	case ID_PROJECT_ADDFILE:
		hr = AddFilePrompt();
		break;

	case ID_PROJECT_NEWFILE:
		hr = NewFilePrompt();
		break;

	case ID_FILE_SAVEFILE:
		hr = SaveTab(m_pTabs->GetActiveTab());
		break;

	case ID_CONTEXT_SETDEFAULT:
		{
			CProjectFile* pFile = GetProjectFromTreeItem((HTREEITEM)pvaIn->byref);

			hr = S_FALSE;
			if(pFile)
			{
				CProjectFile* pDefault = FindDefaultScript();
				if(pDefault != pFile)
				{
					if(pDefault)
						pDefault->m_fDefault = false;
					pFile->m_fDefault = !pFile->m_fDefault;

					Check(UpdateFiles());
					Check(Save());
				}
			}
		}
		break;

	case ID_CONTEXT_REMOVE:
		{
			CProjectFile* pFile = GetProjectFromTreeItem((HTREEITEM)pvaIn->byref);
			if(pFile)
				RemoveFilePrompt(pFile);
			hr = S_OK;
		}
		break;

	case ID_RUN_SCRIPT:
		hr = RunScript();
		break;

	case ID_PROJECT_COMPILE:
		hr = ShowProjectCompiler();
		break;

	default:
		hr = E_NOTIMPL;
		break;
	}

Cleanup:
	return hr;
}

// CBaseWindow

HINSTANCE CQuadooProject::GetInstance (VOID)
{
	return m_hInstance;
}

VOID CQuadooProject::OnFinalDestroy (HWND hwnd)
{
}

HRESULT CQuadooProject::FinalizeAndShow (DWORD dwStyle, INT nCmdShow)
{
	return __super::FinalizeAndShow(dwStyle, nCmdShow);
}


BOOL CQuadooProject::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	switch (message)
	{
	case WM_CREATE:
		m_hwndEditor = CreateWindowEx(0,
			RICHEDIT_CLASS, NULL, WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE,
			0, 0, 100, 100, m_hwnd, NULL, m_hInstance, NULL);
		if(NULL == m_hwndEditor)
		{
			lResult = -1;
			return TRUE;
		}

		{
			CHARFORMAT2 cf;
			ZeroMemory(&cf, sizeof(cf));
			cf.cbSize = sizeof(CHARFORMAT2);
			cf.dwMask = CFM_FACE;
			TStrCchCpy(cf.szFaceName, ARRAYSIZE(cf.szFaceName), L"Courier New");
			SendMessage(m_hwndEditor, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

			LRESULT lMask = SendMessage(m_hwndEditor, EM_GETEVENTMASK, 0, 0);
			lMask |= ENM_CLIPFORMAT | ENM_KEYEVENTS;
			SendMessage(m_hwndEditor, EM_SETEVENTMASK, 0, lMask);
		}

		EnableWindow(m_hwndEditor, FALSE);
		break;

	case WM_PAINT:
	{
		RECT rcTabs, rcBody, rc;
		PAINTSTRUCT ps;

		m_pTabs->GetTabsRect(0, 0, rcTabs);

		GetClientRect(m_hwnd, &rcBody);
		rcBody.top = rcTabs.bottom;

		HDC hdc = BeginPaint(m_hwnd, &ps);

		if(IntersectRect(&rc, &ps.rcPaint, &rcTabs))
			m_pTabs->Draw(hdc, 0, 0);

		if(IntersectRect(&rc, &ps.rcPaint, &rcBody))
		{
			HBRUSH hbrBody = CreateSolidBrush(RGB(105, 161, 191));
			FillRect(hdc, &rcBody, hbrBody);
			DeleteObject(hbrBody);
		}

		EndPaint(m_hwnd, &ps);
	}
	break;

	case WM_SIZE:
		m_pTabs->Resize(LOWORD(lParam), HIWORD(lParam));

		{
			const SIZE* pcszTabs = m_pTabs->GetSize();
			MoveWindow(m_hwndEditor, 1, pcszTabs->cy, pcszTabs->cx - 2, (HIWORD(lParam) - pcszTabs->cy) - 1, TRUE);
		}
		break;

	case WM_MOUSEMOVE:
		if(m_pTabs->MouseHover(m_hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)))
		{
			RECT rc;
			m_pTabs->GetTabsRect(0, 0, rc);
			InvalidateRect(m_hwnd, &rc, FALSE);
		}
		break;

	case WM_LBUTTONDOWN:
		if(m_pTabs->IsCloseButton(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)) ||
			m_pTabs->IsDropDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)))
		{
			if(m_pTabs->MouseHover(m_hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)))
			{
				RECT rc;
				m_pTabs->GetTabsRect(0, 0, rc);
				InvalidateRect(m_hwnd, &rc, FALSE);

				SetCapture(m_hwnd);
			}
		}
		else
		{
			CProjectFile* pFile = m_pTabs->TFindTabFromPoint<CProjectFile>(LOWORD(lParam), HIWORD(lParam));
			if(pFile)
				SwitchToFile(pFile);
		}
		break;

	case WM_LBUTTONUP:
		ReleaseCapture();

		if(m_pTabs->IsCloseButton(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)))
		{
			sysint idxTab = m_pTabs->GetActiveTab();
			if(-1 != idxTab)
				CloseTab(idxTab);
		}
		else if(m_pTabs->IsDropDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)))
		{
			sysint idxTab;
			if(m_pTabs->PopupTabs(m_hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), &idxTab) && m_pTabs->GetActiveTab() != idxTab)
				SwitchToFile(m_pTabs->TGetTabData<CProjectFile>(idxTab));
		}
		break;

	case WM_NOTIFY:
		{
			MSGFILTER* pMsgFilter = (MSGFILTER *)lParam;
			if(pMsgFilter->msg == WM_CHAR)
			{
				if(SendMessage(m_hwndEditor, EM_GETMODIFY, 0, 0))
				{
					FormatWord();
					sysint idxTab = m_pTabs->GetActiveTab();
					if(-1 != idxTab)
					{
						CProjectFile* pFile = m_pTabs->TGetTabData<CProjectFile>(idxTab);
						if(!pFile->m_fModified)
						{
							RECT rc;

							pFile->m_fModified = true;
							m_pTabs->HighlightTab(idxTab, true);

							m_pTabs->GetTabsRect(0, 0, rc);
							InvalidateRect(m_hwnd, &rc, FALSE);
						}
					}
				}
			}
			else if(pMsgFilter->msg == WM_KEYUP)
			{
				if((pMsgFilter->wParam == 0x56 && GetAsyncKeyState(VK_CONTROL))
					|| (pMsgFilter->wParam == VK_INSERT && GetAsyncKeyState(VK_SHIFT)))
				{
					// Reformat the pasted text.
					InitFormatText();
				}
			}
		}
		break;

	case WM_SETFOCUS:
		SetFocus(m_hwndEditor);
		break;
	}

	return __super::DefWindowProc(message, wParam, lParam, lResult);
}

HRESULT CQuadooProject::SwitchToFile (CProjectFile* pFile)
{
	HRESULT hr;
	RSTRING rstrLabel = NULL;
	HDC hdc = NULL;
	RECT rc;
	sysint idxTab = m_pTabs->GetActiveTab();

	if(-1 != idxTab)
		SaveTabData(m_pTabs->TGetTabData<CProjectFile>(idxTab));

	idxTab = m_pTabs->FindTab(pFile);
	if(-1 == idxTab)
	{
		PCWSTR pcwzLabel = TStrRChr(RStrToWide(pFile->m_rstrPath), L'\\');
		if(pcwzLabel)
			pcwzLabel++;
		else
			pcwzLabel = RStrToWide(pFile->m_rstrPath);

		hdc = GetDC(m_hwnd);
		Check(RStrCreateW(TStrLenAssert(pcwzLabel), pcwzLabel, &rstrLabel));
		Check(m_pTabs->AddTab(hdc, rstrLabel, pFile));
		idxTab = 0;
	}

	Check(m_pTabs->SetActive(idxTab));
	Check(LoadTabData(pFile));

	m_pTabs->GetTabsRect(0, 0, rc);
	InvalidateRect(m_hwnd, &rc, FALSE);

	PostMessage(m_hwnd, WM_SETFOCUS, 0, 0);

Cleanup:
	EnableWindow(m_hwndEditor, SUCCEEDED(hr));
	if(FAILED(hr) && -1 != idxTab)
		CloseTab(idxTab);

	RStrRelease(rstrLabel);
	if(hdc)
		ReleaseDC(m_hwnd, hdc);
	return hr;
}

HRESULT CQuadooProject::EditRunParams (VOID)
{
	HRESULT hr;
	CDialogHost dlgHost(m_hInstance);
	RSTRING rstrEngine = NULL;

	FindInstalledEngine(&rstrEngine);

	{
		CRunParamsDlg dlgParams(m_pProject, rstrEngine, m_rstrProjectDir);
		Check(dlgHost.Display(m_hwnd, &dlgParams));
	}

	if(IDOK == dlgHost.GetReturnValue())
		Check(Save());

Cleanup:
	RStrRelease(rstrEngine);
	return hr;
}

HRESULT CQuadooProject::AddFilePrompt (VOID)
{
	HRESULT hr;
	CChooseFile pick;
	WCHAR wzRelative[MAX_PATH];
	RSTRING rstrPath = NULL;
	CProjectFile* pFile;

	Check(pick.Initialize());
	CheckIfIgnore(!pick.OpenSingleFile(m_hwnd, L"Add Existing File To Project", L"QuadooScript (*.quadoo)\0*.quadoo\0"), E_ABORT);

	if(PathRelativePathToW(wzRelative, RStrToWide(m_rstrProjectDir), FILE_ATTRIBUTE_DIRECTORY, pick.GetFile(0), 0))
	{
		Check(RStrCreateW(TStrLenAssert(wzRelative), wzRelative, &rstrPath));
		CheckIf(m_mapFiles.HasItem(rstrPath), E_FAIL);

		Check(AddFile(rstrPath, &pFile));
		if(1 == m_mapFiles.Length())
			pFile->m_fDefault = true;

		Check(UpdateFiles());
		Check(Save());

		Check(SwitchToFile(pFile));
	}
	else
	{
		hr = HrEnsureLastError();
		MessageBox(m_hwnd, L"The file must be on the same volume as the project file!", L"Path Error", MB_OK);
	}

Cleanup:
	RStrRelease(rstrPath);
	return hr;
}

HRESULT CQuadooProject::NewFilePrompt (VOID)
{
	HRESULT hr;
	CChooseFile pick;
	WCHAR wzRelative[MAX_PATH];
	RSTRING rstrPath = NULL;
	CProjectFile* pFile;
	HANDLE hFile = INVALID_HANDLE_VALUE;

	Check(pick.Initialize());
	Check(pick.SetInitialDirectory(RStrToWide(m_rstrProjectDir)));
	CheckIfIgnore(!pick.SaveFile(m_hwnd, L"Add New File To Project", L"QuadooScript (*.quadoo)\0*.quadoo\0"), E_ABORT);

	if(PathRelativePathToW(wzRelative, RStrToWide(m_rstrProjectDir), FILE_ATTRIBUTE_DIRECTORY, pick.GetFile(0), 0))
	{
		Check(RStrCreateW(TStrLenAssert(wzRelative), wzRelative, &rstrPath));
		CheckIf(m_mapFiles.HasItem(rstrPath), E_FAIL);

		hFile = CreateFileW(pick.GetFile(0), GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
		CheckIfGetLastError(INVALID_HANDLE_VALUE == hFile);
		SafeCloseFileHandle(hFile);

		Check(AddFile(rstrPath, &pFile));
		if(1 == m_mapFiles.Length())
			pFile->m_fDefault = true;

		Check(UpdateFiles());
		Check(Save());

		Check(SwitchToFile(pFile));
	}
	else
	{
		hr = HrEnsureLastError();
		MessageBox(m_hwnd, L"The file must be on the same volume as the project file!", L"Path Error", MB_OK);
	}

Cleanup:
	SafeCloseFileHandle(hFile);
	RStrRelease(rstrPath);
	return hr;
}

HRESULT CQuadooProject::AddFile (RSTRING rstrPath, __deref_out CProjectFile** ppFile)
{
	HRESULT hr;
	CProjectFile* pFile = NULL;

	hr = m_mapFiles.Find(rstrPath, ppFile);
	if(FAILED(hr))
	{
		TVINSERTSTRUCT tvis = {0};
		PCWSTR pcwzLabel = TStrRChr(RStrToWide(rstrPath), L'\\');
		if(pcwzLabel)
			pcwzLabel++;

		pFile = __new CProjectFile(rstrPath);
		CheckAlloc(pFile);
		Check(m_mapFiles.Add(rstrPath, pFile));

		Check(Formatting::TBuildDirectory(RStrToWide(m_rstrProjectDir), RStrLen(m_rstrProjectDir), RStrToWide(rstrPath), RStrLen(rstrPath), &pFile->m_pwzAbsolutePath, &pFile->m_cchAbsolutePath));

		tvis.hParent        = m_hProjectRoot;
		tvis.hInsertAfter   = TVI_LAST;
		tvis.item.mask      = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
		tvis.item.pszText   = const_cast<PWSTR>(pcwzLabel);
		tvis.item.iImage    = 2;
		tvis.item.iSelectedImage = 2;
		tvis.item.lParam    = (LPARAM)pFile;

		pFile->m_hItem = TreeView_InsertItem(m_hwndTree, &tvis);
		CheckIfGetLastError(NULL == pFile->m_hItem);

		if(1 == m_mapFiles.Length())
			TreeView_Expand(m_hwndTree, m_hProjectRoot, TVE_EXPAND);

		*ppFile = pFile;
		pFile = NULL;
	}

Cleanup:
	SafeDelete(pFile);
	return hr;
}

HRESULT CQuadooProject::SaveAll (VOID)
{
	HRESULT hr = S_FALSE;
	sysint idxActive = m_pTabs->GetActiveTab();

	for(sysint idxTab = 0; idxTab < m_pTabs->Tabs(); idxTab++)
	{
		CProjectFile* pFile = m_pTabs->TGetTabData<CProjectFile>(idxTab);
		if(idxActive == idxTab)
		{
			if(SendMessage(m_hwndEditor, EM_GETMODIFY, 0, 0))
				Check(SaveTab(idxTab));
		}
		else if(pFile->m_fModified)
			Check(SaveTab(idxTab));
	}

Cleanup:
	return hr;
}

HRESULT CQuadooProject::RunScript (VOID)
{
	HRESULT hr;
	RSTRING rstrEngine = NULL, rstrTarget = NULL, rstrArgs = NULL, rstrScriptArgs = NULL, rstrStartDir = NULL;
	RSTRING rstrBatch = NULL, rstrBatchArgs = NULL;
	TStackRef<IJSONValue> srv;
	CProjectFile* pFile = FindDefaultScript();
	PWSTR pwzStartDir = NULL;
	INT cchStartDir = 0, nCompare;
	LONG_PTR nResult;

	Check(SaveAll());

	CheckIf(NULL == pFile, HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
	Check(FindInstalledEngine(&rstrEngine));
	Check(GetProjectTarget(&rstrTarget));

	Check(m_pProject->FindNonNullValueW(L"args", &srv));
	Check(srv->GetString(&rstrArgs));
	srv.Release();

	if(0 < RStrLen(rstrArgs))
		Check(RStrFormatW(&rstrScriptArgs, L"\"%ls\" %r", pFile->m_pwzAbsolutePath, rstrArgs));
	else
		Check(RStrFormatW(&rstrScriptArgs, L"\"%ls\"", pFile->m_pwzAbsolutePath));

	Check(m_pProject->FindNonNullValueW(L"startDir", &srv));
	Check(srv->GetString(&rstrStartDir));
	Check(Formatting::TBuildDirectory(RStrToWide(m_rstrProjectDir), RStrLen(m_rstrProjectDir), RStrToWide(rstrStartDir), RStrLen(rstrStartDir), &pwzStartDir, &cchStartDir));

	Check(RStrCompareIW(rstrTarget, L"qvm", &nCompare));
	if(0 == nCompare)
	{
		WCHAR wzBatchFile[MAX_PATH], wzComSpec[MAX_PATH];

		Check(RStrFormatW(&rstrBatch, L"@Echo off\r\ntitle %r\r\n\"%r\" %r\r\npause\r\ndel \"%%~f0\"\r\n", pFile->m_rstrPath, rstrEngine, rstrScriptArgs));
		Check(Formatting::TPrintF(wzBatchFile, ARRAYSIZE(wzBatchFile), NULL, L"%r_run.cmd", m_rstrProjectDir));
		Check(Text::SaveToFileNoBOM(RStrToWide(rstrBatch), RStrLen(rstrBatch), CP_ACP, wzBatchFile));
		Check(RStrFormatW(&rstrBatchArgs, L"/c \"%ls\"", wzBatchFile));
		CheckIfGetLastError(0 == GetEnvironmentVariable(L"ComSpec", wzComSpec, ARRAYSIZE(wzComSpec)));
		Check(HrShellExecute(m_hwnd, L"open", wzComSpec, RStrToWide(rstrBatchArgs), pwzStartDir, SW_SHOW, nResult));
	}
	else
		Check(HrShellExecute(m_hwnd, L"open", RStrToWide(rstrEngine), RStrToWide(rstrScriptArgs), pwzStartDir, SW_SHOW, nResult));

Cleanup:
	if(FAILED(hr))
	{
		RSTRING rstrError;
		if(SUCCEEDED(RStrFormatW(&rstrError, L"Failed to run your project due to error %.8X!", hr)))
		{
			MessageBox(m_hwnd, RStrToWide(rstrError), L"Run Error", MB_OK | MB_ICONERROR);
			RStrRelease(rstrError);
		}
	}

	SafeDeleteArray(pwzStartDir);

	RStrRelease(rstrBatchArgs);
	RStrRelease(rstrBatch);

	RStrRelease(rstrStartDir);
	RStrRelease(rstrScriptArgs);
	RStrRelease(rstrArgs);
	RStrRelease(rstrTarget);
	RStrRelease(rstrEngine);
	return hr;
}

HRESULT CQuadooProject::GetProjectTarget (__deref_out RSTRING* prstrTarget)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;

	Check(m_pProject->FindNonNullValueW(L"target", &srv));
	Check(srv->GetString(prstrTarget));

Cleanup:
	return hr;
}

HRESULT CQuadooProject::FindInstalledEngine (__deref_out RSTRING* prstrEngine)
{
	HRESULT hr;
	HKEY hKey = NULL;
	WCHAR wzCommand[MAX_PATH], *pwzPtr;
	LONG cb = sizeof(wzCommand), cch;
	RSTRING rstrTarget = NULL;
	INT nResult;

	CheckWin32Error(RegOpenKey(HKEY_CLASSES_ROOT, L"QuadooScript\\shell\\open\\command", &hKey));
	CheckWin32Error(RegQueryValue(hKey, NULL, wzCommand, &cb));

	CheckIf(cb % sizeof(WCHAR), E_UNEXPECTED);
	cch = cb / sizeof(WCHAR);

	pwzPtr = wzCommand;
	if(0 < cch)
	{
		PCWSTR pcwzEnd = NULL;

		if(*pwzPtr == '"')
		{
			PCWSTR pcwzEnd = TStrChr(++pwzPtr, L'"');
			CheckIf(NULL == pcwzEnd, E_UNEXPECTED);

			cch = static_cast<INT>(pcwzEnd - pwzPtr);
		}
		else
			pcwzEnd = TStrChr(pwzPtr, L' ');

		if(pcwzEnd)
			cch = static_cast<INT>(pcwzEnd - pwzPtr);

		Check(GetProjectTarget(&rstrTarget));
		Check(RStrCompareIW(rstrTarget, L"wqvm", &nResult));

		if(0 == nResult)
		{
			pcwzEnd = TStrCchRChr(pwzPtr, cch, L'\\');
			if(pcwzEnd)
			{
				PWSTR pwzNewEnd;
				pcwzEnd++;
				Check(TStrCchCpyEx(const_cast<PWSTR>(pcwzEnd), ARRAYSIZE(wzCommand) - static_cast<INT>(pcwzEnd - wzCommand), L"WQVM.exe", &pwzNewEnd, NULL));
				cch = static_cast<INT>(pwzNewEnd - pwzPtr);
			}
		}
	}

	Check(RStrCreateW(cch, pwzPtr, prstrEngine));

Cleanup:
	RStrRelease(rstrTarget);

	if(hKey)
		RegCloseKey(hKey);
	return hr;
}

CProjectFile* CQuadooProject::GetProjectFromTreeItem (HTREEITEM hItem)
{
	TVITEM tvItem = {0};
	tvItem.hItem = hItem;
	tvItem.mask = TVIF_PARAM;
	if(TreeView_GetItem(m_hwndTree, &tvItem) && tvItem.lParam)
		return reinterpret_cast<CProjectFile*>(tvItem.lParam);
	return NULL;
}

CProjectFile* CQuadooProject::FindDefaultScript (VOID)
{
	for(sysint i = 0; i < m_mapFiles.Length(); i++)
	{
		CProjectFile* pFile = *m_mapFiles.GetValuePtr(i);
		if(pFile->m_fDefault)
			return pFile;
	}
	return NULL;
}

HRESULT CQuadooProject::SaveTabData (CProjectFile* pFile)
{
	HRESULT hr;

	CheckIf(NULL == m_hwndEditor, E_FAIL);

	pFile->m_cchTabText = GetWindowTextLength(m_hwndEditor);
	if(0 < pFile->m_cchTabText)
	{
		pFile->m_pwzTabText = __new WCHAR[pFile->m_cchTabText + 1];
		CheckAlloc(pFile->m_pwzTabText);
		GetWindowText(m_hwndEditor, pFile->m_pwzTabText, pFile->m_cchTabText + 1);
		pFile->m_fModified = FALSE != SendMessage(m_hwndEditor, EM_GETMODIFY, 0, 0);
	}

	hr = S_OK;

Cleanup:
	return hr;
}

HRESULT CQuadooProject::LoadTabData (CProjectFile* pFile)
{
	HRESULT hr;
	PWSTR pwzText = NULL;
	INT cchText;

	if(pFile->m_pwzTabText)
	{
		pwzText = pFile->m_pwzTabText;
		cchText = pFile->m_cchTabText;

		pFile->m_pwzTabText = NULL;
		pFile->m_cchTabText = 0;
	}
	else
		Check(Text::LoadFromFile(pFile->m_pwzAbsolutePath, &pwzText, &cchText));
	
	CheckIfGetLastError(!SetWindowTextW(m_hwndEditor, pwzText));
	Check(InitFormatText());
	SendMessage(m_hwndEditor, EM_SETMODIFY, pFile->m_fModified, 0);
	pFile->m_fModified = false;
	
Cleanup:
	if(FAILED(hr))
		SendMessage(m_hwndEditor, EM_SETMODIFY, false, 0);
	SafeDeleteArrayCount(pwzText, cchText);
	return hr;
}

HRESULT CQuadooProject::SaveTab (sysint idxTab)
{
	HRESULT hr;
	CProjectFile* pFile = m_pTabs->TGetTabData<CProjectFile>(idxTab);
	bool fActiveTab = false;
	RECT rc;

	if(m_pTabs->GetActiveTab() == idxTab)
	{
		Assert(NULL == pFile->m_pwzTabText);
		Check(SaveTabData(pFile));
		fActiveTab = true;
	}

	Check(Text::SaveToFile(pFile->m_pwzTabText, pFile->m_cchTabText, pFile->m_pwzAbsolutePath));

	if(fActiveTab)
	{
		// Clear the modified flag for the active tab's editor.
		SendMessage(m_hwndEditor, EM_SETMODIFY, 0, 0);
	}

	Check(m_pTabs->HighlightTab(idxTab, false));
	m_pTabs->GetTabsRect(0, 0, rc);
	InvalidateRect(m_hwnd, &rc, FALSE);

Cleanup:
	if(fActiveTab)
	{
		SafeDeleteArrayCount(pFile->m_pwzTabText, pFile->m_cchTabText);
		pFile->m_fModified = false;
	}
	return hr;
}

BOOL CQuadooProject::PromptToSaveTab (sysint idxTab)
{
	BOOL fProceed = FALSE;
	CProjectFile* pFile = m_pTabs->TGetTabData<CProjectFile>(idxTab);
	RSTRING rstrPrompt;

	if(SUCCEEDED(RStrFormatW(&rstrPrompt, L"Would you like to save changes to %r?", pFile->m_rstrPath)))
	{
		INT nResult = MessageBox(m_hwnd, RStrToWide(rstrPrompt), L"Save Changes?", MB_YESNOCANCEL);
		if(IDCANCEL != nResult)
		{
			fProceed = TRUE;
			if(IDYES == nResult)
			{
				HRESULT hrSave = SaveTab(idxTab);
				if(FAILED(hrSave))
				{
					RStrRelease(rstrPrompt); rstrPrompt = NULL;
					if(SUCCEEDED(RStrFormatW(&rstrPrompt, L"Failed to save file!  Error=0x%X", hrSave)))
						MessageBox(m_hwnd, RStrToWide(rstrPrompt), L"File Error", MB_OK);
					fProceed = FALSE;
				}
			}
		}
		RStrRelease(rstrPrompt);
	}

	return fProceed;
}

BOOL CQuadooProject::CloseTab (sysint idxTab)
{
	sysint idxActive = m_pTabs->GetActiveTab();
	if(idxActive == idxTab && SendMessage(m_hwndEditor, EM_GETMODIFY, 0, 0))
	{
		if(!PromptToSaveTab(idxTab))
			return FALSE;
	}
	else
	{
		CProjectFile* pFile = m_pTabs->TGetTabData<CProjectFile>(idxTab);
		if(pFile->m_fModified && !PromptToSaveTab(idxTab))
			return FALSE;
	}

	SideAssertHr(m_pTabs->RemoveTab(idxTab));

	RECT rc;
	m_pTabs->GetTabsRect(0, 0, rc);
	InvalidateRect(m_hwnd, &rc, FALSE);

	CProjectFile* pNext = m_pTabs->TGetTabData<CProjectFile>(idxTab);
	if(NULL == pNext && 0 < idxTab)
		pNext = m_pTabs->TGetTabData<CProjectFile>(idxTab - 1);

	if(NULL == pNext || FAILED(SwitchToFile(pNext)))
	{
		SetWindowText(m_hwndEditor, L" ");
		EnableWindow(m_hwndEditor, FALSE);
	}

	return TRUE;
}

VOID CQuadooProject::RemoveFilePrompt (CProjectFile* pFile)
{
	sysint idxTab = m_pTabs->FindTab(pFile);
	if(-1 == idxTab || CloseTab(idxTab))
	{
		TreeView_DeleteItem(m_hwndTree, pFile->m_hItem);
		m_mapFiles.Remove(pFile->m_rstrPath, NULL);
		__delete pFile;

		UpdateFiles();
		Save();
	}
}

HRESULT CQuadooProject::ShowProjectCompiler(VOID)
{
	HRESULT hr;
	RSTRING rstrTarget = NULL, rstrEngine = NULL;
	CProjectFile* pDefault = FindDefaultScript();
	CDialogHost dlgHost(m_hInstance);

	CheckIf(NULL == pDefault, E_UNEXPECTED);
	Check(GetProjectTarget(&rstrTarget));
	Check(FindInstalledEngine(&rstrEngine));

	Check(SaveAll());

	{
		CProjectCompilerDlg dlgCompiler(m_pProject, rstrTarget, rstrEngine, m_rstrProjectDir, pDefault->m_pwzAbsolutePath);

		Check(dlgHost.Display(m_hwnd, &dlgCompiler));
		if(dlgHost.GetReturnValue() == IDOK)
			Check(Save());
	}

Cleanup:
	RStrRelease(rstrTarget);
	RStrRelease(rstrEngine);
	return hr;
}

HRESULT CQuadooProject::InitFormatText (VOID)
{
	HRESULT hr;
	PWSTR pwzText = NULL;
	INT cchText;
	CHARRANGE OldRange;
	CHARFORMAT2 cf;
	int curPos = 0;
	int word_end_pos = 1;
	int addition_pos = 0;

	SendMessage(m_hwndEditor, EM_HIDESELECTION, 1, 0);

	cchText = GetWindowTextLength(m_hwndEditor);
	pwzText = __new WCHAR[cchText + 1];
	CheckAlloc(pwzText);

	GetWindowText(m_hwndEditor, pwzText, cchText + 1);

	SendMessage(m_hwndEditor, EM_EXGETSEL, 0, LPARAM(&OldRange));

	cf.cbSize = sizeof(CHARFORMAT2);
	cf.dwMask = CFM_COLOR;
	cf.dwEffects = 0;

	while(curPos != -1)
	{
		word_end_pos = SendMessage(m_hwndEditor, EM_FINDWORDBREAK, WB_MOVEWORDRIGHT, curPos);
		if(curPos >= word_end_pos)
			break;
		int cchWord = word_end_pos - curPos;

		PCWSTR pcwzWord = pwzText + curPos + addition_pos;
		for(INT j = cchWord - 1; j >= 0; j--)
		{
			if(pcwzWord[j] != L' ')
			{
				cchWord = j + 1;
				break;
			}
		}
		if(cchWord == 1 && (pcwzWord[0] == L'\n' || pcwzWord[0] == L'\r'))
			addition_pos++;
		if(cchWord == 1)
		{
			curPos = word_end_pos;
			continue;
		}

		for(INT i = 0; i < keyword_count; i++)
		{
			if(cchWord != keywords[i].cchKeyword)
				continue;
			if(0 == TStrCmpNAssert(pcwzWord, keywords[i].pcwzKeyword, cchWord))
			{
				CHARRANGE cr2;
				cr2.cpMin = curPos + 0;
				cr2.cpMax = curPos + keywords[i].cchKeyword;
				cf.crTextColor = keywords[i].crKeyword;
				SendMessage(m_hwndEditor, EM_SETSEL, (WPARAM)curPos, (LPARAM)(curPos + keywords[i].cchKeyword));
				SendMessage(m_hwndEditor, EM_SETCHARFORMAT, SCF_SELECTION | SCF_WORD, (LPARAM)&cf);
				break;
			}
		}
		curPos = word_end_pos;
	}

	SendMessage(m_hwndEditor, EM_EXSETSEL, 0, LPARAM(&OldRange));
	cf.crTextColor = RGB(0, 0, 0);
	SendMessage(m_hwndEditor, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);

	hr = S_OK;

Cleanup:
	SendMessage(m_hwndEditor, EM_HIDESELECTION, 0, 0);
	__delete_array pwzText;
	return hr;
}

VOID CQuadooProject::FormatWord (VOID)
{
	CHARRANGE cr1;
	SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&cr1);
	int word_start_pos = SendMessage(m_hwndEditor, EM_FINDWORDBREAK, WB_MOVEWORDLEFT, cr1.cpMin);

	CHARFORMAT cf;
	cf.cbSize = sizeof(CHARFORMAT);
	cf.dwMask = CFM_COLOR;
	cf.dwEffects = 0;
	int i;
	for(i = 0; i < keyword_count; i++)
	{
		FINDTEXTEX ft;
		CHARRANGE ch;
		ch.cpMin = word_start_pos;
		ch.cpMax = cr1.cpMin;
		ft.chrg = ch;
		ft.lpstrText = keywords[i].pcwzKeyword;
		int fIndex = SendMessage(m_hwndEditor, EM_FINDTEXTW, (WPARAM)FR_DOWN | FR_WHOLEWORD, (LPARAM)&ft);
		if(fIndex >= 0)
		{
			CHARRANGE cr2;
			cr2.cpMin = fIndex;
			cr2.cpMax = fIndex + keywords[i].cchKeyword;
			SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (WPARAM)&cr2);
			cf.crTextColor = keywords[i].crKeyword;
			SendMessage(m_hwndEditor, EM_SETCHARFORMAT, SCF_SELECTION | SCF_WORD, (LPARAM)&cf);
			cr2.cpMin = cr1.cpMin;
			cr2.cpMax = cr1.cpMin;
			SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (WPARAM)&cr2);
			break;
		}
	}
	if(i == keyword_count)
	{
		CHARRANGE cr2;
		cr2.cpMin = word_start_pos;
		cr2.cpMax = cr1.cpMin;
		SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (WPARAM)&cr2);
		cf.crTextColor = RGB(0, 0, 0);
		SendMessage(m_hwndEditor, EM_SETCHARFORMAT, SCF_SELECTION | SCF_WORD, (LPARAM)&cf);
		cr2.cpMin = cr1.cpMin;
		cr2.cpMax = cr1.cpMin;
		SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (WPARAM)&cr2);
	}
}
