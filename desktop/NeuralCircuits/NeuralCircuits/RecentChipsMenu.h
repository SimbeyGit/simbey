#pragma once

#include <docobj.h>

#define	MAX_RECENT_ITEMS	10

class CRecentChipsMenu : public IOleCommandTarget
{
private:
	ULONG m_cRef;

protected:
	HMENU m_hMenu;
	LPSTR m_lpszRecentChips[MAX_RECENT_ITEMS];
	INT m_cRecentChips;
	bool m_fMenuRegistered;
	ULONG m_nCmdIdRangeStart;
	LPSTR m_lpszRegKey;

public:
	CRecentChipsMenu ();
	~CRecentChipsMenu ();

	HRESULT Initialize (ULONG nCmdStart, LPCSTR lpcszRegKey);

	// IUnknown
	HRESULT WINAPI QueryInterface (REFIID iid, LPVOID* lplpvObject);
	ULONG WINAPI AddRef (VOID);
	ULONG WINAPI Release (VOID);

	// IOleCommandTarget
	HRESULT WINAPI QueryStatus (const GUID* lpguidCmdGroup, ULONG cCmds, OLECMD* lprgCmds, OLECMDTEXT* lpCmdText);
	HRESULT WINAPI Exec (const GUID* lpguidCmdGroup, DWORD nCmdID, DWORD nCmdExecOpt, VARIANTARG* lpvaIn, VARIANTARG* lpvaOut);

	VOID UpdateMenu (HMENU hMenu);
	HRESULT AddRecentItem (LPCSTR lpcszItem);
	HRESULT ExecRecentItem (ULONG nCmdID, __out_ecount(cchMaxItem) LPSTR lpszItem, INT cchMaxItem, __out INT* pcchItem);

protected:
	HRESULT AddRecentItemInternal (LPCSTR lpcszItem);
	HRESULT RebuildMenu (VOID);
	HRESULT LoadFromRegistry (VOID);
	HRESULT SaveToRegistry (VOID);
};