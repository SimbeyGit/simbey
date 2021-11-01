#include <windows.h>
#include "Core\CoreDefs.h"
#include "core\StringCore.h"
#include "MenuUtil.h"

typedef struct
{
	UINT fType;
	CHAR szText[128];
	INT cText;
	HBITMAP hbmpItem;
} MENUITEMDETAILS, *LPMENUITEMDETAILS;

namespace MenuUtil
{
	HMENU WINAPI RebuildPopupMenu (const GUID* lpCmdGroup, HMENU hMenu, IOleCommandTarget* lpTarget)
	{
		HMENU hNew = CreatePopupMenu();
		if(hNew)
		{
			INT cItems = GetMenuItemCount(hMenu);
			if(cItems > 0)
			{
				OLECMD* lpCmdList = __new OLECMD[cItems];
				LPMENUITEMDETAILS lpDetails = __new MENUITEMDETAILS[cItems];
				if(lpCmdList && lpDetails)
				{
					INT i, w = 0;
					MENUITEMINFO2 mii;

					mii.cbSize = sizeof(MENUITEMINFO2);

					for(i = 0; i < cItems; i++)
					{
						mii.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STRING | MIIM_BITMAP;
						mii.cch = 256;
						mii.dwTypeData = lpDetails[w].szText;
						if(GetMenuItemInfo(hMenu,i,TRUE,(MENUITEMINFO*)&mii))
						{
							if(mii.wID == 0)
								lpCmdList[w].cmdf = OLECMDF_SUPPORTED | OLECMDF_ENABLED;
							else
								lpCmdList[w].cmdf = 0;
							lpCmdList[w].cmdID = mii.wID;

							lpDetails[w].fType = mii.fType;
							lpDetails[w].cText = mii.cch;
							lpDetails[w].hbmpItem = mii.hbmpItem;
							w++;
						}
					}

					if(w > 0 && SUCCEEDED(lpTarget->QueryStatus(lpCmdGroup,w,lpCmdList,NULL)))
					{
						for(i = 0; i < w; i++)
						{
							if(lpCmdList[i].cmdf & OLECMDF_SUPPORTED)
							{
								mii.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STATE;
								mii.fType = lpDetails[i].fType;
								mii.wID = lpCmdList[i].cmdID;
								if((lpDetails[w].fType & MFT_STRING) == MFT_STRING)
								{
									mii.fMask |= MIIM_STRING;
									mii.cch = lpDetails[i].cText;
									mii.dwTypeData = lpDetails[i].szText;
								}
								if((lpDetails[w].fType & MFT_BITMAP) == MFT_BITMAP)
								{
									mii.fMask |= MIIM_BITMAP;
									mii.hbmpItem = lpDetails[i].hbmpItem;
								}
								if(lpCmdList[i].cmdf & OLECMDF_ENABLED)
									mii.fState = MFS_ENABLED;
								else
									mii.fState = MFS_GRAYED;
								if(lpCmdList[i].cmdf & OLECMDF_LATCHED)
									mii.fState |= MFS_CHECKED;
								InsertMenuItem(hNew,i,TRUE,(MENUITEMINFO*)&mii);
							}
						}
					}
				}
				__delete_array lpDetails;
				__delete_array lpCmdList;
			}
		}
		return hNew;
	}

	HRESULT WINAPI EnableMenuItems (const GUID* lpCmdGroup, HMENU hMenu, IOleCommandTarget* lpCmdTarget, __out_opt INT* pcEnabledItems)
	{
		TArray<OLECMD> aCommands;
		TArray<HMENU> aSubMenus;
		INT cEnabledItems = 0;
		HRESULT hr = BuildMenuItemList(hMenu, aCommands, aSubMenus);
		if(SUCCEEDED(hr))
		{
			sysint cCmds;
			OLECMD* lprgCmds;
			aCommands.GetData(&lprgCmds, &cCmds);

			hr = lpCmdTarget->QueryStatus(lpCmdGroup,static_cast<ULONG>(cCmds),lprgCmds,NULL);
			if(SUCCEEDED(hr))
			{
				MENUITEMINFO2 mii;
				INT i;

				ZeroMemory(&mii,sizeof(MENUITEMINFO2));
				mii.cbSize = sizeof(MENUITEMINFO2);
				mii.fMask = MIIM_STATE;

				for(i = 0; i < cCmds; i++)
				{
					if(lprgCmds[i].cmdf & OLECMDF_ENABLED)
					{
						mii.fState = MFS_ENABLED;
						cEnabledItems++;
					}
					else
						mii.fState = MFS_GRAYED;

					if(lprgCmds[i].cmdf & OLECMDF_LATCHED)
						mii.fState |= MFS_CHECKED;

					SetMenuItemInfo(hMenu,lprgCmds[i].cmdID,FALSE,(MENUITEMINFO*)&mii);

					if(lprgCmds[i].cmdf & OLECMDF_NINCHED)
						CheckMenuRadioItem(hMenu,lprgCmds[i].cmdID,lprgCmds[i].cmdID,lprgCmds[i].cmdID,MF_BYCOMMAND);
				}

				for(i = 0; i < aSubMenus.Length(); i++)
				{
					INT cEnabledSubMenuItems;
					hr = EnableMenuItems(lpCmdGroup, aSubMenus[i], lpCmdTarget, &cEnabledSubMenuItems);
					if(FAILED(hr))
						break;
					if(0 < cEnabledSubMenuItems)
					{
						mii.fState = MFS_ENABLED;
						cEnabledItems++;
					}
					else
						mii.fState = MFS_GRAYED;

					SetMenuItemInfo(hMenu,FindSubMenuPos(hMenu,aSubMenus[i]),TRUE,(MENUITEMINFO*)&mii);
				}
			}
		}
		if(pcEnabledItems)
			*pcEnabledItems = cEnabledItems;
		return hr;
	}

