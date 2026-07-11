#pragma once

#include "Library\Core\BaseUnknown.h"
#include "Library\Window\BaseWindow.h"

class CQuadooDebugSession;
class CTabs;

class CQuadooCallStackDisplay :
	public CBaseUnknown,
	public CBaseWindow
{
private:
	HINSTANCE m_hInstance;
	CTabs* m_pTabs;
	HWND m_hwndList;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IOleWindow)
		UNK_INTERFACE(IBaseWindow)
	END_UNK_MAP

public:
	CQuadooCallStackDisplay (HINSTANCE hInstance, CTabs* pTabs);
	~CQuadooCallStackDisplay ();

	static HRESULT Register (HINSTANCE hInstance);
	static HRESULT Unregister (HINSTANCE hInstance);

	static INT GetDefaultHeight (VOID);

	HRESULT Initialize (HWND hwndParent);
	HRESULT SetFrames (CQuadooDebugSession* pSession);
	VOID Clear (VOID);

	VOID UpdateColorScheme (bool fDarkMode);

	// CBaseWindow
	virtual HINSTANCE GetInstance (VOID);
	virtual VOID OnFinalDestroy (HWND hwnd);

	virtual HRESULT FinalizeAndShow (DWORD dwStyle, INT nCmdShow);
	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);
};
