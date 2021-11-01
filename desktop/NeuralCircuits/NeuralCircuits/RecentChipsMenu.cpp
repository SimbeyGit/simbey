#include <windows.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Util\Registry.h"
#include "Library\MenuUtil.h"
#include "RecentChipsMenu.h"

CRecentChipsMenu::CRecentChipsMenu ()
{
	m_cRef = 1;

	m_hMenu = NULL;
	m_cRecentChips = 0;
	m_fMenuRegistered = FALSE;
	m_nCmdIdRangeStart = 0;
	m_lpszRegKey = NULL;
}

CRecentChipsMenu::~CRecentChipsMenu ()
{
	__delete_array m_lpszRegKey;

	for(INT i = 0; i < m_cRecentChips; i++)
		__delete_array m_lpszRecentChips[i];

	if(m_hMenu && !m_fMenuRegistered)
		DestroyMenu(m_hMenu);
}

HRESULT CRecentChipsMenu::Initialize (ULONG nCmdStart, LPCSTR lpcszRegKey)
{
	HRESULT hr;

	m_hMenu = CreatePopupMenu();
	if(m_hMenu)
	{
		m_nCmdIdRangeStart = nCmdStart;
		hr = S_OK;

		if(lpcszRegKey)
		{
			INT cchRegKey = lstrlen(lpcszRegKey);
			m_lpszRegKey = __new CHAR[cchRegKey + 1];
			if(m_lpszRegKey)
			{
				CopyMemory(m_lpszRegKey,lpcszRegKey,cchRegKey + 1);
				LoadFromRegistry();
			}
			else
				hr = E_OUTOFMEMORY;
		}
	}
	else
		hr = E_OUTOFMEMORY;

	return hr;
}

// IUnknown

HRESULT WINAPI CRecentChipsMenu::QueryInterface (REFIID iid, LPVOID* lplpvObject)
{
	HRESULT hr;

	if(lplpvObject)
	{
		if(iid == IID_IOleCommandTarget)
			*lplpvObject = (IOleCommandTarget*)this;
		else if(iid == IID_IUnknown)
			*lplpvObject = (IUnknown*)(IOleCommandTarget*)this;
		else
		{
			hr = E_NOINTERFACE;
			goto exit;
		}
		AddRef();
		hr = S_OK;
	}
	else
		hr = E_INVALIDARG;

exit:
	return hr;
}

ULONG WINAPI CRecentChipsMenu::AddRef (VOID)
{
	return (ULONG)InterlockedIncrement((LONG*)&m_cRef);
}

ULONG WINAPI CRecentChipsMenu::Release (VOID)
{
	ULONG c = (ULONG)InterlockedDecrement((LONG*)&m_cRef);
	if(c == 0)
		__delete this;
	return c;
}

// IOleCommandTarget

HRESULT WINAPI CRecentChipsMenu::QueryStatus (const GUID* lpguidCmdGroup, ULONG cCmds, OLECMD* lprgCmds, OLECMDTEXT* lpCmdText)
{
	UNREFERENCED_PARAMETER(lpguidCmdGroup);
	UNREFERENCED_PARAMETER(lpCmdText);

	for(ULONG i = 0; i < cCmds; i++)
	{
		if(lprgCmds[i].cmdf == 0)
		{
			switch(lprgCmds[i].cmdID)
			{
			case ID_RECENT_CHIPS:
				if(m_cRecentChips > 0)
					lprgCmds[i].cmdf = OLECMDF_SUPPORTED | OLECMDF_ENABLED;
				else
					lprgCmds[i].cmdf = OLECMDF_SUPPORTED;
				break;

			default:
				if(lprgCmds[i].cmdID >= m_nCmdIdRangeStart && lprgCmds[i].cmdID < m_nCmdIdRangeStart + m_cRecentChips)
					lprgCmds[i].cmdf = OLECMDF_SUPPORTED | OLECMDF_ENABLED;
				break;
			}
		}
	}

	return S_OK;
}

HRESULT WINAPI CRecentChipsMenu::Exec (const GUID* lpguidCmdGroup, DWORD nCmdID, DWORD nCmdExecOpt, VARIANTARG* lpvaIn, VARIANTARG* lpvaOut)
{
	UNREFERENCED_PARAMETER(lpguidCmdGroup);
	UNREFERENCED_PARAMETER(nCmdID);
	UNREFERENCED_PARAMETER(nCmdExecOpt);
	UNREFERENCED_PARAMETER(lpvaIn);
	UNREFERENCED_PARAMETER(lpvaOut);

	return OLECMDERR_E_NOTSUPPORTED;
}

VOID CRecentChipsMenu::UpdateMenu (HMENU hMenu)
{
	if(m_fMenuRegistered != (m_cRecentChips > 0))
	{
		INT nPos = MenuUtil::FindMenuItemPos(hMenu,ID_RECENT_CHIPS);
		if(nPos >= 0)
		{
			MENUITEMINFO mii = {0};
			mii.cbSize = sizeof(mii);
			mii.fMask = MIIM_SUBMENU;

			if(m_cRecentChips > 0)
				mii.hSubMenu = m_hMenu;

			if(SetMenuItemInfo(hMenu,nPos,TRUE,&mii))
				m_fMenuRegistered = !m_fMenuRegistered;
		}
	}
}

