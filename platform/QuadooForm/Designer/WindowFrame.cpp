#include "Designer.h"

namespace QFD
{
static INT Metric(INT index, UINT dpi)
{
	HDC dc = GetDC(NULL); INT system = dc ? GetDeviceCaps(dc,LOGPIXELSY) : 96; if(dc) ReleaseDC(NULL,dc);
	return max(1,MulDiv(GetSystemMetrics(index),dpi,system));
}
VOID CDesignerWindowFrame::Insets(IQuadooControl* root, BOOL form, UINT dpi, RECT* insets)
{
	ZeroMemory(insets,sizeof(*insets));
	if(form && IntProperty(root,L"BorderStyle",QuadooForm::BorderSizeable) != QuadooForm::BorderNone)
	{
		BOOL sizeable = IntProperty(root,L"BorderStyle",QuadooForm::BorderSizeable) == QuadooForm::BorderSizeable;
		insets->left = insets->right = Metric(sizeable ? SM_CXSIZEFRAME : SM_CXFIXEDFRAME,dpi);
		insets->bottom = Metric(sizeable ? SM_CYSIZEFRAME : SM_CYFIXEDFRAME,dpi);
		insets->top = insets->bottom + Metric(SM_CYCAPTION,dpi);
	}
	if(form)
	{
		CMenuCells cells;
		INT height = 0;
		QuadooForm::Rect bounds;
		if(SUCCEEDED(root->GetBounds(&bounds)) && SUCCEEDED(LayoutMenuBar(root,dpi,MulDiv(bounds.Width,dpi,96),cells,&height)))
		{
			insets->top += height;
		}
	}
}
VOID CDesignerWindowFrame::Paint(HWND window, HDC dc, IQuadooControl* root, BOOL form, UINT dpi, const RECT* client, REFGUID selected)
{
	RECT insets; Insets(root,form,dpi,&insets); if(!insets.top) return;
	RECT outer = { client->left - insets.left,client->top - insets.top,client->right + insets.right,client->bottom + insets.bottom };
	CMenuCells cells;
	INT menuHeight = 0;
	LayoutMenuBar(root,dpi,client->right - client->left,cells,&menuHeight);
	RECT menu = {client->left,client->top - menuHeight,client->right,client->top};
	if(IntProperty(root,L"BorderStyle",QuadooForm::BorderSizeable) == QuadooForm::BorderNone)
	{
		PaintMenuBar(window,dc,root,dpi,&menu,selected);
		return;
	}
	RECT caption = { outer.left + insets.left,outer.top + insets.bottom,outer.right - insets.right,client->top - menuHeight };
	FillRect(dc,&outer,GetSysColorBrush(COLOR_3DFACE)); DrawEdge(dc,&outer,EDGE_RAISED,BF_RECT);
	FillRect(dc,&caption,GetSysColorBrush(COLOR_ACTIVECAPTION));
	// Theme painting is best effort. Classic system-color/GDI rendering remains
	// available on XP and when the theme service is disabled.
	HMODULE theme = LoadLibraryW(L"uxtheme.dll");
	if(theme)
	{
		typedef HANDLE (WINAPI* OPEN_THEME)(HWND,PCWSTR);
		typedef HRESULT (WINAPI* DRAW_THEME)(HANDLE,HDC,INT,INT,const RECT*,const RECT*);
		typedef HRESULT (WINAPI* CLOSE_THEME)(HANDLE);
		OPEN_THEME open = reinterpret_cast<OPEN_THEME>(GetProcAddress(theme,"OpenThemeData"));
		DRAW_THEME draw = reinterpret_cast<DRAW_THEME>(GetProcAddress(theme,"DrawThemeBackground"));
		CLOSE_THEME close = reinterpret_cast<CLOSE_THEME>(GetProcAddress(theme,"CloseThemeData"));
		if(open && draw && close) { HANDLE data = open(window,L"WINDOW"); if(data) { draw(data,dc,1,1,&caption,NULL); close(data); } }
		FreeLibrary(theme);
	}
	INT icons = IntProperty(root,L"BorderIcons",15), buttonWidth = Metric(SM_CXSIZE,dpi);
	RECT text = caption; text.left += Metric(SM_CXEDGE,dpi);
	if(icons & QuadooForm::BorderSystemMenu)
	{
		HICON icon = NULL; TStackRef<IJSONValue> value;
		UINT size = min(256,Metric(SM_CXSMICON,dpi));
		if(SUCCEEDED(ReadProperty(root,L"Icon",&value))) QuadooLoadIconValue(value,size,size,&icon);
		DrawIconEx(dc,text.left,caption.top + (caption.bottom - caption.top - size) / 2,icon ? icon : LoadIcon(NULL,IDI_APPLICATION),size,size,0,NULL,DI_NORMAL);
		if(icon) DestroyIcon(icon); text.left += size + Metric(SM_CXEDGE,dpi);
		RECT button = caption; button.left = button.right - buttonWidth;
		DrawFrameControl(dc,&button,DFC_CAPTION,DFCS_CAPTIONCLOSE | ((icons & QuadooForm::BorderClose) ? 0 : DFCS_INACTIVE)); text.right = button.left;
		if(icons & QuadooForm::BorderMaximize) { OffsetRect(&button,-buttonWidth,0); DrawFrameControl(dc,&button,DFC_CAPTION,DFCS_CAPTIONMAX); text.right = button.left; }
		if(icons & QuadooForm::BorderMinimize) { OffsetRect(&button,-buttonWidth,0); DrawFrameControl(dc,&button,DFC_CAPTION,DFCS_CAPTIONMIN); text.right = button.left; }
	}
	NONCLIENTMETRICSW metrics; ZeroMemory(&metrics,sizeof(metrics));
	// The prefix ending at lfMessageFont is also the XP structure size.
	metrics.cbSize = FIELD_OFFSET(NONCLIENTMETRICSW,lfMessageFont) + sizeof(LOGFONTW);
	HFONT font = NULL;
	if(SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,metrics.cbSize,&metrics,0))
	{
		HDC screen = GetDC(NULL); INT system = screen ? GetDeviceCaps(screen,LOGPIXELSY) : 96; if(screen) ReleaseDC(NULL,screen);
		metrics.lfCaptionFont.lfHeight = MulDiv(metrics.lfCaptionFont.lfHeight,dpi,system); font = CreateFontIndirectW(&metrics.lfCaptionFont);
	}
	HGDIOBJ previous = SelectObject(dc,font ? font : GetStockObject(DEFAULT_GUI_FONT)); SetBkMode(dc,TRANSPARENT); SetTextColor(dc,GetSysColor(COLOR_CAPTIONTEXT));
	CRString title; if(FAILED(StringProperty(root,L"Caption",&title)) || !RStrLen(title)) { title = static_cast<RSTRING>(NULL); root->GetName(&title); }
	DrawTextW(dc,title.c_wstr(),RStrLen(title),&text,DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
	SelectObject(dc,previous); if(font) DeleteObject(font);
	PaintMenuBar(window,dc,root,dpi,&menu,selected);
}
}
