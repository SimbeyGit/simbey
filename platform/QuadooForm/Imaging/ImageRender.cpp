#include "ImageResource.h"
#include "ImageSurface.h"
#include <math.h>
#pragma comment(lib,"msimg32.lib")

namespace QFI
{
static const BYTE* Pixel(const Raster& source, INT x, INT y)
{
	x = max(0,min(static_cast<INT>(source.width) - 1,x));
	y = max(0,min(static_cast<INT>(source.height) - 1,y));
	return source.pixels + (y * source.width + x) * 4;
}
static DOUBLE Cubic(DOUBLE value)
{
	value = fabs(value);
	if(value <= 1) return (1.5 * value - 2.5) * value * value + 1;
	if(value < 2) return ((-0.5 * value + 2.5) * value - 4) * value + 2;
	return 0;
}
static VOID Sample(const Raster& source, DOUBLE x, DOUBLE y, DOUBLE stepX, DOUBLE stepY, UINT filter, BYTE* output)
{
	if(filter == Nearest)
	{
		CopyMemory(output,Pixel(source,static_cast<INT>(floor(x + 0.5)),static_cast<INT>(floor(y + 0.5))),4);
		return;
	}
	DOUBLE sum[4] = {0,0,0,0}, weight = 0;
	INT left, top, right, bottom;
	BOOL area = filter == HighQuality && (stepX > 1.000001 || stepY > 1.000001);
	DOUBLE x0 = x + 0.5 - max(1.0,stepX) / 2, x1 = x + 0.5 + max(1.0,stepX) / 2;
	DOUBLE y0 = y + 0.5 - max(1.0,stepY) / 2, y1 = y + 0.5 + max(1.0,stepY) / 2;
	if(area)
	{
		left = static_cast<INT>(floor(x0)); top = static_cast<INT>(floor(y0));
		right = static_cast<INT>(ceil(x1)) - 1; bottom = static_cast<INT>(ceil(y1)) - 1;
	}
	else
	{
		left = static_cast<INT>(floor(x)); top = static_cast<INT>(floor(y));
		right = left + 1; bottom = top + 1;
		if(filter != Bilinear) { left--; top--; right++; bottom++; }
	}
	for(INT row = top; row <= bottom; row++)
	{
		DOUBLE wy = area ? max(0.0,min(y1,row + 1.0) - max(y0,static_cast<DOUBLE>(row))) : filter == Bilinear ? max(0.0,1.0 - fabs(y - row)) : Cubic(y - row);
		for(INT column = left; column <= right; column++)
		{
			DOUBLE wx = area ? max(0.0,min(x1,column + 1.0) - max(x0,static_cast<DOUBLE>(column))) : filter == Bilinear ? max(0.0,1.0 - fabs(x - column)) : Cubic(x - column);
			DOUBLE w = wx * wy;
			const BYTE* pixel = Pixel(source,column,row);
			for(UINT channel = 0; channel < 4; channel++) sum[channel] += pixel[channel] * w;
			weight += w;
		}
	}
	if(weight <= 0) { ZeroMemory(output,4); return; }
	for(UINT channel = 0; channel < 4; channel++) output[channel] = static_cast<BYTE>(max(0.0,min(255.0,floor(sum[channel] / weight + 0.5))));
	// Cubic kernels can overshoot. Retain the premultiplied-alpha invariant.
	for(UINT channel = 0; channel < 3; channel++) output[channel] = min(output[channel],output[3]);
}
VOID CImageResource::InvalidateCache()
{
	if(m_grayDC)
	{
		SelectObject(m_grayDC,m_grayOld);
		DeleteDC(m_grayDC);
	}
	if(m_grayBitmap)
	{
		DeleteObject(m_grayBitmap);
	}
	m_grayDC = NULL;
	m_grayBitmap = NULL;
	m_grayOld = NULL;
	if(m_cacheDC)
	{
		SelectObject(m_cacheDC,m_cacheOld);
		DeleteDC(m_cacheDC);
	}
	if(m_cacheBitmap) DeleteObject(m_cacheBitmap);
	m_cacheDC = NULL; m_cacheBitmap = NULL; m_cacheOld = NULL;
	m_cacheWidth = m_cacheHeight = 0;
	m_cacheXor.Clear();
}
HRESULT CImageResource::Rasterize(UINT width, UINT height, UINT dpi, UINT mode, UINT alignment, UINT filter, INT offsetX, INT offsetY)
{
	HRESULT hr;
	Raster output, effect;
	HDC dc = NULL;
	HBITMAP bitmap = NULL;
	HGDIOBJ old = NULL;
	PVOID pixels = NULL;
	CheckIf(!width || !height || width > MaxDimension || height > MaxDimension || width > MaxPixels / height,HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED));
	CheckIf(dpi < 48 || dpi > 768 || mode > Tile || alignment > 8 || filter > HighQuality,E_INVALIDARG);
	DOUBLE naturalWidth = Width() * dpi / 96.0, naturalHeight = Height() * dpi / 96.0;
	DOUBLE drawWidth = naturalWidth, drawHeight = naturalHeight;
	if(mode == Stretch) { drawWidth = width; drawHeight = height; }
	else if(mode == Contain || mode == Cover)
	{
		DOUBLE scaleX = width / naturalWidth, scaleY = height / naturalHeight;
		DOUBLE scale = mode == Contain ? min(scaleX,scaleY) : max(scaleX,scaleY);
		drawWidth *= scale; drawHeight *= scale;
	}
	if(m_ico)
	{
		UINT entry = SelectEntry(static_cast<UINT>(min(1000000.0,ceil(drawWidth))),static_cast<UINT>(min(1000000.0,ceil(drawHeight))));
		CheckIf(entry == UINT_MAX,E_UNEXPECTED);
		if(entry != m_decoded)
		{
			Raster decoded, xorPixels;
			BOOL alpha = FALSE;
			Check(DecodeEntry(entry,decoded,xorPixels,&alpha));
			m_source.Swap(decoded); m_sourceXor.Swap(xorPixels); m_decoded = entry;
		}
	}
	DOUBLE left = (width - drawWidth) * (alignment % 3) / 2;
	DOUBLE top = (height - drawHeight) * (alignment / 3) / 2;
	DOUBLE stepX = m_source.width / drawWidth, stepY = m_source.height / drawHeight;
	DOUBLE samples = filter == Nearest ? 1 : filter == Bilinear ? 4 : filter == HighQuality && (stepX > 1.000001 || stepY > 1.000001) ? (ceil(max(1.0,stepX)) + 1) * (ceil(max(1.0,stepY)) + 1) : 16;
	DOUBLE paintedWidth = width, paintedHeight = height;
	if(mode != Tile)
	{
		// Count only sampled pixels, excluding letterbox areas. In particular,
		// a thin source in Contain must not spend its work budget on empty rows.
		paintedWidth = max(0.0,min(static_cast<DOUBLE>(width),ceil(left + drawWidth - 0.5)) - max(0.0,ceil(left - 0.5)));
		paintedHeight = max(0.0,min(static_cast<DOUBLE>(height),ceil(top + drawHeight - 0.5)) - max(0.0,ceil(top - 0.5)));
	}
	CheckIf(samples * paintedWidth * paintedHeight * (m_sourceXor.pixels ? 2 : 1) > 64.0 * 1024 * 1024,HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED));
	Check(output.Allocate(width,height));
	if(m_sourceXor.pixels)
	{
		Check(effect.Allocate(width,height));
	}
	for(UINT y = 0; y < height; y++)
	{
		for(UINT x = 0; x < width; x++)
		{
			DOUBLE sx, sy;
			if(mode == Tile)
			{
				sx = (x + 0.5 - static_cast<DOUBLE>(offsetX) * dpi / 96.0) / drawWidth;
				sy = (y + 0.5 - static_cast<DOUBLE>(offsetY) * dpi / 96.0) / drawHeight;
				sx = (sx - floor(sx)) * m_source.width - 0.5;
				sy = (sy - floor(sy)) * m_source.height - 0.5;
			}
			else
			{
				if(x + 0.5 < left || y + 0.5 < top || x + 0.5 >= left + drawWidth || y + 0.5 >= top + drawHeight) continue;
				sx = (x + 0.5 - left) * stepX - 0.5;
				sy = (y + 0.5 - top) * stepY - 0.5;
			}
			Sample(m_source,sx,sy,stepX,stepY,filter,output.pixels + (y * width + x) * 4);
			if(effect.pixels) Sample(m_sourceXor,sx,sy,stepX,stepY,filter,effect.pixels + (y * width + x) * 4);
		}
	}
	{
		BITMAPINFO info = {0};
		info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		info.bmiHeader.biWidth = width; info.bmiHeader.biHeight = -static_cast<INT>(height);
		info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
		dc = CreateCompatibleDC(NULL);
		CheckIf(!dc,HrEnsureLastError());
		bitmap = CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,NULL,0);
		CheckIf(!bitmap,HrEnsureLastError());
		old = SelectObject(dc,bitmap);
		CheckIf(!old || old == HGDI_ERROR,HrEnsureLastError());
		CopyMemory(pixels,output.pixels,width * height * 4);
	}
	InvalidateCache();
	m_cacheDC = dc; dc = NULL;
	m_cacheBitmap = bitmap; bitmap = NULL;
	m_cacheOld = old; old = NULL;
	m_cacheXor.Swap(effect);
	m_cacheWidth = width; m_cacheHeight = height; m_cacheDPI = dpi;
	m_cacheMode = mode; m_cacheAlign = alignment; m_cacheFilter = filter;
	m_cacheOffsetX = offsetX; m_cacheOffsetY = offsetY;
	hr = S_OK;
