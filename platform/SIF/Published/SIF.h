#pragma once

// Simbey Interchange Format (SIF)
//
// The SIF library exists as a convenience for handling particular code paths.
// PNGs, BMPs, and JPEGs may all be used as a source and be translated into
// SIF files at build time.  At run time, a SIF is loaded, and DIBs, DDBs, and
// OpenGL textures can be loaded from a SIF.  SIFs can be stored as separate
// files or stored as resources within an EXE or DLL.  Custom loaders can be
// written to load SIFs from 3rd party sources.
//
// There are basically two formats used throughout the SIF library.  There are
// raw bits and DIBs.  When handling 32 bpp bits, the pixels are laid out in
// RGBA format.  Thus, if BYTE bits[4] is defined, then bits[0] is red, bits[1]
// is green, bits[2] is blue, and bits[3] is the alpha channel.  The colors in
// raw bits are not premultiplied with the alpha channel, and the rows are
// stored top-down.  Raw bits are compressed and stored in a SIF file.
//
// The other primary format seen in the SIF library, a 32 bpp DIB, doesn't
// technically exist, at least not on disk.  Software support with GDI is
// tricky at best.  Most GDI routines not only ignore the alpha channel in a 32
// bpp DIB but actually set it to zero when writing pixels.  This was one of
// the motivators for writing the SIF library.  Anyway, 32 bpp DIBs store their
// pixels in BGRA format.  They are usually upside down, and their colors are
// premultiplied with the alpha channel.

// If you wish to use IPicture, then include ocIdl.h
interface IPicture;

// If you wish to use IWICBitmap, then include wincodec.h
interface IWICBitmap;

// If you wish to use ISeekableStream, then include ISeekableStream.h
interface ISeekableStream;

// Forward declaration for ISpriteAnimationCompleted::OnSpriteAnimationCompleted()
interface ISimbeyInterchangeSprite;

// Forward declaration for
interface ISimbeyInterchangeAnimator;

#define	SIF_NO_TRANSPARENCY		0xFF000000

#define	SIF_MAX_LAYER_NAME		256			// 255 characters + 1 terminator

#define	FILL_TYPE_SOLID			0
#define	FILL_TYPE_GRADIENT		1
#define	FILL_TYPE_NONE			2

#define	SIF_RESIZE_NEAREST		0
#define	SIF_RESIZE_LINEAR		1
#define	SIF_RESIZE_BICUBIC		2

typedef struct
{
	BYTE Red;
	BYTE Green;
	BYTE Blue;
} SIFPAL, *PSIFPAL;

typedef struct
{
	BYTE ID[4];		// "SIF2"
	USHORT nSquareSize;
	USHORT cPixelsPerInch;
	USHORT cLayers;
	USHORT iExtra;
} SIFHEAD, *PSIFHEAD;

typedef struct
{
	FILETIME ftStamp;
	DWORD idLayer;
	LONG x, y;
	LONG zIndex;
	USHORT nWidth;
	USHORT nHeight;
	DWORD iDataOffset;
	DWORD nDataSize;
	DWORD dwThisIntegrity;
} SIFLAYER, *PSIFLAYER;

#ifndef	HAS_SIF_LINE_OFFSET
#define	HAS_SIF_LINE_OFFSET

struct SIF_LINE_OFFSET
{
	INT xOffset;	// Drawing beginning with this pixel
	INT cxDraw;		// Draw this many pixels
};

#endif

struct SIF_SURFACE
{
	PBYTE pbSurface;
	INT cBitsPerPixel;
	INT xSize;
	INT ySize;
	LONG lPitch;
};

interface __declspec(uuid("4CC1DB43-C86A-4150-BD01-DC1390CCA42A")) ISimbeyInterchangeFileLayer : IUnknown
{
	STDMETHOD_(DWORD, GetLayerID) (VOID) = 0;

	STDMETHOD(GetSize) (__out SIZE* pSize) = 0;
	STDMETHOD(GetPosition) (__out RECT* prcPosition) = 0;
	STDMETHOD_(VOID, SetPosition) (LONG x, LONG y) = 0;

	// Z Index is based on the position in the array of layers.
	// It will always be 0 <= zIndex < NumberOfLayers
	STDMETHOD_(LONG, GetZIndex) (VOID) = 0;
	STDMETHOD(SetZIndex) (LONG zIndex) = 0;

