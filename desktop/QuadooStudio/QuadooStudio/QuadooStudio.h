#pragma once

#include <docobj.h>
#include <gdiplus.h>
#include "Library\Core\BaseUnknown.h"
#include "Library\Window\BaseWindow.h"
#include "Library\RecentlyUsed.h"
#include "DarkMode.h"
#include "CustomMenu.h"
#include "UpdateTitle.h"
#include "..\..\..\platform\QuadooForm\Packages\Packages.h"

class CSplitter;
class CQuadooProject;

class CQuadooStudio :
	public CBaseUnknown,
	public CBaseWindow,
	public IUpdateTitle,
	public IOleCommandTarget,
	public IOpenRecentlyUsed,
	public QFP::IHost
{
protected:
	HINSTANCE m_hInstance;
	HACCEL m_hAccel, m_hDesignerAccel;
	INT m_treeHeight;
	BOOL m_dragTreeSplit;

	Gdiplus::GdiplusStartupInput* m_pGdiPlusStartupInput;
	ULONG_PTR m_gdiplusToken;

	HWND m_hwndStatus;
	HWND m_hwndTree;
	HIMAGELIST m_hImageList;

	CDarkMode m_dm;
	CCustomMenu* m_pCustomMenu;
	CSplitter* m_pSplitter;
	CQuadooProject* m_pProject;
	TStackRef<QFP::CCatalog> m_packages;
	BOOL m_packagesBusy;
	CRecentlyUsed m_recentProjects;
	TStackRef<CRecentlyUsedMenu> m_recentMenu;
	HMENU m_hRecentProjects;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IOleWindow)
		UNK_INTERFACE(IOleCommandTarget)
		UNK_INTERFACE(IBaseWindow)
		UNK_INTERFACE(IUpdateTitle)
	END_UNK_MAP

public:
	CQuadooStudio (HINSTANCE hInstance);
	~CQuadooStudio ();

	static HRESULT Register (HINSTANCE hInstance);
	static HRESULT Unregister (HINSTANCE hInstance);

	HRESULT Initialize (PCWSTR pcwzCmdLine, INT nWidth, INT nHeight, INT nCmdShow, PCWSTR packageDirectory = NULL);
	BOOL PreTranslate (__inout MSG* pmsg);
	HRESULT OpenProject(PCWSTR path);
	CQuadooProject* Project() const { return m_pProject; }
	QFP::CCatalog* Packages() const { return m_packages; }
	HRESULT VisitDefinitions(QFP::IDefinitionVisitor* visitor);
	HRESULT GetEditors(TArray<QFD::CDesignerEditor*>& editors);
	HRESULT RefreshPackages();

	// IUpdateTitle
	virtual HRESULT UpdateTitle (PCWSTR pcwzProject, __in_opt PCWSTR pcwzStatus);

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
	HRESULT OpenAndRecordProject (PCWSTR pcwzPath);
	virtual HRESULT OpenRecentlyUsed (PCWSTR pcwzPath);
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
