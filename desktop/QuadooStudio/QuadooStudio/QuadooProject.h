#pragma once

#include <docobj.h>
#include "Library\Core\Map.h"
#include "Library\Core\RStrMap.h"
#include "Library\Core\BaseUnknown.h"
#include "Library\Window\BaseWindow.h"
#include "Published\CodeEdit.h"
#include "UpdateTitle.h"
#include "FormProjectFile.h"

interface IJSONObject;
interface IJSONArray;
interface IQuadooDebugTree;

namespace QFP { struct IDefinitionVisitor; }
namespace StudioApplication { class CApplicationDebugData; }
class CDarkMode;
class CTabs;
class CMemoryStream;
class CQuadooDebugSession;
class CQuadooCallStackDisplay;

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
	TArray<ULONG> m_aBreakpoints;

public:
	CStudioFormUnit* m_form;
	IJSONObject* m_metadata;
	CProjectFile (RSTRING rstrPath) :
		CProjectTab(rstrPath),
		m_pTabDocument(NULL), m_form(NULL), m_metadata(NULL)
	{
		ZeroMemory(&m_tev, sizeof(m_tev));
	}

	virtual ~CProjectFile ();
	ITextDocument* StoredDocument() const { return m_pTabDocument; }
	VOID DiscardStoredDocument() { SafeRelease(m_pTabDocument); }
	VOID StoreDocument(ITextDocument* document) { ReplaceInterface(m_pTabDocument,document); }
	TEXT_EDIT_VIEW& StoredView() { return m_tev; }

	virtual bool IsModified (VOID);
	virtual HRESULT SaveFromEditor (ICodeEditor* pEditor);
	virtual HRESULT RestoreEditor (ICodeEditor* pEditor);
	virtual HRESULT SaveToStorage (ICodeEditor* pEditor, bool fActiveTab);
	virtual HRESULT OpenCustomLayout (ICodeEditor* pEditor) { return S_FALSE; }
	virtual HRESULT ResizeCustomLayout (INT x, INT y, INT nWidth, INT nHeight, __out INT* pnDocHeight);
	virtual HRESULT CloseCustomLayout (VOID) { return S_FALSE; }
	virtual VOID CheckAutoIndent (ICodeEditor* pEditor, ULONG nLine, WCHAR wchInsert);

	bool HasBreakpoint (ULONG nLine) const;
	sysint Breakpoints (VOID) const { return m_aBreakpoints.Length(); }
	ULONG GetBreakpoint (sysint idxBreakpoint) const { return m_aBreakpoints[idxBreakpoint]; }
	HRESULT ToggleBreakpoint (ULONG nLine);
	VOID AdjustBreakpoints (const TVNLINESCHANGED* pLinesChanged, __out BOOL* pfChanged);
	HRESULT LoadBreakpoints (IJSONArray* pBreakpoints);
	HRESULT SaveBreakpoints (__deref_out IJSONArray** ppBreakpoints);

	static INT ScanIndentationSyntax (PCWSTR pcwzCode, size_w cchCode);
};

