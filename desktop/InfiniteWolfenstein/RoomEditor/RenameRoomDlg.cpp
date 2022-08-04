#include <windows.h>
#include "resource.h"
#include "Rooms.h"
#include "RenameRoomDlg.h"

CRenameRoomDlg::CRenameRoomDlg (CRoom* pRoom) :
	CBaseDialog(IDD_RENAME_ROOM),
	m_pRoom(pRoom)
{
}

CRenameRoomDlg::~CRenameRoomDlg ()
{
}

BOOL CRenameRoomDlg::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	BOOL fHandled = FALSE;
	switch(message)
	{
	case WM_INITDIALOG:
		SetWindowText(GetDlgItem(IDC_CURRENT_NAME), RStrToWide(m_pRoom->m_rstrName));
		SetFocus(GetDlgItem(IDC_NEW_NAME));
		CenterHost();
		break;
	case WM_COMMAND:
		if(IDCANCEL == LOWORD(wParam))
			End(FALSE);
		else if(IDOK == LOWORD(wParam))
		{
			if(SUCCEEDED(UpdateName()))
				End(TRUE);
		}
		break;
	}
	return fHandled;
}

HRESULT CRenameRoomDlg::UpdateName (VOID)
{
	HRESULT hr;
	HWND hwndName = GetDlgItem(IDC_NEW_NAME);
	INT cchName = GetWindowTextLength(hwndName);
	PWSTR pwzWritePtr;
	RSTRING rstrName = NULL;

	CheckIf(0 == cchName, E_FAIL);
	Check(RStrAllocW(cchName, &rstrName, &pwzWritePtr));
	GetWindowText(hwndName, pwzWritePtr, cchName + 1);

	RStrReplace(&m_pRoom->m_rstrName, rstrName);

Cleanup:
	RStrRelease(rstrName);
	return hr;
}
