#include <windows.h>
#include "Published\SIF.h"
#include "ImageProc.h"

RGBQUAD GetPixelColor (const BYTE* pImage, int w, int h, int x, int y)
{
	int dwEffWidth = 4 * w;
	RGBQUAD rgb;
	rgb.rgbBlue = 0;
	rgb.rgbGreen = 0;
	rgb.rgbRed = 0;
	rgb.rgbReserved = 0;
	if((x < 0)||(y < 0)|| (x >= w)||(y >= h))
		return rgb;

	const BYTE* iSrc  = pImage + y * dwEffWidth + x * 4;
	rgb.rgbRed = *iSrc++;
	rgb.rgbGreen = *iSrc++;
	rgb.rgbBlue = *iSrc++;
	rgb.rgbReserved = *iSrc;
	return rgb;
}

void SetPixelColor (BYTE* pImage, int w, int h, int x,int y, RGBQUAD c)
{
	if((x < 0)||(y < 0)||(x >= w)||(y >= h))
		return;

	int dwEffWidth = (24 * w + 31) / 32 * 4;
	BYTE* iDst = pImage + y * dwEffWidth + x * 3;
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

void CopyBits (const BYTE* pSrcBits, int w, int h, int srcXDest, int srcYDest, float srcfZoom, BYTE* pDestBits, int w1, int h1, int xDest, int yDest, int xScrollPos, int yScrollPos, float fZoom, int nImageWidth, int nImageHeight)
{
	float fTotalZoom = srcfZoom * fZoom;
	float xScale, yScale, fX, fY;
	xScale = 1.0f / fTotalZoom;
	yScale = 1.0f / fTotalZoom;

	int newW = (int)(w * fTotalZoom);
	int newH = (int)(h * fTotalZoom);

	for(int y = yScrollPos - srcYDest * fZoom; y < newH; y++)
	{
		fY = y * yScale;
		if(h1 - y - yDest + yScrollPos - srcYDest * fZoom < h1 - yDest - nImageHeight || h1 - y - yDest + yScrollPos - srcYDest * fZoom < 0)
			break;
		if(y < 0)
			continue;
		for(int x = xScrollPos - srcXDest * fZoom; x < newW; x++)
		{
			if(x < 0)
				continue;
			fX = x * xScale;
			if(x + xDest + srcXDest * fZoom - xScrollPos > xDest + nImageWidth || x + xDest + srcXDest * fZoom - xScrollPos > w1)
				break;
			SetPixelColor(pDestBits, w1, h1, x + xDest + srcXDest * fZoom - xScrollPos, h1 - y - yDest + yScrollPos - srcYDest * fZoom, GetPixelColor(pSrcBits, w, h, (int)fX,(int)fY));
		}
	}
}
