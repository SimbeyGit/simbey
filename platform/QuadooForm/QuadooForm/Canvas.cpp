#include "Canvas.h"

namespace QF
{
	static const UINT ResizeMessage = WM_APP + 0x352;
	static const DWORD Stored = QuadooForm::PropertySerializable;
	static const EnumSpec scales[] = {{L"Pixels",QuadooForm::CanvasPixels}};
	static const PropertySpec properties[] =
	{
		{L"Width",QuadooProperty::Int,Stored | QuadooForm::PropertyAffectsLayout,200,3},
		{L"Height",QuadooProperty::Int,Stored | QuadooForm::PropertyAffectsLayout,150,4},
		{L"BackColor",QuadooProperty::Color,Stored,static_cast<INT>(0x80000000 | COLOR_WINDOW),21},
		{L"AutoRedraw",QuadooProperty::Boolean,Stored,1,24},
		{L"DoubleBuffered",QuadooProperty::Boolean,Stored,1,25},
		{L"ScaleMode",QuadooProperty::Enum,Stored,0,26,scales,ARRAYSIZE(scales)},
		{L"ClearOnResize",QuadooProperty::Boolean,Stored,0,27},
		{L"PixelWidth",QuadooProperty::Int,QuadooForm::PropertyRuntimeOnly | QuadooForm::PropertyReadOnly,0,28},
		{L"PixelHeight",QuadooProperty::Int,QuadooForm::PropertyRuntimeOnly | QuadooForm::PropertyReadOnly,0,29}
	};
	static const ArgumentSpec click[] = {{L"Handled",QuadooForm::ValueBool,TRUE}};
	static const ArgumentSpec paint[] =
	{
		{L"ClipLeft",QuadooForm::ValueInt,FALSE},
		{L"ClipTop",QuadooForm::ValueInt,FALSE},
		{L"ClipWidth",QuadooForm::ValueInt,FALSE},
		{L"ClipHeight",QuadooForm::ValueInt,FALSE}
	};
	static const ArgumentSpec resize[] =
	{
		{L"PixelWidth",QuadooForm::ValueInt,FALSE},
		{L"PixelHeight",QuadooForm::ValueInt,FALSE}
	};
	static const ArgumentSpec mouse[] =
	{
		{L"X",QuadooForm::ValueInt,FALSE},
		{L"Y",QuadooForm::ValueInt,FALSE},
		{L"Button",QuadooForm::ValueInt,FALSE},
		{L"Buttons",QuadooForm::ValueInt,FALSE},
		{L"Keys",QuadooForm::ValueInt,FALSE}
	};
	static const EventSpec events[] =
	{
		{L"OnClick",0,ARRAYSIZE(click),click,2},
		{L"OnPaint",0,ARRAYSIZE(paint),paint,20},
		{L"OnResize",0,ARRAYSIZE(resize),resize,21},
		{L"OnMouseDown",0,ARRAYSIZE(mouse),mouse,22},
		{L"OnMouseUp",0,ARRAYSIZE(mouse),mouse,23},
		{L"OnMouseMove",0,ARRAYSIZE(mouse),mouse,24}
	};
	const TypeSpec* CCanvas::Type()
	{
		static const TypeSpec type = {L"Canvas",NativeWindowType(),properties,ARRAYSIZE(properties),events,ARRAYSIZE(events),IDI_PALETTE_CANVAS};
		return &type;
	}
	HRESULT RegisterCanvasFactory(IQuadooControlRegistry* registry)
	{
		HRESULT hr;
		TStackRef<CFactory> factory;
		factory.Attach(__new TFactory<CCanvas>);
		CheckAlloc(factory);
		Check(factory->Initialize());
		Check(registry->RegisterFactory(factory));
	Cleanup:
		return hr;
	}
	CCanvas::CCanvas(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) :
		Super(factory,context,parent), m_paintDC(NULL), m_painting(FALSE), m_drawing(FALSE),
		m_resizeQueued(FALSE), m_resizePending(FALSE), m_pressed(FALSE), m_generation(1), m_paintGeneration(0)
	{
		ZeroMemory(&m_knownSize,sizeof(m_knownSize));
		ZeroMemory(&m_notifiedSize,sizeof(m_notifiedSize));
	}
	VOID CCanvas::NativeStyle(PCWSTR* name, DWORD* style, DWORD* extended)
	{
		Super::NativeStyle(name,style,extended);
		*style |= WS_TABSTOP;
	}
	HRESULT CCanvas::NativeCreated()
	{
		HRESULT hr;
		Check(Super::NativeCreated());
		Check(GetPixelSize(&m_knownSize));
		m_notifiedSize = m_knownSize;
	Cleanup:
		return hr;
	}
	VOID CCanvas::NativeDestroyed(BOOL content)
	{
		m_generation++;
		m_paintDC = NULL;
		m_surface.Release();
		m_paintSurface.Release();
		m_resizeQueued = FALSE;
		m_resizePending = FALSE;
		Super::NativeDestroyed(content);
	}
	STDMETHODIMP CCanvas::GetPixelSize(SIZE* size)
	{
		HRESULT hr;
		RECT rect;
		CheckIf(!size,E_POINTER);
		size->cx = size->cy = 0;
		Check(CheckAlive());
		CheckIf(!m_hwnd,QuadooForm::EInvalidState);
		CheckIf(!GetClientRect(m_hwnd,&rect),HrEnsureLastError());
		size->cx = rect.right;
		size->cy = rect.bottom;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CCanvas::GetValueJSON(UINT index, IJSONValue** value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		Check(CheckAlive());
		if(index == FindProperty(L"PixelWidth") || index == FindProperty(L"PixelHeight"))
		{
			SIZE size;
			Check(GetPixelSize(&size));
			Check(JSONCreateInteger(index == FindProperty(L"PixelWidth") ? size.cx : size.cy,value));
		}
		else
		{
			Check(Super::GetValueJSON(index,value));
		}
	Cleanup:
		return hr;
	}
	STDMETHODIMP CCanvas::GetValueInt(UINT index, INT* value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = 0;
		Check(CheckAlive());
		if(index == FindProperty(L"PixelWidth") || index == FindProperty(L"PixelHeight"))
		{
			SIZE size;
			Check(GetPixelSize(&size));
			*value = index == FindProperty(L"PixelWidth") ? size.cx : size.cy;
		}
		else
		{
			Check(Super::GetValueInt(index,value));
		}
	Cleanup:
		return hr;
	}
	HRESULT CCanvas::EnsureSurface()
	{
		HRESULT hr;
		SIZE size;
		TStackRef<CCanvasSurface> candidate;
		Check(GetPixelSize(&size));
		CheckIf(size.cx <= 0 || size.cy <= 0 || IsIconic(m_hwnd),QuadooForm::EInvalidState);
		if(!m_surface || m_surface->width != static_cast<UINT>(size.cx) || m_surface->height != static_cast<UINT>(size.cy) || m_surface->dpi != m_dpi)
		{
			CheckIf(m_painting || m_drawing,QuadooForm::EInvalidState);
			candidate.Attach(__new CCanvasSurface);
			CheckAlloc(candidate);
			Check(candidate->Create(size.cx,size.cy,m_dpi,ResolveQuadooColor(Integer(L"BackColor")),Boolean(L"ClearOnResize") ? NULL : m_surface));
			m_surface = candidate;
			m_generation++;
		}
		hr = S_OK;
	Cleanup:
		return hr;
	}
	HRESULT CCanvas::ArrangeNative(const QuadooForm::Rect* bounds, BOOL changed)
	{
		HRESULT hr;
		Check(Super::ArrangeNative(bounds,changed));
		if(m_surface && Boolean(L"AutoRedraw") && !m_painting && !m_drawing)
		{
			SIZE size;
			Check(GetPixelSize(&size));
			if(size.cx && size.cy)
			{
				Check(EnsureSurface());
			}
		}
	Cleanup:
		return hr;
	}
	HRESULT CCanvas::ApplyNative(PCWSTR name)
	{
		HRESULT hr = S_OK;
		if(!lstrcmpW(name,L"Font"))
		{
			// Font authority stays in the base class. Its general presentation
			// path also invalidates the parent, which would restart OnPaint.
			Check(RecreateFont());
		}
		else if(lstrcmpW(name,L"BackColor") && lstrcmpW(name,L"ForeColor"))
		{
			Check(Super::ApplyNative(name));
		}
	Cleanup:
		return hr;
	}
	HRESULT CCanvas::SetValue(UINT index, QuadooProperty::Type type, IJSONValue* value)
	{
		HRESULT hr;
		TStackRef<CCanvas> keepAlive(this);
		Check(CheckAlive());
		CheckIf(index >= static_cast<UINT>(m_values.Length()),DISP_E_MEMBERNOTFOUND);
		PCWSTR name = m_values[index].spec->name;
		CheckIf(m_drawing,QuadooForm::EInvalidState);
		CheckIf(m_painting && (!lstrcmpW(name,L"AutoRedraw") || !lstrcmpW(name,L"DoubleBuffered") || !lstrcmpW(name,L"ClearOnResize") || !lstrcmpW(name,L"ScaleMode")),QuadooForm::EInvalidState);
		// Drop a persistent bitmap only after the base setter has accepted the
		// change. An observer failure can follow an already committed property.
		hr = Super::SetValue(index,type,value);
		if(m_state < QuadooForm::Destroying && !Boolean(L"AutoRedraw"))
		{
			m_surface.Release();
		}
		if(hr == S_OK && !m_painting && m_state < QuadooForm::Destroying)
		{
			::InvalidateRect(m_hwnd,NULL,FALSE);
		}
	Cleanup:
		return hr;
	}
	HRESULT CCanvas::CanvasEvent(PCWSTR name, const INT* values, UINT count)
	{
		HRESULT hr = S_OK;
		TStackRef<CCanvas> keepAlive(this);
		QuadooForm::ContextMode mode;
		CheckIfIgnore(m_state != QuadooForm::Ready,S_FALSE);
		Check(m_context->GetMode(&mode));
		CheckIfIgnore(mode != QuadooForm::Runtime,S_FALSE);
		for(sysint n = 0; n < m_bindings.Length(); n++)
		{
			if(!lstrcmpW(m_bindings[n].spec->name,name) && m_bindings[n].delegate)
			{
				TStackRef<IQuadooDelegate> delegate(m_bindings[n].delegate);
				TStackRef<CEventArgs> args;
				CheckIf(count != m_bindings[n].spec->count,E_UNEXPECTED);
				args.Attach(__new CEventArgs(m_bindings[n].spec,Control()));
				CheckAlloc(args);
				Check(args->Initialize());
				for(UINT a = 0; a < count; a++)
				{
					args->SetNativeValue(a,values[a]);
				}
				Check(delegate->Invoke(args));
				break;
			}
		}
	Cleanup:
		return hr;
	}
	HRESULT CCanvas::ResizeSurface()
	{
		HRESULT hr;
		SIZE size;
		Check(GetPixelSize(&size));
		CheckIf(m_painting || m_drawing,QuadooForm::EInvalidState);
		if(m_surface && Boolean(L"AutoRedraw") && size.cx && size.cy)
		{
			Check(EnsureSurface());
		}
		if(size.cx != m_notifiedSize.cx || size.cy != m_notifiedSize.cy)
		{
			m_notifiedSize = size;
			INT values[] = {size.cx,size.cy};
			Check(CanvasEvent(L"OnResize",values,ARRAYSIZE(values)));
		}
		hr = S_OK;
	Cleanup:
		return hr;
	}
	HRESULT CCanvas::Paint(HDC target, const RECT* clip)
	{
		HRESULT hr;
		TStackRef<CCanvas> keepAlive(this);
		TStackRef<CCanvasSurface> surface;
		SIZE size;
		BOOL entered = FALSE;
		RECT client = {0};
		HWND original = m_hwnd;
		INT saved = SaveDC(target);
		CheckIf(!saved,E_FAIL);
		CheckIfIgnore(m_painting,S_FALSE);
		Check(GetPixelSize(&size));
		CheckIfIgnore(!size.cx || !size.cy,S_FALSE);
		client.right = size.cx;
		client.bottom = size.cy;
		if(Boolean(L"AutoRedraw"))
		{
			Check(EnsureSurface());
			surface = m_surface;
		}
		else if(Boolean(L"DoubleBuffered"))
		{
			surface.Attach(__new CCanvasSurface);
			CheckAlloc(surface);
			Check(surface->Create(size.cx,size.cy,m_dpi,ResolveQuadooColor(Integer(L"BackColor")),NULL));
		}
		if(!surface)
		{
			SetDCBrushColor(target,ResolveQuadooColor(Integer(L"BackColor")));
			FillRect(target,&client,static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
		}
		m_painting = TRUE;
		entered = TRUE;
		m_paintSurface = surface;
		m_paintDC = surface ? surface->DC() : target;
		m_paintGeneration = m_generation;
		INT values[] = {clip->left,clip->top,clip->right - clip->left,clip->bottom - clip->top};
		hr = CanvasEvent(L"OnPaint",values,ARRAYSIZE(values));
		// GetSurface may have promoted a direct transient paint to a DIB.
		if(!surface)
		{
			surface = m_paintSurface;
		}
		// The callback may destroy this window, replace a target, or pump layout.
		if(m_state < QuadooForm::Destroying && original == m_hwnd && m_generation == m_paintGeneration && surface)
		{
			HRESULT present = surface->Opaque();
			if(SUCCEEDED(present) && !BitBlt(target,0,0,size.cx,size.cy,surface->DC(),0,0,SRCCOPY))
			{
				present = E_FAIL;
			}
			if(FAILED(present) && SUCCEEDED(hr))
			{
				hr = present;
			}
		}
		if(m_state < QuadooForm::Destroying && original == m_hwnd)
		{
			QuadooForm::ContextMode mode;
			if(SUCCEEDED(m_context->GetMode(&mode)) && mode == QuadooForm::Design)
			{
				SelectObject(target,m_font);
				SetTextColor(target,GetSysColor(COLOR_GRAYTEXT));
				SetBkMode(target,TRANSPARENT);
				DrawFocusRect(target,&client);
				::DrawTextW(target,L"Canvas",StaticLength(L"Canvas"),&client,DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
			}
		}
	Cleanup:
		if(entered)
		{
			m_paintDC = NULL;
			m_paintSurface.Release();
			m_painting = FALSE;
			ScheduleResize();
		}
		else if(FAILED(hr) && client.right && client.bottom && m_state < QuadooForm::Destroying)
		{
			SetDCBrushColor(target,ResolveQuadooColor(Integer(L"BackColor")));
			FillRect(target,&client,static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
		}
		if(saved)
		{
			RestoreDC(target,saved);
		}
		return hr;
	}
	VOID CCanvas::ScheduleResize()
	{
		if(m_resizePending && !m_resizeQueued && !m_painting && !m_drawing && m_state == QuadooForm::Ready && m_hwnd)
		{
			m_resizeQueued = PostMessageW(m_hwnd,ResizeMessage,0,0);
			if(!m_resizeQueued)
			{
				ReportCallback(HrEnsureLastError(),L"OnResize");
			}
		}
	}
	LRESULT CCanvas::Message(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled)
	{
		if(message == WM_ERASEBKGND)
		{
			*handled = TRUE;
			return 1;
		}
		if(message == WM_PAINT || message == WM_PRINTCLIENT)
		{
			PAINTSTRUCT paint;
			RECT clip;
			HDC dc = message == WM_PAINT ? BeginPaint(window,&paint) : reinterpret_cast<HDC>(wp);
			*handled = TRUE;
			if(dc && m_state < QuadooForm::Destroying)
			{
				if(GetClipBox(dc,&clip) == NULLREGION)
				{
					GetClientRect(window,&clip);
				}
				ReportCallback(Paint(dc,&clip),L"OnPaint");
			}
			if(message == WM_PAINT)
			{
				EndPaint(window,&paint);
			}
			return 0;
		}
		if(message == WM_SIZE)
		{
			SIZE size = {LOWORD(lp),HIWORD(lp)};
			if(size.cx != m_knownSize.cx || size.cy != m_knownSize.cy)
			{
				m_knownSize = size;
				m_generation++;
				m_resizePending = TRUE;
				ScheduleResize();
			}
		}
		if(message == ResizeMessage)
		{
			m_resizeQueued = FALSE;
			*handled = TRUE;
			if(m_state == QuadooForm::Ready && !m_painting && !m_drawing)
			{
				m_resizePending = FALSE;
				ReportCallback(ResizeSurface(),L"OnResize");
			}
			return 0;
		}
		if(message == WM_MOUSEMOVE || message == WM_LBUTTONDOWN || message == WM_LBUTTONUP ||
			message == WM_RBUTTONDOWN || message == WM_RBUTTONUP || message == WM_MBUTTONDOWN || message == WM_MBUTTONUP)
		{
			BOOL down = message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN;
			BOOL move = message == WM_MOUSEMOVE;
			INT button = move ? 0 : (message == WM_LBUTTONDOWN || message == WM_LBUTTONUP) ? MK_LBUTTON :
				(message == WM_RBUTTONDOWN || message == WM_RBUTTONUP) ? MK_RBUTTON : MK_MBUTTON;
			INT values[] = {static_cast<SHORT>(LOWORD(lp)),static_cast<SHORT>(HIWORD(lp)),button,
				static_cast<INT>(wp & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON)),static_cast<INT>(wp & (MK_SHIFT | MK_CONTROL))};
			if(message == WM_LBUTTONDOWN)
			{
				m_pressed = TRUE;
				::SetFocus(window);
				SetCapture(window);
			}
			BOOL clicked = message == WM_LBUTTONUP && m_pressed && GetCapture() == window;
			if(message == WM_LBUTTONUP)
			{
				m_pressed = FALSE;
				if(GetCapture() == window)
				{
					ReleaseCapture();
				}
			}
			ReportCallback(CanvasEvent(move ? L"OnMouseMove" : down ? L"OnMouseDown" : L"OnMouseUp",values,ARRAYSIZE(values)),L"Canvas mouse");
			if(clicked && m_state == QuadooForm::Ready)
			{
				RECT rect;
				POINT point = {values[0],values[1]};
				GetClientRect(window,&rect);
				if(PtInRect(&rect,point))
				{
					ReportCallback(Dispatch(L"OnClick"),L"OnClick");
				}
			}
		}
		return Super::Message(window,message,wp,lp,content,handled);
	}
}
