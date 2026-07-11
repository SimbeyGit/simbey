#include <windows.h>
#include <commctrl.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "QuadooProject.h"
#include "StartDebuggerDlg.h"

#define	WM_START_DEBUGGER	(WM_USER + 10)
#define	WM_DEBUGGER_STATUS	(WM_USER + 11)
#define	WM_FINISH_DEBUGGER	(WM_USER + 12)

CStartDebuggerDlg::CStartDebuggerDlg (CQuadooProject* pProject, PCWSTR pcwzDebugger, PCWSTR pcwzStartDir, PCWSTR pcwzScriptPath, RSTRING rstrScriptArgs, INT nPort) :
	CBaseDialog(IDD_START_DEBUGGER),
	m_pProject(pProject),
	m_pcwzDebugger(pcwzDebugger),
	m_pcwzStartDir(pcwzStartDir),
	m_pcwzScriptPath(pcwzScriptPath),
	m_rstrScriptArgs(rstrScriptArgs),
	m_nPort(nPort),
	m_pDebugTree(NULL),
	m_hwndStatus(NULL),
	m_hDebuggerIcon(NULL),
	m_hStartThread(NULL),
	m_fAbort(FALSE),
	m_pDebugger(NULL),
	m_rstrRemoteHost(NULL),
	m_fDisplayedErrors(false)
{
	m_pProject->AddRef();
	RStrAddRef(m_rstrScriptArgs);
}

CStartDebuggerDlg::~CStartDebuggerDlg ()
{
	Assert(NULL == m_pDebugger);

	if(m_hDebuggerIcon)
		DestroyIcon(m_hDebuggerIcon);
	SafeRelease(m_pDebugTree);
	RStrRelease(m_rstrRemoteHost);
	RStrRelease(m_rstrScriptArgs);
	m_pProject->Release();
}

HRESULT CStartDebuggerDlg::Start (VOID)
{
	HRESULT hr;
	HWND hwnd;
	DWORD idThread;

	Check(GetWindow(&hwnd));

	Check(QuadooParseToStream(m_pcwzScriptPath, QUADOO_COMPILE_LINE_NUMBER_MAP | QUADOO_COMPILE_FOR_DEBUGGING, &m_stmQBC, &m_stmDebug, this, &m_pDebugTree));
	SetWindowText(m_hwndStatus, L"Starting debugger...");
	UpdateWindow(m_hwndStatus);

	Check(m_pProject->CreateDebugger(&m_pDebugger, &m_stmBreakpoints, &m_rstrRemoteHost));

	m_hStartThread = CreateThread(NULL, 0, _StartAsync, this, 0, &idThread);
	CheckIfGetLastError(NULL == m_hStartThread);

Cleanup:
	if(FAILED(hr))
		PostMessage(hwnd, WM_FINISH_DEBUGGER, (WPARAM)hr, 0);
	return hr;
}

BOOL CStartDebuggerDlg::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	HWND hwnd;

	switch(message)
	{
	case WM_INITDIALOG:
		m_hwndStatus = GetDlgItem(IDC_STATUS);
		SetDebuggerIcon();
		CenterHost();
		break;

	case WM_SHOWWINDOW:
		SideAssertHr(GetWindow(&hwnd));
		PostMessage(hwnd, WM_START_DEBUGGER, 0, 0);
		break;

	case WM_CLOSE:
		EnableWindow(GetDlgItem(IDC_ABORT), FALSE);
		m_fAbort = TRUE;
		return TRUE;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDC_ABORT:
			EnableWindow(GetDlgItem(IDC_ABORT), FALSE);
			m_fAbort = TRUE;
			break;
		}
		break;

	case WM_START_DEBUGGER:
		if(FAILED(Start()))
			End(IDABORT);
		break;

	case WM_DEBUGGER_STATUS:
		{
			RSTRING rstrStatus = (RSTRING)lParam;
			SetWindowText(m_hwndStatus, RStrToWide(rstrStatus));
			UpdateWindow(m_hwndStatus);
			RStrRelease(rstrStatus);
		}
		break;

	case WM_FINISH_DEBUGGER:
		{
			HRESULT hr = (HRESULT)wParam;
			SideAssertHr(GetWindow(&hwnd));
			if(SUCCEEDED(hr))
			{
				hr = m_pProject->AttachDebugger(m_pDebugger, m_pDebugTree);
				m_pDebugger = NULL;	// Either owned by or destroyed by AttachDebugger()
			}
			SafeDelete(m_pDebugger);
			SetForegroundWindow(hwnd);
			End(SUCCEEDED(hr) ? IDOK : IDCANCEL);
		}
		break;
	}

	return FALSE;
}