	STDMETHOD(GetLayerLocation) (__out USHORT* pnSquareSize, __out LONG* px, __out LONG* py, __out USHORT* pnWidth, __out USHORT* pnHeight) = 0;

	STDMETHOD(GetBitsPtr) (__deref_out_bcount(*pcb) PBYTE* ppBits, __out DWORD* pcb) = 0;
	STDMETHOD_(VOID, MarkBitsDirty)(VOID) = 0;

	STDMETHOD(GetName) (__out_ecount(cchMaxName) PWSTR pwzName, INT cchMaxName) = 0;
	STDMETHOD(SetName) (__in_opt PCWSTR pcwzName) = 0;

	STDMETHOD(GetAsDIB) (__in_opt ISimbeyInterchangeFileLayer* pOverlay, __out HBITMAP* phDIB, __out_opt BITMAPINFOHEADER* pbmih = NULL, __out_opt PBYTE* ppBits = NULL) = 0;
	STDMETHOD(GetAsDDB) (HDC hdc, __out HBITMAP* phDDB) = 0;

	STDMETHOD(DrawToDIB32) (SIF_SURFACE* psifSurface32, INT xDest, INT yDest) = 0;
	STDMETHOD(DrawToDIB24) (SIF_SURFACE* psifSurface24, INT xDest, INT yDest) = 0;

	STDMETHOD(DrawClippedToDIB32) (SIF_SURFACE* psifSurface32, INT xDest, INT yDest, const RECT& rcClip) = 0;
	STDMETHOD(DrawClippedToDIB24) (SIF_SURFACE* psifSurface24, INT xDest, INT yDest, const RECT& rcClip) = 0;

	STDMETHOD(DrawGrayToDIB32) (SIF_SURFACE* psifSurface32, INT xDest, INT yDest) = 0;
	STDMETHOD(DrawGrayToDIB24) (SIF_SURFACE* psifSurface24, INT xDest, INT yDest) = 0;

	STDMETHOD(DrawMaskedToDIB24) (SIF_SURFACE* psifSurface24, INT xDest, INT yDest) = 0;
	STDMETHOD(DrawTileToDIB24) (SIF_SURFACE* psifSurface24, INT xDest, INT yDest, SIF_LINE_OFFSET* pslOffsets) = 0;
	STDMETHOD(DrawColorizedToDIB24) (SIF_SURFACE* psifSurface24, INT xDest, INT yDest, COLORREF cr) = 0;

	STDMETHOD(DrawToBits32) (SIF_SURFACE* psifSurface32, INT xDest, INT yDest) = 0;
	STDMETHOD(CopyToBits32) (SIF_SURFACE* psifSurface32, INT xDest, INT yDest) = 0;

	STDMETHOD_(VOID, GetTimeStamp) (__out FILETIME* pft) = 0;
	STDMETHOD_(VOID, SetTimeStamp) (const FILETIME* pcft) = 0;
};

interface __declspec(uuid("8D401C1F-B59A-43d4-89CA-2A22D40CFBCE")) ISimbeyInterchangeFile : IUnknown
{
	STDMETHOD_(VOID, Close) (VOID) = 0;
	STDMETHOD(Save) (VOID) = 0;
	STDMETHOD(SaveAs) (PCWSTR pcwzFile, BOOL fOverwriteExisting) = 0;
	STDMETHOD(GetFileName) (__out_ecount(cchMaxFile) PWSTR pwzFile, INT cchMaxFile) = 0;
	STDMETHOD_(BOOL, IsDirty) (VOID) = 0;

	STDMETHOD_(VOID, GetCanvasPixelSize) (USHORT* pnSquareSize) = 0;
	STDMETHOD_(USHORT, GetCanvasPPI) (VOID) = 0;

	STDMETHOD(SetCanvasPixelSize) (USHORT nSquareSize) = 0;
	STDMETHOD(SetCanvasPPI) (USHORT cPixelsPerInch) = 0;

	STDMETHOD_(PBYTE, GetMergedCanvas24) (BYTE bRed, BYTE bGreen, BYTE bBlue) = 0;
	STDMETHOD(MergeToBits24) (BYTE bRed, BYTE bGreen, BYTE bBlue, __in PBYTE pBits24) = 0;

