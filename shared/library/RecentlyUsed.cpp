#include <windows.h>
#include <sddl.h>
#include "Core\StringCore.h"
#include "RecentlyUsed.h"

namespace
{
	const DWORD c_maxPathChars = 32768;
	const WCHAR c_valueName[] = L"RecentlyUsed0";

	HRESULT CreateRecentMutex (HANDLE* phMutex)
	{
		HRESULT hr;
		HANDLE hToken = NULL;
		DWORD cbToken;
		PWSTR pwzSid = NULL;
		CRString name;
		union
		{
			TOKEN_USER user;
			BYTE buffer[sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE];
		} token;

		// All recent-list writes for this user share a lock, including across sessions.
		CheckIfGetLastError(!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken));
		CheckIfGetLastError(!GetTokenInformation(hToken, TokenUser, token.buffer, sizeof(token.buffer), &cbToken));
		CheckIfGetLastError(!ConvertSidToStringSidW(token.user.User.Sid, &pwzSid));
		Check(RStrFormatW(&name, L"Global\\Simbey.RecentlyUsed.%ls", pwzSid));
		*phMutex = CreateMutexW(NULL, FALSE, name.c_wstr());
		CheckIfGetLastError(NULL == *phMutex);
		hr = S_OK;

	Cleanup:
		if(pwzSid)
			LocalFree(pwzSid);
		if(hToken)
			CloseHandle(hToken);
		return hr;
	}

	UINT FindPath (const CRString* paths, const CRString& path)
	{
		for(UINT i = 0; i < CRecentlyUsed::MaxItems && paths[i]; i++)
		{
			if(CSTR_EQUAL == CompareStringOrdinal(paths[i].c_wstr(), paths[i].Length(), path.c_wstr(), path.Length(), TRUE))
				return i;
		}
		return CRecentlyUsed::MaxItems;
	}

	HRESULT ReadPaths (HKEY hKey, CRString* paths)
	{
		HRESULT hr = S_OK;
		WCHAR wzValue[ARRAYSIZE(c_valueName)];
		CopyMemory(wzValue, c_valueName, sizeof(wzValue));

		for(UINT i = 0, count = 0; i < CRecentlyUsed::MaxItems; i++)
		{
			DWORD type, cbValue, cbRead;
			PWSTR pwzPath;
			CRString path;
			wzValue[ARRAYSIZE(wzValue) - 2] = static_cast<WCHAR>(L'0' + i);
			LONG result = RegQueryValueExW(hKey, wzValue, NULL, &type, NULL, &cbValue);
			if(ERROR_FILE_NOT_FOUND == result)
				continue;
			CheckWin32Error(result);
			if(REG_SZ != type || cbValue <= sizeof(WCHAR) || cbValue % sizeof(WCHAR) || cbValue > c_maxPathChars * sizeof(WCHAR))
				continue;

			INT cchPath = cbValue / sizeof(WCHAR) - 1;
			Check(RStrAllocW(cchPath, &path, &pwzPath));
			cbRead = cbValue;
			result = RegQueryValueExW(hKey, wzValue, NULL, &type, reinterpret_cast<PBYTE>(pwzPath), &cbRead);
			if(ERROR_FILE_NOT_FOUND == result || ERROR_MORE_DATA == result)
				continue;
			CheckWin32Error(result);
			if(REG_SZ == type && cbRead == cbValue && L'\0' == pwzPath[cchPath] &&
				!TStrCchChr(pwzPath, cchPath, L'\0') && CRecentlyUsed::MaxItems == FindPath(paths, path))
			{
				paths[count] = path;
				count++;
			}
		}

	Cleanup:
		return hr;
	}

	HRESULT WritePaths (HKEY hKey, const CRString* paths)
	{
		HRESULT hr = S_OK;
		WCHAR wzValue[ARRAYSIZE(c_valueName)];
		CopyMemory(wzValue, c_valueName, sizeof(wzValue));

		for(INT i = CRecentlyUsed::MaxItems - 1; i >= 0; i--)
		{
			const CRString& path = paths[i];
			wzValue[ARRAYSIZE(wzValue) - 2] = static_cast<WCHAR>(L'0' + i);
			if(path)
			{
				DWORD cbPath = (path.Length() + 1) * sizeof(WCHAR);
				CheckWin32Error(RegSetValueExW(hKey, wzValue, 0, REG_SZ, reinterpret_cast<const BYTE*>(path.c_wstr()), cbPath));
			}
			else
			{
				LONG result = RegDeleteValueW(hKey, wzValue);
				if(ERROR_FILE_NOT_FOUND != result)
				{
					CheckWin32Error(result);
				}
			}
		}

	Cleanup:
		return hr;
	}
}

