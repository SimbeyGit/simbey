#include <windows.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Util\Formatting.h"
#include "NewWorldDlg.h"

CNewWorldDlg::CNewWorldDlg (INT nWidth, INT nHeight) :
	CBaseDialog(IDD_NEW_WORLD),
	m_nWidth(nWidth),
	m_nHeight(nHeight)
{
}

BOOL CNewWorldDlg::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	BOOL fHandled = FALSE;
	WCHAR wzValue[32];

	switch(message)
	{
	case WM_INITDIALOG:
		Formatting::TInt32ToAsc(m_nWidth, wzValue, ARRAYSIZE(wzValue), 10, NULL);
		SetWindowText(GetDlgItem(IDC_WIDTH), wzValue);

		Formatting::TInt32ToAsc(m_nHeight, wzValue, ARRAYSIZE(wzValue), 10, NULL);
		SetWindowText(GetDlgItem(IDC_HEIGHT), wzValue);

		SetFocus(GetDlgItem(IDC_WIDTH));
		CenterHost();
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			{
				INT nWidth, nHeight;

				if(0 == GetWindowText(GetDlgItem(IDC_WIDTH), wzValue, ARRAYSIZE(wzValue)))
					break;
				nWidth = Formatting::TAscToInt32(wzValue);
				if(32 > nWidth || nWidth > 1024)
					break;

				if(0 == GetWindowText(GetDlgItem(IDC_HEIGHT), wzValue, ARRAYSIZE(wzValue)))
					break;
				nHeight = Formatting::TAscToInt32(wzValue);
				if(24 > nHeight || nHeight > 768)
					break;

				m_nWidth = nWidth;
				m_nHeight = nHeight;
			}
			__fallthrough;
		case IDCANCEL:
			End(LOWORD(wParam));
			fHandled = TRUE;
			break;
		}
		break;
	}

	return fHandled;
}
