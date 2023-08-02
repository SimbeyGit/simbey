#pragma once

#include "Library\DataSequence.h"

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

	bool GetLineFromOffset (size_w index, __out ULONG* pnLine, __out ULONG* pnOffset);
	size_w LineLength (sysint cLine);

	HRESULT Update (VOID);
};
