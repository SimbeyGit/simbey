#include <windows.h>
#include <shlobj.h>
#include <OleAcc.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "Library\Util\Registry.h"
#include "Library\ChooseFile.h"
#include "Library\MenuUtil.h"
#include "Library\DPI.h"
#include "resource.h"
#include "NeuralAPI.h"
#include "GdiList.h"
#include "NeuralDocument.h"
#include "NeuralNetApp.h"

const CHAR c_szWindowTitle[] = "Neural Circuits Simulator";

CNeuralNetApp::CNeuralNetApp (HINSTANCE hInstance)
{
	m_cRef = 1;

	m_hInstance = hInstance;
	m_hMenu = NULL;

	m_fCapture = FALSE;
	m_lpDoc = NULL;
	m_hOleAcc = NULL;

	m_lpTimers = NULL;
	m_cActiveTimers = 0;
	m_cMaxTimers = 0;
}

CNeuralNetApp::~CNeuralNetApp ()
{
	Assert(m_lpTimers == NULL);
	Assert(m_lpDoc == NULL);

	if(m_hOleAcc)
		FreeLibrary(m_hOleAcc);

	if(m_hMenu)
		DestroyMenu(m_hMenu);
}

HRESULT CNeuralNetApp::Initialize (INT nWidth, INT nHeight, INT nCmdShow)
{
	HRESULT hr = GdiList::Initialize();
	if(SUCCEEDED(hr))
		hr = Create(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE,WS_OVERLAPPEDWINDOW,"NeuralCircuitsCls",c_szWindowTitle,0,0,nWidth,nHeight,NULL,nCmdShow);
	return hr;
}

VOID CNeuralNetApp::Uninitialize (VOID)
{
	Destroy();
	GdiList::Uninitialize();
}

