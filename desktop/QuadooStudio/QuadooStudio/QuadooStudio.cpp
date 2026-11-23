#include <windows.h>
#include <commctrl.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Core\MemoryStream.h"
#include "Library\Util\Registry.h"
#include "Library\Util\Formatting.h"
#include "Library\Util\TextHelpers.h"
#include "Library\Window\DialogHost.h"
#include "Library\DPI.h"
#include "Library\MenuUtil.h"
#include "Library\ChooseFile.h"
#include "Published\JSON.h"
#include "Splitter.h"
#include "NewProjectDlg.h"
#include "QuadooProject.h"
#include "QuadooStudio.h"
#include "ApplicationProject.h"

const WCHAR c_wzQuadooStudioClass[] = L"QuadooStudioCls";
#ifdef	_WIN64
const WCHAR c_wzQuadooStudioTitle[] = L"Quadoo Studio (x64)";
#else
const WCHAR c_wzQuadooStudioTitle[] = L"Quadoo Studio";
#endif
const WCHAR c_wzRegistryKey[] = L"Software\\Simbey\\QuadooStudio";

CQuadooStudio::CQuadooStudio (HINSTANCE hInstance) :
	m_hInstance(hInstance),
	m_hAccel(NULL),
	m_hDesignerAccel(NULL),
	m_treeHeight(0),
	m_dragTreeSplit(FALSE),
	m_pGdiPlusStartupInput(NULL),
	m_gdiplusToken(NULL),
	m_hwndStatus(NULL),
	m_hwndTree(NULL),
	m_hImageList(NULL),
	m_pCustomMenu(NULL),
	m_pSplitter(NULL),
	m_pProject(NULL),
	m_packagesBusy(FALSE),
	m_hRecentProjects(NULL)
{
}

CQuadooStudio::~CQuadooStudio ()
{
	if(m_pCustomMenu)
	{
		DetachSubclassHandler(m_pCustomMenu);
		SafeDelete(m_pCustomMenu);
	}

	if(m_hImageList)
		ImageList_Destroy(m_hImageList);

	if(m_pGdiPlusStartupInput)
	{
		Gdiplus::GdiplusShutdown(m_gdiplusToken);
		SafeDelete(m_pGdiPlusStartupInput);
	}
}

HRESULT CQuadooStudio::Register (HINSTANCE hInstance)
{
	WNDCLASSEX wnd = {0};

	wnd.cbSize = sizeof(WNDCLASSEX);
	wnd.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	wnd.cbClsExtra = 0;
	wnd.cbWndExtra = 0;
	wnd.hInstance = hInstance;
	wnd.hIcon = LoadIcon(hInstance, MAKEINTRESOURCEW(IDI_MAIN));
	wnd.hCursor = LoadCursor(NULL, IDC_ARROW);
	wnd.hbrBackground = GetSysColorBrush(COLOR_APPWORKSPACE);
	wnd.lpszMenuName = MAKEINTRESOURCEW(IDR_MENU);
	wnd.lpszClassName = c_wzQuadooStudioClass;

	return RegisterClass(&wnd,NULL);
}

HRESULT CQuadooStudio::Unregister (HINSTANCE hInstance)
{
	return UnregisterClass(c_wzQuadooStudioClass, hInstance);
}

HRESULT CQuadooStudio::Initialize (PCWSTR pcwzCmdLine, INT nWidth, INT nHeight, INT nCmdShow, PCWSTR packageDirectory)
{
	HRESULT hr;

	m_packages.Attach(__new QFP::CCatalog);
	CheckAlloc(m_packages);
	Check(m_packages->Initialize(packageDirectory));

	C_ASSERT(ID_FILE_RECENT_LAST - ID_FILE_RECENT_FIRST + 1 == CRecentlyUsed::MaxItems);
	m_recentMenu.Attach(__new CRecentlyUsedMenu(m_recentProjects, ID_FILE_RECENT_FIRST));
	CheckAlloc(m_recentMenu);
	m_recentProjects.Initialize(c_wzRegistryKey, this);

	m_hDesignerAccel = LoadAccelerators(m_hInstance,MAKEINTRESOURCE(IDR_DESIGNER_ACCELERATOR));
	m_hAccel = LoadAccelerators(m_hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR));
	CheckIfGetLastError(NULL == m_hAccel);

	m_pGdiPlusStartupInput = __new Gdiplus::GdiplusStartupInput;
	CheckAlloc(m_pGdiPlusStartupInput);
	Gdiplus::GdiplusStartup(&m_gdiplusToken, m_pGdiPlusStartupInput, NULL);

	Check(Create(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, c_wzQuadooStudioClass, c_wzQuadooStudioTitle, CW_USEDEFAULT, CW_USEDEFAULT, nWidth, nHeight, NULL, nCmdShow));

