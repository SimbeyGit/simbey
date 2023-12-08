#pragma once

#include <docobj.h>
#include "Library\Core\Map.h"
#include "Library\Core\RStrMap.h"
#include "Library\Core\BaseUnknown.h"
#include "Library\Window\BaseWindow.h"
#include "Published\CodeEdit.h"

interface IJSONObject;

class CDarkMode;
class CTabs;

struct TVNSYNTAXHIGHLIGHT;

struct DPI_AWARE_FONT
{
	LONG lFontSize;
	LONG lEscapement;
	LONG lOrientation;
	LONG lWeight;
	BYTE bItalic;
	BYTE bUnderline;
	BYTE bStrikeOut;
	BYTE bCharSet;
	BYTE bOutPrecision;
	BYTE bClipPrecision;
	BYTE bQuality;
	BYTE bPitchAndFamily;
	WCHAR wzFaceName[LF_FACESIZE];
};

class CProjectTab
{
public:
	RSTRING m_rstrPath;

	PWSTR m_pwzAbsolutePath;
	INT m_cchAbsolutePath;

	HTREEITEM m_hItem;

	bool m_fDefault;

public:
	CProjectTab (RSTRING rstrPath) :
		m_pwzAbsolutePath(NULL),
		m_cchAbsolutePath(0),
		m_hItem(NULL),
		m_fDefault(false)
	{
		RStrSet(m_rstrPath, rstrPath);
	}

	virtual ~CProjectTab ()
	{
		SafeDeleteArrayCount(m_pwzAbsolutePath, m_cchAbsolutePath);
		RStrRelease(m_rstrPath);
	}

	virtual bool IsModified (VOID) = 0;
	virtual HRESULT SaveFromEditor (ICodeEditor* pEditor) = 0;
	virtual HRESULT RestoreEditor (ICodeEditor* pEditor) = 0;
	virtual HRESULT SaveToStorage (ICodeEditor* pEditor, bool fActiveTab) = 0;
	virtual HRESULT OpenCustomLayout (ICodeEditor* pEditor) = 0;
	virtual HRESULT ResizeCustomLayout (INT x, INT y, INT nWidth, INT nHeight, __out INT* pnDocHeight) = 0;
	virtual HRESULT CloseCustomLayout (VOID) = 0;
	virtual VOID CheckAutoIndent (ICodeEditor* pEditor, ULONG nLine, WCHAR wchInsert) = 0;
};

class CProjectFile : public CProjectTab
{
private:
	ITextDocument* m_pTabDocument;
	TEXT_EDIT_VIEW m_tev;

public:
	CProjectFile (RSTRING rstrPath) :
		CProjectTab(rstrPath),
		m_pTabDocument(NULL)
	{
		ZeroMemory(&m_tev, sizeof(m_tev));
	}

	virtual ~CProjectFile ()
	{
		SafeRelease(m_pTabDocument);
	}

	virtual bool IsModified (VOID);
	virtual HRESULT SaveFromEditor (ICodeEditor* pEditor);
	virtual HRESULT RestoreEditor (ICodeEditor* pEditor);
	virtual HRESULT SaveToStorage (ICodeEditor* pEditor, bool fActiveTab);
	virtual HRESULT OpenCustomLayout (ICodeEditor* pEditor) { return S_FALSE; }
	virtual HRESULT ResizeCustomLayout (INT x, INT y, INT nWidth, INT nHeight, __out INT* pnDocHeight);
	virtual HRESULT CloseCustomLayout (VOID) { return S_FALSE; }
	virtual VOID CheckAutoIndent (ICodeEditor* pEditor, ULONG nLine, WCHAR wchInsert);

	static INT ScanIndentationSyntax (PCWSTR pcwzCode, size_w cchCode);
};

