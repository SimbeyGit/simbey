#include <windows.h>
#include "resource.h"
#include "Library\Util\Formatting.h"
#include "Library\Util\WinUtil.h"
#include "Library\Window\DialogHost.h"
#include "NeuralAPI.h"
#include "Neurone.h"
#include "NeuralLinks.h"
#include "EditLinksDlg.h"
#include "NeuronePropsDlg.h"

CNeuronePropsDlg::CNeuronePropsDlg (INeuralLinks* lpLinks, INeurone* lpNeurone) : CBaseDialog(IDD_PROPERTIES)
{
	m_lpLinks = lpLinks;
	m_lpNeurone = lpNeurone;
	m_lpNeurone->AddRef();

	m_nDefaultPin = 0;
}

CNeuronePropsDlg::~CNeuronePropsDlg ()
{
	m_lpNeurone->Release();
}

VOID CNeuronePropsDlg::EditLinks (VOID)
{
	IIONeurone* lpIO;
	if(SUCCEEDED(m_lpNeurone->QueryInterface(&lpIO)))
	{
		CHAR szTemp[256];
		if(SUCCEEDED(lpIO->GetLinkName(szTemp, ARRAYSIZE(szTemp))))
		{
			HWND hwnd;
			CEditLinksDlg Edit(m_lpLinks, szTemp);
			CDialogHost Dialog(GetInstance());
			GetWindow(&hwnd);
			if(SUCCEEDED(Dialog.Display(hwnd, &Edit)) && Dialog.GetReturnValue())
				FillLinks();
		}
		lpIO->Release();
	}
}

VOID CNeuronePropsDlg::FillLinks (VOID)
{
	IIONeurone* lpIO;
	if(SUCCEEDED(m_lpNeurone->QueryInterface(&lpIO)))
	{
		CHAR szTemp[256];
		if(SUCCEEDED(lpIO->GetLinkName(szTemp, ARRAYSIZE(szTemp))))
		{
			SendMessage(m_hLink, CB_RESETCONTENT, 0, 0);
			m_lpLinks->FillLinksList(m_hLink, szTemp, TRUE);
		}
		lpIO->Release();
	}
}

VOID CNeuronePropsDlg::Save (VOID)
{
	IIONeurone* lpIO;

	m_lpNeurone->SetThreshold((FLOAT)WinUtil::ReadDoubleFromWindow(m_hThreshold));

	if(SUCCEEDED(m_lpNeurone->QueryInterface(&lpIO)))
	{
		CHAR szTemp[256];
		INT nSel;

		GetWindowText(m_hPin, szTemp, ARRAYSIZE(szTemp));
		lpIO->SetPin(Formatting::TAscToUInt32(szTemp));

		nSel = (INT)SendMessage(m_hLink, CB_GETCURSEL, 0, 0);
		if(CB_ERR != SendMessage(m_hLink, CB_GETLBTEXT, nSel, (LPARAM)szTemp))
		{
			if(0 == lstrcmp(DEFAULT_SOURCE_IO_NAME, szTemp))
				lpIO->SetLinkName(m_lpLinks, NULL);
			else
				lpIO->SetLinkName(m_lpLinks, szTemp);
		}

		lpIO->Release();
	}
	else
	{
		TStackRef<IBiasNeurone> srBiasNeurone;

		if(SUCCEEDED(m_lpNeurone->QueryInterface(IID_PPV_ARGS(&srBiasNeurone))))
			srBiasNeurone->SetBias((FLOAT)WinUtil::ReadDoubleFromWindow(m_hBias));
	}
}

VOID CNeuronePropsDlg::DlgCreate (VOID)
{
	IIONeurone* lpIO;

	CenterHost();

	m_hThreshold = GetDlgItem(IDC_THRESHOLD);
	m_hPin = GetDlgItem(IDC_PIN);
	m_hLink = GetDlgItem(IDC_LINK);
	m_hEditLinks = GetDlgItem(IDC_EDIT_LINKS);
	m_hBias = GetDlgItem(IDC_BIAS);

	WinUtil::SetWindowDouble(m_hThreshold,m_lpNeurone->GetThreshold());

	if(SUCCEEDED(m_lpNeurone->QueryInterface(&lpIO)))
	{
		CHAR szTemp[32];
		m_nDefaultPin = lpIO->GetPin();
		Formatting::TInt32ToAsc(m_nDefaultPin, szTemp, ARRAYSIZE(szTemp), 10, NULL);
		SetWindowText(m_hPin, szTemp);

		EIO_TYPE eType = lpIO->GetIOType();
		EnableWindow(m_hPin, INPUT_LINK == eType || OUTPUT_LINK == eType);

		FillLinks();

		EnableWindow(m_hLink, !lpIO->HasParentChip());
		EnableWindow(m_hEditLinks, !lpIO->HasParentChip());
		EnableWindow(m_hBias, FALSE);

		lpIO->Release();
	}
	else
	{
		TStackRef<IBiasNeurone> srBiasNeurone;

		EnableWindow(m_hPin, FALSE);
		EnableWindow(m_hLink, FALSE);
		EnableWindow(m_hEditLinks, FALSE);

		if(SUCCEEDED(m_lpNeurone->QueryInterface(IID_PPV_ARGS(&srBiasNeurone))))
			WinUtil::SetWindowDouble(m_hBias, srBiasNeurone->GetBias());
		else
			EnableWindow(m_hBias, FALSE);
	}
}

VOID CNeuronePropsDlg::DlgCommand (USHORT id, USHORT iCode)
{
	UNREFERENCED_PARAMETER(iCode);

	switch(id)
	{
	case IDOK:
		Save();
		End(TRUE);
		break;

	case IDCANCEL:
		End(FALSE);
		break;

	case IDC_LINK:
		if(iCode == CBN_SELCHANGE)
		{
			CHAR szName[256];
			INT nSel = (INT)SendMessage(m_hLink, CB_GETCURSEL, 0, 0);
			if(CB_ERR != SendMessage(m_hLink, CB_GETLBTEXT, nSel, (LPARAM)szName))
			{
				BOOL fCanEditPin = TRUE;

				if(0 == lstrcmp(DEFAULT_SOURCE_IO_NAME, szName))
				{
					CHAR szTemp[16];
					Formatting::TInt32ToAsc(m_nDefaultPin, szTemp, ARRAYSIZE(szTemp), 10, NULL);
					SetWindowText(m_hPin, szTemp);
					fCanEditPin = FALSE;
				}

				EnableWindow(m_hPin, fCanEditPin);
			}
		}
		break;

	case IDC_EDIT_LINKS:
		EditLinks();
		break;
	}
}

BOOL CNeuronePropsDlg::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	UNREFERENCED_PARAMETER(lParam);

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
	}

	return fHandled;
}