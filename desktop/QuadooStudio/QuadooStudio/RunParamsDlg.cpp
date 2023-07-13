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

Cleanup:
	RStrRelease(rstrValue);
	return hr;
}
