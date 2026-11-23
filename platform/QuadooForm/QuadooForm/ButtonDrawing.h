#pragma once
#include "..\Imaging\ImageSurface.h"
#include <uxtheme.h>

namespace QF
{
	class CButtonTheme
	{
		typedef HTHEME (WINAPI* OpenTheme)(HWND,LPCWSTR);
		typedef HRESULT (WINAPI* CloseTheme)(HTHEME);
		typedef BOOL (WINAPI* AppThemed)();
		typedef HRESULT (WINAPI* Background)(HTHEME,HDC,int,int,const RECT*,const RECT*);
		typedef HRESULT (WINAPI* Content)(HTHEME,HDC,int,int,const RECT*,RECT*);
		typedef HRESULT (WINAPI* ParentBackground)(HWND,HDC,RECT*);
		typedef BOOL (WINAPI* PartTransparent)(HTHEME,int,int);
		HMODULE m_module;
		HTHEME m_theme;
		HWND m_window;
		CloseTheme m_close;
		Background m_background;
		Content m_content;
		ParentBackground m_parent;
		PartTransparent m_transparent;
		BOOL m_attempted;
	public:
		CButtonTheme();
		~CButtonTheme();
		VOID Open(HWND window);
		VOID Close();
		HRESULT Paint(HDC dc, RECT* bounds, UINT state, RECT* content);
	};
	struct ImageButtonAppearance
	{
		UINT dpi;
		UINT position;
		UINT width;
		UINT height;
		UINT spacing;
		UINT filter;
		BOOL enabled;
		BOOL pressed;
		BOOL hot;
		BOOL focused;
		BOOL isDefault;
		BOOL hideFocus;
		BOOL hideAccel;
		HFONT font;
		COLORREF fore;
		COLORREF back;
		PCWSTR caption;
		UINT length;
		QFI::CImageResource* picture;
		QFI::CImageResource* disabledPicture;
	};
	HRESULT ImageButtonLayout(const RECT* content, const SIZE* caption, UINT position, UINT width,
		UINT height, UINT spacing, BOOL image, RECT* glyph, RECT* text);
	HRESULT PaintImageButton(HDC dc, const RECT* bounds, const ImageButtonAppearance* appearance, CButtonTheme* theme = NULL);
}
