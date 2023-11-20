#pragma once

#ifdef	UNICODE
	typedef WCHAR				seqchar_t;
#else
	typedef unsigned char		seqchar_t;
#endif

#ifdef	_WIN64
	typedef unsigned __int64	size_w;
#else
	typedef unsigned long		size_w;
#endif

#include "UspAttr.h"

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
#define TXC_MARGIN_BORDER		8			// margin border
#define TXC_MARGIN				9			// margin background
#define TXC_LONGLINETEXT		10			// long-line text
#define TXC_LONGLINE			11			// long-line background
#define TXC_CURRENTLINETEXT		12			// active line text
#define TXC_CURRENTLINE			13			// active line background
#define TXC_DISABLED			14			// disabled window background
#define TXC_MAX_COLORS			15			// keep this updated!

//
//	Text Editor Notification Messages defined here - 
//	sent via the WM_NOTIFY message
//
#define TVN_BASE				500
#define TVN_CURSOR_CHANGE		(TVN_BASE + 0)
#define TVN_EDITMODE_CHANGE		(TVN_BASE + 1)
#define	TVN_MARGIN_CLICK		(TVN_BASE + 2)
#define TVN_SYNTAX_HIGHLIGHT	(TVN_BASE + 3)
#define TVN_INIT_CONTEXT_MENU	(TVN_BASE + 4)
#define	TVN_CLOSE_CONTEXT_MENU	(TVN_BASE + 5)
#define TVN_ENTER_CHAR			(TVN_BASE + 6)

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

struct TVNCURSORINFO : NMHDR
{
	ULONG	nLineNo;
	ULONG	nColumnNo;
	ULONG	nOffset;
};

struct TVNENTERCHAR : TVNCURSORINFO
{
	WCHAR wch;
};

struct TVNMARGINCLICK : NMHDR
{
	ULONG	nLineNo;
	BOOL	fHandled;
};

struct TVNSYNTAXHIGHLIGHT : NMHDR
{
	ULONG	nLineNo;
	PCWSTR	pcwzText;
	INT		nTextLen;
	PATTR	pAttr;
};

struct TVNMCONTEXTMENU : NMHDR
{
	HMENU	hMenu;
	POINT	ptClient;
	ULONG	nLine;
	ULONG	nOffset;
	INT		xCaretPos;
	ULONG	nWordOffset;
	PCWSTR	pcwzWord;
	PVOID	pvUserParam;
};

struct TVNMCLOSECONTEXT : NMHDR
{
	PVOID	pvUserParam;
};

//
//
//	TextView Window Styles defined here
//	(set using TXM_SETSTYLE)
//
#define TXS_SELMARGIN			1
#define TXS_LEFTMARGIN			2
#define TXS_LONGLINES			4
#define TXS_HIGHLIGHTCURLINE	8

// maximum fonts that a TextView can hold
#define MAX_FONTS 32

enum SELMODE { SEL_NONE, SEL_NORMAL, SEL_MARGIN, SEL_BLOCK };

//
//	TextView edit modes
//
#define MODE_READONLY	0
#define MODE_INSERT		1
#define MODE_OVERWRITE	2

interface __declspec(uuid("716C76FE-ED58-43d7-8719-457162C426B4")) ITextDocument : IUnknown
{
	STDMETHOD(Load) (__in_ecount_opt(cchText) PCWSTR pcwzText, INT cchText) = 0;
	STDMETHOD_(VOID, Clear) (VOID) = 0;
	STDMETHOD_(size_w, Size) () const = 0;
	STDMETHOD_(bool, IsPrepared) (VOID) const = 0;

	STDMETHOD_(bool, CanUndo) (VOID) const = 0;
	STDMETHOD_(bool, CanRedo) (VOID) const = 0;

