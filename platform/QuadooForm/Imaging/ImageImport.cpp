#include "ImageResource.h"
#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib,"gdiplus.lib")

namespace QFI
{
static HRESULT ImportRaster(const BYTE* bytes, UINT size, IJSONValue** value, BOOL optimizeColor)
{
	HRESULT hr;
	HGLOBAL memory = NULL;
	TStackRef<IStream> stream;
	Gdiplus::GdiplusStartupInput startup;
	ULONG_PTR token = 0;
	CMemoryStream png;
	Raster rgba;
	CheckIf(Gdiplus::GdiplusStartup(&token,&startup,NULL) != Gdiplus::Ok,E_FAIL);
	memory = GlobalAlloc(GMEM_MOVEABLE,size);
	CheckAlloc(memory);
	{
		PVOID target = GlobalLock(memory);
		CheckIf(!target,HrEnsureLastError());
		CopyMemory(target,bytes,size);
		GlobalUnlock(memory);
	}
	Check(CreateStreamOnHGlobal(memory,TRUE,&stream));
	memory = NULL;
	{
		Gdiplus::Bitmap image(stream,FALSE);
		GUID format;
		CheckIf(image.GetLastStatus() != Gdiplus::Ok || image.GetRawFormat(&format) != Gdiplus::Ok,HRESULT_FROM_WIN32(ERROR_INVALID_DATA));
		CheckIf(!IsEqualGUID(format,Gdiplus::ImageFormatJPEG) && !IsEqualGUID(format,Gdiplus::ImageFormatBMP),HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED));
		UINT width = image.GetWidth(), height = image.GetHeight();
		CheckIf(!width || !height || width > MaxDimension || height > MaxDimension || width > MaxPixels / height,HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED));
		if(IsEqualGUID(format,Gdiplus::ImageFormatJPEG))
		{
			UINT length = image.GetPropertyItemSize(PropertyTagOrientation);
			if(length >= sizeof(Gdiplus::PropertyItem) && length <= 65536)
			{
				Gdiplus::PropertyItem* property = static_cast<Gdiplus::PropertyItem*>(Allocate(length));
				CheckAlloc(property);
				UINT orientation = 1;
				if(image.GetPropertyItem(PropertyTagOrientation,length,property) == Gdiplus::Ok && property->type == PropertyTagTypeShort && property->length == sizeof(WORD) && property->value)
					CopyMemory(&orientation,property->value,sizeof(WORD));
				Free(property);
				static const Gdiplus::RotateFlipType rotations[] = { Gdiplus::RotateNoneFlipNone,Gdiplus::RotateNoneFlipNone,Gdiplus::RotateNoneFlipX,Gdiplus::Rotate180FlipNone,Gdiplus::RotateNoneFlipY,Gdiplus::Rotate90FlipX,Gdiplus::Rotate90FlipNone,Gdiplus::Rotate270FlipX,Gdiplus::Rotate270FlipNone };
				if(orientation > 1 && orientation < ARRAYSIZE(rotations))
				{
					CheckIf(image.RotateFlip(rotations[orientation]) != Gdiplus::Ok,HRESULT_FROM_WIN32(ERROR_INVALID_DATA));
				}
			}
		}
		width = image.GetWidth(); height = image.GetHeight();
		Check(rgba.Allocate(width,height));
		Gdiplus::Rect rect(0,0,width,height);
		Gdiplus::BitmapData data;
		CheckIf(image.LockBits(&rect,Gdiplus::ImageLockModeRead,PixelFormat32bppARGB,&data) != Gdiplus::Ok,HRESULT_FROM_WIN32(ERROR_INVALID_DATA));
		for(UINT y = 0; y < height; y++)
		{
			const BYTE* source = static_cast<const BYTE*>(data.Scan0) + static_cast<INT>(y) * data.Stride;
			BYTE* target = rgba.pixels + y * width * 4;
			for(UINT x = 0; x < width; x++)
			{
				target[x * 4] = source[x * 4 + 2]; target[x * 4 + 1] = source[x * 4 + 1];
				target[x * 4 + 2] = source[x * 4]; target[x * 4 + 3] = source[x * 4 + 3];
			}
		}
		Gdiplus::Status status = image.UnlockBits(&data);
		CheckIf(status != Gdiplus::Ok,HRESULT_FROM_WIN32(ERROR_INVALID_DATA));
	}
	Check(EncodePNG(rgba.pixels,rgba.width,rgba.height,png,optimizeColor));
	Check(CreateValue(L"png",png.GetReadPtr(),png.DataRemaining(),value));
Cleanup:
	stream.Release();
	if(memory) GlobalFree(memory);
	if(token) Gdiplus::GdiplusShutdown(token);
	return hr;
}
HRESULT ImportFile(PCWSTR path, IJSONValue** value, BOOL optimizeColor)
{
	HRESULT hr;
	HANDLE file = INVALID_HANDLE_VALUE;
	LARGE_INTEGER size;
	BYTE* bytes = NULL;
	DWORD read;
	CheckIf(!value,E_POINTER);
	*value = NULL;
	CheckIf(!path,E_INVALIDARG);
	file = CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	CheckIf(file == INVALID_HANDLE_VALUE,HrEnsureLastError());
	CheckIf(!GetFileSizeEx(file,&size),HrEnsureLastError());
	CheckIf(size.QuadPart < 4 || size.QuadPart > MaxBytes,HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE));
	bytes = static_cast<BYTE*>(Allocate(size.LowPart));
	CheckAlloc(bytes);
	CheckIf(!ReadFile(file,bytes,size.LowPart,&read,NULL),HrEnsureLastError());
	CheckIf(read != size.LowPart,HRESULT_FROM_WIN32(ERROR_HANDLE_EOF));
	CloseHandle(file); file = INVALID_HANDLE_VALUE;
	if(bytes[0] == 137 && bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G')
	{
		Check(CreateValue(L"png",bytes,read,value));
	}
	else if(!bytes[0] && !bytes[1] && bytes[2] == 1 && !bytes[3])
	{
		Check(CreateValue(L"ico",bytes,read,value));
	}
	else
	{
		Check(ImportRaster(bytes,read,value,optimizeColor));
	}
Cleanup:
	if(file != INVALID_HANDLE_VALUE) CloseHandle(file);
	Free(bytes);
	return hr;
}
}
