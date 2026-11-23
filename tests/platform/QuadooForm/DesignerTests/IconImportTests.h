#include "Library\IconTypes.h"
#include "PngFixtures.h"

static HRESULT IconData(IJSONValue* value, CMemoryStream& ico)
{
	HRESULT hr;
	TStackRef<IJSONObject> object; TStackRef<IJSONValue> part; CRString format, encoding, data; CMemoryStream ascii; BYTE* write;
	Check(value->GetObject(&object));
	Check(object->FindNonNullValueW(L"format",&part));
	Check(part->GetString(&format));
	part.Release();
	Check(object->FindNonNullValueW(L"encoding",&part));
	Check(part->GetString(&encoding));
	part.Release();
	REQUIRE(format == RSTRING_CAST(L"ico") && encoding == RSTRING_CAST(L"base64"));
	Check(object->FindNonNullValueW(L"data",&part));
	Check(part->GetString(&data));
	Check(ascii.WriteAdvance(&write,RStrLen(data)));
	for(INT n = 0; n < RStrLen(data); n++) write[n] = static_cast<BYTE>(data.c_wstr()[n]);
	Check(ScDecodeBase64A(reinterpret_cast<PCSTR>(write),RStrLen(data),&ico,NULL));
Cleanup:
	return hr;
}
static HRESULT IconFrame(CMemoryStream& ico, UINT side, const BYTE** color, const BYTE** mask)
{
	HRESULT hr = E_FAIL; ICONHEAD header;
	CheckIf(ico.DataRemaining() < sizeof(header),E_FAIL);
	CopyMemory(&header,ico.GetReadPtr(),sizeof(header));
	REQUIRE(header.wType == 1 && header.wReserved == 0 && header.wCount > 0 && header.wCount <= 4);
	for(UINT n = 0; n < header.wCount; n++)
	{
		ICONDIRENTRY entry; BITMAPINFOHEADER bitmap;
		CheckIf(sizeof(header) + (n + 1) * sizeof(entry) > ico.DataRemaining(),E_FAIL);
		CopyMemory(&entry,ico.GetReadPtr() + sizeof(header) + n * sizeof(entry),sizeof(entry));
		if((entry.bWidth ? entry.bWidth : 256) != side) continue;
		CheckIf(entry.dwImageOffset > ico.DataRemaining() || entry.dwBytesInRes > ico.DataRemaining() - entry.dwImageOffset || entry.dwBytesInRes < sizeof(bitmap),E_FAIL);
		CopyMemory(&bitmap,ico.GetReadPtr() + entry.dwImageOffset,sizeof(bitmap));
		REQUIRE(bitmap.biSize == sizeof(bitmap) && bitmap.biWidth == side && bitmap.biHeight == side * 2 && bitmap.biBitCount == 32 && bitmap.biCompression == BI_RGB);
		*color = ico.GetReadPtr() + entry.dwImageOffset + sizeof(bitmap); *mask = *color + side * side * 4;
		hr = S_OK; break;
	}
Cleanup:
	return hr;
}
static const BYTE* IconPixel(const BYTE* color, UINT side, UINT x, UINT y) { return color + ((side - 1 - y) * side + x) * 4; }
static BOOL IconMask(const BYTE* mask, UINT side, UINT x, UINT y) { return (mask[(side - 1 - y) * ((side + 31) / 32) * 4 + x / 8] & (0x80 >> (x % 8))) != 0; }
static HRESULT DrawImportedIcon(IJSONValue* value, UINT side, BOOL transparent)
{
	HRESULT hr;
	HICON icon = NULL; HDC dc = NULL; HBITMAP bitmap = NULL; HGDIOBJ previous = NULL; HBRUSH brush = NULL;
	BITMAPINFO info; VOID* pixels; RECT bounds = { 0,0,static_cast<LONG>(side),static_cast<LONG>(side) }; COLORREF background = RGB(16,32,64), pixel;
	ZeroMemory(&info,sizeof(info)); info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth = side; info.bmiHeader.biHeight = -static_cast<INT>(side); info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
	Check(QuadooLoadIconValue(value,side,side,&icon));
	dc = CreateCompatibleDC(NULL);
	CheckIf(!dc,HrEnsureLastError());
	bitmap = CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,NULL,0);
	CheckIf(!bitmap,HrEnsureLastError());
	previous = SelectObject(dc,bitmap); brush = CreateSolidBrush(background);
	CheckIf(!brush,HrEnsureLastError());
	CheckIf(!FillRect(dc,&bounds,brush),HrEnsureLastError());
	CheckIf(!DrawIconEx(dc,0,0,icon,side,side,0,NULL,DI_NORMAL),HrEnsureLastError());
	GdiFlush();
	if(transparent)
	{
		for(UINT y = 0; y < side; y++) for(UINT x = 0; x < side; x++)
		{
			REQUIRE(GetPixel(dc,x,y) == background);
		}
	}
	else
	{
		REQUIRE(GetPixel(dc,0,0) == RGB(255,0,0) && GetPixel(dc,0,1) == RGB(0,0,255));
		REQUIRE(GetPixel(dc,2,0) == background && GetPixel(dc,1,2) == background);
		pixel = GetPixel(dc,1,0);
		REQUIRE(abs(GetRValue(pixel) - 8) <= 1 && abs(GetGValue(pixel) - 144) <= 1 && abs(GetBValue(pixel) - 32) <= 1);
	}