Cleanup:
	return hr;
}

BOOL CQuadooStudio::PreTranslate (__inout MSG* pmsg)
{
	if(m_pProject)
	{
		if(m_pProject->TranslateProjectMessage(pmsg))
			return TRUE;
		if(m_pProject->DesignerActive() && TranslateAccelerator(m_hwnd,m_hDesignerAccel,pmsg))
			return TRUE;
	}

	return TranslateAccelerator(m_hwnd,m_hAccel,pmsg);
}

// IUpdateTitle

HRESULT CQuadooStudio::UpdateTitle (PCWSTR pcwzProject, __in_opt PCWSTR pcwzStatus)
{
	HRESULT hr;
	RSTRING rstrLabel = NULL, rstrTitle = NULL;
	PCWSTR pcwzLabel = TStrRChr(pcwzProject, L'\\'), pcwzExt;
	INT cchLabel;

	if(pcwzLabel)
		pcwzLabel++;
	else
		pcwzLabel = pcwzProject;

	pcwzExt = TStrRChr(pcwzLabel, L'.');
	if(pcwzExt)
		cchLabel = static_cast<INT>(pcwzExt - pcwzLabel);
	else
		cchLabel = TStrLenAssert(pcwzLabel);
	Check(RStrCreateW(cchLabel, pcwzLabel, &rstrLabel));

	if(pcwzStatus)
		Check(RStrFormatW(&rstrTitle, L"%r - %ls (%ls)", rstrLabel, c_wzQuadooStudioTitle, pcwzStatus));
	else
		Check(RStrFormatW(&rstrTitle, L"%r - %ls", rstrLabel, c_wzQuadooStudioTitle));
	SetWindowText(m_hwnd, RStrToWide(rstrTitle));

Cleanup:
	RStrRelease(rstrTitle);
	RStrRelease(rstrLabel);
	return hr;
}

// IOleCommandTarget

