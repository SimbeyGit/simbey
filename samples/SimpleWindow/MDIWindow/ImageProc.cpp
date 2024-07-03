#include "ImageProc.h"

RGBQUAD GetPixelColor (BYTE* pImage, int w, int h, int x, int y)
{
	int dwEffWidth = (24 * w + 31) / 32 * 4;
	RGBQUAD rgb;
	rgb.rgbBlue = 0;
	rgb.rgbGreen = 0;
	rgb.rgbRed = 0;
	rgb.rgbReserved = 0;
	if ((x < 0)||(y < 0)|| (x >= w)||(y >= h)){
		return rgb;
	}
	BYTE* iDst  = pImage + y * dwEffWidth + x * 3;
	rgb.rgbBlue = *iDst++;
	rgb.rgbGreen= *iDst++;
	rgb.rgbRed  = *iDst;
	return rgb;
}

void SetPixelColor (BYTE* pImage, int w, int h, int x,int y, RGBQUAD c)
{
	if ((x < 0)||(y < 0)||(x >= w)||(y >= h)) 
		return;

	int dwEffWidth = (24 * w + 31) / 32 * 4;
	BYTE* iDst = pImage + y * dwEffWidth + x * 3;
	*iDst++ = c.rgbBlue;
	*iDst++ = c.rgbGreen;
	*iDst   = c.rgbRed;
}

void CopyBits (BYTE* pSrcBits, int w, int h, BYTE* pDestBits, int w1, int h1, int xDest, int yDest, int xScrollPos, int yScrollPos, float fZoom)
{
	float xScale, yScale, fX, fY;
	xScale = 1.0f  / fZoom;
	yScale = 1.0f / fZoom;

	int newW = w * fZoom;
	int newH = h * fZoom;

	if(newW <= w1 && newH <= h1)
	{
		for(int y = yDest; y < h1 - yDest; y++)
		{
			fY = (y - yDest) * yScale;
			for(int x =  xDest; x < xDest + w * fZoom; x++)
			{
				fX = (x - xDest) * xScale;	
				SetPixelColor(pDestBits, w1, h1, x, y, GetPixelColor(pSrcBits, w, h, (int)fX,(int)fY));
			}
		}
	}
	else if(newW > w1 && newH <= h1)
	{
		for(int y = yDest; y < h1 - yDest; y++)
		{
			fY = (y - yDest) * yScale;
			for(int x =  xScrollPos; x < xScrollPos + w1; x++)
			{
				fX = x * xScale;	
				SetPixelColor(pDestBits, w1, h1, x - xScrollPos, y, GetPixelColor(pSrcBits, w, h, (int)fX,(int)fY));
			}
		}
	}
	else if(newW <= w1 && newH > h1)
	{
		for(int y = h1 - 1; y >= 0; y--)
		{
			fY = (newH - 1 - yScrollPos - (h1 - 1 - y)) * yScale;
			for(int x =  xDest; x < xDest + newW; x++)
			{
				fX = (x - xDest) * xScale;
				SetPixelColor(pDestBits, w1, h1, x, y, GetPixelColor(pSrcBits, w, h, (int)fX, (int)fY));
			}
		}
	}
	else if(newW > w1 && newH > h1)
	{
		for(int y = h1 - 1; y >= 0; y--)
		{
			fY = (newH - 1 - yScrollPos - (h1 - 1 - y)) * yScale;
			for(int x =  xScrollPos; x < xScrollPos + w1; x++)
			{
				fX = x * xScale;
				SetPixelColor(pDestBits, w1, h1, x - xScrollPos, y, GetPixelColor(pSrcBits, w, h, (int)fX, (int)fY));
			}
		}
	}
}
