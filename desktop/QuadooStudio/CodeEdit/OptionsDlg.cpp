#include <windows.h>
#include <commctrl.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Util\Formatting.h"
#include "OptionsDlg.h"

COLORREF RealizeColor (COLORREF col);

#define	REALIZE_SYSCOL(col) (RealizeColor(col))
#define	NUM_DEFAULT_COLORS	17
#define	MSG_UPDATE_PREVIEW	(WM_USER + 1)

struct _CUSTCOL
{
	COLORREF cr;
	WCHAR* szName;

} CUSTCOL[NUM_DEFAULT_COLORS] = 
{
	{ RGB(255,255,255),	L"Automatic" },
	{ RGB(0,0,0),		L"Black" },
	{ RGB(255,255,255),	L"White" },
	{ RGB(128, 0, 0),	L"Maroon" },
	{ RGB(0, 128,0),	L"Dark Green" },
	{ RGB(128,128,0),	L"Olive" },
	{ RGB(0,0,128),		L"Dark Blue" },
	{ RGB(128,0,128),	L"Purple" },
	{ RGB(0,128,128),	L"Aquamarine" },
	{ RGB(196,196,196),	L"Light Grey" },
	{ RGB(128,128,128),	L"Dark Grey" },
	{ RGB(255,0,0),		L"Red" },
	{ RGB(0,255,0),		L"Green" },
	{ RGB(255,255,0),	L"Yellow" },
	{ RGB(0,0,255),		L"Blue" },
	{ RGB(255,0,255),	L"Magenta" },
	{ RGB(0,255,255),	L"Cyan" }
};

UINT_PTR COptionsDlg::m_idLastSubclass = 0;