HRESULT STDMETHODCALLTYPE CQuadooStudio::QueryStatus (
	const GUID* pguidCmdGroup,
	ULONG cCmds,
	OLECMD* prgCmds,
	OLECMDTEXT* pCmdText)
{
	for(ULONG i = 0; i < cCmds; i++)
	{
		if(!pguidCmdGroup && m_recentMenu && m_recentMenu->Handles(prgCmds[i].cmdID))
		{
			m_recentMenu->QueryStatus(NULL, 1, prgCmds + i, pCmdText);
			continue;
		}
		switch(prgCmds[i].cmdID)
		{
		case QFP::PackagesCommand:
		case ID_FILE_NEW_PROJECT:
		case ID_FILE_OPENPROJECT:
		case ID_FILE_EXIT:
			prgCmds[i].cmdf = OLECMDF_SUPPORTED | OLECMDF_ENABLED;
			break;
		case ID_FILE_CLOSEPROJECT:
			if(m_pProject)
				prgCmds[i].cmdf = OLECMDF_SUPPORTED | OLECMDF_ENABLED;
			else
				prgCmds[i].cmdf = OLECMDF_SUPPORTED;
			break;
		default:
			if(m_pProject)
				m_pProject->QueryStatus(pguidCmdGroup, 1, prgCmds + i, pCmdText);
			else
				prgCmds[i].cmdf = OLECMDF_SUPPORTED;
			break;
		}
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CQuadooStudio::Exec (
	const GUID* pguidCmdGroup,
	DWORD nCmdID,
	DWORD nCmdexecopt,
	VARIANT* pvaIn,
	VARIANT* pvaOut)
{
	HRESULT hr;

	if(!pguidCmdGroup && m_recentMenu && m_recentMenu->Handles(nCmdID))
	{
		hr = m_recentMenu->Exec(NULL, nCmdID, nCmdexecopt, pvaIn, pvaOut);
		goto Cleanup;
	}

	switch(nCmdID)
	{
	case QFP::PackagesCommand:
		CheckIf(m_packagesBusy,HRESULT_FROM_WIN32(ERROR_BUSY));
		m_packagesBusy = TRUE;
		hr = QFP::ShowPackages(m_hwnd,m_hInstance,m_packages,this);
		m_packagesBusy = FALSE;
		break;

	case ID_FILE_NEW_PROJECT:
		hr = CreateNewProject();
		break;

	case ID_FILE_OPENPROJECT:
		hr = OpenProjectPrompt();
		break;

	case ID_FILE_CLOSEPROJECT:
		CheckIfIgnore(NULL == m_pProject, S_FALSE);
		Check(m_pProject->CloseProject(TRUE));
		SafeRelease(m_pProject);
		if(m_packages) m_packages->UpdateUsage(this);
		TreeView_DeleteAllItems(m_hwndTree);
		ImageList_SetImageCount(m_hImageList,3);
		PostMessage(m_hwnd,WM_STUDIO_DESIGNER,0,0);
		SetWindowText(m_hwnd, c_wzQuadooStudioTitle);
		break;

	case ID_FILE_EXIT:
		if(m_pProject)
		{
			hr = m_pProject->CloseProject(TRUE);
			if(SUCCEEDED(hr))
				SafeRelease(m_pProject);
		}
		else
			hr = S_OK;

		if(SUCCEEDED(hr))
		{
			if(NULL == pvaIn || (pvaIn->vt == VT_BOOL && pvaIn->boolVal == VARIANT_FALSE))
				Destroy();
		}
		else if(pvaOut)
		{
			// Cancel
			pvaOut->vt = VT_BOOL;
			pvaOut->boolVal = VARIANT_TRUE;

			// Flip the result back to success (because we set pvaOut)
			hr = S_OK;
		}
		break;

	default:
		if(m_pProject)
			hr = m_pProject->Exec(pguidCmdGroup, nCmdID, nCmdexecopt, pvaIn, pvaOut);
		else
			hr = E_NOTIMPL;
		break;
	}

Cleanup:
	return hr;
}

HRESULT CQuadooStudio::CreateNewProject (VOID)
{
	HRESULT hr;
	CDialogHost dlgHost(m_hInstance);
	CNewProjectDlg dlgNewProject;
	TStackRef<IJSONObject> srProject;
	TStackRef<IJSONArray> srFiles;
	TStackRef<IJSONValue> srv;
	CMemoryStream stmJSON;
	RSTRING rstrField = NULL;

	Check(dlgNewProject.Initialize());
	Check(dlgHost.Display(m_hwnd, &dlgNewProject));
	CheckIfIgnore(dlgHost.GetReturnValue() != IDOK, E_ABORT);

	Check(Exec(NULL, ID_FILE_CLOSEPROJECT, 0, NULL, NULL));

	Check(JSONCreateObject(&srProject));

	Check(RStrCreateW(LSP(L"target"), &rstrField));
	Check(JSONAddStringWToObject(srProject, rstrField, dlgNewProject.GetProjectType(), TStrLenAssert(dlgNewProject.GetProjectType())));
	RStrRelease(rstrField); rstrField = NULL;

	Check(RStrCreateW(LSP(L"args"), &rstrField));
	Check(JSONAddStringWToObject(srProject, rstrField, SLP(L"")));
	RStrRelease(rstrField); rstrField = NULL;

	Check(RStrCreateW(LSP(L"argsEmbedded"), &rstrField));
	Check(JSONAddStringWToObject(srProject, rstrField, SLP(L"")));
	RStrRelease(rstrField); rstrField = NULL;

	Check(RStrCreateW(LSP(L"startDir"), &rstrField));
	Check(JSONAddStringWToObject(srProject, rstrField, SLP(L".\\")));
	RStrRelease(rstrField); rstrField = NULL;

	Check(RStrCreateW(LSP(L"debugPort"), &rstrField));
	Check(JSONCreateInteger(1200, &srv));
	Check(srProject->AddValue(rstrField, srv));
	srv.Release();
	RStrRelease(rstrField); rstrField = NULL;

	Check(RStrCreateW(LSP(L"remoteHost"), &rstrField));
	Check(JSONAddStringWToObject(srProject, rstrField, SLP(L"")));
	RStrRelease(rstrField); rstrField = NULL;

	Check(JSONCreateArray(&srFiles));
	Check(JSONWrapArray(srFiles, &srv));
	Check(srProject->AddValueW(L"files", srv));

	if(!lstrcmpW(dlgNewProject.GetProjectType(),L"application"))
	{
		Check(CreateApplicationProject(srProject,dlgNewProject.GetProjectFile()));
	}
	else
	{
		Check(JSONSerializeObject(srProject, &stmJSON));
		Check(Text::SaveToFile(stmJSON.TGetReadPtr<WCHAR>(), stmJSON.TDataRemaining<WCHAR>(), dlgNewProject.GetProjectFile()));
	}

	Check(OpenAndRecordProject(dlgNewProject.GetProjectFile()));
	if(m_pProject->IsApplication())
	{
		hr = m_pProject->NewFormUnit(TRUE);
		if(hr == OLECMDERR_E_CANCELED)
		{
			hr = S_OK;
		}
		Check(hr);
	}

Cleanup:
	RStrRelease(rstrField);
	return hr;
}

HRESULT CQuadooStudio::OpenProjectPrompt (VOID)
{
	HRESULT hr;
	CChooseFile pick;

	Check(Exec(NULL, ID_FILE_CLOSEPROJECT, 0, NULL, NULL));

	Check(pick.Initialize());
	CheckIfIgnore(!pick.OpenSingleFile(m_hwnd, L"Open Project File", L"Project (*.qsproj)\0*.qsproj\0"), E_ABORT);
	Check(OpenAndRecordProject(pick.GetFile(0)));

Cleanup:
	return hr;
}

HRESULT CQuadooStudio::OpenAndRecordProject (PCWSTR pcwzPath)
{
	HRESULT hr = OpenProject(pcwzPath);
	if(SUCCEEDED(hr))
		m_recentProjects.RecordOpen(pcwzPath);
	return hr;
}

HRESULT CQuadooStudio::OpenRecentlyUsed (PCWSTR pcwzPath)
{
	return OpenProject(pcwzPath);
}

HRESULT CQuadooStudio::OpenProject (PCWSTR pcwzProject)
{
	HRESULT hr;
	CQuadooProject* pProject = NULL;
	RECT rcStatus, rcSite;

	Check(Exec(NULL, ID_FILE_CLOSEPROJECT, 0, NULL, NULL));

	GetClientRect(m_hwnd, &rcSite);
	rcSite.left += GetTreeWidth() + m_pSplitter->GetWidth();

	GetClientRect(m_hwndStatus, &rcStatus);
	rcSite.bottom -= (rcStatus.bottom - rcStatus.top);

	pProject = __new CQuadooProject(m_hInstance, this, &m_dm, m_hwndTree);
	CheckAlloc(pProject);
	Check(m_packages->ProtectUnknown());
	m_packagesBusy = TRUE;
	pProject->SetFormRegistry(m_packages->Registry());
	pProject->SetStatusWindow(m_hwndStatus);
	Check(pProject->Initialize(m_hwnd, rcSite, pcwzProject));

	UpdateTitle(pcwzProject, NULL);

	m_pProject = pProject;
	pProject = NULL;
	PostMessage(m_hwnd,WM_STUDIO_DESIGNER,0,0);

Cleanup:
	if(pProject)
	{
		pProject->CloseProject(FALSE);
		SafeRelease(pProject);

		TreeView_DeleteAllItems(m_hwndTree);
		SetWindowText(m_hwnd, c_wzQuadooStudioTitle);
	}
	m_packagesBusy = FALSE;
	if(m_packages) m_packages->UpdateUsage(this);
	return hr;
}

VOID CQuadooStudio::UpdateColorScheme (VOID)
{
	m_dm.Update();
	m_dm.StylizeTitleBar(m_hwnd);

	if(m_dm.IsDarkMode())
		TreeView_SetBkColor(m_hwndTree, RGB(40, 40, 40));
	else
		TreeView_SetBkColor(m_hwndTree, (COLORREF)-1);

	if(m_pProject)
		m_pProject->UpdateColorScheme();

	if(m_dm.IsDarkMode())
	{
		if(NULL == m_pCustomMenu)
		{
			m_pCustomMenu = __new CCustomMenu(::GetMenu(m_hwnd));
			m_pCustomMenu->SetDarkMode();
			AttachSubclassHandler(m_pCustomMenu);
		}
	}
	else
	{
		if(m_pCustomMenu)
		{
			DetachSubclassHandler(m_pCustomMenu);
			SafeDelete(m_pCustomMenu);

			InvalidateRect(m_hwnd, NULL, TRUE);
		}
	}
}

// CBaseWindow

HINSTANCE CQuadooStudio::GetInstance (VOID)
{
	return m_hInstance;
}

VOID CQuadooStudio::OnFinalDestroy (HWND hwnd)
{
	Registry::SaveWindowPosition(hwnd, c_wzRegistryKey, L"WindowPlacement");

	SafeRelease(m_pSplitter);

	if(m_hAccel)
	{
		DestroyAcceleratorTable(m_hAccel);
		m_hAccel = NULL;
	}

	PostQuitMessage(0);
}

HRESULT CQuadooStudio::FinalizeAndShow (DWORD dwStyle, INT nCmdShow)
{
	Registry::LoadWindowPosition(m_hwnd, c_wzRegistryKey, L"WindowPlacement", &nCmdShow);

	return __super::FinalizeAndShow(dwStyle, nCmdShow);
}

BOOL CQuadooStudio::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	if(message == QFP::CatalogChangedMessage() || (message == WM_ACTIVATE && LOWORD(wParam) != WA_INACTIVE))
	{
		RefreshPackages();
		if(message == QFP::CatalogChangedMessage()) { lResult = 0; return TRUE; }
	}
	switch(message)
	{
	case WM_CREATE:
		if(FAILED(OnCreate()))
		{
			lResult = -1;
			return TRUE;
		}
		break;

	case QFP::RefreshPackagesMessage:
		if(m_packages && !m_packagesBusy) m_packages->UpdateUsage(this);
		lResult = 0; return TRUE;
	case WM_STUDIO_DESIGNER:
		{ RECT client; GetClientRect(m_hwnd,&client); SendMessage(m_hwnd,WM_SIZE,0,MAKELPARAM(client.right,client.bottom)); }
		if(m_pProject) m_pProject->UpdateDesignerStatus();
		else for(UINT n = 0; n < 4; n++) SendMessage(m_hwndStatus,SB_SETTEXTW,n,reinterpret_cast<LPARAM>(n ? L"" : L"Ready"));
		lResult = 0; return TRUE;
	case WM_SIZE:
		if(SIZE_MINIMIZED != wParam && SIZE_MAXHIDE != wParam)
		{
			RECT rc, rcStatus;
			INT nParts[4];
			INT cxTree, cxSplitter = m_pSplitter->GetWidth();

			GetClientRect(m_hwnd, &rc);
			cxTree = GetTreeWidth();

			GetClientRect(m_hwndStatus, &rcStatus);
			MoveWindow(m_hwndStatus, 0, rc.bottom - (rcStatus.bottom - rcStatus.top), rc.right - rc.left, rcStatus.bottom - rcStatus.top, TRUE);

			nParts[0] = max(0,rc.right - static_cast<INT>(DPI::Scale(330)));
			nParts[1] = nParts[0] + static_cast<INT>(DPI::Scale(110));
			nParts[2] = nParts[1] + static_cast<INT>(DPI::Scale(110));
			nParts[3] = -1;
			SendMessage(m_hwndStatus, SB_SETPARTS, ARRAYSIZE(nParts), (LPARAM)nParts);

			rc.bottom -= (rcStatus.bottom - rc.top);

			HWND palette = m_pProject ? m_pProject->ActivePalette() : NULL;
			INT height = rc.bottom - rc.top;
			INT treeHeight = palette ? max(80,min(height - 140,m_treeHeight ? m_treeHeight : height * 2 / 5)) : height;
			MoveWindow(m_hwndTree,rc.left,rc.top,cxTree,treeHeight,TRUE);
			if(palette) MoveWindow(palette,rc.left,treeHeight + 5,cxTree,max(1,height - treeHeight - 5),TRUE);
			m_pSplitter->Move(cxTree, 0, cxSplitter, rc.bottom - rc.top, TRUE);

			if(m_pProject)
			{
				rc.left += cxTree + cxSplitter;
				m_pProject->Move(rc);
			}
		}
		break;

	case WM_LBUTTONDOWN:
		if(m_pProject && m_pProject->ActivePalette())
		{
			RECT tree; GetWindowRect(m_hwndTree,&tree); MapWindowPoints(NULL,m_hwnd,reinterpret_cast<POINT*>(&tree),2);
			if(GET_X_LPARAM(lParam) < tree.right && abs(GET_Y_LPARAM(lParam) - tree.bottom - 2) <= 3) { m_dragTreeSplit = TRUE; SetCapture(m_hwnd); lResult = 0; return TRUE; }
		}
		break;
	case WM_MOUSEMOVE:
		if(m_dragTreeSplit)
		{
			m_treeHeight = GET_Y_LPARAM(lParam);
			PostMessage(m_hwnd,WM_STUDIO_DESIGNER,0,0);
			lResult = 0;
			return TRUE;
		}
		break;
	case WM_LBUTTONUP:
		if(m_dragTreeSplit)
		{
			m_dragTreeSplit = FALSE;
			ReleaseCapture();
			lResult = 0;
			return TRUE;
		}
		break;
	case WM_CAPTURECHANGED:
		m_dragTreeSplit = FALSE;
		break;
	case WM_INITMENUPOPUP:
		if(!HIWORD(lParam))
		{
			if(m_hRecentProjects && (HMENU)wParam == GetSubMenu(::GetMenu(m_hwnd), 0))
				m_recentMenu->Populate(m_hRecentProjects, NULL != m_pCustomMenu);
			if(SUCCEEDED(MenuUtil::EnableMenuItems(NULL, (HMENU)wParam, this)))
			{
				if(m_pCustomMenu)
					m_pCustomMenu->Rebuild((HMENU)wParam);
				lResult = 0;
				return TRUE;
			}
		}
		break;

	case WM_COMMAND:
		{
			HRESULT hr = Exec(NULL,LOWORD(wParam),0,NULL,NULL);
			if(m_recentMenu && m_recentMenu->Handles(LOWORD(wParam)))
			{
				if(FAILED(hr) && E_ABORT != hr && OLECMDERR_E_CANCELED != hr && OLECMDERR_E_DISABLED != hr)
				{
					CRString text;
					if(SUCCEEDED(RStrFormatW(&text, L"The recent project could not be opened (0x%.8x).", hr)))
						MessageBoxW(m_hwnd, text.c_wstr(), c_wzQuadooStudioTitle, MB_OK | MB_ICONERROR);
				}
				lResult = 0;
				return TRUE;
			}
			if(hr == S_OK || hr == OLECMDERR_E_CANCELED)
			{
				lResult = 0;
				return TRUE;
			}
			if(FAILED(hr) && (LOWORD(wParam) == ID_FILE_SAVEFILE || LOWORD(wParam) == ID_FILE_SAVEALL ||
				(LOWORD(wParam) >= ID_FORM_NEWFORM && LOWORD(wParam) <= ID_FORM_MOVESTATUSPANELRIGHT) || LOWORD(wParam) == ID_FORM_SYNCSOURCE))
			{
				CRString text;
				if(m_pProject && FAILED(m_pProject->SourceError()) && RStrLen(m_pProject->SourceDiagnostic()))
					MessageBoxW(m_hwnd,RStrToWide(m_pProject->SourceDiagnostic()),L"Source synchronization",MB_OK | MB_ICONERROR);
				else if(hr == HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH))
					MessageBoxW(m_hwnd,L"The source or definition changed on disk. Save your in-memory edits to a separate file, then close and reopen the project before saving.",L"File changed",MB_OK | MB_ICONERROR);
				else if(SUCCEEDED(RStrFormatW(&text,L"The command could not be completed (0x%.8x).",hr)))
					MessageBoxW(m_hwnd,text.c_wstr(),L"Quadoo Studio",MB_OK | MB_ICONERROR);
				lResult = 0;
				return TRUE;
			}
			break;
		}

	case WM_NOTIFY:
		{
			LPNMHDR pnmh = (LPNMHDR)lParam;
			if(TVN_CURSOR_CHANGE == pnmh->code && m_pProject && !m_pProject->DesignerActive())
			{
				WCHAR wzStatus[64];
				TVNCURSORINFO* pci = static_cast<TVNCURSORINFO*>(pnmh);

				Formatting::TPrintF(wzStatus, ARRAYSIZE(wzStatus), NULL, L"Ln %d", pci->nLineNo + 1);
				SendMessage(m_hwndStatus, SB_SETTEXT, 1, (LPARAM)wzStatus);

				Formatting::TPrintF(wzStatus, ARRAYSIZE(wzStatus), NULL, L"Col %d", pci->nColumnNo + 1);
				SendMessage(m_hwndStatus, SB_SETTEXT, 2, (LPARAM)wzStatus);
			}
			else if(m_pProject)
			{
				if(pnmh->hwndFrom == m_hwndTree)
				{
					HTREEITEM hItem;

					switch(pnmh->code)
					{
					case NM_CUSTOMDRAW:
						if(m_dm.IsDarkMode())
						{
							LPNMTVCUSTOMDRAW pCustomDraw = (LPNMTVCUSTOMDRAW)lParam;
							switch(pCustomDraw->nmcd.dwDrawStage)
							{
							case CDDS_PREPAINT:
								lResult = CDRF_NOTIFYITEMDRAW;
								return TRUE;
							case CDDS_ITEMPREPAINT:
								if(pCustomDraw->nmcd.uItemState & CDIS_SELECTED)
								{
									pCustomDraw->clrText = RGB(255, 255, 255);
									pCustomDraw->clrTextBk = RGB(70, 70, 70);
								}
								else
								{
									pCustomDraw->clrText = RGB(255, 255, 255);
									pCustomDraw->clrTextBk = RGB(40, 40, 40);
								}
								lResult = 0;
								return TRUE;
							}
						}
						break;
					case TVN_SELCHANGINGW:
						if(!m_pProject->IsTreeUpdating() && m_pProject->ActiveDesigner() && FAILED(m_pProject->ActiveDesigner()->CommitPropertyEdit())) { lResult = TRUE; return TRUE; }
						break;
					case TVN_SELCHANGEDW:
						{
							HRESULT hr = m_pProject->SelectTreeItem(reinterpret_cast<NMTREEVIEWW*>(lParam)->itemNew.hItem);
							if(FAILED(hr)) { CRString text; if(SUCCEEDED(RStrFormatW(&text,L"Unable to open the designer (0x%.8x). Check the associated definition and project metadata.",hr))) MessageBoxW(m_hwnd,text.c_wstr(),L"Designer",MB_OK | MB_ICONERROR); }
						}
						break;
					case NM_DBLCLK:
						{
							POINT pt;
							GetCursorPos(&pt);

							// Convert screen coordinates to client coordinates
							TVHITTESTINFO tvhti;
							tvhti.pt = pt;
							ScreenToClient(m_hwndTree, &tvhti.pt);

							hItem = TreeView_HitTest(m_hwndTree, &tvhti);
							if(hItem)
							{
								m_pProject->ActivateItem(hItem);
								return TRUE;
							}
						}
						break;
					case NM_RETURN:
						hItem = TreeView_GetSelection(m_hwndTree);
						if(hItem)
						{
							m_pProject->ActivateItem(hItem);
							return TRUE;
						}
						break;
					case NM_RCLICK:
						{
							// Get the cursor position in screen coordinates
							POINT pt;
							GetCursorPos(&pt);

							// Convert screen coordinates to client coordinates
							TVHITTESTINFO tvhti;
							tvhti.pt = pt;
							ScreenToClient(m_hwndTree, &tvhti.pt);

							hItem = TreeView_HitTest(m_hwndTree, &tvhti);
							if(hItem)
								m_pProject->ShowTreeContext(hItem, pt);
						}
						break;
					}
				}
			}
		}
		break;

	case WM_SETFOCUS:
		if(m_pProject)
		{
			HWND hwnd;
			m_pProject->GetWindow(&hwnd);
			SetFocus(hwnd);
		}
		break;

	case WM_CLOSE:
		{
			VARIANT vSystemClose, vCancel;

			vSystemClose.vt = VT_BOOL;
			vSystemClose.boolVal = VARIANT_TRUE;

			vCancel.vt = VT_EMPTY;

			if(Exec(NULL,ID_FILE_EXIT,0,&vSystemClose,&vCancel) == S_OK && vCancel.vt == VT_BOOL && vCancel.boolVal == VARIANT_TRUE)
			{
				lResult = 0;
				return TRUE;
			}
		}
		break;

	case WM_DESTROY:
		{
			HKEY hKey;
			if(SUCCEEDED(Registry::CreateKey(HKEY_CURRENT_USER, c_wzRegistryKey, KEY_WRITE, &hKey)))
			{
				INT cxNormalized = (INT)DPI::Normalize((FLOAT)GetTreeWidth());

				RegSetValueEx(hKey, L"TreeWidth", NULL, REG_DWORD, (LPBYTE)&cxNormalized, sizeof(cxNormalized));

				RegCloseKey(hKey);
			}
		}
		break;

	case WM_SETTINGCHANGE:
		UpdateColorScheme();
		InvalidateRect(m_hwnd, NULL, TRUE);
		break;
	}

	return __super::DefWindowProc(message, wParam, lParam, lResult);
}