HRESULT CNeuralNetApp::RegisterFileType (VOID)
{
	HRESULT hr;

	DWORD dwDisposition = 0;
	HKEY hKey;
	LPSTR lpszModulePath, lpszPath = NULL;
	INT cPath = 512;

	lpszModulePath = __new CHAR[cPath];
	if(lpszModulePath)
	{
		cPath = ::GetModuleFileName(NULL,lpszModulePath,cPath);

		if(cPath > 0)
		{
			if(lpszModulePath[0] == '\\' && lpszModulePath[1] == '\\' && lpszModulePath[2] == '?' && lpszModulePath[3] == '\\')
				lpszPath = lpszModulePath + 4;
			else
				lpszPath = lpszModulePath;
			hr = S_OK;
		}
		else
			hr = HRESULT_FROM_WIN32(GetLastError());
	}
	else
		hr = E_OUTOFMEMORY;

	if(SUCCEEDED(hr))
	{
		if(RegCreateKeyEx(HKEY_CLASSES_ROOT,".snn",0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&hKey,&dwDisposition) == ERROR_SUCCESS)
		{
			if(RegSetValueEx(hKey,NULL,0,REG_SZ,(LPBYTE)"SnnFile",7) != ERROR_SUCCESS)
				hr = HRESULT_FROM_WIN32(GetLastError());
			else if(RegSetValueEx(hKey,"Content Type",0,REG_SZ,(LPBYTE)"application/snn",16) != ERROR_SUCCESS)
				hr = HRESULT_FROM_WIN32(GetLastError());
			RegCloseKey(hKey);
		}
		else
			hr = HRESULT_FROM_WIN32(GetLastError());
	}

	if(SUCCEEDED(hr))
	{
		if(RegCreateKeyEx(HKEY_CLASSES_ROOT,"SnnFile",0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&hKey,&dwDisposition) == ERROR_SUCCESS)
		{
			HKEY hKeyShell;
			if(RegSetValueEx(hKey,NULL,0,REG_SZ,(LPBYTE)"Simbey Neural Net (SNN) File",29) != ERROR_SUCCESS)
				hr = HRESULT_FROM_WIN32(GetLastError());
			if(SUCCEEDED(hr) && RegCreateKeyEx(hKey,"DefaultIcon",0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&hKeyShell,&dwDisposition) == ERROR_SUCCESS)
			{
				LPSTR lpszIcon = __new CHAR[cPath + 16];
				if(lpszIcon)
				{
					INT cchIcon = wsprintf(lpszIcon,"%s,1",lpszPath);
					if(RegSetValueEx(hKeyShell,NULL,0,REG_SZ,(LPBYTE)lpszIcon,cchIcon) != ERROR_SUCCESS)
						hr = HRESULT_FROM_WIN32(GetLastError());
					__delete_array lpszIcon;
				}
				else
					hr = E_OUTOFMEMORY;
				RegCloseKey(hKeyShell);
			}
			else
				hr = HRESULT_FROM_WIN32(GetLastError());
			if(SUCCEEDED(hr) && RegCreateKeyEx(hKey,"shell",0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&hKeyShell,&dwDisposition) == ERROR_SUCCESS)
			{
				HKEY hKeyCommand, hKeyAction;
				if(RegCreateKeyEx(hKeyShell,"Open",0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&hKeyAction,&dwDisposition) == ERROR_SUCCESS)
				{
					if(RegSetValueEx(hKeyAction,NULL,0,REG_SZ,(LPBYTE)"&Open",5) != ERROR_SUCCESS)
						hr = HRESULT_FROM_WIN32(GetLastError());
					if(SUCCEEDED(hr) && RegCreateKeyEx(hKeyAction,"command",0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&hKeyCommand,&dwDisposition) == ERROR_SUCCESS)
					{
						LPSTR lpszCommand = __new CHAR[cPath + 16];
						if(lpszCommand)
						{
							INT cCommand = wsprintf(lpszCommand,"\"%s\" /open \"%%L\"",lpszPath);
							if(RegSetValueEx(hKeyCommand,NULL,0,REG_SZ,(LPBYTE)lpszCommand,cCommand) == ERROR_SUCCESS)
								hr = S_OK;
							else
								hr = HRESULT_FROM_WIN32(GetLastError());
							RegCloseKey(hKeyCommand);
							__delete_array lpszCommand;
						}
						else
							hr = E_OUTOFMEMORY;
					}
					else
						hr = HRESULT_FROM_WIN32(GetLastError());
					RegCloseKey(hKeyAction);
				}
				else
					hr = HRESULT_FROM_WIN32(GetLastError());
				RegCloseKey(hKeyShell);
			}
			else
				hr = HRESULT_FROM_WIN32(GetLastError());
			RegCloseKey(hKey);
		}
		else
			hr = HRESULT_FROM_WIN32(GetLastError());

		if(SUCCEEDED(hr))
			SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
	}

	__delete_array lpszModulePath;

	return hr;
}

HRESULT CNeuralNetApp::UnregisterFileType (VOID)
{
	HRESULT hr;

	LONG a = Registry::RegDeleteKeyRecursive(HKEY_CLASSES_ROOT,".snn");
	LONG b = Registry::RegDeleteKeyRecursive(HKEY_CLASSES_ROOT,"SnnFile");
	if(a == ERROR_SUCCESS && b == ERROR_SUCCESS)
	{
		SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
		hr = S_OK;
	}
	else
		hr = S_FALSE;

	return hr;
}

HRESULT CNeuralNetApp::ExecCommand (LPCSTR lpcszCommand)
{
	HRESULT hr;

	if(TCompareLeftIChecked(lpcszCommand, "open "))
	{
		VARIANTARG vArg;
		CHAR szFile[MAX_PATH];
		INT cchFile;

		lpcszCommand += 5;
		if('"' == *lpcszCommand)
			lstrcpy(szFile, lpcszCommand + 1);
		else
			lstrcpy(szFile, lpcszCommand);

		cchFile = lstrlen(szFile);
		if('"' == szFile[cchFile - 1])
			szFile[--cchFile] = '\0';

		vArg.vt = VT_BYREF | VT_I1;
		vArg.pcVal = szFile;
		hr = Exec(NULL, IDM_OPEN, 0, &vArg, NULL);
	}
	else
		hr = E_FAIL;

	return hr;
}

