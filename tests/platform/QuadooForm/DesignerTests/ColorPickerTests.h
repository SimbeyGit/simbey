#include "..\..\..\..\platform\QuadooForm\QuadooForm\ColorNames.h"
#include "..\..\..\..\platform\QuadooForm\QuadooForm\PaletteResources.h"
#include "ColorPickerData.h"
#include <ColorDlg.h>

enum PickerTest { PickerCancel, PickerKeep, PickerVista, PickerSystem, PickerHex, PickerInvalidCancel, PickerCustom, PickerCustomCancel, PickerEscape, PickerEnter, PickerWheel, PickerClose };
static struct
{
	HWND owner, picker, edit, accept, cancel, scroll;
	PickerTest test;
	COLORREF initial, expected;
	HRESULT error;
	UINT ticks, step;
	BOOL seen, standardSeen, standardCorrect, windowsFull;
	HWND windows[32];
	UINT windowCount;
	HICON icons[2];
} g_picker;

static BOOL CALLBACK PickerChildren(HWND window, LPARAM)
{
	BOOL recorded = FALSE;
	for(UINT n = 0; n < g_picker.windowCount; n++) if(g_picker.windows[n] == window) recorded = TRUE;
	if(!recorded)
	{
		if(g_picker.windowCount < ARRAYSIZE(g_picker.windows)) g_picker.windows[g_picker.windowCount++] = window;
		else g_picker.windowsFull = TRUE;
	}
	WCHAR type[40], caption[64];
	GetClassNameW(window,type,ARRAYSIZE(type));
	GetWindowTextW(window,caption,ARRAYSIZE(caption));
	if(!lstrcmpiW(type,L"EDIT")) g_picker.edit = window;
	if(GetWindowLongW(window,GWL_STYLE) & WS_VSCROLL) g_picker.scroll = window;
	if(!lstrcmpiW(type,L"BUTTON") && !lstrcmpW(caption,L"OK")) g_picker.accept = window;
	if(!lstrcmpiW(type,L"BUTTON") && !lstrcmpW(caption,L"Cancel")) g_picker.cancel = window;
	return TRUE;
}

static HWND PickerPoint(INT x, INT y, POINT* local)
{
	RECT client;
	HWND parent = g_picker.picker, child;
	GetClientRect(parent,&client);
	// The footer is 48 logical pixels. Its width remains 920 on ordinary
	// displays; use the actual font DPI for all swatch coordinates.
	HDC dc = GetDC(parent);
	INT dpi = GetDeviceCaps(dc,LOGPIXELSX);
	ReleaseDC(parent,dc);
	if(g_picker.scroll)
	{
		SCROLLINFO info = {sizeof(info),SIF_POS};
		GetScrollInfo(g_picker.scroll,SB_VERT,&info);
		y -= info.nPos;
	}
	local->x = MulDiv(x,dpi,96); local->y = MulDiv(y,dpi,96);
	for(UINT depth = 0; depth < 16; depth++)
	{
		child = ChildWindowFromPointEx(parent,*local,CWP_SKIPINVISIBLE | CWP_SKIPDISABLED | CWP_SKIPTRANSPARENT);
		if(!child || child == parent) break;
		MapWindowPoints(parent,child,local,1);
		parent = child;
	}
	return parent;
}

static VOID PickerClick(INT x, INT y)
{
	POINT point;
	if(g_picker.scroll)
	{
		RECT viewport;
		HDC dc = GetDC(g_picker.scroll);
		INT dpi = GetDeviceCaps(dc,LOGPIXELSY);
		ReleaseDC(g_picker.scroll,dc);
		GetClientRect(g_picker.scroll,&viewport);
		SCROLLINFO info = {sizeof(info),SIF_POS};
		GetScrollInfo(g_picker.scroll,SB_VERT,&info);
		BOOL bottom = MulDiv(y + 12,dpi,96) >= viewport.bottom;
		if(bottom || info.nPos) SendMessageW(g_picker.scroll,WM_VSCROLL,bottom ? SB_BOTTOM : SB_TOP,0);
	}
	HWND target = PickerPoint(x,y,&point);
	SendMessageW(target,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(point.x,point.y));
	SendMessageW(target,WM_LBUTTONUP,0,MAKELPARAM(point.x,point.y));
}