//
//	Convert 'points' to logical-units suitable for CreateFont
//
int PointsToLogical (int nPointSize)
{
	HDC hdc      = GetDC(0);
	int nLogSize = -MulDiv(nPointSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
	ReleaseDC(0, hdc);

	return nLogSize;
}

int LogicalToPoints (int nLogical)
{
	HDC hdc      = GetDC(0);
	int nPoints = MulDiv(nLogical, 72, GetDeviceCaps(hdc, LOGPIXELSY));
	ReleaseDC(0, hdc);

	return nPoints;
}

DWORD_PTR GetCurrentComboData (HWND hwnd, UINT uCtrl)
{
	LRESULT idx = SendDlgItemMessage(hwnd, uCtrl, CB_GETCURSEL, 0, 0);
	return SendDlgItemMessage(hwnd, uCtrl, CB_GETITEMDATA, idx == -1 ? 0 : idx, 0);
}

DWORD_PTR GetCurrentListData (HWND hwnd, UINT uCtrl)
{
	LRESULT idx = SendDlgItemMessage(hwnd, uCtrl, LB_GETCURSEL, 0, 0);
	return SendDlgItemMessage(hwnd, uCtrl, LB_GETITEMDATA, idx == -1 ? 0 : idx, 0);
}

//
//	Simple wrapper around CreateFont
//
HFONT EasyCreateFont (int nPointSize, BOOL fBold, DWORD dwQuality, WCHAR* szFace)
{
	return CreateFont(PointsToLogical(nPointSize), 
					  0, 0, 0, 
					  fBold ? FW_BOLD : 0,
					  0,0,0,DEFAULT_CHARSET,0,0,
					  dwQuality,
					  0,
					  szFace);
}

void AddColorListItem (HWND hwnd, UINT uItem, int fgIdx, int bgIdx, WCHAR* szName)
{
	HWND hwndCtrl = GetDlgItem(hwnd, uItem);
	LRESULT idx = SendMessage(hwndCtrl, LB_ADDSTRING, 0, (LPARAM)szName);
	SendMessage(hwndCtrl, LB_SETITEMDATA, idx, MAKELONG(fgIdx, bgIdx));
}

void AddColorComboItem (HWND hwnd, UINT uItem, COLORREF col, WCHAR* szName)
{
	HWND hwndCtrl = GetDlgItem(hwnd, uItem);
	LRESULT idx = SendMessage(hwndCtrl, CB_ADDSTRING, 0, (LPARAM)szName);
	SendMessage(hwndCtrl, CB_SETITEMDATA, idx, col);
}

void PaintFrameRect (HDC hdc, RECT* rect, COLORREF border, COLORREF fill)
{
	HBRUSH   hbrFill	= CreateSolidBrush(fill);
	HBRUSH   hbrBorder	= CreateSolidBrush(border);

	FrameRect(hdc, rect, hbrBorder);
	InflateRect(rect, -1, -1);
	FillRect(hdc, rect,  hbrFill);
	InflateRect(rect, 1, 1);

	DeleteObject(hbrFill);
	DeleteObject(hbrBorder);
}

void DrawItem_DefaultColors (DRAWITEMSTRUCT* dis)
{
	if(dis->itemState & ODS_DISABLED)
	{
		SetTextColor(dis->hDC, GetSysColor(COLOR_3DSHADOW));
		SetBkColor(dis->hDC,   GetSysColor(COLOR_3DFACE));
	}
	else
	{
		if((dis->itemState & ODS_SELECTED))
		{
			SetTextColor(dis->hDC,  GetSysColor(COLOR_HIGHLIGHTTEXT));
			SetBkColor(dis->hDC,	GetSysColor(COLOR_HIGHLIGHT));
		}
		else
		{
			SetTextColor(dis->hDC, GetSysColor(COLOR_WINDOWTEXT));
			SetBkColor(dis->hDC,   GetSysColor(COLOR_WINDOW));
		}
	}
}

BOOL PickColor (HWND hwndParent, COLORREF* col, COLORREF* custCol)
{
	CHOOSECOLOR cc = { sizeof(cc) };
	COLORREF    custTmp[16];

	memcpy(custTmp, custCol, sizeof(custTmp));

	cc.Flags			= CC_ANYCOLOR|CC_FULLOPEN|CC_SOLIDCOLOR|CC_RGBINIT;
	cc.hwndOwner		= hwndParent;
	cc.lpCustColors		= custTmp;
	cc.rgbResult		= *col;

	if(ChooseColor(&cc))
	{
		*col = cc.rgbResult;
		CopyMemory(custCol, custTmp, sizeof(custTmp));
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

COptionsDlg::COptionsDlg (COLORREF* prgColors) :
	CBaseDialog(IDD_COLORS),
	m_prgColors(prgColors),
	m_hIcon2(NULL),
	m_hIcon3(NULL),
	m_hPreviewFont(NULL),
	m_hNormalFont(NULL),
	m_hBoldFont(NULL),
	m_idPreviewSubclass(0)
{
	ZeroMemory(m_rgbCustColors, sizeof(m_rgbCustColors));
}

COptionsDlg::~COptionsDlg ()
{
}

BOOL COptionsDlg::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	switch(message)
	{
	case WM_INITDIALOG:
		InitializeFontOptions();
		CenterHost();
		break;

	case WM_MEASUREITEM:
		// can't do anything here because we haven't created
		// the fonts yet, so send a manual CB_SETITEMHEIGHT instead
		return FALSE;

	case WM_DRAWITEM:
		if(wParam == IDC_FONTLIST)
			return FontCombo_DrawItem((DRAWITEMSTRUCT *)lParam);
		else if(wParam == IDC_FGCOLCOMBO || wParam == IDC_BGCOLCOMBO)
			return ColorCombo_DrawItem(wParam, (DRAWITEMSTRUCT *)lParam, FALSE);
		return FALSE;

	case WM_NOTIFY:
		{
			NMHDR* pnmhdr = (NMHDR*)lParam;
			if(pnmhdr->code == NM_CUSTOMDRAW)
				return FALSE;
		}
		return FALSE;

	case MSG_UPDATE_PREVIEW:
		{
			HWND hwnd;
			GetWindow(&hwnd);
			UpdatePreviewPane(hwnd);
		}
		return TRUE;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDC_FONTLIST:
			{
				HWND hwnd;

				GetWindow(&hwnd);
				if(HIWORD(wParam) == CBN_SELCHANGE)
				{
					InitSizeList(hwnd);
				}

				PostMessage(hwnd, MSG_UPDATE_PREVIEW, 0, 0);
				return TRUE;
			}

		case IDC_ITEMLIST:
			if(HIWORD(wParam) == CBN_SELCHANGE)
			{
				HWND hwnd;
				DWORD_PTR itemidx;

				GetWindow(&hwnd);
				itemidx = GetCurrentListData(hwnd, IDC_ITEMLIST);

				SelectColorInList(IDC_FGCOLCOMBO, LOWORD(itemidx));
				SelectColorInList(IDC_BGCOLCOMBO, HIWORD(itemidx));

				UpdatePreviewPane(hwnd);
			}
			return TRUE;

		case IDC_SIZELIST:
			if(HIWORD(wParam) == CBN_SELCHANGE || 
			   HIWORD(wParam) == CBN_EDITCHANGE)
			{
				HWND hwnd;
				GetWindow(&hwnd);
				PostMessage(hwnd, MSG_UPDATE_PREVIEW, 0, 0);
			}
			return TRUE;

		case IDC_FGCOLCOMBO:
		case IDC_BGCOLCOMBO:
			if(HIWORD(wParam) == CBN_SELCHANGE)
			{
				HWND hwnd;
				GetWindow(&hwnd);

				short fgidx = LOWORD(GetCurrentListData(hwnd, IDC_ITEMLIST));
				short bgidx = HIWORD(GetCurrentListData(hwnd, IDC_ITEMLIST));

				if(fgidx >= 0)
					m_rgbTempColorList[fgidx] = (COLORREF)GetCurrentComboData(hwnd, IDC_FGCOLCOMBO);
				
				if(bgidx >= 0)
					m_rgbTempColorList[bgidx] = (COLORREF)GetCurrentComboData(hwnd, IDC_BGCOLCOMBO);

				PostMessage(hwnd, MSG_UPDATE_PREVIEW, 0, 0);
			}
			return TRUE;
			
		case IDC_BOLD:
			{
				HWND hwnd;
				GetWindow(&hwnd);
				PostMessage(hwnd, MSG_UPDATE_PREVIEW, 0, 0);
			}
			return TRUE;

		case IDC_BUTTON2:
			{
				HWND hwnd;
				GetWindow(&hwnd);

				COLORREF col = 0;
				int idx = LOWORD(GetCurrentListData(hwnd, IDC_ITEMLIST));

				if(idx >= 0)
					col = REALIZE_SYSCOL(m_rgbTempColorList[idx]);
				
				if(PickColor(hwnd, &col, m_rgbCustColors))
				{
					m_rgbTempColorList[idx] = col;
					SelectColorInList(IDC_FGCOLCOMBO, (short)idx);
					UpdatePreviewPane(hwnd);
				}
			}
			return TRUE;

		case IDC_BUTTON3:
			{
				HWND hwnd;
				GetWindow(&hwnd);

				COLORREF col = 0;
				int idx = HIWORD(GetCurrentListData(hwnd, IDC_ITEMLIST));
				
				if(idx >= 0)
					col = REALIZE_SYSCOL(m_rgbTempColorList[idx]);
				
				if(PickColor(hwnd, &col, m_rgbCustColors))
				{
					m_rgbTempColorList[idx] = col;
					SelectColorInList(IDC_BGCOLCOMBO, (short)idx);
					UpdatePreviewPane(hwnd);
				}
			}
			return TRUE;

		case IDOK:
			{
				HWND hwnd;

				GetWindow(&hwnd);

				m_lfEdit.lfWidth = 0;
				m_lfEdit.lfHeight = GetDlgItemInt(hwnd, IDC_SIZELIST, 0, 0);
				if(IsDlgButtonChecked(hwnd, IDC_BOLD) & BST_CHECKED)
					m_lfEdit.lfWeight = FW_BOLD;
				else
					m_lfEdit.lfWeight = FW_NORMAL;

				GetDlgItemText(hwnd, IDC_FONTLIST, m_lfEdit.lfFaceName, LF_FACESIZE);

				CopyMemory(m_prgColors, m_rgbTempColorList, sizeof(m_rgbTempColorList));
				End(IDOK);
			}
			return TRUE;

		case IDCANCEL:
			End(IDCANCEL);
			return TRUE;
		}
		break;

	case WM_DESTROY:
		RemoveWindowSubclass(GetDlgItem(IDC_PREVIEW), PreviewWndProc, m_idPreviewSubclass);
		m_idPreviewSubclass = 0;

		DeleteObject(m_hPreviewFont);
		DeleteObject(m_hNormalFont);
		DeleteObject(m_hBoldFont);

		DeleteObject(m_hIcon2);
		DeleteObject(m_hIcon3);
		break;
	}

	return FALSE;
}

void COptionsDlg::SetComboItemHeight (HWND hwndCombo, int nMinHeight)
{
	TEXTMETRIC	tm;
	HDC			hdc	 = GetDC(hwndCombo);
	HANDLE		hold = SelectObject(hdc, m_hNormalFont);
	
	// item height must fit the font+smallicon height
	GetTextMetrics(hdc, &tm);
	nMinHeight = max(tm.tmHeight, nMinHeight);

	SelectObject(hdc, hold);
	ReleaseDC(hwndCombo, hdc);

	SendMessage(hwndCombo, CB_SETITEMHEIGHT, -1, nMinHeight);
	SendMessage(hwndCombo, CB_SETITEMHEIGHT, 0, nMinHeight);
}

void COptionsDlg::FillFontComboList (HWND hwndCombo)
{
	HDC		hdc = GetDC(hwndCombo);
	LOGFONT lf;
	LRESULT	idx;

	SendMessage(hwndCombo, WM_SETFONT, (WPARAM)m_hNormalFont, 0);

	lf.lfCharSet			= ANSI_CHARSET;	// DEFAULT_CHARSET;
	lf.lfFaceName[0]		= '\0';			// all fonts
	lf.lfPitchAndFamily		= 0;

	// store the list of fonts in the combo
	EnumFontFamiliesEx(hdc, &lf, (FONTENUMPROC)EnumFontNames, (LPARAM)hwndCombo, 0);

	ReleaseDC(hwndCombo, hdc);

	// select current font in list
	idx = SendMessage(hwndCombo, CB_FINDSTRING, 0, (LPARAM)m_lfEdit.lfFaceName);
	SendMessage(hwndCombo, CB_SETCURSEL, idx, 0);
}

void COptionsDlg::UpdatePreviewPane (HWND hwnd)
{
	TCHAR szFaceName[200];
	LRESULT idx = SendDlgItemMessage(hwnd, IDC_FONTLIST, CB_GETCURSEL, 0, 0);
	int size;
	LRESULT data;

	SendDlgItemMessage(hwnd, IDC_FONTLIST, CB_GETLBTEXT, idx, (LPARAM)szFaceName);

	size = GetDlgItemInt(hwnd, IDC_SIZELIST, 0, FALSE);

	SafeDeleteGdiObject(m_hPreviewFont);
	m_hPreviewFont = EasyCreateFont(size, IsDlgButtonChecked(hwnd, IDC_BOLD), m_lfEdit.lfQuality, szFaceName);

	idx  = SendDlgItemMessage(hwnd, IDC_ITEMLIST, LB_GETCURSEL, 0, 0);
	data = SendDlgItemMessage(hwnd, IDC_ITEMLIST, LB_GETITEMDATA, idx, 0);

	if((short)LOWORD(data) >= 0)
		m_crPreviewFG = REALIZE_SYSCOL(m_rgbTempColorList[LOWORD(data)]);
	else
		m_crPreviewFG = GetSysColor(COLOR_WINDOWTEXT);

	if((short)HIWORD(data) >= 0)
		m_crPreviewBG = REALIZE_SYSCOL(m_rgbTempColorList[HIWORD(data)]);
	else
		m_crPreviewBG = GetSysColor(COLOR_WINDOW);

	InvalidateRect(GetDlgItem(IDC_PREVIEW), 0, TRUE); 
}

void COptionsDlg::InitSizeList (HWND hwnd)
{
	LOGFONT lf = { 0 };
	HDC hdc = GetDC(hwnd);

	// get current font size
	int cursize = GetDlgItemInt(hwnd, IDC_SIZELIST, 0, 0);

	LRESULT item = SendDlgItemMessage(hwnd, IDC_FONTLIST, CB_GETCURSEL, 0, 0);
	LRESULT count, nearest = 0;

	lf.lfCharSet		= DEFAULT_CHARSET;
	lf.lfPitchAndFamily = 0;
	SendDlgItemMessage(hwnd, IDC_FONTLIST, CB_GETLBTEXT, item, (LPARAM)lf.lfFaceName);

	// empty list
	SendDlgItemMessage(hwnd, IDC_SIZELIST, CB_RESETCONTENT, 0, 0);

	// enumerate font sizes
	EnumFontFamiliesEx(hdc, &lf, (FONTENUMPROC)EnumFontSizes, (LPARAM)GetDlgItem(IDC_SIZELIST), 0);

	// set selection to first item
	count = SendDlgItemMessage(hwnd, IDC_SIZELIST, CB_GETCOUNT, 0, 0);

	for(LRESULT i = 0; i < count; i++)
	{
		LRESULT n = SendDlgItemMessage(hwnd, IDC_SIZELIST, CB_GETITEMDATA, i, 0);

		if(n <= cursize)
			nearest = i;
	}

	SendDlgItemMessage(hwnd, IDC_SIZELIST, CB_SETCURSEL, nearest, 0);

	ReleaseDC(hwnd, hdc);
}

void COptionsDlg::SelectColorInList (UINT uComboIdx, short itemIdx)
{
	HWND hwndCombo = GetDlgItem(uComboIdx);
	LRESULT i;

	if(itemIdx == (short)-1)
	{
		SendMessage(hwndCombo, CB_SETCURSEL, 0, 0);
		EnableWindow(hwndCombo, FALSE);
		EnableWindow(::GetWindow(hwndCombo, GW_HWNDNEXT), FALSE);
		return;
	}
	else
	{
		EnableWindow(hwndCombo, TRUE);
		EnableWindow(::GetWindow(hwndCombo, GW_HWNDNEXT), TRUE);
	}

	if(itemIdx < 0 || itemIdx >= TXC_MAX_COLORS)
		return;

	// update the Auto item
	SendMessage(hwndCombo, CB_SETITEMDATA, 0, m_rgbAutoColorList[itemIdx]);

	// remove the custom entry (if any)
	SendMessage(hwndCombo, CB_DELETESTRING, NUM_DEFAULT_COLORS, 0);

	// if an "AUTO" color
	if((m_rgbTempColorList[itemIdx] & 0x80000000) ||
		m_rgbTempColorList[itemIdx] == m_rgbAutoColorList[itemIdx])
	{
		SendMessage(hwndCombo, CB_SETCURSEL, 0, 0);
	}
	// normal color
	else
	{
		// try to match current color with a default color
		for(i = 1; i < NUM_DEFAULT_COLORS; i++)
		{
			if(m_rgbTempColorList[itemIdx] == CUSTCOL[i].cr)
			{
				SendMessage(hwndCombo, CB_SETCURSEL, i, 0);
				break;
			}
		}
		
		// if we didn't match the color, add it as a custom entry
		if(i == NUM_DEFAULT_COLORS)
		{
			i = SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM)L"Custom");
			SendMessage(hwndCombo, CB_SETITEMDATA, i, m_rgbTempColorList[itemIdx]);
			SendMessage(hwndCombo, CB_SETCURSEL, i, 0);
		}
	}
}

