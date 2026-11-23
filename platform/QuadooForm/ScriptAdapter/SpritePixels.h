#pragma once
#include "..\Imaging\ImageResource.h"
#include "..\Published\QuadooForm.h"

namespace AQVM
{
	enum SpritePixelFormat
	{
		SpriteRGB24,
		SpriteRGBA32,
		SpriteBGRA32
	};

	// Immutable decoded source, straight BGRA32, top-down and tightly packed.
	// These objects belong to the application adapter, never to QuadooForm.dll.
	class CSpriteImage : public CBaseUnknown
	{
		CMemoryStream m_pixels;
		LONG m_reserved;
		HRESULT Allocate(UINT width, UINT height, BYTE** pixels);
	public:
		UINT width;
		UINT height;
		BEGIN_UNK_MAP
			UNK_INTERFACE_PATH(IUnknown,CBaseUnknown)
		END_UNK_MAP
		CSpriteImage();
		~CSpriteImage();
		const BYTE* Pixels() const;
		HRESULT LoadPixels(UINT cx, UINT cy, const BYTE* bytes, DWORD count, SpritePixelFormat format, UINT stride);
		HRESULT LoadPng(const BYTE* bytes, DWORD count);
		HRESULT LoadFile(RSTRING path);
		HRESULT Composite(SIF_SURFACE* surface, INT x, INT y, BOOL* changed) const;
	};
	LONG SpriteAllocatedBytes();
}