static BOOL PickerPreview(COLORREF expected)
{
	POINT point;
	if(g_picker.scroll)
	{
		SCROLLINFO info = {sizeof(info),SIF_POS};
		GetScrollInfo(g_picker.scroll,SB_VERT,&info);
		if(info.nPos) SendMessageW(g_picker.scroll,WM_VSCROLL,SB_TOP,0);
	}
	HWND target = PickerPoint(699,74,&point);
	RECT client;
	CImageTestCanvas canvas;
	GetClientRect(target,&client);
	if(FAILED(canvas.Create(client.right,client.bottom))) return FALSE;
	canvas.Fill(RGB(11,22,33));
	SendMessageW(target,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(canvas.DC()),PRF_CLIENT);
	COLORREF actual = canvas.Pixel(point.x,point.y);
	if(actual != ResolveQuadooColor(expected))
	{
		CRString text; if(SUCCEEDED(RStrFormatW(&text,L"Preview step %u: %.8x, expected %.8x\r\n",g_picker.step,actual,ResolveQuadooColor(expected)))) Log(text.c_wstr());
	}
	return actual == ResolveQuadooColor(expected);
}

static BOOL PickerText(COLORREF expected)
{
	WCHAR actual[80], text[80];
	if((expected & 0xff000000) == 0x80000000) wsprintfW(text,L"0x%.8X",expected);
	else wsprintfW(text,L"#%.2X%.2X%.2X",GetRValue(expected),GetGValue(expected),GetBValue(expected));
	GetWindowTextW(g_picker.edit,actual,ARRAYSIZE(actual));
	if(lstrcmpW(actual,text))
	{
		CRString detail; if(SUCCEEDED(RStrFormatW(&detail,L"Picker step %u text: %ls, expected %ls\r\n",g_picker.step,actual,text))) Log(detail.c_wstr());
	}
	return !lstrcmpW(actual,text);
}

static BOOL CALLBACK PickerWindow(HWND window, LPARAM)
{
	WCHAR caption[80];
	GetWindowTextW(window,caption,ARRAYSIZE(caption));
	if(!lstrcmpW(caption,L"Choose Color") && GetWindow(window,GW_OWNER) == g_picker.owner)
	{
		g_picker.picker = window;
		return FALSE;
	}
	return TRUE;
}

static BOOL CALLBACK PickerStandard(HWND window, LPARAM)
{
	if(GetWindow(window,GW_OWNER) != g_picker.picker || !GetDlgItem(window,COLOR_RED)) return TRUE;
	g_picker.standardSeen = TRUE;
	g_picker.standardCorrect = GetDlgItemInt(window,COLOR_RED,NULL,FALSE) == GetRValue(ResolveQuadooColor(g_picker.initial)) &&
		GetDlgItemInt(window,COLOR_GREEN,NULL,FALSE) == GetGValue(ResolveQuadooColor(g_picker.initial)) &&
		GetDlgItemInt(window,COLOR_BLUE,NULL,FALSE) == GetBValue(ResolveQuadooColor(g_picker.initial));
	if(g_picker.test == PickerCustom)
	{
		SetDlgItemInt(window,COLOR_RED,41,FALSE);
		SetDlgItemInt(window,COLOR_GREEN,82,FALSE);
		SetDlgItemInt(window,COLOR_BLUE,123,FALSE);
	}
	g_picker.step = 2;
	SendMessageW(window,WM_COMMAND,g_picker.test == PickerCustom ? IDOK : IDCANCEL,0);
	return FALSE;
}

