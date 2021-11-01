#ifndef	_H_MENUUTIL
#define	_H_MENUUTIL

#include <docobj.h>
#include "core\Array.h"

#define MIIM_STRING		0x00000040
#define MIIM_BITMAP		0x00000080
#define MIIM_FTYPE		0x00000100

typedef struct MENUITEMINFO2
{
	UINT	cbSize;
	UINT	fMask;
	UINT	fType;			// used if MIIM_TYPE (4.0) or MIIM_FTYPE (>4.0)
	UINT	fState;			// used if MIIM_STATE
	UINT	wID;			// used if MIIM_ID
	HMENU   hSubMenu;		// used if MIIM_SUBMENU
	HBITMAP hbmpChecked;	// used if MIIM_CHECKMARKS
	HBITMAP hbmpUnchecked;	// used if MIIM_CHECKMARKS
	DWORD   dwItemData;		// used if MIIM_DATA
	LPSTR   dwTypeData;		// used if MIIM_TYPE (4.0) or MIIM_STRING (>4.0)
	UINT	cch;			// used if MIIM_TYPE (4.0) or MIIM_STRING (>4.0)
	HBITMAP hbmpItem;		// used if MIIM_BITMAP
} MENUITEMINFO2, *LPMENUITEMINFO2;

namespace MenuUtil
{
	HMENU WINAPI RebuildPopupMenu (const GUID* lpCmdGroup, HMENU hMenu, IOleCommandTarget* lpTarget);
	HRESULT WINAPI EnableMenuItems (const GUID* lpCmdGroup, HMENU hMenu, IOleCommandTarget* lpCmdTarget, __out_opt INT* pcEnabledItems = NULL);
	HRESULT WINAPI BuildMenuItemList (HMENU hMenu, __out TArray<OLECMD>& aCommands, __out TArray<HMENU>& aSubMenus);
	INT WINAPI FindMenuItemPos (HMENU hMenu, UINT wID);
	INT WINAPI FindSubMenuPos (HMENU hMenu, HMENU hSubMenu);
	HRESULT WINAPI DupMenu (HMENU hMenu, HMENU* lphDup);
	VOID WINAPI ClearMenu (HMENU hMenu);
	HMENU WINAPI LoadPopupMenu (HINSTANCE hInstance, UINT id);
	HMENU WINAPI LoadSubPopupMenu (HINSTANCE hInstance, UINT id, const TCHAR* pctzLabel);
	HRESULT WINAPI AddBitmapToMenuItem (HMENU hMenu, INT iItem, BOOL fByPosition, HBITMAP hBitmap);
	BOOL WINAPI IsMenuItemSeparator (HMENU hMenu, INT nPos);
	UINT WINAPI GetFirstSubMenuParentPos (HMENU hMenu, INT nFirstPos = 0);
	INT RemoveExtraneousSeparators (HMENU hMenu);
};

#endif