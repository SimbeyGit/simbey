#pragma once

//
//	MODULE:		TextEditor.h
//
//	NOTES:		This is an adaptation of the text editing from NeatPad (https://www.catch22.net/tags/neatpad/).
//

#include "Library\Core\BaseUnknown.h"
#include "Library\Core\MemoryStream.h"
#include "Library\Window\BaseWindow.h"
#include "UspLib\usplib.h"
#include "TextDocument.h"

//
//	TextView colors
//
#define TXC_FOREGROUND			0			// normal foreground color
#define TXC_BACKGROUND			1			// normal background color
#define TXC_HIGHLIGHTTEXT		2			// normal text highlight color
#define TXC_HIGHLIGHT			3			// normal background highlight color
#define TXC_HIGHLIGHTTEXT2		4			// inactive text highlight colour
#define TXC_HIGHLIGHT2			5			// inactive background highlight color
#define TXC_SELMARGIN1			6			// selection margin color #1
#define TXC_SELMARGIN2			7			// selection margin color #2
#define TXC_LINENUMBERTEXT		8			// line number text
#define TXC_LINENUMBER			9			// line number background
#define TXC_LONGLINETEXT		10			// long-line text
#define TXC_LONGLINE			11			// long-line background
#define TXC_CURRENTLINETEXT		12			// active line text
#define TXC_CURRENTLINE			13			// active line background
#define TXC_DISABLED			14			// disabled window background
#define TXC_MAX_COLORS			15			// keep this updated!

#define SYSCOL(COLOR_IDX)					   ( 0x80000000 |                     COLOR_IDX  )
#define MIXED_SYSCOL(COLOR_IDX1, COLOR_IDX2)   ( 0xC0000000 | (COLOR_IDX2 << 8) | COLOR_IDX1 )
#define MIXED_SYSCOL2(COLOR_IDX1, COLOR_IDX2)  ( 0xE0000000 | (COLOR_IDX2 << 8) | COLOR_IDX1 )

#define SYSCOLIDX(COLREF)   ( 0x00FFFFFF & COLREF )

//
//	Text Editor Notification Messages defined here - 
//	sent via the WM_NOTIFY message
//
#define TVN_BASE				(WM_USER)
#define TVN_CURSOR_CHANGE		(TVN_BASE + 0)
#define TVN_SELECTION_CHANGE	(TVN_BASE + 1)
#define TVN_EDITMODE_CHANGE		(TVN_BASE + 2)
#define	TVN_MARGIN_CLICK		(TVN_BASE + 3)
#define TVN_SYNTAX_HIGHLIGHT	(TVN_BASE + 4)

struct TEXT_EDIT_VIEW
{
	ULONG		nVScrollPos;
	int			nHScrollPos;

	ULONG		nCurrentLine;
	ULONG		nSelectionStart;
	ULONG		nSelectionEnd;
	ULONG		nCursorOffset;
	ULONG		nSelMarginOffset1;
	ULONG		nSelMarginOffset2;
	int			nCaretPosX;
	int			nAnchorPosX;
};

struct TVNCURSORINFO
{
	NMHDR	hdr;
	ULONG	nLineNo;
	ULONG	nColumnNo;
	ULONG	nOffset;
};

struct TVNMARGINCLICK
{
	NMHDR	hdr;
	ULONG	nLineNo;
	INT		xClick;
	INT		xMargin;
	BOOL	fHandled;
};

struct TVNSYNTAXHIGHLIGHT : NMHDR
{
	ULONG	nLineNo;
	PCWSTR	pcwzText;
	INT		nTextLen;
	PATTR	pAttr;
};

//
//
//	TextView Window Styles defined here
//	(set using TXM_SETSTYLE)
//
#define TXS_SELMARGIN			1
#define TXS_LINENUMBERS			2
#define TXS_TREECTRL			4
#define TXS_LONGLINES			8
#define TXS_HIGHLIGHTCURLINE	16

// maximum fonts that a TextView can hold
#define MAX_FONTS 32

enum SELMODE { SEL_NONE, SEL_NORMAL, SEL_MARGIN, SEL_BLOCK };

