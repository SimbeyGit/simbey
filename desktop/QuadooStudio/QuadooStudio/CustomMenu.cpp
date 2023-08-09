#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "CustomMenu.h"

void SetMenuItemTypes (HMENU hMenu, UINT nAddFlags, UINT nRemoveFlags)
{
	int itemCount = GetMenuItemCount(hMenu);

	for(int i = 0; i < itemCount; ++i)
	{
		MENUITEMINFO menuItemInfo = { sizeof(MENUITEMINFO) };
		menuItemInfo.fMask = MIIM_FTYPE;

		if(GetMenuItemInfo(hMenu, i, TRUE, &menuItemInfo))
		{
			menuItemInfo.fType = (menuItemInfo.fType | nAddFlags) & ~nRemoveFlags;

			SetMenuItemInfo(hMenu, i, TRUE, &menuItemInfo);

			HMENU hSubMenu = GetSubMenu(hMenu, i);
			if(hSubMenu)
				SetMenuItemTypes(hSubMenu, nAddFlags, nRemoveFlags);
		}
	}
}

CCustomMenu::CCustomMenu (HMENU hMenu) :
	m_hMenu(hMenu),
	m_hbrBackground(NULL)
{
}

CCustomMenu::~CCustomMenu ()
{
	ClearMenuMap();
	SafeDeleteGdiObject(m_hbrBackground);
}

HRESULT CCustomMenu::SetBackground (COLORREF crBackground)
{
	SafeDeleteGdiObject(m_hbrBackground);
	m_crBackground = crBackground;
	m_hbrBackground = CreateSolidBrush(crBackground);
	return m_hbrBackground ? S_OK : E_FAIL;
}

HRESULT CCustomMenu::SetDarkMode (VOID)
{
	m_crDivider = RGB(120, 120, 120);
	m_crNormalText = RGB(255, 255, 255);
	m_crSelectText = RGB(255, 255, 240);
	m_crDisabledText = RGB(128, 128, 128);
	m_crActiveBackground = RGB(80, 80, 80);
	return SetBackground(RGB(40, 40, 40));
}

HRESULT CCustomMenu::Rebuild (HMENU hMenu)
{
	ClearMenuMap();
	return BuildMenuMap(hMenu);
}

VOID CCustomMenu::OnAttached (IBaseWindow* lpWindow)
{
	MENUINFO menuInfo = { sizeof(MENUINFO) };
	menuInfo.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS;
	menuInfo.hbrBack = m_hbrBackground;
	SetMenuInfo(m_hMenu, &menuInfo);

	m_pWindow = lpWindow;
	SetMenuItemTypes(m_hMenu, MFT_OWNERDRAW, 0);
}

VOID CCustomMenu::OnDetached (IBaseWindow* lpWindow)
{
	MENUINFO menuInfo = { sizeof(MENUINFO) };
	menuInfo.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS;
	menuInfo.hbrBack = GetSysColorBrush(COLOR_MENU); // Default system background color

	Assert(lpWindow == m_pWindow);
	SetMenuItemTypes(m_hMenu, 0, MFT_OWNERDRAW);
	SetMenuInfo(m_hMenu, &menuInfo);
	m_pWindow = NULL;
}