	STDMETHOD_(PBYTE, GetMergedCanvas32) (BYTE bRed, BYTE bGreen, BYTE bBlue, BYTE iAlpha) = 0;
	STDMETHOD(MergeToBits32) (BYTE bRed, BYTE bGreen, BYTE bBlue, BYTE iAlpha, __in PBYTE pBits32) = 0;

	STDMETHOD_(DWORD, GetLayerCount) (VOID) = 0;

	STDMETHOD(GetLayerID) (DWORD dwLayerIndex, __out DWORD* pidLayer) = 0;
	STDMETHOD(GetLayer) (DWORD idLayer, __deref_out ISimbeyInterchangeFileLayer** ppLayer) = 0;
	STDMETHOD(GetLayerByIndex) (DWORD dwLayerIndex, __deref_out ISimbeyInterchangeFileLayer** ppLayer) = 0;
	STDMETHOD(AddLayer) (USHORT nWidth, USHORT nHeight, __deref_out ISimbeyInterchangeFileLayer** ppLayer, __out_opt DWORD* pdwLayerIndex) = 0;
	STDMETHOD(AddLayerFromBits) (USHORT nWidth, USHORT nHeight, __in const BYTE* pcbBits, INT cBitsPerPixel, INT cBytesPerRow, __deref_out ISimbeyInterchangeFileLayer** ppLayer, __out_opt DWORD* pdwLayerIndex) = 0;
	STDMETHOD(RemoveLayer) (DWORD idLayer) = 0;
	STDMETHOD(ChangeLayerID) (DWORD idLayer, DWORD idNew) = 0;

	STDMETHOD(GetSupportedCanvasSizes) (__out_ecount(*pcSizes) USHORT* pnSizes, __inout INT* pcSizes) = 0;

	STDMETHOD(CreateWICBitmapFromLayer) (__in ISimbeyInterchangeFileLayer* pLayer, __deref_out IWICBitmap** ppwicBitmap) = 0;
	STDMETHOD(FindLayer) (PCWSTR pcwzName, __deref_out ISimbeyInterchangeFileLayer** ppLayer, __out_opt DWORD* pdwLayerIndex) = 0;

	STDMETHOD(ImportLayer) (ISimbeyInterchangeFileLayer* pImport, __deref_out ISimbeyInterchangeFileLayer** ppLayer) = 0;
};

interface __declspec(uuid("09E2F236-D840-4c2c-B365-307C2ACBE2F4")) ISimbeyInterchangeFileFont : IUnknown
{
	STDMETHOD(MeasureText) (PCWSTR pcwzText, __out LONG& rWidth, __out LONG& rHeight) = 0;
	STDMETHOD(GetGlyph) (CHAR ch, __deref_out ISimbeyInterchangeFileLayer** ppGlyph) = 0;
	STDMETHOD(DrawTextDIB) (HBITMAP hbmDIB, PCWSTR pcwzText, INT x, INT y, UINT nFormat) = 0;
	STDMETHOD(DrawGrayTextDIB) (HBITMAP hbmDIB, PCWSTR pcwzText, INT x, INT y, UINT nFormat) = 0;
	STDMETHOD(DrawTextToSurface) (SIF_SURFACE* psifSurface, PCWSTR pcwzText, INT x, INT y, UINT nFormat) = 0;
	STDMETHOD(DrawGrayTextToSurface) (SIF_SURFACE* psifSurface, PCWSTR pcwzText, INT x, INT y, UINT nFormat) = 0;
};

interface __declspec(uuid("ED064682-5F54-4adf-8CCA-CADC00B2043C")) ISpriteAnimationCompleted : IUnknown
{
	STDMETHOD_(VOID, OnSpriteAnimationCompleted) (ISimbeyInterchangeSprite* pSprite, INT nAnimation) = 0;
};

interface __declspec(uuid("F9ED0968-51C0-402c-A999-45CB164B0402")) ISimbeyInterchangeSprite : IUnknown
{
	STDMETHOD(SelectAnimation) (INT nAnimation, INT nFrame = 0, INT cTicks = -1) = 0;
	STDMETHOD_(VOID, GetCurrentAnimation) (__out INT* pnAnimation, __out INT* pnFrame, __out INT* pcTicks) = 0;
	STDMETHOD_(VOID, GetCurrentFrameSize) (__out INT* pxSize, __out INT* pySize) = 0;
	STDMETHOD_(VOID, GetCurrentHitBox) (__out RECT* prcHitBox) = 0;

