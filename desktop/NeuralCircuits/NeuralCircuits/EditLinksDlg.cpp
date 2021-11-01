#include <windows.h>
#include "resource.h"
#include "NeuralAPI.h"
#include "NeuralLinks.h"
#include "EditLinksDlg.h"

CEditLinksDlg::CEditLinksDlg (INeuralLinks* lpLinks, LPCSTR lpcszSelectedLink) : CBaseDialog(IDD_EDIT_LINKS)
{
	m_lpLinks = lpLinks;
	m_lpcszSelectedLink = lpcszSelectedLink;
	m_fMadeChanges = FALSE;
}

CEditLinksDlg::~CEditLinksDlg ()
{
}

VOID CEditLinksDlg::DlgCommand (USHORT id, USHORT iCode)
{
	UNREFERENCED_PARAMETER(iCode);

	INT nSel;
	CHAR szName[256];

	switch(id)
	{
	case IDC_EDIT_LINK:
		if(GetSelectedLink(szName, ARRAYSIZE(szName)))
		{
			HWND hwnd;
			if(SUCCEEDED(GetWindow(&hwnd)))
				m_lpLinks->EditLink(hwnd, szName);
		}
		break;
	case IDC_DELETE_LINK:
		if(GetSelectedLink(szName, ARRAYSIZE(szName)))
		{
			INeuralLink* lpLink;
			if(SUCCEEDED(m_lpLinks->GetNeuralLink(szName, &lpLink)))
			{
				if(SUCCEEDED(lpLink->Remove()))
				{
					RefreshLinks();
					m_fMadeChanges = TRUE;
				}
				lpLink->Release();
			}
		}
		break;
	case IDC_CREATE_LINK:
		nSel = SendMessage(m_hSources, LB_GETCURSEL, 0, 0);
		if(0 <= nSel)
		{
			CHAR szLinkName[256];
			if(0 < GetWindowText(m_hName, szLinkName, ARRAYSIZE(szLinkName)))
			{
				SendMessage(m_hSources, LB_GETTEXT, nSel, (LPARAM)szName);
				if(SUCCEEDED(m_lpLinks->CreateLink(m_hSources, szName, szLinkName)))
				{
					SetWindowText(m_hName, NULL);
					RefreshLinks();
					m_fMadeChanges = TRUE;
				}
			}
		}
		break;
	case IDCANCEL:
	case IDOK:
		if(IDOK == id)
		{
			if(GetFocus() == m_hName)
			{
				if(0 == GetWindowTextLength(m_hName))
					SetFocus(GetDlgItem(IDOK));
				else
					DlgCommand(IDC_CREATE_LINK, iCode);
			}
			else if(GetFocus() == m_hSources)
			{
				if(0 == GetWindowTextLength(m_hName))
					SetFocus(m_hName);
				else
					DlgCommand(IDC_CREATE_LINK, iCode);
			}
			else
				End(m_fMadeChanges);
		}
		else
			End(m_fMadeChanges);
		break;
	}
}

BOOL CEditLinksDlg::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	UNREFERENCED_PARAMETER(lParam);

	BOOL fHandled = FALSE;
	switch(message)
	{
	case WM_INITDIALOG:
		CenterHost();

		m_hLinks = GetDlgItem(IDC_LINKS);
		m_hSources = GetDlgItem(IDC_SOURCES);
		m_hName = GetDlgItem(IDC_NAME);

		m_lpLinks->FillLinksList(m_hLinks, m_lpcszSelectedLink, FALSE);
		m_lpLinks->FillSources(m_hSources);

		fHandled = TRUE;
		lResult = TRUE;
		break;
	case WM_COMMAND:
		DlgCommand(LOWORD(wParam),HIWORD(wParam));
		break;
	}
	return fHandled;
}

VOID CEditLinksDlg::RefreshLinks (VOID)
{
	SendMessage(m_hLinks, LB_RESETCONTENT, 0, 0);
	m_lpLinks->FillLinksList(m_hLinks, m_lpcszSelectedLink, FALSE);
}

BOOL CEditLinksDlg::GetSelectedLink (__out_ecount(cchMaxLink) LPSTR lpszLink, INT cchMaxLink)
{
	UNREFERENCED_PARAMETER(cchMaxLink);

	BOOL fHasLink = FALSE;
	INT nSel = SendMessage(m_hLinks, LB_GETCURSEL, 0, 0);
	if(0 <= nSel)
	{
		SendMessage(m_hLinks, LB_GETTEXT, nSel, (LPARAM)lpszLink);
		fHasLink = TRUE;
	}
	return fHasLink;
}