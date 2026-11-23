#pragma once
#include "ImageResource.h"

namespace QFI
{
	const UINT MaxImageListCell = 256;
	const UINT MaxImageListCount = 256;
	enum ImageListSizing
	{
		ImageCrop,
		ImageStretch,
		ImageCenter,
		ImageContain
	};
	PCWSTR ImageListSizingName(UINT sizing);
	HRESULT ValidateImageListOptions(UINT width, UINT height, UINT depth);

	// Private shared model. Sources remain original-size PNGs; cell pixels are
	// generated from them, never from an already reduced thumbnail.
	class CImageListData : public CBaseUnknown
	{
		struct Entry
		{
			IJSONValue* definition;
			CImageResource* image;
			CImageResource* keyed;
			UINT sizing;
			BOOL colorKey;
		};
		TArray<Entry*> m_entries;
		UINT m_bytes;
		UINT m_pixels;
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IUnknown)
		END_UNK_MAP
		UINT width;
		UINT height;
		UINT depth;
		BOOL masked;
		CImageListData();
		~CImageListData();
		static HRESULT Create(IJSONValue* images, UINT width, UINT height, UINT depth, BOOL masked, CImageListData** result);
		HRESULT Append(IJSONValue* entry);
		HRESULT Images(IJSONValue** images);
		HRESULT Configure(UINT width, UINT height, UINT depth, BOOL masked);
		HRESULT SetColorKey(BOOL enabled);
		BOOL ColorKeyEnabled() const;
		HRESULT Render(UINT index, Raster& pixels, UINT cx = 0, UINT cy = 0);
		HRESULT SourceSize(UINT index, UINT* width, UINT* height, UINT* sizing);
		HRESULT Move(UINT from, UINT to);
		HRESULT Remove(UINT index);
		VOID Clear();
		UINT Count() const;
	};
	// Each call stages its entire result, including optional row-major BMP-strip
	// splitting. Batch callers append only after every selected file succeeds.
	HRESULT ImportImageListFile(PCWSTR path, UINT width, UINT height, UINT sizing, BOOL split, BOOL colorKey, IJSONArray** entries);
	HRESULT ImageListEntry(IJSONValue* png, UINT sizing, IJSONValue** entry);
	HRESULT PaintImageListPixels(HDC dc, const RECT* bounds, const Raster& pixels, BOOL checker);
}