Cleanup:
	if(dc) { if(old && old != HGDI_ERROR) SelectObject(dc,old); DeleteDC(dc); }
	if(bitmap) DeleteObject(bitmap);
	return hr;
}
static HRESULT PaintXor(HDC dc, const RECT* bounds, const Raster& effect, BYTE opacity)
{
	HRESULT hr;
	HDC temporary = NULL;
	HBITMAP bitmap = NULL;
	HGDIOBJ old = NULL;
	BYTE* pixels = NULL;
	BITMAPINFO info = {0};
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = effect.width; info.bmiHeader.biHeight = -static_cast<INT>(effect.height);
	info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
	temporary = CreateCompatibleDC(dc);
	CheckIf(!temporary,HrEnsureLastError());
	bitmap = CreateDIBSection(temporary,&info,DIB_RGB_COLORS,reinterpret_cast<PVOID*>(&pixels),NULL,0);
	CheckIf(!bitmap,HrEnsureLastError());
	old = SelectObject(temporary,bitmap);
	CheckIf(!old || old == HGDI_ERROR,HrEnsureLastError());
	CheckIf(!BitBlt(temporary,0,0,effect.width,effect.height,dc,bounds->left,bounds->top,SRCCOPY),HrEnsureLastError());
	GdiFlush();
	for(UINT n = 0; n < effect.width * effect.height; n++)
	{
		const BYTE* source = effect.pixels + n * 4;
		if(!source[3]) continue;
		BYTE* dest = pixels + n * 4;
		UINT alpha = (source[3] * opacity + 127) / 255;
		for(UINT c = 0; c < 3; c++)
		{
			UINT color = min(255,source[c] * 255 / source[3]);
			dest[c] = static_cast<BYTE>((dest[c] * (255 - alpha) + (dest[c] ^ color) * alpha + 127) / 255);
		}
	}
	CheckIf(!BitBlt(dc,bounds->left,bounds->top,effect.width,effect.height,temporary,0,0,SRCCOPY),HrEnsureLastError());
	hr = S_OK;
Cleanup:
	if(temporary) { if(old && old != HGDI_ERROR) SelectObject(temporary,old); DeleteDC(temporary); }
	if(bitmap) DeleteObject(bitmap);
	return hr;
}
HRESULT CImageResource::Paint(HDC dc, const RECT* bounds, UINT dpi, UINT mode, UINT alignment, UINT filter, BYTE opacity, INT offsetX, INT offsetY)
{
	HRESULT hr = S_OK;
	INT saved = 0;
	CheckIf(!dc || !bounds || bounds->right < bounds->left || bounds->bottom < bounds->top,E_INVALIDARG);
	CheckIf(dpi < 48 || dpi > 768 || mode > Tile || alignment > 8 || filter > HighQuality,E_INVALIDARG);
	LONGLONG wideWidth = static_cast<LONGLONG>(bounds->right) - bounds->left, wideHeight = static_cast<LONGLONG>(bounds->bottom) - bounds->top;
	if(!opacity || !wideWidth || !wideHeight) goto Cleanup;
	CheckIf(wideWidth > MaxDimension || wideHeight > MaxDimension,HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED));
	UINT width = static_cast<UINT>(wideWidth), height = static_cast<UINT>(wideHeight);
	if(!m_cacheDC || m_cacheWidth != width || m_cacheHeight != height || m_cacheDPI != dpi || m_cacheMode != mode || m_cacheAlign != alignment || m_cacheFilter != filter || m_cacheOffsetX != offsetX || m_cacheOffsetY != offsetY)
	{
		Check(Rasterize(width,height,dpi,mode,alignment,filter,offsetX,offsetY));
	}
	saved = SaveDC(dc);
	CheckIf(!saved,HrEnsureLastError());
	CheckIf(IntersectClipRect(dc,bounds->left,bounds->top,bounds->right,bounds->bottom) == ERROR,HrEnsureLastError());
	if(m_cacheXor.pixels)
	{
		Check(PaintXor(dc,bounds,m_cacheXor,opacity));
	}
	{
		BLENDFUNCTION blend = { AC_SRC_OVER,0,opacity,AC_SRC_ALPHA };
		CheckIf(!AlphaBlend(dc,bounds->left,bounds->top,width,height,m_cacheDC,0,0,width,height,blend),HrEnsureLastError());
	}