HRESULT CNeuralNetApp::UpdateAppTitle (VOID)
{
	HRESULT hr;

	if(m_lpDoc)
	{
		LPSTR lpszFile;
		hr = m_lpDoc->GetFileName(&lpszFile);
		if(SUCCEEDED(hr))
		{
			CHAR szTitle[MAX_PATH + 100];
			if(lpszFile)
			{
				CHAR szName[MAX_PATH], *lpszPtr;
				lpszPtr = strrchr(lpszFile, '\\');
				if(lpszPtr)
					lstrcpy(szName, lpszPtr + 1);
				else
					lstrcpy(szName, lpszFile);
				wsprintf(szTitle, "%s - %s", c_szWindowTitle, szName);
			}
			else
				wsprintf(szTitle, "%s - <New Circuit>", c_szWindowTitle);
			SetWindowText(m_hwnd, szTitle);
			__delete_array lpszFile;
		}
	}
	else
	{
		SetWindowText(m_hwnd, c_szWindowTitle);
		hr = S_OK;
	}

	return hr;
}

BOOL CNeuralNetApp::PreTranslateMessage (LPMSG lpmsg)
{
	UNREFERENCED_PARAMETER(lpmsg);

	return FALSE;
}

// IUnknown

HRESULT WINAPI CNeuralNetApp::QueryInterface (REFIID iid, LPVOID* lplpvObject)
{
	HRESULT hr = E_INVALIDARG;
	if(lplpvObject)
	{
		if(iid == IID_IOleCommandTarget)
			*lplpvObject = (IOleCommandTarget*)this;
		else if(iid == IID_IBaseContainer)
			*lplpvObject = (IBaseContainer*)this;
		else if(iid == __uuidof(IBaseWindow))
			*lplpvObject = (IBaseWindow*)this;
		else if(iid == IID_IOleWindow)
			*lplpvObject = (IOleWindow*)this;
		else if(iid == IID_IUnknown)
			*lplpvObject = (IOleWindow*)this;
		else
		{
			hr = E_NOINTERFACE;
			goto exit;
		}
		AddRef();
		hr = S_OK;
	}

exit:
	return hr;
}

ULONG WINAPI CNeuralNetApp::AddRef (VOID)
{
	return (ULONG)InterlockedIncrement((LONG*)&m_cRef);
}

ULONG WINAPI CNeuralNetApp::Release (VOID)
{
	ULONG c = (ULONG)InterlockedDecrement((LONG*)&m_cRef);
	if(0 == c)
		__delete this;
	return c;
}

// IOleCommandTarget

HRESULT WINAPI CNeuralNetApp::QueryStatus (const GUID* lpguidCmdGroup, ULONG cCmds, OLECMD* lprgCmds, OLECMDTEXT* lpCmdText)
{
	ULONG i;

	if(m_lpDoc)
		m_lpDoc->QueryStatus(lpguidCmdGroup,cCmds,lprgCmds,lpCmdText);

	for(i = 0; i < cCmds; i++)
	{
		if(lprgCmds[i].cmdf == 0)
		{
			switch(lprgCmds[i].cmdID)
			{
			case IDM_NEW:
			case IDM_OPEN:
			case IDM_EXIT:
				lprgCmds[i].cmdf = OLECMDF_SUPPORTED | OLECMDF_ENABLED;
				break;

			case IDM_REOPEN:
			case IDM_CLOSE:
			case IDM_SAVE:
			case IDM_SAVE_AS:
				if(m_lpDoc)
					lprgCmds[i].cmdf = OLECMDF_SUPPORTED | OLECMDF_ENABLED;
				else
					lprgCmds[i].cmdf = OLECMDF_SUPPORTED;
				break;
			}
		}
	}

	return S_OK;
}

