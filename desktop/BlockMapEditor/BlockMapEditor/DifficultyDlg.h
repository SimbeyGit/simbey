#pragma once

#include "Library\Core\BaseUnknown.h"
#include "Library\Window\BaseDialog.h"

class CDifficultyDlg : public CBaseDialog
{
private:
	SHORT m_sObjectFlags;

public:
	CDifficultyDlg (SHORT sObjectFlags);
	~CDifficultyDlg ();

	SHORT GetObjectFlags (VOID);

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);
};
