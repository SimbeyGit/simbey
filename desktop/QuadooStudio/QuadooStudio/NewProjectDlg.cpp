#include <windows.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "NewProjectDlg.h"

CNewProjectDlg::CNewProjectDlg () : CBaseDialog(IDD_NEW_PROJECT)
{
}

CNewProjectDlg::~CNewProjectDlg ()
{
}

HRESULT CNewProjectDlg::Initialize (VOID)
{
	return m_browse.Initialize();
}

PCWSTR CNewProjectDlg::GetProjectType (VOID)
{
	return m_wzProjectType;
}

PCWSTR CNewProjectDlg::GetProjectFile (VOID)
{
	return m_browse.GetFile(0);
}

BOOL CNewProjectDlg::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	switch(message)
	{
	case WM_INITDIALOG:
		SendMessage(GetDlgItem(IDC_QVM), BM_SETCHECK, BST_CHECKED, 0);
		CenterHost();
		break;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			if(0 == GetWindowTextLength(GetDlgItem(IDC_PROJECT_PATH)))
			{
				SetFocus(GetDlgItem(IDC_BROWSE));
				break;
			}

			if(SendMessage(GetDlgItem(IDC_QVM), BM_GETCHECK, 0, 0) & BST_CHECKED)
				TStrCchCpy(m_wzProjectType, ARRAYSIZE(m_wzProjectType), L"qvm");
			else if(SendMessage(GetDlgItem(IDC_WQVM), BM_GETCHECK, 0, 0) & BST_CHECKED)
				TStrCchCpy(m_wzProjectType, ARRAYSIZE(m_wzProjectType), L"wqvm");
			else if(SendMessage(GetDlgItem(IDC_WEBSERVICE), BM_GETCHECK, 0, 0) & BST_CHECKED)
				TStrCchCpy(m_wzProjectType, ARRAYSIZE(m_wzProjectType), L"web");
			else
				break;
			__fallthrough;
		case IDCANCEL:
			End(LOWORD(wParam));
			break;

		case IDC_BROWSE:
			{
				HWND hwnd;
				GetWindow(&hwnd);
				if(m_browse.SaveFile(hwnd, L"Create Project File", L"Project (*.qsproj)\0*.qsproj"))
					SetWindowText(GetDlgItem(IDC_PROJECT_PATH), GetProjectFile());
			}
			break;
		}
		break;
	}

	return FALSE;
}