HRESULT WINAPI CNeuralNetApp::Exec (const GUID* lpguidCmdGroup, DWORD nCmdID, DWORD nCmdExecOpt, VARIANTARG* lpvaIn, VARIANTARG* lpvaOut)
{
	HRESULT hr = OLECMDERR_E_NOTSUPPORTED;

	switch(nCmdID)
	{
	case IDM_NEW:
	case IDM_OPEN:
		{
			CNeuralDocument* lpNew = __new CNeuralDocument(this);
			if(lpNew)
			{
				BOOL fUseNewDocument = FALSE;

				hr = lpNew->Initialize();

				if(nCmdID == IDM_OPEN && SUCCEEDED(hr))
				{
					if(lpvaIn)
						hr = lpNew->Exec(NULL, IDM_OPEN, 0, lpvaIn, NULL);
					else
					{
						CChooseFile Choose;
						if(SUCCEEDED(Choose.Initialize()) && Choose.OpenSingleFile(m_hwnd,"Simbey Neural Net\0*.snn\0BrainBox\0*.bbx\0\0"))
						{
							GUID guidHwnd;
							VARIANTARG vArg;

							ZeroMemory(&guidHwnd + sizeof(m_hwnd), sizeof(guidHwnd) - sizeof(m_hwnd));
							CopyMemory(&guidHwnd, &m_hwnd, sizeof(m_hwnd));

							vArg.vt = VT_BYREF | VT_I1;
							vArg.pcVal = const_cast<PSTR>(Choose.GetFile(0));
							hr = lpNew->Exec(&guidHwnd,IDM_OPEN,0,&vArg,NULL);
						}
						else
							hr = E_ABORT;
					}
					if(SUCCEEDED(hr))
						fUseNewDocument = TRUE;
					else if(E_ABORT != hr)
						MessageBox(m_hwnd,"Could not open the file!","File error",MB_OK);
				}
				else
					fUseNewDocument = SUCCEEDED(hr);

				if(m_lpDoc && SUCCEEDED(hr) && fUseNewDocument)
				{
					hr = Exec(lpguidCmdGroup,IDM_CLOSE,nCmdExecOpt,lpvaIn,lpvaOut);
					if(FAILED(hr))
						fUseNewDocument = FALSE;
				}

				if(SUCCEEDED(hr) && fUseNewDocument)
				{
					RECT rect;
					GetClientRect(m_hwnd,&rect);

					Assert(m_lpDoc == NULL);

					m_lpDoc = lpNew;
					m_lpDoc->AddRef();
					m_lpDoc->AttachContainer(this);
					m_lpDoc->SizeObject(rect.right - rect.left,rect.bottom - rect.top);

					::InvalidateRect(m_hwnd,NULL,FALSE);

					UpdateAppTitle();
				}

				lpNew->Release();
			}
			else
				hr = E_OUTOFMEMORY;
			break;
		}

	case IDM_REOPEN:
		if(m_lpDoc)
		{
			LPSTR lpszDup;
			hr = m_lpDoc->GetFileName(&lpszDup);
			if(SUCCEEDED(hr))
			{
				hr = Exec(NULL, IDM_CLOSE, 0, NULL, NULL);
				if(SUCCEEDED(hr))
					hr = Exec(NULL, IDM_NEW, 0, NULL, NULL);
				if(SUCCEEDED(hr))
				{
					GUID guidHwnd;
					VARIANTARG v;

					ZeroMemory(&guidHwnd + sizeof(m_hwnd), sizeof(guidHwnd) - sizeof(m_hwnd));
					CopyMemory(&guidHwnd, &m_hwnd, sizeof(m_hwnd));

					v.pcVal = lpszDup;
					v.vt = VT_BYREF | VT_I1;
					hr = m_lpDoc->Exec(&guidHwnd, IDM_OPEN, 0, &v, NULL);
				}
				if(SUCCEEDED(hr))
					UpdateAppTitle();
				__delete_array lpszDup;
			}
		}
		else
			hr = E_FAIL;
		break;

	case IDM_CLOSE:
		if(m_lpDoc->ChangesMade())
		{
			LRESULT lResult = MessageBox(m_hwnd,"Do you want to save your changes?","Save Changes",MB_YESNOCANCEL);
			if(lResult == IDYES)
				hr = Exec(lpguidCmdGroup,IDM_SAVE,nCmdExecOpt,lpvaIn,lpvaOut);
			else if(lResult == IDCANCEL)
			{
				hr = OLECMDERR_E_CANCELED;
				break;
			}
			else
				hr = S_OK;
		}
		else
			hr = S_OK;

		m_lpDoc->AttachContainer(NULL);
		m_lpDoc->Release();
		m_lpDoc = NULL;

		::InvalidateRect(m_hwnd,NULL,FALSE);

		UpdateAppTitle();

		break;

	case IDM_SAVE:
	case IDM_SAVE_AS:
		if(nCmdID == IDM_SAVE_AS || m_lpDoc->IsSavedOnDisk() == FALSE)
		{
			CChooseFile Choose;
			if(SUCCEEDED(Choose.Initialize()) && Choose.SaveFile(m_hwnd,"Simbey Neural Net\0*.snn\0\0"))
			{
				VARIANTARG vArg;
				vArg.vt = VT_BYREF | VT_I1;
				vArg.pcVal = const_cast<PSTR>(Choose.GetFile(0));
				hr = m_lpDoc->Exec(NULL,IDM_SAVE,0,&vArg,NULL);
			}
			else
				hr = S_FALSE;
		}
		else
			hr = m_lpDoc->Exec(NULL,IDM_SAVE,0,NULL,NULL);
		break;

	case IDM_EXIT:
		if(m_lpDoc)
			hr = Exec(lpguidCmdGroup,IDM_CLOSE,nCmdExecOpt,lpvaIn,lpvaOut);
		else
			hr = S_OK;

		if(hr != OLECMDERR_E_CANCELED)
		{
			WINDOWPLACEMENT wp = {0};
			wp.length = sizeof(WINDOWPLACEMENT);
			if(GetWindowPlacement(m_hwnd,&wp))
			{
				HKEY hKey;
				if(SUCCEEDED(Registry::CreateKey(HKEY_CURRENT_USER, "Software\\Simbey\\NeuralNet", KEY_WRITE, &hKey)))
				{
					// Normalize the scaled window placement to 96 DPI
					DPI::NormalizeScaledPoint(&wp.ptMaxPosition);
					DPI::NormalizeScaledPoint(&wp.ptMinPosition);
					DPI::NormalizeScaledRectSize(&wp.rcNormalPosition);

					RegSetValueEx(hKey,"WindowPlacement",NULL,REG_BINARY,(LPBYTE)&wp,sizeof(WINDOWPLACEMENT));
					RegCloseKey(hKey);
				}
			}
			PostQuitMessage(0);
			hr = S_OK;
		}
		break;

	default:
		if(m_lpDoc)
			hr = m_lpDoc->Exec(lpguidCmdGroup,nCmdID,nCmdExecOpt,lpvaIn,lpvaOut);
		break;
	}

	return hr;
}