	STDMETHOD_(VOID, UpdateFrameTick) (VOID) = 0;
	STDMETHOD_(VOID, SetPosition) (INT x, INT y) = 0;
	STDMETHOD_(VOID, GetPosition) (__out INT& x, __out INT& y) = 0;

	STDMETHOD_(BOOL, DrawMaskedToDIB24) (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24) = 0;
	STDMETHOD_(BOOL, DrawTileToDIB24) (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24, SIF_LINE_OFFSET* pslOffsets) = 0;
	STDMETHOD_(BOOL, DrawBlendedToDIB24) (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24) = 0;
	STDMETHOD_(BOOL, DrawColorizedToDIB24) (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24) = 0;

	// These methods only affect the DrawColorizedToDIB24() call
	STDMETHOD_(COLORREF, GetColorized) (VOID) = 0;
	STDMETHOD_(BOOL, SetColorized) (COLORREF cr) = 0;

	STDMETHOD(Clone) (__deref_out ISimbeyInterchangeSprite** ppSprite) = 0;
	STDMETHOD_(VOID, GetFrameOffset) (__out INT& x, __out INT& y) = 0;
	STDMETHOD(GetFrameImage) (__out PBYTE* ppBits32P, __out INT* pnWidth, __out INT* pnHeight) = 0;
	STDMETHOD(SetAnimationCompletedCallback) (__in_opt ISpriteAnimationCompleted* pCallback) = 0;
	STDMETHOD(GetAnimator) (__deref_out ISimbeyInterchangeAnimator** ppAnimator) = 0;
};

interface __declspec(uuid("D9578F6E-0FE6-417f-9F35-7D5827940E94")) ISimbeyInterchangeAnimator : IUnknown
{
	STDMETHOD(SetImage) (INT nImage, BOOL fPremultiply, ISimbeyInterchangeFileLayer* pLayer, BOOL fUsePositionAsOffset) = 0;
	STDMETHOD(SetImage) (INT nImage, BOOL fPremultiply, PBYTE pBits32, INT nWidth, INT nHeight, INT xOffset, INT yOffset) = 0;

	STDMETHOD(ConfigureAnimation) (INT nAnimation, __in_opt const RECT* pcrcHitBox, INT nNextAnimation) = 0;
	STDMETHOD(AddFrame) (INT nAnimation, INT cTicks, INT nImage, INT xOffset, INT yOffset) = 0;
	STDMETHOD(CompactFrames) (INT nAnimation) = 0;

	STDMETHOD(CreateSprite) (__deref_out ISimbeyInterchangeSprite** ppSprite) = 0;

	STDMETHOD_(INT, GetImageCount) (VOID) = 0;
	STDMETHOD(GetImage) (INT nImage, PBYTE* ppBits32P, INT* pnWidth, INT* pnHeight) = 0;
	STDMETHOD(GetImageOffset) (INT nImage, __out INT* pxOffset, __out INT* pyOffset) = 0;

	STDMETHOD(Duplicate) (__deref_out ISimbeyInterchangeAnimator** ppAnimator) = 0;
};

interface __declspec(uuid("1F33FDB9-8333-4869-871C-5AA8B0B9C709")) ISimbeyFontCollection : IUnknown
{
	STDMETHOD(LoadFontFile) (PCWSTR pcwzFontFile) = 0;
	STDMETHOD(LoadMemoryFont) (const VOID* pcvFont, INT cbFont) = 0;
	STDMETHOD(LoadStreamFont) (ISequentialStream* pstmFont, INT cbFont) = 0;
	STDMETHOD_(INT, GetFamilyCount) (VOID) = 0;
	STDMETHOD(GetFamilies) (__deref_out PVOID* ppvFamilies, __out INT* pcFamilies) = 0;
	STDMETHOD(GetFamilyName) (PVOID pvFamilies, INT nFamily, __out_ecount(LF_FACESIZE) PWSTR pwzFamilyName) = 0;
	STDMETHOD(CreateFont) (PCWSTR pcwzFamily, FLOAT rSize, INT nFontStyles, __deref_out PVOID* ppvFont) = 0;
	STDMETHOD_(VOID, DeleteFont) (PVOID pvFont) = 0;
	STDMETHOD_(VOID, DeleteFamilies) (PVOID pvFamilies) = 0;
};

