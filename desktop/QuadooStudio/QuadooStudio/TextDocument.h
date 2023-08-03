#pragma once

#include "Library\DataSequence.h"

class CTextIterator;

class CTextDocument
{
private:
	ULONG m_cRef;

public:
	CDataSequence m_seq;
	TArray<size_w> m_aLines;
	INT m_nTabWidth;
	size_w m_cchLongestLine;
	sysint m_nSnapshotUndo, m_nSnapshotRedo;

public:
	CTextDocument (INT nTabWidth);
	~CTextDocument ();

	HRESULT Initialize (VOID);
	HRESULT Load (__in_ecount_opt(cchText) PCWSTR pcwzText, INT cchText);
	VOID Clear (VOID);

	ULONG AddRef (VOID);
	ULONG Release (VOID);

	HRESULT Insert (size_w index, PCWSTR pcwzText, INT cchText);
	HRESULT Replace (size_w index, PCWSTR pcwzText, INT cchText, size_w erase_length);
	HRESULT Erase (size_w index, size_w erase_length);

	HRESULT Undo (__out ULONG* offset_start, __out ULONG* offset_end);
	HRESULT Redo (__out ULONG* offset_start, __out ULONG* offset_end);

	bool IsModified (VOID);
	VOID ResetModifiedSnapshot (VOID);

	inline ULONG LineCount (VOID) { return 0 < m_aLines.Length() ? static_cast<ULONG>(m_aLines.Length() - 1) : 0; }
	bool GetLineFromOffset (size_w index, __out ULONG* pnLine, __out ULONG* pnOffset);
	size_w LineLength (ULONG cLine);

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
			if(buflen > len_bytes)
				buflen = len_bytes;

			// get text from the TextDocument at the specified byte-offset
			if(SUCCEEDED(text_doc->m_seq.render(off_bytes, buf, buflen, &buflen)))
			{
				// adjust the iterator's internal position
				off_bytes += buflen;
				len_bytes -= buflen;
			}
			else
				buflen = 0;

			return buflen;
		}
		else
		{
			return 0;
		}
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