// IBaseContainer

HRESULT WINAPI CNeuralNetApp::GetHwnd (HWND* lphwnd)
{
	HRESULT hr = E_INVALIDARG;
	if(lphwnd)
	{
		*lphwnd = m_hwnd;
		if(m_hwnd)
			hr = S_OK;
		else
			hr = E_FAIL;
	}
	return hr;
}

HRESULT WINAPI CNeuralNetApp::GetCapture (IWindowless** lplpObject)
{
	if(m_fCapture && m_lpDoc)
	{
		*lplpObject = m_lpDoc;
		(*lplpObject)->AddRef();
	}
	else
		*lplpObject = NULL;

	return S_OK;
}

HRESULT WINAPI CNeuralNetApp::SetCapture (IWindowless* lpObject, BOOL fCapture)
{
	UNREFERENCED_PARAMETER(lpObject);

	HRESULT hr = E_FAIL;
	if(fCapture)
	{
		::SetCapture(m_hwnd);
		if(::GetCapture() == m_hwnd)
		{
			m_fCapture = TRUE;
			hr = S_OK;
		}
	}
	else if(m_fCapture)
	{
		m_fCapture = NULL;
		ReleaseCapture();
	}
	else
		hr = S_FALSE;
	return hr;
}

HRESULT WINAPI CNeuralNetApp::GetFocus (IWindowless** lplpObject)
{
	if(m_lpDoc)
	{
		*lplpObject = m_lpDoc;
		(*lplpObject)->AddRef();
	}
	else
		*lplpObject = NULL;

	return S_OK;
}

