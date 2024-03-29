#pragma once

//
//	MODULE:		TextEditor.h
//
//	NOTES:		This is an adaptation of the text editing from NeatPad (https://www.catch22.net/tags/neatpad/).
//

#include "Library\Core\BaseUnknown.h"
#include "Library\Core\MemoryStream.h"
#include "Library\Window\BaseWindow.h"
#include "Published\CodeEdit.h"
#include "UspLib\usplib.h"

interface ITextDocument;

#define SYSCOL(COLOR_IDX)					   ( 0x80000000 |                     COLOR_IDX  )
#define MIXED_SYSCOL(COLOR_IDX1, COLOR_IDX2)   ( 0xC0000000 | (COLOR_IDX2 << 8) | COLOR_IDX1 )
#define MIXED_SYSCOL2(COLOR_IDX1, COLOR_IDX2)  ( 0xE0000000 | (COLOR_IDX2 << 8) | COLOR_IDX1 )

#define SYSCOLIDX(COLREF)   ( 0x00FFFFFF & COLREF )

struct USPCACHE
{
	USPDATA* uspData;
	ULONG	 lineno;		// line#
	ULONG	 offset;		// offset (in WCHAR's) of this line
	ULONG	 usage;			// cache-count

	int		 length;		// length in chars INCLUDING CR/LF
	int		 length_CRLF;	// length in chars EXCLUDING CR/LF
};

typedef const SCRIPT_LOGATTR CSCRIPT_LOGATTR;

#define USP_CACHE_SIZE 200

typedef struct
{
	ULONG	line;
	ULONG	xpos;
} CURPOS;

class CTextEditor :
	public CBaseUnknown,
	public CBaseWindow,
	public ICodeEditor
{
private:
	HINSTANCE m_hInstance;
	ITextDocument* m_pTextDoc;
	ULONG		m_uStyleFlags;

	// Font-related data	
	USPFONT		m_uspFontList[MAX_FONTS];
	int			m_nNumFonts;
	int			m_nFontWidth;
	int			m_nMaxAscent;
	int			m_nLineHeight;
	int			m_nHeightAbove;
	int			m_nHeightBelow;

	// Scrollbar-related data
	ULONG		m_nVScrollPos;
	ULONG		m_nVScrollMax;
	int			m_nHScrollPos;
	int			m_nHScrollMax;

	int			m_nWindowLines;
	int			m_nWindowColumns;

	// Cursor/Caret position 
	ULONG		m_nCurrentLine;
	ULONG		m_nSelectionStart;
	ULONG		m_nSelectionEnd;
	ULONG		m_nCursorOffset;
	ULONG		m_nSelMarginOffset1;
	ULONG		m_nSelMarginOffset2;
	int			m_nCaretPosX;
	int			m_nAnchorPosX;

	SELMODE		m_nSelectionMode;
	SELMODE		m_nSelectionType;
	CURPOS		m_cpBlockStart;
	CURPOS		m_cpBlockEnd;
	UINT		m_nEditMode;

	DWORD		m_nCaretWidth;
	int			m_nLongLineLimit;

	// Runtime data
	UINT_PTR	m_nScrollTimer;
	int			m_nScrollCounter;
	bool		m_fHideCaret;

	HCURSOR		m_hMarginCursor;

	COLORREF	m_rgbColorList[TXC_MAX_COLORS];

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IOleWindow)
		UNK_INTERFACE(IBaseWindow)
		UNK_INTERFACE(ICodeEditor)
	END_UNK_MAP

