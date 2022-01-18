#include <windows.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "AboutDialog.h"

CAboutDialog::CAboutDialog () :
	CBaseDialog(IDD_ABOUT)
{
}

CAboutDialog::~CAboutDialog ()
{
}

BOOL CAboutDialog::OnInitDialog (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	CenterHost();
	return FALSE;
}

BOOL CAboutDialog::OnCommand (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	switch(LOWORD(wParam))
	{
	case IDOK:
		End(0);
		break;
	}
	return FALSE;
}
