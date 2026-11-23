#pragma once

#include <windows.h>
#include "Library\Core\BaseUnknown.h"
#include "Library\Core\Array.h"
#include "Library\Core\MemoryStream.h"
#include "Published\SIF.h"
#include "Published\JSON.h"

// Private, reusable image support. No runtime control or HWND dependency.
namespace QFI
{
const UINT MaxBytes = 8 * 1024 * 1024;
const UINT MaxDimension = 4096;
const UINT MaxPixels = 4 * 1024 * 1024;
enum SizeMode { Normal, Stretch, Contain, Cover, Tile };
enum Interpolation { Nearest, Bilinear, Bicubic, HighQuality };
struct Raster
{
	UINT width, height;
	BYTE* pixels; // Top-down, premultiplied BGRA8.
	Raster() : width(0), height(0), pixels(NULL) {}
	~Raster();
	HRESULT Allocate(UINT cx, UINT cy);
	VOID Clear();
	VOID Swap(Raster& other);
private:
	Raster(const Raster&);
	Raster& operator=(const Raster&);
};
HRESULT DecodePNG(const BYTE* bytes, UINT size, CMemoryStream& rgba, UINT* width, UINT* height);
HRESULT EncodePNG(const BYTE* rgba, UINT width, UINT height, CMemoryStream& bytes, BOOL optimizeColor = TRUE);
HRESULT CreateValue(PCWSTR format, const BYTE* bytes, UINT size, IJSONValue** value);
HRESULT ImportFile(PCWSTR path, IJSONValue** value, BOOL optimizeColor = TRUE); // JPEG/BMP import through GDI+.

// Internal deterministic fault injection, shared by the private native tests.
// -1 disables it; 0 fails the next imaging allocation and resets the countdown.
VOID FailAllocationAfter(LONG count);
LONG AllocatedBytes();
VOID ReleaseCodecTLS();
PVOID Allocate(SIZE_T size);
VOID Free(PVOID memory);

class CImageResource : public CBaseUnknown
{
	struct Entry { UINT width, height, depth, offset, size; BOOL png, supported; };
	CMemoryStream m_bytes;
	TArray<Entry> m_entries;
	UINT m_natural, m_decoded;
	BOOL m_ico, m_alpha;
	Raster m_source, m_sourceXor;
	Raster m_cacheXor;
	HDC m_cacheDC;
	HBITMAP m_cacheBitmap;
	HGDIOBJ m_cacheOld;
	UINT m_cacheWidth, m_cacheHeight, m_cacheDPI, m_cacheMode, m_cacheAlign, m_cacheFilter;
	INT m_cacheOffsetX, m_cacheOffsetY;
	HDC m_grayDC;
	HBITMAP m_grayBitmap;
	HGDIOBJ m_grayOld;
	HRESULT Open(PCWSTR format, const BYTE* bytes, UINT size);
	HRESULT DecodeEntry(UINT index, Raster& pixels, Raster& xorPixels, BOOL* alpha);
	UINT SelectEntry(UINT width, UINT height) const;
	HRESULT Rasterize(UINT width, UINT height, UINT dpi, UINT mode, UINT alignment, UINT filter, INT offsetX, INT offsetY);
	HRESULT PrepareGlyph(UINT width, UINT height, UINT dpi, UINT filter);
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IUnknown)
	END_UNK_MAP
	CImageResource();
	~CImageResource();
	static HRESULT Create(IJSONValue* value, CImageResource** resource);
	static HRESULT FromBytes(PCWSTR format, const BYTE* bytes, UINT size, CImageResource** resource);
	HRESULT GetValue(IJSONValue** value) const;
	HRESULT NormalizePNG(UINT width, UINT height, IJSONValue** value);
	HRESULT ColorKeyPNG(COLORREF color, CImageResource** resource);
	HRESULT RenderPixels(UINT width, UINT height, UINT mode, UINT alignment, Raster& pixels, INT offsetX = 0, INT offsetY = 0);
	HRESULT Paint(HDC dc, const RECT* bounds, UINT dpi, UINT mode, UINT alignment, UINT filter, BYTE opacity, INT offsetX = 0, INT offsetY = 0);
	HRESULT PaintGrayscale(HDC dc, const RECT* bounds, UINT dpi, UINT filter);
	HRESULT RenderGlyphPixels(UINT width, UINT height, UINT dpi, UINT filter, Raster& pixels);
	VOID InvalidateCache();
	UINT Width() const { return m_entries[m_natural].width; }
	UINT Height() const { return m_entries[m_natural].height; }
	BOOL HasAlpha() const { return m_alpha; }
	BOOL IsIcon() const { return m_ico; }
	UINT ByteSize() const { return m_bytes.DataRemaining(); }
	UINT SelectedWidth() const { return m_entries[m_decoded].width; }
};
}
