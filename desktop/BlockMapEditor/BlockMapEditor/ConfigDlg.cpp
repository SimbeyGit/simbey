#include <windows.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "Library\Util\Registry.h"
#include "Published\SimbeyCore.h"
#include "ConfigDlg.h"

const WCHAR c_wzTexturePath[] = L"TexturePath";
const WCHAR c_wzBehaviorPath[] = L"BehaviorPath";
const WCHAR c_wzZDBSPPath[] = L"ZDBSPPath";
const WCHAR c_wzFloorName[] = L"FloorName";
const WCHAR c_wzCeilingName[] = L"CeilingName";

CConfigDlg::CConfigDlg () : CBaseDialog(IDD_CONFIG)
{
	m_wzTexturePath[0] = L'\0';
	m_wzBehaviorPath[0] = L'\0';
	m_wzZDBSPPath[0] = L'\0';
	m_wzFloorName[0] = L'\0';
	m_wzCeilingName[0] = L'\0';
}

CConfigDlg::~CConfigDlg ()
{
}

HRESULT CConfigDlg::Load (PCWSTR pcszRegPath)
{
	HRESULT hr;
	HKEY hKey = NULL;
	DWORD dwType, cbData;

	Check(Registry::CreateKey(HKEY_CURRENT_USER, pcszRegPath, KEY_READ, &hKey));

	cbData = sizeof(m_wzTexturePath);
	if(ERROR_SUCCESS != RegQueryValueEx(hKey, c_wzTexturePath, NULL, &dwType, (PBYTE)m_wzTexturePath, &cbData))
	{
		// If we don't have a saved texture asset path, then let's try to find any PK3 file in the OUTPUT folder.
		FindAnyPK3(L"OUTPUT", m_wzTexturePath, ARRAYSIZE(m_wzTexturePath));
	}

	cbData = sizeof(m_wzBehaviorPath);
	if(ERROR_SUCCESS != RegQueryValueEx(hKey, c_wzBehaviorPath, NULL, &dwType, (PBYTE)m_wzBehaviorPath, &cbData) || 0 == cbData)
	{
		INT cchBehaviorPath;
		ScSearchPaths(L"OUTPUT", NULL, 0, 2, FALSE, m_wzBehaviorPath, ARRAYSIZE(m_wzBehaviorPath), &cchBehaviorPath);
	}

	cbData = sizeof(m_wzZDBSPPath);
	if(ERROR_SUCCESS != RegQueryValueEx(hKey, c_wzZDBSPPath, NULL, &dwType, (PBYTE)m_wzZDBSPPath, &cbData) || 0 == cbData)
	{
		INT cchZDBSPPath;
		ScSearchPaths(L"LIBZDBSP.DLL", NULL, 0, 2, TRUE, m_wzZDBSPPath, ARRAYSIZE(m_wzZDBSPPath), &cchZDBSPPath);
	}

	cbData = sizeof(m_wzFloorName);
	RegQueryValueEx(hKey, c_wzFloorName, NULL, &dwType, (PBYTE)m_wzFloorName, &cbData);

	cbData = sizeof(m_wzCeilingName);
	RegQueryValueEx(hKey, c_wzCeilingName, NULL, &dwType, (PBYTE)m_wzCeilingName, &cbData);

Cleanup:
	if(hKey)
		RegCloseKey(hKey);
	return hr;
}

HRESULT CConfigDlg::Save (PCWSTR pcszRegPath)
{
	HRESULT hr;
	HKEY hKey = NULL;
	DWORD cbData;

	Check(Registry::CreateKey(HKEY_CURRENT_USER, pcszRegPath, KEY_ALL_ACCESS, &hKey));

	cbData = (TStrLenAssert(m_wzTexturePath) + 1) * sizeof(WCHAR);
	RegSetValueExW(hKey, c_wzTexturePath, 0, REG_SZ, (PBYTE)m_wzTexturePath, cbData);

	cbData = (TStrLenAssert(m_wzBehaviorPath) + 1) * sizeof(WCHAR);
	RegSetValueExW(hKey, c_wzBehaviorPath, 0, REG_SZ, (PBYTE)m_wzBehaviorPath, cbData);

	cbData = (TStrLenAssert(m_wzZDBSPPath) + 1) * sizeof(WCHAR);
	RegSetValueExW(hKey, c_wzZDBSPPath, 0, REG_SZ, (PBYTE)m_wzZDBSPPath, cbData);

	cbData = (TStrLenAssert(m_wzFloorName) + 1) * sizeof(WCHAR);
	RegSetValueExW(hKey, c_wzFloorName, 0, REG_SZ, (PBYTE)m_wzFloorName, cbData);

	cbData = (TStrLenAssert(m_wzCeilingName) + 1) * sizeof(WCHAR);
	RegSetValueExW(hKey, c_wzCeilingName, 0, REG_SZ, (PBYTE)m_wzCeilingName, cbData);

Cleanup:
	if(hKey)
		RegCloseKey(hKey);
	return hr;
}

