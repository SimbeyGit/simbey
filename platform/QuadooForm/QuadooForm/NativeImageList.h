#pragma once
#include "..\Imaging\ImageListData.h"

namespace QF
{
	HRESULT AppendNativeImage(HIMAGELIST images, const QFI::Raster& pixels, INT index);
	HRESULT BuildNativeImageList(QFI::CImageListData* data, UINT dpi, IUnknown* factory,
		UINT* live, IQuadooImageListSnapshot** result);
}