VOID COptionsDlg::InitializeFontOptions (VOID)
{
	HINSTANCE hInstance;
	HWND hwnd, hwndPreview;
	LOGFONT lf;
	WCHAR ach[LF_FACESIZE];
	INT cColors = ARRAYSIZE(m_rgbCustColors);

	GetWindow(&hwnd);
#ifdef	_WIN64
	hInstance = (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);
#else
	hInstance = (HINSTANCE)GetWindowLongPtrW(hwnd, GWL_HINSTANCE);
#endif

	CopyMemory(m_rgbTempColorList, m_prgColors, sizeof(m_rgbTempColorList));
	CopyMemory(m_rgbAutoColorList, m_prgColors, sizeof(m_rgbAutoColorList));

	m_hIcon2 = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON2), IMAGE_ICON, 16, 16, 0);
	m_hIcon3 = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON3), IMAGE_ICON, 16, 16, 0);

	GetObject((HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0), sizeof(lf), &lf);
	lf.lfWeight   = FW_NORMAL;
	m_hNormalFont = CreateFontIndirect(&lf);
	lf.lfWeight   = FW_BOLD;
	m_hBoldFont   = CreateFontIndirect(&lf);

	//
	//	Manually set the COMBO item-heights because WM_MEASUREITEM has already
	//  been sent and we missed it..
	//
	SetComboItemHeight(GetDlgItem(IDC_FGCOLCOMBO), 14);
	SetComboItemHeight(GetDlgItem(IDC_BGCOLCOMBO), 14);
	SetComboItemHeight(GetDlgItem(IDC_FONTLIST), 16);

	// Create the list of fonts
	FillFontComboList(GetDlgItem(IDC_FONTLIST));

	// Update the font-size-list
	InitSizeList(hwnd);

	//
	//	Subclass the PREVIEW static control so we can custom-draw it
	//
	hwndPreview = GetDlgItem(IDC_PREVIEW);
	m_idPreviewSubclass = ++m_idLastSubclass;
	SetWindowSubclass(hwndPreview, PreviewWndProc, m_idPreviewSubclass, (DWORD_PTR)this);

	AddColorListItem(hwnd, IDC_ITEMLIST, TXC_FOREGROUND,		TXC_BACKGROUND,		L"Text");
	AddColorListItem(hwnd, IDC_ITEMLIST, TXC_HIGHLIGHTTEXT,		TXC_HIGHLIGHT,		L"Selected Text");
	AddColorListItem(hwnd, IDC_ITEMLIST, TXC_HIGHLIGHTTEXT2,	TXC_HIGHLIGHT2,		L"Inactive Selection");
	AddColorListItem(hwnd, IDC_ITEMLIST, TXC_MARGIN_BORDER,		TXC_MARGIN,			L"Left Margin");
	AddColorListItem(hwnd, IDC_ITEMLIST, -1,					TXC_DISABLED,		L"Disabled Window");
	AddColorListItem(hwnd, IDC_ITEMLIST, -1,					TXC_LONGLINE,		L"Long Lines");
	AddColorListItem(hwnd, IDC_ITEMLIST, TXC_CURRENTLINETEXT,	TXC_CURRENTLINE,	L"Current Line");

	SendDlgItemMessage(hwnd, IDC_ITEMLIST, LB_SETCURSEL, 0, 0);
	PostMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDC_ITEMLIST, LBN_SELCHANGE), (LPARAM)GetDlgItem(IDC_ITEMLIST));

	for(INT i = 0; i < NUM_DEFAULT_COLORS; i++)
	{
		AddColorComboItem(hwnd, IDC_FGCOLCOMBO, CUSTCOL[i].cr, CUSTCOL[i].szName);
		AddColorComboItem(hwnd, IDC_BGCOLCOMBO, CUSTCOL[i].cr, CUSTCOL[i].szName);
	}

	SendDlgItemMessage(hwnd, IDC_FGCOLCOMBO, CB_SETCURSEL, 1, 0);
	SendDlgItemMessage(hwnd, IDC_BGCOLCOMBO, CB_SETCURSEL, 0, 0);

	Formatting::TInt32ToAsc(m_lfEdit.lfHeight, ach, ARRAYSIZE(ach), 10, NULL);
	SendDlgItemMessage(hwnd, IDC_SIZELIST, CB_SELECTSTRING, -1, (LPARAM)ach);
	SendDlgItemMessage(hwnd, IDC_FONTLIST, CB_SELECTSTRING, -1, (LPARAM)m_lfEdit.lfFaceName);

	CheckDlgButton(hwnd, IDC_BOLD, FW_BOLD <= m_lfEdit.lfWeight ? BST_CHECKED : BST_UNCHECKED);

	UpdatePreviewPane(hwnd);
}

