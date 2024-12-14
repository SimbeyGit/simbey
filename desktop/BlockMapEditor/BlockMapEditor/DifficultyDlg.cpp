#include <windows.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "DifficultyDlg.h"

CDifficultyDlg::CDifficultyDlg (SHORT sObjectFlags) :
	CBaseDialog(IDD_DIFFICULTY),
	m_sObjectFlags(sObjectFlags)
{
}

CDifficultyDlg::~CDifficultyDlg ()
{
}

SHORT CDifficultyDlg::GetObjectFlags (VOID)
{
	return m_sObjectFlags;
}

BOOL CDifficultyDlg::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	BOOL fHandled = FALSE;

	switch(message)
	{
	case WM_INITDIALOG:
		if(m_sObjectFlags & 1)
			SendMessage(GetDlgItem(IDC_DIFFICULTY_EASY), BM_SETCHECK, BST_CHECKED, 0);
		if(m_sObjectFlags & 2)
			SendMessage(GetDlgItem(IDC_DIFFICULTY_MEDIUM), BM_SETCHECK, BST_CHECKED, 0);
		if(m_sObjectFlags & 4)
			SendMessage(GetDlgItem(IDC_DIFFICULTY_HARD), BM_SETCHECK, BST_CHECKED, 0);
		if(m_sObjectFlags & 8)
			SendMessage(GetDlgItem(IDC_AMBUSH), BM_SETCHECK, BST_CHECKED, 0);
		CenterHost();
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			if(SendMessage(GetDlgItem(IDC_DIFFICULTY_EASY), BM_GETCHECK, 0, 0) & BST_CHECKED)
				m_sObjectFlags |= 1;
			else
				m_sObjectFlags &= ~1;
			if(SendMessage(GetDlgItem(IDC_DIFFICULTY_MEDIUM), BM_GETCHECK, 0, 0) & BST_CHECKED)
				m_sObjectFlags |= 2;
			else
				m_sObjectFlags &= ~2;
			if(SendMessage(GetDlgItem(IDC_DIFFICULTY_HARD), BM_GETCHECK, 0, 0) & BST_CHECKED)
				m_sObjectFlags |= 4;
			else
				m_sObjectFlags &= ~4;
			if(SendMessage(GetDlgItem(IDC_AMBUSH), BM_GETCHECK, 0, 0) & BST_CHECKED)
				m_sObjectFlags |= 8;
			else
				m_sObjectFlags &= ~8;
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
