#include <windows.h>
#include "resource.h"
#include "Library\Util\Formatting.h"
#include "NeuralAPI.h"
#include "InputPad.h"
#include "InputPadProps.h"

CInputPadProps::CInputPadProps (CInputPad* lpInputPad) : CBaseDialog(IDD_INPUTPADPROPS)
{
	m_lpInputPad = lpInputPad;
	m_lpInputPad->AddRef();
}

CInputPadProps::~CInputPadProps ()
{
	m_lpInputPad->Release();
}

VOID CInputPadProps::DlgCreate (VOID)
{
	FLOAT fSetCell, fClearCell;
	CHAR szValue[32];

	m_lpInputPad->GetCellValues(fSetCell,fClearCell);

	m_hwndSetValue = GetDlgItem(IDC_SET_VALUE);
	m_hwndClearValue = GetDlgItem(IDC_CLEAR_VALUE);

	Formatting::TPrintF(szValue, ARRAYSIZE(szValue), NULL, "%f", fSetCell);
	SetWindowText(m_hwndSetValue,szValue);

	Formatting::TPrintF(szValue, ARRAYSIZE(szValue), NULL, "%f", fClearCell);
	SetWindowText(m_hwndClearValue,szValue);

	CenterHost();
}

VOID CInputPadProps::DlgCommand (USHORT id, USHORT iCode)
{
	UNREFERENCED_PARAMETER(iCode);

	FLOAT fSetCell, fClearCell;
	CHAR szValue[32];

	switch(id)
	{
	case IDOK:
		if(GetWindowText(m_hwndSetValue,szValue,sizeof(szValue)) < sizeof(szValue))
		{
			fSetCell = (FLOAT)atof(szValue);
			if(GetWindowText(m_hwndClearValue,szValue,sizeof(szValue)) < sizeof(szValue))
			{
				fClearCell = (FLOAT)atof(szValue);
				m_lpInputPad->SetCellValues(fSetCell,fClearCell);
				End(TRUE);
			}
		}
		break;

	case IDCANCEL:
		End(FALSE);
		break;
	}
}

BOOL CInputPadProps::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
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