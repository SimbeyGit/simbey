#pragma once

#include <docobj.h>
#include "Core\BaseUnknown.h"
#include "Util\RString.h"

#ifdef RECENTLY_USED_RIBBON
#include <uiRibbon.h>
#endif

struct IOpenRecentlyUsed
{
	virtual HRESULT OpenRecentlyUsed (PCWSTR pcwzPath) = 0;
};

class CRecentlyUsed
{
public:
	enum { MaxItems = 10 };

	CRecentlyUsed ();
	~CRecentlyUsed ();

	// The application owns the callback and uses the list on its UI thread.
	HRESULT Initialize (PCWSTR pcwzAppKey, IOpenRecentlyUsed* pOpen);
	HRESULT RecordOpen (PCWSTR pcwzPath);
	// Pass the displayed path before RecordOpen can change the cached order.
	HRESULT OpenSelection (RSTRING rstrPath);
	UINT Count () const;
	// The list owns this string until the next update.
	RSTRING PathAt (UINT index) const;

private:
	CRString m_key;
	CRString m_paths[MaxItems];
	HANDLE m_hMutex;
	IOpenRecentlyUsed* m_pOpen;

	CRecentlyUsed (const CRecentlyUsed&);
	CRecentlyUsed& operator= (const CRecentlyUsed&);
	HRESULT Lock ();
	HRESULT Reload ();
};

class CRecentlyUsedMenu :
	public CBaseUnknown,
	public IOleCommandTarget
{
public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IOleCommandTarget)
	END_UNK_MAP

public:
	CRecentlyUsedMenu (CRecentlyUsed& list, UINT firstCommand);

	bool Handles (DWORD command) const;
	// Replaces the contents of a popup dedicated to recent files.
	HRESULT Populate (HMENU hMenu, bool ownerDraw);

	// IOleCommandTarget
	STDMETHOD(QueryStatus)(const GUID* pGroup, ULONG count, OLECMD* pCommands, OLECMDTEXT* pText);
	STDMETHOD(Exec)(const GUID* pGroup, DWORD command, DWORD options, VARIANT* pIn, VARIANT* pOut);

private:
	CRecentlyUsed& m_list;
	UINT m_firstCommand;
	CRString m_displayed[CRecentlyUsed::MaxItems];
};

#ifdef RECENTLY_USED_RIBBON
class CRecentlyUsedRibbon :
	public CBaseUnknown,
	public IUICommandHandler
{
public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IUICommandHandler)
	END_UNK_MAP

public:
	CRecentlyUsedRibbon (CRecentlyUsed& list, UINT32 command);

	VOID Detach ();

	// IUICommandHandler
	STDMETHOD(Execute)(UINT32 command, UI_EXECUTIONVERB verb, const PROPERTYKEY* pKey, const PROPVARIANT* pValue, IUISimplePropertySet* pProperties);
	STDMETHOD(UpdateProperty)(UINT32 command, REFPROPERTYKEY key, const PROPVARIANT* pCurrent, PROPVARIANT* pNew);

private:
	CRecentlyUsed* m_pList;
	UINT32 m_command;
	CRString m_displayed[CRecentlyUsed::MaxItems];
};
#endif
