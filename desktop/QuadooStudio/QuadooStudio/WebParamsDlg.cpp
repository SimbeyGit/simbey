#include <windows.h>
#include <shlwapi.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "Library\Util\RString.h"
#include "Library\Util\Formatting.h"
#include "Published\JSON.h"
#include "WebParamsDlg.h"

CWebParamsDlg::CWebParamsDlg (IJSONObject* pProject, RSTRING rstrEngine, RSTRING rstrProjectDir) :
	CBaseDialog(IDD_WEB_PARAMS)
{
	SetInterface(m_pProject, pProject);
	RStrSet(m_rstrEngine, rstrEngine);
	RStrSet(m_rstrProjectDir, rstrProjectDir);
}

CWebParamsDlg::~CWebParamsDlg ()
{
	RStrRelease(m_rstrProjectDir);
	RStrRelease(m_rstrEngine);
	SafeRelease(m_pProject);
}

BOOL CWebParamsDlg::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	switch(message)
	{
	case WM_INITDIALOG:
		SetWindowText(GetDlgItem(IDC_INSTALLED_ENGINE), RStrToWide(m_rstrEngine));
		ReadServerAndPath();
		SetFocus(GetDlgItem(IDC_HOST_AND_SERVER));
		CenterHost();
		break;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			if(FAILED(WriteServerAndPath()))
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

HRESULT CWebParamsDlg::ReadServerAndPath (VOID)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	RSTRING rstrValue = NULL;

	if(SUCCEEDED(m_pProject->FindNonNullValueW(L"webHost", &srv)))
	{
		Check(srv->GetString(&rstrValue));
		SetWindowText(GetDlgItem(IDC_HOST_AND_SERVER), RStrToWide(rstrValue));

		RStrRelease(rstrValue); rstrValue = NULL;
		srv.Release();
	}

	if(0 == GetWindowTextLength(GetDlgItem(IDC_HOST_AND_SERVER)))
		SetWindowText(GetDlgItem(IDC_HOST_AND_SERVER), L"http://localhost/");

	if(SUCCEEDED(m_pProject->FindNonNullValueW(L"copyPath", &srv)))
	{
		PWSTR pwzAbsolute;
		INT cchAbsolute;

		Check(srv->GetString(&rstrValue));

		Check(Formatting::TBuildDirectory(RStrToWide(m_rstrProjectDir), RStrLen(m_rstrProjectDir), RStrToWide(rstrValue), RStrLen(rstrValue), &pwzAbsolute, &cchAbsolute));
		SetWindowText(GetDlgItem(IDC_COPY_TO_PATH), pwzAbsolute);

		__delete_array pwzAbsolute;
		srv.Release();
	}

	if(SUCCEEDED(m_pProject->FindNonNullValueW(L"copyWebPath", &srv)))
	{
		bool fCopyPath;

		Check(srv->GetBoolean(&fCopyPath));
		if(fCopyPath)
			SendMessage(GetDlgItem(IDC_COPY_PATH), BM_SETCHECK, BST_CHECKED, 0);
	}

	hr = S_OK;

Cleanup:
	RStrRelease(rstrValue);
	return hr;
}

HRESULT CWebParamsDlg::WriteServerAndPath (VOID)
{
	HRESULT hr;
	HWND hwnd;
	INT cch;
	PWSTR pwzPtr;
	RSTRING rstrValue = NULL;
	WCHAR wzRelative[MAX_PATH];
	TStackRef<IJSONValue> srv;
	bool fCopyPath = SendMessage(GetDlgItem(IDC_COPY_PATH), BM_GETCHECK, 0, 0) == BST_CHECKED;

	hwnd = GetDlgItem(IDC_COPY_TO_PATH);
	cch = GetWindowTextLength(hwnd);
	Check(RStrAllocW(cch, &rstrValue, &pwzPtr));
	GetWindowText(hwnd, pwzPtr, cch + 1);

	CheckIfGetLastError(fCopyPath && GetFileAttributes(pwzPtr) == INVALID_FILE_ATTRIBUTES);

	if(PathRelativePathToW(wzRelative, RStrToWide(m_rstrProjectDir), FILE_ATTRIBUTE_DIRECTORY, pwzPtr, 0))
	{
		RStrRelease(rstrValue); rstrValue = NULL;
		Check(RStrCreateW(TStrLenAssert(wzRelative), wzRelative, &rstrValue));
	}

	Check(JSONCreateString(rstrValue, &srv));
	Check(m_pProject->AddValueW(L"copyPath", srv));

	srv.Release();
	RStrRelease(rstrValue); rstrValue = NULL;

	hwnd = GetDlgItem(IDC_HOST_AND_SERVER);
	cch = GetWindowTextLength(hwnd);
	Check(RStrAllocW(cch, &rstrValue, &pwzPtr));
	GetWindowText(hwnd, pwzPtr, cch + 1);
	Check(JSONCreateString(rstrValue, &srv));
	Check(m_pProject->AddValueW(L"webHost", srv));

	if(fCopyPath)
	{
		srv.Release();
		Check(JSONCreateBool(true, &srv));
		Check(m_pProject->AddValueW(L"copyWebPath", srv));
	}
	else
		m_pProject->RemoveValueW(L"copyWebPath");

Cleanup:
	RStrRelease(rstrValue);
	return hr;
}