//
//	TextView edit modes
//
#define MODE_READONLY	0
#define MODE_INSERT		1
#define MODE_OVERWRITE	2

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
	public CBaseWindow
{
private:
	HINSTANCE m_hInstance;
	CTextDocument* m_pTextDoc;
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
	END_UNK_MAP

public:
	CTextEditor (HINSTANCE hInstance);
	~CTextEditor ();

	static HRESULT Register (HINSTANCE hInstance);
	static HRESULT Unregister (HINSTANCE hInstance);

	HRESULT Initialize (HWND hwndParent, const RECT& rcSite, INT nTabWidth);
	HRESULT Prepare (__in_ecount_opt(cchText) PCWSTR pcwzText, INT cchText);

	// CBaseWindow
	virtual HINSTANCE GetInstance (VOID);
	virtual VOID OnFinalDestroy (HWND hwnd);

	virtual HRESULT FinalizeAndShow (DWORD dwStyle, INT nCmdShow);
	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

	bool IsModified (VOID);
	BOOL Undo (VOID);
	BOOL Redo (VOID);
	bool CanUndo (VOID);
	bool CanRedo (VOID);

	ULONG SelectionSize (VOID);
	BOOL SelectAll (VOID);

	BOOL ForwardDelete (VOID);
	BOOL BackDelete (VOID);
	ULONG EnterText (PCWSTR pcwzText, ULONG nLength);

	VOID EnableEditor (BOOL fEnable);

	CTextDocument* GetTextDocument (VOID);
	VOID SetTextDocument (CTextDocument* pDocument);

	VOID GetTextEditView (TEXT_EDIT_VIEW* ptev);
	VOID SetTextEditView (const TEXT_EDIT_VIEW* pctev);
	VOID ResetTextEditView (VOID);

private:
	VOID RefreshWindow (VOID);

	VOID SendUpdateCommand (VOID);
	ULONG NotifyParent (UINT nNotifyCode, NMHDR* optional = NULL);

	ULONG GetStyleMask (ULONG uMask);
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

	LONG InvalidateRange (ULONG nStart, ULONG nFinish);
	LONG InvalidateLine(ULONG nLineNo, bool forceAnalysis);
	void UpdateLine (ULONG nLineNo);

	VOID FinalizeNavigation (UINT nKeyCode, BOOL fShiftDown, BOOL fCtrlDown, BOOL fAdvancing);
	BOOL MouseCoordToFilePos (int mx, int my, ULONG* pnLineNo, ULONG* pnFileOffset, int* psnappedX);

	VOID MoveWordPrev ();
	VOID MoveWordNext ();
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
	VOID SetFont (HFONT hFont, int idx);
	LRESULT SizeEditor (UINT nFlags, int width, int height);

	HRGN ScrollRgn (int dx, int dy, bool fReturnUpdateRgn);
	VOID Scroll (int dx, int dy);
	VOID ScrollToPosition (int xpos, ULONG lineno);
	VOID ScrollToCaret ();
	static LONG GetTrackPos32 (HWND hwnd, int nBar);

	static bool IsKeyPressed (UINT nVirtKey);

	LRESULT OnSize (UINT nFlags, int width, int height);
	LRESULT OnVScroll (UINT nSBCode, UINT nPos);
	LRESULT OnHScroll (UINT nSBCode, UINT nPos);
	LRESULT OnMouseActivate (HWND hwndTop, UINT nHitTest, UINT nMessage);
	LRESULT OnMouseWheel (int nDelta);
	LRESULT OnTimer (UINT_PTR nTimer);
	LRESULT OnLButtonDown (UINT nFlags, int mx, int my);
	LRESULT OnLButtonUp (UINT nFlags, int mx, int my);
	LRESULT OnLButtonDblClick (UINT nFlags, int mx, int my);
	LRESULT OnMouseMove (UINT nFlags, int mx, int my);
	LRESULT OnKeyDown (UINT nKeyCode, UINT nFlags);
	LRESULT OnChar (UINT nChar, UINT nFlags);
	LRESULT OnPaint (VOID);
	LRESULT OnSetFont (HFONT hFont);
	LRESULT OnSetFocus (HWND hwndOld);
	LRESULT OnKillFocus (HWND hwndNew);

	BOOL OnCut ();
	BOOL OnCopy ();
	BOOL OnPaste ();
	BOOL OnClear ();
};