HRESULT CRecentChipsMenu::AddRecentItem (LPCSTR lpcszItem)
{
	HRESULT hr;

	hr = AddRecentItemInternal(lpcszItem);
	if(SUCCEEDED(hr))
		hr = SaveToRegistry();

	return hr;
}

HRESULT CRecentChipsMenu::ExecRecentItem (ULONG nCmdID, __out_ecount(cchMaxItem) PSTR pszItem, INT cchMaxItem, __out INT* pcchItem)
{
	HRESULT hr;

	if(nCmdID >= m_nCmdIdRangeStart && nCmdID < m_nCmdIdRangeStart + m_cRecentChips)
	{
		nCmdID -= m_nCmdIdRangeStart;

		INT cchItem = lstrlen(m_lpszRecentChips[nCmdID]);
		if(cchMaxItem > cchItem)
		{
			CopyMemory(pszItem, m_lpszRecentChips[nCmdID], cchItem + 1);
			hr = S_OK;
		}
		else
			hr = E_FAIL;

		if(pcchItem)
			*pcchItem = cchItem;
	}
	else
		hr = E_FAIL;

	return hr;
}

HRESULT CRecentChipsMenu::AddRecentItemInternal (LPCSTR lpcszItem)
{
	HRESULT hr;

	LPSTR lpszInsertPtr = NULL;

	for(INT i = 0; i < m_cRecentChips; i++)
	{
		if(lstrcmpi(m_lpszRecentChips[i],lpcszItem) == 0)
		{
			lpszInsertPtr = m_lpszRecentChips[i];
			MoveMemory(m_lpszRecentChips + i,m_lpszRecentChips + i + 1,sizeof(LPSTR) * (--m_cRecentChips - i));
			break;
		}
	}

	if(NULL == lpszInsertPtr)
	{
		INT cchItem = lstrlen(lpcszItem);
		lpszInsertPtr = __new CHAR[cchItem + 1];
		if(lpszInsertPtr)
		{
			CopyMemory(lpszInsertPtr,lpcszItem,cchItem + 1);
			hr = S_OK;
		}
		else
			hr = E_OUTOFMEMORY;
	}
	else
		hr = S_OK;

	if(S_OK == hr)
	{
		if(m_cRecentChips == MAX_RECENT_ITEMS)
			__delete_array m_lpszRecentChips[--m_cRecentChips];

		MoveMemory(m_lpszRecentChips + 1,m_lpszRecentChips,sizeof(LPSTR) * m_cRecentChips);
		m_lpszRecentChips[0] = lpszInsertPtr;
		m_cRecentChips++;

		hr = RebuildMenu();
	}

	return hr;
}

HRESULT CRecentChipsMenu::RebuildMenu (VOID)
{
	HRESULT hr;
	MENUITEMINFO mii = {0};
	LPSTR lpszPtr;
	CHAR szName[_MAX_PATH + 1];

	MenuUtil::ClearMenu(m_hMenu);
	hr = S_OK;

	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_ID | MIIM_STRING;
	mii.dwTypeData = szName;
	for(INT i = 0; i < m_cRecentChips; i++)
	{
		lpszPtr = strrchr(m_lpszRecentChips[i], '\\');
		if(lpszPtr)
			lstrcpy(szName,lpszPtr + 1);
		else
			lstrcpy(szName,m_lpszRecentChips[i]);

		lpszPtr = strrchr(szName, '.');
		if(lpszPtr)
			*lpszPtr = 0;

		mii.wID = m_nCmdIdRangeStart + i;
		if(FALSE == InsertMenuItem(m_hMenu,i,TRUE,&mii))
		{
			hr = E_FAIL;
			break;
		}
	}

	return hr;
}

HRESULT CRecentChipsMenu::LoadFromRegistry (VOID)
{
	HKEY hKey;
	HRESULT hr = Registry::CreateKey(HKEY_CURRENT_USER, m_lpszRegKey, KEY_READ, &hKey);
	if(SUCCEEDED(hr))
	{
		CHAR szName[16];
		CHAR szPath[_MAX_PATH];
		DWORD cbMaxValue;

		hr = S_FALSE;

		// Load the items in reverse order.
		for(INT i = MAX_RECENT_ITEMS; i > 0; i--)
		{
			wsprintf(szName,"Item%d",i);
			cbMaxValue = sizeof(szPath) - 1;
			if(ERROR_SUCCESS == RegQueryValueEx(hKey,szName,NULL,NULL,(LPBYTE)szPath,&cbMaxValue))
			{
				szPath[cbMaxValue] = 0;
				hr = AddRecentItemInternal(szPath);
				if(FAILED(hr))
					break;
			}
		}

		RegCloseKey(hKey);
	}

	return hr;
}

HRESULT CRecentChipsMenu::SaveToRegistry (VOID)
{
	HKEY hKey;
	HRESULT hr = Registry::CreateKey(HKEY_CURRENT_USER, m_lpszRegKey, KEY_WRITE, &hKey);
	if(SUCCEEDED(hr))
	{
		CHAR szName[16];

		hr = S_OK;
		for(INT i = 0; i < m_cRecentChips; i++)
		{
			wsprintf(szName,"Item%d",i + 1);
			if(ERROR_SUCCESS != RegSetValueEx(hKey,szName,NULL,REG_BINARY,(LPBYTE)m_lpszRecentChips[i],lstrlen(m_lpszRecentChips[i])))
			{
				hr = HRESULT_FROM_WIN32(GetLastError());
				break;
			}
		}

		RegCloseKey(hKey);
	}
	return hr;
}