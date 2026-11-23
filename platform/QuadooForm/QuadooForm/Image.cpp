#include "Runtime.h"

namespace QF
{
CImage::~CImage() { SafeRelease(m_picture); }
HRESULT CImage::CreateNative()
{
	HRESULT hr;
	TStackRef<IJSONValue> value;
	TStackRef<QFI::CImageResource> picture;
	Check(StoredValue(L"Picture",&value));
	Check(QFI::CImageResource::Create(value,&picture));
	Check(CControl::CreateNative());
	m_picture = picture.Detach();
Cleanup:
	return hr;
}
HRESULT CImage::DestroyNative()
{
	SafeRelease(m_picture);
	return CControl::DestroyNative();
}
HRESULT CImage::ApplyNative(PCWSTR name)
{
	HRESULT hr;
	TStackRef<QFI::CImageResource> candidate;
	TStackRef<IJSONValue> value;
	if(!lstrcmpW(name,L"Picture"))
	{
		Check(StoredValue(L"Picture",&value));
		// Decode before changing the live resource. CControl rolls back the JSON
		// slot when this fails, so the property and its old raster stay together.
		Check(QFI::CImageResource::Create(value,&candidate));
		static PCWSTR const names[] = {L"ImageWidth",L"ImageHeight",L"HasAlpha",L"ImageFormat",L"ImageSize"};
		for(UINT n = 0; n < ARRAYSIZE(names); n++)
		{
			Check(Dirty(names[n]));
		}
		SafeRelease(m_picture);
		m_picture = candidate.Detach();
	}
	Check(CControl::ApplyNative(name));
Cleanup:
	return hr;
}
HRESULT CImage::PropertyChanged(PCWSTR name)
{
	HRESULT hr = S_OK;
	if(!lstrcmpW(name,L"Visible") && m_state == QuadooForm::Ready)
	{
		Check(Dispatch(Boolean(L"Visible") ? L"OnShow" : L"OnHide"));
	}
Cleanup:
	return hr;
}
HRESULT CImage::MeasureContent(QuadooForm::Size* size)
{
	if(m_picture) { size->Width = m_picture->Width(); size->Height = m_picture->Height(); return S_OK; }
	return CControl::MeasureContent(size);
}
STDMETHODIMP CImage::Paint(HDC dc, const RECT* bounds)
{
	HRESULT hr = S_OK;
	INT saved = 0;
	Check(CheckAlive());
	CheckIf(!dc || !bounds || bounds->right < bounds->left || bounds->bottom < bounds->top,E_INVALIDARG);
	saved = SaveDC(dc);
	CheckIf(!saved,HrEnsureLastError());
	CheckIf(IntersectClipRect(dc,bounds->left,bounds->top,bounds->right,bounds->bottom) == ERROR,HrEnsureLastError());
	if(!Boolean(L"Transparent"))
	{
		SetDCBrushColor(dc,ResolveQuadooColor(Integer(L"BackColor")));
		CheckIf(!FillRect(dc,bounds,static_cast<HBRUSH>(GetStockObject(DC_BRUSH))),HrEnsureLastError());
	}
	if(m_picture)
	{
		Check(m_picture->Paint(dc,bounds,m_dpi,Integer(L"SizeMode"),Integer(L"ImageAlignment"),Integer(L"Interpolation"),static_cast<BYTE>(Integer(L"Opacity")),Integer(L"TileOffsetX"),Integer(L"TileOffsetY")));
	}
Cleanup:
	if(saved) RestoreDC(dc,saved);
	return hr;
}
STDMETHODIMP CImage::HitTest(INT x, INT y, BOOL* hit)
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
STDMETHODIMP CImage::HandleMessage(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result)
{
	HRESULT hr = S_OK;
	TStackRef<CControl> keepAlive(this);
	UNREFERENCED_PARAMETER(window); UNREFERENCED_PARAMETER(wp); UNREFERENCED_PARAMETER(lp);
	CheckIf(!handled || !result,E_POINTER);
	*handled = FALSE; *result = 0;
	Check(CheckAlive());
	if(message == WM_LBUTTONUP && m_state == QuadooForm::Ready)
	{
		*handled = TRUE;
		Check(Dispatch(L"OnClick"));
	}
Cleanup:
	return hr;
}
HRESULT CImage::Diagnostic(UINT index, IJSONValue** value)
{
	HRESULT hr = S_FALSE;
	CheckIf(!value,E_POINTER);
	*value = NULL;
	Check(CheckThread());
	hr = S_FALSE;
	if(index >= static_cast<UINT>(m_values.Length())) goto Cleanup;
	PCWSTR name = m_values[index].spec->name;
	if(!lstrcmpW(name,L"ImageWidth")) hr = JSONCreateInteger(m_picture ? m_picture->Width() : 0,value);
	else if(!lstrcmpW(name,L"ImageHeight")) hr = JSONCreateInteger(m_picture ? m_picture->Height() : 0,value);
	else if(!lstrcmpW(name,L"ImageSize")) hr = JSONCreateInteger(m_picture ? m_picture->ByteSize() : 0,value);
	else if(!lstrcmpW(name,L"HasAlpha")) hr = JSONCreateBool(m_picture && m_picture->HasAlpha(),value);
	else if(!lstrcmpW(name,L"ImageFormat")) hr = JSONCreateString(RSTRING_CAST(!m_picture ? L"None" : m_picture->IsIcon() ? L"ICO" : L"PNG"),value);
Cleanup:
	return hr;
}
STDMETHODIMP CImage::GetValueJSON(UINT index, IJSONValue** value)
{
	HRESULT hr = Diagnostic(index,value);
	if(hr == S_FALSE) hr = CControl::GetValueJSON(index,value);
	return hr;
}
STDMETHODIMP CImage::GetValueInt(UINT index, INT* value)
{
	HRESULT hr;
	TStackRef<IJSONValue> json;
	CheckIf(!value,E_POINTER);
	*value = 0;
	hr = Diagnostic(index,&json);
	if(hr == S_FALSE) hr = CControl::GetValueInt(index,value);
	else if(SUCCEEDED(hr)) hr = json->GetInteger(value);
Cleanup:
	return hr;
}
STDMETHODIMP CImage::GetValueBool(UINT index, BOOL* value)
{
	HRESULT hr;
	TStackRef<IJSONValue> json;
	bool flag;
	CheckIf(!value,E_POINTER);
	*value = FALSE;
	hr = Diagnostic(index,&json);
	if(hr == S_FALSE) hr = CControl::GetValueBool(index,value);
	else if(SUCCEEDED(hr)) { hr = json->GetBoolean(&flag); if(SUCCEEDED(hr)) *value = flag; }
Cleanup:
	return hr;
}
STDMETHODIMP CImage::GetValueString(UINT index, RSTRING* value)
{
	HRESULT hr;
	TStackRef<IJSONValue> json;
	CheckIf(!value,E_POINTER);
	*value = NULL;
	hr = Diagnostic(index,&json);
	if(hr == S_FALSE) hr = CControl::GetValueString(index,value);
	else if(SUCCEEDED(hr)) hr = json->GetString(value);
Cleanup:
	return hr;
}
}
