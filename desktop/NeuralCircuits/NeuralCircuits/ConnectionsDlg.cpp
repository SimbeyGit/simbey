#define	_CRT_SECURE_NO_DEPRECATE

#include <windows.h>
#include <commctrl.h>
#include "resource.h"
#include "Library\Util\Formatting.h"
#include "Library\Util\WinUtil.h"
#include "NeuralAPI.h"
#include "Neurone.h"
#include "ConnectionsDlg.h"

CConnectionsDlg::CConnectionsDlg (HWND hwndParent, INetDocObject* lpObject, ULONG iPin) : CBaseDialog(IDD_CONN_LIST)
{
	m_hwndParent = hwndParent;
	m_lpObject = lpObject;
	m_lpObject->AddRef();
	m_iPin = iPin;

	m_bModified = FALSE;
}

CConnectionsDlg::~CConnectionsDlg ()
{
	m_lpObject->Release();
}

VOID CConnectionsDlg::DlgCreate (VOID)
{
	LVCOLUMN lvc;

	CenterHost();

	m_hList = GetDlgItem(IDC_LIST);

	WinUtil::SetListViewStyleEx(m_hList,LVS_EX_FULLROWSELECT);

	lvc.mask = LVCF_TEXT | LVCF_WIDTH;
	lvc.pszText = "Source";
	lvc.cchTextMax = 6;
	lvc.cx = 120;
	lvc.iSubItem = 0;
	SendMessage(m_hList,LVM_INSERTCOLUMN,0,(LPARAM)&lvc);
	lvc.mask |= LVCF_SUBITEM;
	lvc.pszText = "Target";
	lvc.cchTextMax = 6;
	lvc.cx = 120;
	lvc.iSubItem = 1;
	SendMessage(m_hList,LVM_INSERTCOLUMN,1,(LPARAM)&lvc);
	lvc.pszText = "Pin";
	lvc.cchTextMax = 3;
	lvc.cx = 80;
	SendMessage(m_hList,LVM_INSERTCOLUMN,2,(LPARAM)&lvc);
	lvc.pszText = "Multiplier";
	lvc.cchTextMax = 10;
	lvc.cx = 80;
	SendMessage(m_hList,LVM_INSERTCOLUMN,3,(LPARAM)&lvc);

	m_hWeight = GetDlgItem(IDC_WEIGHT);

	Refresh();
}

VOID CConnectionsDlg::DlgCommand (USHORT id, USHORT iCode)
{
	UNREFERENCED_PARAMETER(iCode);

	INeurone* lpNeurone = NULL;
	INT i, c;

	switch(id)
	{
	case IDC_SETWEIGHT:
		UpdateWeights();
		break;
	case IDC_REMOVE:
		if(SUCCEEDED(m_lpObject->QueryInterface(&lpNeurone)))
		{
			c = SendMessage(m_hList,LVM_GETITEMCOUNT,0,0);
			for(i = 0; i < c; i++)
			{
				if(ListView_GetItemState(m_hList,i,LVIS_SELECTED))
				{
					SendMessage(m_hList,LVM_DELETEITEM,i,0);
					lpNeurone->ClearConnection(m_iPin,i--);
					c--;
					m_bModified = TRUE;
				}
			}
			InvalidateRect(m_hwndParent,NULL,FALSE);
			lpNeurone->Release();
		}
		break;
	case IDOK:
		if(GetFocus() == m_hWeight && UpdateWeights())
			break;
		__fallthrough;
	case IDCANCEL:
		m_lpObject->HighlightConn(m_iPin,-1);
		::InvalidateRect(m_hwndParent,NULL,FALSE);
		End(m_bModified);
		break;
	}
}