static HRESULT PickerStep()
{
	HRESULT hr = S_OK;
	if(!g_picker.seen)
	{
		EnumThreadWindows(GetCurrentThreadId(),PickerWindow,0);
		CheckIfIgnore(!g_picker.picker,S_FALSE);
		EnumChildWindows(g_picker.picker,PickerChildren,0);
		REQUIRE(g_picker.edit && g_picker.accept && g_picker.cancel && !g_picker.windowsFull);
		g_picker.icons[0] = reinterpret_cast<HICON>(SendMessageW(g_picker.picker,WM_GETICON,ICON_SMALL,0));
		g_picker.icons[1] = reinterpret_cast<HICON>(SendMessageW(g_picker.picker,WM_GETICON,ICON_BIG,0));
		g_picker.seen = TRUE;
		REQUIRE(PickerText(g_picker.initial) && PickerPreview(g_picker.initial));
		REQUIRE(GetFocus() == g_picker.edit);
		REQUIRE(!IsWindowEnabled(g_picker.owner));
		REQUIRE(!(GetWindowLongW(g_picker.picker,GWL_STYLE) & (WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX)));
		REQUIRE(SendMessageW(g_picker.picker,WM_GETICON,ICON_SMALL,0) != 0);
		RECT client, first, second;
		GetClientRect(g_picker.picker,&client);
		GetWindowRect(g_picker.accept,&first); GetWindowRect(g_picker.cancel,&second);
		MapWindowPoints(NULL,g_picker.picker,reinterpret_cast<POINT*>(&first),2);
		MapWindowPoints(NULL,g_picker.picker,reinterpret_cast<POINT*>(&second),2);
		REQUIRE(first.left + second.right >= client.right - 1 && first.left + second.right <= client.right + 1 && first.bottom <= client.bottom);
	}
	if(g_picker.test == PickerCustom || g_picker.test == PickerCustomCancel)
	{
		if(g_picker.step == 0)
		{
			g_picker.step = 1;
			PickerClick(699,74);
			goto Cleanup;
		}
		if(g_picker.step == 1) { EnumThreadWindows(GetCurrentThreadId(),PickerStandard,0); goto Cleanup; }
		REQUIRE(g_picker.standardSeen && g_picker.standardCorrect);
		REQUIRE(PickerText(g_picker.expected) && PickerPreview(g_picker.expected));
	}
	else if(g_picker.test == PickerVista)
	{
		if(g_picker.step < ARRAYSIZE(PickerVistaColors))
		{
			UINT n = g_picker.step++;
			g_picker.expected = PickerVistaColors[n];
			PickerClick(16 + (n % 21)*22 + 9,38 + (n / 21)*22 + 9);
			REQUIRE(PickerText(g_picker.expected) && PickerPreview(g_picker.expected));
			goto Cleanup;
		}
	}
	else if(g_picker.test == PickerSystem)
	{
		UINT offset = 0;
		for(UINT n = 0; n < ARRAYSIZE(PickerGroups); n++)
		{
			if(g_picker.step < offset + PickerGroups[n].count)
			{
				UINT item = g_picker.step++ - offset;
				g_picker.expected = 0x80000000 | PickerGroups[n].colors[item];
				PickerClick(16 + (2*item + 1)*888/(2*PickerGroups[n].count),PickerGroups[n].y + 10);
				REQUIRE(PickerText(g_picker.expected) && PickerPreview(g_picker.expected));
				goto Cleanup;
			}
			offset += PickerGroups[n].count;
		}
	}
	else if(g_picker.test == PickerHex)
	{
		static PCWSTR const texts[] = { L"#1E90FF",L"a1b2c3",L" 0x8000000F ",L"0X80000000",L"112233",
			L"",L"#12",L"#GG0000",L"0x80000019",L"0x8000001F",L"0x8100000D",L"0x80010000",L"#1234567",L"123456garbage",L"#1e90ff" };
		if(g_picker.step < ARRAYSIZE(texts))
		{
			UINT n = g_picker.step++;
			static const COLORREF valid[] = { RGB(30,144,255),RGB(161,178,195),0x8000000f,0x80000000,RGB(17,34,51) };
			SetWindowTextW(g_picker.edit,texts[n]);
			BOOL accepted = n < ARRAYSIZE(valid) || n == ARRAYSIZE(texts) - 1;
			if(accepted) g_picker.expected = n < ARRAYSIZE(valid) ? valid[n] : RGB(30,144,255);
			REQUIRE(!!IsWindowEnabled(g_picker.accept) == accepted);
			REQUIRE(PickerPreview(g_picker.expected));
			goto Cleanup;
		}
	}
	else if(g_picker.test == PickerWheel)
	{
		if(!g_picker.step)
		{
			RECT bounds;
			GetWindowRect(g_picker.picker,&bounds);
			REQUIRE(SetWindowPos(g_picker.picker,NULL,0,0,bounds.right - bounds.left,420,SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE));
			EnumChildWindows(g_picker.picker,PickerChildren,0);
			REQUIRE(g_picker.scroll);
			for(UINT n = 0; n < 20; n++)
			{
				SendMessageW(g_picker.edit,WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA),0);
				SendMessageW(g_picker.scroll,WM_VSCROLL,SB_LINEUP,0);
				SendMessageW(g_picker.scroll,WM_VSCROLL,SB_PAGEUP,0);
				SendMessageW(g_picker.scroll,WM_HSCROLL,SB_LINELEFT,0);
				SendMessageW(g_picker.scroll,WM_HSCROLL,SB_PAGELEFT,0);
			}
			SCROLLINFO info = {sizeof(info),SIF_ALL};
			REQUIRE(GetScrollInfo(g_picker.scroll,SB_VERT,&info) && info.nPos == 0);
			for(UINT n = 0; n < 30; n++) SendMessageW(g_picker.scroll,WM_MOUSEWHEEL,MAKEWPARAM(0,static_cast<WORD>(-WHEEL_DELTA)),0);
			REQUIRE(GetScrollInfo(g_picker.scroll,SB_VERT,&info) && info.nPos == max(0,info.nMax - static_cast<INT>(info.nPage) + 1));
			for(UINT n = 0; n < 20; n++) { SendMessageW(g_picker.scroll,WM_VSCROLL,SB_LINEDOWN,0); SendMessageW(g_picker.scroll,WM_VSCROLL,SB_PAGEDOWN,0); }
			g_picker.step++;
			goto Cleanup; // Let the real modal loop observe any recorded callback error.
		}
		REQUIRE(IsWindow(g_picker.picker) && !IsWindowEnabled(g_picker.owner));
		SendMessageW(g_picker.scroll,WM_VSCROLL,SB_TOP,0);
		REQUIRE(PickerText(g_picker.initial) && PickerPreview(g_picker.initial));
	}
	else if(g_picker.test == PickerInvalidCancel)
	{
		SetWindowTextW(g_picker.edit,L"#invalid");
		REQUIRE(!IsWindowEnabled(g_picker.accept));
		SendMessageW(g_picker.accept,BM_CLICK,0,0);
		REQUIRE(IsWindow(g_picker.picker));
		REQUIRE(PickerPreview(g_picker.initial));
	}
	if(g_picker.test == PickerClose) PostMessageW(g_picker.picker,WM_CLOSE,0,0);
	else if(g_picker.test == PickerEscape || g_picker.test == PickerEnter)
	{
		SetFocus(g_picker.edit);
		PostMessageW(g_picker.edit,WM_KEYDOWN,g_picker.test == PickerEscape ? VK_ESCAPE : VK_RETURN,0);
	}
	else SendMessageW(g_picker.test == PickerCancel || g_picker.test == PickerCustomCancel || g_picker.test == PickerInvalidCancel ? g_picker.cancel : g_picker.accept,BM_CLICK,0,0);
