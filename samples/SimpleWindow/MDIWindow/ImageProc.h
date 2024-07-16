#pragma once

RGBQUAD GetPixelColor (const BYTE* pImage, int w, int h, int x, int y);
void SetPixelColor (BYTE* pTarget, const SIZE& szDest, int x, int y, RGBQUAD c);
void CopyBits (const BYTE* pSrcBits, const SIZE& szSource, int srcXDest, int srcYDest, BYTE* pDestBits, const SIZE& szDest, int xDest, int yDest, int xScrollPos, int yScrollPos, float fZoom, int nImageWidth, int nImageHeight);