class CQuadooProject :
	public CBaseUnknown,
	public CBaseWindow,
	public IOleCommandTarget
{
private:
	HINSTANCE m_hInstance;
	IUpdateTitle* m_pTitle;
	CDarkMode* m_pdm;
	HWND m_hwndTree;
	HWND m_hwndStatus;
	CProjectFile* m_pCodeFile;
	IQuadooControlRegistry* m_formRegistry;
	TArray<CProjectTreeNode*> m_treeNodes;
	TRStrMap<INT> m_treeIcons;
	UINT m_treeDepth;
	UINT m_sourceDepth;
	CProjectFile* m_savingUnit;
	friend class CStudioSourceEdit;

	HICON m_hCloseIcon, m_hDropDown;
	CTabs* m_pTabs;

	RSTRING m_rstrProject;
	RSTRING m_rstrProjectDir;
	CRString m_savedProject;
	HTREEITEM m_hProjectRoot;

	IJSONObject* m_pProject;
	TRStrMap<CProjectTab*> m_mapFiles;

	TNamedMapW<COLORREF> m_mapKeywords;
	ICodeEditor* m_pEditor;
	CQuadooDebugSession* m_pDebugger;
	BOOL m_applicationDebugReadOnly;
	CQuadooCallStackDisplay* m_pCallStack;
	IQuadooDebugTree* m_pDebugTree;
	HWND m_hwndDebugTip;
	DWORD m_nHoverRequest;
	BOOL m_fHoverPending;
	RSTRING m_rstrHoverName;
	RSTRING m_rstrTooltipText;

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
	CQuadooProject (HINSTANCE hInstance, IUpdateTitle* pTitle, CDarkMode* pdm, HWND hwndTree);
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
	BOOL IsApplication(VOID);
	BOOL IsTreeUpdating() const { return m_treeDepth != 0; }
	HRESULT ReloadSavedOutline(CProjectFile* file);
	BOOL DesignerActive(VOID);
	QFD::CDesignerEditor* ActiveDesigner(VOID);
	HWND ActivePalette(VOID);
	VOID SetStatusWindow(HWND status) { m_hwndStatus = status; }
	VOID UpdateDesignerStatus(VOID);
	BOOL TranslateProjectMessage(MSG* message);
	HRESULT SelectTreeItem(HTREEITEM item);
	HRESULT ExpandTreeItem(HTREEITEM item);
	HRESULT NewFormUnit(BOOL form, PCWSTR sourcePath = NULL);
	HRESULT AddFormUnit(BOOL form);
	HRESULT SetDesignView(BOOL design);
	HRESULT FindSource(RSTRING path, CProjectFile** file);
	ICodeEditor* ActiveSourceEditor() const { return m_pCodeFile ? m_pEditor : NULL; }
	IQuadooControlRegistry* FormRegistry() const { return m_formRegistry; }
	VOID SetFormRegistry(IQuadooControlRegistry* registry) { ReplaceInterface(m_formRegistry,registry); }
	HRESULT VisitPackageDefinitions(QFP::IDefinitionVisitor* visitor);
	HRESULT GetPackageEditors(TArray<QFD::CDesignerEditor*>& editors);
	HRESULT PrepareFormSource(CProjectFile* file, IJSONObject* proposed, RSTRING handler, QFD::ISourceEdit** edit, INT* position = NULL, BOOL repair = FALSE);
	HRESULT OpenFormHandler(CProjectFile* file, REFGUID component, RSTRING eventName);
	HRESULT SynchronizeFormSource(CProjectFile* file, BOOL repair = FALSE);
	HRESULT SourceDocument(CProjectFile* file, ITextDocument** document);
	RSTRING SourceDiagnostic() { CProjectFile* file = ActiveFile(); return file && file->m_form ? static_cast<RSTRING>(file->m_form->sourceDiagnostic) : NULL; }
	HRESULT SourceError() { CProjectFile* file = ActiveFile(); return file && file->m_form ? file->m_form->sourceError : S_OK; }
	HRESULT PrepareUnitSave(CProjectFile* file, CMemoryStream* project, RSTRING* path);
	HRESULT SavedUnitBreakpoints(CProjectFile* file, IJSONValue** points);
	VOID UnitProjectSaved(RSTRING text) { m_savedProject = text; }

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
	HRESULT EditApplicationSettings(VOID);
	HRESULT AddFilePrompt (VOID);
	HRESULT NewFilePrompt (VOID);
	HRESULT AddFile (RSTRING rstrPath, __deref_out CProjectTab** ppFile);
	HRESULT CreateDebugger (__deref_out CQuadooDebugSession** ppDebugger, __out CMemoryStream* pstmBreakpoints, __deref_out RSTRING* prstrRemoteHost);
	HRESULT AttachDebugger (CQuadooDebugSession* pDebugger, IQuadooDebugTree* pDebugTree);
	HRESULT CompileApplicationDebug(StudioApplication::CApplicationDebugData& data, CMemoryStream& byteCode,
		CMemoryStream& debug, IQuadooDebugTree** tree, CRString& diagnostic);
	VOID SetApplicationDebugReadOnly(BOOL readOnly);

private:
	CProjectFile* ActiveFile(VOID);
	CProjectTreeNode* TreeNode(HTREEITEM item);
	HRESULT AddTreeNode(CProjectTab* file, BOOL source, REFGUID id, HTREEITEM parent, RSTRING label, INT image, CProjectTreeNode** node);
	VOID ClearTreeNodes(CProjectTab* file = NULL);
	HRESULT RefreshOutline(CProjectFile* file);
	HRESULT SavedOutline(CProjectFile* file, IJSONObject* definition, HTREEITEM parent, UINT depth);
	HRESULT InitializeFormUnit(CProjectFile* file, IJSONObject* metadata);
	VOID DesignerChanged(UINT_PTR session);
	INT TreeIcon(RSTRING type);
	VOID HideDesigners(VOID);
	HRESULT DesignerCommand(UINT command);
	HRESULT ExportDefinition();
	VOID ResizeEditor (INT nWindowHeight);
	HRESULT UpdateColors (VOID);
	INT CustomTrackPopupMenu (HMENU hPopup, UINT uFlags, const POINT& ptScreen);

	HRESULT ExtractFindSymbol (PCWSTR pcwzWord, INT idxWordPtr);
	HRESULT FindSymbol (VOID);

	VOID ApplySyntaxColoring (TVNSYNTAXHIGHLIGHT* pHighlight);
	VOID ColorKeyword (TVNSYNTAXHIGHLIGHT* pHighlight, WCHAR wchPrevKeyword, INT idxStart, INT idxEnd);
	BOOL DrawBreakpoint (DRAWITEMSTRUCT* pdis);
	HRESULT ToggleBreakpoint (ULONG nLine);
	HRESULT AdjustBreakpoints (const TVNLINESCHANGED* pLinesChanged);
	CProjectFile* GetActiveProjectFile (VOID);

	HRESULT SaveAll (VOID);
	HRESULT RunScript (VOID);
	HRESULT RunApplication(VOID);
	HRESULT SaveApplicationInputs(VOID);
	HRESULT DebugScript (VOID);
	HRESULT BuildBreakpointData (__out CMemoryStream* pstmBreakpoints);
	HRESULT SendDebuggerCommand (DWORD nMessage);
	HRESULT SendDebuggerBreakpoint (CProjectFile* pFile, DWORD nLine, BOOL fAdd);
	HRESULT ShowDebuggerLocation (VOID);
	HRESULT BeginVariableHover (const TVNMOUSEHOVER* pHover);
	HRESULT ExtractHoverIdentifier (ULONG nLine, ULONG nOffset, __deref_out RSTRING* prstrIdentifier);
	HRESULT InitializeDebuggerTooltip (VOID);
	VOID UpdateVariableTooltip (VOID);
	VOID HideDebuggerTooltip (BOOL fInvalidateRequest = TRUE);
	VOID UpdateDebuggerUI (VOID);
	VOID StopDebugger (VOID);
	BOOL IsDebugging (VOID);

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
