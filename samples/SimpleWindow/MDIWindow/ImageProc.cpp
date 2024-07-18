#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Published\SIF.h"
#include "ImageProc.h"

RGBQUAD GetPixelColor (const BYTE* pImage, const SIZE& szSource, int x, int y)
{
	int dwEffWidth = 4 * szSource.cx;
	RGBQUAD rgb;
	rgb.rgbBlue = 0;
	rgb.rgbGreen = 0;
	rgb.rgbRed = 0;
	rgb.rgbReserved = 0;

	const BYTE* iSrc = pImage + y * dwEffWidth + x * 4;
	rgb.rgbRed = *iSrc++;
	rgb.rgbGreen = *iSrc++;
	rgb.rgbBlue = *iSrc++;
	rgb.rgbReserved = *iSrc;
	return rgb;
}

void SetPixelColor (BYTE* pTarget, const SIZE& szDest, int x, int y, RGBQUAD c)
{
	int dwEffWidth = (24 * szDest.cx + 31) / 32 * 4;
	BYTE* iDst = pTarget + y * dwEffWidth + x * 3;
	BYTE bAlpha = c.rgbReserved;
	if(255 == bAlpha)
	{
		*iDst++ = c.rgbBlue;
		*iDst++ = c.rgbGreen;
		*iDst   = c.rgbRed;
	}
	else
	{
		iDst[0] = sifBlendColorComponents(iDst[0], c.rgbBlue, bAlpha);
		iDst[1] = sifBlendColorComponents(iDst[1], c.rgbGreen, bAlpha);
		iDst[2] = sifBlendColorComponents(iDst[2], c.rgbRed, bAlpha);
	}
}

void CopyBits (const BYTE* pSrcBits, const SIZE& szSource, int srcXDest, int srcYDest, BYTE* pDestBits, const SIZE& szDest, int xDest, int yDest, int xScrollPos, int yScrollPos, float fZoom, int nImageWidth, int nImageHeight)
{
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
		if(szDest.cy - y - yDest + yScrollPos - ryDestZoom < szDest.cy - yDest - nImageHeight || szDest.cy - y - yDest + yScrollPos - ryDestZoom < 0)
			break;

		int yDestRow = szDest.cy - y - yDest + yScrollPos - ryDestZoom;
		if(yDestRow < szDest.cy)
		{
			float fY = y * rScale;
			for(int x = xStart; x < newW; x++)
			{
				if(x + xDest + rxDestZoom - xScrollPos > xDest + nImageWidth || x + xDest + rxDestZoom - xScrollPos > szDest.cx)
					break;

				int xDestColumn = x + xDest + rxDestZoom - xScrollPos;
				if(xDestColumn >= szDest.cx)
					break;

				float fX = x * rScale;
				SetPixelColor(pDestBits, szDest, xDestColumn, yDestRow, GetPixelColor(pSrcBits, szSource, (int)fX, (int)fY));
			}
		}
	}
}