HRESULT WINAPI CNeuralNetApp::SetFocus (IWindowless* lpObject)
{
	UNREFERENCED_PARAMETER(lpObject);

	return S_OK;
}

HRESULT WINAPI CNeuralNetApp::SetTimer (IWindowless* lpObject, UINT uElapse, INT* lpnTimer)
{
	HRESULT hr = S_OK;
	INT nTimer = -1;
	for(INT n = 0; n < m_cMaxTimers; n++)
	{
		if(m_lpTimers[n] == NULL)
		{
			nTimer = n;
			break;
		}
	}
	if(nTimer == -1)
	{
		INT cNewMax = (m_cMaxTimers == 0) ? 4 : (m_cMaxTimers << 1);
		IWindowless** lpNew = __new IWindowless*[cNewMax];
		if(lpNew)
		{
			CopyMemory(lpNew,m_lpTimers,sizeof(IWindowless*) * m_cMaxTimers);
			__delete_array m_lpTimers;
			nTimer = m_cMaxTimers;
			m_lpTimers = lpNew;
			m_cMaxTimers = cNewMax;
		}
		else
			hr = E_OUTOFMEMORY;
	}
	if(nTimer != -1)
	{
		if(::SetTimer(m_hwnd,nTimer + 1,uElapse,NULL) != 0)
		{
			m_lpTimers[nTimer] = lpObject;
			lpObject->AddRef();
			*lpnTimer = nTimer + 1;
			m_cActiveTimers++;
		}
		else
			hr = E_FAIL;
	}
	return hr;
}

HRESULT WINAPI CNeuralNetApp::KillTimer (INT nTimer)
{
	HRESULT hr = E_FAIL;
	if(nTimer > 0 && nTimer <= m_cMaxTimers && m_lpTimers[nTimer - 1])
	{
		if(::KillTimer(m_hwnd,nTimer))
		{
			m_lpTimers[nTimer - 1]->Release();
			m_lpTimers[nTimer - 1] = NULL;

			if(--m_cActiveTimers == 0)
			{
				__delete_array m_lpTimers;
				m_lpTimers = NULL;
				m_cMaxTimers = 0;
			}

			hr = S_OK;
		}
	}
	return hr;
}

HRESULT WINAPI CNeuralNetApp::GetDC (IWindowless* lpObject, HDC* lphdc)
{
	UNREFERENCED_PARAMETER(lpObject);
	HRESULT hr = E_FAIL;

	*lphdc = ::GetDC(m_hwnd);
	if(*lphdc)
		hr = S_OK;

	return hr;
}

HRESULT WINAPI CNeuralNetApp::ReleaseDC (IWindowless* lpObject, HDC hdc)
{
	UNREFERENCED_PARAMETER(lpObject);
	HRESULT hr = E_INVALIDARG;

	if(hdc)
	{
		::ReleaseDC(m_hwnd,hdc);
		hr = S_OK;
	}

	return hr;
}

HRESULT WINAPI CNeuralNetApp::InvalidateContainer (VOID)
{
	::InvalidateRect(m_hwnd,NULL,FALSE);
	return S_OK;
}

HRESULT WINAPI CNeuralNetApp::OnWindowlessMessage (IWindowless* lpObject, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* lplResult)
{
	UNREFERENCED_PARAMETER(lpObject);

	HRESULT hr = S_FALSE;
	if(DefWindowProc(msg,wParam,lParam,*lplResult))
		hr = S_OK;

	return hr;
}

