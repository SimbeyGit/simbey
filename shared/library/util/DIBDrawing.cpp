#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "DIBDrawing.h"

namespace DIBDrawing
{
	VOID NormalizeRect (RECT* lprc)
	{
		if(lprc->left > lprc->right)
		{
			LONG nLeft = lprc->left;
			lprc->left = lprc->right;
			lprc->right = nLeft;
		}

		if(lprc->top > lprc->bottom)
		{
			LONG nTop = lprc->top;
			lprc->top = lprc->bottom;
			lprc->bottom = nTop;
		}
	}

	BOOL AlphaFill24 (LPBYTE lpPtr, INT xView, INT yView, LONG lPitch, const RECT* lprc, COLORREF cr, BYTE bAlpha)
	{
		BOOL fIntersection = FALSE;
		RECT rc, rcView;
		rcView.left = 0;
		rcView.top = 0;
		rcView.right = xView;
		rcView.bottom = yView;
		if(IntersectRect(&rc,&rcView,lprc))
		{
			INT Red = GetRValue(cr) * bAlpha;
			INT Green = GetGValue(cr) * bAlpha;
			INT Blue = GetBValue(cr) * bAlpha;
			INT xSize = rc.right - rc.left;
			INT ySize = rc.bottom - rc.top;
			INT x, y, iAlpha = (INT)bAlpha, iDiff = 255 - iAlpha;
			LPBYTE lpPixel;
			lpPtr += (yView - rc.top - 1) * lPitch + rc.left * 3;
			for(y = 0; y < ySize; y++)
			{
				lpPixel = lpPtr;
				for(x = 0; x < xSize; x++)
				{
					lpPixel[0] = (BYTE)((Blue + lpPixel[0] * iDiff) / 255);
					lpPixel[1] = (BYTE)((Green + lpPixel[1] * iDiff) / 255);
					lpPixel[2] = (BYTE)((Red + lpPixel[2] * iDiff) / 255);
					lpPixel += 3;
				}
				lpPtr -= lPitch;
			}
			fIntersection = TRUE;
		}
		return fIntersection;
	}

	BOOL InvertRect (LPBYTE lpPtr, INT xView, INT yView, LONG lPitch, const RECT* lprc)
	{
		BOOL fIntersection = FALSE;
		RECT rc, rcView;
		rcView.left = 0;
		rcView.top = 0;
		rcView.right = xView;
		rcView.bottom = yView;
		if(IntersectRect(&rc,&rcView,lprc))
		{
			INT xSize = rc.right - rc.left;
			INT ySize = rc.bottom - rc.top;
			INT x, y;

			lpPtr += (yView - rc.top - 1) * lPitch + rc.left * 3;
			for(y = 0; y < ySize; y++)
			{
				PBYTE lpPixel = lpPtr;
				for(x = 0; x < xSize; x++)
				{
					lpPixel[0] = ~lpPixel[0];
					lpPixel[1] = ~lpPixel[1];
					lpPixel[2] = ~lpPixel[2];
					lpPixel += 3;
				}
				lpPtr -= lPitch;
			}
			fIntersection = TRUE;
		}
		return fIntersection;
	}

	COLORREF BlendAdditive (COLORREF crA, COLORREF crB)
	{
		return RGB((GetRValue(crA) + GetRValue(crB)) >> 1,(GetGValue(crA) + GetGValue(crB)) >> 1,(GetBValue(crA) + GetBValue(crB)) >> 1);
	}
}
