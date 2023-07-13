#pragma once

#include "Library\Core\BaseUnknown.h"
#include "Library\Window\BaseWindow.h"

class CSplitter :
	public CBaseUnknown,
	public CBaseWindow
{
private:
	HINSTANCE m_hInstance;
	HBRUSH m_hbrPattern;
	HWND m_hwndLeft;
	BOOL m_fDragging;
	INT m_xDragStart;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IOleWindow)
		UNK_INTERFACE(IBaseWindow)
	END_UNK_MAP

public:
	CSplitter (HINSTANCE hInstance);
	~CSplitter ();

	static HRESULT Register (HINSTANCE hInstance);
	static HRESULT Unregister (HINSTANCE hInstance);

	HRESULT Initialize (HWND hwndParent, HWND hwndLeft);

	// CBaseWindow
	virtual HINSTANCE GetInstance (VOID);
	virtual VOID OnFinalDestroy (HWND hwnd);

	virtual HRESULT FinalizeAndShow (DWORD dwStyle, INT nCmdShow);
	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);
};
