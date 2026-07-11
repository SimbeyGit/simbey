#include <windows.h>
#include <shlwapi.h>
#include <commctrl.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Core\MemoryStream.h"
#include "Library\Util\Formatting.h"
#include "Library\Util\TextHelpers.h"
#include "Library\Util\Shell.h"
#include "Library\Util\Registry.h"
#include "Library\ChooseFile.h"
#include "Library\MenuUtil.h"
#include "Library\Window\DialogHost.h"
#include "Published\JSON.h"
#include "Published\QuadooParser.h"
#include "Keywords.h"
#include "Tabs.h"
#include "DarkMode.h"
#include "CustomMenu.h"
#include "RunParamsDlg.h"
#include "WebParamsDlg.h"
#include "ProjectCompilerDlg.h"
#include "GotoDefinitionDlg.h"
#include "RunWebServiceDlg.h"
#include "StartDebuggerDlg.h"
#include "DebugProtocol.h"
#include "DebugSession.h"
#include "QuadooCallStackDisplay.h"
#include "QuadooProject.h"

const WCHAR c_wzQuadooProjectClass[] = L"QuadooProjectCls";
const WCHAR c_wzFontKey[] = L"Software\\Simbey\\QuadooStudio";

class CVariableResolver :
	public CBaseUnknown,
	public IQuadooEnumVariables
{
private:
	RSTRING m_rstrName;
	BOOL m_fFound;
	QUADOO_DEBUG_VARIABLE_QUERY m_query;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooEnumVariables)
	END_UNK_MAP

	CVariableResolver (RSTRING rstrName) :
		m_rstrName(rstrName),
		m_fFound(FALSE)
	{
		RStrAddRef(m_rstrName);
		ZeroMemory(&m_query, sizeof(m_query));
	}

	~CVariableResolver ()
	{
		RStrRelease(m_rstrName);
	}

	BOOL GetQuery (__out QUADOO_DEBUG_VARIABLE_QUERY* pQuery) const
	{
		if(!m_fFound)
			return FALSE;
		*pQuery = m_query;
		return TRUE;
	}

	virtual STDMETHODIMP EnumCallback (const SCOPE_PARENT& scope, RSTRING rstrVariableW,
		PCWSTR pcwzDeclaredType, BOOL fStatic, LONG idxOffset, __out BOOL* pfContinue)
	{
		UNREFERENCED_PARAMETER(pcwzDeclaredType);
		if(0 == TStrCmpAssert(RStrToWide(m_rstrName), RStrToWide(rstrVariableW)))
		{
			if(fStatic)
				m_query.nKind = QDVC_GLOBAL;
			else if(scope.pcwzType && (0 == TStrCmpAssert(scope.pcwzType, L"class") ||
				0 == TStrCmpAssert(scope.pcwzType, L"lambda")))
			{
				m_query.nKind = QDVC_MEMBER;
				m_query.nBaseOffset = -1;
			}
			else
				m_query.nKind = QDVC_LOCAL;
			m_query.nOffset = idxOffset;
			m_fFound = TRUE;
			*pfContinue = FALSE;
		}
		return S_OK;
	}
};

///////////////////////////////////////////////////////////////////////////////
// CProjectFile
///////////////////////////////////////////////////////////////////////////////

bool CProjectFile::IsModified (VOID)
{
	return m_pTabDocument->IsModified();
}

HRESULT CProjectFile::SaveFromEditor (ICodeEditor* pEditor)
{
	Assert(NULL == m_pTabDocument);

	SetInterface(m_pTabDocument, pEditor->GetTextDocument());
	pEditor->GetTextEditView(&m_tev);
	return S_OK;
}

HRESULT CProjectFile::RestoreEditor (ICodeEditor* pEditor)
{
	HRESULT hr;

	CheckIfIgnore(NULL == m_pTabDocument, S_FALSE);
	pEditor->SetTextDocument(m_pTabDocument);
	pEditor->SetTextEditView(&m_tev);
	SafeRelease(m_pTabDocument);
	hr = S_OK;

Cleanup:
	return hr;
}

HRESULT CProjectFile::SaveToStorage (ICodeEditor* pEditor, bool fActiveTab)
{
	HRESULT hr;
	ITextDocument* pDocument = m_pTabDocument;
	CMemoryStream stmText;

	if(fActiveTab)
	{
		Assert(NULL == pDocument);
		pDocument = pEditor->GetTextDocument();
	}

	Check(pDocument->StreamOut(&stmText));
	Check(Text::SaveToFile(stmText.TGetReadPtr<WCHAR>(), stmText.TDataRemaining<WCHAR>(), m_pwzAbsolutePath));
	pDocument->ResetModifiedSnapshot();

Cleanup:
	return hr;
}

HRESULT CProjectFile::ResizeCustomLayout (INT x, INT y, INT nWidth, INT nHeight, __out INT* pnDocHeight)
{
	*pnDocHeight = 0;
	return S_FALSE;
}

VOID CProjectFile::CheckAutoIndent (ICodeEditor* pEditor, ULONG nLine, WCHAR wchInsert)
{
	if(L'\n' == wchInsert || L'}' == wchInsert || L':' == wchInsert)
	{
		ITextDocument* pDocument = pEditor->GetTextDocument();
		size_w idxCur = pDocument->LineOffset(nLine);
		size_w idxPrev = pDocument->LineOffset(nLine - 1);
		size_w idxNext = pDocument->LineOffset(nLine + 1);
		size_w cchPrev = idxCur - idxPrev, cchCur = idxNext - idxCur;
		PWSTR pwzPrev = __new WCHAR[cchPrev + 1];
		if(pwzPrev)
		{
			if(SUCCEEDED(pDocument->Render(idxPrev, pwzPrev, cchPrev, &cchPrev)))
			{
				PWSTR pwzCur = __new WCHAR[cchCur + 1];
				if(pwzCur)
				{
					if(SUCCEEDED(pDocument->Render(idxCur, pwzCur, cchCur, &cchCur)))
					{
						INT nIndentation = 0;

						pwzPrev[cchPrev] = L'\0';
						pwzCur[cchCur] = L'\0';

						for(size_w i = 0; i < cchPrev; i++)
						{
							if(pwzPrev[i] != L'\t')
								break;
							nIndentation++;
						}

						nIndentation += ScanIndentationSyntax(pwzPrev + nIndentation, cchPrev - nIndentation);
						nIndentation += ScanIndentationSyntax(pwzCur, cchCur);

						if(0 > nIndentation)
							nIndentation = 0;

						pEditor->AdjustIndentation(nLine, nIndentation);
					}
					__delete_array pwzCur;
				}
			}
			__delete_array pwzPrev;
		}
	}
}

INT CProjectFile::ScanIndentationSyntax (PCWSTR pcwzCode, size_w cchCode)
{
	INT nIndentation = 0;
	bool fQuoted = false;

	for(size_w i = 0; i < cchCode; i++)
	{
		WCHAR wch = pcwzCode[i];
		if(wch == L'"')
			fQuoted = !fQuoted;
		else if(!fQuoted)
		{
			if(L'{' == wch || L':' == wch)
				nIndentation++;
			else if(L'}' == wch)
				nIndentation--;
			else if(L'/' == wch && (L'/' == pcwzCode[i + 1] || L'*' == pcwzCode[i + 1]))
				break;
		}
	}
	return nIndentation;
}

bool CProjectFile::HasBreakpoint (ULONG nLine) const
{
	sysint idx;
	return m_aBreakpoints.IndexOf(nLine, idx);
}

HRESULT CProjectFile::ToggleBreakpoint (ULONG nLine)
{
	sysint idx;
	if(m_aBreakpoints.IndexOf(nLine, idx))
	{
		m_aBreakpoints.Remove(idx, NULL);
		return S_FALSE;
	}
	return m_aBreakpoints.Append(nLine);
}

VOID CProjectFile::AdjustBreakpoints (const TVNLINESCHANGED* pLinesChanged, __out BOOL* pfChanged)
{
	ULONG nAdjustLine = pLinesChanged->fAtLineStart ? pLinesChanged->nLineNo : pLinesChanged->nLineNo + 1;
	ULONG nRemoveEnd = nAdjustLine + pLinesChanged->cOldLines;
	LONG nDelta = static_cast<LONG>(pLinesChanged->cNewLines) - static_cast<LONG>(pLinesChanged->cOldLines);

	*pfChanged = FALSE;

	if(0 < pLinesChanged->cOldLines)
	{
		for(sysint i = m_aBreakpoints.Length() - 1; i >= 0; i--)
		{
			ULONG nBreakpoint = m_aBreakpoints[i];
			if(nAdjustLine <= nBreakpoint && nBreakpoint < nRemoveEnd)
			{
				m_aBreakpoints.Remove(i, NULL);
				*pfChanged = TRUE;
			}
		}
	}

	if(0 != nDelta)
	{
		for(sysint i = 0; i < m_aBreakpoints.Length(); i++)
		{
			if(m_aBreakpoints[i] >= nRemoveEnd)
			{
				m_aBreakpoints[i] = static_cast<ULONG>(static_cast<LONG>(m_aBreakpoints[i]) + nDelta);
				*pfChanged = TRUE;
			}
		}
	}
}

HRESULT CProjectFile::LoadBreakpoints (IJSONArray* pBreakpoints)
{
	HRESULT hr = S_OK;

	m_aBreakpoints.Clear();
	if(pBreakpoints)
	{
		for(sysint i = 0; i < pBreakpoints->Count(); i++)
		{
			TStackRef<IJSONValue> srv;
			INT nLine;

			Check(pBreakpoints->GetValue(i, &srv));
			if(srv && SUCCEEDED(srv->GetInteger(&nLine)) && 0 < nLine)
			{
				ULONG nEditorLine = static_cast<ULONG>(nLine - 1);
				if(!HasBreakpoint(nEditorLine))
					Check(m_aBreakpoints.Append(nEditorLine));
			}
		}
	}

Cleanup:
	return hr;
}