	HRESULT WINAPI BuildMenuItemList (HMENU hMenu, __out TArray<OLECMD>& aCommands, __out TArray<HMENU>& aSubMenus)
	{
		HRESULT hr = S_FALSE;
		ULONG cItems = GetMenuItemCount(hMenu);
		MENUITEMINFO2 mii;

		ZeroMemory(&mii, sizeof(MENUITEMINFO2));
		mii.cbSize = sizeof(MENUITEMINFO2);
		mii.fMask = MIIM_ID | MIIM_SUBMENU;

		for(ULONG i = 0; i < cItems; i++)
		{
			if(GetMenuItemInfo(hMenu,i,TRUE,(MENUITEMINFO*)&mii))
			{
				if(mii.hSubMenu)
				{
					hr = aSubMenus.Append(mii.hSubMenu);
					if(FAILED(hr))
						break;
				}
				else if(mii.wID != -1 && mii.wID != 0)
				{
					OLECMD cmd = { mii.wID, 0 };
					hr = aCommands.Append(cmd);
					if(FAILED(hr))
						break;
				}
			}
		}

		return hr;
	}

	INT WINAPI FindMenuItemPos (HMENU hMenu, UINT wID)
	{
		INT nPos = -1;
		INT cItems = GetMenuItemCount(hMenu);
		for(INT i = 0; i < cItems; i++)
		{
			if(GetMenuItemID(hMenu,i) == wID)
			{
				nPos = i;
				break;
			}
		}
		return nPos;
	}

	INT WINAPI FindSubMenuPos (HMENU hMenu, HMENU hSubMenu)
	{
		INT nPos = -1;
		INT cItems = GetMenuItemCount(hMenu);
		for(INT i = 0; i < cItems; i++)
		{
			if(GetSubMenu(hMenu, i) == hSubMenu)
			{
				nPos = i;
				break;
			}
		}
		return nPos;
	}

	HRESULT WINAPI DupMenu (HMENU hMenu, HMENU* lphDup)
	{
		HRESULT hr;

		Assert(lphDup);

		if(hMenu)
		{
			HMENU hDup = CreateMenu();
			if(hDup)
			{
				MENUITEMINFO mii = {0};
				TCHAR tzItem[128];
				INT cItems = GetMenuItemCount(hMenu);

				hr = S_OK;

				mii.cbSize = sizeof(MENUITEMINFO);
				for(INT n = 0; n < cItems; n++)
				{
					BOOL fSuccess = TRUE;

					mii.fMask = MIIM_ID | MIIM_STATE | MIIM_SUBMENU | MIIM_TYPE;
					mii.dwTypeData = tzItem;
					mii.cch = ARRAYSIZE(tzItem);
					mii.fType = 0;

					if(GetMenuItemInfo(hMenu,n,TRUE,&mii))
					{
						if(mii.hSubMenu)
						{
							hr = DupMenu(mii.hSubMenu,&mii.hSubMenu);
							if(FAILED(hr))
								break;
						}
						else if((mii.fState & MF_SEPARATOR) == 0)
							mii.fMask |= MIIM_STRING;
						fSuccess = InsertMenuItem(hDup,n,TRUE,&mii);
					}
					else
						fSuccess = FALSE;

					if(!fSuccess)
					{
						DestroyMenu(hDup);
						hDup = NULL;
						hr = E_FAIL;
						break;
					}
				}
			}
			else
				hr = E_OUTOFMEMORY;

			*lphDup = hDup;
		}
		else
		{
			*lphDup = NULL;
			hr = S_FALSE;
		}

		return hr;
	}

	VOID WINAPI ClearMenu (HMENU hMenu)
	{
		INT cItems = GetMenuItemCount(hMenu);
		for(INT i = 0; i < cItems; i++)
		{
			SideAssert(RemoveMenu(hMenu,0,MF_BYPOSITION));
		}
	}

	HMENU WINAPI LoadPopupMenu (HINSTANCE hInstance, UINT id)
	{
		HMENU hMenuPopup = NULL;
		HMENU hMenuParent = ::LoadMenu(hInstance, MAKEINTRESOURCE(id));
		if(hMenuParent)
		{
			hMenuPopup = ::GetSubMenu(hMenuParent, 0);
			::RemoveMenu(hMenuParent,0,MF_BYPOSITION);
			::DestroyMenu(hMenuParent);
		}
		return hMenuPopup;
	}

