#pragma once

#include "ImageResource.h"

namespace QFI
{
	// Bounded, top-down BGRA memory surface. It never creates a window.
	class CImageSurface
	{
		HDC m_dc;
		HBITMAP m_bitmap;
		HGDIOBJ m_old;
		SIF_SURFACE m_surface;

		CImageSurface(const CImageSurface&);
		CImageSurface& operator=(const CImageSurface&);
	public:
		CImageSurface();
		~CImageSurface();
		HRESULT Create(UINT width, UINT height);
		VOID Clear();
		VOID Swap(CImageSurface& other);
		HDC DC() const
		{
			return m_dc;
		}
		BYTE* Pixels() const
		{
			return m_surface.pbSurface;
		}
		HBITMAP Bitmap() const
		{
			return m_bitmap;
		}
		SIF_SURFACE* Surface()
		{
			return &m_surface;
		}
	};
	VOID GrayscalePixels(BYTE* pixels, UINT count);
	VOID CompositeGrayscaleXor(BYTE* background, const BYTE* normal, const BYTE* effect, UINT count);
}
