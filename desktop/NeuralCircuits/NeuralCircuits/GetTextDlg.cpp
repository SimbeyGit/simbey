#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "Library\Util\WinUtil.h"
#include "resource.h"
#include "GetTextDlg.h"

CGetTextDlg::CGetTextDlg () : CBaseDialog(IDD_GETTEXT)
{
	m_lpszText = NULL;
}

CGetTextDlg::~CGetTextDlg ()
{
	__delete_array m_lpszText;
}

HRESULT CGetTextDlg::SetText (LPCSTR lpcszText)
{
	return TReplaceStringChecked(lpcszText, &m_lpszText);
}

LPCSTR CGetTextDlg::GetText (VOID)
{
	return m_lpszText;
}

BOOL CGetTextDlg::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	UNREFERENCED_PARAMETER(lParam);

	BOOL fHandled = FALSE;
	HWND hwndText;

	switch(message)
	{
	case WM_INITDIALOG:
		CenterHost();

		hwndText = GetDlgItem(IDC_TEXT);
		Assert(hwndText);
		SetWindowText(hwndText,m_lpszText);

		lResult = TRUE;
		fHandled = TRUE;
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			hwndText = GetDlgItem(IDC_TEXT);
			Assert(hwndText);

			__delete_array m_lpszText;
			if(FAILED(WinUtil::GetWindowText(hwndText,&m_lpszText,NULL)))
				break;
			// Fall through
		case IDCANCEL:
			End(LOWORD(wParam));
			fHandled = TRUE;
			break;
		}
		break;
	}

	return fHandled;
}