#include "Canvas.h"

namespace QF
{
	CCanvasDrawScope::CCanvasDrawScope(CCanvas* owner) : m_owner(owner), m_saved(0), m_entered(FALSE), dc(NULL), generation(0)
	{
	}

	HRESULT CCanvasDrawScope::Begin()
	{
		HRESULT hr;
		SIZE size;
		Check(m_owner->CheckAlive());
		CheckIf(m_owner->m_drawing,QuadooForm::EInvalidState);
		Check(m_owner->GetPixelSize(&size));
		CheckIf(size.cx <= 0 || size.cy <= 0,QuadooForm::EInvalidState);
		if(m_owner->m_painting)
		{
			CheckIf(m_owner->m_generation != m_owner->m_paintGeneration,QuadooForm::EInvalidState);
			dc = m_owner->m_paintDC;
			surface = m_owner->m_paintSurface;
		}
		else
		{
			CheckIf(!m_owner->Boolean(L"AutoRedraw"),QuadooForm::EInvalidState);
			Check(m_owner->EnsureSurface());
			surface = m_owner->m_surface;
			dc = surface->DC();
		}
		CheckIf(!dc,QuadooForm::EInvalidState);
		m_saved = SaveDC(dc);
		CheckIf(!m_saved,E_FAIL);
		CheckIf(!SetMapMode(dc,MM_TEXT),E_FAIL);
		if(surface)
		{
			SetWindowOrgEx(dc,0,0,NULL);
			SetViewportOrgEx(dc,0,0,NULL);
		}
		CheckIf(IntersectClipRect(dc,0,0,size.cx,size.cy) == ERROR,E_FAIL);
		generation = m_owner->m_generation;
		m_owner->m_drawing = TRUE;
		m_entered = TRUE;
		hr = S_OK;
	Cleanup:
		return hr;
	}

	HRESULT CCanvasDrawScope::Complete(HRESULT result)
	{
		if(m_saved)
		{
			RestoreDC(dc,m_saved);
			m_saved = 0;
		}
		if(m_entered)
		{
			if(surface)
			{
				HRESULT opaque = surface->Opaque();
				if(FAILED(opaque) && SUCCEEDED(result))
				{
					result = opaque;
				}
			}
			m_owner->m_drawing = FALSE;
			if(m_owner->m_state < QuadooForm::Destroying && !m_owner->m_painting)
			{
				::InvalidateRect(m_owner->m_hwnd,NULL,FALSE);
			}
			m_entered = FALSE;
			m_owner->ScheduleResize();
		}
		return result;
	}

	STDMETHODIMP CCanvas::Clear(COLORREF color)
	{
		HRESULT hr;
		CCanvasDrawScope scope(this);
		COLORREF rgb;
		SIZE size;
		RECT rect = {0};
		Check(CheckAlive());
		Check(CanvasColor(color,FALSE,&rgb));
		Check(scope.Begin());
		Check(GetPixelSize(&size));
		rect.right = size.cx;
		rect.bottom = size.cy;
		SetDCBrushColor(scope.dc,rgb);
		CheckIf(!FillRect(scope.dc,&rect,static_cast<HBRUSH>(GetStockObject(DC_BRUSH))),E_FAIL);
	Cleanup:
		return scope.Complete(hr);
	}

