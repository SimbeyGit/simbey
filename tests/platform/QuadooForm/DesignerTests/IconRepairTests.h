#include "..\Harness\IconRepairData.h"

static HRESULT VerifyRepairedIcon(const BYTE* source, UINT size, UINT extra, IJSONValue* value, BOOL legacy = FALSE)
{
	HRESULT hr;
	CMemoryStream repaired;
	ICONHEAD directory;
	HICON icon = NULL;
	TStackRef<IJSONValue> again;
	Check(IconData(value,repaired));
	REQUIRE(repaired.DataRemaining() == size + extra);
	CopyMemory(&directory,source,sizeof(directory));
	UINT end = sizeof(directory) + directory.wCount * sizeof(ICONDIRENTRY);
	for(UINT n = 0; n < directory.wCount; n++)
	{
		ICONDIRENTRY before;
		ICONDIRENTRY after;
		BITMAPINFOHEADER bitmap;
		CopyMemory(&before,source + sizeof(directory) + n * sizeof(before),sizeof(before));
		CopyMemory(&after,repaired.GetReadPtr() + sizeof(directory) + n * sizeof(after),sizeof(after));
		UINT side = before.bWidth ? before.bWidth : 256;
		UINT colorBytes = side * side * 4;
		UINT stride = ((side + 31) / 32) * 4;
		REQUIRE(after.dwImageOffset == end && after.dwBytesInRes == sizeof(bitmap) + colorBytes + stride * side);
		CopyMemory(&bitmap,repaired.GetReadPtr() + after.dwImageOffset,sizeof(bitmap));
		const BYTE* pixels = repaired.GetReadPtr() + after.dwImageOffset + sizeof(bitmap);
		const BYTE* mask = pixels + colorBytes;
		REQUIRE(!memcmp(pixels,source + before.dwImageOffset + sizeof(bitmap),colorBytes));
		if(before.dwBytesInRes == sizeof(bitmap) + colorBytes)
		{
			REQUIRE(bitmap.biSizeImage == 0);
			for(UINT y = 0; y < side; y++)
			{
				for(UINT x = 0; x < side; x++)
				{
					BOOL transparent = !legacy && pixels[(y * side + x) * 4 + 3] == 0;
					REQUIRE(!!(mask[y * stride + x / 8] & (0x80 >> (x % 8))) == transparent);
				}
			}
		}
		else
		{
			REQUIRE(!memcmp(repaired.GetReadPtr() + after.dwImageOffset,source + before.dwImageOffset,before.dwBytesInRes));
		}
		Check(QuadooLoadIconValue(value,side,side,&icon));
		REQUIRE(icon != NULL);
		DestroyIcon(icon);
		icon = NULL;
		end += after.dwBytesInRes;
	}
	REQUIRE(end == repaired.DataRemaining());
	Check(QuadooCreateIconValue(repaired.GetReadPtr(),repaired.DataRemaining(),&again));
	REQUIRE(JSONCompare(value,again) == 0);
Cleanup:
	if(icon)
	{
		DestroyIcon(icon);
	}
	return hr;
}

static HRESULT DrawRepairedIcon(IJSONValue* value, UINT alphaMode)
{
	HRESULT hr;
	HICON icon = NULL;
	HDC dc = NULL;
	HBITMAP bitmap = NULL;
	HGDIOBJ previous = NULL;
	HBRUSH brush = NULL;
	BITMAPINFO info = {0};
	VOID* pixels;
	RECT bounds = {0,0,16,16};
	COLORREF background = RGB(16,32,64);
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = 16;
	info.bmiHeader.biHeight = -16;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	Check(QuadooLoadIconValue(value,16,16,&icon));
	dc = CreateCompatibleDC(NULL);
	CheckIf(!dc,HrEnsureLastError());
	bitmap = CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,NULL,0);
	CheckIf(!bitmap,HrEnsureLastError());
	previous = SelectObject(dc,bitmap);
	brush = CreateSolidBrush(background);
	CheckIf(!brush,HrEnsureLastError());
	CheckIf(!FillRect(dc,&bounds,brush),HrEnsureLastError());
	CheckIf(!DrawIconEx(dc,0,0,icon,16,16,0,NULL,DI_NORMAL),HrEnsureLastError());
	GdiFlush();
	REQUIRE(GetPixel(dc,2,0) == RGB(32,96,144));
	if(alphaMode == 1)
	{
		REQUIRE(GetPixel(dc,0,0) == background);
		COLORREF half = GetPixel(dc,1,0);
		REQUIRE(abs(GetRValue(half) - 24) <= 1 && abs(GetGValue(half) - 64) <= 1 && abs(GetBValue(half) - 104) <= 1);
	}
	else
	{
		REQUIRE(GetPixel(dc,0,0) == RGB(32,96,144));
	}
Cleanup:
	if(previous)
	{
		SelectObject(dc,previous);
	}
	if(bitmap)
	{
		DeleteObject(bitmap);
	}
	if(brush)
	{
		DeleteObject(brush);
	}
	if(dc)
	{
		DeleteDC(dc);
	}
	if(icon)
	{
		DestroyIcon(icon);
	}
	return hr;
}