BOOL CConfigDlg::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	BOOL fHandled = FALSE;

	switch(message)
	{
	case WM_INITDIALOG:
		{
			HWND hDlg;

			GetWindow(&hDlg);
			SetDlgItemText(hDlg, IDC_TEXTURE_DATA, m_wzTexturePath);
			SetDlgItemText(hDlg, IDC_BEHAVIOR_PATH, m_wzBehaviorPath);
			SetDlgItemText(hDlg, IDC_ZDBSP_PATH, m_wzZDBSPPath);
			SetDlgItemText(hDlg, IDC_FLOOR_NAME, m_wzFloorName);
			SetDlgItemText(hDlg, IDC_CEILING_NAME, m_wzCeilingName);

			CenterHost();
		}
		break;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			if(FAILED(ReadFromDialog()))
				break;
			__fallthrough;
		case IDCANCEL:
			End(LOWORD(wParam));
			break;
		}
		break;
	}

	return fHandled;
}

HRESULT CConfigDlg::FindAnyPK3 (PCWSTR pcwzFolder, __out_ecount(cchMaxPK3) PWSTR pwzPK3, INT cchMaxPK3)
{
	HRESULT hr;
	INT cchFolderPath;
	WIN32_FIND_DATA FindData;
	HANDLE hFind = INVALID_HANDLE_VALUE;

	Check(ScSearchPaths(pcwzFolder, NULL, 0, 2, FALSE, pwzPK3, cchMaxPK3, &cchFolderPath));
	pwzPK3[cchFolderPath++] = L'\\';
	Check(TStrCchCpy(pwzPK3 + cchFolderPath, cchMaxPK3 - cchFolderPath, L"*.pk3"));

	hFind = FindFirstFile(pwzPK3, &FindData);
	CheckIfGetLastError(INVALID_HANDLE_VALUE == hFind);

	Check(TStrCchCpy(pwzPK3 + cchFolderPath, cchMaxPK3 - cchFolderPath, FindData.cFileName));

Cleanup:
	if(INVALID_HANDLE_VALUE != hFind)
		FindClose(hFind);
	return hr;
}

HRESULT CConfigDlg::ReadFromDialog (VOID)
{
	HRESULT hr;

	Check(ReadAndVerifyPath(IDC_TEXTURE_DATA, m_wzTexturePath));
	Check(ReadAndVerifyPath(IDC_BEHAVIOR_PATH, m_wzBehaviorPath));
	Check(ReadAndVerifyPath(IDC_ZDBSP_PATH, m_wzZDBSPPath));

	if(0 == GetWindowText(GetDlgItem(IDC_FLOOR_NAME), m_wzFloorName, ARRAYSIZE(m_wzFloorName)))
		Check(TStrCchCpy(m_wzFloorName, ARRAYSIZE(m_wzFloorName), L"FLAT5_4"));
	if(0 == GetWindowText(GetDlgItem(IDC_CEILING_NAME), m_wzCeilingName, ARRAYSIZE(m_wzCeilingName)))
		Check(TStrCchCpy(m_wzCeilingName, ARRAYSIZE(m_wzCeilingName), L"CEIL3_5"));

Cleanup:
	return hr;
}

HRESULT CConfigDlg::ReadAndVerifyPath (INT idControl, PWSTR pwzPath)
{
	HRESULT hr;
	HWND hwndControl = GetDlgItem(idControl);

	CheckIfGetLastError(0 == GetWindowText(hwndControl, pwzPath, MAX_PATH));
	CheckIf(GetFileAttributes(pwzPath) == INVALID_FILE_ATTRIBUTES, HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
	hr = S_OK;

Cleanup:
	if(FAILED(hr) && hwndControl)
		SetFocus(hwndControl);
	return hr;
}