HRESULT WINAPI sifCreateNew (__deref_out ISimbeyInterchangeFile** ppSIF);
HRESULT WINAPI sifLoad (PCWSTR pcwzSIF, __deref_out ISimbeyInterchangeFile** ppSIF);
HRESULT WINAPI sifLoadResource (HMODULE hModule, PCWSTR pcwzResName, __deref_out ISimbeyInterchangeFile** ppSIF);
HRESULT WINAPI sifAlloc (DWORD cbAlloc, __deref_out PBYTE* ppbMem);
VOID WINAPI sifDeleteMemoryPtr (PVOID pMem);
BYTE WINAPI sifBlendColorComponents (BYTE bComponentA, BYTE bComponentB, INT nAlpha);
BOOL WINAPI sifMergeLayer24 (PBYTE pImage24, INT niWidth, INT niHeight, PBYTE pLayer, INT xPos, INT yPos, INT nlWidth, INT nlHeight, INT cBitsPerPixel, PSIFPAL pPalette, INT cEntries, DWORD iTransparent);
BOOL WINAPI sifMergeLayer32 (PBYTE pImage32, INT niWidth, INT niHeight, PBYTE pLayer, INT xPos, INT yPos, INT nlWidth, INT nlHeight, INT cBitsPerPixel, PSIFPAL pPalette, INT cEntries, DWORD iTransparent);
BOOL WINAPI sifConvertBitsToBits24 (PBYTE pImage, INT nWidth, INT nHeight, INT cBitsPerPixel, PSIFPAL pPalette, INT cEntries, DWORD iTransparent, __deref_out PBYTE* ppBits24);
COLORREF WINAPI sifReadPixel (const BYTE* pcBits, INT x, INT y, INT nWidth, INT cBitsPerPixel, PSIFPAL pPalette, INT cEntries, DWORD iTransparent);
HRESULT WINAPI sifDrawBits24ToHDC (HDC hdc, INT x, INT y, INT nDestWidth, INT nDestHeight, PBYTE pBits24, INT nWidth, INT nHeight);
HBITMAP WINAPI sifCreateDIBFromBits24 (const BYTE* pImage24, INT nWidth, INT nHeight, __out_opt BITMAPINFOHEADER* pbmih, __out PBYTE* ppDIBits);
HBITMAP WINAPI sifCreateDIBFromBits32 (const BYTE* pImage32, INT nWidth, INT nHeight, __out_opt BITMAPINFOHEADER* pbmih, __out PBYTE* ppDIBits);
HRESULT WINAPI sifCreateDDBFromBits (const BYTE* pImage, INT nWidth, INT nHeight, INT cBitsPerPixel, BOOL fSetBitFields, HDC hdc, __out HBITMAP* phDDB);
HRESULT WINAPI sifLoadImageFileToBits32 (PCWSTR pcwzImage, __deref_out PBYTE* ppBits32, __out INT* pnWidth, __out INT* pnHeight);
HRESULT WINAPI sifAddIPictureAsLayer (__in IPicture* pPicture, __in ISimbeyInterchangeFile* pSIF, __out_opt DWORD* pdwLayerIndex);
HRESULT WINAPI sifAddImageFileAsLayer (PCWSTR pcwzImage, __in ISimbeyInterchangeFile* pSIF, __out_opt DWORD* pdwLayerIndex);
HRESULT WINAPI sifAddImageFileAsLayerEx (PCWSTR pcwzImage, __in ISimbeyInterchangeFile* pSIF, BOOL fAlwaysUseGdiPlus, __out_opt DWORD* pdwLayerIndex);
BOOL WINAPI sifDrawBitsToDIB24 (__out PBYTE pDIB, INT nWidth, INT nHeight, __in PBYTE pBits, INT xDest, INT yDest, INT nWidthBits, INT nHeightBits, INT cBitsPerPixel, PSIFPAL pPalette, INT cEntries, DWORD iTransparent);
BOOL WINAPI sifDrawBits32 (__out_bcount(nWidthDest * nHeightDest * 4) PBYTE pBitsDest, INT xDest, INT yDest, INT nWidthDest, INT nHeightDest, __in_bcount(nWidthSrc * nHeightSrc * 4) const BYTE* pBitsSrc, INT xSrc, INT ySrc, INT nWidthSrc, INT nHeightSrc, INT xSrcCopy, INT ySrcCopy);
BOOL WINAPI sifCopyBits32 (__out_bcount(nWidthDest * nHeightDest * 4) PBYTE pBitsDest, INT xDest, INT yDest, INT nWidthDest, INT nHeightDest, __in_bcount(nWidthSrc * nHeightSrc * 4) const BYTE* pBitsSrc, INT xSrc, INT ySrc, INT nWidthSrc, INT nHeightSrc, INT xSrcCopy, INT ySrcCopy);
VOID WINAPI sifFlipBits32 (__inout_bcount(nWidth * nHeight * 4) PBYTE pBits, INT nWidth, INT nHeight);
VOID WINAPI sifCopyBits32ToDIB32 (__in_bcount(nWidth * nHeight * 4) const BYTE* pBits, INT nWidth, INT nHeight, __out_bcount(nWidth * nHeight * 4) PBYTE pDIB);
HRESULT WINAPI sifGetThumbnailBitsFromJPEG (ISeekableStream* pStream, DWORD cbFile, __deref_out HGLOBAL* phBits, __out DWORD* pcbBits);
HRESULT WINAPI sifGetThumbnailFromJPEG (__deref_inout HMODULE* phOleAut32, ISeekableStream* pStream, DWORD cbFile, __deref_out IPicture** ppPicture);
BOOL WINAPI sifFillRect (__out_bcount(nWidth * nHeight * 4) PBYTE pBits32, INT nWidth, INT nHeight, INT xStart, INT yStart, INT xFill, INT yFill, DWORD dwFillColor);
HRESULT WINAPI sifLoadFont (HMODULE hModule, PCWSTR pcwzRes, __deref_out ISimbeyInterchangeFileFont** ppFont);
HRESULT WINAPI sifCreateFontFromSIF (ISimbeyInterchangeFile* pSIF, BOOL fCloseOnRelease, __deref_out ISimbeyInterchangeFileFont** ppFont);
VOID WINAPI sifGetGrayScaleMultipliers (__out FLOAT* prMultRed, __out FLOAT* prMultGreen, __out FLOAT* prMultBlue);
BOOL WINAPI sifSetGrayScaleMultipliers (FLOAT rMultRed, FLOAT rMultGreen, FLOAT rMultBlue);
HRESULT WINAPI sifWriteBits32ToPNGStream (__in_bcount(nWidth * nHeight * 4) const BYTE* pBits32, INT nWidth, INT nHeight, INT nPPI, __out ISequentialStream* pstmPNG, __out DWORD* pcbPNG);
HRESULT WINAPI sifWriteBits24ToPNGStream (const BYTE* pBits24, INT nWidth, INT nHeight, INT nPPI, __out ISequentialStream* pstmPNG, __out DWORD* pcbPNG);
HRESULT WINAPI sifCreateBlankDIB (HDC hdc, LONG xSize, LONG ySize, INT cBitsPerPixel, __deref_opt_out void** ppvBits, __out HBITMAP* phDIB);
HRESULT WINAPI sifAddLayerFromGDI (HDC hdc, HBITMAP hBitmap, __in ISimbeyInterchangeFile* pSIF, __out_opt DWORD* pdwLayerIndex);
HRESULT WINAPI sifCreateTextOutlineLayer (PCWSTR pcwzText, INT cchText, PCWSTR pcwzFont, INT nSize, BOOL fBold, BOOL fItalic, INT nBorderWidth, PCWSTR pcwzBorderStyle, INT nFillType, COLORREF* pcrData, ISimbeyInterchangeFile* pSIF, __out_opt DWORD* pdwLayerIndex);
HRESULT WINAPI sifCreateTextOutlineLayerUsingFont (PCWSTR pcwzText, INT cchText, ISimbeyFontCollection* pCollection, PVOID pvFamilies, INT nFamily, INT nSize, BOOL fBold, BOOL fItalic, INT nBorderWidth, PCWSTR pcwzBorderStyle, INT nFillType, COLORREF* pcrData, ISimbeyInterchangeFile* pSIF, __out_opt DWORD* pdwLayerIndex);
HRESULT WINAPI sifCreateFontCollection (__deref_out ISimbeyFontCollection** ppCollection);
HRESULT WINAPI sifLoadImageDataToBits32 (const BYTE* pcbData, const BITMAPINFOHEADER* pcbiInfo, __deref_out PBYTE* ppBits32, __out INT* pnWidth, __out INT* pnHeight);
HRESULT WINAPI sifResizeImageBits32 (__in_bcount(nWidth * nHeight * 4) const BYTE* pBits32, INT nWidth, INT nHeight, __deref_out_bcount(xResize * yResize * sizeof(DWORD)) PBYTE* ppBits32, INT xResize, INT yResize);
VOID WINAPI sifPremultiplyAlpha (__in_bcount(nWidth * nHeight * 4) const BYTE* pBits32, __out_bcount(nWidth * nHeight * 4) BYTE* pBits32P, INT nWidth, INT nHeight);
HRESULT WINAPI sifCreateAnimator (INT cImages, INT cAnimations, __deref_out ISimbeyInterchangeAnimator** ppAnimator);
HRESULT WINAPI sifLoadFromRawImage (const BYTE* pcbData, ULONG cbData, BOOL fUseEmbeddedColorManagement, __deref_out PBYTE* ppBits32, __out INT* pnWidth, __out INT* pnHeight);
HRESULT WINAPI sifLoadToDDB (const BYTE* pcbData, ULONG cbData, BOOL fUseEmbeddedColorManagement, HDC hdc, __out INT* pnWidth, __out INT* pnHeight, __out HBITMAP* phDDB);
HRESULT WINAPI sifResizeBitsX (__in_bcount(lPitch * nHeight) const BYTE* pcbBits, INT nWidth, INT nHeight, INT cBytesPerPixel, LONG lPitch, __deref_out_bcount(lResizePitch * yResize) PBYTE pbResized, INT xResize, INT yResize, LONG lResizePitch, INT nMode);
HRESULT WINAPI sifTrimBits32 (const BYTE* pcbData, INT xSize, INT ySize, __deref_out_bcount(*pxNew * *pyNew * 4) BYTE** ppbNew, __out INT* pxNew, __out INT* pyNew, __out INT* pxOffset, __out INT* pyOffset);
BOOL WINAPI sifToggleChannels (SIF_SURFACE* psifSurface);
HRESULT WINAPI sifCreateStaticSprite (ISimbeyInterchangeFileLayer* pLayer, INT xStaticOffset, INT yStaticOffset, __deref_out ISimbeyInterchangeSprite** ppSprite);
HRESULT WINAPI sifReadPNGToBits32Stream (const BYTE* pcbPNG, DWORD cbPNG, __out INT* pxSize, __out INT* pySize, __out ISequentialStream* pstmBits32, __out DWORD* pcbBits32);