	HMENU WINAPI LoadSubPopupMenu (HINSTANCE hInstance, UINT id, const TCHAR* pctzLabel)
	{
		HMENU hMenuPopup = NULL;
		HMENU hMenuParent = ::LoadMenu(hInstance, MAKEINTRESOURCE(id));
		if(hMenuParent)
		{
			MENUITEMINFO mii = {0};
			TCHAR tzItem[128];
			INT cItems = GetMenuItemCount(hMenuParent);

			mii.cbSize = sizeof(MENUITEMINFO);

			for(INT i = 0; i < cItems; i++)
			{
				mii.fMask = MIIM_ID | MIIM_STATE | MIIM_SUBMENU | MIIM_TYPE;
				mii.dwTypeData = tzItem;
				mii.cch = ARRAYSIZE(tzItem);
				mii.fType = 0;

				if(GetMenuItemInfo(hMenuParent, i, TRUE, &mii))
				{
					if(mii.hSubMenu && 0 == TStrCmpAssert(tzItem, pctzLabel))
					{
						hMenuPopup = ::GetSubMenu(hMenuParent, i);
						::RemoveMenu(hMenuParent,i,MF_BYPOSITION);
						break;
					}
				}
			}

			::DestroyMenu(hMenuParent);
		}
		return hMenuPopup;
	}

	HRESULT WINAPI AddBitmapToMenuItem (HMENU hMenu, INT iItem, BOOL fByPosition, HBITMAP hBitmap)
	{
		HRESULT hr;

		MENUITEMINFO mii = { sizeof(mii) };
		mii.fMask = MIIM_BITMAP;
		mii.hbmpItem = hBitmap;
		CheckIfGetLastError(!SetMenuItemInfo(hMenu, iItem, fByPosition, &mii));
		hr = S_OK;

	Cleanup:
		return hr;
	}

	BOOL WINAPI IsMenuItemSeparator (HMENU hMenu, INT nPos)
	{
		MENUITEMINFO mii = { sizeof (mii), MIIM_TYPE };
		GetMenuItemInfo (hMenu, nPos, MF_BYPOSITION, &mii);

		return ((mii.fType & MFT_SEPARATOR) != 0);
	}

	UINT WINAPI GetFirstSubMenuParentPos (HMENU hMenu, INT nFirstPos/*=0*/)
	{
		UINT uiPos = (UINT) -1;
		MENUITEMINFO info;
		::ZeroMemory(&info, sizeof(MENUITEMINFO));
		info.cbSize = sizeof (MENUITEMINFO);    // must fill up this field
		info.fMask = MIIM_SUBMENU;                 // get the state of the menu item
		INT iItem = nFirstPos;
		INT iMax = GetMenuItemCount(hMenu);
		while((iItem < iMax) && GetMenuItemInfo(hMenu, iItem, TRUE, &info))
		{
			if(info.hSubMenu)
			{
				uiPos = iItem;
				break;
			}
			iItem++;
		}

		return uiPos;
	}

	INT RemoveExtraneousSeparators (HMENU hMenu)
	{
		INT cRemovedSeparators = 0;

		/*
		 * remove leading separators
		 */
		while(IsMenuItemSeparator(hMenu, 0))
		{
			DeleteMenu (hMenu, 0, MF_BYPOSITION);
			cRemovedSeparators++;
		}

		/*
		 * remove trailing separators
		 */
		INT iLast = GetMenuItemCount(hMenu)-1;
		for( ; (iLast >= 0) && IsMenuItemSeparator(hMenu, iLast); iLast--)
		{
			DeleteMenu (hMenu, iLast, MF_BYPOSITION);
			cRemovedSeparators++;
		}

		/*
		 * remove internal consecutive separators
		 */
		BOOL fPreviousItemIsSeparator = FALSE;
		for(INT i = iLast; i >= 0; i--)
		{
			if(IsMenuItemSeparator(hMenu, i))
			{
				// If the previous entry is a separator then this is a duplicate so we need to remove
				// the current entry
				if(fPreviousItemIsSeparator)
				{
					DeleteMenu(hMenu, i, MF_BYPOSITION);
					cRemovedSeparators++;
				}
				fPreviousItemIsSeparator = TRUE;
			}
			else
			{
				fPreviousItemIsSeparator = FALSE;
			}
		}

		/*
		 * RemoveExtraneousSeparators recursivly on submenus
		 */
		MENUITEMINFO mii;
		INT iPos = GetFirstSubMenuParentPos(hMenu, 0);
		while(iPos != -1)
		{
			ZeroMemory(&mii, sizeof(mii));
			mii.cbSize = sizeof(mii);
			mii.fMask = MIIM_SUBMENU;
			if(GetMenuItemInfo(hMenu, iPos, MF_BYPOSITION, &mii))
			{
				Assert(mii.hSubMenu != NULL);
				if(mii.hSubMenu != NULL)
				{
					cRemovedSeparators += RemoveExtraneousSeparators(mii.hSubMenu);
				}
			}
			iPos = GetFirstSubMenuParentPos(hMenu, iPos+1);
		}

		return cRemovedSeparators;
	}
}