HRESULT CStartDebuggerDlg::SetDebuggerIcon (VOID)
{
	HRESULT hr;
	HICON hLargeIcon = NULL, hSmallIcon = NULL;
	UINT cIcons;

	CheckIf(NULL == m_pcwzDebugger, E_INVALIDARG);
	cIcons = ExtractIconExW(m_pcwzDebugger, 0, &hLargeIcon, &hSmallIcon, 1);
	CheckIf(0 == cIcons || static_cast<UINT>(-1) == cIcons ||
		(NULL == hLargeIcon && NULL == hSmallIcon),
		HRESULT_FROM_WIN32(ERROR_RESOURCE_DATA_NOT_FOUND));

	if(hLargeIcon)
	{
		m_hDebuggerIcon = hLargeIcon;
		hLargeIcon = NULL;
	}
	else
	{
		m_hDebuggerIcon = hSmallIcon;
		hSmallIcon = NULL;
	}
	SendMessage(GetDlgItem(IDC_DEBUGGER_ICON), STM_SETICON,
		reinterpret_cast<WPARAM>(m_hDebuggerIcon), 0);
	hr = S_OK;

Cleanup:
	if(hLargeIcon)
		DestroyIcon(hLargeIcon);
	if(hSmallIcon)
		DestroyIcon(hSmallIcon);
	return hr;
}

HRESULT CStartDebuggerDlg::StartAsync (VOID)
{
	HRESULT hr;
	HWND hwnd = NULL;

	Check(GetWindow(&hwnd));
	Check(m_pDebugger->Start(m_pcwzDebugger, m_pcwzStartDir, m_rstrRemoteHost, m_nPort, m_pcwzScriptPath, m_rstrScriptArgs, &m_stmQBC, &m_stmDebug, &m_stmBreakpoints, this));

Cleanup:
	if(hwnd)
		PostMessage(hwnd, WM_FINISH_DEBUGGER, (WPARAM)hr, 0);
	return hr;
}

DWORD CALLBACK CStartDebuggerDlg::_StartAsync (PVOID pvParam)
{
	return reinterpret_cast<CStartDebuggerDlg*>(pvParam)->StartAsync();
}

// IStartDebuggerStatus

VOID CStartDebuggerDlg::ReportStatus (PCWSTR pcwzStatus, INT cchStatus)
{
	HWND hwnd;
	if(SUCCEEDED(GetWindow(&hwnd)))
	{
		RSTRING rstrStatus;
		if(SUCCEEDED(RStrCreateW(cchStatus, pcwzStatus, &rstrStatus)))
			SendMessage(hwnd, WM_DEBUGGER_STATUS, 0, (LPARAM)rstrStatus);
	}
}

BOOL CStartDebuggerDlg::CheckAbortFlag (VOID)
{
	return m_fAbort;
}

// IQuadooCompilerStatus

VOID STDMETHODCALLTYPE CStartDebuggerDlg::OnCompilerAddFile (PCWSTR pcwzFile, INT cchFile)
{
}

VOID STDMETHODCALLTYPE CStartDebuggerDlg::OnCompilerStatus (PCWSTR pcwzStatus)
{
}

VOID STDMETHODCALLTYPE CStartDebuggerDlg::OnCompilerError (HRESULT hrCode, INT nLine, PCWSTR pcwzFile, PCWSTR pcwzError)
{
	HWND hwnd;
	if(!m_fDisplayedErrors && SUCCEEDED(GetWindow(&hwnd)))
	{
		RSTRING rstrError;
		if(SUCCEEDED(RStrFormatW(&rstrError, L"%ls(%d) - %ls", pcwzFile, nLine, pcwzError)))
		{
			MessageBox(hwnd, RStrToWide(rstrError), L"Compilation Error", MB_OK | MB_ICONHAND);
			RStrRelease(rstrError);

			m_fDisplayedErrors = true;
		}
	}
}

STDMETHODIMP CStartDebuggerDlg::OnCompilerResolvePath (PCWSTR pcwzPath, __out_ecount(cchMaxAbsolutePath) PWSTR pwzAbsolutePath, INT cchMaxAbsolutePath)
{
	return E_NOTIMPL;
}
