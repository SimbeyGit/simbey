#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "Library\DPI.h"
#include "Tabs.h"

VOID CTab::Draw (CTabs* pTabs, Gdiplus::Graphics& graphics, Gdiplus::Font* pFont, bool fHover)
{
	FLOAT rBarSize = (FLOAT)pTabs->m_szTabs.cy - 1.0f;
	Gdiplus::PointF pt[ARRAYSIZE(m_pt)];

	for(INT i = 0; i < ARRAYSIZE(pt); i++)
	{
		pt[i].X = m_xCalculated + m_pt[i].X;
		pt[i].Y = (rBarSize - m_pt[i].Y);
	}

	Gdiplus::LinearGradientBrush brGradient(
		Gdiplus::PointF(pt[0].X, pt[1].Y),			// Start point of the gradient (top-left)
		Gdiplus::PointF(pt[0].X, pt[0].Y),			// End point of the gradient (bottom-left)
		m_fActive ? pTabs->m_tabColors.crActive1 : (fHover ? pTabs->m_tabColors.crHover1 : pTabs->m_tabColors.crNormal1),	// Starting color (top color)
		m_fHighlight ? pTabs->m_tabColors.crHighlight : (m_fActive ? pTabs->m_tabColors.crActive2 : (fHover ? pTabs->m_tabColors.crHover2 : pTabs->m_tabColors.crNormal2)));	// Ending color (bottom color)
	graphics.FillPolygon(&brGradient, pt, ARRAYSIZE(pt));

	Gdiplus::PointF ptLabel(pt[1].X + pTabs->m_yTextOffset, pt[1].Y + pTabs->m_yTextOffset);
	Gdiplus::SolidBrush brLabel(pTabs->m_tabColors.crLabel);

	Gdiplus::Pen pnOutline(m_fActive ? pTabs->m_tabColors.crActiveOutline : pTabs->m_tabColors.crNormalOutline, 1.0f);
	graphics.DrawLine(&pnOutline, pt[0], pt[1]);
	graphics.DrawLine(&pnOutline, pt[1], pt[2]);
	graphics.DrawLine(&pnOutline, pt[2], pt[3]);
	graphics.DrawLine(&pnOutline, pt[3], pt[4]);

	graphics.DrawString(RStrToWide(m_rstrLabel), RStrLen(m_rstrLabel), pFont, ptLabel, &brLabel);

	if(m_fActive)
	{
		Gdiplus::PointF ptLeft(0.0f, pt[0].Y);
		Gdiplus::PointF ptRight((FLOAT)pTabs->m_szTabs.cx, pt[4].Y);
		graphics.DrawLine(&pnOutline, pt[0], ptLeft);
		graphics.DrawLine(&pnOutline, pt[4], ptRight);
	}
}

BOOL CTab::IsPointOver (CTabs* pTabs, LONG x, LONG y)
{
	FLOAT rBarSize = (FLOAT)pTabs->m_szTabs.cy - 1.0f;
	Gdiplus::PointF pt[ARRAYSIZE(m_pt)];

	for(INT i = 0; i < ARRAYSIZE(pt); i++)
	{
		pt[i].X = m_xCalculated + m_pt[i].X;
		pt[i].Y = (rBarSize - m_pt[i].Y);
	}

	{
		Gdiplus::GraphicsPath path;
		path.AddPolygon(pt, ARRAYSIZE(pt));
		return path.IsVisible(x, y);
	}
}

CTabs::CTabs (HICON hCloseIcon, HICON hDropDown) :
	m_hCloseIcon(hCloseIcon),
	m_hDropDown(hDropDown),
	m_pBuffer(NULL),
	m_pNormal(NULL),
	m_pBold(NULL),
	m_fHoverClose(FALSE),
	m_fHoverDropDown(FALSE),
	m_idxHover(-1)
{
}

CTabs::~CTabs ()
{
	m_aTabs.DeleteAll();

	SafeDelete(m_pNormal);
	SafeDelete(m_pBold);
	SafeDelete(m_pBuffer);
}

