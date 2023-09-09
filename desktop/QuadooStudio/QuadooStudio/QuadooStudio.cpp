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

const WCHAR c_wzQuadooStudioClass[] = L"QuadooStudioCls";
const WCHAR c_wzQuadooStudioTitle[] = L"Quadoo Studio";
const WCHAR c_wzRegistryKey[] = L"Software\\Simbey\\QuadooStudio";

CQuadooStudio::CQuadooStudio (HINSTANCE hInstance) :
	m_hInstance(hInstance),
	m_hAccel(NULL),
	m_pGdiPlusStartupInput(NULL),
	m_gdiplusToken(NULL),
	m_hwndStatus(NULL),
	m_hwndTree(NULL),
	m_hImageList(NULL),
	m_pCustomMenu(NULL),
	m_pSplitter(NULL),
	m_pProject(NULL)
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

HRESULT CQuadooStudio::Initialize (PCWSTR pcwzCmdLine, INT nWidth, INT nHeight, INT nCmdShow)
{
	HRESULT hr;

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
	return TranslateAccelerator(m_hwnd, m_hAccel, pmsg);
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
		switch(prgCmds[i].cmdID)
		{
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

	switch(nCmdID)
	{
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
		TreeView_DeleteAllItems(m_hwndTree);
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

	Check(JSONCreateArray(&srFiles));
	Check(JSONWrapArray(srFiles, &srv));
	Check(srProject->AddValueW(L"files", srv));

	Check(JSONSerializeObject(srProject, &stmJSON));
	Check(Text::SaveToFile(stmJSON.TGetReadPtr<WCHAR>(), stmJSON.TDataRemaining<WCHAR>(), dlgNewProject.GetProjectFile()));

	Check(OpenProject(dlgNewProject.GetProjectFile()));

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
	CheckIfIgnore(!pick.OpenSingleFile(m_hwnd, L"Open Project File", L"Project (*.qsproj)\0*.qsproj"), E_ABORT);
	Check(OpenProject(pick.GetFile(0)));

Cleanup:
	return hr;
}

HRESULT CQuadooStudio::OpenProject (PCWSTR pcwzProject)
{
	HRESULT hr;
	CQuadooProject* pProject = NULL;
	RECT rcStatus, rcSite;

	GetClientRect(m_hwnd, &rcSite);
	rcSite.left += GetTreeWidth() + m_pSplitter->GetWidth();

	GetClientRect(m_hwndStatus, &rcStatus);
	rcSite.bottom -= (rcStatus.bottom - rcStatus.top);

	pProject = __new CQuadooProject(m_hInstance, &m_dm, m_hwndTree);
	CheckAlloc(pProject);
	Check(pProject->Initialize(m_hwnd, rcSite, pcwzProject));

	UpdateTitle(pcwzProject);

	m_pProject = pProject;
	pProject = NULL;

Cleanup:
	if(pProject)
	{
		pProject->CloseProject(FALSE);
		SafeRelease(pProject);
	}
	return hr;
}

HRESULT CQuadooStudio::UpdateTitle (PCWSTR pcwzProject)
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

	Check(RStrFormatW(&rstrTitle, L"%r - %ls", rstrLabel, c_wzQuadooStudioTitle));
	SetWindowText(m_hwnd, RStrToWide(rstrTitle));

Cleanup:
	RStrRelease(rstrTitle);
	RStrRelease(rstrLabel);
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
	switch(message)
	{
	case WM_CREATE:
		if(FAILED(OnCreate()))
		{
			lResult = -1;
			return TRUE;
		}
		break;

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

			nParts[0] = (rc.right - rc.left) / 2;
			nParts[1] = nParts[0] + 80;
			nParts[2] = nParts[1] + 80;
			nParts[3] = -1;
			SendMessage(m_hwndStatus, SB_SETPARTS, ARRAYSIZE(nParts), (LPARAM)nParts);

			rc.bottom -= (rcStatus.bottom - rc.top);

			MoveWindow(m_hwndTree, rc.left, rc.top, cxTree, rc.bottom - rc.top, TRUE);
			m_pSplitter->Move(cxTree, 0, cxSplitter, rc.bottom - rc.top, TRUE);

			if(m_pProject)
			{
				rc.left += cxTree + cxSplitter;
				m_pProject->Move(rc);
			}
		}
		break;

	case WM_INITMENUPOPUP:
		if(!HIWORD(lParam))
		{
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
			if(hr == S_OK || hr == OLECMDERR_E_CANCELED)
			{
				lResult = 0;
				return TRUE;
			}
			break;
		}

	case WM_NOTIFY:
		{
			LPNMHDR pnmh = (LPNMHDR)lParam;
			if(TVN_CURSOR_CHANGE == pnmh->code)
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