Cleanup:
	return hr;
}

static VOID CALLBACK PickerTimer(HWND window, UINT, UINT_PTR timer, DWORD)
{
	if(++g_picker.ticks > 500)
	{
		g_picker.error = HRESULT_FROM_WIN32(ERROR_TIMEOUT);
		KillTimer(window,timer); PostQuitMessage(1); return;
	}
	HRESULT hr = PickerStep();
	if(FAILED(hr))
	{
		g_picker.error = hr;
		if(g_picker.picker) PostMessageW(g_picker.picker,WM_CLOSE,0,0);
	}
}

static HRESULT StartPicker(HWND owner, PickerTest test, COLORREF initial)
{
	ZeroMemory(&g_picker,sizeof(g_picker));
	g_picker.owner = owner; g_picker.test = test; g_picker.initial = g_picker.expected = initial;
	if(test == PickerCustom) g_picker.expected = RGB(41,82,123);
	return SetTimer(owner,5430,10,PickerTimer) ? S_OK : HrEnsureLastError();
}

static BOOL PickerIconAlive(HICON icon)
{
	ICONINFO info = {0};
	BOOL alive = GetIconInfo(icon,&info);
	if(info.hbmColor) DeleteObject(info.hbmColor);
	if(info.hbmMask) DeleteObject(info.hbmMask);
	return alive;
}

