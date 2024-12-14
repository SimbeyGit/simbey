#include <windows.h>
#include "SelectSIFDlg.h"

CSelectSIFDlg::CSelectSIFDlg (UINT idDialog, UINT idItems) :
	CBaseDialog(idDialog),
	m_pSelector(NULL),
	m_idItems(idItems)
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

VOID CSelectSIFDlg::SetSelection (RSTRING rstrTitle, DWORD idLayer)
{
	m_pSelector->DeferSelection(rstrTitle, idLayer);
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
		CDialogControlAdapter::Attach(GetDlgItem(m_idItems), m_pSelector);
		CenterHost();
		SetFocus(GetDlgItem(m_idItems));
		break;
	case WM_COMMAND:
		if(IDCANCEL == LOWORD(wParam))
			End(FALSE);
		else if(IDOK == LOWORD(wParam))
		{
			if(m_pSelector->HasSelection())
			{
				m_pSelector->LoadSelection();
				End(TRUE);
			}
		}
		break;
	}
	return fHandled;
}