BOOL COptionsDlg::FontCombo_DrawItem (DRAWITEMSTRUCT* dis)
{
	TCHAR		szText[100];
	LRESULT		cchText;
	
	BOOL		fFixed		= LOWORD(dis->itemData);
	BOOL		fTrueType	= HIWORD(dis->itemData);

	TEXTMETRIC	tm;
	int			xpos, ypos;
	HANDLE		hOldFont;

	if(dis->itemAction & ODA_FOCUS && !(dis->itemState & ODS_NOFOCUSRECT))
	{
		DrawFocusRect(dis->hDC, &dis->rcItem);
		return TRUE;
	}

	/*{
		HTHEME hTheme = 	OpenThemeData(hwnd, L"combobox");
		RECT rc;
		HDC hdc=GetDC(GetParent(hwnd));
		CopyRect(&rc, &dis->rcItem);
		InflateRect(&rc, 3, 3);
		//GetClientRect(hwnd, &rc);
		//rc.bottom = rc.top + 22;

		//DrawThemeBackground(
		//	hTheme, 
		//	dis->hDC, 
		//	4,//CP_DROPDOWNBUTTON, 
		//	CBXS_HOT,//CBXS_NORMAL, 
		//	&rc, 
		//	&rc);

		CloseThemeData(hTheme);
		ReleaseDC(GetParent(hwnd),hdc);
		return TRUE;
	}*/

	//
	//	Get the item text
	//
	if(dis->itemID == -1)
		cchText = SendMessage(dis->hwndItem, WM_GETTEXT, 0, (LPARAM)szText);
	else
		cchText = SendMessage(dis->hwndItem, CB_GETLBTEXT, dis->itemID, (LPARAM)szText);
	
	//
	//	Set text color and background based on current state
	//
	DrawItem_DefaultColors(dis);

	// set the font: BOLD for fixed-width, NORMAL for 'normal'
	hOldFont = SelectObject(dis->hDC, fFixed ? m_hBoldFont : m_hNormalFont);
	GetTextMetrics(dis->hDC, &tm);

	ypos = dis->rcItem.top  + (dis->rcItem.bottom-dis->rcItem.top-tm.tmHeight)/2;
	xpos = dis->rcItem.left + 20;
	
	// draw the text
	ExtTextOut(dis->hDC, xpos, ypos,
		ETO_CLIPPED|ETO_OPAQUE, &dis->rcItem, szText, static_cast<UINT>(cchText), 0);

	// draw a 'TT' icon if the font is TRUETYPE
	if(fTrueType)
		DrawIconEx(dis->hDC, dis->rcItem.left+2, dis->rcItem.top, m_hIcon2,16, 16, 0, 0, DI_NORMAL);
	//else if(fTrueType == 2)
	//	DrawIconEx(dis->hDC, dis->rcItem.left+2, dis->rcItem.top, m_hIcon3,16, 16, 0, 0, DI_NORMAL);

	SelectObject(dis->hDC, hOldFont);

	// draw the focus rectangle
	if((dis->itemState & ODS_FOCUS) && !(dis->itemState & ODS_NOFOCUSRECT))
	{
		DrawFocusRect(dis->hDC, &dis->rcItem);
	}

	return TRUE;
}

