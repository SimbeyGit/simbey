#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "TemplateData.h"

HRESULT SplitKey (RSTRING rstrKeyW, __out RSTRING* prstrNamespaceW, __out RSTRING* prstrNameW)
{
	HRESULT hr;
	PCWSTR pcwzKey = RStrToWide(rstrKeyW);
	PCWSTR pcwzDot = TStrChr(pcwzKey, L'.');

	CheckIf(NULL == pcwzDot, E_INVALIDARG);
	Check(RStrCreateW(static_cast<INT>(pcwzDot - pcwzKey), pcwzKey, prstrNamespaceW));
	pcwzDot++;
	Check(RStrCreateW(RStrLen(rstrKeyW) - (RStrLen(*prstrNamespaceW) + 1), pcwzDot, prstrNameW));

Cleanup:
	return hr;
}
