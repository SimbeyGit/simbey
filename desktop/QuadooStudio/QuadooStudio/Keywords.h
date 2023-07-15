#pragma once

struct KEYWORD
{
	PCWSTR pcwzKeyword;
	INT cchKeyword;
	COLORREF crKeyword;
};

VOID GetKeywords (__deref_out_ecount(*pcKeywords) const KEYWORD** prgKeywords, __out INT* pcKeywords);