CRecentlyUsed::CRecentlyUsed () :
	m_hMutex(NULL), m_pOpen(NULL)
{
}

CRecentlyUsed::~CRecentlyUsed ()
{
	if(m_hMutex)
		CloseHandle(m_hMutex);
}

HRESULT CRecentlyUsed::Initialize (PCWSTR pcwzAppKey, IOpenRecentlyUsed* pOpen)
{
	HRESULT hr;
	bool locked = false;

	CheckIf(!pcwzAppKey || !*pcwzAppKey || !pOpen, E_INVALIDARG);
	CheckIf(m_key, HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED));
	m_pOpen = pOpen;
	Check(RStrFormatW(&m_key, L"%ls\\Recent", pcwzAppKey));
	Check(Lock());
	locked = true;
	Check(Reload());

Cleanup:
	if(locked)
		ReleaseMutex(m_hMutex);
	return hr;
}

HRESULT CRecentlyUsed::Lock ()
{
	HRESULT hr;
	DWORD wait;

	if(!m_hMutex)
	{
		Check(CreateRecentMutex(&m_hMutex));
	}
	wait = WaitForSingleObject(m_hMutex, 2000);
	CheckIfGetLastError(WAIT_FAILED == wait);
	CheckIf(WAIT_TIMEOUT == wait, HRESULT_FROM_WIN32(ERROR_TIMEOUT));
	hr = S_OK;

Cleanup:
	return hr;
}

HRESULT CRecentlyUsed::Reload ()
{
	HRESULT hr;
	HKEY hKey = NULL;
	CRString paths[MaxItems];
	LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, m_key.c_wstr(), 0, KEY_QUERY_VALUE, &hKey);

	if(ERROR_FILE_NOT_FOUND != result && ERROR_PATH_NOT_FOUND != result)
	{
		CheckWin32Error(result);
		Check(ReadPaths(hKey, paths));
	}
	for(UINT i = 0; i < MaxItems; i++)
		m_paths[i] = paths[i];
	hr = S_OK;

Cleanup:
	if(hKey)
		RegCloseKey(hKey);
	return hr;
}

