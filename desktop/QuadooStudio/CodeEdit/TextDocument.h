#pragma once

#include "Library\Core\BaseUnknown.h"
#include "Library\DataSequence.h"
#include "Published\CodeEdit.h"

class CTextIterator;

class CTextDocument :
	public CBaseUnknown,
	public ITextDocument
{
public:
	CDataSequence m_seq;
	TArray<size_w> m_aLines;
	INT m_nTabWidth;
	size_w m_cchLongestLine;
	sysint m_nSnapshotUndo, m_nSnapshotRedo;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(ITextDocument)
	END_UNK_MAP

public:
	CTextDocument (INT nTabWidth);
	~CTextDocument ();

	static HRESULT Create (INT nTabWidth, __deref_out ITextDocument** ppTextDoc);

	HRESULT Initialize (VOID);

	// ITextDocument
	IFACEMETHOD(Load) (__in_ecount_opt(cchText) PCWSTR pcwzText, INT cchText);
	IFACEMETHOD_(VOID, Clear) (VOID);
	IFACEMETHOD_(size_w, Size) () const { return m_seq.size(); }
	IFACEMETHOD_(bool, IsPrepared) (VOID) const { return m_seq.is_prepared(); }

	IFACEMETHOD_(bool, CanUndo) (VOID) const { return m_seq.canundo(); }
	IFACEMETHOD_(bool, CanRedo) (VOID) const { return m_seq.canredo(); }

	IFACEMETHOD(Insert) (size_w index, PCWSTR pcwzText, INT cchText);
	IFACEMETHOD(Replace) (size_w index, PCWSTR pcwzText, INT cchText, size_w erase_length);
	IFACEMETHOD(Erase) (size_w index, size_w erase_length);
	IFACEMETHOD_(VOID, Group) (VOID) { m_seq.group(); }
	IFACEMETHOD_(VOID, Ungroup) (VOID) { m_seq.ungroup(); }
	IFACEMETHOD_(VOID, Break) (VOID) { m_seq.breakopt(); }

	IFACEMETHOD(Undo) (__out ULONG* offset_start, __out ULONG* offset_end);
	IFACEMETHOD(Redo) (__out ULONG* offset_start, __out ULONG* offset_end);

	IFACEMETHOD_(bool, IsModified) (VOID);
	IFACEMETHOD_(VOID, ResetModifiedSnapshot) (VOID);

	IFACEMETHOD_(ULONG, LineCount) (VOID) const { return 0 < m_aLines.Length() ? static_cast<ULONG>(m_aLines.Length() - 1) : 0; }
	IFACEMETHOD_(size_w, LongestLine) (VOID) const { return m_cchLongestLine; }
	IFACEMETHOD_(bool, GetLineFromOffset) (size_w index, __out ULONG* pnLine, __out ULONG* pnOffset);
	IFACEMETHOD_(size_w, LineOffset) (ULONG nLine) const { return m_aLines[nLine]; }
	IFACEMETHOD_(size_w, LineLength) (ULONG nLine) const;
	IFACEMETHOD(Render) (size_w index, __out_ecount(len) seqchar_t* buf, size_w len, __out size_w* pnCopied) const { return m_seq.render(index, buf, len, pnCopied); }
	IFACEMETHOD(StreamOut) (__out ISequentialStream* pstmText) const { return m_seq.StreamOut(pstmText); }
	IFACEMETHOD_(INT, GetTabWidth) (VOID) const { return m_nTabWidth; }

	CTextIterator Iterate (ULONG offset_chars);
	CTextIterator IterateLine (ULONG lineno, ULONG* linestart, ULONG* linelen);

	HRESULT Update (VOID);
};

class CTextIterator
{
public:
	// default constructor sets all members to zero
	CTextIterator ()
		: text_doc(NULL), off_bytes(0), len_bytes(0)
	{
	}

	CTextIterator (ULONG off, ULONG len, CTextDocument* td)
		: text_doc(td), off_bytes(off), len_bytes(len)
	{
		text_doc->AddRef();
	}

	~CTextIterator ()
	{
		SafeRelease(text_doc);
	}

	// default copy-constructor
	CTextIterator (const CTextIterator &ti)
		: off_bytes(ti.off_bytes), len_bytes(ti.len_bytes)
	{
		SetInterface(text_doc, ti.text_doc);
	}

	// assignment operator
	CTextIterator& operator= (CTextIterator &ti)
	{
		ReplaceInterface(text_doc, ti.text_doc);
		off_bytes = ti.off_bytes;
		len_bytes = ti.len_bytes;
		return *this;
	}

	ULONG gettext (WCHAR* buf, ULONG buflen)
	{
		if(text_doc)
		{
			size_w nCopied;

			if(buflen > len_bytes)
				buflen = len_bytes;

			// get text from the TextDocument at the specified byte-offset
			if(SUCCEEDED(text_doc->m_seq.render(off_bytes, buf, buflen, &nCopied)))
			{
				// adjust the iterator's internal position
				off_bytes += static_cast<ULONG>(nCopied);
				len_bytes -= static_cast<ULONG>(nCopied);
			}
			else
				buflen = 0;

			return buflen;
		}
		return 0;
	}

	operator bool()
	{
		return text_doc ? true : false;
	}

private:
	CTextDocument* text_doc;

	ULONG off_bytes;
	ULONG len_bytes;
};
