#pragma once
#include "Library\IconTypes.h"

// Deliberately store image payloads in a different order from directory entries.
// alphaMode: 0 = legacy BGRX, 1 = mixed alpha, 2 = entirely opaque alpha.
static HRESULT TestUnmaskedIcon(CMemoryStream& bytes, UINT alphaMode = 1, UINT masks = 0)
{
	HRESULT hr;
	CMemoryStream result;
	BYTE* output;
	ICONHEAD header = {0,1,2};
	UINT length = sizeof(ICONHEAD) + 2 * sizeof(ICONDIRENTRY);
	for(UINT n = 0; n < 2; n++)
	{
		UINT side = n ? 32 : 16;
		length += sizeof(BITMAPINFOHEADER) + side * side * 4;
		if(masks & (1 << n))
		{
			length += side * 4;
		}
	}
	Check(result.WriteAdvance(&output,length));
	ZeroMemory(output,length);
	CopyMemory(output,&header,sizeof(header));
	UINT offset = sizeof(header) + 2 * sizeof(ICONDIRENTRY);
	for(INT n = 1; n >= 0; n--)
	{
		UINT side = n ? 32 : 16;
		ICONDIRENTRY entry = {0};
		BITMAPINFOHEADER bitmap = {0};
		entry.bWidth = static_cast<BYTE>(side);
		entry.bHeight = static_cast<BYTE>(side);
		entry.wPlanes = 1;
		entry.wBitCount = 32;
		entry.dwBytesInRes = sizeof(bitmap) + side * side * 4 + ((masks & (1 << n)) ? side * 4 : 0);
		entry.dwImageOffset = offset;
		bitmap.biSize = sizeof(bitmap);
		bitmap.biWidth = side;
		bitmap.biHeight = side * 2;
		bitmap.biPlanes = 1;
		bitmap.biBitCount = 32;
		bitmap.biSizeImage = entry.dwBytesInRes;
		CopyMemory(output + sizeof(header) + n * sizeof(entry),&entry,sizeof(entry));
		CopyMemory(output + offset,&bitmap,sizeof(bitmap));
		BYTE* pixels = output + offset + sizeof(bitmap);
		for(UINT pixel = 0; pixel < side * side; pixel++)
		{
			pixels[pixel * 4] = 0x90;
			pixels[pixel * 4 + 1] = 0x60;
			pixels[pixel * 4 + 2] = 0x20;
			pixels[pixel * 4 + 3] = alphaMode == 0 ? 0 : alphaMode == 2 ? 255 : pixel % 3 == 0 ? 0 : pixel % 3 == 1 ? 128 : 255;
		}
		if(masks & (1 << n))
		{
			FillMemory(pixels + side * side * 4,side * 4,0x55);
		}
		offset += entry.dwBytesInRes;
	}
	bytes.Swap(result);
Cleanup:
	return hr;
}

// Build an unnormalized, canonical icon value to exercise existing saved JSON.
static HRESULT TestRawIconValue(const BYTE* bytes, UINT size, IJSONValue** value)
{
	HRESULT hr;
	CMemoryStream encoded;
	CRString text;
	PWSTR write;
	TStackRef<IJSONObject> object;
	TStackRef<IJSONValue> part;
	Check(ScEncodeBase64A(bytes,size,&encoded,NULL));
	Check(RStrAllocW(encoded.DataRemaining(),&text,&write));
	for(UINT n = 0; n < encoded.DataRemaining(); n++)
	{
		write[n] = encoded.GetReadPtr()[n];
	}
	Check(JSONCreateObject(&object));
	Check(JSONCreateStringW(SLP(L"ico"),&part));
	Check(object->AddValueW(L"format",part));
	part.Release();
	Check(JSONCreateStringW(SLP(L"base64"),&part));
	Check(object->AddValueW(L"encoding",part));
	part.Release();
	Check(JSONCreateString(text,&part));
	Check(object->AddValueW(L"data",part));
	Check(JSONWrapObject(object,value));
Cleanup:
	return hr;
}
