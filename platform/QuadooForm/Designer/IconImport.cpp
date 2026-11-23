#include "Designer.h"
#include "Library\IconTypes.h"

namespace QFD
{
HRESULT DecodeIconPNG(const BYTE* bytes, UINT size, CMemoryStream& pixels, UINT* width, UINT* height);
static const UINT MaxIconInputBytes = 8 * 1024 * 1024;

static VOID IconPixels(const BYTE* source, UINT width, UINT height, UINT side, BYTE* color, BYTE* mask)
{
	UINT largest = max(width,height), w = max(1,(width * side + largest / 2) / largest), h = max(1,(height * side + largest / 2) / largest);
	UINT left = (side - w) / 2, top = (side - h) / 2, maskStride = ((side + 31) / 32) * 4;
	ULONGLONG area = static_cast<ULONGLONG>(width) * height;
	ZeroMemory(color,side * side * 4); FillMemory(mask,maskStride * side,0xff);
	for(UINT y = 0; y < h; y++)
	{
		UINT y0 = y * height, y1 = (y + 1) * height;
		for(UINT x = 0; x < w; x++)
		{
			UINT x0 = x * width, x1 = (x + 1) * width;
			ULONGLONG alpha = 0, red = 0, green = 0, blue = 0;
			for(UINT sy = y0 / h; sy < (y1 + h - 1) / h; sy++)
			{
				UINT overlapY = min(y1,(sy + 1) * h) - max(y0,sy * h);
				for(UINT sx = x0 / w; sx < (x1 + w - 1) / w; sx++)
				{
					const BYTE* pixel = source + (sy * width + sx) * 4;
					UINT overlapX = min(x1,(sx + 1) * w) - max(x0,sx * w);
					ULONGLONG weight = static_cast<ULONGLONG>(overlapX) * overlapY * pixel[3];
					alpha += weight; red += weight * pixel[0]; green += weight * pixel[1]; blue += weight * pixel[2];
				}
			}
			UINT row = side - 1 - top - y, column = left + x;
			BYTE* pixel = color + (row * side + column) * 4;
			pixel[3] = static_cast<BYTE>((alpha + area / 2) / area);
			if(pixel[3])
			{
				// Average premultiplied color, then store straight-alpha BGRA for
				// the ICO loader. Transparent RGB cannot darken neighboring pixels.
				pixel[0] = static_cast<BYTE>((blue + alpha / 2) / alpha);
				pixel[1] = static_cast<BYTE>((green + alpha / 2) / alpha);
				pixel[2] = static_cast<BYTE>((red + alpha / 2) / alpha);
				mask[row * maskStride + column / 8] &= ~(0x80 >> (column % 8));
			}
		}
	}
}
static HRESULT ConvertPNG(const BYTE* bytes, UINT size, IJSONValue** value)
{
	HRESULT hr;
	CMemoryStream pixels, ico; UINT width, height, sizes[4], count = 0, length; BYTE* output;
	Check(DecodeIconPNG(bytes,size,pixels,&width,&height));
	{
		UINT largest = min(256,max(width,height));
		static const UINT standard[] = { 16,32,48 };
		for(UINT n = 0; n < ARRAYSIZE(standard); n++) if(standard[n] < largest) sizes[count++] = standard[n];
		sizes[count++] = largest;
	}
	length = sizeof(ICONHEAD) + count * sizeof(ICONDIRENTRY);
	for(UINT n = 0; n < count; n++) length += sizeof(BITMAPINFOHEADER) + sizes[n] * sizes[n] * 4 + ((sizes[n] + 31) / 32) * 4 * sizes[n];
	Check(ico.Reserve(length));
	Check(ico.WriteAdvance(&output,length));
	ZeroMemory(output,length);
	{
		ICONHEAD header = { 0,1,static_cast<WORD>(count) };
		CopyMemory(output,&header,sizeof(header));
		UINT offset = sizeof(ICONHEAD) + count * sizeof(ICONDIRENTRY);
		for(UINT n = 0; n < count; n++)
		{
			UINT side = sizes[n], colorBytes = side * side * 4, maskBytes = ((side + 31) / 32) * 4 * side;
			ICONDIRENTRY entry; BITMAPINFOHEADER bitmap;
			ZeroMemory(&entry,sizeof(entry)); ZeroMemory(&bitmap,sizeof(bitmap));
			entry.bWidth = entry.bHeight = static_cast<BYTE>(side); entry.wPlanes = 1; entry.wBitCount = 32;
			entry.dwBytesInRes = sizeof(bitmap) + colorBytes + maskBytes; entry.dwImageOffset = offset;
			bitmap.biSize = sizeof(bitmap); bitmap.biWidth = side; bitmap.biHeight = side * 2; bitmap.biPlanes = 1; bitmap.biBitCount = 32;
			CopyMemory(output + sizeof(header) + n * sizeof(entry),&entry,sizeof(entry)); CopyMemory(output + offset,&bitmap,sizeof(bitmap));
			IconPixels(pixels.GetReadPtr(),width,height,side,output + offset + sizeof(bitmap),output + offset + sizeof(bitmap) + colorBytes);
			offset += entry.dwBytesInRes;
		}
	}
	Check(QuadooCreateIconValue(output,length,value));
Cleanup:
	return hr;
}
HRESULT ImportIcon(const BYTE* bytes, UINT size, IJSONValue** value)
{
	HRESULT hr;
	static const BYTE signature[] = { 137,80,78,71,13,10,26,10 };
	CheckIf(!value,E_POINTER);
	*value = NULL;
	CheckIf(!bytes || !size,E_INVALIDARG);
	CheckIf(size > MaxIconInputBytes,HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE));
	if(size >= sizeof(signature) && !memcmp(bytes,signature,sizeof(signature)))
	{
		Check(ConvertPNG(bytes,size,value));
	}
	else
	{
		Check(QuadooCreateIconValue(bytes,size,value));
	}
Cleanup:
	return hr;
}
HRESULT LoadIconFile(PCWSTR path, IJSONValue** value)
{
	HRESULT hr;
	HANDLE file = INVALID_HANDLE_VALUE; LARGE_INTEGER size; CMemoryStream bytes; BYTE* write; DWORD read;
	CheckIf(!value,E_POINTER);
	*value = NULL;
	CheckIf(!path,E_INVALIDARG);
	file = CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	CheckIf(file == INVALID_HANDLE_VALUE,HrEnsureLastError());
	CheckIf(!GetFileSizeEx(file,&size),HrEnsureLastError());
	CheckIf(size.QuadPart <= 0,E_INVALIDARG);
	CheckIf(size.QuadPart > MaxIconInputBytes,HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE));
	Check(bytes.Reserve(size.LowPart));
	Check(bytes.WriteAdvance(&write,size.LowPart));
	CheckIf(!ReadFile(file,write,size.LowPart,&read,NULL),HrEnsureLastError());
	CheckIf(read != size.LowPart,HRESULT_FROM_WIN32(ERROR_HANDLE_EOF));
	Check(ImportIcon(write,read,value));
Cleanup:
	if(file != INVALID_HANDLE_VALUE) CloseHandle(file);
	return hr;
}
}