class CQuadooProject :
	public CBaseUnknown,
	public CBaseWindow,
	public IOleCommandTarget
{
private:
	HINSTANCE m_hInstance;
	CDarkMode* m_pdm;
	HWND m_hwndTree;

	HICON m_hCloseIcon, m_hDropDown;
	CTabs* m_pTabs;

	RSTRING m_rstrProject;
	RSTRING m_rstrProjectDir;
	HTREEITEM m_hProjectRoot;

	IJSONObject* m_pProject;
	TRStrMap<CProjectTab*> m_mapFiles;

	TNamedMapW<COLORREF> m_mapKeywords;
	ICodeEditor* m_pEditor;

	COLORREF m_crStrings, m_crCommentForeground, m_crCommentHighlight;

	WCHAR m_wzFindSymbol[200];	// Used when finding symbols using the context menu

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IOleWindow)
		UNK_INTERFACE(IOleCommandTarget)
		UNK_INTERFACE(IBaseWindow)
	END_UNK_MAP

public:
	CQuadooProject (HINSTANCE hInstance, CDarkMode* pdm, HWND hwndTree);
	~CQuadooProject ();

	static HRESULT Register (HINSTANCE hInstance);
	static HRESULT Unregister (HINSTANCE hInstance);

	HRESULT Initialize (HWND hwndParent, const RECT& rcSite, PCWSTR pcwzProject);
	HRESULT Save (VOID);
	HRESULT UpdateFiles (VOID);
	HRESULT CloseProject (BOOL fPromptUserForSave);

	HRESULT SetPageScript (INT idScript);

	VOID UpdateColorScheme (VOID);
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

	HRESULT SwitchToFile (CProjectTab* pFile);

	HRESULT EditRunParams (VOID);
	HRESULT AddFilePrompt (VOID);
	HRESULT NewFilePrompt (VOID);
	HRESULT AddFile (RSTRING rstrPath, __deref_out CProjectTab** ppFile);

private:
	VOID ResizeEditor (INT nWindowHeight);
	HRESULT UpdateColors (VOID);
	INT CustomTrackPopupMenu (HMENU hPopup, UINT uFlags, const POINT& ptScreen);

	HRESULT ExtractFindSymbol (PCWSTR pcwzWord, INT idxWordPtr);
	HRESULT FindSymbol (VOID);

	VOID ApplySyntaxColoring (TVNSYNTAXHIGHLIGHT* pHighlight);
	VOID ColorKeyword (TVNSYNTAXHIGHLIGHT* pHighlight, WCHAR wchPrevKeyword, INT idxStart, INT idxEnd);

	HRESULT SaveAll (VOID);
	HRESULT RunScript (VOID);

	HRESULT GetProjectTarget (__deref_out RSTRING* prstrTarget);
	HRESULT FindInstalledEngine (__deref_out RSTRING* prstrEngine);
	static HRESULT FindActiveQuadoo (__deref_out RSTRING* prstrEngine);
	BOOL IsWebProject (VOID);

	CProjectTab* GetProjectFromTreeItem (HTREEITEM hItem);
	CProjectTab* FindDefaultScript (VOID);

	HRESULT SaveDefaultFont (INT nFontSize);
	HRESULT LoadDefaultFont (VOID);

	HRESULT SaveTabData (CProjectTab* pFile);
	HRESULT LoadTabData (CProjectTab* pFile);

	HRESULT SaveTab (sysint idxTab);
	BOOL PromptToSaveTab (sysint idxTab);
	BOOL CloseTab (sysint idxTab);

	VOID RemoveFilePrompt (CProjectTab* pFile);
	HRESULT ShowProjectCompiler (VOID);
	HRESULT CopyWebScripts (VOID);

	HRESULT CreateRelativeProjectPath (__out_ecount(cchMaxRelative) PWSTR pwzRelative, INT cchMaxRelative, PCWSTR pcwzTo, DWORD dwAttrTo);

	static BOOL IsFileOutOfSync (PCWSTR pcwzSource, PCWSTR pcwzTarget);
};