//
//	Combobox must have the CBS_HASSTRINGS style set!!
//	
BOOL COptionsDlg::ColorCombo_DrawItem (UINT_PTR uCtrlId, DRAWITEMSTRUCT* dis, BOOL fSelectImage)
{
	RECT		rect	= dis->rcItem;
	int			boxsize = (dis->rcItem.bottom - dis->rcItem.top) - 4, cchText;
	int			xpos;
	int			ypos;
	TEXTMETRIC	tm;
	TCHAR		szText[80];
	HANDLE		hOldFont;
	
	if(!fSelectImage)
		rect.left += boxsize + 4;

	if(dis->itemAction & ODA_FOCUS && !(dis->itemState & ODS_NOFOCUSRECT))
	{
		DrawFocusRect(dis->hDC, &rect);
		return TRUE;
	}
	
	//
	//	Get the item text
	//
	if(dis->itemID == -1)
		cchText = (INT)SendMessage(dis->hwndItem, WM_GETTEXT, 0, (LPARAM)szText);
	else
		cchText = (INT)SendMessage(dis->hwndItem, CB_GETLBTEXT, dis->itemID, (LPARAM)szText);
	
	//
	//	Set text color and background based on current state
	//
	DrawItem_DefaultColors(dis);
	
	//
	//	Draw the text (centered vertically)
	//
	hOldFont = SelectObject(dis->hDC, m_hNormalFont);

	GetTextMetrics(dis->hDC, &tm);
	ypos = dis->rcItem.top  + (dis->rcItem.bottom - dis->rcItem.top - tm.tmHeight) / 2;
	xpos = dis->rcItem.left + boxsize + 4 + 4;
	
	ExtTextOut(dis->hDC, xpos, ypos, 
		ETO_CLIPPED|ETO_OPAQUE, &rect, szText, cchText, 0);
	
	if((dis->itemState & ODS_FOCUS) && !(dis->itemState & ODS_NOFOCUSRECT))
	{
		DrawFocusRect(dis->hDC, &rect);
	}
	
	// 
	//	Paint the color rectangle
	//	
	rect = dis->rcItem;
	InflateRect(&rect, -2, -2);
	rect.right = rect.left + boxsize;
	
	if(dis->itemState & ODS_DISABLED)
		PaintFrameRect(dis->hDC, &rect, GetSysColor(COLOR_3DSHADOW), GetSysColor(COLOR_3DFACE));
	else
		PaintFrameRect(dis->hDC, &rect, RGB(0,0,0), REALIZE_SYSCOL(static_cast<COLORREF>(dis->itemData)));

	return TRUE;
}

