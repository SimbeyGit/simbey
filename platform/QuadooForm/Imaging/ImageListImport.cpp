#include "ImageListData.h"

namespace QFI
{
	static HRESULT RasterPNG(const Raster& pixels, UINT left, UINT top, UINT width, UINT height, IJSONValue** value)
	{
		HRESULT hr;
		Raster rgba;
		CMemoryStream encoded;
		Check(rgba.Allocate(width,height));
		for(UINT y = 0; y < height; y++)
		{
			for(UINT x = 0; x < width; x++)
			{
				const BYTE* source = pixels.pixels + ((top + y) * pixels.width + left + x) * 4;
				BYTE* target = rgba.pixels + (y * width + x) * 4;
				for(UINT c = 0; c < 3; c++)
				{
					target[c] = source[3] ? static_cast<BYTE>(min(255,(source[2 - c] * 255 + source[3] / 2) / source[3])) : 0;
				}
				target[3] = source[3];
			}
		}
		Check(EncodePNG(rgba.pixels,width,height,encoded));
		Check(CreateValue(L"png",encoded.GetReadPtr(),encoded.DataRemaining(),value));
	Cleanup:
		return hr;
	}
	HRESULT ImportImageListFile(PCWSTR path, UINT width, UINT height, UINT sizing, BOOL split, BOOL colorKey, IJSONArray** entries)
	{
		HRESULT hr;
		HANDLE file = INVALID_HANDLE_VALUE;
		BYTE signature[4];
		DWORD read;
		BOOL bitmap;
		TStackRef<IJSONValue> imported;
		TStackRef<IJSONValue> png;
		TStackRef<CImageResource> image;
		TStackRef<CImageResource> normalized;
		TStackRef<IJSONArray> candidate;
		Raster pixels;
		CheckIf(!entries || !path || sizing > ImageContain,E_INVALIDARG);
		*entries = NULL;
		Check(ValidateImageListOptions(width,height,32));
		file = CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
		CheckIf(file == INVALID_HANDLE_VALUE,HrEnsureLastError());
		CheckIf(!ReadFile(file,signature,sizeof(signature),&read,NULL) || read != sizeof(signature),E_INVALIDARG);
		CloseHandle(file);
		file = INVALID_HANDLE_VALUE;
		bitmap = signature[0] == 'B' && signature[1] == 'M';
		CheckIf(!bitmap && !(signature[0] == 137 && signature[1] == 'P' && signature[2] == 'N' && signature[3] == 'G') &&
			!(signature[0] == 0 && signature[1] == 0 && signature[2] == 1 && signature[3] == 0),E_INVALIDARG);
		Check(ImportFile(path,&imported));
		Check(CImageResource::Create(imported,&image));
		Check(image->NormalizePNG(width,height,&png));
		Check(CImageResource::Create(png,&normalized));
		UINT cx = normalized->Width();
		UINT cy = normalized->Height();
		if(split)
		{
			CheckIf(!bitmap || cx % width || cy % height || (cx == width && cy == height),E_INVALIDARG);
			CheckIf((cx / width) * (cy / height) > MaxImageListCount,E_INVALIDARG);
		}
		Check(JSONCreateArray(&candidate));
		if(split)
		{
			Check(normalized->RenderPixels(cx,cy,Normal,0,pixels));
			UINT cellWidth = split ? width : cx;
			UINT cellHeight = split ? height : cy;
			for(UINT y = 0; y < cy; y += cellHeight)
			{
				for(UINT x = 0; x < cx; x += cellWidth)
				{
					TStackRef<IJSONValue> cell;
					TStackRef<IJSONValue> entry;
					TStackRef<IJSONObject> object;
					TStackRef<IJSONValue> color;
					Check(RasterPNG(pixels,x,y,cellWidth,cellHeight,&cell));
					Check(ImageListEntry(cell,sizing,&entry));
					Check(entry->GetObject(&object));
					// Every cell retains the strip's original background key,
					// including cells whose own top-left pixel is foreground.
					Check(JSONCreateInteger(RGB(pixels.pixels[2],pixels.pixels[1],pixels.pixels[0]),&color));
					Check(object->AddValueW(L"maskColor",color));
					Check(candidate->Add(entry));
				}
			}
		}
		else
		{
			TStackRef<IJSONValue> entry;
			Check(ImageListEntry(png,sizing,&entry));
			Check(candidate->Add(entry));
		}
		if(colorKey)
		{
			for(sysint n = 0; n < candidate->Count(); n++)
			{
				TStackRef<IJSONObject> object;
				TStackRef<IJSONValue> enabled;
				Check(candidate->GetObject(n,&object));
				Check(JSONCreateBool(true,&enabled));
				Check(object->AddValueW(L"colorKey",enabled));
			}
		}
		*entries = candidate.Detach();
	Cleanup:
		if(file != INVALID_HANDLE_VALUE)
		{
			CloseHandle(file);
		}
		return hr;
	}
	HRESULT PaintImageListPixels(HDC dc, const RECT* bounds, const Raster& pixels, BOOL checker)
	{
		HRESULT hr = S_OK;
		HDC source = NULL;
		HBITMAP bitmap = NULL;
		HGDIOBJ previous = NULL;
		PVOID bits;
		BITMAPINFO info = {0};
		BLENDFUNCTION blend = { AC_SRC_OVER,0,255,AC_SRC_ALPHA };
		CheckIf(!dc || !bounds || !pixels.pixels,E_INVALIDARG);
		CheckIf(bounds->right <= bounds->left || bounds->bottom <= bounds->top,E_INVALIDARG);
		for(INT y = bounds->top; y < bounds->bottom; y += 8)
		{
			for(INT x = bounds->left; x < bounds->right; x += 8)
			{
				RECT tile = {x,y,min(x + 8,bounds->right),min(y + 8,bounds->bottom)};
				SetDCBrushColor(dc,checker && ((x - bounds->left) / 8 + (y - bounds->top) / 8) % 2 ? RGB(218,222,226) : RGB(255,255,255));
				FillRect(dc,&tile,static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
			}
		}
		info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		info.bmiHeader.biWidth = pixels.width;
		info.bmiHeader.biHeight = -static_cast<INT>(pixels.height);
		info.bmiHeader.biPlanes = 1;
		info.bmiHeader.biBitCount = 32;
		source = CreateCompatibleDC(dc);
		CheckIf(!source,HrEnsureLastError());
		bitmap = CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,NULL,0);
		CheckIf(!bitmap,HrEnsureLastError());
		previous = SelectObject(source,bitmap);
		CheckIf(!previous || previous == HGDI_ERROR,E_FAIL);
		CopyMemory(bits,pixels.pixels,pixels.width * pixels.height * 4);
		INT width = bounds->right - bounds->left;
		INT height = bounds->bottom - bounds->top;
		INT cx = width;
		INT cy = MulDiv(pixels.height,cx,pixels.width);
		if(cy > height)
		{
			cy = height;
			cx = MulDiv(pixels.width,cy,pixels.height);
		}
		CheckIf(!AlphaBlend(dc,bounds->left + (width - cx) / 2,bounds->top + (height - cy) / 2,cx,cy,source,0,0,pixels.width,pixels.height,blend),E_FAIL);
	Cleanup:
		if(previous && previous != HGDI_ERROR)
		{
			SelectObject(source,previous);
		}
		if(bitmap)
		{
			DeleteObject(bitmap);
		}
		if(source)
		{
			DeleteDC(source);
		}
		return hr;
	}
}
