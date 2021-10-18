#pragma once

namespace DIBDrawing
{
	VOID NormalizeRect (RECT* lprc);
	BOOL AlphaFill24 (LPBYTE lpPtr, INT xView, INT yView, LONG lPitch, const RECT* lprc, COLORREF cr, BYTE bAlpha);
	BOOL InvertRect (LPBYTE lpPtr, INT xView, INT yView, LONG lPitch, const RECT* lprc);
	COLORREF BlendAdditive (COLORREF crA, COLORREF crB);
}
