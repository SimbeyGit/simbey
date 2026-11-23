#include "SpritePixels.h"

namespace AQVM
{
	static volatile LONG s_spriteBytes;
	static const LONG SpriteByteLimit = 64 * 1024 * 1024;

	LONG SpriteAllocatedBytes()
	{
		return InterlockedCompareExchange(&s_spriteBytes,0,0);
	}

	CSpriteImage::CSpriteImage() : m_reserved(0), width(0), height(0)
	{
	}

	CSpriteImage::~CSpriteImage()
	{
		InterlockedExchangeAdd(&s_spriteBytes,-m_reserved);
	}

	const BYTE* CSpriteImage::Pixels() const
	{
		return m_pixels.GetReadPtr();
	}

	HRESULT CSpriteImage::Allocate(UINT cx, UINT cy, BYTE** pixels)
	{
		HRESULT hr;
		LONG reserved = 0;
		CheckIf(!pixels,E_POINTER);
		*pixels = NULL;
		CheckIf(m_reserved,QuadooForm::EInvalidState);
		CheckIf(!cx || !cy || cx > QFI::MaxDimension || cy > QFI::MaxDimension || cx > QFI::MaxPixels / cy,HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED));
		LONG count = cx * cy * 4;
		LONG used = InterlockedExchangeAdd(&s_spriteBytes,count);
		reserved = count;
		CheckIf(used > SpriteByteLimit - count,E_OUTOFMEMORY);
		Check(m_pixels.TWriteAdvance<BYTE>(pixels,count));
		m_reserved = count;
		reserved = 0;
		width = cx;
		height = cy;
	Cleanup:
		if(reserved)
		{
			InterlockedExchangeAdd(&s_spriteBytes,-reserved);
		}
		return hr;
	}

	HRESULT CSpriteImage::LoadPixels(UINT cx, UINT cy, const BYTE* bytes, DWORD count, SpritePixelFormat format, UINT stride)
	{
		HRESULT hr;
		BYTE* target;
		CheckIf(!bytes,E_POINTER);
		CheckIf(format != SpriteRGB24 && format != SpriteRGBA32 && format != SpriteBGRA32,E_INVALIDARG);
		UINT pixelSize = format == SpriteRGB24 ? 3 : 4;
		CheckIf(!cx || !cy || cx > QFI::MaxDimension || cy > QFI::MaxDimension || cx > QFI::MaxPixels / cy,HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED));
		UINT packed = cx * pixelSize;
		if(!stride)
		{
			stride = packed;
		}
		CheckIf(stride < packed || static_cast<ULONGLONG>(stride) * cy != count,E_INVALIDARG);
		Check(Allocate(cx,cy,&target));
		for(UINT y = 0; y < cy; y++)
		{
			const BYTE* input = bytes + static_cast<SIZE_T>(y) * stride;
			BYTE* output = target + y * cx * 4;
			for(UINT x = 0; x < cx; x++)
			{
				output[x * 4] = input[x * pixelSize + (format == SpriteBGRA32 ? 0 : 2)];
				output[x * 4 + 1] = input[x * pixelSize + 1];
				output[x * 4 + 2] = input[x * pixelSize + (format == SpriteBGRA32 ? 2 : 0)];
				output[x * 4 + 3] = pixelSize == 3 ? 255 : input[x * pixelSize + 3];
			}
		}
	Cleanup:
		return hr;
	}

	HRESULT CSpriteImage::LoadPng(const BYTE* bytes, DWORD count)
	{
		HRESULT hr;
		CMemoryStream decoded;
		UINT cx;
		UINT cy;
		Check(QFI::DecodePNG(bytes,count,decoded,&cx,&cy));
		Check(LoadPixels(cx,cy,decoded.GetReadPtr(),decoded.DataRemaining(),SpriteRGBA32,0));
	Cleanup:
		return hr;
	}

	HRESULT CSpriteImage::LoadFile(RSTRING path)
	{
		HRESULT hr;
		HANDLE file = INVALID_HANDLE_VALUE;
		LARGE_INTEGER length;
		CMemoryStream bytes;
		BYTE* target;
		DWORD read;
		CheckIf(!path || !RStrIsWide(path) || !RStrLen(path),E_INVALIDARG);
		CheckIf(TStrCchLenChecked(RStrToWide(path),RStrLen(path)) != RStrLen(path),E_INVALIDARG);
		file = CreateFileW(RStrToWide(path),GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
		CheckIf(file == INVALID_HANDLE_VALUE,HrEnsureLastError());
		CheckIf(!GetFileSizeEx(file,&length),HrEnsureLastError());
		CheckIf(length.QuadPart < 4 || length.QuadPart > QFI::MaxBytes,HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE));
		Check(bytes.TWriteAdvance<BYTE>(&target,length.LowPart));
		CheckIf(!ReadFile(file,target,length.LowPart,&read,NULL),HrEnsureLastError());
		CheckIf(read != length.LowPart,HRESULT_FROM_WIN32(ERROR_HANDLE_EOF));
		if(target[0] == 137 && target[1] == 'P' && target[2] == 'N' && target[3] == 'G')
		{
			Check(LoadPng(target,read));
		}
		else
		{
			TStackRef<IJSONValue> value;
			TStackRef<QFI::CImageResource> image;
			QFI::Raster decoded;
			BOOL bitmap = target[0] == 'B' && target[1] == 'M';
			CheckIf(!bitmap && (target[0] || target[1] || target[2] != 1 || target[3]),HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED));
			if(bitmap)
			{
				// Reuse BMP import without the legacy optional palette profiler.
				Check(QFI::ImportFile(RStrToWide(path),&value,FALSE));
				Check(QFI::CImageResource::Create(value,&image));
			}
			else
			{
				Check(QFI::CImageResource::FromBytes(L"ico",target,read,&image));
			}
			Check(image->RenderPixels(image->Width(),image->Height(),QFI::Normal,0,decoded));
			Check(Allocate(decoded.width,decoded.height,&target));
			for(UINT n = 0; n < width * height; n++)
			{
				UINT alpha = decoded.pixels[n * 4 + 3];
				for(UINT channel = 0; channel < 3; channel++)
				{
					target[n * 4 + channel] = alpha ? static_cast<BYTE>(min(255,(decoded.pixels[n * 4 + channel] * 255 + alpha / 2) / alpha)) : 0;
				}
				target[n * 4 + 3] = static_cast<BYTE>(alpha);
			}
		}
	Cleanup:
		if(file != INVALID_HANDLE_VALUE)
		{
			CloseHandle(file);
		}
		return hr;
	}

	HRESULT CSpriteImage::Composite(SIF_SURFACE* surface, INT x, INT y, BOOL* changed) const
	{
		HRESULT hr = S_OK;
		CheckIf(!changed,E_POINTER);
		*changed = FALSE;
		CheckIf(!surface || !surface->pbSurface || surface->cBitsPerPixel != 32 || surface->xSize <= 0 || surface->ySize <= 0,E_INVALIDARG);
		// Decoder limits constrain owned source images, not a borrowed Canvas.
		// Validate its addressable rows without imposing source-image limits.
		ULONGLONG rowBytes = static_cast<ULONGLONG>(surface->xSize) * 4;
		CheckIf(surface->lPitch <= 0 || rowBytes > static_cast<ULONGLONG>(surface->lPitch),E_INVALIDARG);
		ULONGLONG span = static_cast<ULONGLONG>(surface->ySize - 1) * surface->lPitch + rowBytes;
		CheckIf(span > static_cast<SIZE_T>(-1),E_INVALIDARG);
		CheckIf(!width || !height || !Pixels(),QuadooForm::EInvalidState);
		// Intersect destination rectangles before narrowing or touching pixels.
		// In particular, negating INT_MIN or adding width to INT_MAX must not
		// overflow simply because an entirely offscreen draw is a no-op.
		LONGLONG left = max(0LL,static_cast<LONGLONG>(x));
		LONGLONG top = max(0LL,static_cast<LONGLONG>(y));
		LONGLONG right = min(static_cast<LONGLONG>(surface->xSize),static_cast<LONGLONG>(x) + width);
		LONGLONG bottom = min(static_cast<LONGLONG>(surface->ySize),static_cast<LONGLONG>(y) + height);
		CheckIfIgnore(left >= right || top >= bottom,S_OK);
		UINT sourceX = static_cast<UINT>(left - x);
		UINT sourceY = static_cast<UINT>(top - y);
		INT copyWidth = static_cast<INT>(right - left);
		INT copyHeight = static_cast<INT>(bottom - top);
		for(INT row = 0; row < copyHeight; row++)
		{
			const BYTE* source = Pixels() + ((sourceY + row) * width + sourceX) * 4;
			BYTE* target = surface->pbSurface + static_cast<SIZE_T>(top + row) * surface->lPitch + static_cast<SIZE_T>(left) * 4;
			for(INT column = 0; column < copyWidth; column++)
			{
				UINT alpha = source[3];
				if(alpha == 255)
				{
					CopyMemory(target,source,4);
					*changed = TRUE;
				}
				else if(alpha)
				{
					for(UINT channel = 0; channel < 3; channel++)
					{
						target[channel] = static_cast<BYTE>((source[channel] * alpha + target[channel] * (255 - alpha) + 127) / 255);
					}
					target[3] = 255;
					*changed = TRUE;
				}
				source += 4;
				target += 4;
			}
		}
	Cleanup:
		return hr;
	}
}
