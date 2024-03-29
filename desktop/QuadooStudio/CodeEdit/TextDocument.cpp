#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "TextDocument.h"

CTextDocument::CTextDocument (INT nTabWidth) :
	m_nTabWidth(nTabWidth),
	m_cchLongestLine(0),
	m_nSnapshotUndo(0),
	m_nSnapshotRedo(0)
{
}

CTextDocument::~CTextDocument ()
{
}

HRESULT CTextDocument::Initialize (VOID)
{
	return m_seq.Initialize();
}

HRESULT CTextDocument::Create (INT nTabWidth, __deref_out ITextDocument** ppTextDoc)
{
	HRESULT hr;
	CTextDocument* pTextDoc = __new CTextDocument(nTabWidth);

	CheckAlloc(pTextDoc);
	Check(pTextDoc->Initialize());
	*ppTextDoc = pTextDoc;
	pTextDoc = NULL;

Cleanup:
	SafeRelease(pTextDoc);
	return hr;
}

IFACEMETHODIMP CTextDocument::Load (__in_ecount_opt(cchText) PCWSTR pcwzText, INT cchText)
{
	HRESULT hr;

	if(0 < cchText)
		Check(m_seq.prepare(pcwzText, cchText));
	else
		Check(m_seq.prepare());

	Check(Update());

Cleanup:
	return hr;
}

IFACEMETHODIMP_(VOID) CTextDocument::Clear (VOID)
{
	m_seq.clear();
	m_aLines.Clear();
	m_cchLongestLine = 0;
}

IFACEMETHODIMP CTextDocument::Insert (size_w index, PCWSTR pcwzText, INT cchText)
{
	HRESULT hr = m_seq.insert(index, pcwzText, cchText);
	if(SUCCEEDED(hr))
		Update();
	return hr;
}

IFACEMETHODIMP CTextDocument::Replace (size_w index, PCWSTR pcwzText, INT cchText, size_w erase_length)
{
	HRESULT hr = m_seq.replace(index, pcwzText, cchText, erase_length);
	if(SUCCEEDED(hr))
		Update();
	return hr;
}

IFACEMETHODIMP CTextDocument::Erase (size_w index, size_w erase_length)
{
	HRESULT hr = m_seq.erase(index, erase_length);
	if(SUCCEEDED(hr))
		Update();
	return hr;
}

IFACEMETHODIMP CTextDocument::Undo (__out ULONG* offset_start, __out ULONG* offset_end)
{
	HRESULT hr = m_seq.undo();
	if(SUCCEEDED(hr))
	{
		*offset_start = static_cast<ULONG>(m_seq.event_index());
		*offset_end = static_cast<ULONG>(*offset_start + m_seq.event_length());

		Update();
	}
	return hr;
}

IFACEMETHODIMP CTextDocument::Redo (__out ULONG* offset_start, __out ULONG* offset_end)
{
	HRESULT hr = m_seq.redo();
	if(SUCCEEDED(hr))
	{
		*offset_start = static_cast<ULONG>(m_seq.event_index());
		*offset_end = static_cast<ULONG>(*offset_start + m_seq.event_length());

		Update();
	}
	return hr;
}

IFACEMETHODIMP_(bool) CTextDocument::IsModified (VOID)
{
	sysint nSnapshotUndo, nSnapshotRedo;

	m_seq.event_pair(&nSnapshotUndo, &nSnapshotRedo);
	return nSnapshotUndo != m_nSnapshotUndo || nSnapshotRedo != m_nSnapshotRedo;
}

IFACEMETHODIMP_(VOID) CTextDocument::ResetModifiedSnapshot (VOID)
{
	m_seq.event_pair(&m_nSnapshotUndo, &m_nSnapshotRedo);
}

IFACEMETHODIMP_(bool) CTextDocument::GetLineFromOffset (size_w index, __out ULONG* pnLine, __out ULONG* pnOffset)
{
	sysint low  = 0;
	sysint high = LineCount() - 1;
	sysint line = 0;

	if(high == -1)
	{
		if(pnLine)			*pnLine			= 0;
		if(pnOffset)		*pnOffset		= 0;
		return false;
	}

	while(low <= high)
	{
		line = (high + low) / 2;

		if(index >= m_aLines[line] && index < m_aLines[line + 1])
		{
			break;
		}
		else if(index < m_aLines[line])
		{
			high = line-1;
		}
		else
		{
			low = line+1;
		}
	}

	if(pnLine)				*pnLine			= static_cast<ULONG>(line);
	if(pnOffset)			*pnOffset		= static_cast<ULONG>(m_aLines[line]);

	return true;
}

IFACEMETHODIMP_(size_w) CTextDocument::LineLength (ULONG nLine) const
{
	if(nLine < LineCount())
		return m_aLines[nLine + 1] - m_aLines[nLine];
	return 0;
}

CTextIterator CTextDocument::Iterate (ULONG offset_chars)
{
	return CTextIterator(offset_chars, static_cast<ULONG>(m_seq.size() - offset_chars), this);
}

CTextIterator CTextDocument::IterateLine (ULONG lineno, ULONG* linestart, ULONG* linelen)
{
	if(static_cast<sysint>(lineno) >= LineCount() || !GetLineFromOffset(m_aLines[lineno], NULL, linestart))
	{
		*linestart = 0;
		*linelen = 0;
		return CTextIterator();
	}

	*linelen = static_cast<ULONG>(LineLength(lineno));
	return CTextIterator(*linestart, *linelen, this);
}

HRESULT CTextDocument::Update (VOID)
{
	m_cchLongestLine = m_seq.longest_line(L'\n', L'\t', m_nTabWidth);
	m_aLines.Clear();
	return m_seq.render_offsets(m_aLines, L'\n');
}
