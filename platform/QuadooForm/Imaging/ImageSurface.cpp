#include "ImageSurface.h"

namespace QFI
{
	CImageSurface::CImageSurface() : m_dc(NULL), m_bitmap(NULL), m_old(NULL)
	{
		ZeroMemory(&m_surface, sizeof(m_surface));
	}
	CImageSurface::~CImageSurface()
	{
		Clear();
	}
	VOID CImageSurface::Clear()
	{
		if(m_dc)
		{
			if(m_old)
			{
				SelectObject(m_dc,m_old);
			}
			DeleteDC(m_dc);
		}
		if(m_bitmap)
		{
			DeleteObject(m_bitmap);
		}
		m_dc = NULL;
		m_bitmap = NULL;
		m_old = NULL;
		ZeroMemory(&m_surface,sizeof(m_surface));
	}

	VOID CImageSurface::Swap(CImageSurface& other)
	{
		SwapData(m_dc, other.m_dc);
		SwapData(m_bitmap, other.m_bitmap);
		SwapData(m_old, other.m_old);
		SwapData(m_surface.pbSurface, other.m_surface.pbSurface);
		SwapData(m_surface.xSize, other.m_surface.xSize);
		SwapData(m_surface.ySize, other.m_surface.ySize);
		SwapData(m_surface.cBitsPerPixel, other.m_surface.cBitsPerPixel);
		SwapData(m_surface.lPitch, other.m_surface.lPitch);
	}
	HRESULT CImageSurface::Create(UINT width, UINT height)
	{
		HRESULT hr;
		CImageSurface candidate;
		BITMAPINFO info = {0};
		CheckIf(!width || !height || width > MaxDimension || height > MaxDimension || width > MaxPixels / height,HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED));
		info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		info.bmiHeader.biWidth = width;
		info.bmiHeader.biHeight = -static_cast<INT>(height);
		info.bmiHeader.biPlanes = 1;
		info.bmiHeader.biBitCount = 32;
		candidate.m_dc = CreateCompatibleDC(NULL);
		CheckIf(!candidate.m_dc,HrEnsureLastError());
		candidate.m_bitmap = CreateDIBSection(candidate.m_dc,&info,DIB_RGB_COLORS,reinterpret_cast<PVOID*>(&candidate.m_surface.pbSurface),NULL,0);
		CheckIf(!candidate.m_bitmap,HrEnsureLastError());
		candidate.m_old = SelectObject(candidate.m_dc,candidate.m_bitmap);
		if(!candidate.m_old || candidate.m_old == HGDI_ERROR)
		{
			candidate.m_old = NULL;
			hr = E_FAIL;
			goto Cleanup;
		}
		candidate.m_surface.xSize = width;
		candidate.m_surface.ySize = height;
		candidate.m_surface.cBitsPerPixel = 32;
		candidate.m_surface.lPitch = width * 4;
		ZeroMemory(candidate.m_surface.pbSurface,candidate.m_surface.lPitch * height);
		Swap(candidate);
		hr = S_OK;
	Cleanup:
		return hr;
	}
	VOID GrayscalePixels(BYTE* pixels, UINT count)
	{
		for(UINT n = 0; pixels && n < count; n++)
		{
			BYTE* pixel = pixels + n * 4;
			BYTE gray = static_cast<BYTE>((77 * pixel[2] + 150 * pixel[1] + 29 * pixel[0] + 128) >> 8);
			pixel[0] = gray;
			pixel[1] = gray;
			pixel[2] = gray;
		}
	}
	VOID CompositeGrayscaleXor(BYTE* background, const BYTE* normal, const BYTE* effect, UINT count)
	{
		for(UINT n = 0; background && normal && effect && n < count; n++)
		{
			BYTE* dest = background + n * 4;
			const BYTE* source = normal + n * 4;
			const BYTE* xorPixel = effect + n * 4;
			UINT xorAlpha = xorPixel[3];
			UINT inverted[3] = {0};
			if(xorAlpha)
			{
				for(UINT c = 0; c < 3; c++)
				{
					UINT mask = min(255,xorPixel[c] * 255 / xorAlpha);
					inverted[c] = dest[c] ^ mask;
				}
			}
			UINT grayXor = (77 * inverted[2] + 150 * inverted[1] + 29 * inverted[0] + 128) >> 8;
			UINT grayNormal = (77 * source[2] + 150 * source[1] + 29 * source[0] + 128) >> 8;
			for(UINT c = 0; c < 3; c++)
			{
				UINT under = (dest[c] * (255 - xorAlpha) + grayXor * xorAlpha + 127) / 255;
				dest[c] = static_cast<BYTE>(min(255,grayNormal + (under * (255 - source[3]) + 127) / 255));
			}
		}
	}
}
