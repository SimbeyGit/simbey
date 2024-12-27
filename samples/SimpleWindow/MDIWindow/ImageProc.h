#pragma once

void CopyBitsToDIB24 (const BYTE* pSrcBits, const SIZE& szSource, int srcXDest, int srcYDest, BYTE* pDestBits, const SIZE& szDest, const POINT& ptDest, int xScrollPos, int yScrollPos, float fZoom, int nImageWidth, int nImageHeight);