Cleanup:
	if(saved) RestoreDC(dc,saved);
	return hr;
}
HRESULT CImageResource::PrepareGlyph(UINT width, UINT height, UINT dpi, UINT filter)
{
	HRESULT hr = S_OK;
	CheckIf(!width || !height || width > MaxDimension || height > MaxDimension || width > MaxPixels / height,E_INVALIDARG);
	CheckIf(dpi < 48 || dpi > 768 || filter > HighQuality,E_INVALIDARG);
	if(!m_cacheDC || m_cacheWidth != width || m_cacheHeight != height || m_cacheDPI != dpi ||
		m_cacheMode != Contain || m_cacheAlign != 4 || m_cacheFilter != filter || m_cacheOffsetX || m_cacheOffsetY)
	{
		Check(Rasterize(width,height,dpi,Contain,4,filter,0,0));
	}
Cleanup:
	return hr;
}
HRESULT CImageResource::RenderGlyphPixels(UINT width, UINT height, UINT dpi, UINT filter, Raster& pixels)
{
	HRESULT hr;
	Raster candidate;
	DIBSECTION section;
	Check(PrepareGlyph(width,height,dpi,filter));
	CheckIf(m_cacheXor.pixels,HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED));
	Check(candidate.Allocate(width,height));
	CheckIf(GetObject(m_cacheBitmap,sizeof(section),&section) != sizeof(section) || !section.dsBm.bmBits,E_FAIL);
	CopyMemory(candidate.pixels,section.dsBm.bmBits,width * height * 4);
	pixels.Swap(candidate);