BOOL WINAPI sifDrawBits32ToDIB32 (__out_bcount(nWidth * nHeight * 4) PBYTE pDIB32, INT xDest, INT yDest, INT nWidth, INT nHeight, __in_bcount(nWidthBits * nHeightBits * 4) const BYTE* pBits, INT nWidthBits, INT nHeightBits);
BOOL WINAPI sifDrawBits32ToDIB24 (__out PBYTE pDIB24, INT xDest, INT yDest, INT nWidth, INT nHeight, __in_bcount(nWidthBits * nHeightBits * 4) const BYTE* pBits, INT nWidthBits, INT nHeightBits);
BOOL WINAPI sifDrawGrayBits32ToDIB32 (__out_bcount(nWidth * nHeight * 4) PBYTE pDIB32, INT xDest, INT yDest, INT nWidth, INT nHeight, __in_bcount(nWidthBits * nHeightBits * 4) const BYTE* pBits, INT nWidthBits, INT nHeightBits);
BOOL WINAPI sifDrawGrayBits32ToDIB24 (__out PBYTE pDIB24, INT xDest, INT yDest, INT nWidth, INT nHeight, __in_bcount(nWidthBits * nHeightBits * 4) const BYTE* pBits, INT nWidthBits, INT nHeightBits);
BOOL WINAPI sifDrawMaskedBits32ToDIB24 (__out PBYTE pDIB24, INT xDest, INT yDest, INT nWidth, INT nHeight, __in_bcount(nWidthBits * nHeightBits * 4) const BYTE* pBits, INT nWidthBits, INT nHeightBits);
BOOL WINAPI sifDrawTileBits32ToDIB24 (__out PBYTE pDIB24, INT xDest, INT yDest, INT nWidth, INT nHeight, __in_bcount(nWidthBits * nHeightBits * 4) const BYTE* pBits, INT nWidthBits, INT nHeightBits, SIF_LINE_OFFSET* pslOffsets);
BOOL WINAPI sifDrawColorizedBits32ToDIB24 (__out PBYTE pDIB24, INT xDest, INT yDest, INT nWidth, INT nHeight, __in_bcount(nWidthBits * nHeightBits * 4) const BYTE* pBits, INT nWidthBits, INT nHeightBits, COLORREF cr);
BOOL WINAPI sifDrawBits32PToDIB24 (__out PBYTE pDIB24, INT xDest, INT yDest, INT nWidth, INT nHeight, __in_bcount(nWidthBits * nHeightBits * 4) const BYTE* pBits32P, INT nWidthBits, INT nHeightBits);