	STDMETHOD(Insert) (size_w index, PCWSTR pcwzText, INT cchText) = 0;
	STDMETHOD(Replace) (size_w index, PCWSTR pcwzText, INT cchText, size_w erase_length) = 0;
	STDMETHOD(Erase) (size_w index, size_w erase_length) = 0;
	STDMETHOD_(VOID, Group) (VOID) = 0;
	STDMETHOD_(VOID, Ungroup) (VOID) = 0;
	STDMETHOD_(VOID, Break) (VOID) = 0;

	STDMETHOD(Undo) (__out ULONG* offset_start, __out ULONG* offset_end) = 0;
	STDMETHOD(Redo) (__out ULONG* offset_start, __out ULONG* offset_end) = 0;

	STDMETHOD_(bool, IsModified) (VOID) = 0;
	STDMETHOD_(VOID, ResetModifiedSnapshot) (VOID) = 0;

	STDMETHOD_(ULONG, LineCount) (VOID) const = 0;
	STDMETHOD_(size_w, LongestLine) (VOID) const = 0;
	STDMETHOD_(bool, GetLineFromOffset) (size_w index, __out ULONG* pnLine, __out ULONG* pnOffset) = 0;
	STDMETHOD_(size_w, LineOffset) (ULONG nLine) const = 0;
	STDMETHOD_(size_w, LineLength) (ULONG cLine) const = 0;
	STDMETHOD(Render) (size_w index, __out_ecount(len) seqchar_t* buf, size_w len, __out size_w* pnCopied) const = 0;
	STDMETHOD(StreamOut) (__out ISequentialStream* pstmText) const = 0;
	STDMETHOD_(INT, GetTabWidth) (VOID) const = 0;
};

interface __declspec(uuid("CF4C929F-A311-40ec-90D4-45AE343597B7")) ICodeEditor : IUnknown
{
	STDMETHOD_(ITextDocument*, GetTextDocument) (VOID) = 0;
	STDMETHOD_(VOID, SetTextDocument) (ITextDocument* pDocument) = 0;

	STDMETHOD_(VOID, GetTextEditView) (TEXT_EDIT_VIEW* ptev) = 0;
	STDMETHOD_(VOID, SetTextEditView) (const TEXT_EDIT_VIEW* pctev) = 0;
	STDMETHOD_(VOID, ResetTextEditView) (VOID) = 0;

	STDMETHOD(Prepare) (__in_ecount_opt(cchText) PCWSTR pcwzText, INT cchText) = 0;
	STDMETHOD_(VOID, EnableEditor) (BOOL fEnable) = 0;
	STDMETHOD_(VOID, SetFocus) (VOID) = 0;

	STDMETHOD_(bool, IsModified) (VOID) = 0;
	STDMETHOD_(bool, Undo) (VOID) = 0;
	STDMETHOD_(bool, Redo) (VOID) = 0;
	STDMETHOD_(bool, CanUndo) (VOID) = 0;
	STDMETHOD_(bool, CanRedo) (VOID) = 0;

	STDMETHOD_(ULONG, SelectionSize) (VOID) = 0;
	STDMETHOD_(bool, SelectAll) (VOID) = 0;

	STDMETHOD_(VOID, ScrollView) (INT xCaret, ULONG nLine) = 0;
	STDMETHOD_(VOID, SetDarkMode) (bool fDarkMode, bool fUseSystemColors) = 0;
	STDMETHOD(DisplayOptions) (__inout_ecount_opt(cCustom) COLORREF* prgCustom, INT cCustom, __out_opt BOOL* pfChanged) = 0;

	STDMETHOD_(ULONG, GetStyleMask) (ULONG uMask) = 0;
	STDMETHOD_(ULONG, SetStyleMask) (ULONG uMask, ULONG uStyles) = 0;

	STDMETHOD(AdjustIndentation) (ULONG nLine, INT nIndentation) = 0;
};

HRESULT WINAPI CodeEditRegister (VOID);
HRESULT WINAPI CodeEditUnregister (VOID);
HRESULT WINAPI CodeEditCreate (HWND hwndParent, const RECT& rcSite, INT nTabWidth, bool fDarkMode, bool fUseSystemColors, __deref_out ICodeEditor** ppEditor);