static HRESULT IcoRepairs()
{
	HRESULT hr;
	CMemoryStream bytes;
	CMemoryStream original;
	TStackRef<IJSONValue> value;
	TStackRef<IJSONValue> raw;
	TStackRef<IJSONValue> stored;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONObject> again;
	TStackRef<QFD::CDesignerEditor> editor;
	HWND host = NULL;
	HICON icon = NULL;
	ULONG written;
	for(UINT alpha = 0; alpha < 3; alpha++)
	{
		for(UINT masks = 0; masks < 3; masks++)
		{
			Check(TestUnmaskedIcon(bytes,alpha,masks));
			original.Reset();
			Check(original.Write(bytes.GetReadPtr(),bytes.DataRemaining(),&written));
			Check(QuadooCreateIconValue(bytes.GetReadPtr(),bytes.DataRemaining(),&value));
			REQUIRE(!memcmp(bytes.GetReadPtr(),original.GetReadPtr(),bytes.DataRemaining()));
			Check(VerifyRepairedIcon(bytes.GetReadPtr(),bytes.DataRemaining(),(masks & 1 ? 0 : 64) + (masks & 2 ? 0 : 128),value,alpha == 0));
			if(!masks)
			{
				Check(DrawRepairedIcon(value,alpha));
			}
			value.Release();
		}
	}
	Check(TestUnmaskedIcon(bytes));
	Check(TestRawIconValue(bytes.GetReadPtr(),bytes.DataRemaining(),&raw));
	Check(QuadooLoadIconValue(raw,32,32,&icon));
	REQUIRE(icon != NULL);
	DestroyIcon(icon);
	icon = NULL;
	Check(QuadooCreateIconValue(bytes.GetReadPtr(),bytes.DataRemaining(),&value));
	host = Host();
	CheckIf(!host,HrEnsureLastError());
	Check(CreateEditor(host,&editor));
	Check(editor->NewDocument(TRUE));
	Check(Edit(editor,editor->Document().Root(),L"Icon",raw));
	Check(QFD::ReadProperty(editor->Document().Root(),L"Icon",&stored));
	REQUIRE(JSONCompare(value,stored) == 0);
	Check(editor->Document().Serialize(&definition));
	Check(editor->LoadDefinition(definition));
	Check(editor->Document().Serialize(&again));
	REQUIRE(JSONCompareObjects(definition,again) == 0);
	// The user's real ICO has four complete alpha bitmaps and no AND masks.
	{
		HMODULE module = GetModuleHandle(NULL);
		HRSRC resource = FindResourceW(module,MAKEINTRESOURCEW(201),RT_RCDATA);
		REQUIRE(resource != NULL);
		const BYTE* source = static_cast<const BYTE*>(LockResource(LoadResource(module,resource)));
		UINT size = SizeofResource(module,resource);
		REQUIRE(source != NULL && size == 30950);
		value.Release();
		Check(QFD::ImportIcon(source,size,&value));
		Check(VerifyRepairedIcon(source,size,1088,value));
	}
Cleanup:
	if(icon)
	{
		DestroyIcon(icon);
	}
	editor.Release();
	if(host)
	{
		DestroyWindow(host);
	}
	return hr;
}

static HRESULT IcoRepairFailures()
{
	HRESULT hr;
	CMemoryStream bytes;
	TStackRef<IJSONValue> rejected;
	for(UINT problem = 0; problem < 7; problem++)
	{
		Check(TestUnmaskedIcon(bytes));
		BYTE* output = const_cast<BYTE*>(bytes.GetReadPtr());
		ICONDIRENTRY entry;
		ICONDIRENTRY other;
		CopyMemory(&entry,output + sizeof(ICONHEAD),sizeof(entry));
		CopyMemory(&other,output + sizeof(ICONHEAD) + sizeof(entry),sizeof(other));
		if(problem == 0)
		{
			entry.dwBytesInRes--; // Truncated color pixels cannot be repaired.
		}
		else if(problem == 1)
		{
			entry.dwImageOffset = other.dwImageOffset; // Overlapping images.
		}
		else if(problem == 2)
		{
			entry.dwImageOffset = bytes.DataRemaining();
		}
		else if(problem == 3)
		{
			BITMAPINFOHEADER bitmap;
			CopyMemory(&bitmap,output + entry.dwImageOffset,sizeof(bitmap));
			bitmap.biWidth++;
			CopyMemory(output + entry.dwImageOffset,&bitmap,sizeof(bitmap));
		}
		else if(problem == 4)
		{
			BITMAPINFOHEADER bitmap;
			CopyMemory(&bitmap,output + entry.dwImageOffset,sizeof(bitmap));
			bitmap.biBitCount = 24;
			entry.wBitCount = 24;
			entry.dwBytesInRes = sizeof(bitmap) + 16 * 16 * 3;
			CopyMemory(output + entry.dwImageOffset,&bitmap,sizeof(bitmap));
		}
		else if(problem == 5)
		{
			BYTE* extra;
			Check(bytes.WriteAdvance(&extra,1));
			*extra = 0;
			output = const_cast<BYTE*>(bytes.GetReadPtr());
			entry.dwBytesInRes++; // A partial mask is not the omitted-mask case.
		}
		else
		{
			BYTE* extra;
			Check(bytes.WriteAdvance(&extra,1024 * 1024 - bytes.DataRemaining()));
			output = const_cast<BYTE*>(bytes.GetReadPtr());
		}
		CopyMemory(output + sizeof(ICONHEAD),&entry,sizeof(entry));
		REQUIRE(FAILED(QuadooCreateIconValue(bytes.GetReadPtr(),bytes.DataRemaining(),&rejected)) && !rejected);
	}
Cleanup:
	return hr;
}