HRESULT WINAPI CNeuralNetApp::ScreenToWindowless (IWindowless* lpObject, LPPOINT lpPoint)
{
	UNREFERENCED_PARAMETER(lpObject);

	HRESULT hr = E_INVALIDARG;

	if(lpPoint)
	{
		if(ScreenToClient(m_hwnd, lpPoint))
			hr = S_OK;
		else
			hr = HRESULT_FROM_WIN32(GetLastError());
	}

	return hr;
}

HRESULT WINAPI CNeuralNetApp::TrackPopupMenu (HMENU hMenu, INT x, INT y, IOleCommandTarget* lpTarget)
{
	HRESULT hr;

	Assert(lpTarget);

	if(m_hwnd)
	{
		POINT pt = {x,y};
		if(ClientToScreen(m_hwnd,&pt))
		{
			INT nCmd = (INT)::TrackPopupMenu(hMenu,TPM_RETURNCMD,pt.x,pt.y,0,m_hwnd,NULL);
			hr = lpTarget->Exec(NULL,nCmd,0,NULL,NULL);
		}
		else
			hr = HRESULT_FROM_WIN32(GetLastError());
	}
	else
		hr = E_UNEXPECTED;

	return hr;
}

// CBaseWindow

HRESULT CNeuralNetApp::Register (HINSTANCE hInstance)
{
	WNDCLASSEX wnd;
	ZeroMemory(&wnd,sizeof(WNDCLASSEX));

	wnd.cbSize = sizeof(WNDCLASSEX);
	wnd.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;
	wnd.cbClsExtra = 0;
	wnd.cbWndExtra = 0;
	wnd.hInstance = hInstance;
	wnd.hIcon = LoadIcon(hInstance,MAKEINTRESOURCE(IDI_MAIN));
	wnd.hCursor = LoadCursor(NULL, IDC_ARROW);
	wnd.hbrBackground = NULL;
	wnd.lpszMenuName = NULL;
	wnd.lpszClassName = "NeuralCircuitsCls";

	return RegisterClass(&wnd,NULL);
}

HINSTANCE CNeuralNetApp::GetInstance (VOID)
{
	return m_hInstance;
}

VOID CNeuralNetApp::OnFinalDestroy (HWND /*hwnd*/)
{
}

HRESULT CNeuralNetApp::FinalizeAndShow (DWORD dwStyle, INT nCmdShow)
{
	BOOL fSuccess = FALSE;
	HKEY hKey;
	if(SUCCEEDED(Registry::CreateKey(HKEY_CURRENT_USER, "Software\\Simbey\\NeuralNet", KEY_READ, &hKey)))
	{
		WINDOWPLACEMENT wp;
		DWORD cbSize = sizeof(WINDOWPLACEMENT);
		if(RegQueryValueEx(hKey,"WindowPlacement",NULL,NULL,(LPBYTE)&wp,&cbSize) == ERROR_SUCCESS)
		{
			if(cbSize == sizeof(WINDOWPLACEMENT) && wp.rcNormalPosition.left < wp.rcNormalPosition.right && wp.rcNormalPosition.top < wp.rcNormalPosition.bottom)
			{
				wp.length = sizeof(WINDOWPLACEMENT);

				// If the startup visibility option is uninteresting, use the saved option.
				if(SW_SHOWDEFAULT == nCmdShow || SW_SHOWNORMAL == nCmdShow || SW_SHOW == nCmdShow)
					nCmdShow = wp.showCmd;

				// Call SetWindowPlacement() with SW_HIDE to avoid flashing the window white.
				wp.showCmd = SW_HIDE;

				// Normalize the scaled window placement from 96 DPI
				DPI::ScaleNormalizedPoint(&wp.ptMaxPosition);
				DPI::ScaleNormalizedPoint(&wp.ptMinPosition);
				DPI::ScaleNormalizedRectSize(&wp.rcNormalPosition);

				fSuccess = ::SetWindowPlacement(m_hwnd,&wp);
			}
		}
		RegCloseKey(hKey);
	}

	return CBaseWindow::FinalizeAndShow(dwStyle,nCmdShow);
}