HRESULT CProjectFile::SaveBreakpoints (__deref_out IJSONArray** ppBreakpoints)
{
	HRESULT hr;
	TStackRef<IJSONArray> srBreakpoints;
	TStackRef<IJSONValue> srv;

	Check(JSONCreateArray(&srBreakpoints));
	for(sysint i = 0; i < m_aBreakpoints.Length(); i++)
	{
		Check(JSONCreateInteger(static_cast<INT>(m_aBreakpoints[i] + 1), &srv));
		Check(srBreakpoints->Add(srv));
		srv.Release();
	}

	*ppBreakpoints = srBreakpoints.Detach();

Cleanup:
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
// CQuadooProject
///////////////////////////////////////////////////////////////////////////////

CQuadooProject::CQuadooProject (HINSTANCE hInstance, IUpdateTitle* pTitle, CDarkMode* pdm, HWND hwndTree) :
	m_hInstance(hInstance),
	m_pTitle(pTitle),
	m_pdm(pdm),
	m_hwndTree(hwndTree),
	m_hCloseIcon(NULL),
	m_hDropDown(NULL),
	m_pTabs(NULL),
	m_rstrProject(NULL),
	m_rstrProjectDir(NULL),
	m_pProject(NULL),
	m_hProjectRoot(NULL),
	m_pEditor(NULL),
	m_pDebugger(NULL),
	m_pCallStack(NULL),
	m_pDebugTree(NULL),
	m_hwndDebugTip(NULL),
	m_nHoverRequest(0),
	m_fHoverPending(FALSE),
	m_rstrHoverName(NULL),
	m_rstrTooltipText(NULL)
{
	m_pTitle->AddRef();
}

CQuadooProject::~CQuadooProject()
{
	Assert(NULL == m_pCallStack);
	Assert(NULL == m_pEditor);

	SafeDelete(m_pDebugger);
	SafeRelease(m_pDebugTree);
	SafeDelete(m_pTabs);
	if(m_hwndDebugTip)
		DestroyWindow(m_hwndDebugTip);
	RStrRelease(m_rstrTooltipText);
	RStrRelease(m_rstrHoverName);

	DestroyIcon(m_hCloseIcon);
	DestroyIcon(m_hDropDown);

	SafeRelease(m_pProject);
	RStrRelease(m_rstrProjectDir);
	RStrRelease(m_rstrProject);

	m_pTitle->Release();
}

HRESULT CQuadooProject::Register (HINSTANCE hInstance)
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

	Check(UpdateColors());

	m_hCloseIcon = (HICON)LoadImage(m_hInstance, MAKEINTRESOURCE(IDI_CLOSE), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
	m_hDropDown = (HICON)LoadImage(m_hInstance, MAKEINTRESOURCE(IDI_DROP_DOWN), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);

	m_pTabs = __new CTabs(m_hCloseIcon, m_hDropDown);
	CheckAlloc(m_pTabs);
	Check(m_pTabs->LoadMetrics(hwndParent));
	m_pTabs->SetDarkMode(m_pdm->IsDarkMode());

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
	CheckIf(NULL == srv, HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
	Check(srv->GetObject(&m_pProject));

	srv.Release();
	Check(m_pProject->FindNonNullValueW(L"files", &srv));
	Check(srv->GetArray(&srFiles));

	for(sysint i = 0; i < srFiles->Count(); i++)
	{
		TStackRef<IJSONObject> srFile;
		CProjectTab* pFile;

		Check(srFiles->GetObject(i, &srFile));

		srv.Release();
		Check(srFile->FindNonNullValueW(L"path", &srv));
		Check(srv->GetString(&rstrPath));
		Check(AddFile(rstrPath, &pFile));

		srv.Release();
		if(SUCCEEDED(srFile->FindNonNullValueW(L"breakpoints", &srv)))
		{
			TStackRef<IJSONArray> srBreakpoints;
			Check(srv->GetArray(&srBreakpoints));
			Check(static_cast<CProjectFile*>(pFile)->LoadBreakpoints(srBreakpoints));
		}

		srv.Release();
		if(SUCCEEDED(srFile->FindNonNullValueW(L"default", &srv)))
		{
			Check(srv->GetBoolean(&pFile->m_fDefault));
			if(pFile->m_fDefault && FAILED(SwitchToFile(pFile)))
			{
				RSTRING rstrError;
				Check(RStrFormatW(&rstrError, L"Could not open %r (default file)!", pFile->m_rstrPath));
				MessageBox(m_hwnd, RStrToWide(rstrError), L"File Error", MB_OK | MB_ICONERROR);
				RStrRelease(rstrError);
			}
		}

		RStrRelease(rstrPath); rstrPath = NULL;
	}

	Check(RStrCreateW(TStrLenAssert(pcwzProject), pcwzProject, &m_rstrProject));

	if(IsWebProject() && 0 == srFiles->Count() && IDYES == MessageBox(m_hwnd, L"Would you like to add a default web service script to your project?", L"Add Web Service Script", MB_YESNO))
	{
		if(SUCCEEDED(NewFilePrompt()))
			SetPageScript(IDR_WEBSERVICE);
	}

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
		CProjectTab* pFile = *m_mapFiles.GetValuePtr(i);

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

		CProjectFile* pProjectFile = static_cast<CProjectFile*>(pFile);
		if(0 < pProjectFile->Breakpoints())
		{
			TStackRef<IJSONArray> srBreakpoints;
			Check(pProjectFile->SaveBreakpoints(&srBreakpoints));
			Check(JSONWrapArray(srBreakpoints, &srv));
			Check(srFile->AddValueW(L"breakpoints", srv));
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

	StopDebugger();
	m_mapFiles.DeleteAll();

	Destroy();
	return S_OK;
}

HRESULT CQuadooProject::SetPageScript (INT idScript)
{
	HRESULT hr;
	HMODULE hModule = GetModuleHandle(NULL);
	PWSTR pwzScript = NULL;
	HRSRC hResource = FindResourceExW(hModule, L"TEXT", MAKEINTRESOURCEW(idScript), 0);
	HGLOBAL hMemory = NULL;
	PVOID pvResource = NULL;
	DWORD cbSize;
	INT cchScript;

	CheckIfGetLastError(NULL == hResource);
	cbSize = SizeofResource(hModule, hResource);

	hMemory = LoadResource(hModule, hResource);
	CheckIfGetLastError(NULL == hMemory);

	pvResource = LockResource(hMemory);
	CheckIfGetLastError(NULL == pvResource);

	Check(Text::ConvertRawTextToUnicode(reinterpret_cast<PBYTE>(pvResource), cbSize, &pwzScript, &cchScript));
	Check(m_pEditor->Prepare(pwzScript, cchScript));

	sysint idxTab = m_pTabs->GetActiveTab();
	if(-1 != idxTab)
	{
		CProjectTab* pFile = m_pTabs->TGetTabData<CProjectTab>(idxTab);
		m_pTabs->HighlightTab(idxTab, true);
	}

Cleanup:
	SafeDeleteArray(pwzScript);
	return hr;
}

VOID CQuadooProject::UpdateColorScheme (VOID)
{
	RECT rc;

	m_pTabs->SetDarkMode(m_pdm->IsDarkMode());
	m_pTabs->GetTabsRect(0, 0, rc);
	InvalidateRect(m_hwnd, &rc, FALSE);

	m_pEditor->SetDarkMode(m_pdm->IsDarkMode(), !m_pdm->HasThemes());
	UpdateColors();

	if(m_pCallStack)
		m_pCallStack->UpdateColorScheme(m_pdm->IsDarkMode());
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
			nCmd = CustomTrackPopupMenu(hPopup, TPM_LEFTALIGN | TPM_RETURNCMD | TPM_NONOTIFY, ptScreen);
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
			CProjectTab* pFile = GetProjectFromTreeItem(hItem);

			if(pFile->m_fDefault)
				CheckMenuItem(hPopup, ID_CONTEXT_SETDEFAULT, MF_BYCOMMAND | MF_CHECKED);

			nCmd = CustomTrackPopupMenu(hPopup, TPM_LEFTALIGN | TPM_RETURNCMD | TPM_NONOTIFY, ptScreen);
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
	CProjectTab* pFile = GetProjectFromTreeItem(hItem);
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
	BOOL fDebugging = IsDebugging();
	BOOL fDebugPaused = fDebugging && m_pDebugger->IsPaused();

	for(ULONG i = 0; i < cCmds; i++)
	{
		switch(prgCmds[i].cmdID)
		{
		case ID_PROJECT_RUNPARAMETERS:
		case ID_PROJECT_ADDFILE:
		case ID_PROJECT_NEWFILE:
		case ID_CONTEXT_REMOVE:
		case ID_VIEW_OPTIONS:
			prgCmds[i].cmdf = OLECMDF_SUPPORTED | OLECMDF_ENABLED;
			break;
		case ID_RUN_SCRIPT:
			prgCmds[i].cmdf = OLECMDF_SUPPORTED;
			if(!fDebugging && NULL != FindDefaultScript())
				prgCmds[i].cmdf |= OLECMDF_ENABLED;
			break;
		case ID_DEBUG_SCRIPT:
			prgCmds[i].cmdf = OLECMDF_SUPPORTED;
			if(fDebugPaused || (!fDebugging && NULL != FindDefaultScript() && !IsWebProject()))
				prgCmds[i].cmdf |= OLECMDF_ENABLED;
			break;
		case ID_PROJECT_COMPILE:
			prgCmds[i].cmdf = OLECMDF_SUPPORTED;
			if(!fDebugging && NULL != FindDefaultScript() && !IsWebProject())
				prgCmds[i].cmdf |= OLECMDF_ENABLED;
			break;
		case ID_FILE_SAVEFILE:
			prgCmds[i].cmdf = OLECMDF_SUPPORTED;
			if(m_pTabs->GetActiveTabHighlight())
				prgCmds[i].cmdf |= OLECMDF_ENABLED;
			break;
		case ID_STEP_INTO:
		case ID_STEP_OVER:
			prgCmds[i].cmdf = OLECMDF_SUPPORTED;
			if(fDebugPaused)
				prgCmds[i].cmdf |= OLECMDF_ENABLED;
			break;
		case ID_DEBUG_STOP:
			prgCmds[i].cmdf = OLECMDF_SUPPORTED;
			if(fDebugging)
				prgCmds[i].cmdf |= OLECMDF_ENABLED;
			break;
		case ID_TOGGLE_BREAKPOINT:
			prgCmds[i].cmdf = OLECMDF_SUPPORTED;
			if(!IsWebProject())
				prgCmds[i].cmdf |= OLECMDF_ENABLED;
			break;
		case ID_NEXT_TAB:
			prgCmds[i].cmdf = OLECMDF_SUPPORTED;
			if(1 < m_pTabs->GetVisibleTabs())
				prgCmds[i].cmdf |= OLECMDF_ENABLED;
			break;
		case ID_CLOSE_TAB:
			prgCmds[i].cmdf = OLECMDF_SUPPORTED;
			if(0 < m_pTabs->GetVisibleTabs())
				prgCmds[i].cmdf |= OLECMDF_ENABLED;
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
			CProjectTab* pFile = GetProjectFromTreeItem((HTREEITEM)pvaIn->byref);

			hr = S_FALSE;
			if(pFile)
			{
				CProjectTab* pDefault = FindDefaultScript();
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
			CProjectTab* pFile = GetProjectFromTreeItem((HTREEITEM)pvaIn->byref);
			if(pFile)
				RemoveFilePrompt(pFile);
			hr = S_OK;
		}
		break;

	case ID_DEBUG_SCRIPT:
		if(IsDebugging())
			hr = m_pDebugger->IsPaused() ? SendDebuggerCommand(QDM_CONTINUE) : HRESULT_FROM_WIN32(ERROR_BUSY);
		else
			hr = DebugScript();
		break;

	case ID_RUN_SCRIPT:
		hr = IsDebugging() ? HRESULT_FROM_WIN32(ERROR_BUSY) : RunScript();
		break;

	case ID_PROJECT_COMPILE:
		hr = IsDebugging() ? HRESULT_FROM_WIN32(ERROR_BUSY) : ShowProjectCompiler();
		break;

	case ID_VIEW_OPTIONS:
		{
			COLORREF crCustom[16];
			INT cColors = ARRAYSIZE(crCustom), nFontSize;
			BOOL fChanged;

			Registry::LoadCustomColors(NULL, crCustom, &cColors);
			hr = m_pEditor->DisplayOptions(crCustom, cColors, &fChanged, &nFontSize);
			if(SUCCEEDED(hr))
			{
				if(fChanged)
					Registry::SaveCustomColors(NULL, crCustom, cColors);

				SaveDefaultFont(nFontSize);
			}
		}
		break;

	case ID_STEP_INTO:
		hr = SendDebuggerCommand(QDM_STEP_INTO);
		break;

	case ID_STEP_OVER:
		hr = SendDebuggerCommand(QDM_STEP_OVER);
		break;

	case ID_DEBUG_STOP:
		StopDebugger();
		hr = S_OK;
		break;

	case ID_TOGGLE_BREAKPOINT:
		{
			TEXT_EDIT_VIEW tev;

			m_pEditor->GetTextEditView(&tev);
			hr = ToggleBreakpoint(tev.nCurrentLine);
		}
		break;

	case ID_NEXT_TAB:
		{
			sysint idxVisualTab = m_pTabs->ResolveVisualIndex(m_pTabs->GetActiveTab());
			if(idxVisualTab < m_pTabs->GetVisibleTabs() - 1)
				idxVisualTab++;
			else
				idxVisualTab = 0;
			hr = SwitchToFile(m_pTabs->TGetTabData<CProjectTab>(m_pTabs->GetTabByVisualIndex(idxVisualTab)));
		}
		break;

	case ID_CLOSE_TAB:
		hr = CloseTab(m_pTabs->GetActiveTab()) ? S_OK : S_FALSE;
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
	SafeRelease(m_pEditor);
}

HRESULT CQuadooProject::FinalizeAndShow (DWORD dwStyle, INT nCmdShow)
{
	return __super::FinalizeAndShow(dwStyle, nCmdShow);
}

BOOL CQuadooProject::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	switch(message)
	{
	case WM_QUADOO_DEBUG_SOCKET:
		if(m_pDebugger)
		{
			if(FAILED(m_pDebugger->OnSocketEvent(wParam, lParam)))
				StopDebugger();
			else
			{
				SideAssertHr(ShowDebuggerLocation());
				if(m_pCallStack)
					SideAssertHr(m_pCallStack->SetFrames(m_pDebugger));
				UpdateVariableTooltip();
			}
		}
		lResult = 0;
		return TRUE;

	case WM_CREATE:
		{
			RECT rcSite;
			GetClientRect(m_hwnd, &rcSite);

			if(FAILED(CodeEditCreate(m_hwnd, rcSite, 4, m_pdm->IsDarkMode(), !m_pdm->HasThemes(), &m_pEditor)))
			{
				lResult = -1;
				return TRUE;
			}

			m_pEditor->SetStyleMask(0, TXS_SELMARGIN | TXS_LEFTMARGIN);
			LoadDefaultFont();
		}

		m_pEditor->EnableEditor(FALSE);
		break;

	case WM_DRAWITEM:
		if(DrawBreakpoint(reinterpret_cast<DRAWITEMSTRUCT*>(lParam)))
			return TRUE;
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
		ResizeEditor(HIWORD(lParam));
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
			CProjectTab* pFile = m_pTabs->TFindTabFromPoint<CProjectTab>(LOWORD(lParam), HIWORD(lParam));
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
				SwitchToFile(m_pTabs->TGetTabData<CProjectTab>(idxTab));
		}
		break;

	case WM_NOTIFY:
		{
			NMHDR* pnmhdr = reinterpret_cast<NMHDR*>(lParam);

			switch(pnmhdr->code)
			{
			case TVN_MOUSE_HOVER:
				SideAssertHr(BeginVariableHover(static_cast<TVNMOUSEHOVER*>(pnmhdr)));
				break;
			case TVN_MOUSE_MOVE:
			case TVN_MOUSE_LEAVE:
				HideDebuggerTooltip();
				break;
			case TVN_CURSOR_CHANGE:
				SendMessage(GetParent(m_hwnd), WM_NOTIFY, wParam, lParam);
				break;
			case TVN_MARGIN_CLICK:
				{
					TVNMARGINCLICK* pMarginClick = static_cast<TVNMARGINCLICK*>(pnmhdr);
					if(SUCCEEDED(ToggleBreakpoint(pMarginClick->nLineNo)))
						pMarginClick->fHandled = TRUE;
				}
				break;
			case TVN_LINES_CHANGED:
				SideAssertHr(AdjustBreakpoints(static_cast<TVNLINESCHANGED*>(pnmhdr)));
				break;
			case TVN_SYNTAX_HIGHLIGHT:
				{
					TVNSYNTAXHIGHLIGHT* pHighlight = static_cast<TVNSYNTAXHIGHLIGHT*>(pnmhdr);
					ApplySyntaxColoring(pHighlight);
				}
				break;
			case TVN_INIT_CONTEXT_MENU:
				{
					TVNMCONTEXTMENU* pContext = static_cast<TVNMCONTEXTMENU*>(pnmhdr);
					PCWSTR pcwzWord = pContext->pcwzWord;
					if(pcwzWord && SUCCEEDED(ExtractFindSymbol(pcwzWord, pContext->nOffset - pContext->nWordOffset)))
					{
						WCHAR wzCmd[200];
						if(SUCCEEDED(Formatting::TPrintF(wzCmd, ARRAYSIZE(wzCmd), NULL, L"Find %ls...", m_wzFindSymbol)))
						{
							HMENU hMenu = pContext->hMenu;
							MENUITEMINFO mii = {0};

							mii.cbSize = sizeof(mii);

							mii.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STRING;
							mii.wID = ID_FIND_SYMBOL;
							mii.dwTypeData = wzCmd;
							InsertMenuItem(hMenu, 0, TRUE, &mii);

							mii.fMask = MIIM_FTYPE;
							mii.fType = MFT_MENUBARBREAK;
							InsertMenuItem(hMenu, 1, TRUE, &mii);
						}
					}

					if(m_pdm->IsDarkMode())
					{
						TStackRef<IBaseWindow> srWindow;
						if(SUCCEEDED(m_pEditor->QueryInterface(&srWindow)))
						{
							CCustomMenu* pCustomMenu = __new CCustomMenu(pContext->hMenu);
							pCustomMenu->SetDarkMode();
							srWindow->AttachSubclassHandler(pCustomMenu);
							pCustomMenu->Rebuild(pContext->hMenu);

							pContext->pvUserParam = pCustomMenu;
						}
					}
				}
				break;
			case TVN_CLOSE_CONTEXT_MENU:
				{
					TVNMCLOSECONTEXT* pClose = static_cast<TVNMCLOSECONTEXT*>(pnmhdr);
					if(pClose->pvUserParam)
					{
						CCustomMenu* pCustomMenu = reinterpret_cast<CCustomMenu*>(pClose->pvUserParam);
						TStackRef<IBaseWindow> srWindow;
						if(SUCCEEDED(m_pEditor->QueryInterface(&srWindow)))
							srWindow->DetachSubclassHandler(pCustomMenu);
						__delete pCustomMenu;
					}
				}
				break;
			case TVN_ENTER_CHAR:
				{
					sysint idxTab = m_pTabs->GetActiveTab();
					if(-1 != idxTab)
					{
						TVNENTERCHAR* pEC = static_cast<TVNENTERCHAR*>(pnmhdr);
						CProjectTab* pFile = m_pTabs->TGetTabData<CProjectTab>(idxTab);
						pFile->CheckAutoIndent(m_pEditor, pEC->nLineNo, pEC->wch);
					}
				}
				break;
			}
		}
		break;

	case WM_COMMAND:
		if(EN_UPDATE == HIWORD(wParam))
		{
			if(m_pEditor->IsModified())
			{
				sysint idxTab = m_pTabs->GetActiveTab();
				if(-1 != idxTab)
				{
					CProjectTab* pFile = m_pTabs->TGetTabData<CProjectTab>(idxTab);
					RECT rc;

					m_pTabs->HighlightTab(idxTab, true);

					m_pTabs->GetTabsRect(0, 0, rc);
					InvalidateRect(m_hwnd, &rc, FALSE);
				}
			}
		}
		else if(0 == HIWORD(wParam) && ID_FIND_SYMBOL == LOWORD(wParam))
			FindSymbol();
		break;

	case WM_SETFOCUS:
		m_pEditor->SetFocus();
		break;
	}

	return __super::DefWindowProc(message, wParam, lParam, lResult);
}

HRESULT CQuadooProject::SwitchToFile (CProjectTab* pFile)
{
	HRESULT hr;
	RSTRING rstrLabel = NULL;
	HDC hdc = NULL;
	RECT rc;
	sysint idxTab = m_pTabs->GetActiveTab();
	bool fResize = false;

	if(-1 != idxTab)
	{
		CProjectTab* pTab = m_pTabs->TGetTabData<CProjectTab>(idxTab);
		fResize = S_OK == pTab->CloseCustomLayout();
		SaveTabData(pTab);
	}

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

	if(S_OK == pFile->OpenCustomLayout(m_pEditor) || fResize)
	{
		GetClientRect(m_hwnd, &rc);
		ResizeEditor(rc.bottom);
	}

	m_pTabs->GetTabsRect(0, 0, rc);
	InvalidateRect(m_hwnd, &rc, FALSE);

	PostMessage(m_hwnd, WM_SETFOCUS, 0, 0);

Cleanup:
	m_pEditor->EnableEditor(SUCCEEDED(hr));
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

	if(IsWebProject())
	{
		if(FAILED(FindActiveQuadoo(&rstrEngine)))
			Check(RStrCreateW(LSP(L"NOT INSTALLED"), &rstrEngine));

		{
			CWebParamsDlg dlgParams(m_pProject, rstrEngine, m_rstrProjectDir);
			Check(dlgHost.Display(m_hwnd, &dlgParams));
			CheckIfIgnore(IDCANCEL == dlgHost.GetReturnValue(), E_ABORT);
		}
	}
	else
	{
		FindInstalledEngine(&rstrEngine);

		{
			CRunParamsDlg dlgParams(m_pProject, rstrEngine, m_rstrProjectDir);
			Check(dlgHost.Display(m_hwnd, &dlgParams));
		}
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
	CProjectTab* pFile;

	Check(pick.Initialize());
	CheckIfIgnore(!pick.OpenSingleFile(m_hwnd, L"Add Existing File To Project",
		IsWebProject() ? L"ASP (*.asp)\0*.asp\0" : L"QuadooScript (*.quadoo)\0*.quadoo\0"), E_ABORT);

	hr = CreateRelativeProjectPath(wzRelative, ARRAYSIZE(wzRelative), pick.GetFile(0), 0);
	if(SUCCEEDED(hr))
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
		MessageBox(m_hwnd, L"The file must be on the same volume as the project file!", L"Path Error", MB_OK);

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
	CProjectTab* pFile;
	HANDLE hFile = INVALID_HANDLE_VALUE;

	Check(pick.Initialize());
	Check(pick.SetInitialDirectory(RStrToWide(m_rstrProjectDir)));
	CheckIfIgnore(!pick.SaveFile(m_hwnd, L"Add New File To Project",
		IsWebProject() ? L"ASP (*.asp)\0*.asp\0" : L"QuadooScript (*.quadoo)\0*.quadoo\0"), E_ABORT);

	hr = CreateRelativeProjectPath(wzRelative, ARRAYSIZE(wzRelative), pick.GetFile(0), 0);
	if(SUCCEEDED(hr))
	{
		Check(RStrCreateW(TStrLenAssert(wzRelative), wzRelative, &rstrPath));
		CheckIf(m_mapFiles.HasItem(rstrPath), E_FAIL);

		hFile = CreateFileW(pick.GetFile(0), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
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
		MessageBox(m_hwnd, L"The file must be on the same volume as the project file!", L"Path Error", MB_OK);

Cleanup:
	SafeCloseFileHandle(hFile);
	RStrRelease(rstrPath);
	return hr;
}

HRESULT CQuadooProject::AddFile (RSTRING rstrPath, __deref_out CProjectTab** ppFile)
{
	HRESULT hr;
	CProjectTab* pFile = NULL;

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

VOID CQuadooProject::ResizeEditor (INT nWindowHeight)
{
	TStackRef<IBaseWindow> srEditorWin;
	const SIZE* pcszTabs = m_pTabs->GetSize();
	INT nEditorHeight, nDocHeight = 0;
	sysint idxTab = m_pTabs->GetActiveTab();

	SideAssertHr(m_pEditor->QueryInterface(&srEditorWin));

	nEditorHeight = (nWindowHeight - pcszTabs->cy) - 1;
	if(-1 != idxTab)
	{
		CProjectTab* pFile = m_pTabs->TGetTabData<CProjectTab>(idxTab);
		pFile->ResizeCustomLayout(1, pcszTabs->cy, pcszTabs->cx - 2, nEditorHeight, &nDocHeight);
	}

	if(NULL == m_pCallStack)
		srEditorWin->Move(1, pcszTabs->cy + nDocHeight, pcszTabs->cx - 2, nEditorHeight - nDocHeight, TRUE);
	else
	{
		const INT nCallStackHeight = CQuadooCallStackDisplay::GetDefaultHeight();
		INT nCodeHeight = max(0, nEditorHeight - nDocHeight - nCallStackHeight);
		INT yCallStack = pcszTabs->cy + nDocHeight + nCodeHeight;

		srEditorWin->Move(1, pcszTabs->cy + nDocHeight, pcszTabs->cx - 2, nCodeHeight, TRUE);
		SideAssertHr(m_pCallStack->Move(1, yCallStack, pcszTabs->cx - 2, nCallStackHeight, TRUE));
	}
}

HRESULT CQuadooProject::UpdateColors (VOID)
{
	HRESULT hr = S_OK;
	INT cKeywords;
	const KEYWORD* pcrgKeywords;

	m_mapKeywords.Clear();
	GetKeywords(&pcrgKeywords, &cKeywords);
	if(m_pdm->IsDarkMode())
	{
		for(INT i = 0; i < cKeywords; i++)
			Check(m_mapKeywords.Add(pcrgKeywords[i].pcwzKeyword, pcrgKeywords[i].crDarkMode));
	}
	else
	{
		for(INT i = 0; i < cKeywords; i++)
			Check(m_mapKeywords.Add(pcrgKeywords[i].pcwzKeyword, pcrgKeywords[i].crKeyword));
	}

	if(m_pdm->IsDarkMode())
	{
		m_crStrings = RGB(255, 80, 80);
		m_crCommentForeground = RGB(220, 220, 240);
		m_crCommentHighlight = RGB(90, 90, 110);
	}
	else
	{
		m_crStrings = RGB(80, 0, 0);
		m_crCommentForeground = RGB(255, 255, 255);
		m_crCommentHighlight = RGB(0, 0, 255);
	}

Cleanup:
	return hr;
}

INT CQuadooProject::CustomTrackPopupMenu (HMENU hPopup, UINT uFlags, const POINT& ptScreen)
{
	CCustomMenu* pCustomMenu = NULL;

	if(m_pdm->IsDarkMode())
	{
		pCustomMenu = __new CCustomMenu(hPopup);
		pCustomMenu->SetDarkMode();
		AttachSubclassHandler(pCustomMenu);
		pCustomMenu->Rebuild(hPopup);
	}

	INT nCmd = TrackPopupMenu(hPopup, TPM_LEFTALIGN | TPM_RETURNCMD | TPM_NONOTIFY, ptScreen.x, ptScreen.y, 0, m_hwnd, NULL);

	if(pCustomMenu)
	{
		DetachSubclassHandler(pCustomMenu);
		__delete pCustomMenu;
	}

	return nCmd;
}

HRESULT CQuadooProject::ExtractFindSymbol (PCWSTR pcwzWord, INT idxWordPtr)
{
	HRESULT hr;
	PCWSTR pcwzPtrA = TStrCchRChr(pcwzWord, idxWordPtr, L'(');
	PCWSTR pcwzPtrB = TStrCchRChr(pcwzWord, idxWordPtr, L')');
	PCWSTR pcwzPtr = (pcwzPtrA > pcwzPtrB) ? pcwzPtrA : pcwzPtrB;
	if(pcwzPtr)
	{
		idxWordPtr -= static_cast<INT>((pcwzPtr + 1) - pcwzWord);
		pcwzWord = pcwzPtr + 1;
	}

	while(Formatting::IsSpace(*pcwzWord) || L'[' == *pcwzWord || L'.' == *pcwzWord)
	{
		pcwzWord++;
		idxWordPtr--;
	}

	pcwzPtr = TFindEnd(pcwzWord + idxWordPtr, L"]().,;");
	if(pcwzPtr)
		hr = TStrCchCpyN(m_wzFindSymbol, ARRAYSIZE(m_wzFindSymbol), pcwzWord, static_cast<INT>(pcwzPtr - pcwzWord));
	else
		hr = TStrCchCpy(m_wzFindSymbol, ARRAYSIZE(m_wzFindSymbol), pcwzWord);

	return hr;
}

HRESULT CQuadooProject::FindSymbol (VOID)
{
	HRESULT hr;
	TStackRef<IQuadooDefinitions> srDefs;
	sysint idxActive = m_pTabs->GetActiveTab();
	RSTRING rstrFile = NULL;

	PCWSTR pcwzFind = TStrRChr(m_wzFindSymbol, L'.');
	if(pcwzFind)
		pcwzFind++;
	else
		pcwzFind = m_wzFindSymbol;

	CProjectTab* pFile = m_pTabs->TGetTabData<CProjectTab>(idxActive);

	Check(QuadooFindDefinitions(pFile->m_pwzAbsolutePath, pcwzFind, &srDefs));

	if(0 < srDefs->Count())
	{
		CGotoDefinitionDlg dlgGotoDef(srDefs);

		if(1 < srDefs->Count())
		{
			CDialogHost dlgHost(m_hInstance);

			Check(dlgHost.Display(m_hwnd, &dlgGotoDef));
			CheckIfIgnore(IDCANCEL == dlgHost.GetReturnValue(), E_ABORT);
		}

		INT nLine;
		PCWSTR pcwzToken;

		Check(srDefs->GetDefinition(dlgGotoDef.GetSelectedDef(), &rstrFile, &nLine, &pcwzToken));
		if(0 != TStrCmpIAssert(pFile->m_pwzAbsolutePath, RStrToWide(rstrFile)))
		{
			pFile = NULL;

			for(sysint i = 0; i < m_mapFiles.Length(); i++)
			{
				CProjectTab* pProjectFile = *m_mapFiles.GetValuePtr(i);
				if(0 == TStrCmpIAssert(pProjectFile->m_pwzAbsolutePath, RStrToWide(rstrFile)))
				{
					pFile = pProjectFile;
					break;
				}
			}

			CheckIf(NULL == pFile, HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
			SwitchToFile(pFile);
		}

		m_pEditor->ScrollView(0, nLine - 1);
	}

Cleanup:
	RStrRelease(rstrFile);
	return hr;
}

VOID CQuadooProject::ApplySyntaxColoring (TVNSYNTAXHIGHLIGHT* pHighlight)
{
	INT cch = pHighlight->nTextLen, idxKeyword = -1;
	PCWSTR pcwzText = pHighlight->pcwzText;
	WCHAR wchPrev = L'\0', wchPrevKeyword = L'\0';
	bool fQuoted = false;

	for(INT i = 0; i < cch; i++)
	{
		WCHAR wch = pcwzText[i];

		if(L'"' == wch)
		{
			if(fQuoted)
			{
				if(L'\\' != wchPrev)
					fQuoted = false;
			}
			else
			{
				if(-1 != idxKeyword)
				{
					ColorKeyword(pHighlight, wchPrevKeyword, idxKeyword, i);
					idxKeyword = -1;
				}

				fQuoted = true;
			}

			pHighlight->pAttr[i].fg = m_crStrings;
		}
		else if(fQuoted)
			pHighlight->pAttr[i].fg = m_crStrings;
		else if((wch >= L'a' && wch <= L'z') || (wch >= L'A' && wch <= L'Z'))
		{
			if(-1 == idxKeyword)
			{
				idxKeyword = i;
				wchPrevKeyword = wchPrev;
			}
		}
		else if(idxKeyword != -1)
		{
			ColorKeyword(pHighlight, wchPrevKeyword, idxKeyword, i);
			idxKeyword = -1;
		}
		else if(L'/' == wch && L'/' == wchPrev)
		{
			for(INT n = i - 1; n < cch; n++)
			{
				pHighlight->pAttr[n].fg = m_crCommentForeground;
				pHighlight->pAttr[n].bg = m_crCommentHighlight;
			}
			break;
		}

		wchPrev = wch;
	}

	if(-1 != idxKeyword)
		ColorKeyword(pHighlight, wchPrevKeyword, idxKeyword, cch);
}

VOID CQuadooProject::ColorKeyword (TVNSYNTAXHIGHLIGHT* pHighlight, WCHAR wchPrevKeyword, INT idxStart, INT idxEnd)
{
	if(L'.' != wchPrevKeyword)
	{
		WCHAR wzKeyword[64];
		INT cch = idxEnd - idxStart;

		if(cch < ARRAYSIZE(wzKeyword))
		{
			COLORREF cr;

			CopyMemory(wzKeyword, pHighlight->pcwzText + idxStart, cch * sizeof(WCHAR));
			wzKeyword[cch] = L'\0';

			if(SUCCEEDED(m_mapKeywords.Find(wzKeyword, &cr)))
			{
				for(INT i = 0; i < cch; i++)
					pHighlight->pAttr[idxStart + i].fg = cr;
			}
		}
	}
}

CProjectFile* CQuadooProject::GetActiveProjectFile (VOID)
{
	sysint idxTab = m_pTabs->GetActiveTab();
	if(-1 != idxTab)
		return static_cast<CProjectFile*>(m_pTabs->TGetTabData<CProjectTab>(idxTab));
	return NULL;
}

BOOL CQuadooProject::DrawBreakpoint (DRAWITEMSTRUCT* pdis)
{
	CProjectFile* pFile = GetActiveProjectFile();
	ITextDocument* pDocument = m_pEditor ? m_pEditor->GetTextDocument() : NULL;

	if(NULL == pdis || ODT_STATIC != pdis->CtlType || reinterpret_cast<ULONG_PTR>(pDocument) != pdis->itemData)
		return FALSE;

	if(pFile)
	{
		if(pFile->HasBreakpoint(pdis->itemID))
		{
			RECT rc = pdis->rcItem;
			INT nWidth = rc.right - rc.left;
			INT nHeight = rc.bottom - rc.top;
			INT nSize = min(nWidth - 6, nHeight - 4);
			if(2 < nSize)
			{
				INT x = rc.left + (nWidth - nSize) / 2 - 1;
				INT y = rc.top + (nHeight - nSize) / 2;
				HBRUSH hbrRed = CreateSolidBrush(RGB(255, 160, 160));
				HPEN hpnRed = CreatePen(PS_SOLID, 1, RGB(170, 40, 40));
				HGDIOBJ hbrOld = SelectObject(pdis->hDC, hbrRed);
				HGDIOBJ hpnOld = SelectObject(pdis->hDC, hpnRed);

				Ellipse(pdis->hDC, x, y, x + nSize, y + nSize);

				SelectObject(pdis->hDC, hpnOld);
				SelectObject(pdis->hDC, hbrOld);
				DeleteObject(hpnRed);
				DeleteObject(hbrRed);
			}
		}

		// Draw the active-line arrow after the breakpoint so it remains visible
		// when both indicators occupy the same margin cell.
		if(m_pDebugger && m_pDebugger->IsCurrentLocation(pFile->m_pwzAbsolutePath, pdis->itemID + 1))
		{
			RECT rc = pdis->rcItem;
			INT nWidth = rc.right - rc.left;
			INT nHeight = rc.bottom - rc.top;
			if(5 >= nWidth || 5 >= nHeight)
				return TRUE;
			INT xLeft = rc.left + 2;
			INT xTail = rc.left + max(3, nWidth / 3);
			INT xTip = rc.right - 2;
			INT yTop = rc.top + 2;
			INT yMid = rc.top + nHeight / 2;
			INT yBottom = rc.bottom - 2;
			INT nTailHalf = max(1, nHeight / 6);
			POINT rgArrow[] =
			{
				{xLeft, yMid - nTailHalf},
				{xTail, yMid - nTailHalf},
				{xTail, yTop},
				{xTip, yMid},
				{xTail, yBottom},
				{xTail, yMid + nTailHalf},
				{xLeft, yMid + nTailHalf}
			};
			HBRUSH hbrYellow = CreateSolidBrush(RGB(255, 220, 40));
			HPEN hpnYellow = CreatePen(PS_SOLID, 1, RGB(150, 115, 0));
			HGDIOBJ hbrOld = SelectObject(pdis->hDC, hbrYellow);
			HGDIOBJ hpnOld = SelectObject(pdis->hDC, hpnYellow);

			Polygon(pdis->hDC, rgArrow, ARRAYSIZE(rgArrow));

			SelectObject(pdis->hDC, hpnOld);
			SelectObject(pdis->hDC, hbrOld);
			DeleteObject(hpnYellow);
			DeleteObject(hbrYellow);
		}
	}

	return TRUE;
}

HRESULT CQuadooProject::ToggleBreakpoint (ULONG nLine)
{
	HRESULT hr;
	CProjectFile* pFile = GetActiveProjectFile();
	BOOL fAdd;
	ITextDocument* pDocument;
	TStackRef<IOleWindow> srEditorWindow;
	HWND hwndEditor;

	CheckIf(NULL == pFile, E_FAIL);
	pDocument = m_pEditor->GetTextDocument();
	CheckIf(NULL == pDocument || nLine >= pDocument->LineCount(), HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));

	fAdd = !pFile->HasBreakpoint(nLine);
	Check(pFile->ToggleBreakpoint(nLine));
	Check(UpdateFiles());
	Check(Save());
	if(IsDebugging() && FAILED(SendDebuggerBreakpoint(pFile, nLine, fAdd)))
		StopDebugger();

	if(SUCCEEDED(m_pEditor->QueryInterface(&srEditorWindow)) && SUCCEEDED(srEditorWindow->GetWindow(&hwndEditor)))
		InvalidateRect(hwndEditor, NULL, FALSE);

Cleanup:
	return hr;
}

HRESULT CQuadooProject::AdjustBreakpoints (const TVNLINESCHANGED* pLinesChanged)
{
	HRESULT hr = S_FALSE;
	CProjectFile* pFile = GetActiveProjectFile();
	BOOL fChanged = FALSE;

	CheckIf(NULL == pFile || NULL == pLinesChanged, E_FAIL);
	pFile->AdjustBreakpoints(pLinesChanged, &fChanged);
	if(fChanged)
	{
		TStackRef<IOleWindow> srEditorWindow;
		HWND hwndEditor;

		Check(UpdateFiles());
		Check(Save());

		if(SUCCEEDED(m_pEditor->QueryInterface(&srEditorWindow)) && SUCCEEDED(srEditorWindow->GetWindow(&hwndEditor)))
			InvalidateRect(hwndEditor, NULL, FALSE);
	}

Cleanup:
	return hr;
}

HRESULT CQuadooProject::SaveAll (VOID)
{
	HRESULT hr = S_FALSE;
	sysint idxActive = m_pTabs->GetActiveTab();

	for(sysint idxTab = 0; idxTab < m_pTabs->Tabs(); idxTab++)
	{
		CProjectTab* pFile = m_pTabs->TGetTabData<CProjectTab>(idxTab);
		if(idxActive == idxTab)
		{
			if(m_pTabs->GetActiveTabHighlight())
				Check(SaveTab(idxTab));
		}
		else if(pFile->IsModified())
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
	CProjectTab* pFile = FindDefaultScript();
	PWSTR pwzStartDir = NULL;
	INT cchStartDir = 0, nCompare;
	LONG_PTR nResult;

	Check(SaveAll());
	CheckIf(NULL == pFile, HRESULT_FROM_WIN32(ERROR_NOT_FOUND));

	if(IsWebProject())
	{
		PCWSTR pcwzName = TStrCchRChr(pFile->m_pwzAbsolutePath, pFile->m_cchAbsolutePath, L'\\');
		bool fCopyPath;

		if(pcwzName) pcwzName++;

		if(FAILED(m_pProject->FindNonNullValueW(L"webHost", &srv)))
		{
			Check(EditRunParams());
			Check(m_pProject->FindNonNullValueW(L"webHost", &srv));
		}

		Check(srv->GetString(&rstrStartDir));
		srv.Release();

		PCWSTR pcwzHost = RStrToWide(rstrStartDir);
		INT cchHost = RStrLen(rstrStartDir);
		CheckIf(0 == cchHost, E_INVALIDARG);

		if(pcwzHost[cchHost - 1] != L'/')
			Check(RStrFormatW(&rstrTarget, L"%r/%ls", rstrStartDir, pcwzName));
		else
			Check(RStrFormatW(&rstrTarget, L"%r%ls", rstrStartDir, pcwzName));

		if(SUCCEEDED(m_pProject->FindNonNullValueW(L"copyWebPath", &srv)) && SUCCEEDED(srv->GetBoolean(&fCopyPath)) && fCopyPath)
			Check(CopyWebScripts());

		{
			CDialogHost dlgHost(m_hInstance);
			CRunWebServiceDlg dlgRunWebService(m_pProject, rstrTarget);

			Check(dlgHost.Display(m_hwnd, &dlgRunWebService));
		}
	}
	else
	{
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
	}

Cleanup:
	if(FAILED(hr) && E_ABORT != hr)
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

HRESULT CQuadooProject::DebugScript (VOID)
{
	HRESULT hr;
	RSTRING rstrEngine = NULL, rstrArgs = NULL, rstrScriptArgs = NULL, rstrStartDir = NULL;
	TStackRef<IJSONValue> srv;
	CProjectTab* pFile = FindDefaultScript();
	PCWSTR pcwzDotExe;
	PWSTR pwzStartDir = NULL;
	INT cchStartDir = 0, cchBaseEngine, nPort;
	WCHAR wzDebugger[MAX_PATH];

	Check(SaveAll());
	CheckIf(NULL == pFile, HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
	CheckIf(IsWebProject(), E_UNEXPECTED);

	Check(FindInstalledEngine(&rstrEngine));

	pcwzDotExe = TStrCchRChr(RStrToWide(rstrEngine), RStrLen(rstrEngine), L'.');
	CheckIf(NULL == pcwzDotExe, E_UNEXPECTED);
	Check(TStrCchCpyLen(wzDebugger, ARRAYSIZE(wzDebugger), RStrToWide(rstrEngine), static_cast<INT>(pcwzDotExe - RStrToWide(rstrEngine)), &cchBaseEngine));
	Check(TStrCchCpy(wzDebugger + cchBaseEngine, ARRAYSIZE(wzDebugger) - cchBaseEngine, L"Dbg.exe"));

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
	srv.Release();

	if(SUCCEEDED(m_pProject->FindNonNullValueW(L"debugPort", &srv)))
		Check(srv->GetInteger(&nPort));
	else
		nPort = 1200;

	{
		CDialogHost dlgHost(m_hInstance);
		CStartDebuggerDlg dlgStartDebugger(this, wzDebugger, pwzStartDir, pFile->m_pwzAbsolutePath, rstrScriptArgs, nPort);

		Check(dlgHost.Display(m_hwnd, &dlgStartDebugger));
	}

Cleanup:
	SafeDeleteArray(pwzStartDir);

	RStrRelease(rstrStartDir);
	RStrRelease(rstrScriptArgs);
	RStrRelease(rstrArgs);
	RStrRelease(rstrEngine);
	return hr;
}

HRESULT CQuadooProject::BuildBreakpointData (__out CMemoryStream* pstmBreakpoints)
{
	HRESULT hr;
	DWORD cBreakpoints = 0, cb;

	for(sysint i = 0; i < m_mapFiles.Length(); i++)
	{
		CProjectFile* pFile = static_cast<CProjectFile*>(*m_mapFiles.GetValuePtr(i));
		Check(HrSafeAdd(cBreakpoints, static_cast<DWORD>(pFile->Breakpoints()), &cBreakpoints));
	}

	Check(pstmBreakpoints->Write(&cBreakpoints, sizeof(cBreakpoints), &cb));
	for(sysint i = 0; i < m_mapFiles.Length(); i++)
	{
		CProjectFile* pFile = static_cast<CProjectFile*>(*m_mapFiles.GetValuePtr(i));
		DWORD cchFile = static_cast<DWORD>(pFile->m_cchAbsolutePath);
		DWORD cbFile;

		Check(HrSafeMult(cchFile, static_cast<DWORD>(sizeof(WCHAR)), &cbFile));
		for(sysint n = 0; n < pFile->Breakpoints(); n++)
		{
			QUADOO_DEBUG_BREAKPOINT breakpoint;

			breakpoint.cchFile = cchFile;
			breakpoint.nLine = pFile->GetBreakpoint(n) + 1;
			Check(pstmBreakpoints->Write(&breakpoint, sizeof(breakpoint), &cb));
			Check(pstmBreakpoints->Write(pFile->m_pwzAbsolutePath, cbFile, &cb));
		}
	}

Cleanup:
	return hr;
}

HRESULT CQuadooProject::CreateDebugger (__deref_out CQuadooDebugSession** ppDebugger, __out CMemoryStream* pstmBreakpoints, __deref_out RSTRING* prstrRemoteHost)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	CQuadooDebugSession* pDebugger = NULL;
	bool fAutoStartDebugger = true;

	CheckIf(IsDebugging(), HRESULT_FROM_WIN32(ERROR_BUSY));
	Check(BuildBreakpointData(pstmBreakpoints));

	if(SUCCEEDED(m_pProject->FindNonNullValueW(L"autoStartDebugger", &srv)))
	{
		Check(srv->GetBoolean(&fAutoStartDebugger));
		srv.Release();
	}

	pDebugger = __new CQuadooDebugSession(fAutoStartDebugger);
	CheckAlloc(pDebugger);
	Check(pDebugger->Initialize());

	if(!fAutoStartDebugger && SUCCEEDED(m_pProject->FindNonNullValueW(L"remoteHost", &srv)))
		Check(srv->GetString(prstrRemoteHost));

	*ppDebugger = pDebugger;
	pDebugger = NULL;

Cleanup:
	SafeDelete(pDebugger);
	return hr;
}

HRESULT CQuadooProject::AttachDebugger (CQuadooDebugSession* pDebugger, IQuadooDebugTree* pDebugTree)
{
	HRESULT hr;
	CQuadooCallStackDisplay* pCallStack = NULL;
	RECT rc;

	CheckIf(NULL != m_pDebugger, E_UNEXPECTED);
	CheckIf(NULL == pDebugger || NULL == pDebugTree, E_INVALIDARG);

	pCallStack = __new CQuadooCallStackDisplay(m_hInstance, m_pTabs);
	CheckAlloc(pCallStack);
	Check(pCallStack->Initialize(m_hwnd));

	Check(pDebugger->AssociateSocket(m_hwnd));

	m_pDebugger = pDebugger;
	pDebugger = NULL;
	SetInterface(m_pDebugTree, pDebugTree);
	m_pCallStack = pCallStack;
	pCallStack = NULL;
	UpdateDebuggerUI();

	m_pEditor->SetReadOnly(TRUE);
	GetClientRect(m_hwnd, &rc);
	ResizeEditor(rc.bottom);
	SideAssertHr(InitializeDebuggerTooltip());

Cleanup:
	SafeDelete(pDebugger);
	if(pCallStack)
	{
		pCallStack->Destroy();
		SafeRelease(pCallStack);
	}
	return hr;
}

HRESULT CQuadooProject::SendDebuggerCommand (DWORD nMessage)
{
	HRESULT hr;

	if(!IsDebugging())
		return HRESULT_FROM_WIN32(ERROR_DEBUGGER_INACTIVE);
	hr = m_pDebugger->SendCommand(nMessage);
	if(FAILED(hr))
		StopDebugger();
	else
	{
		HideDebuggerTooltip();
		if(m_pCallStack)
			m_pCallStack->Clear();
		UpdateDebuggerUI();
	}
	return hr;
}

HRESULT CQuadooProject::SendDebuggerBreakpoint (CProjectFile* pFile, DWORD nLine, BOOL fAdd)
{
	if(NULL == pFile || !IsDebugging())
		return HRESULT_FROM_WIN32(ERROR_DEBUGGER_INACTIVE);
	return m_pDebugger->SendBreakpoint(fAdd, pFile->m_pwzAbsolutePath, nLine + 1);
}

HRESULT CQuadooProject::ShowDebuggerLocation (VOID)
{
	HRESULT hr;
	RSTRING rstrFile = NULL;
	DWORD nLine;
	BOOL fException;
	CProjectTab* pFile = NULL;

	CheckIf(NULL == m_pDebugger, HRESULT_FROM_WIN32(ERROR_DEBUGGER_INACTIVE));
	hr = m_pDebugger->GetCurrentLocation(&rstrFile, &nLine, &fException);
	CheckIfIgnore(S_FALSE == hr, S_FALSE);
	Check(hr);

	for(sysint i = 0; i < m_mapFiles.Length(); i++)
	{
		CProjectTab* pProjectFile = *m_mapFiles.GetValuePtr(i);
		if(0 == TStrCmpIAssert(pProjectFile->m_pwzAbsolutePath, RStrToWide(rstrFile)))
		{
			pFile = pProjectFile;
			break;
		}
	}
	CheckIf(NULL == pFile, HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
	Check(SwitchToFile(pFile));
	m_pEditor->ScrollView(0, nLine - 1);
	m_pEditor->SetFocus();
	UpdateDebuggerUI();

Cleanup:
	RStrRelease(rstrFile);
	return hr;
}

HRESULT CQuadooProject::InitializeDebuggerTooltip (VOID)
{
	HRESULT hr;
	TStackRef<IOleWindow> srEditorWindow;
	HWND hwndEditor;
	TOOLINFO ti = {0};

	CheckIf(NULL != m_hwndDebugTip, S_FALSE);
	Check(m_pEditor->QueryInterface(&srEditorWindow));
	Check(srEditorWindow->GetWindow(&hwndEditor));
	m_hwndDebugTip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL,
		WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT, m_hwnd, NULL, m_hInstance, NULL);
	CheckIfGetLastError(NULL == m_hwndDebugTip);

	ti.cbSize = sizeof(ti);
	ti.uFlags = TTF_TRACK | TTF_ABSOLUTE;
	ti.hwnd = hwndEditor;
	ti.uId = 1;
	ti.lpszText = L"";
	CheckIf(!SendMessage(m_hwndDebugTip, TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(&ti)), E_FAIL);
	SendMessage(m_hwndDebugTip, TTM_SETMAXTIPWIDTH, 0, 500);
	hr = S_OK;

Cleanup:
	if(FAILED(hr) && m_hwndDebugTip)
	{
		DestroyWindow(m_hwndDebugTip);
		m_hwndDebugTip = NULL;
	}
	return hr;
}

VOID CQuadooProject::HideDebuggerTooltip (BOOL fInvalidateRequest)
{
	if(m_hwndDebugTip)
	{
		TStackRef<IOleWindow> srEditorWindow;
		HWND hwndEditor;
		if(SUCCEEDED(m_pEditor->QueryInterface(&srEditorWindow)) &&
			SUCCEEDED(srEditorWindow->GetWindow(&hwndEditor)))
		{
			TOOLINFO ti = {0};
			ti.cbSize = sizeof(ti);
			ti.hwnd = hwndEditor;
			ti.uId = 1;
			SendMessage(m_hwndDebugTip, TTM_TRACKACTIVATE, FALSE, reinterpret_cast<LPARAM>(&ti));
		}
	}
	m_fHoverPending = FALSE;
	RStrRelease(m_rstrHoverName); m_rstrHoverName = NULL;
	RStrRelease(m_rstrTooltipText); m_rstrTooltipText = NULL;
	if(fInvalidateRequest)
		m_nHoverRequest++;
}

HRESULT CQuadooProject::ExtractHoverIdentifier (ULONG nLine, ULONG nOffset, RSTRING* prstrIdentifier)
{
	HRESULT hr;
	ITextDocument* pDocument = m_pEditor->GetTextDocument();
	PWSTR pwzLine = NULL;
	size_w idxLine, cchLine, cchCopied;
	size_w idxChar, idxStart, idxEnd, idxPrev;

	CheckIf(NULL == prstrIdentifier, E_POINTER);
	*prstrIdentifier = NULL;
	CheckIf(NULL == pDocument || nLine >= pDocument->LineCount(), HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
	idxLine = pDocument->LineOffset(nLine);
	cchLine = pDocument->LineLength(nLine);
	CheckIf(0 == cchLine || nOffset < idxLine || nOffset > idxLine + cchLine, HRESULT_FROM_WIN32(ERROR_NOT_FOUND));

	pwzLine = __new WCHAR[static_cast<sysint>(cchLine) + 1];
	CheckAlloc(pwzLine);
	Check(pDocument->Render(idxLine, pwzLine, cchLine, &cchCopied));
	pwzLine[cchCopied] = L'\0';
	idxChar = nOffset - idxLine;
	if(idxChar >= cchCopied || !(L'_' == pwzLine[idxChar] || iswalnum(pwzLine[idxChar])))
	{
		CheckIfIgnore(0 == idxChar || !(L'_' == pwzLine[idxChar - 1] || iswalnum(pwzLine[idxChar - 1])),
			HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
		idxChar--;
	}

	idxStart = idxChar;
	while(0 < idxStart && (L'_' == pwzLine[idxStart - 1] || iswalnum(pwzLine[idxStart - 1])))
		idxStart--;
	idxEnd = idxChar + 1;
	while(idxEnd < cchCopied && (L'_' == pwzLine[idxEnd] || iswalnum(pwzLine[idxEnd])))
		idxEnd++;

	idxPrev = idxStart;
	while(0 < idxPrev && iswspace(pwzLine[idxPrev - 1]))
		idxPrev--;
	CheckIf(0 < idxPrev && L'.' == pwzLine[idxPrev - 1], HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
	Check(RStrCreateW(static_cast<INT>(idxEnd - idxStart), pwzLine + idxStart, prstrIdentifier));

Cleanup:
	SafeDeleteArray(pwzLine);
	return hr;
}

HRESULT CQuadooProject::BeginVariableHover (const TVNMOUSEHOVER* pHover)
{
	HRESULT hr;
	RSTRING rstrIdentifier = NULL, rstrFile = NULL;
	INT nCompare;
	DWORD nLine;
	BOOL fException;
	CProjectFile* pFile;
	QUADOO_DEBUG_VARIABLE_QUERY query = {0};

	CheckIfIgnore(NULL == pHover || NULL == m_pDebugger || NULL == m_pDebugTree || !m_pDebugger->IsPaused(), S_FALSE);
	Check(m_pDebugger->GetCurrentLocation(&rstrFile, &nLine, &fException));
	pFile = GetActiveProjectFile();
	CheckIf(NULL == pFile || NULL == rstrFile ||
		0 != TStrCmpIAssert(pFile->m_pwzAbsolutePath, RStrToWide(rstrFile)), S_FALSE);
	hr = ExtractHoverIdentifier(pHover->nLineNo, pHover->nOffset, &rstrIdentifier);
	CheckIfIgnore(HRESULT_FROM_WIN32(ERROR_NOT_FOUND) == hr, S_FALSE);
	Check(hr);
	if(!m_fHoverPending && m_rstrHoverName &&
		SUCCEEDED(RStrCompareRStr(m_rstrHoverName, rstrIdentifier, &nCompare)) && 0 == nCompare)
	{
		hr = S_OK;
		goto Cleanup;
	}

	{
		TStackRef<CVariableResolver> srResolver;

		srResolver.Attach(__new CVariableResolver(rstrIdentifier));
		CheckAlloc(srResolver);
		Check(m_pDebugTree->EnumVariables(rstrFile, static_cast<INT>(nLine), srResolver));
		CheckIf(!srResolver->GetQuery(&query), S_FALSE);
	}

	query.requestId = m_nHoverRequest;
	RStrSet(m_rstrHoverName, rstrIdentifier);
	m_fHoverPending = TRUE;
	Check(m_pDebugger->SendVariableQuery(query));

Cleanup:
	if(S_OK != hr)
		HideDebuggerTooltip();
	RStrRelease(rstrFile);
	RStrRelease(rstrIdentifier);
	return hr;
}

VOID CQuadooProject::UpdateVariableTooltip (VOID)
{
	DWORD nRequestId;
	HRESULT hrValue;
	RSTRING rstrValue = NULL;

	if(NULL == m_pDebugger || S_OK != m_pDebugger->GetVariableValue(&nRequestId, &hrValue, &rstrValue))
		return;
	if(m_fHoverPending && nRequestId == m_nHoverRequest && SUCCEEDED(hrValue) &&
		m_pDebugger->IsPaused() && m_hwndDebugTip && m_rstrHoverName)
	{
		TStackRef<IOleWindow> srEditorWindow;
		HWND hwndEditor;
		POINT pt;

		if(m_rstrTooltipText)
		{
			RStrRelease(m_rstrTooltipText);
			m_rstrTooltipText = NULL;
		}

		if(SUCCEEDED(RStrFormatW(&m_rstrTooltipText, L"%r = %r", m_rstrHoverName, rstrValue)) &&
			SUCCEEDED(m_pEditor->QueryInterface(&srEditorWindow)) &&
			SUCCEEDED(srEditorWindow->GetWindow(&hwndEditor)))
		{
			TOOLINFO ti = {0};
			ti.cbSize = sizeof(ti);
			ti.hwnd = hwndEditor;
			ti.uId = 1;
			ti.lpszText = const_cast<PWSTR>(RStrToWide(m_rstrTooltipText));
			SendMessage(m_hwndDebugTip, TTM_UPDATETIPTEXT, 0, reinterpret_cast<LPARAM>(&ti));
			GetCursorPos(&pt);
			SendMessage(m_hwndDebugTip, TTM_TRACKPOSITION, 0, MAKELPARAM(pt.x + 12, pt.y + 20));
			SendMessage(m_hwndDebugTip, TTM_TRACKACTIVATE, TRUE, reinterpret_cast<LPARAM>(&ti));
		}
	}
	m_fHoverPending = FALSE;
	RStrRelease(rstrValue);
}

VOID CQuadooProject::UpdateDebuggerUI (VOID)
{
	if(m_pDebugger)
		m_pTitle->UpdateTitle(RStrToWide(m_rstrProject), m_pDebugger->IsPaused() ? L"Debugging" : L"Running");
	else
		m_pTitle->UpdateTitle(RStrToWide(m_rstrProject), NULL);

	if(m_pEditor)
	{
		TStackRef<IOleWindow> srEditorWindow;
		HWND hwndEditor;
		if(SUCCEEDED(m_pEditor->QueryInterface(&srEditorWindow)) && SUCCEEDED(srEditorWindow->GetWindow(&hwndEditor)))
			InvalidateRect(hwndEditor, NULL, FALSE);
	}
}

VOID CQuadooProject::StopDebugger (VOID)
{
	HideDebuggerTooltip();
	if(m_hwndDebugTip)
	{
		DestroyWindow(m_hwndDebugTip);
		m_hwndDebugTip = NULL;
	}
	SafeRelease(m_pDebugTree);
	SafeDelete(m_pDebugger);
	UpdateDebuggerUI();

	if(m_pCallStack)
	{
		RECT rc;

		m_pCallStack->Destroy();
		SafeRelease(m_pCallStack);

		GetClientRect(m_hwnd, &rc);
		ResizeEditor(rc.bottom);
	}

	m_pEditor->SetReadOnly(FALSE);
}

BOOL CQuadooProject::IsDebugging (VOID)
{
	if(m_pDebugger && !m_pDebugger->IsActive())
		StopDebugger();
	return NULL != m_pDebugger;
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

HRESULT CQuadooProject::FindActiveQuadoo (__deref_out RSTRING* prstrEngine)
{
	HRESULT hr;
	HKEY hKey = NULL;
	WCHAR wzClass[80], *pwzClassPtr, wzPath[MAX_PATH];
	LONG cb;
	INT cch, cchRemaining;

	Check(TStrCchCpyEx(wzClass, ARRAYSIZE(wzClass), L"CLSID\\", &pwzClassPtr, &cchRemaining));
	cb = cchRemaining * sizeof(WCHAR);

	CheckWin32Error(RegOpenKey(HKEY_CLASSES_ROOT, L"QuadooScript\\CLSID", &hKey));
	CheckWin32Error(RegQueryValue(hKey, NULL, pwzClassPtr, &cb));
	CheckIf(cb % sizeof(WCHAR), E_UNEXPECTED);
	cch = (ARRAYSIZE(wzClass) - cchRemaining) + cb / sizeof(WCHAR);
	if(0 < cch && wzClass[cch - 1] == L'\0')
		cch--;

	Check(TStrCchCpy(wzClass + cch, ARRAYSIZE(wzClass) - cch, L"\\InprocServer32"));
	cb = sizeof(wzPath);

	RegCloseKey(hKey); hKey = NULL;
	CheckWin32Error(RegOpenKey(HKEY_CLASSES_ROOT, wzClass, &hKey));

	CheckWin32Error(RegQueryValue(hKey, NULL, wzPath, &cb));
	CheckIf(cb % sizeof(WCHAR), E_UNEXPECTED);
	cch = cb / sizeof(WCHAR);
	if(0 < cch && wzPath[cch - 1] == L'\0')
		cch--;

	Check(RStrCreateW(cch, wzPath, prstrEngine));

Cleanup:
	if(hKey)
		RegCloseKey(hKey);
	return hr;
}

BOOL CQuadooProject::IsWebProject (VOID)
{
	HRESULT hr;
	RSTRING rstrTarget = NULL;
	INT nResult;

	Check(GetProjectTarget(&rstrTarget));
	Check(RStrCompareIW(rstrTarget, L"web", &nResult));
	CheckIfIgnore(0 != nResult, S_FALSE);

Cleanup:
	RStrRelease(rstrTarget);
	return S_OK == hr;
}

CProjectTab* CQuadooProject::GetProjectFromTreeItem (HTREEITEM hItem)
{
	TVITEM tvItem = {0};
	tvItem.hItem = hItem;
	tvItem.mask = TVIF_PARAM;
	if(TreeView_GetItem(m_hwndTree, &tvItem) && tvItem.lParam)
		return reinterpret_cast<CProjectTab*>(tvItem.lParam);
	return NULL;
}

CProjectTab* CQuadooProject::FindDefaultScript (VOID)
{
	for(sysint i = 0; i < m_mapFiles.Length(); i++)
	{
		CProjectTab* pFile = *m_mapFiles.GetValuePtr(i);
		if(pFile->m_fDefault)
			return pFile;
	}
	return NULL;
}

HRESULT CQuadooProject::SaveDefaultFont (INT nFontSize)
{
	HRESULT hr;
	TStackRef<IOleWindow> srWindow;
	HWND hwndEditor;
	HFONT hFont;
	HKEY hKey = NULL;
	LOGFONT lf;
	DPI_AWARE_FONT dpiFont;

	Check(m_pEditor->QueryInterface(&srWindow));
	Check(srWindow->GetWindow(&hwndEditor));

	hFont = (HFONT)SendMessage(hwndEditor, WM_GETFONT, 0, 0);
	CheckIfGetLastError(GetObject(hFont, sizeof(LOGFONT), &lf) == 0);

	dpiFont.lFontSize = nFontSize;
	dpiFont.lEscapement = lf.lfEscapement;
	dpiFont.lOrientation = lf.lfOrientation;
	dpiFont.lWeight = lf.lfWeight;
	dpiFont.bItalic = lf.lfItalic;
	dpiFont.bUnderline = lf.lfUnderline;
	dpiFont.bStrikeOut = lf.lfStrikeOut;
	dpiFont.bCharSet = lf.lfCharSet;
	dpiFont.bOutPrecision = lf.lfOutPrecision;
	dpiFont.bClipPrecision = lf.lfClipPrecision;
	dpiFont.bQuality = lf.lfQuality;
	dpiFont.bPitchAndFamily = lf.lfPitchAndFamily;
	Check(TStrCchCpy(dpiFont.wzFaceName, ARRAYSIZE(dpiFont.wzFaceName), lf.lfFaceName));

	Check(Registry::CreateKey(HKEY_CURRENT_USER, c_wzFontKey, KEY_WRITE, &hKey));
	CheckWin32Error(RegSetValueEx(hKey, L"EditorFont", NULL, REG_BINARY, (LPBYTE)&dpiFont, sizeof(dpiFont)));

Cleanup:
	if(hKey)
		RegCloseKey(hKey);
	return hr;
}

HRESULT CQuadooProject::LoadDefaultFont (VOID)
{
	HRESULT hr;
	HKEY hKey = NULL;
	HFONT hFont;
	LOGFONT lf;
	DPI_AWARE_FONT dpiFont;
	DWORD cbData = sizeof(dpiFont);
	HDC hdc = GetDC(m_hwnd);

	CheckWin32Error(RegOpenKey(HKEY_CURRENT_USER, c_wzFontKey, &hKey));
	CheckWin32Error(RegQueryValueEx(hKey, L"EditorFont", NULL, NULL, (LPBYTE)&dpiFont, &cbData));
	CheckIf(cbData != sizeof(dpiFont), HRESULT_FROM_WIN32(ERROR_INVALID_DATA));

	lf.lfHeight = -MulDiv(dpiFont.lFontSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
	lf.lfEscapement = dpiFont.lEscapement;
	lf.lfOrientation = dpiFont.lOrientation;
	lf.lfWeight = dpiFont.lWeight;
	lf.lfItalic = dpiFont.bItalic;
	lf.lfUnderline = dpiFont.bUnderline;
	lf.lfStrikeOut = dpiFont.bStrikeOut;
	lf.lfCharSet = dpiFont.bCharSet;
	lf.lfOutPrecision = dpiFont.bOutPrecision;
	lf.lfClipPrecision = dpiFont.bClipPrecision;
	lf.lfQuality = dpiFont.bQuality;
	lf.lfPitchAndFamily = dpiFont.bPitchAndFamily;
	Check(TStrCchCpy(lf.lfFaceName, ARRAYSIZE(lf.lfFaceName), dpiFont.wzFaceName));

	hFont = CreateFontIndirect(&lf);
	CheckIfGetLastError(NULL == hFont);
	m_pEditor->SetDefaultFont(hFont, true);

Cleanup:
	if(hKey)
		RegCloseKey(hKey);
	ReleaseDC(m_hwnd, hdc);
	return hr;
}

HRESULT CQuadooProject::SaveTabData (CProjectTab* pFile)
{
	HRESULT hr;

	CheckIf(NULL == m_pEditor, E_FAIL);
	Check(pFile->SaveFromEditor(m_pEditor));

Cleanup:
	return hr;
}

HRESULT CQuadooProject::LoadTabData (CProjectTab* pFile)
{
	HRESULT hr;
	PWSTR pwzText = NULL;
	INT cchText;

	Check(pFile->RestoreEditor(m_pEditor));
	if(S_FALSE == hr)
	{
		Check(Text::LoadFromFile(pFile->m_pwzAbsolutePath, &pwzText, &cchText));
		Check(m_pEditor->Prepare(pwzText, cchText));
		m_pEditor->ResetTextEditView();
	}

Cleanup:
	SafeDeleteArrayCount(pwzText, cchText);
	return hr;
}

HRESULT CQuadooProject::SaveTab (sysint idxTab)
{
	HRESULT hr;
	CProjectTab* pFile = m_pTabs->TGetTabData<CProjectTab>(idxTab);
	RECT rc;

	Check(pFile->SaveToStorage(m_pEditor, m_pTabs->GetActiveTab() == idxTab));
	Check(m_pTabs->HighlightTab(idxTab, false));
	m_pTabs->GetTabsRect(0, 0, rc);
	InvalidateRect(m_hwnd, &rc, FALSE);

Cleanup:
	return hr;
}

BOOL CQuadooProject::PromptToSaveTab (sysint idxTab)
{
	BOOL fProceed = FALSE;
	CProjectTab* pFile = m_pTabs->TGetTabData<CProjectTab>(idxTab);
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
	CProjectTab* pActiveFile = NULL;
	sysint idxActive = m_pTabs->GetActiveTab();
	if(idxActive == idxTab)
	{
		pActiveFile = m_pTabs->TGetTabData<CProjectTab>(idxTab);
		if(m_pTabs->GetActiveTabHighlight() && !PromptToSaveTab(idxTab))
			return FALSE;
	}
	else
	{
		CProjectTab* pFile = m_pTabs->TGetTabData<CProjectTab>(idxTab);
		if(pFile->IsModified() && !PromptToSaveTab(idxTab))
			return FALSE;
	}

	SideAssertHr(m_pTabs->RemoveTab(idxTab));

	RECT rc;
	m_pTabs->GetTabsRect(0, 0, rc);
	InvalidateRect(m_hwnd, &rc, FALSE);

	CProjectTab* pNext = m_pTabs->TGetTabData<CProjectTab>(idxTab);
	if(NULL == pNext && 0 < idxTab)
		pNext = m_pTabs->TGetTabData<CProjectTab>(idxTab - 1);

	if(NULL == pNext || FAILED(SwitchToFile(pNext)))
	{
		if(pActiveFile && S_OK == pActiveFile->CloseCustomLayout())
		{
			GetClientRect(m_hwnd, &rc);
			ResizeEditor(rc.bottom);
		}

		m_pEditor->Prepare(NULL, 0);
		m_pEditor->ResetTextEditView();
		m_pEditor->EnableEditor(FALSE);
	}

	return TRUE;
}

VOID CQuadooProject::RemoveFilePrompt (CProjectTab* pFile)
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

HRESULT CQuadooProject::ShowProjectCompiler (VOID)
{
	HRESULT hr;
	RSTRING rstrTarget = NULL, rstrEngine = NULL;
	CProjectTab* pDefault = FindDefaultScript();
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

HRESULT CQuadooProject::CopyWebScripts (VOID)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	RSTRING rstrCopyPath = NULL, rstrTarget = NULL;

	Check(m_pProject->FindNonNullValueW(L"copyPath", &srv));
	Check(srv->GetString(&rstrCopyPath));

	for(sysint i = 0; i < m_mapFiles.Length(); i++)
	{
		CProjectTab* pFile = *m_mapFiles.GetValuePtr(i);
		PCWSTR pcwzName = TStrRChr(pFile->m_pwzAbsolutePath, L'\\');
		if(pcwzName) pcwzName++;

		Check(RStrFormatW(&rstrTarget, L"%r\\%ls", rstrCopyPath, pcwzName));

		if(IsFileOutOfSync(pFile->m_pwzAbsolutePath, RStrToWide(rstrTarget)))
			CheckIfGetLastError(!CopyFile(pFile->m_pwzAbsolutePath, RStrToWide(rstrTarget), FALSE));
		RStrRelease(rstrTarget); rstrTarget = NULL;
	}

Cleanup:
	RStrRelease(rstrTarget);
	RStrRelease(rstrCopyPath);
	return hr;
}

HRESULT CQuadooProject::CreateRelativeProjectPath (__out_ecount(cchMaxRelative) PWSTR pwzRelative, INT cchMaxRelative, PCWSTR pcwzTo, DWORD dwAttrTo)
{
	HRESULT hr;

	CheckIf(cchMaxRelative < MAX_PATH, E_INVALIDARG);
	CheckIf(!PathRelativePathToW(pwzRelative, RStrToWide(m_rstrProjectDir), FILE_ATTRIBUTE_DIRECTORY, pcwzTo, dwAttrTo), E_FAIL);

	if(L'\\' == *pwzRelative)
	{
		// Is this a Windows XP thing?
		INT cchRelative = TStrLenAssert(pwzRelative) + 1;
		CheckIf(cchRelative + 1 == cchMaxRelative, HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW));
		MoveMemory(pwzRelative + 1, pwzRelative, cchRelative * sizeof(WCHAR));
		*pwzRelative = L'.';
	}

	hr = S_OK;

Cleanup:
	return hr;
}

BOOL CQuadooProject::IsFileOutOfSync (PCWSTR pcwzSource, PCWSTR pcwzTarget)
{
	BOOL fOutOfSync = TRUE;
	WIN32_FIND_DATA wfdTarget, wfdSource;

	HANDLE hTarget = FindFirstFile(pcwzTarget, &wfdTarget);
	if(INVALID_HANDLE_VALUE != hTarget)
	{
		HANDLE hSource = FindFirstFile(pcwzSource, &wfdSource);
		if(INVALID_HANDLE_VALUE != hSource)
		{
			fOutOfSync = wfdSource.nFileSizeLow != wfdTarget.nFileSizeLow ||
				wfdSource.nFileSizeHigh != wfdTarget.nFileSizeHigh ||
				wfdSource.ftLastWriteTime.dwLowDateTime != wfdTarget.ftLastWriteTime.dwLowDateTime ||
				wfdSource.ftLastWriteTime.dwHighDateTime != wfdTarget.ftLastWriteTime.dwHighDateTime;
			FindClose(hSource);
		}

		FindClose(hTarget);
	}

	return fOutOfSync;
}
