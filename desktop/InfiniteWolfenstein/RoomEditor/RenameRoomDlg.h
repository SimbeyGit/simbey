#pragma once

#include "Library\Window\BaseDialog.h"
#include "Published\JSON.h"

class CRoom;

class CRenameRoomDlg : public CBaseDialog
{
private:
	CRoom* m_pRoom;

public:
	CRenameRoomDlg (CRoom* pRoom);
	~CRenameRoomDlg ();

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

private:
	HRESULT UpdateName (VOID);
};
