#pragma once

#include <docobj.h>
#include <gdiplus.h>
#include "Library\Core\BaseUnknown.h"
#include "Library\Window\BaseWindow.h"
#include "DarkMode.h"
#include "CustomMenu.h"

class CSplitter;
class CQuadooProject;

class CQuadooStudio :
	public CBaseUnknown,
	public CBaseWindow,
	public IOleCommandTarget
{
protected:
	HINSTANCE m_hInstance;
	HACCEL m_hAccel;

	Gdiplus::GdiplusStartupInput* m_pGdiPlusStartupInput;
	ULONG_PTR m_gdiplusToken;

	HWND m_hwndStatus;
	HWND m_hwndTree;
	HIMAGELIST m_hImageList;

	CDarkMode m_dm;
	CCustomMenu* m_pCustomMenu;
	CSplitter* m_pSplitter;
	CQuadooProject* m_pProject;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IOleWindow)
		UNK_INTERFACE(IOleCommandTarget)
		UNK_INTERFACE(IBaseWindow)
	END_UNK_MAP

public:
	CQuadooStudio (HINSTANCE hInstance);
	~CQuadooStudio ();

	static HRESULT Register (HINSTANCE hInstance);
	static HRESULT Unregister (HINSTANCE hInstance);

	HRESULT Initialize (PCWSTR pcwzCmdLine, INT nWidth, INT nHeight, INT nCmdShow);
	BOOL PreTranslate (__inout MSG* pmsg);

	// IOleCommandTarget
	virtual HRESULT STDMETHODCALLTYPE QueryStatus (
		const GUID* pguidCmdGroup,
		ULONG cCmds,
		OLECMD* prgCmds,
		OLECMDTEXT* pCmdText);
	virtual HRESULT STDMETHODCALLTYPE Exec (
		const GUID* pguidCmdGroup,
		DWORD nCmdID,
		DWORD nCmdexecopt,
		VARIANT* pvaIn,
		VARIANT* pvaOut);

private:
	HRESULT CreateNewProject (VOID);
	HRESULT OpenProjectPrompt (VOID);
	HRESULT OpenProject (PCWSTR pcwzProject);
	HRESULT UpdateTitle (PCWSTR pcwzProject);
	VOID UpdateColorScheme (VOID);

	// CBaseWindow
	virtual HINSTANCE GetInstance (VOID);
	virtual VOID OnFinalDestroy (HWND hwnd);

	virtual HRESULT FinalizeAndShow (DWORD dwStyle, INT nCmdShow);
	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

private:
	HRESULT OnCreate (VOID);
	INT GetTreeWidth (VOID);

	VOID ClearMenuMap (VOID);
	HRESULT BuildMenuMap (HMENU hMenu);
};
