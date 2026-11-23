#include "Runtime.h"
#include <commdlg.h>

namespace QF
{
// These limits bound painting and JSON edits, including edits made by extensions.
static const INT MaximumSwatchColors = 256;

// Called only on private storage or a proposed deep copy. Keep the stored
// palette canonical even when JSON's parser widens the INT_MIN system color.
static HRESULT NormalizePalette(IJSONValue* value)
{
	HRESULT hr;
	TStackRef<IJSONArray> colors;
	CheckIf(!value,E_INVALIDARG);
	Check(value->GetArray(&colors));
	CheckIf(colors->Count() < 1 || colors->Count() > MaximumSwatchColors,E_INVALIDARG);
	for(sysint n = 0; n < colors->Count(); n++)
	{
		TStackRef<IJSONValue> color, number;
		Check(colors->GetValue(n,&color));
		CheckIf(!color,E_INVALIDARG);
		if(color->GetType() == JSON::LongInteger)
		{
			LONGLONG raw;
			Check(color->GetLongInteger(&raw));
			CheckIf(raw < INT_MIN || raw > INT_MAX,E_INVALIDARG);
			Check(JSONCreateInteger(static_cast<INT>(raw),&number));
			Check(colors->Replace(n,number));
		}
	}
Cleanup:
	return hr;
}

HRESULT CColorSwatch::Validate(IJSONValue* colors, IJSONValue* labels, INT mode, INT selected)
{
	HRESULT hr;
	TStackRef<IJSONArray> palette, captions;
	CheckIf(!colors || !labels,E_INVALIDARG);
	Check(colors->GetArray(&palette));
	Check(labels->GetArray(&captions));
	CheckIf(palette->Count() < 1 || palette->Count() > MaximumSwatchColors,E_INVALIDARG);
	CheckIf(mode < 0 || mode > 1 || (mode == 1 && palette->Count() != 1),E_INVALIDARG);
	CheckIf(selected < -1 || selected >= palette->Count() || captions->Count() > palette->Count(),E_INVALIDARG);
	for(sysint n = 0; n < palette->Count(); n++)
	{
		TStackRef<IJSONValue> value;
		INT color;
		Check(palette->GetValue(n,&value));
		CheckIf(!value,E_INVALIDARG);
		Check(value->GetInteger(&color));
		DWORD tag = static_cast<DWORD>(color) & 0xff000000;
		CheckIf(tag != 0 && tag != 0x80000000,E_INVALIDARG);
	}
	for(sysint n = 0; n < captions->Count(); n++)
	{
		CRString text;
		Check(captions->GetString(n,&text));
		CheckIf(!RStrIsWide(static_cast<RSTRING>(text)) || RStrLen(text) > 4096,E_INVALIDARG);
		for(INT c = 0; c < RStrLen(text); c++)
		{
			CheckIf(!text.c_wstr()[c] || text.c_wstr()[c] == L'\r' || text.c_wstr()[c] == L'\n',E_INVALIDARG);
		}
	}
Cleanup:
	return hr;
}

HRESULT CColorSwatch::CreateNative()
{
	HRESULT hr;
	TStackRef<IJSONValue> colors, labels;
	Check(StoredValue(L"Colors",&colors));
	Check(StoredValue(L"Labels",&labels));
	Check(NormalizePalette(colors));
	Check(Validate(colors,labels,Integer(L"Mode"),Integer(L"Selected")));
	Check(CControl::CreateNative());
Cleanup:
	return hr;
}

HRESULT CColorSwatch::SetValue(UINT index, QuadooProperty::Type type, IJSONValue* value)
{
	HRESULT hr;
	static PCWSTR const names[] = {L"Colors",L"Labels",L"Mode",L"Selected"};
	TStackRef<IJSONValue> next[4];
	TStackRef<IJSONArray> palette, captions, trimmed;
	TStackRef<CControl> keepAlive(this);
	UINT slots[4], edited = ARRAYSIZE(names);
	BOOL changed[4], any = FALSE;
	INT mode, selected;
	Check(CheckAlive());
	CheckIf(index >= static_cast<UINT>(m_values.Length()),DISP_E_MEMBERNOTFOUND);
	for(UINT n = 0; n < ARRAYSIZE(names); n++)
	{
		if(!lstrcmpW(m_values[index].spec->name,names[n])) edited = n;
	}
	if(edited == ARRAYSIZE(names))
	{
		Check(CControl::SetValue(index,type,value));
		goto Cleanup;
	}
	CheckIf(type != m_values[index].spec->type && !(edited == 2 && type == QuadooProperty::Int),DISP_E_TYPEMISMATCH);
	for(UINT n = 0; n < ARRAYSIZE(names); n++)
	{
		slots[n] = FindProperty(names[n]);
		CheckIf(slots[n] >= static_cast<UINT>(m_values.Length()),DISP_E_MEMBERNOTFOUND);
		if(n == edited)
		{
			Check(NormalizeValue(m_values[index].spec,value,&next[n]));
		}
		else { next[n] = m_values[slots[n]].value; }
	}
	if(edited == 0)
	{
		Check(NormalizePalette(next[0]));
	}
	Check(next[0]->GetArray(&palette));
	Check(next[1]->GetArray(&captions));
	Check(next[2]->GetInteger(&mode));
	Check(next[3]->GetInteger(&selected));
	if(edited == 0)
	{
		// Prepare all dependent values before committing any of the slots. Observers
		// see a complete state, and a failed validation leaves every slot unchanged.
		if(selected >= palette->Count())
		{
			selected = -1;
			next[3].Release();
			Check(JSONCreateInteger(selected,&next[3]));
		}
		if(captions->Count() > palette->Count())
		{
			Check(JSONCreateArray(&trimmed));
			for(sysint n = 0; n < palette->Count(); n++)
			{
				TStackRef<IJSONValue> text;
				Check(captions->GetValue(n,&text));
				Check(trimmed->Add(text));
			}
			next[1].Release();
			Check(JSONWrapArray(trimmed,&next[1]));
		}
	}
	Check(Validate(next[0],next[1],mode,selected));
	for(UINT n = 0; n < ARRAYSIZE(names); n++)
	{
		changed[n] = JSONCompare(m_values[slots[n]].value,next[n]) != 0;
		any |= changed[n];
	}
	CheckIfIgnore(!any,S_FALSE);
	for(UINT n = 0; n < ARRAYSIZE(names); n++)
	{
		if(changed[n])
		{
			SafeRelease(m_values[slots[n]].value);
			m_values[slots[n]].value = next[n].Detach();
			m_values[slots[n]].dirty = TRUE;
		}
	}
	m_revision++;
	Check(CControl::ApplyNative(names[edited]));
	Check(FlushNotifications());
Cleanup:
	return hr;
}

HRESULT CColorSwatch::PropertyChanged(PCWSTR name)
{
	HRESULT hr = S_OK;
	if(!lstrcmpW(name,L"Visible") && m_state == QuadooForm::Ready)
	{
		Check(Dispatch(Boolean(L"Visible") ? L"OnShow" : L"OnHide"));
	}
Cleanup:
	return hr;
}

static COLORREF SwatchBorder(COLORREF color)
{
	INT red = GetRValue(color), green = GetGValue(color), blue = GetBValue(color);
	if(!red && !green && !blue) return RGB(48,48,48);
	return RGB(red ? red - max(1,red / 5) : 0,green ? green - max(1,green / 5) : 0,blue ? blue - max(1,blue / 5) : 0);
}

STDMETHODIMP CColorSwatch::Paint(HDC dc, const RECT* bounds)
{
	HRESULT hr;
	TStackRef<IJSONValue> colors, labels;
	TStackRef<IJSONArray> palette, captions;
	INT saved = 0, bodySaved = 0;
	HRGN rounded = NULL;
	RECT body;
	TEXTMETRICW metrics;
	INT width, height, labelHeight = 0, padding, radius, selected;
	Check(CheckAlive());
	CheckIf(!dc || !bounds || bounds->right < bounds->left || bounds->bottom < bounds->top,E_INVALIDARG);
	CheckIf(static_cast<LONGLONG>(bounds->right) - bounds->left > INT_MAX || static_cast<LONGLONG>(bounds->bottom) - bounds->top > INT_MAX || bounds->right == INT_MAX || bounds->bottom == INT_MAX,E_INVALIDARG);
	selected = Integer(L"Selected");
	width = bounds->right - bounds->left;
	height = bounds->bottom - bounds->top;
	CheckIfIgnore(!width || !height,S_OK);
	Check(StoredValue(L"Colors",&colors));
	Check(StoredValue(L"Labels",&labels));
	Check(colors->GetArray(&palette));
	Check(labels->GetArray(&captions));
	CheckIf(palette->Count() < 1,E_UNEXPECTED);
	saved = SaveDC(dc);
	CheckIf(!saved,HrEnsureLastError());
	CheckIf(IntersectClipRect(dc,bounds->left,bounds->top,bounds->right,bounds->bottom) == ERROR,HrEnsureLastError());
	if(m_font) SelectObject(dc,m_font);
	if(captions->Count())
	{
		CheckIf(!GetTextMetricsW(dc,&metrics),HrEnsureLastError());
		labelHeight = min(height,metrics.tmHeight + max(1,MulDiv(3,m_dpi,96)));
	}
	padding = min(max(1,MulDiv(3,m_dpi,96)),max(0,(height - labelHeight - 1) / 2));
	body = *bounds;
	body.top += padding;
	body.bottom -= labelHeight + padding;
	radius = max(2,MulDiv(8,m_dpi,96));
	if(body.bottom > body.top)
	{
		bodySaved = SaveDC(dc);
		CheckIf(!bodySaved,HrEnsureLastError());
		rounded = CreateRoundRectRgn(body.left,body.top,body.right + 1,body.bottom + 1,radius,radius);
		CheckIf(!rounded,HrEnsureLastError());
		CheckIf(ExtSelectClipRgn(dc,rounded,RGN_AND) == ERROR,HrEnsureLastError());
		for(sysint n = 0; n < palette->Count(); n++)
		{
			INT color;
			TStackRef<IJSONValue> value;
			RECT cell = body;
			cell.left += static_cast<INT>(static_cast<LONGLONG>(n) * width / palette->Count());
			cell.right = body.left + static_cast<INT>(static_cast<LONGLONG>(n + 1) * width / palette->Count());
			Check(palette->GetValue(n,&value));
			Check(value->GetInteger(&color));
			SetDCBrushColor(dc,ResolveQuadooColor(color));
			if(cell.right > cell.left) FillRect(dc,&cell,static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
		}
		RestoreDC(dc,bodySaved); bodySaved = 0;
		if(selected >= 0 && selected < palette->Count())
		{
			INT color;
			TStackRef<IJSONValue> value;
			RECT cell = body;
			cell.left += static_cast<INT>(static_cast<LONGLONG>(selected) * width / palette->Count());
			cell.right = body.left + static_cast<INT>(static_cast<LONGLONG>(selected + 1) * width / palette->Count());
			if(cell.right > cell.left)
			{
				cell.top -= padding;
				cell.bottom += padding;
				Check(palette->GetValue(selected,&value));
				Check(value->GetInteger(&color));
				COLORREF rgb = ResolveQuadooColor(color);
				SelectObject(dc,GetStockObject(DC_BRUSH));
				SelectObject(dc,GetStockObject(DC_PEN));
				SetDCBrushColor(dc,rgb);
				SetDCPenColor(dc,SwatchBorder(rgb));
				RoundRect(dc,cell.left,cell.top,cell.right,cell.bottom,radius,radius);
			}
		}
	}
	SetTextColor(dc,ResolveQuadooColor(Integer(L"ForeColor")));
	SetBkMode(dc,TRANSPARENT);
	for(sysint n = 0; n < captions->Count(); n++)
	{
		CRString text;
		RECT cell = *bounds;
		cell.left += static_cast<INT>(static_cast<LONGLONG>(n) * width / palette->Count());
		cell.right = bounds->left + static_cast<INT>(static_cast<LONGLONG>(n + 1) * width / palette->Count());
		cell.top = bounds->bottom - labelHeight;
		Check(captions->GetString(n,&text));
		DrawTextW(dc,text.c_wstr(),RStrLen(text),&cell,DT_CENTER | DT_BOTTOM | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
	}
	hr = S_OK;
Cleanup:
	if(bodySaved) RestoreDC(dc,bodySaved);
	if(rounded) DeleteObject(rounded);
	if(saved) RestoreDC(dc,saved);
	return hr;
}

STDMETHODIMP CColorSwatch::HitTest(INT x, INT y, BOOL* hit)
{
	HRESULT hr;
	CheckIf(!hit,E_POINTER);
	*hit = FALSE;
	Check(CheckAlive());
	*hit = Boolean(L"Visible") && Boolean(L"Enabled") && x >= m_effective.Left && y >= m_effective.Top &&
		static_cast<LONGLONG>(x) < static_cast<LONGLONG>(m_effective.Left) + m_effective.Width && static_cast<LONGLONG>(y) < static_cast<LONGLONG>(m_effective.Top) + m_effective.Height;
Cleanup:
	return hr;
}

STDMETHODIMP CColorSwatch::HandleMessage(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result)
{
	HRESULT hr = S_OK;
	TStackRef<CControl> keepAlive(this);
	TStackRef<IJSONValue> colors, replacement;
	TStackRef<IJSONArray> palette, edited;
	QuadooForm::ContextMode mode;
	BOOL hit;
	UNREFERENCED_PARAMETER(wp);
	CheckIf(!handled || !result,E_POINTER);
	*handled = FALSE; *result = 0;
	Check(CheckAlive());
	if(message != WM_LBUTTONUP || m_state != QuadooForm::Ready) goto Cleanup;
	Check(m_context->GetMode(&mode));
	CheckIfIgnore(mode == QuadooForm::Design || m_choosing,S_FALSE);
	Check(HitTest(MulDiv(static_cast<short>(LOWORD(lp)),96,m_dpi),MulDiv(static_cast<short>(HIWORD(lp)),96,m_dpi),&hit));
	if(!hit) goto Cleanup;
	*handled = TRUE;
	Check(StoredValue(L"Colors",&colors));
	Check(colors->GetArray(&palette));
	if(Integer(L"Mode") == 1)
	{
		CHOOSECOLORW choose;
		COLORREF custom[16];
		INT color;
		TStackRef<IJSONValue> first;
		CheckIf(!IsWindow(window),E_INVALIDARG);
		Check(palette->GetValue(0,&first));
		Check(first->GetInteger(&color));
		ZeroMemory(&choose,sizeof(choose));
		ZeroMemory(custom,sizeof(custom));
		choose.lStructSize = sizeof(choose);
		choose.hwndOwner = GetAncestor(window,GA_ROOT);
		choose.rgbResult = ResolveQuadooColor(color);
		choose.lpCustColors = custom;
		choose.Flags = CC_RGBINIT | CC_FULLOPEN;
		ULONG revision = m_revision;
		m_choosing = TRUE;
		BOOL accepted = ChooseColorW(&choose);
		m_choosing = FALSE;
		if(!accepted)
		{
			DWORD error = CommDlgExtendedError();
			hr = error ? E_FAIL : S_FALSE;
			goto Cleanup;
		}
		// Timers and host callbacks may edit or destroy the control in the modal
		// loop. Never overwrite a replacement palette or dispatch on a dead object.
		CheckIfIgnore(m_state != QuadooForm::Ready || revision != m_revision,S_FALSE);
		Check(JSONCreateArray(&edited));
		Check(JSONCreateInteger(choose.rgbResult,&replacement));
		Check(edited->Add(replacement));
		replacement.Release();
		Check(JSONWrapArray(edited,&replacement));
		Check(SetNamedValue(L"Colors",QuadooProperty::Collection,replacement));
	}
	else
	{
		INT left = MulDiv(m_effective.Left,m_dpi,96);
		INT width = MulDiv(m_effective.Left + m_effective.Width,m_dpi,96) - left;
		INT offset = static_cast<short>(LOWORD(lp)) - left;
		CheckIfIgnore(width <= 0 || offset < 0 || offset >= width,S_FALSE);
		INT selected = static_cast<INT>((static_cast<LONGLONG>(offset + 1) * palette->Count() - 1) / width);
		Check(JSONCreateInteger(selected,&replacement));
		Check(SetNamedValue(L"Selected",QuadooProperty::Int,replacement));
	}
	if(m_state == QuadooForm::Ready)
	{
		Check(Dispatch(L"OnClick"));
	}
Cleanup:
	return hr;
}
}