HRESULT CQuadooStudio::OnCreate (VOID)
{
	HRESULT hr;
	INT cxTree = -1;
	HICON hClosedFolder = NULL, hOpenFolder = NULL, hQuadooFile = NULL;
	HMENU hFile = GetSubMenu(::GetMenu(m_hwnd), 0);

	for(INT i = 0, count = GetMenuItemCount(hFile); i < count; i++)
	{
		HMENU hSubMenu = GetSubMenu(hFile, i);
		if(hSubMenu && 0 <= MenuUtil::FindMenuItemPos(hSubMenu, ID_FILE_RECENT_FIRST))
		{
			m_hRecentProjects = hSubMenu;
			break;
		}
	}

	HKEY hKey;
	if(ERROR_SUCCESS == RegOpenKey(HKEY_CURRENT_USER, c_wzRegistryKey, &hKey))
	{
		DWORD cbData;

		if(ERROR_SUCCESS == RegQueryValueEx(hKey, L"TreeWidth", NULL, NULL, (LPBYTE)&cxTree, &cbData))
			cxTree = (INT)DPI::Scale((FLOAT)cxTree);

		RegCloseKey(hKey);
	}

	if(-1 == cxTree)
		cxTree = (INT)DPI::Scale(150.0f);

	m_hwndStatus = CreateStatusWindowW(WS_CHILD | WS_VISIBLE, L"Ready", m_hwnd, ID_STATUS_BAR);
	CheckIfGetLastError(NULL == m_hwndStatus);

	m_hImageList = ImageList_Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), ILC_COLOR32 | ILC_MASK, 3, 1);
	CheckIfGetLastError(NULL == m_hImageList);

	hClosedFolder = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_CLOSED_FOLDER));
	hOpenFolder = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_OPEN_FOLDER));
	hQuadooFile = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_QUADOO_FILE));

	ImageList_AddIcon(m_hImageList, hClosedFolder);
	ImageList_AddIcon(m_hImageList, hOpenFolder);
	ImageList_AddIcon(m_hImageList, hQuadooFile);

	m_hwndTree = CreateWindowEx(0, L"SysTreeView32",
		NULL, WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
		0, 0, cxTree, 100, m_hwnd, (HMENU)((UINT_PTR)IDC_PROJECT_FILES), m_hInstance, NULL);
	CheckIfGetLastError(NULL == m_hwndTree);

	m_pSplitter = __new CSplitter(m_hInstance);
	CheckAlloc(m_pSplitter);
	Check(m_pSplitter->Initialize(m_hwnd, m_hwndTree));

	TreeView_SetImageList(m_hwndTree, m_hImageList, TVSIL_NORMAL);

	UpdateColorScheme();

Cleanup:
	DestroyIcon(hClosedFolder);
	DestroyIcon(hOpenFolder);
	DestroyIcon(hQuadooFile);
	return hr;
}

INT CQuadooStudio::GetTreeWidth (VOID)
{
	RECT rc;
	GetWindowRect(m_hwndTree, &rc);
	return rc.right - rc.left;
}
