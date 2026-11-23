#include "ButtonDrawing.h"
#include <vssym32.h>

namespace QF
{
	CButtonTheme::CButtonTheme() : m_module(NULL), m_theme(NULL), m_window(NULL), m_close(NULL),
		m_background(NULL), m_content(NULL), m_parent(NULL), m_transparent(NULL), m_attempted(FALSE)
	{
	}
	CButtonTheme::~CButtonTheme()
	{
		Close();
	}
	VOID CButtonTheme::Close()
	{
		if(m_theme && m_close)
		{
			m_close(m_theme);
		}
		if(m_module)
		{
			FreeLibrary(m_module);
		}
		m_module = NULL;
		m_theme = NULL;
		m_window = NULL;
		m_close = NULL;
		m_background = NULL;
		m_content = NULL;
		m_parent = NULL;
		m_transparent = NULL;
		m_attempted = FALSE;
	}
	VOID CButtonTheme::Open(HWND window)
	{
		if(m_attempted || !window)
		{
			return;
		}
		m_attempted = TRUE;
		m_module = LoadLibraryW(L"uxtheme.dll");
		if(!m_module)
		{
			return;
		}
		OpenTheme open = reinterpret_cast<OpenTheme>(GetProcAddress(m_module,"OpenThemeData"));
		AppThemed themed = reinterpret_cast<AppThemed>(GetProcAddress(m_module,"IsAppThemed"));
		m_close = reinterpret_cast<CloseTheme>(GetProcAddress(m_module,"CloseThemeData"));
		m_background = reinterpret_cast<Background>(GetProcAddress(m_module,"DrawThemeBackground"));
		m_content = reinterpret_cast<Content>(GetProcAddress(m_module,"GetThemeBackgroundContentRect"));
		m_parent = reinterpret_cast<ParentBackground>(GetProcAddress(m_module,"DrawThemeParentBackground"));
		m_transparent = reinterpret_cast<PartTransparent>(GetProcAddress(m_module,"IsThemeBackgroundPartiallyTransparent"));
		if(open && themed && themed() && m_close && m_background && m_content)
		{
			m_theme = open(window,L"BUTTON");
			m_window = window;
		}
	}
	HRESULT CButtonTheme::Paint(HDC dc, RECT* bounds, UINT state, RECT* content)
	{
		HRESULT hr = S_FALSE;
		HMODULE retained = NULL;
		if(m_theme)
		{
			HTHEME current = m_theme;
			HWND window = m_window;
			// Parent-background drawing can reenter application/container code.
			// Keep the DLL loaded and recheck the theme after that call.
			CheckIf(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
				reinterpret_cast<PCWSTR>(m_background),&retained),S_FALSE);
			if(m_parent && m_transparent && m_transparent(m_theme,BP_PUSHBUTTON,state))
			{
				m_parent(m_window,dc,bounds);
			}
			CheckIfIgnore(m_theme != current || m_window != window,S_FALSE);
			hr = m_background(current,dc,BP_PUSHBUTTON,state,bounds,NULL);
			if(SUCCEEDED(hr))
			{
				hr = m_content(current,dc,BP_PUSHBUTTON,state,bounds,content);
			}
		}
	Cleanup:
		if(retained)
		{
			FreeLibrary(retained);
		}
		return hr;
	}
	HRESULT ImageButtonLayout(const RECT* content, const SIZE* caption, UINT position, UINT width,
		UINT height, UINT spacing, BOOL image, RECT* glyph, RECT* text)
	{
		HRESULT hr = S_OK;
		CheckIf(!content || !caption || !glyph || !text,E_POINTER);
		SetRectEmpty(glyph);
		SetRectEmpty(text);
		LONGLONG wide = static_cast<LONGLONG>(content->right) - content->left;
		LONGLONG high = static_cast<LONGLONG>(content->bottom) - content->top;
		CheckIf(position > 1 || width > 2048 || height > 2048 || spacing > 8192 || wide < 0 || high < 0 ||
			wide > INT_MAX || high > INT_MAX || caption->cx < 0 || caption->cy < 0,E_INVALIDARG);
		INT cx = static_cast<INT>(wide);
		INT cy = static_cast<INT>(high);
		BOOL label = caption->cx > 0 && caption->cy > 0;
		INT gw = image ? min(cx,static_cast<INT>(width)) : 0;
		INT gh = image ? min(cy,static_cast<INT>(height)) : 0;
		INT tw = label ? min(cx,caption->cx) : 0;
		INT th = label ? min(cy,caption->cy) : 0;
		INT gap = gw && gh && label ? static_cast<INT>(spacing) : 0;
		if(position)
		{
			gap = min(gap,max(0,cy - th));
			gh = min(gh,max(0,cy - th - gap));
			if(!gh)
			{
				gw = 0;
				gap = 0;
			}
			INT y = content->top + (cy - gh - gap - th) / 2;
			SetRect(glyph,content->left + (cx - gw) / 2,y,content->left + (cx + gw) / 2,y + gh);
			y += gh + gap;
			SetRect(text,content->left + (cx - tw) / 2,y,content->left + (cx + tw) / 2,y + th);
		}
		else
		{
			gap = min(gap,max(0,cx - gw));
			tw = min(tw,max(0,cx - gw - gap));
			INT x = content->left + (cx - gw - gap - tw) / 2;
			SetRect(glyph,x,content->top + (cy - gh) / 2,x + gw,content->top + (cy + gh) / 2);
			x += gw + gap;
			SetRect(text,x,content->top + (cy - th) / 2,x + tw,content->top + (cy + th) / 2);
		}
	Cleanup:
		return hr;
	}
	static HRESULT RenderButton(HDC dc, RECT bounds, const ImageButtonAppearance* a, CButtonTheme* theme)
	{
		HRESULT hr;
		RECT content;
		RECT glyph;
		RECT text;
		RECT measured = {0};
		SIZE caption = {0};
		UINT state = !a->enabled ? PBS_DISABLED : a->pressed ? PBS_PRESSED : a->hot ? PBS_HOT : a->isDefault ? PBS_DEFAULTED : PBS_NORMAL;
		BOOL themed = FALSE;
		CheckIf(SetDCBrushColor(dc,a->back) == CLR_INVALID,HrEnsureLastError());
		CheckIf(!FillRect(dc,&bounds,static_cast<HBRUSH>(GetStockObject(DC_BRUSH))),HrEnsureLastError());
		if(theme)
		{
			themed = theme->Paint(dc,&bounds,state,&content) == S_OK;
		}
		if(!themed)
		{
			RECT face = bounds;
			if(a->isDefault)
			{
				FrameRect(dc,&face,GetSysColorBrush(COLOR_WINDOWFRAME));
				InflateRect(&face,-1,-1);
			}
			if(face.right > face.left && face.bottom > face.top)
			{
				DrawFrameControl(dc,&face,DFC_BUTTON,DFCS_BUTTONPUSH | (a->pressed ? DFCS_PUSHED : 0) | (!a->enabled ? DFCS_INACTIVE : 0));
			}
			content = bounds;
			InflateRect(&content,-MulDiv(4,a->dpi,96),-MulDiv(3,a->dpi,96));
		}
		content.left = max(bounds.left,min(content.left,bounds.right));
		content.top = max(bounds.top,min(content.top,bounds.bottom));
		content.right = max(content.left,min(content.right,bounds.right));
		content.bottom = max(content.top,min(content.bottom,bounds.bottom));
		SelectObject(dc,a->font ? a->font : GetStockObject(DEFAULT_GUI_FONT));
		SetBkMode(dc,TRANSPARENT);
		SetTextColor(dc,a->enabled ? a->fore : GetSysColor(COLOR_GRAYTEXT));
		if(a->length)
		{
			DrawTextW(dc,a->caption,a->length,&measured,DT_CALCRECT | DT_SINGLELINE);
		}
		caption.cx = max(0,measured.right);
		caption.cy = max(0,measured.bottom);
		Check(ImageButtonLayout(&content,&caption,a->position,MulDiv(a->width,a->dpi,96),
			MulDiv(a->height,a->dpi,96),MulDiv(a->spacing,a->dpi,96),a->picture || a->disabledPicture,&glyph,&text));
		if(a->pressed && !themed)
		{
			OffsetRect(&glyph,1,1);
			OffsetRect(&text,1,1);
		}
		CheckIf(IntersectClipRect(dc,content.left,content.top,content.right,content.bottom) == ERROR,E_FAIL);
		QFI::CImageResource* picture = !a->enabled && a->disabledPicture ? a->disabledPicture : a->picture;
		if(picture && !IsRectEmpty(&glyph))
		{
			if(!a->enabled && !a->disabledPicture)
			{
				Check(picture->PaintGrayscale(dc,&glyph,a->dpi,a->filter));
			}
			else
			{
				Check(picture->Paint(dc,&glyph,a->dpi,QFI::Contain,4,a->filter,255));
			}
		}
		if(a->length && !IsRectEmpty(&text))
		{
			DrawTextW(dc,a->caption,a->length,&text,DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS | (a->hideAccel ? DT_HIDEPREFIX : 0));
		}
		if(a->focused && !a->hideFocus && content.right > content.left && content.bottom > content.top)
		{
			DrawFocusRect(dc,&content);
		}
		hr = S_OK;
	Cleanup:
		return hr;
	}
	HRESULT PaintImageButton(HDC dc, const RECT* bounds, const ImageButtonAppearance* a, CButtonTheme* theme)
	{
		HRESULT hr;
		QFI::CImageSurface surface;
		INT saved = 0;
		CheckIf(!dc || !bounds || !a,E_POINTER);
		CheckIf(a->dpi < 48 || a->dpi > 768 || a->position > 1 || a->width < 1 || a->width > 256 ||
			a->height < 1 || a->height > 256 || a->spacing > 1024 || a->filter > QFI::HighQuality ||
			a->length > 32767 || (a->length && !a->caption),E_INVALIDARG);
		LONGLONG width = static_cast<LONGLONG>(bounds->right) - bounds->left;
		LONGLONG height = static_cast<LONGLONG>(bounds->bottom) - bounds->top;
		CheckIf(width < 0 || height < 0 || width > INT_MAX || height > INT_MAX,E_INVALIDARG);
		CheckIfIgnore(!width || !height,S_OK);
		if(width <= QFI::MaxDimension && height <= QFI::MaxDimension && width <= QFI::MaxPixels / height)
		{
			RECT local = {0,0,static_cast<LONG>(width),static_cast<LONG>(height)};
			Check(surface.Create(static_cast<UINT>(width),static_cast<UINT>(height)));
			Check(RenderButton(surface.DC(),local,a,theme));
			CheckIf(!BitBlt(dc,bounds->left,bounds->top,static_cast<INT>(width),static_cast<INT>(height),surface.DC(),0,0,SRCCOPY),HrEnsureLastError());
		}
		else
		{
			// Oversized controls still draw without an unbounded temporary surface.
			saved = SaveDC(dc);
			CheckIf(!saved,HrEnsureLastError());
			CheckIf(IntersectClipRect(dc,bounds->left,bounds->top,bounds->right,bounds->bottom) == ERROR,E_FAIL);
			Check(RenderButton(dc,*bounds,a,theme));
		}
		hr = S_OK;
	Cleanup:
		if(saved)
		{
			RestoreDC(dc,saved);
		}
		return hr;
	}
}