public:
	CTextEditor (HINSTANCE hInstance, bool fDarkMode, bool fUseSystemColors);
	~CTextEditor ();

	static HRESULT Register (HINSTANCE hInstance, DWORD dwClassCookie);
	static HRESULT Unregister (HINSTANCE hInstance, DWORD dwClassCookie);

	HRESULT Initialize (HWND hwndParent, const RECT& rcSite, INT nTabWidth, DWORD dwClassCookie);

	// ICodeEditor
	IFACEMETHOD_(ITextDocument*, GetTextDocument) (VOID) { return m_pTextDoc; }
	IFACEMETHOD_(VOID, SetTextDocument) (ITextDocument* pDocument);

	IFACEMETHOD_(VOID, GetTextEditView) (TEXT_EDIT_VIEW* ptev);
	IFACEMETHOD_(VOID, SetTextEditView) (const TEXT_EDIT_VIEW* pctev);
	IFACEMETHOD_(VOID, ResetTextEditView) (VOID);

	IFACEMETHOD(Prepare) (__in_ecount_opt(cchText) PCWSTR pcwzText, INT cchText);
	IFACEMETHOD_(VOID, EnableEditor) (BOOL fEnable);
	IFACEMETHOD_(VOID, SetFocus) (VOID);

	IFACEMETHOD_(bool, IsModified) (VOID);
	IFACEMETHOD_(bool, Undo) (VOID);
	IFACEMETHOD_(bool, Redo) (VOID);
	IFACEMETHOD_(bool, CanUndo) (VOID);
	IFACEMETHOD_(bool, CanRedo) (VOID);

	IFACEMETHOD_(ULONG, SelectionSize) (VOID);
	IFACEMETHOD_(bool, SelectAll) (VOID);

	IFACEMETHOD_(VOID, ScrollView) (INT xCaret, ULONG nLine);
	IFACEMETHOD_(VOID, SetDarkMode) (bool fDarkMode, bool fUseSystemColors);
	IFACEMETHOD(DisplayOptions) (__inout_ecount_opt(cCustom) COLORREF* prgCustom, INT cCustom, __out_opt BOOL* pfChanged, __out_opt INT* pnFontSize);

	IFACEMETHOD_(ULONG, GetStyleMask) (ULONG uMask);
	IFACEMETHOD_(ULONG, SetStyleMask) (ULONG uMask, ULONG uStyles);

	IFACEMETHOD(AdjustIndentation) (ULONG nLine, INT nIndentation);
	IFACEMETHOD_(LRESULT, SetDefaultFont)(HFONT hFont, bool fTakeOwnership);

	// CBaseWindow
	virtual HINSTANCE GetInstance (VOID);
	virtual VOID OnFinalDestroy (HWND hwnd);

	virtual HRESULT FinalizeAndShow (DWORD dwStyle, INT nCmdShow);
	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

	BOOL ForwardDelete (VOID);
	BOOL BackDelete (VOID);
	ULONG EnterText (PCWSTR pcwzText, ULONG nLength);