VOID CTabs::SetDarkMode (bool fDarkMode)
{
	if(fDarkMode)
	{
		m_tabColors.crBackground = Gdiplus::Color(255, 60, 60, 60);

		m_tabColors.crNormal1 = Gdiplus::Color(255, 80, 80, 80);
		m_tabColors.crNormal2 = Gdiplus::Color(255, 100, 100, 100);

		m_tabColors.crHover1 = Gdiplus::Color(255, 100, 100, 100);
		m_tabColors.crHover2 = Gdiplus::Color(255, 120, 120, 120);

		m_tabColors.crActive1 = Gdiplus::Color(255, 90, 90, 90);
		m_tabColors.crActive2 = Gdiplus::Color(255, 110, 110, 110);

		m_tabColors.crActiveOutline = Gdiplus::Color(255, 50, 50, 50);
		m_tabColors.crNormalOutline = Gdiplus::Color(255, 40, 40, 40);

		m_tabColors.crHighlight = Gdiplus::Color(255, 210, 140, 20);
		m_tabColors.crLabel = Gdiplus::Color(255, 220, 220, 220);

		m_tabColors.crButtonOutline = Gdiplus::Color(255, 80, 80, 80);
		m_tabColors.crButtonBackground = Gdiplus::Color(255, 100, 100, 100);
	}
	else
	{
		m_tabColors.crBackground = Gdiplus::Color(255, 236, 239, 250);

		m_tabColors.crNormal1 = Gdiplus::Color(255, 236, 245, 252);
		m_tabColors.crNormal2 = Gdiplus::Color(255, 152, 180, 210);

		m_tabColors.crHover1 = Gdiplus::Color(255, 247, 252, 254);
		m_tabColors.crHover2 = Gdiplus::Color(255, 129, 208, 241);

		m_tabColors.crActive1 = Gdiplus::Color(255, 249, 252, 254);
		m_tabColors.crActive2 = Gdiplus::Color(255, 210, 230, 250);

		m_tabColors.crActiveOutline = Gdiplus::Color(255, 105, 161, 191);
		m_tabColors.crNormalOutline = Gdiplus::Color(255, 145, 150, 162);

		m_tabColors.crHighlight = Gdiplus::Color(255, 215, 138, 255);
		m_tabColors.crLabel = Gdiplus::Color(255, 0, 0, 0);

		m_tabColors.crButtonOutline = Gdiplus::Color(255, 51, 153, 255);
		m_tabColors.crButtonBackground = Gdiplus::Color(255, 206, 237, 250);
	}
}

HRESULT CTabs::LoadMetrics (HWND hwnd)
{
	HRESULT hr;
	HDC hdc = GetDC(hwnd);
	Gdiplus::Graphics graphics(hdc);

	CheckIf(!GetIconDimensions(m_hDropDown, m_szDropDown.cx, m_szDropDown.cy), E_FAIL);
	CheckIf(!GetIconDimensions(m_hCloseIcon, m_szCloseButton.cx, m_szCloseButton.cy), E_FAIL);
	m_xButtonPadding = (LONG)DPI::Scale(4.0f);
	m_xRightButtons = m_szDropDown.cx + m_szCloseButton.cx + m_xButtonPadding * 5;

	Check(CreateFont(hdc, L"Arial", 11, FW_NORMAL, &m_pNormal));
	Check(CreateFont(hdc, L"Arial", 11, FW_MEDIUM, &m_pBold));

	m_yFontSize = m_pNormal->GetHeight(&graphics);
	m_yTextOffset = DPI::ScaleY(3);
	m_yNormalTab = m_yFontSize + m_yTextOffset * 2.0f;
	m_yActiveTab = m_yNormalTab + m_yTextOffset;
	m_xLabelOffset = m_yNormalTab;

Cleanup:
	ReleaseDC(hwnd, hdc);
	return hr;
}

VOID CTabs::Resize (LONG nWidth, LONG nHeight)
{
	m_szTabs.cx = nWidth;
	m_szTabs.cy = (LONG)(m_yActiveTab + m_yTextOffset + 1.0f);

	CalculateVisibleTabs();
}

VOID CTabs::GetTabsRect (LONG xOffset, LONG yOffset, __out RECT& rc)
{
	rc.left = xOffset;
	rc.top = yOffset;
	rc.right = rc.left + m_szTabs.cx;
	rc.bottom = rc.top + m_szTabs.cy;
}

sysint CTabs::FindTab (PVOID pvData)
{
	for(sysint i = 0; i < m_aTabs.Length(); i++)
	{
		if(m_aTabs[i]->m_pvData == pvData)
			return i;
	}
	return -1;
}