VOID COptionsDlg::OnPreviewPaint (HWND hwndPreview)
{
	RECT rect;
	PAINTSTRUCT ps;
	HANDLE hold;
	HDC hdc = BeginPaint(hwndPreview, &ps);

	GetClientRect(hwndPreview, &rect);

	FrameRect(hdc, &rect, GetSysColorBrush(COLOR_3DSHADOW));
	InflateRect(&rect, -1, -1);

	SetTextColor(hdc, m_crPreviewFG);
	SetBkColor(hdc, m_crPreviewBG);

	ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rect, 0, 0, 0);
	hold = SelectObject(hdc, m_hPreviewFont);

	DrawText(hdc, L"Sample Text", -1, &rect, DT_SINGLELINE|DT_CENTER|DT_VCENTER);

	SelectObject(hdc, hold);
	EndPaint(hwndPreview, &ps);
}

int CALLBACK COptionsDlg::EnumFontSizes (ENUMLOGFONTEX* lpelfe, NEWTEXTMETRICEX* lpntme, DWORD FontType, LPARAM lParam)
{
	static int ttsizes[] = { 8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28, 36, 48, 72 };
	WCHAR ach[100];

	BOOL fTrueType = (lpelfe->elfLogFont.lfOutPrecision == OUT_STROKE_PRECIS) ? TRUE : FALSE;

	HWND hwndCombo = (HWND)lParam;
	LRESULT count, idx;

	if(fTrueType)
	{
		for(LRESULT i = 0; i < (sizeof(ttsizes) / sizeof(ttsizes[0])); i++)
		{
			Formatting::TInt32ToAsc(ttsizes[i], ach, ARRAYSIZE(ach), 10, NULL);
			idx = SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM)ach);
			SendMessage(hwndCombo, CB_SETITEMDATA, idx, ttsizes[i]);
			//nFontSizes[i] = ttsizes[i];
		}
		//nNumFontSizes = i;
		return 0;
	}
	else
	{
		int size = LogicalToPoints(lpntme->ntmTm.tmHeight);

		count = SendMessage(hwndCombo, CB_GETCOUNT, 0, 0);
		idx = count;

		for(LRESULT i = 0; i < count; i++)
		{
			LRESULT nItemSize = SendMessage(hwndCombo, CB_GETITEMDATA, i, 0);
			if(size <= nItemSize)
			{
				if(size < nItemSize)
					idx = i;
				else
					idx = -1;
				break;
			}
		}
		
		if(idx != -1)
		{
			Formatting::TInt32ToAsc(size, ach, ARRAYSIZE(ach), 10, NULL);
			idx = SendMessage(hwndCombo, CB_INSERTSTRING, idx, (LPARAM)ach);
			SendMessage(hwndCombo, CB_SETITEMDATA, idx, size);
		}
	}

	return 1;
}