VOID CConnectionsDlg::DlgNotify (WPARAM id, NMHDR* lpnm)
{
	UNREFERENCED_PARAMETER(id);

	if(lpnm->code == NM_CLICK || lpnm->code == LVN_ITEMACTIVATE)
	{
		LPNMITEMACTIVATE lpClick = (LPNMITEMACTIVATE)lpnm;
		m_lpObject->HighlightConn(m_iPin,lpClick->iItem);
		::InvalidateRect(m_hwndParent,NULL,FALSE);
	}
	else if(lpnm->code == LVN_ITEMCHANGED)
	{
		LPNMLISTVIEW lpItem = (LPNMLISTVIEW)lpnm;
		m_lpObject->HighlightConn(m_iPin,lpItem->iItem);
		::InvalidateRect(m_hwndParent,NULL,FALSE);
	}
}

BOOL CConnectionsDlg::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	BOOL fHandled = FALSE;

	switch(message)
	{
	case WM_INITDIALOG:
		DlgCreate();
		fHandled = TRUE;
		lResult = TRUE;
		break;
	case WM_COMMAND:
		DlgCommand(LOWORD(wParam),HIWORD(wParam));
		break;
	case WM_NOTIFY:
		DlgNotify(wParam, (NMHDR*)lParam);
		break;
	}

	return fHandled;
}

BOOL CConnectionsDlg::UpdateWeights (VOID)
{
	BOOL fUpdated = FALSE;

	if(GetWindowTextLength(m_hWeight) > 0)
	{
		INeurone* lpNeurone = NULL;

		if(SUCCEEDED(m_lpObject->QueryInterface(&lpNeurone)))
		{
			FLOAT fWeight = (FLOAT)WinUtil::ReadDoubleFromWindow(m_hWeight);
			INT c = SendMessage(m_hList,LVM_GETITEMCOUNT,0,0);
			for(INT i = 0; i < c; i++)
			{
				if(ListView_GetItemState(m_hList,i,LVIS_SELECTED))
				{
					if(lpNeurone->SetConnectionWeight(m_iPin,i,fWeight))
					{
						m_bModified = TRUE;
						fUpdated = TRUE;
					}
				}
			}
			lpNeurone->Release();
		}
		Refresh();
	}

	return fUpdated;
}

VOID CConnectionsDlg::Refresh (VOID)
{
	INeurone* lpNeurone;

	SendMessage(m_hList,LVM_DELETEALLITEMS,0,0);

	if(SUCCEEDED(m_lpObject->QueryInterface(&lpNeurone)))
	{
		lpNeurone->EnumConnections(m_iPin,ConnectionCallback,this);
		lpNeurone->Release();
	}
}

VOID WINAPI CConnectionsDlg::ConnectionCallback (INT i, INeurone* lpSource, INeurone* lpTarget, ULONG iPin, FLOAT fWeight, LPVOID lpParam)
{
	CConnectionsDlg* lpDlg = (CConnectionsDlg*)lpParam;
	CHAR szTemp[128];
	LVITEM lvi;

	lvi.mask = LVIF_TEXT;
	lpSource->GetObjectClass(szTemp, ARRAYSIZE(szTemp), &lvi.cchTextMax);
	lvi.pszText = szTemp;
	lvi.iItem = i;
	lvi.iSubItem = 0;
	SendMessage(lpDlg->m_hList,LVM_INSERTITEM,0,(LPARAM)&lvi);

	lpTarget->GetObjectClass(szTemp, ARRAYSIZE(szTemp), &lvi.cchTextMax);
	lvi.pszText = szTemp;
	lvi.iItem = i;
	lvi.iSubItem = 1;
	SendMessage(lpDlg->m_hList,LVM_SETITEM,0,(LPARAM)&lvi);

	Formatting::TUInt32ToAsc(iPin, szTemp, ARRAYSIZE(szTemp), 10, &lvi.cchTextMax);
	lvi.pszText = szTemp;
	lvi.iItem = i;
	lvi.iSubItem = 2;
	SendMessage(lpDlg->m_hList,LVM_SETITEM,0,(LPARAM)&lvi);

	Formatting::TDoubleToString((DOUBLE)fWeight, szTemp, ARRAYSIZE(szTemp), 4, &lvi.cchTextMax);
	lvi.pszText = szTemp;
	lvi.iItem = i;
	lvi.iSubItem = 3;
	SendMessage(lpDlg->m_hList,LVM_SETITEM,0,(LPARAM)&lvi);
}
