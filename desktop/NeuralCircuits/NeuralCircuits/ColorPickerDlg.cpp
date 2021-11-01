#include <windows.h>
#include "Library\Core\Assert.h"
#include "Library\Util\Formatting.h"
#include "Library\VistaColors.h"
#include "resource.h"
#include "ColorPickerDlg.h"

COLORREF CColorPickerDlg::m_crCustomColors[16];

CColorPickerDlg::CColorPickerDlg () : CBaseDialog(IDD_COLOR_PICKER)
{
	m_crSolid = 0;
	m_bAlpha = 0;
}

CColorPickerDlg::~CColorPickerDlg ()
{
}

VOID CColorPickerDlg::SetSolidColor (COLORREF crSolid)
{
	m_crSolid = crSolid;
}

VOID CColorPickerDlg::SetAlphaChannel (BYTE bAlpha)
{
	m_bAlpha = bAlpha;
}

COLORREF CColorPickerDlg::GetSolidColor (VOID)
{
	return m_crSolid;
}

BYTE CColorPickerDlg::GetAlphaChannel (VOID)
{
	return m_bAlpha;
}

VOID CColorPickerDlg::FillColors (VOID)
{
	INT c = CVistaColors::Colors(), nSelected = -1;
	HWND hwndColors = GetDlgItem(IDC_COLOR_LIST);
	HWND hwndAlpha = GetDlgItem(IDC_ALPHA);
	CHAR szAlpha[4];

	Assert(hwndColors && hwndAlpha);

	SendMessage(hwndColors,CB_RESETCONTENT,0,0);

	for(INT i = 0; i < c; i++)
	{
		SendMessage(hwndColors,CB_INSERTSTRING,i,(LPARAM)CVistaColors::m_vColors[i].lpszFriendlyName);
		if(m_crSolid == CVistaColors::m_vColors[i].crColor)
			nSelected = i;
	}

	SendMessage(hwndColors,CB_INSERTSTRING,c,(LPARAM)"Custom");

	if(-1 == nSelected)
	{
		CHAR szCustom[8];
		Formatting::TPrintF(szCustom, ARRAYSIZE(szCustom), NULL, "#%.6X", m_crSolid);
		SendMessage(hwndColors,CB_INSERTSTRING,c + 1,(LPARAM)szCustom);
		nSelected = c + 1;
	}

	SendMessage(hwndColors,CB_SETCURSEL,nSelected,0);

	Formatting::TUInt32ToAsc(m_bAlpha, szAlpha, ARRAYSIZE(szAlpha), 10, NULL);
	SetWindowText(hwndAlpha, szAlpha);
}

VOID CColorPickerDlg::SelectColorByIndex (INT nSelected)
{
	INT c = CVistaColors::Colors();
	HWND hwndColors = GetDlgItem(IDC_COLOR_LIST);
	BOOL fInvalidateSwatch = FALSE;

	Assert(hwndColors);

	nSelected = SendMessage(hwndColors,CB_GETCURSEL,0,0);
	if(nSelected < c)
	{
		m_crSolid = CVistaColors::m_vColors[nSelected].crColor;
		fInvalidateSwatch = TRUE;
	}
	else if(nSelected == c)
	{
		CHOOSECOLOR cc = {0};

		cc.lStructSize = sizeof(cc);
		GetWindow(&cc.hwndOwner);
		cc.lpCustColors = m_crCustomColors;
		cc.rgbResult = m_crSolid;
		cc.Flags = CC_FULLOPEN | CC_RGBINIT;

		if(ChooseColor(&cc))
		{
			m_crSolid = cc.rgbResult;
			FillColors();
			fInvalidateSwatch = TRUE;
		}
	}

	if(fInvalidateSwatch)
		InvalidateRect(GetDlgItem(IDC_COLOR_SWATCH),NULL,TRUE);
}

BOOL CColorPickerDlg::ReadAlphaChannel (VOID)
{
	BOOL fSuccess = FALSE;
	HWND hwndAlpha = GetDlgItem(IDC_ALPHA);
	CHAR szAlpha[4];

	Assert(hwndAlpha);

	if (0 < ::GetWindowText(hwndAlpha,szAlpha,sizeof(szAlpha)))
	{
		INT n = atoi(szAlpha);
		if(n >= 0 && n <= 255)
		{
			m_bAlpha = (BYTE)n;
			fSuccess = TRUE;
		}
	}

	return fSuccess;
}

VOID CColorPickerDlg::DrawColorSwatch (LPDRAWITEMSTRUCT lpdis)
{
	Assert(IDC_COLOR_SWATCH == lpdis->CtlID);

	if(ODA_DRAWENTIRE == lpdis->itemAction)
	{
		HBRUSH hbrColor = CreateSolidBrush(m_crSolid);
		FillRect(lpdis->hDC,&lpdis->rcItem,hbrColor);
		DeleteObject(hbrColor);
	}
}

BOOL CColorPickerDlg::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	UNREFERENCED_PARAMETER(lParam);

	BOOL fHandled = FALSE;

	switch(message)
	{
	case WM_INITDIALOG:
		FillColors();

		lResult = TRUE;
		fHandled = TRUE;
		break;
	case WM_DRAWITEM:
		DrawColorSwatch((LPDRAWITEMSTRUCT)lParam);
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDC_COLOR_LIST:
			if(HIWORD(wParam) == CBN_SELCHANGE)
				SelectColorByIndex(SendMessage((HWND)lParam,CB_GETCURSEL,0,0));
			break;
		case IDOK:
			if(FALSE == ReadAlphaChannel())
				break;
			// Fall through
		case IDCANCEL:
			End(LOWORD(wParam));
			fHandled = TRUE;
			break;
		}
		break;
	}

	return fHandled;
}