BOOL CCustomMenu::OnSubclassMessage (IBaseWindow* lpWindow, UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	switch(message)
	{
	case WM_MEASUREITEM:
		{
			MEASUREITEMSTRUCT* pmis = (MEASUREITEMSTRUCT*)lParam;
			if(pmis->CtlType == ODT_MENU)
			{
				RSTRING rstrLabel;
				HWND hwnd;

				SIZE size = { pmis->itemWidth, pmis->itemHeight };
				SideAssertHr(m_pWindow->GetWindow(&hwnd));
				HDC hdc = GetDC(hwnd);

				if(SUCCEEDED(m_mapMenu.Find(pmis->itemID, &rstrLabel)))
				{
					GetTextExtentPoint32(hdc, RStrToWide(rstrLabel), RStrLen(rstrLabel), &size);
					size.cx += size.cy + 2;
				}
				else
				{
					WCHAR wzLabel[256];

					INT cch = GetMenuString(m_hMenu, pmis->itemID, wzLabel, ARRAYSIZE(wzLabel), MF_BYCOMMAND);
					if(0 < cch)
						GetTextExtentPoint32(hdc, wzLabel, cch, &size);
					else
						size.cy = 4;
				}

				pmis->itemWidth = size.cx + 2;
				pmis->itemHeight = size.cy + 2;
				ReleaseDC(hwnd, hdc);

				lResult = TRUE;
				return TRUE;
			}
		}
		break;

	case WM_DRAWITEM:
		{
			DRAWITEMSTRUCT* pdis = (DRAWITEMSTRUCT*)lParam;
			if(pdis->CtlType == ODT_MENU)
			{
				RECT rc = pdis->rcItem;
				WCHAR wzItem[256];
				INT cchItem;
				HBRUSH hBrush;

				if(pdis->itemState & ODS_SELECTED)
					hBrush = CreateSolidBrush(m_crActiveBackground);
				else
					hBrush = CreateSolidBrush(m_crBackground);

				FillRect(pdis->hDC, &pdis->rcItem, hBrush);
				DeleteObject(hBrush);

				cchItem = GetMenuString((HMENU)pdis->hwndItem, pdis->itemID, wzItem, ARRAYSIZE(wzItem), MF_BYCOMMAND);

				if(0 < cchItem)
				{
					UINT nFormat = DT_VCENTER | DT_SINGLELINE;

					// Draw the menu item text
					if(pdis->itemState & ODS_DISABLED)
						SetTextColor(pdis->hDC, m_crDisabledText);
					else if(pdis->itemState & ODS_SELECTED)
						SetTextColor(pdis->hDC, m_crSelectText);
					else
						SetTextColor(pdis->hDC, m_crNormalText);
					INT nPrevMode = SetBkMode(pdis->hDC, TRANSPARENT);

					if(m_mapMenu.HasItem(pdis->itemID))
					{
						rc.left++;
						rc.right--;

						if(pdis->itemState & ODS_CHECKED)
						{
							RECT rcCheck = rc;
							rcCheck.right = rcCheck.left + (rc.bottom - rc.top);

							TEXTMETRIC tm;
							GetTextMetrics(pdis->hDC, &tm);

							HFONT hfPrevious = (HFONT)SelectObject(pdis->hDC, CreateFont(tm.tmHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
								SYMBOL_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
								DEFAULT_QUALITY, DEFAULT_PITCH, L"Wingdings"));

							WCHAR wch = 0xFC;
							DrawText(pdis->hDC, &wch, 1, &rcCheck, DT_VCENTER | DT_SINGLELINE | DT_CENTER);
							DeleteObject(SelectObject(pdis->hDC, hfPrevious));
						}

						rc.left += (rc.bottom - rc.top) + 2;
					}
					else
						nFormat |= DT_CENTER;

					PWSTR pwzBreak = const_cast<PWSTR>(TStrCchRChr(wzItem, cchItem, L'\t'));
					if(pwzBreak)
					{
						INT cchTemp = static_cast<INT>(pwzBreak - wzItem);
						INT cchShortcut = (cchItem - cchTemp) - 1;
						cchItem = cchTemp;
						pwzBreak++;

						RECT rcShortcut = rc;
						rcShortcut.right -= 10;
						DrawText(pdis->hDC, pwzBreak, cchShortcut, &rcShortcut, DT_VCENTER | DT_SINGLELINE | DT_RIGHT);
					}

					DrawText(pdis->hDC, wzItem, cchItem, &rc, nFormat);
					SetBkMode(pdis->hDC, nPrevMode);
				}
				else
				{
					rc.left += 10;
					rc.right -= 10;

					while(rc.bottom - rc.top > 3)
					{
						rc.top++;
						rc.bottom--;
					}

					hBrush = CreateSolidBrush(m_crDivider);
					FillRect(pdis->hDC, &rc, hBrush);
					DeleteObject(hBrush);
				}

				lResult = TRUE;
				return TRUE;
			}
		}
		break;
	}

	return FALSE;
}

VOID CCustomMenu::ClearMenuMap (VOID)
{
	for(sysint i = 0; i < m_mapMenu.Length(); i++)
		RStrRelease(*m_mapMenu.GetValuePtr(i));
	m_mapMenu.Clear();
}

HRESULT CCustomMenu::BuildMenuMap (HMENU hMenu)
{
	HRESULT hr = S_FALSE;
	RSTRING rstrLabel = NULL;
	int itemCount = GetMenuItemCount(hMenu);

	for(int i = 0; i < itemCount; i++)
	{
		MENUITEMINFO menuItemInfo = { sizeof(MENUITEMINFO) };
		menuItemInfo.fMask = MIIM_STRING;

		if(GetMenuItemInfo(hMenu, i, TRUE, &menuItemInfo) && 0 < menuItemInfo.cch)
		{
			Check(RStrAllocW(++menuItemInfo.cch, &rstrLabel, &menuItemInfo.dwTypeData));

			menuItemInfo.fMask = MIIM_STRING | MIIM_ID;
			GetMenuItemInfo(hMenu, i, TRUE, &menuItemInfo);

			Check(m_mapMenu.Add(menuItemInfo.wID, rstrLabel));
			rstrLabel = NULL;

			HMENU hSubMenu = GetSubMenu(hMenu, i);
			if(hSubMenu)
				BuildMenuMap(hSubMenu);
		}
	}

Cleanup:
	RStrRelease(rstrLabel);
	return hr;
}
