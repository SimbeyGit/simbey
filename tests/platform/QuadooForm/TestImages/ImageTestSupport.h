#pragma once
#include "Library\Core\MemoryStream.h"
#include "..\..\..\TestPaths.h"
#include "Published\SimbeyCore.h"
#include "ImageFixtures.h"

class CImageTestCanvas
{
	HDC m_dc;
	HBITMAP m_bitmap;
	HGDIOBJ m_old;
	BYTE* m_pixels;
	UINT m_width, m_height;
public:
	CImageTestCanvas() : m_dc(NULL), m_bitmap(NULL), m_old(NULL), m_pixels(NULL), m_width(0), m_height(0) {}
	~CImageTestCanvas()
	{
		if(m_dc) { if(m_old) SelectObject(m_dc,m_old); DeleteDC(m_dc); }
		if(m_bitmap) DeleteObject(m_bitmap);
	}
	HRESULT Create(UINT width, UINT height)
	{
		HRESULT hr = S_OK;
		BITMAPINFO info = {0};
		info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		info.bmiHeader.biWidth = width; info.bmiHeader.biHeight = -static_cast<INT>(height);
		info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
		m_dc = CreateCompatibleDC(NULL);
		CheckIf(!m_dc,HrEnsureLastError());
		m_bitmap = CreateDIBSection(m_dc,&info,DIB_RGB_COLORS,reinterpret_cast<PVOID*>(&m_pixels),NULL,0);
		CheckIf(!m_bitmap,HrEnsureLastError());
		m_old = SelectObject(m_dc,m_bitmap);
		CheckIf(!m_old || m_old == HGDI_ERROR,HrEnsureLastError());
		m_width = width; m_height = height;
Cleanup:
		return hr;
	}
	VOID Fill(COLORREF color)
	{
		GdiFlush();
		for(UINT n = 0; n < m_width * m_height; n++)
		{
			m_pixels[n * 4] = GetBValue(color); m_pixels[n * 4 + 1] = GetGValue(color);
			m_pixels[n * 4 + 2] = GetRValue(color); m_pixels[n * 4 + 3] = 255;
		}
	}
	COLORREF Pixel(UINT x, UINT y)
	{
		GdiFlush();
		const BYTE* pixel = m_pixels + (y * m_width + x) * 4;
		return RGB(pixel[2],pixel[1],pixel[0]);
	}
	DWORD Hash()
	{
		GdiFlush();
		DWORD hash = 2166136261U;
		for(UINT n = 0; n < m_width * m_height * 4; n++) if(n % 4 != 3) hash = (hash ^ m_pixels[n]) * 16777619U;
		return hash;
	}
	HDC DC() const { return m_dc; }
};
static HRESULT ImageFixtureValue(PCWSTR format, const BYTE* bytes, UINT size, IJSONValue** result)
{
	HRESULT hr;
	TStackRef<IJSONObject> object;
	TStackRef<IJSONValue> value;
	CMemoryStream base64;
	CRString text;
	PWSTR write;
	Check(ScEncodeBase64A(bytes,size,&base64,NULL));
	Check(RStrAllocW(base64.DataRemaining(),&text,&write));
	for(UINT n = 0; n < base64.DataRemaining(); n++) write[n] = base64.GetReadPtr()[n];
	Check(JSONCreateObject(&object));
	Check(JSONCreateString(RSTRING_CAST(format),&value));
	Check(object->AddValueW(L"format",value));
	value.Release();
	Check(JSONCreateString(RSTRING_CAST(L"base64"),&value));
	Check(object->AddValueW(L"encoding",value));
	value.Release();
	Check(JSONCreateString(text,&value));
	Check(object->AddValueW(L"data",value));
	Check(JSONWrapObject(object,result));
Cleanup:
	return hr;
}
static HRESULT ImageProperty(IQuadooComponent* control, PCWSTR name, IQuadooProperties** properties, UINT* index)
{
	HRESULT hr;
	TStackRef<IQuadooProperties> values;
	TStackRef<IQuadooPropertyDescriptors> descriptors;
	UINT count;
	Check(control->GetProperties(&values));
	Check(values->GetDescriptors(&descriptors));
	Check(descriptors->GetCount(&count));
	for(UINT n = 0; n < count; n++)
	{
		TStackRef<IQuadooPropertyDescriptor> descriptor;
		CRString text;
		Check(descriptors->GetAt(n,&descriptor));
		Check(descriptor->GetName(&text));
		if(text == RSTRING_CAST(name)) { *index = n; *properties = values.Detach(); hr = S_OK; goto Cleanup; }
	}
	hr = DISP_E_MEMBERNOTFOUND;
Cleanup:
	return hr;
}
static HRESULT ImageSet(IQuadooComponent* control, PCWSTR name, IJSONValue* value)
{
	HRESULT hr;
	TStackRef<IQuadooProperties> values;
	UINT index;
	Check(ImageProperty(control,name,&values,&index));
	Check(values->SetValueJSON(index,value));
Cleanup:
	return hr;
}
static HRESULT ImageNumber(IQuadooComponent* control, PCWSTR name, INT value)
{
	HRESULT hr;
	TStackRef<IJSONValue> json;
	Check(JSONCreateInteger(value,&json));
	Check(ImageSet(control,name,json));
Cleanup:
	return hr;
}
static HRESULT ImageFlag(IQuadooComponent* control, PCWSTR name, BOOL value)
{
	HRESULT hr;
	TStackRef<IJSONValue> json;
	Check(JSONCreateBool(value != FALSE,&json));
	Check(ImageSet(control,name,json));
Cleanup:
	return hr;
}
static HRESULT ImageRead(IQuadooComponent* control, PCWSTR name, IJSONValue** value)
{
	HRESULT hr;
	TStackRef<IQuadooProperties> properties;
	UINT index;
	Check(ImageProperty(control,name,&properties,&index));
	Check(properties->GetValueJSON(index,value));
Cleanup:
	return hr;
}
static INT ImageInteger(IQuadooComponent* control, PCWSTR name)
{
	TStackRef<IJSONValue> value;
	INT result = -999999;
	if(SUCCEEDED(ImageRead(control,name,&value)) && value) value->GetInteger(&result);
	return result;
}
static HRESULT ImageTempFile(const BYTE* bytes, UINT length, PWSTR path)
{
	HRESULT hr = S_OK;
	WCHAR directory[MAX_PATH];
	HANDLE file = INVALID_HANDLE_VALUE;
	DWORD written;
	Check(TestScratchDirectory(L"QuadooFormImages",directory,ARRAYSIZE(directory)));
	CheckIf(!GetTempFileNameW(directory,L"QFI",0,path),HrEnsureLastError());
	file = CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
	CheckIf(file == INVALID_HANDLE_VALUE,HrEnsureLastError());
	CheckIf(!WriteFile(file,bytes,length,&written,NULL) || written != length,HrEnsureLastError());
Cleanup:
	if(file != INVALID_HANDLE_VALUE) CloseHandle(file);
	return hr;
}