HMENU CNeuralNetApp::GetMenu (VOID)
{
	if(m_hMenu == NULL)
		m_hMenu = LoadMenu(m_hInstance,MAKEINTRESOURCE(IDR_MENU));
	return m_hMenu;
}

BOOL CNeuralNetApp::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, __out LRESULT& lResult)
{
	BOOL fHandled = FALSE;
	switch(message)
	{
	case WM_TIMER:
		if(wParam > 0 && wParam <= (WPARAM)m_cMaxTimers)
			fHandled = m_lpTimers[wParam - 1]->OnMessage(WM_TIMER,wParam,lParam,lResult);
		break;

	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(m_hwnd,&ps);
			if(m_lpDoc)
				m_lpDoc->Paint(hdc);
			else
			{
				RECT rect;
				GetClientRect(m_hwnd,&rect);
				FillRect(hdc,&rect,(HBRUSH)GetStockObject(GRAY_BRUSH));
			}
			EndPaint(m_hwnd,&ps);
			break;
		}

	case WM_ERASEBKGND:
		lResult = 1;
		fHandled = TRUE;
		break;

	case WM_SIZE:
		if(SIZE_MINIMIZED != LOWORD(wParam))
		{
			// Only process sizing if the window was not minimized.
			if(m_lpDoc)
				m_lpDoc->SizeObject(LOWORD(lParam), HIWORD(lParam));
		}
		break;

	case WM_COMMAND:
		{
			HRESULT hr = Exec(NULL,LOWORD(wParam),0,NULL,NULL);
			if(hr == S_OK || hr == OLECMDERR_E_CANCELED)
				fHandled = TRUE;
			break;
		}

	case WM_INITMENUPOPUP:
		if(!HIWORD(lParam))
		{
			if(m_lpDoc)
				m_lpDoc->OnModifyMenu((HMENU)wParam);
			if(SUCCEEDED(MenuUtil::EnableMenuItems(NULL,(HMENU)wParam, this)))
			{
				lResult = 0;
				fHandled = TRUE;
			}
		}
		break;

	case WM_CAPTURECHANGED:
		if(m_fCapture)
		{
			m_fCapture = FALSE;
			if(m_lpDoc)
				fHandled = m_lpDoc->OnMessage(message,wParam,lParam,lResult);
		}
		break;

	case WM_CLOSE:
		if(Exec(NULL,IDM_EXIT,0,NULL,NULL) != S_OK)
			fHandled = TRUE;
		break;

	case WM_GETOBJECT:
		if(lParam == OBJID_CLIENT && m_lpDoc)
		{
			IAccessible* lpAccessible;
			if(SUCCEEDED(m_lpDoc->GetAccObject(&lpAccessible)))
			{
				if(m_hOleAcc == NULL)
					m_hOleAcc = LoadLibrary("OLEACC.DLL");
				if(m_hOleAcc)
				{
					LRESULT (WINAPI* lpfnLresultFromObject)(REFIID,WPARAM,LPUNKNOWN);
					lpfnLresultFromObject = (LRESULT(WINAPI*)(REFIID,WPARAM,LPUNKNOWN))GetProcAddress(m_hOleAcc,"LresultFromObject");
					if(lpfnLresultFromObject)
					{
						lResult = lpfnLresultFromObject(IID_IAccessible,wParam,lpAccessible);
						fHandled = SUCCEEDED((HRESULT)lResult);
					}
				}
				lpAccessible->Release();
			}
		}
		break;

	default:
		if(m_lpDoc)
			fHandled = m_lpDoc->OnMessage(message,wParam,lParam,lResult);
		break;
	}

	if(fHandled == FALSE)
		fHandled = CBaseWindow::DefWindowProc(message,wParam,lParam,lResult);

	return fHandled;
}