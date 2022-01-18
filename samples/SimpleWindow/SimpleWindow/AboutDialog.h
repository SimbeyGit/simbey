#pragma once

#include "Library\Window\BaseWindow.h"
#include "Library\Window\BaseDialog.h"

class CAboutDialog :
	public CBaseDialog
{
public:
	CAboutDialog ();
	~CAboutDialog ();

	BEGIN_WM_MAP
		HANDLE_WM(WM_INITDIALOG, OnInitDialog)
		HANDLE_WM(WM_COMMAND, OnCommand)
	END_WM_MAP

private:
	BOOL OnInitDialog (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult);
	BOOL OnCommand (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult);
};