private:
	static HRESULT BuildClassName (DWORD dwClassCookie, __out_ecount(cchMaxClass) PWSTR pwzClass, INT cchMaxClass);

	VOID RefreshWindow (VOID);

	VOID SendUpdateCommand (VOID);
	ULONG_PTR NotifyParent (UINT nNotifyCode, NMHDR* optional = NULL);
	VOID NotifyCursorChange (VOID);
	VOID NotifyEnterChar (WCHAR wch);

	bool CheckStyle (ULONG uMask);

	COLORREF GetColor (UINT idx);
	COLORREF SetColor (UINT idx, COLORREF rgbColor);

	COLORREF LineColor (ULONG nLineNo);
	COLORREF LongColor (ULONG nLineNo);
	void ResetLineCache ();
	ULONG GetText (PWSTR pwzDest, ULONG nStartOffset, ULONG nLength);

	// Cache for USPDATA objects
	CMemoryStream m_stmTextBuff;
	CMemoryStream m_stmAttrBuff;
	USPCACHE    m_uspCache[USP_CACHE_SIZE];
	USPDATA		*GetUspData(HDC hdc, ULONG nLineNo, ULONG *nOffset=0);
	USPCACHE    *GetUspCache(HDC hdc, ULONG nLineNo, ULONG *nOffset=0);
	bool		 GetLogAttr(ULONG nLineNo, USPCACHE **puspCache, CSCRIPT_LOGATTR **plogAttr=0, ULONG *pnOffset=0);

	INT			LeftMarginWidth (VOID);
	void		PaintLine (HDC hdc, ULONG nLineNo, int xpos, int ypos, HRGN hrgnUpdate);
	void		PaintText (HDC hdc, ULONG nLineNo, int xpos, int ypos, RECT* bounds);
	void		PaintMargin (HDC hdc, ULONG nLineNo, const RECT& rcMargin);
	int			ApplySelection (USPDATA* uspData, ULONG nLine, ULONG nOffset, ULONG nTextLen);
	int			ApplyTextAttributes (ULONG nLineNo, ULONG nOffset, ULONG &nColumn, PWSTR szText, int nTextLen, ATTR* attr);

	VOID SetupScrollbars ();
	bool PinToBottomCorner ();
	VOID UpdateCaretOffset (ULONG offset, BOOL fTrailing, int* outx, ULONG* outlineno);
	VOID UpdateCaretXY (int xpos, ULONG lineno);
	VOID RepositionCaret ();
	VOID UpdateView (bool fAdvancing);
	INT GetCurrentColumn ();

	LONG InvalidateRange (ULONG nStart, ULONG nFinish);
	LONG InvalidateLine(ULONG nLineNo, bool forceAnalysis);
	void UpdateLine (ULONG nLineNo);

	VOID FinalizeNavigation (WPARAM nKeyCode, BOOL fShiftDown, BOOL fCtrlDown, BOOL fAdvancing);
	VOID MouseCoordToFilePos (int mx, int my, ULONG* pnLineNo, ULONG* pnFileOffset, int* psnappedX);
	HMENU CreateContextMenu (VOID);

	VOID MoveWordPrev ();
	VOID MoveWordNext ();
	VOID MoveWordStart ();
	VOID MoveWordEnd ();
	VOID MoveCharPrev ();
	VOID MoveCharNext ();
	VOID MoveLineUp (int numLines);
	VOID MoveLineDown (int numLines);
	VOID MoveLineStart (ULONG lineNo);
	VOID MoveLineEnd (ULONG lineNo);
	VOID MoveFileStart ();
	VOID MoveFileEnd ();

	VOID UpdateMetrics ();
	VOID RecalcLineHeight ();
	VOID SetFont (HFONT hFont, bool fDeleteFont, int idx);
	LRESULT SizeEditor (WPARAM nFlags, int width, int height);

	HRGN ScrollRgn (int dx, int dy, bool fReturnUpdateRgn);
	VOID Scroll (int dx, int dy);
	VOID ScrollToPosition (int xpos, ULONG lineno);
	VOID ScrollToCaret ();
	static LONG GetTrackPos32 (HWND hwnd, int nBar);

	static bool IsKeyPressed (UINT nVirtKey);

	LRESULT OnSize (WPARAM nFlags, int width, int height);
	LRESULT OnVScroll (UINT nSBCode, UINT nPos);
	LRESULT OnHScroll (UINT nSBCode, UINT nPos);
	LRESULT OnMouseActivate (HWND hwndTop, UINT nHitTest, UINT nMessage);
	LRESULT OnContextMenu (HWND wParam, int x, int y);
	LRESULT OnMouseWheel (int nDelta);
	LRESULT OnTimer (UINT_PTR nTimer);
	LRESULT OnLButtonDown (WPARAM nFlags, int mx, int my);
	LRESULT OnLButtonUp (WPARAM nFlags, int mx, int my);
	LRESULT OnLButtonDblClick (WPARAM nFlags, int mx, int my);
	LRESULT OnMouseMove (WPARAM nFlags, int mx, int my);
	LRESULT OnKeyDown (WPARAM nKeyCode, LPARAM nFlags);
	LRESULT OnChar (WPARAM nChar, LPARAM nFlags);
	LRESULT OnPaint (VOID);
	LRESULT OnSetFocus (HWND hwndOld);
	LRESULT OnKillFocus (HWND hwndNew);

	BOOL OnCut ();
	BOOL OnCopy ();
	BOOL OnPaste ();
	BOOL OnClear ();
};
