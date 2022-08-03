#pragma once

#ifdef	USE_AMD_EGL
	#include "amd\gles2\gl2.h"
#else
	#include <gl\gl.h>
#endif

#include "SIF.h"

interface __declspec(uuid("051EC929-A365-483b-9BF8-753C14706359")) ISimbeyInterchangeFileGLFont : IUnknown
{
	virtual HRESULT RecreateTexture (BYTE bRed, BYTE bGreen, BYTE bBlue, BYTE iAlpha) = 0;
	virtual HRESULT MeasureText (PCWSTR pcwzText, __out FLOAT& rWidth, __out FLOAT& rHeight) = 0;
	virtual HRESULT GetGlyph (CHAR ch, __deref_out ISimbeyInterchangeFileLayer** ppGlyph) = 0;
	virtual HRESULT DrawTextGL (PCWSTR pcwzText, FLOAT rScale, UINT nFormat) = 0;
};

HRESULT WINAPI sifCreateOpenGLTexture (__in_bcount(nWidth * nHeight * 4) PBYTE pBits32, INT nWidth, INT nHeight, __out UINT* pnTexture);
HRESULT WINAPI sifCopyBits32ToOpenGLTexture (__in_bcount(nWidth * nHeight * 4) PBYTE pBits32, INT nWidth, INT nHeight, UINT nTexture, INT xOffset, INT yOffset);
HRESULT WINAPI sifCopyDIBToOpenGLTexture (PBYTE pDIB, INT nWidth, INT nHeight, INT cBitsPerPixel, UINT nTexture, INT xOffset, INT yOffset);
HRESULT WINAPI sifDrawDIBToGLFrameBuffer (PBYTE pDIB, INT nWidth, INT nHeight, INT cBitsPerPixel);

HRESULT WINAPI sifGetGLTexturePositionF (ISimbeyInterchangeFileLayer* pLayer, __out FLOAT* prLeft, __out FLOAT* prTop, __out FLOAT* prRight, __out FLOAT* prBottom);
HRESULT WINAPI sifGetGLTexturePositionD (ISimbeyInterchangeFileLayer* pLayer, __out DOUBLE* pdLeft, __out DOUBLE* pdTop, __out DOUBLE* pdRight, __out DOUBLE* pdBottom);

HRESULT WINAPI sifMergeCanvasToOpenGLTexture32 (ISimbeyInterchangeFile* pSIF, BYTE bRed, BYTE bGreen, BYTE bBlue, BYTE iAlpha, __out UINT* pnTexture);

HRESULT WINAPI sifLoadFontGL (HMODULE hModule, PCWSTR pcwzRes, __deref_out ISimbeyInterchangeFileGLFont** ppFont);
HRESULT WINAPI sifCreateFontGLFromSIF (ISimbeyInterchangeFile* pSIF, BOOL fCloseOnRelease, __deref_out ISimbeyInterchangeFileGLFont** ppFont);

VOID WINAPI sifPushOrthoMode (const SIZE* pcsWindow);
VOID WINAPI sifPopOrthoMode (VOID);
