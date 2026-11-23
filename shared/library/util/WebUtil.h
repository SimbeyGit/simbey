#pragma once

#include "Library\Core\MemoryStream.h"
#include "RString.h"

namespace WebUtil
{
	HRESULT FindHeader (PCSTR pcszHeaders, INT cchHeaders, PCSTR pcszName, INT cchName, __out INT* pnStart, __out INT* pnEnd);
	HRESULT ReadSubPartA (PCSTR pcszHeader, INT cchHeader, PCSTR pcszName, INT cchName, __out PCSTR* ppcszSubPart, __out INT* pcchSubPart);
	HRESULT ReadSubPartW (PCSTR pcszHeader, INT cchHeader, PCSTR pcszName, INT cchName, __out RSTRING* prstrSubPartW);
	INT UrlUnescapePart (PSTR pszPart, INT cchPart);
	HRESULT UrlEscapePart (PCWSTR pcwzPart, INT cchPart, __out CMemoryStream& stmEscapedW);

	HRESULT URLEncode (RSTRING rstrText, __out ISequentialStream* pstmEncoded);
	HRESULT URLDecode (RSTRING rstrText, __out ISequentialStream* pstmDecoded);

	HRESULT URLEncodeA (PCSTR pcszText, INT cchText, __out ISequentialStream* pstmEncoded);
	HRESULT URLDecodeA (PCSTR pcszText, INT cchText, __out ISequentialStream* pstmDecoded);

	inline BOOL IsUrlAttribute (PCSTR pcszHTML, INT cchHTML, __out INT* pcchAttrib, __out CHAR* pchTerm)
	{
		static const struct
		{
			PCSTR pcszAttrib;
			INT cchAttrib;
		} c_rgAttribs[] =
		{
			{ SLP("SRC=") },
			{ SLP("BACKGROUND=") },
			{ SLP("url(") }
		};

		for(INT i = 0; i < ARRAYSIZE(c_rgAttribs); i++)
		{
			INT cchAttrib = c_rgAttribs[i].cchAttrib;

			// There's no point comparing if the HTML text isn't LONGER than the attribute.
			if(cchAttrib < cchHTML && TCompareLeftIAssert(pcszHTML, c_rgAttribs[i].pcszAttrib))
			{
				if('"' == pcszHTML[cchAttrib])
					*pchTerm = '"';
				else
					*pchTerm = '(' == pcszHTML[cchAttrib - 1] ? ')' : '>';
				*pcchAttrib = cchAttrib;
				return TRUE;
			}
		}

		return FALSE;
	}

	HRESULT ResolveInlineLinks (PCSTR pcszText, INT cchText, PCSTR pcszLinkPrefix, INT cchLinkPrefix, __out CMemoryStream& stmResolved);
}
