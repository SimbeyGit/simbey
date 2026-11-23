#pragma once

#include "..\Imaging\ImageSurface.h"

namespace QF
{
	class CCanvasSurface : public CBaseUnknown
	{
	public:
		QFI::CImageSurface m_surface;

	public:
		UINT width;
		UINT height;
		UINT dpi;

		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IUnknown)
		END_UNK_MAP

		CCanvasSurface();
		HRESULT Create(UINT cx, UINT cy, UINT resolution, COLORREF background, CCanvasSurface* previous);
		HRESULT Opaque();
		HRESULT Fill(COLORREF color);
		HRESULT Export(ISequentialStream** data, ULONG* count);
		HDC DC() const;
		BYTE* Pixels() const;
	};
	HRESULT CanvasText(HDC target, HFONT font, RSTRING text, INT x, INT y, COLORREF color, SIZE* measured, BOOL draw);
	HRESULT CanvasColor(COLORREF color, BOOL resolve, COLORREF* result);
}