static HRESULT PickerResourcesReleased()
{
	HRESULT hr = S_OK;
	GdiFlush();
	REQUIRE(!IsWindow(g_picker.picker));
	for(UINT n = 0; n < g_picker.windowCount; n++)
	{
		REQUIRE(!IsWindow(g_picker.windows[n]));
	}
	for(UINT n = 0; n < ARRAYSIZE(g_picker.icons); n++)
	{
		REQUIRE(g_picker.icons[n] && !PickerIconAlive(g_picker.icons[n]));
	}
Cleanup:
	return hr;
}

static HRESULT ColorPickerResource()
{
	HRESULT hr;
	HMODULE module = GetModuleHandleW(L"QuadooForm.dll");
	HRSRC resource = NULL;
	HGLOBAL loaded;
	PCSTR data;
	DWORD bytes;
	PWSTR write;
	INT length;
	CRString text;
	TStackRef<IJSONValue> parsed, value;
	TStackRef<IJSONObject> definition;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IQuadooControl> root;
	TStackRef<IQuadooForm> form;
	QuadooForm::ContextOptions options = {sizeof(options)};
	HICON icon = NULL;
	UINT count = 0;
	REQUIRE(!GetModuleHandleW(L"QuadooFormDesigner.dll"));
	REQUIRE(module && GetProcAddress(module,"ShowQuadooColorPicker") == GetProcAddress(module,MAKEINTRESOURCEA(16)));
	resource = FindResourceW(module,MAKEINTRESOURCEW(IDR_COLOR_PICKER),RT_RCDATA);
	REQUIRE(resource && FindResourceW(module,MAKEINTRESOURCEW(IDI_COLOR_PICKER),RT_GROUP_ICON));
	loaded = LoadResource(module,resource);
	CheckIf(!loaded,HrEnsureLastError());
	data = static_cast<PCSTR>(LockResource(loaded));
	bytes = SizeofResource(module,resource);
	CheckIf(!data || !bytes,E_UNEXPECTED);
	length = MultiByteToWideChar(CP_UTF8,0,data,bytes,NULL,0);
	CheckIf(!length,HrEnsureLastError());
	Check(RStrAllocW(length,&text,&write));
	CheckIf(MultiByteToWideChar(CP_UTF8,0,data,bytes,write,length) != length,HrEnsureLastError());
	Check(JSONParse(NULL,text.c_wstr(),length,&parsed));
	Check(parsed->GetObject(&definition));
	options.Mode = QuadooForm::Preview; options.DPI = 96;
	Check(QuadooCreateConstructionContext(&options,&context));
	Check(QuadooCreateControl(context,NULL,definition,&root));
	Check(root->QueryInterface(__uuidof(IQuadooForm),reinterpret_cast<PVOID*>(&form)));
	REQUIRE(ImageInteger(root,L"BorderStyle") == QuadooForm::BorderSingle && ImageInteger(root,L"BorderIcons") == 9);
	Check(ImageRead(root,L"Icon",&value));
	Check(QuadooLoadIconValue(value,32,32,&icon));
	REQUIRE(icon != NULL);
	for(UINT n = 0; n < ARRAYSIZE(PickerGroups); n++)
	{
		TStackRef<IQuadooControl> swatch;
		TStackRef<IJSONValue> palette, labels;
		TStackRef<IJSONArray> colors, captions;
		Check(form->FindControl(RSTRING_CAST(PickerGroups[n].name),&swatch));
		REQUIRE(ImageInteger(swatch,L"Mode") == 0);
		Check(ImageRead(swatch,L"Colors",&palette));
		Check(palette->GetArray(&colors));
		Check(ImageRead(swatch,L"Labels",&labels));
		Check(labels->GetArray(&captions));
		REQUIRE(colors->Count() == static_cast<INT>(PickerGroups[n].count) && captions->Count() == colors->Count());
		for(INT c = 0; c < colors->Count(); c++)
		{
			TStackRef<IJSONValue> number;
			CRString label;
			INT raw;
			UINT system = PickerGroups[n].colors[c];
			Check(colors->GetValue(c,&number));
			Check(number->GetInteger(&raw));
			REQUIRE(raw == static_cast<INT>(0x80000000 | system));
			Check(captions->GetString(c,&label));
			REQUIRE(label == RSTRING_CAST(QuadooColors::SystemColorName(system) + 6));
			count++;
		}
	}
	for(UINT n = 0; n < ARRAYSIZE(PickerVistaColors); n++)
	{
		TStackRef<IQuadooControl> swatch;
		TStackRef<IJSONValue> palette, number;
		TStackRef<IJSONArray> colors;
		CRString name;
		QuadooForm::Rect bounds;
		INT raw;
		Check(RStrFormatW(&name,L"Vista%u",n));
		Check(form->FindControl(name,&swatch));
		Check(swatch->GetBounds(&bounds));
		REQUIRE(bounds.Width == 18 && bounds.Height == 18 && bounds.Left == (n%21)*22 && bounds.Top == (n/21)*22);
		Check(ImageRead(swatch,L"Colors",&palette));
		Check(palette->GetArray(&colors));
		REQUIRE(colors->Count() == 1);
		Check(colors->GetValue(0,&number));
		Check(number->GetInteger(&raw));
		REQUIRE(static_cast<COLORREF>(raw) == PickerVistaColors[n]);
	}
	REQUIRE(count == 30);
Cleanup:
	if(root) root->Destroy();
	if(icon) DestroyIcon(icon);
	return hr;
}

