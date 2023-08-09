#pragma once

#include "Library\Core\Map.h"
#include "Library\Util\RString.h"
#include "Library\Window\BaseWindow.h"

class CCustomMenu : public ISubclassedHandler
{
private:
	IBaseWindow* m_pWindow;
	HMENU m_hMenu;
	TMap<UINT, RSTRING> m_mapMenu;
	HBRUSH m_hbrBackground;

public:
	COLORREF m_crBackground, m_crDivider;
	COLORREF m_crNormalText, m_crSelectText, m_crDisabledText, m_crActiveBackground;

public:
	CCustomMenu (HMENU hMenu);
	~CCustomMenu ();

	HRESULT SetBackground (COLORREF crBackground);
	HRESULT SetDarkMode (VOID);

	HRESULT Rebuild (HMENU hMenu);

	// ISubclassedHandler
	virtual VOID OnAttached (IBaseWindow* lpWindow);
	virtual VOID OnDetached (IBaseWindow* lpWindow);
	virtual BOOL OnSubclassMessage (IBaseWindow* lpWindow, UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

private:
	VOID ClearMenuMap (VOID);
	HRESULT BuildMenuMap (HMENU hMenu);
};
