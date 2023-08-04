#pragma once

#include <docobj.h>
#include "Library\Core\Map.h"
#include "Library\Core\RStrMap.h"
#include "Library\Core\BaseUnknown.h"
#include "Library\Window\BaseWindow.h"
#include "Published\CodeEdit.h"

interface IJSONObject;

class CTabs;

struct TVNSYNTAXHIGHLIGHT;

class CProjectFile
{
public:
	RSTRING m_rstrPath;

	PWSTR m_pwzAbsolutePath;
	INT m_cchAbsolutePath;

	HTREEITEM m_hItem;

	ITextDocument* m_pTabDocument;
	TEXT_EDIT_VIEW m_tev;

	bool m_fDefault;

public:
	CProjectFile (RSTRING rstrPath) :
		m_pwzAbsolutePath(NULL),
		m_cchAbsolutePath(0),
		m_hItem(NULL),
		m_pTabDocument(NULL),
		m_fDefault(false)
	{
		RStrSet(m_rstrPath, rstrPath);
		ZeroMemory(&m_tev, sizeof(m_tev));
	}

	~CProjectFile ()
	{
		SafeRelease(m_pTabDocument);
		SafeDeleteArrayCount(m_pwzAbsolutePath, m_cchAbsolutePath);
		RStrRelease(m_rstrPath);
	}
};

class CQuadooProject :
	public CBaseUnknown,
	public CBaseWindow,
	public IOleCommandTarget
{
private:
	HINSTANCE m_hInstance;
	HWND m_hwndTree;

	HICON m_hCloseIcon, m_hDropDown;
	CTabs* m_pTabs;

	RSTRING m_rstrProject;
	RSTRING m_rstrProjectDir;
	HTREEITEM m_hProjectRoot;

	IJSONObject* m_pProject;
	TRStrMap<CProjectFile*> m_mapFiles;

	TNamedMapW<COLORREF> m_mapKeywords;
	ICodeEditor* m_pEditor;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IOleWindow)
		UNK_INTERFACE(IOleCommandTarget)
		UNK_INTERFACE(IBaseWindow)
	END_UNK_MAP

public:
	CQuadooProject (HINSTANCE hInstance, HWND hwndTree);
	~CQuadooProject ();

	static HRESULT Register (HINSTANCE hInstance);
	static HRESULT Unregister (HINSTANCE hInstance);

	HRESULT Initialize (HWND hwndParent, const RECT& rcSite, PCWSTR pcwzProject);
	HRESULT Save (VOID);
	HRESULT UpdateFiles (VOID);
	HRESULT CloseProject (BOOL fPromptUserForSave);

	VOID ShowTreeContext (HTREEITEM hItem, const POINT& ptScreen);
	VOID ActivateItem (HTREEITEM hItem);
	VOID Move (const RECT& rcSite);

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

	// CBaseWindow
	virtual HINSTANCE GetInstance (VOID);
	virtual VOID OnFinalDestroy (HWND hwnd);

	virtual HRESULT FinalizeAndShow (DWORD dwStyle, INT nCmdShow);
	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

	HRESULT SwitchToFile (CProjectFile* pFile);

	HRESULT EditRunParams (VOID);
	HRESULT AddFilePrompt (VOID);
	HRESULT NewFilePrompt (VOID);
	HRESULT AddFile (RSTRING rstrPath, __deref_out CProjectFile** ppFile);

private:
	VOID ApplySyntaxColoring (TVNSYNTAXHIGHLIGHT* pHighlight);
	VOID ColorKeyword (TVNSYNTAXHIGHLIGHT* pHighlight, WCHAR wchPrevKeyword, INT idxStart, INT idxEnd);

	HRESULT SaveAll (VOID);
	HRESULT RunScript (VOID);

	HRESULT GetProjectTarget (__deref_out RSTRING* prstrTarget);
	HRESULT FindInstalledEngine (__deref_out RSTRING* prstrEngine);

	CProjectFile* GetProjectFromTreeItem (HTREEITEM hItem);
	CProjectFile* FindDefaultScript (VOID);

	HRESULT SaveTabData (CProjectFile* pFile);
	HRESULT LoadTabData (CProjectFile* pFile);

	HRESULT SaveTab (sysint idxTab);
	BOOL PromptToSaveTab (sysint idxTab);
	BOOL CloseTab (sysint idxTab);

	VOID RemoveFilePrompt (CProjectFile* pFile);
	HRESULT ShowProjectCompiler (VOID);
};