Cleanup:
	return hr;
}
HRESULT CImageResource::PaintGrayscale(HDC dc, const RECT* bounds, UINT dpi, UINT filter)
{
	HRESULT hr = S_OK;
	Raster gray;
	CImageSurface surface;
	DIBSECTION section;
	HDC candidateDC = NULL;
	HBITMAP candidateBitmap = NULL;
	HGDIOBJ candidateOld = NULL;
	PVOID pixels = NULL;
	BLENDFUNCTION blend = {AC_SRC_OVER,0,255,AC_SRC_ALPHA};
	CheckIf(!dc || !bounds || bounds->right < bounds->left || bounds->bottom < bounds->top,E_INVALIDARG);
	LONGLONG wide = static_cast<LONGLONG>(bounds->right) - bounds->left;
	LONGLONG high = static_cast<LONGLONG>(bounds->bottom) - bounds->top;
	CheckIfIgnore(!wide || !high,S_OK);
	CheckIf(wide > MaxDimension || high > MaxDimension,E_INVALIDARG);
	UINT width = static_cast<UINT>(wide);
	UINT height = static_cast<UINT>(high);
	Check(PrepareGlyph(width,height,dpi,filter));
	if(m_cacheXor.pixels)
	{
		// Destination-dependent icons cannot be flattened to an RGBA image.
		// Compose against this paint's background, then touch covered pixels only.
		Check(surface.Create(width,height));
		CheckIf(!BitBlt(surface.DC(),0,0,width,height,dc,bounds->left,bounds->top,SRCCOPY),HrEnsureLastError());
		CheckIf(GetObject(m_cacheBitmap,sizeof(section),&section) != sizeof(section) || !section.dsBm.bmBits,E_FAIL);
		GdiFlush();
		// Grayscale covered colors before alpha weighting. Desaturating the
		// entire composed pixel would gray the background at translucent edges.
		CompositeGrayscaleXor(surface.Pixels(),static_cast<const BYTE*>(section.dsBm.bmBits),m_cacheXor.pixels,width * height);
		CheckIf(!BitBlt(dc,bounds->left,bounds->top,width,height,surface.DC(),0,0,SRCCOPY),HrEnsureLastError());
	}
	else
	{
		if(!m_grayDC)
		{
			BITMAPINFO info = {0};
			Check(RenderGlyphPixels(width,height,dpi,filter,gray));
			GrayscalePixels(gray.pixels,width * height);
			info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			info.bmiHeader.biWidth = width;
			info.bmiHeader.biHeight = -static_cast<INT>(height);
			info.bmiHeader.biPlanes = 1;
			info.bmiHeader.biBitCount = 32;
			candidateDC = CreateCompatibleDC(NULL);
			CheckIf(!candidateDC,HrEnsureLastError());
			candidateBitmap = CreateDIBSection(candidateDC,&info,DIB_RGB_COLORS,&pixels,NULL,0);
			CheckIf(!candidateBitmap,HrEnsureLastError());
			candidateOld = SelectObject(candidateDC,candidateBitmap);
			CheckIf(!candidateOld || candidateOld == HGDI_ERROR,E_FAIL);
			CopyMemory(pixels,gray.pixels,width * height * 4);
			m_grayDC = candidateDC;
			candidateDC = NULL;
			m_grayBitmap = candidateBitmap;
			candidateBitmap = NULL;
			m_grayOld = candidateOld;
			candidateOld = NULL;
		}
		CheckIf(!AlphaBlend(dc,bounds->left,bounds->top,width,height,m_grayDC,0,0,width,height,blend),HrEnsureLastError());
	}
Cleanup:
	if(candidateDC)
	{
		if(candidateOld && candidateOld != HGDI_ERROR)
		{
			SelectObject(candidateDC,candidateOld);
		}
		DeleteDC(candidateDC);
	}
	if(candidateBitmap)
	{
		DeleteObject(candidateBitmap);
	}
	return hr;
}
HRESULT CImageResource::RenderPixels(UINT width, UINT height, UINT mode, UINT alignment, Raster& pixels, INT offsetX, INT offsetY)
{
	HRESULT hr;
	Raster candidate;
	DIBSECTION section;
	BOOL translated = mode == Normal && (offsetX || offsetY);
	Check(Rasterize(translated ? Width() : width,translated ? Height() : height,96,mode,alignment,HighQuality,0,0));
	CheckIf(m_cacheXor.pixels,HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED));
	Check(candidate.Allocate(width,height));
	CheckIf(GetObject(m_cacheBitmap,sizeof(section),&section) != sizeof(section) || !section.dsBm.bmBits,E_FAIL);
	if(translated)
	{
		INT sx = max(0,-offsetX);
		INT sy = max(0,-offsetY);
		INT tx = max(0,offsetX);
		INT ty = max(0,offsetY);
		INT cx = max(0,min(static_cast<INT>(m_cacheWidth) - sx,static_cast<INT>(width) - tx));
		INT cy = max(0,min(static_cast<INT>(m_cacheHeight) - sy,static_cast<INT>(height) - ty));
		for(INT row = 0; row < cy; row++)
		{
			CopyMemory(candidate.pixels + ((ty + row) * width + tx) * 4,
				static_cast<BYTE*>(section.dsBm.bmBits) + ((sy + row) * m_cacheWidth + sx) * 4,cx * 4);
		}
	}
	else
	{
		CopyMemory(candidate.pixels,section.dsBm.bmBits,width * height * 4);
	}
	pixels.Swap(candidate);
Cleanup:
	return hr;
}

}
