#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Published\SIF.h"
#include "ImageProc.h"

struct COPY_WIDTHS
{
	LONG xSource;
	LONG xTarget;
};

inline VOID CopyPixelToDIB24 (const COPY_WIDTHS& cw, const BYTE* pSource, INT xSrcPixel, INT ySrcPixel, BYTE* pTarget, INT xTarget, INT yTarget)
{
	const BYTE* iSrc = pSource + ySrcPixel * cw.xSource + xSrcPixel * 4;

	BYTE* iDst = pTarget + yTarget * cw.xTarget + xTarget * 3;
	BYTE bAlpha = iSrc[3];
	if(255 == bAlpha)
	{
		*iDst++ = iSrc[2];
		*iDst++ = iSrc[1];
		*iDst   = iSrc[0];
	}
	else
	{
		iDst[0] = sifBlendColorComponents(iDst[0], iSrc[2], bAlpha);
		iDst[1] = sifBlendColorComponents(iDst[1], iSrc[1], bAlpha);
		iDst[2] = sifBlendColorComponents(iDst[2], iSrc[0], bAlpha);
	}
}

void CopyBitsToDIB24 (const BYTE* pSrcBits, const SIZE& szSource, int srcXDest, int srcYDest, BYTE* pDestBits, const SIZE& szDest, const POINT& ptDest, int xScrollPos, int yScrollPos, float fZoom, int nImageWidth, int nImageHeight)
{
	COPY_WIDTHS cw =
	{ 
		4 * szSource.cx,
		(24 * szDest.cx + 31) / 32 * 4
	};
	float rScale = 1.0f / fZoom;

	int newW = (int)(szSource.cx * fZoom);
	int newH = (int)(szSource.cy * fZoom);

	float rxDestZoom = srcXDest * fZoom;
	float ryDestZoom = srcYDest * fZoom;

	// Precompute the starting position for x
	int xStart = (int)((float)xScrollPos - rxDestZoom);
	if(xStart < 0)
		xStart = 0;

	// Precompute the starting position for y
	int yStart = (int)((float)yScrollPos - ryDestZoom);
	if(yStart < 0)
		yStart = 0;

	for(int y = yStart; y < newH; y++)
	{
		if(szDest.cy - y - ptDest.y + yScrollPos - ryDestZoom < szDest.cy - ptDest.y - nImageHeight || szDest.cy - y - ptDest.y + yScrollPos - ryDestZoom < 0)
			break;

		int yDestRow = szDest.cy - y - ptDest.y + yScrollPos - ryDestZoom;
		if(yDestRow < szDest.cy)
		{
			float fY = (float)y * rScale;
			int ySrcPixel = (int)fY;

			for(int x = xStart; x < newW; x++)
			{
				if(x + ptDest.x + rxDestZoom - xScrollPos > ptDest.x + nImageWidth || x + ptDest.x + rxDestZoom - xScrollPos > szDest.cx)
					break;

				int xDestColumn = x + ptDest.x + rxDestZoom - xScrollPos;
				if(xDestColumn >= szDest.cx)
					break;

				float fX = (float)x * rScale;
				CopyPixelToDIB24(cw, pSrcBits, (int)fX, ySrcPixel, pDestBits, xDestColumn, yDestRow);
			}
		}
	}
}