HRESULT CTabs::AddTab (HDC hdc, RSTRING rstrLabel, PVOID pvData)
{
	HRESULT hr;
	Gdiplus::Graphics graphics(hdc);
	Gdiplus::RectF boundingRect;
	CTab* pTab = __new CTab(rstrLabel, pvData);

	CheckAlloc(pTab);
	Gdiplus::PointF* pt = pTab->m_pt;

	graphics.MeasureString(RStrToWide(rstrLabel), RStrLen(rstrLabel), m_pNormal, Gdiplus::PointF(0, 0), &boundingRect);

	pt[0].X = -m_xLabelOffset;
	pt[0].Y = 0.0f;

	pt[1].X = 0.0f;
	pt[1].Y = m_yNormalTab;

	pt[2].X = m_yTextOffset + boundingRect.Width + m_yTextOffset;
	pt[2].Y = m_yNormalTab;

	pt[3].X = pt[2].X + m_yTextOffset;
	pt[3].Y = pt[2].Y - m_yTextOffset;

	pt[4].X = pt[3].X;
	pt[4].Y = 0.0f;

	Check(m_aTabs.InsertAt(pTab, 0));
	pTab = NULL;

	CalculateVisibleTabs();

Cleanup:
	SafeDelete(pTab);
	return hr;
}

HRESULT CTabs::SetActive (sysint idxTab)
{
	HRESULT hr;

	CheckIf(idxTab >= m_aTabs.Length(), HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));

	// If the tab isn't visible, then move it to the beginning of the array.
	if(idxTab >= m_cVisible)
	{
		m_aTabs.MoveItem(0, idxTab);
		idxTab = 0;

		CalculateVisibleTabs();
	}

	for(sysint i = 0; i < m_aTabs.Length(); i++)
		m_aTabs[i]->SetActive(idxTab == i, m_yTextOffset);
	hr = S_OK;

Cleanup:
	return hr;
}

sysint CTabs::GetActiveTab (VOID)
{
	for(sysint i = 0; i < m_aTabs.Length(); i++)
	{
		CTab* pTab = m_aTabs[i];
		if(pTab->m_fActive)
			return i;
	}
	return -1;
}

HRESULT CTabs::RemoveTab (sysint idxTab)
{
	HRESULT hr;
	CTab* pTab;

	Check(m_aTabs.RemoveChecked(idxTab, &pTab));
	__delete pTab;

	CalculateVisibleTabs();

Cleanup:
	return hr;
}

PVOID CTabs::GetTabData (sysint idxTab)
{
	if(idxTab < m_aTabs.Length())
		return m_aTabs[idxTab]->m_pvData;
	return NULL;
}