#include "ColorDialogExecution.h"

static HRESULT ColorPickerDialogs()
{
	HRESULT hr;
	HWND host = Host();
	COLORREF selected = RGB(90,80,70);
	const COLORREF system = 0x80000000 | COLOR_HIGHLIGHT;
	GuiResourceCounts baseline = {0}, current = {0};
	CheckIf(!host,HrEnsureLastError());
	Check(ColorDialogMetadata());
	Check(ColorPickerResource());
	Check(ColorDialogExecution());
	REQUIRE(ShowQuadooColorPicker(host,0,NULL) == E_POINTER);
	REQUIRE(ShowQuadooColorPicker(reinterpret_cast<HWND>(1),0,&selected) == E_INVALIDARG && selected == RGB(90,80,70));
	for(UINT n = 0; n <= PickerClose; n++)
	{
		PickerTest test = static_cast<PickerTest>(n);
		selected = RGB(90,80,70);
		Check(StartPicker(host,test,system));
		HRESULT shown = ShowQuadooColorPicker(host,system,&selected);
		KillTimer(host,5430);
		if(FAILED(shown)) { CRString text; if(SUCCEEDED(RStrFormatW(&text,L"Picker failed: 0x%.8x\r\n",shown))) Log(text.c_wstr()); }
		Check(g_picker.error);
		REQUIRE(g_picker.seen && IsWindowEnabled(host) && !IsWindow(g_picker.picker));
		BOOL canceled = test == PickerCancel || test == PickerCustomCancel || test == PickerInvalidCancel || test == PickerEscape || test == PickerClose;
		REQUIRE(shown == (canceled ? S_FALSE : S_OK));
		REQUIRE(selected == (canceled ? RGB(90,80,70) : g_picker.expected));
		Check(PickerResourcesReleased());
	}
	DestroyWindow(host); host = NULL;
	for(UINT cycle = 0; cycle < 3 + CGuiResourceMonitor::MeasuredCycles; cycle++)
	{
		host = Host();
		CheckIf(!host,HrEnsureLastError());
		Check(StartPicker(host,PickerKeep,0x80000000 | COLOR_SCROLLBAR));
		Check(ShowQuadooColorPicker(host,0x80000000 | COLOR_SCROLLBAR,&selected));
		KillTimer(host,5430);
		Check(g_picker.error);
		REQUIRE(selected == (0x80000000 | COLOR_SCROLLBAR));
		Check(PickerResourcesReleased());
		DestroyWindow(host); host = NULL;
		Check(SettleGuiResources(NULL,&current));
		if(cycle == 2) baseline = current;
		if(cycle >= 3)
		{
			// Strict GDI accounting also covers windowless fonts/brushes. USER
			// lifetime is checked against the dialog's exact HWND/HICON handles,
			// excluding unrelated asynchronous Windows accessibility caches.
			REQUIRE(current.gdi <= baseline.gdi);
		}
	}
	hr = S_OK;
Cleanup:
	if(host) { KillTimer(host,5430); DestroyWindow(host); }
	return hr;
}
