#include "Designer.h"

namespace QFD
{
static INT MenuMetric(INT metric, UINT dpi)
{
	HDC dc = GetDC(NULL);
	INT system = dc ? GetDeviceCaps(dc,LOGPIXELSY) : 96;
	if(dc)
	{
		ReleaseDC(NULL,dc);
	}
	return max(1,MulDiv(GetSystemMetrics(metric),dpi,system));
}
static HFONT MenuFont(UINT dpi)
{
	NONCLIENTMETRICSW metrics = {0};
	metrics.cbSize = FIELD_OFFSET(NONCLIENTMETRICSW,lfMessageFont) + sizeof(LOGFONTW);
	HFONT font = NULL;
	if(SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,metrics.cbSize,&metrics,0))
	{
		HDC dc = GetDC(NULL);
		INT system = dc ? GetDeviceCaps(dc,LOGPIXELSY) : 96;
		if(dc)
		{
			ReleaseDC(NULL,dc);
		}
		metrics.lfMenuFont.lfHeight = MulDiv(metrics.lfMenuFont.lfHeight,dpi,system);
		font = CreateFontIndirectW(&metrics.lfMenuFont);
	}
	return font;
}
HRESULT AssignedMainMenu(IQuadooControl* root, IQuadooComponent** menu)
{
	HRESULT hr;
	TStackRef<IQuadooFormMenu> form;
	CheckIf(!menu,E_POINTER);
	*menu = NULL;
	hr = root->QueryInterface(__uuidof(IQuadooFormMenu),reinterpret_cast<PVOID*>(&form));
	CheckIfIgnore(hr == E_NOINTERFACE,S_OK);
	Check(hr);
	Check(form->GetMainMenu(menu));
Cleanup:
	return hr;
}
HRESULT LayoutMenuBar(IQuadooControl* root, UINT dpi, INT width, CMenuCells& cells, INT* height)
{
	HRESULT hr;
	TStackRef<IQuadooComponent> menu;
	TStackRef<IQuadooMenuItems> items;
	HDC dc = NULL;
	HFONT font = NULL;
	HGDIOBJ previous = NULL;
	UINT count;
	CheckIf(!height,E_POINTER);
	*height = 0;
	cells.Clear();
	Check(AssignedMainMenu(root,&menu));
	CheckIfIgnore(!menu || width <= 0,S_OK);
	Check(menu->QueryInterface(__uuidof(IQuadooMenuItems),reinterpret_cast<PVOID*>(&items)));
	Check(items->GetItemCount(&count));
	dc = GetDC(NULL);
	CheckIf(!dc,HrEnsureLastError());
	font = MenuFont(dpi);
	previous = SelectObject(dc,font ? font : GetStockObject(DEFAULT_GUI_FONT));
	TEXTMETRICW metrics;
	CheckIf(!GetTextMetricsW(dc,&metrics),HrEnsureLastError());
	INT padding = max(MenuMetric(SM_CXEDGE,dpi) * 3,MulDiv(8,dpi,96));
	INT rowHeight = max(MenuMetric(SM_CYMENU,dpi),metrics.tmHeight + MenuMetric(SM_CYEDGE,dpi) * 2);
	INT x = 0;
	INT y = 0;
	for(UINT n = 0; n < count; n++)
	{
		TStackRef<IQuadooComponent> item;
		MenuCell cell;
		CRString caption;
		RECT measure = {0,0,0,0};
		Check(items->GetItem(n,&item));
		if(!BoolProperty(item,L"Visible",TRUE) || BoolProperty(item,L"Separator"))
		{
			continue;
		}
		Check(item->GetID(&cell.id));
		Check(StringProperty(item,L"Caption",&caption));
		cell.caption = caption;
		cell.enabled = BoolProperty(item,L"Enabled",TRUE);
		DrawTextW(dc,RStrToWide(cell.caption),RStrLen(cell.caption),&measure,DT_SINGLELINE | DT_CALCRECT);
		INT itemWidth = min(width,max(padding * 2,measure.right + padding * 2));
		if(x && x + itemWidth > width)
		{
			x = 0;
			y += rowHeight;
		}
		SetRect(&cell.bounds,x,y,x + itemWidth,y + rowHeight);
		Check(cells.Append(cell));
		x += itemWidth;
	}
	if(cells.Length())
	{
		*height = y + rowHeight;
	}
Cleanup:
	if(previous)
	{
		SelectObject(dc,previous);
	}
	if(font)
	{
		DeleteObject(font);
	}
	if(dc)
	{
		ReleaseDC(NULL,dc);
	}
	return hr;
}
VOID PaintMenuBar(HWND window, HDC dc, IQuadooControl* root, UINT dpi, const RECT* rect, REFGUID selected)
{
	CMenuCells cells;
	INT height;
	if(FAILED(LayoutMenuBar(root,dpi,rect->right - rect->left,cells,&height)) || !height)
	{
		return;
	}
	INT saved = SaveDC(dc);
	IntersectClipRect(dc,rect->left,rect->top,rect->right,rect->bottom);
	FillRect(dc,rect,GetSysColorBrush(COLOR_MENU));
	HFONT font = MenuFont(dpi);
	HGDIOBJ previous = SelectObject(dc,font ? font : GetStockObject(DEFAULT_GUI_FONT));
	SetBkMode(dc,TRANSPARENT);
	// MENU bar theme parts are unavailable on some XP themes. The system-color
	// fill and DrawText path below is always sufficient to paint the entire bar.
	HMODULE theme = LoadLibraryW(L"uxtheme.dll");
	typedef HANDLE (WINAPI* OPEN_THEME)(HWND,PCWSTR);
	typedef HRESULT (WINAPI* DRAW_THEME)(HANDLE,HDC,INT,INT,const RECT*,const RECT*);
	typedef HRESULT (WINAPI* CLOSE_THEME)(HANDLE);
	OPEN_THEME open = theme ? reinterpret_cast<OPEN_THEME>(GetProcAddress(theme,"OpenThemeData")) : NULL;
	DRAW_THEME draw = theme ? reinterpret_cast<DRAW_THEME>(GetProcAddress(theme,"DrawThemeBackground")) : NULL;
	CLOSE_THEME close = theme ? reinterpret_cast<CLOSE_THEME>(GetProcAddress(theme,"CloseThemeData")) : NULL;
	HANDLE data = open && draw && close ? open(window,L"MENU") : NULL;
	if(data)
	{
		draw(data,dc,7,1,rect,NULL);
	}
	for(sysint n = 0; n < cells.Length(); n++)
	{
		RECT bounds = cells[n].bounds;
		OffsetRect(&bounds,rect->left,rect->top);
		BOOL active = IsEqualGUID(cells[n].id,selected);
		if(active)
		{
			FillRect(dc,&bounds,GetSysColorBrush(COLOR_HIGHLIGHT));
		}
		SetTextColor(dc,GetSysColor(active ? COLOR_HIGHLIGHTTEXT : cells[n].enabled ? COLOR_MENUTEXT : COLOR_GRAYTEXT));
		InflateRect(&bounds,-MulDiv(6,dpi,96),0);
		DrawTextW(dc,RStrToWide(cells[n].caption),RStrLen(cells[n].caption),&bounds,DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_END_ELLIPSIS);
	}
	if(data)
	{
		close(data);
	}
	if(theme)
	{
		FreeLibrary(theme);
	}
	SelectObject(dc,previous);
	if(font)
	{
		DeleteObject(font);
	}
	RestoreDC(dc,saved);
}
HRESULT CDesignerEditor::MenuSelectionRect(RECT* rect)
{
	HRESULT hr;
	CMenuCells cells;
	INT height;
	CheckIf(!m_document.Root() || !rect,E_INVALIDARG);
	CheckIf(!GetWindowRect(m_document.Host(),rect),HrEnsureLastError());
	MapWindowPoints(NULL,m_canvas,reinterpret_cast<POINT*>(rect),2);
	Check(LayoutMenuBar(m_document.Root(),MulDiv(m_dpi,m_zoom,100),rect->right - rect->left,cells,&height));
	for(sysint n = 0; n < cells.Length(); n++)
	{
		if(IsEqualGUID(cells[n].id,m_selected))
		{
			RECT selected = cells[n].bounds;
			OffsetRect(&selected,rect->left,rect->top - height);
			*rect = selected;
			goto Cleanup;
		}
	}
	hr = S_FALSE;
Cleanup:
	return hr;
}
HRESULT CDesignerEditor::SelectMenuAt(POINT point)
{
	HRESULT hr;
	CMenuCells cells;
	INT height;
	RECT client;
	CheckIf(!GetWindowRect(m_document.Host(),&client),HrEnsureLastError());
	MapWindowPoints(NULL,m_canvas,reinterpret_cast<POINT*>(&client),2);
	Check(LayoutMenuBar(m_document.Root(),MulDiv(m_dpi,m_zoom,100),client.right - client.left,cells,&height));
	for(sysint n = 0; n < cells.Length(); n++)
	{
		RECT bounds = cells[n].bounds;
		OffsetRect(&bounds,client.left,client.top - height);
		if(PtInRect(&bounds,point))
		{
			Check(SelectID(cells[n].id));
			goto Cleanup;
		}
	}
	hr = S_FALSE;
Cleanup:
	return hr;
}
}
