#pragma once

#include "Library\Window\BaseDialog.h"
#include "Library\ChooseFile.h"

class CNewProjectDlg :
	public CBaseDialog
{
private:
	CChooseFile m_browse;
	WCHAR m_wzProjectType[16];

public:
	CNewProjectDlg ();
	~CNewProjectDlg ();

	HRESULT Initialize (VOID);

	PCWSTR GetProjectType (VOID);
	PCWSTR GetProjectFile (VOID);

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);
};