HRESULT CTabs::HighlightTab (sysint idxTab, bool fHighlight)
{
	HRESULT hr;

	CheckIf(idxTab >= m_aTabs.Length(), HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
	m_aTabs[idxTab]->m_fHighlight = fHighlight;
	hr = S_OK;

Cleanup:
	return hr;
}

bool CTabs::GetActiveTabHighlight (VOID)
{
	sysint idxTab = GetActiveTab();
	return -1 != idxTab && m_aTabs[idxTab]->m_fHighlight;
}

VOID CTabs::Draw (HDC hdc, LONG x, LONG y)
{
	if(NULL == m_pBuffer || m_pBuffer->GetWidth() != m_szTabs.cx)
	{
		SafeDelete(m_pBuffer);
		m_pBuffer = __new Gdiplus::Bitmap(m_szTabs.cx, m_szTabs.cy, PixelFormat32bppARGB);
	}

	Gdiplus::Graphics graphics(m_pBuffer);
	CTab* pActive = NULL;

	Gdiplus::SolidBrush brBackground(m_tabColors.crBackground);
	Gdiplus::RectF rcBackground(0.0f, 0.0f, (FLOAT)m_szTabs.cx, (FLOAT)m_szTabs.cy);
	graphics.FillRectangle(&brBackground, rcBackground);

	for(sysint i = m_cVisible - 1; i >= 0; i--)
	{
		CTab* pTab = m_aTabs[i];

		if(pTab->m_fActive)
			pActive = pTab;
		else
			pTab->Draw(this, graphics, m_pNormal, m_idxHover == i);
	}

	if(pActive)
		pActive->Draw(this, graphics, m_pBold, false);
	else
	{
		Gdiplus::Pen pnOutline(m_tabColors.crActiveOutline, 1.0f);
		Gdiplus::PointF ptLeft(0.0f, (FLOAT)(m_szTabs.cy - 1));
		Gdiplus::PointF ptRight((FLOAT)m_szTabs.cx, ptLeft.Y);
		graphics.DrawLine(&pnOutline, ptLeft, ptRight);
	}

	if(0 < m_aTabs.Length())
	{
		LONG xButtons = (m_szTabs.cx - m_xRightButtons);
		LONG yCenter = m_szTabs.cy / 2;

		DrawButton(graphics, xButtons, yCenter, m_hDropDown, m_szDropDown, m_fHoverDropDown);
		xButtons += m_szDropDown.cx + m_xButtonPadding * 2;

		DrawButton(graphics, xButtons, yCenter, m_hCloseIcon, m_szCloseButton, m_fHoverClose);
	}

	Gdiplus::Graphics gDevice(hdc);
	gDevice.DrawImage(m_pBuffer, 0, 0);
}

BOOL CTabs::IsDropDown (LONG x, LONG y)
{
	if(0 < m_aTabs.Length())
	{
		LONG xDropDown = (m_szTabs.cx - m_xRightButtons);
		LONG nPadding = (LONG)(m_xButtonPadding * 2.0f);
		if(x >= xDropDown && x < xDropDown + m_szDropDown.cx + nPadding)
		{
			LONG yDropDown = m_szTabs.cy / 2 - (m_szDropDown.cy + nPadding) / 2;
			return y >= yDropDown && y < yDropDown + m_szDropDown.cy + nPadding;
		}
	}
	return FALSE;
}

BOOL CTabs::IsCloseButton (LONG x, LONG y)
{
	if(0 < m_aTabs.Length())
	{
		LONG xCloseButton = (m_szTabs.cx - m_xRightButtons) + m_szDropDown.cx + m_xButtonPadding * 2;
		LONG nPadding = (LONG)(m_xButtonPadding * 2.0f);
		if(x >= xCloseButton && x < xCloseButton + m_szCloseButton.cx + nPadding)
		{
			LONG yCloseButton = m_szTabs.cy / 2 - (m_szCloseButton.cy + nPadding) / 2;
			return y >= yCloseButton && y < yCloseButton + m_szCloseButton.cy + nPadding;
		}
	}
	return FALSE;
}

PVOID CTabs::FindTabFromPoint (LONG x, LONG y)
{
	for(sysint i = m_cVisible - 1; i >= 0; i--)
	{
		CTab* pTab = m_aTabs[i];
		if(pTab->IsPointOver(this, x, y))
			return pTab->m_pvData;
	}

	return NULL;
}

BOOL CTabs::MouseHover (HWND hwnd, LONG x, LONG y)
{
	if(IsDropDown(x, y))
	{
		if(!m_fHoverDropDown)
		{
			ClearHoverFlags();
			m_fHoverDropDown = TRUE;
			return TRUE;
		}
	}
	else if(IsCloseButton(x, y))
	{
		if(!m_fHoverClose)
		{
			ClearHoverFlags();
			m_fHoverClose = TRUE;
			return TRUE;
		}
	}
	else
	{
		for(sysint i = m_cVisible - 1; i >= 0; i--)
		{
			CTab* pTab = m_aTabs[i];
			if(pTab->IsPointOver(this, x, y))
			{
				if(m_idxHover != i)
				{
					ClearHoverFlags();
					m_idxHover = i;
					return TRUE;
				}
				goto Cleanup;
			}
		}

		if(m_idxHover != -1 || m_fHoverDropDown || m_fHoverClose)
		{
			ClearHoverFlags();
			return TRUE;
		}
	}

Cleanup:
	return FALSE;
}

VOID CTabs::ClearHoverFlags (VOID)
{
	m_fHoverClose = FALSE;
	m_fHoverDropDown = FALSE;
	m_idxHover = -1;
}

BOOL CTabs::PopupTabs (HWND hwnd, LONG x, LONG y, __out sysint* pidxTab)
{
	BOOL fSuccess = FALSE;
	POINT pt = {x, y};
	ClientToScreen(hwnd, &pt);

	HMENU hMenu = CreatePopupMenu();
	if(hMenu)
	{
		TArray<CTab*> aSorted;
		MENUITEMINFOW mii = {0};

		for(sysint i = 0; i < m_aTabs.Length(); i++)
			aSorted.Append(m_aTabs[i]);
		Sorting::TQuickSortTArrayPtr(&aSorted, PopupMenuSorter, NULL);

		mii.cbSize = sizeof(MENUITEMINFOW);
		mii.fMask = MIIM_STRING | MIIM_ID;

		for(sysint i = 0; i < aSorted.Length(); i++)
		{
			mii.wID = static_cast<UINT>(i + 1);
			mii.dwTypeData = const_cast<PWSTR>(RStrToWide(aSorted[i]->m_rstrLabel));
			InsertMenuItemW(hMenu, static_cast<UINT>(i), TRUE, &mii);
		}

		UINT nCmd = TrackPopupMenu(hMenu, TPM_RIGHTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, pt.x, pt.y, 0, hwnd, NULL);
		if(0 < nCmd)
		{
			*pidxTab = FindTab(aSorted[nCmd - 1]->m_pvData);
			fSuccess = TRUE;
		}

		DestroyMenu(hMenu);
	}

	return fSuccess;
}

VOID CTabs::DrawButton (Gdiplus::Graphics& graphics, LONG xLeft, LONG yCenter, HICON hButton, const SIZE& szButton, BOOL fHover)
{
	if(fHover)
	{
		Gdiplus::Pen pnOutline(m_tabColors.crButtonOutline, 1.0f);
		Gdiplus::SolidBrush brBackground(m_tabColors.crButtonBackground);
		Gdiplus::RectF rcBackground((FLOAT)xLeft, (FLOAT)yCenter - (FLOAT)szButton.cy / 2.0f - m_xButtonPadding,
			(FLOAT)szButton.cx + m_xButtonPadding * 2.0f, (FLOAT)szButton.cy + m_xButtonPadding * 2.0f);
		graphics.FillRectangle(&brBackground, rcBackground);
		graphics.DrawRectangle(&pnOutline, rcBackground);
	}

	HDC hdc = graphics.GetHDC();
	DrawIconEx(hdc, xLeft + m_xButtonPadding, yCenter - szButton.cy / 2, hButton, szButton.cx, szButton.cy, 0, NULL, DI_NORMAL);
	graphics.ReleaseHDC(hdc);
}

VOID CTabs::CalculateVisibleTabs (VOID)
{
	LONG xTabs = m_szTabs.cx - m_xRightButtons, x = (LONG)m_xLabelOffset;

	m_cVisible = 0;

	for(sysint i = 0; i < m_aTabs.Length(); i++)
	{
		CTab* pTab = m_aTabs[i];
		LONG xNext = x + (LONG)pTab->m_pt[4].X;

		if(xNext > xTabs)
			break;

		pTab->m_xCalculated = (FLOAT)x;
		x = xNext + (LONG)(m_yNormalTab / 2.0f);
		m_cVisible++;
	}
}

HRESULT CTabs::CreateFont (HDC hdc, PCWSTR pcwzFamily, INT nPointSize, INT nWeight, __deref_out Gdiplus::Font** ppFont)
{
	HRESULT hr;
	LOGFONTW lf;
	ZeroMemory(&lf, sizeof(lf));

	lf.lfHeight = DPI::MapPointSize(nPointSize);
	lf.lfWeight = nWeight;
	lf.lfItalic = 0;
	lf.lfUnderline = 0;
	lf.lfStrikeOut = 0;
	lf.lfCharSet = DEFAULT_CHARSET;
	lf.lfQuality = GetPreferredFontQuality();
	lf.lfEscapement = 0;
	lf.lfOrientation = 0;
	Check(TStrCchCpy(lf.lfFaceName, ARRAYSIZE(lf.lfFaceName), pcwzFamily));

	*ppFont = __new Gdiplus::Font(hdc, &lf);
	CheckAlloc(*ppFont);

Cleanup:
	return hr;
}

BYTE CTabs::GetPreferredFontQuality (VOID)
{
	BOOL fSmoothed = FALSE;
	UINT uSmoothType = 0;
	BYTE bQuality = DEFAULT_QUALITY;

	SideAssert(SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &fSmoothed, FALSE));
	if(fSmoothed)
	{
		SideAssert(SystemParametersInfo(SPI_GETFONTSMOOTHINGTYPE, 0, &uSmoothType, FALSE));
		if(uSmoothType == FE_FONTSMOOTHINGCLEARTYPE)
		{
			bQuality = CLEARTYPE_NATURAL_QUALITY;
		}
	}

	return bQuality;
}

BOOL CTabs::GetIconDimensions (HICON hIcon, LONG& width, LONG& height)
{
	BOOL fSuccess = FALSE;
	ICONINFO iconInfo;
	if(GetIconInfo(hIcon, &iconInfo))
	{
		BITMAP bmp;
		GetObject(iconInfo.hbmColor, sizeof(BITMAP), &bmp);

		width = bmp.bmWidth;
		height = bmp.bmHeight;

		DeleteObject(iconInfo.hbmColor);
		DeleteObject(iconInfo.hbmMask);

		fSuccess = TRUE;
	}
	return fSuccess;
}

INT WINAPI CTabs::PopupMenuSorter (CTab** pplhItem, CTab** pprhItem, PVOID pParam)
{
	INT nResult;
	RStrCompareIRStr((*pplhItem)->m_rstrLabel, (*pprhItem)->m_rstrLabel, &nResult);
	return nResult;
}
