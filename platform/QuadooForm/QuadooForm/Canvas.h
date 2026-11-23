#pragma once

#include "Runtime.h"
#include "CanvasDrawing.h"

namespace QF
{
	class CCanvas;
	class CCanvasDrawScope
	{
		TStackRef<CCanvas> m_owner;
		INT m_saved;
		BOOL m_entered;
	public:
		HDC dc;
		TStackRef<CCanvasSurface> surface;
		DWORD generation;
		CCanvasDrawScope(CCanvas* owner);
		HRESULT Begin();
		HRESULT Complete(HRESULT result);
	};
	class CCanvas : public TWindowInterface<CWinControl,IQuadooWinControl>, public IQuadooCanvas
	{
		typedef TWindowInterface<CWinControl,IQuadooWinControl> Super;
		friend class CCanvasDrawScope;
		TStackRef<CCanvasSurface> m_surface;
		TStackRef<CCanvasSurface> m_paintSurface;
		HDC m_paintDC;
		BOOL m_painting;
		BOOL m_drawing;
		BOOL m_resizeQueued;
		BOOL m_resizePending;
		BOOL m_pressed;
		DWORD m_generation;
		DWORD m_paintGeneration;
		SIZE m_knownSize;
		SIZE m_notifiedSize;
		HRESULT EnsureSurface();
		HRESULT Paint(HDC target, const RECT* clip);
		HRESULT CanvasEvent(PCWSTR name, const INT* values, UINT count);
		HRESULT ResizeSurface();
		VOID ScheduleResize();
		HRESULT Shape(const RECT* bounds, COLORREF outline, COLORREF fill, DWORD flags, BOOL ellipse);
	protected:
		virtual VOID NativeStyle(PCWSTR* name, DWORD* style, DWORD* extended);
		virtual HRESULT NativeCreated();
		virtual HRESULT ApplyNative(PCWSTR name);
		virtual VOID NativeDestroyed(BOOL content);
		virtual HRESULT ArrangeNative(const QuadooForm::Rect* bounds, BOOL changed);
		virtual HRESULT SetValue(UINT index, QuadooProperty::Type type, IJSONValue* value);
		virtual LRESULT Message(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled);
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooCanvas)
			UNK_CHAIN_TO(Super)
		END_UNK_MAP

		CCanvas(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent);
		static const TypeSpec* Type();
		STDMETHOD(GetValueJSON)(UINT index, IJSONValue** value);
		STDMETHOD(GetValueInt)(UINT index, INT* value);
		STDMETHOD(Clear)(COLORREF color);
		STDMETHOD(SetPixel)(INT x, INT y, COLORREF color);
		STDMETHOD(GetPixel)(INT x, INT y, COLORREF* color);
		STDMETHOD(GetPixelSize)(SIZE* size);
		STDMETHOD(DrawLine)(INT x1, INT y1, INT x2, INT y2, COLORREF color);
		STDMETHOD(DrawRectangle)(const RECT* bounds, COLORREF outline, COLORREF fill, DWORD flags);
		STDMETHOD(DrawEllipse)(const RECT* bounds, COLORREF outline, COLORREF fill, DWORD flags);
		STDMETHOD(DrawTextString)(RSTRING text, INT x, INT y, COLORREF color);
		STDMETHOD(MeasureText)(RSTRING text, SIZE* size);
		STDMETHOD(SetFont)(RSTRING name, INT points, BOOL bold, BOOL italic);
		STDMETHOD(ResetFont)(VOID);
		STDMETHOD(DrawImageList)(IQuadooImageList* images, UINT index, INT x, INT y);
		STDMETHOD(ExportPng)(ISequentialStream** data, ULONG* count);
		STDMETHOD(ResolveColor)(COLORREF quadooColor, COLORREF* color);
		STDMETHOD(Invalidate)(VOID);
		STDMETHOD(GetSurface)(__deref_out HDC* phdcSurface, __deref_out HBITMAP* phDIB, __deref_out SIF_SURFACE** ppsifSurface);
	};

	HRESULT RegisterCanvasFactory(IQuadooControlRegistry* registry);
}