int CALLBACK COptionsDlg::EnumFontNames (ENUMLOGFONTEX* lpelfe, NEWTEXTMETRICEX* lpntme, DWORD FontType, LPARAM lParam)
{
	HWND hwndCombo = (HWND)lParam;
	WCHAR* pszName = lpelfe->elfLogFont.lfFaceName;

	if(pszName[0] == '@')
		return 1;

	// make sure font doesn't already exist in our list
	if(SendMessage(hwndCombo, CB_FINDSTRING, 0, (LPARAM)pszName) == CB_ERR)
	{
		LRESULT	idx;
		BOOL	fFixed;
		int		fTrueType;		// 0 = normal, 1 = TrueType, 2 = OpenType

		// add the font
		idx = SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM)pszName);

		// record the font's attributes (Fixedwidth and Truetype)
		fFixed		= (lpelfe->elfLogFont.lfPitchAndFamily & FIXED_PITCH) ? TRUE : FALSE;
		fTrueType	= (lpelfe->elfLogFont.lfOutPrecision == OUT_STROKE_PRECIS) ? TRUE : FALSE;
		fTrueType	= (lpntme->ntmTm.ntmFlags & NTM_TT_OPENTYPE) ? 2 : fTrueType;

		// store this information in the list-item's userdata area
		SendMessage(hwndCombo, CB_SETITEMDATA, idx, MAKEWPARAM(fFixed, fTrueType));
	}

	return 1;
}

LRESULT CALLBACK COptionsDlg::PreviewWndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	switch(msg)
	{
	case WM_ERASEBKGND:
		return 1;

	case WM_PAINT:
		reinterpret_cast<COptionsDlg*>(dwRefData)->OnPreviewPaint(hwnd);
		return 0;
	}

	return DefSubclassProc(hwnd, msg, wParam, lParam);
}
