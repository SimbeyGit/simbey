#include <windows.h>
#include <shlwapi.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "Library\Util\RString.h"
#include "Library\Util\Formatting.h"
#include "Published\JSON.h"
#include "RunParamsDlg.h"

CRunParamsDlg::CRunParamsDlg (IJSONObject* pProject, RSTRING rstrEngine, RSTRING rstrProjectDir) :
	CBaseDialog(IDD_RUN_PARAMS)
{
	SetInterface(m_pProject, pProject);
	RStrSet(m_rstrEngine, rstrEngine);
	RStrSet(m_rstrProjectDir, rstrProjectDir);
}

CRunParamsDlg::~CRunParamsDlg ()
{
	RStrRelease(m_rstrProjectDir);
	RStrRelease(m_rstrEngine);
	SafeRelease(m_pProject);
}

BOOL CRunParamsDlg::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	switch(message)
	{
	case WM_INITDIALOG:
		SetWindowText(GetDlgItem(IDC_INSTALLED_ENGINE), RStrToWide(m_rstrEngine));
		ReadArgsAndDir();
		SetFocus(GetDlgItem(IDC_COMMAND_ARGS));
		CenterHost();
		break;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDC_AUTO_START_DEBUGGER:
			if(BN_CLICKED == HIWORD(wParam))
				UpdateDebugHostState();
			break;

		case IDOK:
			if(FAILED(WriteArgsAndDir()))
				break;
			__fallthrough;
		case IDCANCEL:
			End(LOWORD(wParam));
			break;
		}
		break;
	}

	return FALSE;
}

HRESULT CRunParamsDlg::ReadArgsAndDir (VOID)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	RSTRING rstrValue = NULL;

	if(SUCCEEDED(m_pProject->FindNonNullValueW(L"args", &srv)))
	{
		Check(srv->GetString(&rstrValue));
		SetWindowText(GetDlgItem(IDC_COMMAND_ARGS), RStrToWide(rstrValue));

		RStrRelease(rstrValue); rstrValue = NULL;
		srv.Release();
	}

	if(SUCCEEDED(m_pProject->FindNonNullValueW(L"startDir", &srv)))
	{
		PWSTR pwzAbsolute;
		INT cchAbsolute;

		Check(srv->GetString(&rstrValue));

		Check(Formatting::TBuildDirectory(RStrToWide(m_rstrProjectDir), RStrLen(m_rstrProjectDir), RStrToWide(rstrValue), RStrLen(rstrValue), &pwzAbsolute, &cchAbsolute));
		SetWindowText(GetDlgItem(IDC_STARTING_DIR), pwzAbsolute);

		__delete_array pwzAbsolute;
		RStrRelease(rstrValue); rstrValue = NULL;
		srv.Release();
	}

	if(SUCCEEDED(m_pProject->FindNonNullValueW(L"debugPort", &srv)))
	{
		INT nDebugPort;
		WCHAR wzDebugPort[32];

		Check(srv->GetInteger(&nDebugPort));
		srv.Release();

		Check(Formatting::TInt32ToAsc(nDebugPort, wzDebugPort, ARRAYSIZE(wzDebugPort), 10, NULL));
		SetWindowText(GetDlgItem(IDC_DEBUG_PORT), wzDebugPort);
	}
	else
		SetWindowText(GetDlgItem(IDC_DEBUG_PORT), L"1200");

	if(SUCCEEDED(m_pProject->FindNonNullValueW(L"autoStartDebugger", &srv)))
	{
		bool fChecked;

		Check(srv->GetBoolean(&fChecked));
		srv.Release();

		if(fChecked)
			SendMessage(GetDlgItem(IDC_AUTO_START_DEBUGGER), BM_SETCHECK, BST_CHECKED, 0);
	}
	else
	{
		// Default the option to be checked
		SendMessage(GetDlgItem(IDC_AUTO_START_DEBUGGER), BM_SETCHECK, BST_CHECKED, 0);
	}
	UpdateDebugHostState();

	if(SUCCEEDED(m_pProject->FindNonNullValueW(L"remoteHost", &srv)))
	{
		Check(srv->GetString(&rstrValue));
		SetWindowText(GetDlgItem(IDC_DEBUG_HOST), RStrToWide(rstrValue));
		RStrRelease(rstrValue); rstrValue = NULL;
		srv.Release();
	}

	hr = S_OK;

Cleanup:
	RStrRelease(rstrValue);
	return hr;
}

HRESULT CRunParamsDlg::WriteArgsAndDir (VOID)
{
	HRESULT hr;
	HWND hwnd;
	INT cch;
	PWSTR pwzPtr;
	WCHAR wzDebugPort[32];
	RSTRING rstrValue = NULL;
	WCHAR wzRelative[MAX_PATH];
	TStackRef<IJSONValue> srv;

	hwnd = GetDlgItem(IDC_STARTING_DIR);
	cch = GetWindowTextLength(hwnd);
	Check(RStrAllocW(cch, &rstrValue, &pwzPtr));
	GetWindowText(hwnd, pwzPtr, cch + 1);

	if(PathRelativePathToW(wzRelative, RStrToWide(m_rstrProjectDir), FILE_ATTRIBUTE_DIRECTORY, pwzPtr, 0))
	{
		RStrRelease(rstrValue); rstrValue = NULL;
		Check(RStrCreateW(TStrLenAssert(wzRelative), wzRelative, &rstrValue));
	}

	Check(JSONCreateString(rstrValue, &srv));
	Check(m_pProject->AddValueW(L"startDir", srv));

	srv.Release();
	RStrRelease(rstrValue); rstrValue = NULL;

	hwnd = GetDlgItem(IDC_COMMAND_ARGS);
	cch = GetWindowTextLength(hwnd);
	Check(RStrAllocW(cch, &rstrValue, &pwzPtr));
	GetWindowText(hwnd, pwzPtr, cch + 1);
	Check(JSONCreateString(rstrValue, &srv));
	Check(m_pProject->AddValueW(L"args", srv));
	srv.Release();

	GetWindowText(GetDlgItem(IDC_DEBUG_PORT), wzDebugPort, ARRAYSIZE(wzDebugPort));
	Check(JSONCreateInteger(Formatting::TAscToInt32(wzDebugPort), &srv));
	Check(m_pProject->AddValueW(L"debugPort", srv));
	srv.Release();

	Check(JSONCreateBool(BST_CHECKED == (SendMessage(GetDlgItem(IDC_AUTO_START_DEBUGGER), BM_GETCHECK, 0, 0) & BST_CHECKED), &srv));
	Check(m_pProject->AddValueW(L"autoStartDebugger", srv));
	srv.Release();

	hwnd = GetDlgItem(IDC_DEBUG_HOST);
	cch = GetWindowTextLength(hwnd);
	RStrRelease(rstrValue); rstrValue = NULL;
	Check(RStrAllocW(cch, &rstrValue, &pwzPtr));
	GetWindowText(hwnd, pwzPtr, cch + 1);
	Check(JSONCreateString(rstrValue, &srv));
	Check(m_pProject->AddValueW(L"remoteHost", srv));

Cleanup:
	RStrRelease(rstrValue);
	return hr;
}

VOID CRunParamsDlg::UpdateDebugHostState (VOID)
{
	BOOL fAutoStart = BST_CHECKED ==
		(SendMessage(GetDlgItem(IDC_AUTO_START_DEBUGGER), BM_GETCHECK, 0, 0) & BST_CHECKED);
	EnableWindow(GetDlgItem(IDC_DEBUG_HOST), !fAutoStart);
}
