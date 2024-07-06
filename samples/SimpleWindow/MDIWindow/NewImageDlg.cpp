#include <windows.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Util\WinUtil.h"
#include "NewImageDlg.h"

CNewImageDlg::CNewImageDlg () :
	CBaseDialog(IDD_NEW_IMAGE)
{
	m_size.cx = 1024;
	m_size.cy = 1024;
}

CNewImageDlg::~CNewImageDlg ()
{
}

BOOL CNewImageDlg::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	switch(message)
	{
	case WM_INITDIALOG:
		WinUtil::SetWindowInt(GetDlgItem(IDC_IMAGE_WIDTH), m_size.cx);
		WinUtil::SetWindowInt(GetDlgItem(IDC_IMAGE_HEIGHT), m_size.cy);
		SendMessage(GetDlgItem(IDC_WHITE), BM_SETCHECK, BST_CHECKED, 0);
		CenterHost();
		break;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			m_size.cx = WinUtil::ReadIntFromWindow(GetDlgItem(IDC_IMAGE_WIDTH));
			if(m_size.cx <= 0)
				break;
			m_size.cy = WinUtil::ReadIntFromWindow(GetDlgItem(IDC_IMAGE_HEIGHT));
			if(m_size.cy <= 0)
				break;

			if(SendMessage(GetDlgItem(IDC_WHITE), BM_GETCHECK, 0, 0) & BST_CHECKED)
				m_cr = RGB(0xFF, 0xFF, 0xFF) | 0xFF000000;
			else if(SendMessage(GetDlgItem(IDC_BLACK), BM_GETCHECK, 0, 0) & BST_CHECKED)
				m_cr = RGB(0x00, 0x00, 0x00) | 0xFF000000;
			else if(SendMessage(GetDlgItem(IDC_TRANSPARENT), BM_GETCHECK, 0, 0) & BST_CHECKED)
				m_cr = 0;
			__fallthrough;
		case IDCANCEL:
			End(LOWORD(wParam));
			break;
		}
		break;
	}

	return FALSE;
}