	STDMETHODIMP CCanvas::SetPixel(INT x, INT y, COLORREF color)
	{
		HRESULT hr;
		CCanvasDrawScope scope(this);
		COLORREF rgb;
		SIZE size;
		Check(CheckAlive());
		Check(CanvasColor(color,FALSE,&rgb));
		Check(GetPixelSize(&size));
		CheckIf(x < 0 || y < 0 || x >= size.cx || y >= size.cy,HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
		Check(scope.Begin());
		CheckIf(!::SetPixelV(scope.dc,x,y,rgb),E_FAIL);
	Cleanup:
		return scope.Complete(hr);
	}

	STDMETHODIMP CCanvas::GetPixel(INT x, INT y, COLORREF* color)
	{
		HRESULT hr;
		SIZE size;
		CheckIf(!color,E_POINTER);
		*color = 0;
		Check(CheckAlive());
		CheckIf(!Boolean(L"AutoRedraw") || m_drawing,QuadooForm::EInvalidState);
		Check(GetPixelSize(&size));
		CheckIf(x < 0 || y < 0 || x >= size.cx || y >= size.cy,HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
		Check(EnsureSurface());
		Check(m_surface->Opaque());
		const BYTE* pixel = m_surface->Pixels() + (y * m_surface->width + x) * 4;
		*color = RGB(pixel[2],pixel[1],pixel[0]);
	Cleanup:
		return hr;
	}

	static BOOL CanvasCoordinate(INT value)
	{
		// Stay within GDI's safe device-coordinate range and avoid native
		// rasterizer overflow for otherwise unbounded, completely clipped input.
		return value >= -1000000 && value <= 1000000;
	}

	STDMETHODIMP CCanvas::DrawLine(INT x1, INT y1, INT x2, INT y2, COLORREF color)
	{
		HRESULT hr;
		CCanvasDrawScope scope(this);
		COLORREF rgb;
		Check(CheckAlive());
		Check(CanvasColor(color,FALSE,&rgb));
		CheckIf(!CanvasCoordinate(x1) || !CanvasCoordinate(y1) || !CanvasCoordinate(x2) || !CanvasCoordinate(y2),E_INVALIDARG);
		Check(scope.Begin());
		SelectObject(scope.dc,GetStockObject(DC_PEN));
		SetDCPenColor(scope.dc,rgb);
		CheckIf(!MoveToEx(scope.dc,x1,y1,NULL) || !LineTo(scope.dc,x2,y2),E_FAIL);
	Cleanup:
		return scope.Complete(hr);
	}

	HRESULT CCanvas::Shape(const RECT* bounds, COLORREF outline, COLORREF fill, DWORD flags, BOOL ellipse)
	{
		HRESULT hr;
		CCanvasDrawScope scope(this);
		COLORREF pen;
		COLORREF brush;
		Check(CheckAlive());
		CheckIf(!bounds,E_POINTER);
		CheckIf(!flags || (flags & ~(QuadooForm::CanvasOutline | QuadooForm::CanvasFill)),E_INVALIDARG);
		Check(CanvasColor(outline,FALSE,&pen));
		Check(CanvasColor(fill,FALSE,&brush));
		CheckIf(!CanvasCoordinate(bounds->left) || !CanvasCoordinate(bounds->right) ||
			!CanvasCoordinate(bounds->top) || !CanvasCoordinate(bounds->bottom) ||
			bounds->right < bounds->left || bounds->bottom < bounds->top,E_INVALIDARG);
		CheckIfIgnore(IsRectEmpty(bounds),S_FALSE);
		Check(scope.Begin());
		SelectObject(scope.dc,GetStockObject(flags & QuadooForm::CanvasOutline ? DC_PEN : NULL_PEN));
		SelectObject(scope.dc,GetStockObject(flags & QuadooForm::CanvasFill ? DC_BRUSH : NULL_BRUSH));
		SetDCPenColor(scope.dc,pen);
		SetDCBrushColor(scope.dc,brush);
		CheckIf(!(ellipse ? Ellipse(scope.dc,bounds->left,bounds->top,bounds->right,bounds->bottom) :
			Rectangle(scope.dc,bounds->left,bounds->top,bounds->right,bounds->bottom)),E_FAIL);
	Cleanup:
		return scope.Complete(hr);
	}

	STDMETHODIMP CCanvas::DrawRectangle(const RECT* bounds, COLORREF outline, COLORREF fill, DWORD flags)
	{
		return Shape(bounds,outline,fill,flags,FALSE);
	}

	STDMETHODIMP CCanvas::DrawEllipse(const RECT* bounds, COLORREF outline, COLORREF fill, DWORD flags)
	{
		return Shape(bounds,outline,fill,flags,TRUE);
	}

	STDMETHODIMP CCanvas::DrawTextString(RSTRING text, INT x, INT y, COLORREF color)
	{
		HRESULT hr;
		CCanvasDrawScope scope(this);
		COLORREF rgb;
		SIZE size;
		Check(CheckAlive());
		Check(CanvasColor(color,FALSE,&rgb));
		CheckIf(!CanvasCoordinate(x) || !CanvasCoordinate(y),E_INVALIDARG);
		Check(scope.Begin());
		Check(CanvasText(scope.dc,m_font,text,x,y,rgb,&size,TRUE));
	Cleanup:
		return scope.Complete(hr);
	}

	STDMETHODIMP CCanvas::MeasureText(RSTRING text, SIZE* size)
	{
		HRESULT hr;
		CheckIf(!size,E_POINTER);
		size->cx = size->cy = 0;
		Check(CheckAlive());
		Check(CanvasText(NULL,m_font,text,0,0,0,size,FALSE));
	Cleanup:
		return hr;
	}

	STDMETHODIMP CCanvas::SetFont(RSTRING name, INT points, BOOL bold, BOOL italic)
	{
		HRESULT hr;
		TStackRef<IJSONObject> object;
		TStackRef<IJSONValue> value;
		Check(CheckAlive());
		Check(JSONCreateObject(&object));
		Check(PutString(object,L"Name",name));
		Check(PutInt(object,L"Size",points));
		Check(JSONCreateBool(bold != FALSE,&value));
		Check(object->AddValueW(L"Bold",value));
		value.Release();
		Check(JSONCreateBool(italic != FALSE,&value));
		Check(object->AddValueW(L"Italic",value));
		value.Release();
		Check(JSONWrapObject(object,&value));
		Check(SetNamedValue(L"Font",QuadooProperty::Font,value));
	Cleanup:
		return hr;
	}

	STDMETHODIMP CCanvas::ResetFont()
	{
		HRESULT hr;
		TStackRef<IQuadooPropertyDescriptor> descriptor;
		TStackRef<IQuadooPropertyDescriptors> descriptors;
		TStackRef<IJSONValue> value;
		Check(CheckAlive());
		Check(m_factory->GetProperties(&descriptors));
		Check(descriptors->Find(RSTRING_CAST(L"Font"),&descriptor));
		Check(descriptor->GetDefaultValue(&value));
		Check(SetNamedValue(L"Font",QuadooProperty::Font,value));
	Cleanup:
		return hr;
	}

	STDMETHODIMP CCanvas::DrawImageList(IQuadooImageList* images, UINT index, INT x, INT y)
	{
		HRESULT hr;
		CCanvasDrawScope scope(this);
		TStackRef<IQuadooImageListSnapshot> snapshot;
		HIMAGELIST handle = NULL;
		Check(CheckAlive());
		CheckIf(!images,E_INVALIDARG);
		CheckIf(!CanvasCoordinate(x) || !CanvasCoordinate(y),E_INVALIDARG);
		Check(scope.Begin());
		Check(images->Acquire(m_dpi,&snapshot));
		CheckIf(!snapshot,E_UNEXPECTED);
		Check(snapshot->GetHandle(&handle));
		Check(CheckAlive());
		CheckIf(scope.generation != m_generation,QuadooForm::EInvalidState);
		CheckIf(!handle,E_UNEXPECTED);
		INT count = ImageList_GetImageCount(handle);
		CheckIf(count < 0 || index >= static_cast<UINT>(count),HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
		CheckIf(!ImageList_DrawEx(handle,index,scope.dc,x,y,0,0,CLR_NONE,CLR_NONE,ILD_TRANSPARENT),E_FAIL);
	Cleanup:
		return scope.Complete(hr);
	}

	STDMETHODIMP CCanvas::ExportPng(ISequentialStream** data, ULONG* count)
	{
		HRESULT hr;
		TStackRef<CCanvasSurface> surface;
		CheckIf(!data || !count,E_POINTER);
		*data = NULL;
		*count = 0;
		Check(CheckAlive());
		CheckIf(!Boolean(L"AutoRedraw") || m_drawing,QuadooForm::EInvalidState);
		Check(EnsureSurface());
		surface = m_surface;
		Check(surface->Export(data,count));
	Cleanup:
		return hr;
	}

	STDMETHODIMP CCanvas::ResolveColor(COLORREF quadooColor, COLORREF* color)
	{
		HRESULT hr;
		CheckIf(!color,E_POINTER);
		*color = 0;
		Check(CheckAlive());
		Check(CanvasColor(quadooColor,TRUE,color));
	Cleanup:
		return hr;
	}

	STDMETHODIMP CCanvas::Invalidate()
	{
		HRESULT hr;

		Check(CheckAlive());
		CheckIfIgnore(m_painting,S_FALSE);
		CheckIf(!::InvalidateRect(m_hwnd,NULL,FALSE),HrEnsureLastError());

	Cleanup:
		return hr;
	}

	STDMETHODIMP CCanvas::GetSurface (__deref_out HDC* phdcSurface, __deref_out HBITMAP* phDIB, __deref_out SIF_SURFACE** ppsifSurface)
	{
		HRESULT hr;
		CCanvasSurface* pSurface;
		SIZE size;

		if(phdcSurface)
			*phdcSurface = NULL;
		if(phDIB)
			*phDIB = NULL;
		if(ppsifSurface)
			*ppsifSurface = NULL;
		CheckIf(!phdcSurface || !phDIB || !ppsifSurface,E_POINTER);
		Check(CheckAlive());
		CheckIf(m_drawing,QuadooForm::EInvalidState);
		Check(GetPixelSize(&size));
		CheckIf(!size.cx || !size.cy,QuadooForm::EInvalidState);
		if(m_painting)
		{
			CheckIf(m_generation != m_paintGeneration || !m_paintDC,QuadooForm::EInvalidState);
			if(!m_paintSurface)
			{
				TStackRef<CCanvasSurface> srSurface;

				// Direct transient paints acquire a DIB only when a CPU writer
				// needs one. Preserve earlier GDI operations in this paint.
				srSurface.Attach(__new CCanvasSurface);
				CheckAlloc(srSurface);
				Check(srSurface->Create(size.cx,size.cy,m_dpi,ResolveQuadooColor(Integer(L"BackColor")),NULL));
				CheckIf(!BitBlt(srSurface->DC(),0,0,size.cx,size.cy,m_paintDC,0,0,SRCCOPY),E_FAIL);
				Check(srSurface->Opaque());
				m_paintDC = srSurface->DC();
				m_paintSurface = srSurface.Detach();
			}
			pSurface = m_paintSurface;
		}
		else
		{
			CheckIf(!Boolean(L"AutoRedraw"),QuadooForm::EInvalidState);
			Check(EnsureSurface());
			pSurface = m_surface;
		}
		CheckIf(!pSurface || !pSurface->DC() || !pSurface->m_surface.Bitmap() || !pSurface->Pixels(),E_UNEXPECTED);
		CheckIf(!GdiFlush(),E_FAIL);
		*phdcSurface = pSurface->DC();
		*phDIB = pSurface->m_surface.Bitmap();
		*ppsifSurface = pSurface->m_surface.Surface();

	Cleanup:
		return hr;
	}
}
