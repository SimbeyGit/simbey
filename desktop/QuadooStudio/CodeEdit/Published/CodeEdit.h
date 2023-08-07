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
#define TXC_LINENUMBERTEXT		8			// line number text
#define TXC_LINENUMBER			9			// line number background
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

struct TVNMARGINCLICK : NMHDR
{
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

struct TVNMCONTEXTMENU : NMHDR
{
	HMENU	hMenu;
	POINT	ptClient;
	ULONG	nLine;
	ULONG	nOffset;
	INT		xCaretPos;
	ULONG	nWordOffset;
	PCWSTR	pcwzWord;
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

interface __declspec(uuid("716C76FE-ED58-43d7-8719-457162C426B4")) ITextDocument : IUnknown
{
	virtual HRESULT Load (__in_ecount_opt(cchText) PCWSTR pcwzText, INT cchText) = 0;
	virtual VOID Clear (VOID) = 0;
	virtual size_w Size () const = 0;
	virtual bool IsPrepared (VOID) const = 0;

	virtual bool CanUndo (VOID) const = 0;
	virtual bool CanRedo (VOID) const = 0;

	virtual HRESULT Insert (size_w index, PCWSTR pcwzText, INT cchText) = 0;
	virtual HRESULT Replace (size_w index, PCWSTR pcwzText, INT cchText, size_w erase_length) = 0;
	virtual HRESULT Erase (size_w index, size_w erase_length) = 0;
	virtual VOID Group (VOID) = 0;
	virtual VOID Ungroup (VOID) = 0;
	virtual VOID Break (VOID) = 0;

	virtual HRESULT Undo (__out ULONG* offset_start, __out ULONG* offset_end) = 0;
	virtual HRESULT Redo (__out ULONG* offset_start, __out ULONG* offset_end) = 0;

	virtual bool IsModified (VOID) = 0;
	virtual VOID ResetModifiedSnapshot (VOID) = 0;

	virtual ULONG LineCount (VOID) const = 0;
	virtual size_w LongestLine (VOID) const = 0;
	virtual bool GetLineFromOffset (size_w index, __out ULONG* pnLine, __out ULONG* pnOffset) = 0;
	virtual size_w LineOffset (ULONG nLine) const = 0;
	virtual size_w LineLength (ULONG cLine) const = 0;
	virtual HRESULT Render (size_w index, __out_ecount(len) seqchar_t* buf, size_w len, __out size_w* pnCopied) const = 0;
	virtual HRESULT StreamOut (__out ISequentialStream* pstmText) const = 0;
	virtual INT GetTabWidth (VOID) const = 0;
};

interface __declspec(uuid("CF4C929F-A311-40ec-90D4-45AE343597B7")) ICodeEditor : IUnknown
{
	virtual ITextDocument* GetTextDocument (VOID) = 0;
	virtual VOID SetTextDocument (ITextDocument* pDocument) = 0;

	virtual VOID GetTextEditView (TEXT_EDIT_VIEW* ptev) = 0;
	virtual VOID SetTextEditView (const TEXT_EDIT_VIEW* pctev) = 0;
	virtual VOID ResetTextEditView (VOID) = 0;

	virtual HRESULT Prepare (__in_ecount_opt(cchText) PCWSTR pcwzText, INT cchText) = 0;
	virtual VOID EnableEditor (BOOL fEnable) = 0;
	virtual VOID SetFocus (VOID) = 0;

	virtual bool IsModified (VOID) = 0;
	virtual BOOL Undo (VOID) = 0;
	virtual BOOL Redo (VOID) = 0;
	virtual bool CanUndo (VOID) = 0;
	virtual bool CanRedo (VOID) = 0;

	virtual ULONG SelectionSize (VOID) = 0;
	virtual BOOL SelectAll (VOID) = 0;

	virtual VOID ScrollView (INT xCaret, ULONG nLine) = 0;
};

HRESULT WINAPI CodeEditRegister (VOID);
HRESULT WINAPI CodeEditUnregister (VOID);
HRESULT WINAPI CodeEditCreate (HWND hwndParent, const RECT& rcSite, INT nTabWidth, __deref_out ICodeEditor** ppEditor);