BOOL WINAPI sifDrawBits32ToDIB32Pitch (__out_bcount(nWidth * nHeight * 4) PBYTE pDIB32, INT xDest, INT yDest, INT nWidth, INT nHeight, LONG lPitch, __in_bcount(nWidthBits * nHeightBits * 4) const BYTE* pBits, INT nWidthBits, INT nHeightBits, LONG lPitchBits);
BOOL WINAPI sifDrawBits32ToDIB24Pitch (__out PBYTE pDIB24, INT xDest, INT yDest, INT nWidth, INT nHeight, LONG lPitch, __in_bcount(nWidthBits * nHeightBits * 4) const BYTE* pBits, INT nWidthBits, INT nHeightBits, LONG lPitchBits);
BOOL WINAPI sifDrawGrayBits32ToDIB32Pitch (__out_bcount(nWidth * nHeight * 4) PBYTE pDIB32, INT xDest, INT yDest, INT nWidth, INT nHeight, LONG lPitch, __in_bcount(nWidthBits * nHeightBits * 4) const BYTE* pBits, INT nWidthBits, INT nHeightBits, LONG lPitchBits);
BOOL WINAPI sifDrawGrayBits32ToDIB24Pitch (__out PBYTE pDIB24, INT xDest, INT yDest, INT nWidth, INT nHeight, LONG lPitch, __in_bcount(nWidthBits * nHeightBits * 4) const BYTE* pBits, INT nWidthBits, INT nHeightBits, LONG lPitchBits);
BOOL WINAPI sifDrawMaskedBits32ToDIB24Pitch (__out PBYTE pDIB24, INT xDest, INT yDest, INT nWidth, INT nHeight, LONG lPitch, __in_bcount(nWidthBits * nHeightBits * 4) const BYTE* pBits, INT nWidthBits, INT nHeightBits, LONG lPitchBits);
BOOL WINAPI sifDrawTileBits32ToDIB24Pitch (__out PBYTE pDIB24, INT xDest, INT yDest, INT nWidth, INT nHeight, LONG lPitch, __in_bcount(nWidthBits * nHeightBits * 4) const BYTE* pBits, INT nWidthBits, INT nHeightBits, LONG lPitchBits, SIF_LINE_OFFSET* pslOffsets);
BOOL WINAPI sifDrawColorizedBits32ToDIB24Pitch (__out PBYTE pDIB24, INT xDest, INT yDest, INT nWidth, INT nHeight, LONG lPitch, __in_bcount(nWidthBits * nHeightBits * 4) const BYTE* pBits, INT nWidthBits, INT nHeightBits, LONG lPitchBits, COLORREF cr);
BOOL WINAPI sifDrawBits32PToDIB24Pitch (__out PBYTE pDIB24, INT xDest, INT yDest, INT nWidth, INT nHeight, LONG lPitch, __in_bcount(nWidthBits * nHeightBits * 4) const BYTE* pBits32P, INT nWidthBits, INT nHeightBits, LONG lPitchBits);

HRESULT WINAPI sifCreateCursorFromBits (__in_bcount(nWidth * nHeight * 4) const BYTE* pBits32, INT nWidth, INT nHeight, const POINT* pcptHot, __in_opt const BYTE* pcbMask, __out HCURSOR* phCursor);