HRESULT CRecentlyUsed::RecordOpen (PCWSTR pcwzPath)
{
	HRESULT hr;
	HKEY hKey = NULL;
	bool locked = false;
	CRString paths[MaxItems];
	CRString fullPath;
	PWSTR pwzFullPath;
	DWORD cchPath, cchWritten;
	UINT previous;

	CheckIf(!m_key, E_UNEXPECTED);
	CheckIf(!pcwzPath || !*pcwzPath, E_INVALIDARG);
	cchPath = GetFullPathNameW(pcwzPath, 0, NULL, NULL);
	CheckIfGetLastError(0 == cchPath);
	CheckIf(cchPath > c_maxPathChars, HRESULT_FROM_WIN32(ERROR_FILENAME_EXCED_RANGE));
	Check(RStrAllocW(cchPath - 1, &fullPath, &pwzFullPath));
	cchWritten = GetFullPathNameW(pcwzPath, cchPath, pwzFullPath, NULL);
	CheckIfGetLastError(0 == cchWritten);
	CheckIf(cchWritten >= cchPath, HRESULT_FROM_WIN32(ERROR_RETRY));
	if(cchWritten == cchPath - 1)
		paths[0] = fullPath;
	else
	{
		Check(RStrCreateW(cchWritten, pwzFullPath, &paths[0]));
	}

	Check(Lock());
	locked = true;
	Check(Reload());
	previous = FindPath(m_paths, paths[0]);
	for(UINT src = 0, dest = 1; dest < MaxItems && m_paths[src]; src++)
	{
		if(src != previous)
		{
			paths[dest] = m_paths[src];
			dest++;
		}
	}

	CheckWin32Error(RegCreateKeyExW(HKEY_CURRENT_USER, m_key.c_wstr(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hKey, NULL));
	if(FAILED(hr = WritePaths(hKey, paths)))
	{
		// A multi-value write can fail partway through. Reflect the values that survived.
		Reload();
		goto Cleanup;
	}
	for(UINT i = 0; i < MaxItems; i++)
		m_paths[i] = paths[i];

Cleanup:
	if(hKey)
		RegCloseKey(hKey);
	if(locked)
		ReleaseMutex(m_hMutex);
	return hr;
}

HRESULT CRecentlyUsed::OpenSelection (RSTRING rstrPath)
{
	HRESULT hr;
	CRString path(rstrPath);

	CheckIf(!m_pOpen, E_UNEXPECTED);
	CheckIf(!path, E_INVALIDARG);
	RecordOpen(path.c_wstr());
	hr = m_pOpen->OpenRecentlyUsed(path.c_wstr());

Cleanup:
	return hr;
}

UINT CRecentlyUsed::Count () const
{
	UINT count = 0;
	while(count < MaxItems && m_paths[count])
		count++;
	return count;
}

RSTRING CRecentlyUsed::PathAt (UINT index) const
{
	return index < MaxItems ? static_cast<RSTRING>(m_paths[index]) : NULL;
}

CRecentlyUsedMenu::CRecentlyUsedMenu (CRecentlyUsed& list, UINT firstCommand) :
	m_list(list), m_firstCommand(firstCommand)
{
	Assert(0 < firstCommand && firstCommand <= 0xffff - (CRecentlyUsed::MaxItems - 1));
}

bool CRecentlyUsedMenu::Handles (DWORD command) const
{
	return command - m_firstCommand < CRecentlyUsed::MaxItems;
}

HRESULT CRecentlyUsedMenu::Populate (HMENU hMenu, bool ownerDraw)
{
	HRESULT hr = S_OK;
	INT count = GetMenuItemCount(hMenu);

	CheckIfGetLastError(count < 0);
	while(count > 0)
	{
		count--;
		CheckIfGetLastError(!DeleteMenu(hMenu, count, MF_BYPOSITION));
		if(count < CRecentlyUsed::MaxItems)
			m_displayed[count].Clear();
	}

	for(UINT i = 0, items = m_list.Count(); i < items; i++)
	{
		CRString path(m_list.PathAt(i));
		CRString escaped, label;
		MENUITEMINFOW item = {sizeof(item)};

		Check(RStrReplaceSubStringW(path, SLP(L"&"), SLP(L"&&"), &escaped));
		Check(RStrFormatW(&label, L"&%u %ls", (i + 1) % 10, escaped.c_wstr()));
		item.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STRING;
		item.fType = ownerDraw ? MFT_OWNERDRAW : MFT_STRING;
		item.wID = m_firstCommand + i;
		item.dwTypeData = const_cast<PWSTR>(label.c_wstr());
		CheckIfGetLastError(!InsertMenuItemW(hMenu, i, TRUE, &item));
		m_displayed[i] = path;
	}

Cleanup:
	return hr;
}

HRESULT STDMETHODCALLTYPE CRecentlyUsedMenu::QueryStatus (const GUID* pGroup, ULONG count, OLECMD* pCommands, OLECMDTEXT* pText)
{
	HRESULT hr;

	CheckIf(pGroup, OLECMDERR_E_UNKNOWNGROUP);
	CheckIf(count && !pCommands, E_POINTER);
	hr = OLECMDERR_E_NOTSUPPORTED;
	for(ULONG i = 0; i < count; i++)
	{
		OLECMD& cmd = pCommands[i];
		cmd.cmdf = 0;
		if(Handles(cmd.cmdID))
		{
			cmd.cmdf = OLECMDF_SUPPORTED;
			if(m_displayed[cmd.cmdID - m_firstCommand])
				cmd.cmdf |= OLECMDF_ENABLED;
			hr = S_OK;
		}
	}

Cleanup:
	return hr;
}

HRESULT STDMETHODCALLTYPE CRecentlyUsedMenu::Exec (const GUID* pGroup, DWORD command, DWORD options, VARIANT* pIn, VARIANT* pOut)
{
	HRESULT hr;
	UINT index = command - m_firstCommand;

	CheckIf(pGroup, OLECMDERR_E_UNKNOWNGROUP);
	CheckIf(!Handles(command), OLECMDERR_E_NOTSUPPORTED);
	CheckIf(!m_displayed[index], OLECMDERR_E_DISABLED);
	hr = m_list.OpenSelection(m_displayed[index]);

Cleanup:
	return hr;
}

#ifdef RECENTLY_USED_RIBBON
namespace
{
	class CRecentRibbonItem : public CBaseUnknown, public IUISimplePropertySet
	{
	public:
		IMP_BASE_UNKNOWN

		BEGIN_UNK_MAP
			UNK_INTERFACE(IUISimplePropertySet)
		END_UNK_MAP

		CRecentRibbonItem (RSTRING rstrPath) :
			m_path(rstrPath)
		{
		}

		STDMETHOD(GetValue)(REFPROPERTYKEY key, PROPVARIANT* pValue);
	private:
		CRString m_path;
	};

	HRESULT STDMETHODCALLTYPE CRecentRibbonItem::GetValue (REFPROPERTYKEY key, PROPVARIANT* pValue)
	{
		HRESULT hr;
		PCWSTR pcwzPath = m_path.c_wstr();
		PCWSTR pcwzText = pcwzPath;
		INT cchText;
		SIZE_T cbText;
		PWSTR pwzText;

		CheckIf(!pValue, E_POINTER);
		PropVariantInit(pValue);
		if(UI_PKEY_Pinned == key)
		{
			pValue->vt = VT_BOOL;
			pValue->boolVal = VARIANT_FALSE;
		}
		else
		{
			if(UI_PKEY_Label == key)
			{
				PCWSTR pcwzSlash = TStrCchRChr(pcwzPath, m_path.Length(), L'\\');
				if(pcwzSlash)
					pcwzText = pcwzSlash + 1;
			}
			else
			{
				CheckIf(UI_PKEY_LabelDescription != key, E_NOTIMPL);
			}
			cchText = m_path.Length() - static_cast<INT>(pcwzText - pcwzPath);
			cbText = (cchText + 1) * sizeof(WCHAR);
			pwzText = static_cast<PWSTR>(CoTaskMemAlloc(cbText));
			CheckAlloc(pwzText);
			CopyMemory(pwzText, pcwzText, cbText);
			pValue->pwszVal = pwzText;
			pValue->vt = VT_LPWSTR;
		}
		hr = S_OK;

	Cleanup:
		return hr;
	}
}

CRecentlyUsedRibbon::CRecentlyUsedRibbon (CRecentlyUsed& list, UINT32 command) :
	m_pList(&list), m_command(command)
{
}

VOID CRecentlyUsedRibbon::Detach ()
{
	m_pList = NULL;
}

HRESULT STDMETHODCALLTYPE CRecentlyUsedRibbon::Execute (UINT32 command, UI_EXECUTIONVERB verb, const PROPERTYKEY* pKey, const PROPVARIANT* pValue, IUISimplePropertySet* pProperties)
{
	HRESULT hr;
	UINT index;

	CheckIf(command != m_command || UI_EXECUTIONVERB_EXECUTE != verb, E_NOTIMPL);
	CheckIf(!m_pList, E_UNEXPECTED);
	CheckIf(!pKey || UI_PKEY_SelectedItem != *pKey || !pValue || VT_UI4 != pValue->vt, E_INVALIDARG);
	index = pValue->ulVal;
	CheckIf(index >= CRecentlyUsed::MaxItems || !m_displayed[index], E_INVALIDARG);
	hr = m_pList->OpenSelection(m_displayed[index]);

Cleanup:
	return hr;
}

HRESULT STDMETHODCALLTYPE CRecentlyUsedRibbon::UpdateProperty (UINT32 command, REFPROPERTYKEY key, const PROPVARIANT* pCurrent, PROPVARIANT* pNew)
{
	HRESULT hr;
	SAFEARRAY* pItems = NULL;
	CRString displayed[CRecentlyUsed::MaxItems];
	LONG count;

	CheckIf(command != m_command || UI_PKEY_RecentItems != key, E_NOTIMPL);
	CheckIf(!pNew, E_POINTER);
	CheckIf(!m_pList, E_UNEXPECTED);
	count = m_pList->Count();
	pItems = SafeArrayCreateVector(VT_UNKNOWN, 0, count);
	CheckAlloc(pItems);
	for(LONG i = 0; i < count; i++)
	{
		TStackRef<CRecentRibbonItem> item;
		displayed[i] = m_pList->PathAt(i);
		item.Attach(__new CRecentRibbonItem(displayed[i]));
		CheckAlloc(item);
		Check(SafeArrayPutElement(pItems, &i, static_cast<IUISimplePropertySet*>(&*item)));
	}
	for(UINT i = 0; i < CRecentlyUsed::MaxItems; i++)
		m_displayed[i] = displayed[i];
	PropVariantInit(pNew);
	pNew->vt = VT_ARRAY | VT_UNKNOWN;
	pNew->parray = pItems;
	pItems = NULL;
	hr = S_OK;

Cleanup:
	if(pItems)
		SafeArrayDestroy(pItems);
	return hr;
}
#endif
