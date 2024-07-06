#pragma once

RGBQUAD GetPixelColor (const BYTE* pImage, int w, int h, int x, int y);
void SetPixelColor (BYTE* pImage, int w, int h, int x, int y, RGBQUAD c);
void CopyBits (const BYTE* pSrcBits, int w, int h, BYTE* pDestBits, int w1, int h1, int xDest, int yDest, int xScrollPos, int yScrollPos, float fZoom);
