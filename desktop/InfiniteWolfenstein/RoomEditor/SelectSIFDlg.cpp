#include <windows.h>
#include "resource.h"
#include "SelectSIFDlg.h"

CSelectSIFDlg::CSelectSIFDlg () :
	CBaseDialog(IDD_SELECT_SIF),
	m_pSelector(NULL)
{
}

CSelectSIFDlg::~CSelectSIFDlg ()
{
	SafeRelease(m_pSelector);
}

HRESULT CSelectSIFDlg::Initialize (VOID)
{
	HRESULT hr;

	m_pSelector = __new CSIFSelector;
	CheckAlloc(m_pSelector);
	hr = S_OK;

Cleanup:
	return hr;
}

HRESULT CSelectSIFDlg::AddSIF (RSTRING rstrTitle, ISimbeyInterchangeFile* pSIF)
{
	return m_pSelector->AddSIF(rstrTitle, pSIF);
}

HRESULT CSelectSIFDlg::GetSelection (__out RSTRING* prstrTitle, __out DWORD* pidSelection)
{
	return m_pSelector->GetSelected(prstrTitle, pidSelection);
}

BOOL CSelectSIFDlg::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	BOOL fHandled = FALSE;
	switch(message)
	{
	case WM_INITDIALOG:
		CDialogControlAdapter::Attach(GetDlgItem(IDC_ITEMS), m_pSelector);
		CenterHost();
		SetFocus(GetDlgItem(IDC_ITEMS));
		break;
	case WM_COMMAND:
		if(IDCANCEL == LOWORD(wParam))
			End(FALSE);
		else if(IDOK == LOWORD(wParam))
		{
			if(m_pSelector->HasSelection())
				End(TRUE);
		}
		break;
	}
	return fHandled;
}