Cleanup:
	if(previous) SelectObject(dc,previous); if(bitmap) DeleteObject(bitmap); if(brush) DeleteObject(brush); if(dc) DeleteDC(dc); if(icon) DestroyIcon(icon);
	return hr;
}
static HRESULT PngFormats()
{
	HRESULT hr;
	TStackRef<IJSONValue> value, candidate; CMemoryStream ico; const BYTE* color; const BYTE* mask; const BYTE* pixel;
	Check(QFD::ImportIcon(PngRGBA,sizeof(PngRGBA),&value));
	Check(IconData(value,ico));
	Check(IconFrame(ico,3,&color,&mask));
	pixel = IconPixel(color,3,0,0);
	REQUIRE(pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 255 && pixel[3] == 255 && !IconMask(mask,3,0,0));
	pixel = IconPixel(color,3,1,0);
	REQUIRE(pixel[0] == 0 && pixel[1] == 255 && pixel[2] == 0 && pixel[3] == 128 && !IconMask(mask,3,1,0));
	pixel = IconPixel(color,3,2,0);
	REQUIRE(pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 0 && pixel[3] == 0 && IconMask(mask,3,2,0));
	REQUIRE(IconMask(mask,3,0,2) && IconMask(mask,3,1,2) && IconMask(mask,3,2,2));
	Check(DrawImportedIcon(value,3,FALSE));
	value.Release(); ico.Reset();
	Check(QFD::ImportIcon(PngPalette,sizeof(PngPalette),&value));
	Check(IconData(value,ico));
	Check(IconFrame(ico,2,&color,&mask));
	REQUIRE(IconPixel(color,2,0,0)[2] == 255 && IconPixel(color,2,1,1)[2] == 255 && IconMask(mask,2,1,0) && IconMask(mask,2,0,1)); value.Release(); ico.Reset();
	Check(QFD::ImportIcon(PngGray,sizeof(PngGray),&value));
	Check(IconData(value,ico));
	Check(IconFrame(ico,3,&color,&mask));
	pixel = IconPixel(color,3,1,1);
	REQUIRE(pixel[0] == 127 && pixel[1] == 127 && pixel[2] == 127 && pixel[3] == 255 && IconMask(mask,3,0,0)); value.Release(); ico.Reset();
	Check(QFD::ImportIcon(PngGrayAlpha16,sizeof(PngGrayAlpha16),&value));
	Check(IconData(value,ico));
	Check(IconFrame(ico,2,&color,&mask));
	pixel = IconPixel(color,2,0,0);
	REQUIRE(pixel[0] == 255 && pixel[1] == 255 && pixel[2] == 255 && pixel[3] == 128); value.Release(); ico.Reset();
	Check(QFD::ImportIcon(PngRGB16,sizeof(PngRGB16),&value));
	Check(IconData(value,ico));
	Check(IconFrame(ico,1,&color,&mask));
	REQUIRE(color[0] == 0 && color[1] == 128 && color[2] == 255 && color[3] == 255); value.Release(); ico.Reset();
	Check(QFD::ImportIcon(PngAdam7,sizeof(PngAdam7),&value));
	Check(IconData(value,ico));
	Check(IconFrame(ico,5,&color,&mask));
	for(UINT y = 0; y < 3; y++) for(UINT x = 0; x < 5; x++)
	{
		pixel = IconPixel(color,5,x,y + 1);
		UINT alpha = x == 4 ? 0 : (x + y) % 2 ? 128 : 255;
		REQUIRE(pixel[3] == alpha && (alpha ? pixel[0] == 255 - x * 20 && pixel[1] == y * 80 && pixel[2] == x * 40 : pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 0));
	}
	value.Release(); ico.Reset();
	Check(QFD::ImportIcon(PngLarge,sizeof(PngLarge),&value));
	Check(IconData(value,ico));
	for(UINT side = 16; side <= 256; side = side == 16 ? 32 : side == 32 ? 48 : 256)
	{
		Check(IconFrame(ico,side,&color,&mask));
		pixel = IconPixel(color,side,0,side / 4);
		REQUIRE(pixel[2] == 255 && pixel[3] == 255 && IconMask(mask,side,0,0) && IconMask(mask,side,side - 1,side / 2));
		if(side == 256) break;
	}
	value.Release(); ico.Reset();
	Check(QFD::ImportIcon(PngAlphaEdge,sizeof(PngAlphaEdge),&value));
	Check(IconData(value,ico));
	Check(IconFrame(ico,256,&color,&mask));
	pixel = IconPixel(color,256,0,127);
	REQUIRE(pixel[0] == 0 && pixel[2] == 255 && pixel[3] == 218); value.Release(); ico.Reset();
	Check(QFD::ImportIcon(PngTransparent,sizeof(PngTransparent),&value));
	Check(DrawImportedIcon(value,2,TRUE));
	value.Release();
	Check(TestIconValue(&value));
	Check(IconData(value,ico));
	Check(QFD::ImportIcon(ico.GetReadPtr(),ico.DataRemaining(),&candidate));
	REQUIRE(JSONCompare(value,candidate) == 0);
Cleanup:
	return hr;
}
static HRESULT PngFailuresAndPersistence()
{
	HRESULT hr;
	HWND host = Host(); TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IJSONValue> value, loaded, candidate; TStackRef<IJSONObject> definition, after;
	WCHAR path[MAX_PATH] = { 0 }; HANDLE file = INVALID_HANDLE_VALUE; DWORD written; HICON icon = NULL;
	BYTE bad[sizeof(PngRGBA)]; CopyMemory(bad,PngRGBA,sizeof(bad)); bad[sizeof(bad) - 1] ^= 1;
	REQUIRE(FAILED(QFD::ImportIcon(bad,sizeof(bad),&candidate)) && !candidate);
	REQUIRE(FAILED(QFD::ImportIcon(PngRGBA,sizeof(PngRGBA) - 2,&candidate)) && !candidate);
	for(UINT size = 8; size < 33; size++)
	{
		REQUIRE(FAILED(QFD::ImportIcon(PngRGBA,size,&candidate)) && !candidate);
	}
	REQUIRE(FAILED(QFD::ImportIcon(PngShortRows,sizeof(PngShortRows),&candidate)) && !candidate);
	REQUIRE(FAILED(QFD::ImportIcon(PngLongRows,sizeof(PngLongRows),&candidate)) && !candidate);
	REQUIRE(QFD::ImportIcon(PngTooWide,sizeof(PngTooWide),&candidate) == HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED) && !candidate);
	REQUIRE(QFD::ImportIcon(PngTooManyPixels,sizeof(PngTooManyPixels),&candidate) == HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED) && !candidate);
	REQUIRE(QFD::ImportIcon(PngRGBA,8 * 1024 * 1024 + 1,&candidate) == HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE) && !candidate);
	REQUIRE(QFD::ImportIcon(NULL,0,&candidate) == E_INVALIDARG && !candidate);
	REQUIRE(QFD::ImportIcon(PngRGBA,sizeof(PngRGBA),NULL) == E_POINTER);
	CheckIf(!host,HrEnsureLastError());
	Check(CreateEditor(host,&editor));
	Check(editor->NewDocument(TRUE));
	Check(NewTemporary(path,ARRAYSIZE(path)));
	file = CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
	CheckIf(file == INVALID_HANDLE_VALUE,HrEnsureLastError());
	CheckIf(!WriteFile(file,PngRGBA,sizeof(PngRGBA),&written,NULL) || written != sizeof(PngRGBA),HrEnsureLastError());
	CloseHandle(file); file = INVALID_HANDLE_VALUE;
	Check(QFD::LoadIconFile(path,&value));
	Check(Edit(editor,editor->Document().Root(),L"Icon",value));
	Check(editor->Document().Serialize(&definition));
	CheckIf(!DeleteFileW(path),HrEnsureLastError());
	REQUIRE(FAILED(QFD::LoadIconFile(path,&candidate)) && !candidate);
	Check(editor->LoadDefinition(definition));
	Check(QFD::ReadProperty(editor->Document().Root(),L"Icon",&loaded));
	REQUIRE(JSONCompare(value,loaded) == 0 && !editor->Document().Dirty());
	Check(QuadooLoadIconValue(loaded,32,32,&icon));
	REQUIRE(icon != NULL);
	REQUIRE(FAILED(QFD::ImportIcon(PngShortRows,sizeof(PngShortRows),&candidate)) && !candidate);
	Check(editor->Document().Serialize(&after));
	REQUIRE(JSONCompareObjects(definition,after) == 0 && !editor->Document().Dirty());
Cleanup:
	if(file != INVALID_HANDLE_VALUE) CloseHandle(file); if(path[0]) DeleteFileW(path); if(icon) DestroyIcon(icon);
	if(host) DestroyWindow(host); editor.Release();
	return hr;
}
