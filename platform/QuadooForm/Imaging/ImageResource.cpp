#include "ImageResource.h"
#include "IconDirectory.h"
#include "Published\SimbeyCore.h"

namespace QFI
{
static BOOL IsPNG(const BYTE* bytes, UINT size)
{
	static const BYTE signature[] = {137,80,78,71,13,10,26,10};
	return size >= sizeof(signature) && !memcmp(bytes,signature,sizeof(signature));
}
Raster::~Raster() { Clear(); }
VOID Raster::Clear() { Free(pixels); pixels = NULL; width = height = 0; }
VOID Raster::Swap(Raster& other)
{
	BYTE* data = pixels; pixels = other.pixels; other.pixels = data;
	UINT value = width; width = other.width; other.width = value;
	value = height; height = other.height; other.height = value;
}
HRESULT Raster::Allocate(UINT cx, UINT cy)
{
	HRESULT hr = S_OK;
	BYTE* data = NULL;
	CheckIf(!cx || !cy || cx > MaxDimension || cy > MaxDimension || cx > MaxPixels / cy,HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED));
	data = static_cast<BYTE*>(QFI::Allocate(cx * cy * 4));
	CheckAlloc(data);
	Clear(); pixels = data; width = cx; height = cy;
Cleanup:
	return hr;
}
static HRESULT PNGPixels(const BYTE* bytes, UINT size, Raster& raster, BOOL* alpha)
{
	HRESULT hr;
	CMemoryStream rgba;
	UINT width, height;
	Check(DecodePNG(bytes,size,rgba,&width,&height));
	Check(raster.Allocate(width,height));
	for(UINT n = 0; n < width * height; n++)
	{
		const BYTE* input = rgba.GetReadPtr() + n * 4;
		BYTE* output = raster.pixels + n * 4;
		BYTE a = input[3];
		output[0] = static_cast<BYTE>((input[2] * a + 127) / 255);
		output[1] = static_cast<BYTE>((input[1] * a + 127) / 255);
		output[2] = static_cast<BYTE>((input[0] * a + 127) / 255);
		output[3] = a;
		if(a != 255) *alpha = TRUE;
	}
Cleanup:
	return hr;
}
static HRESULT DIBPixels(const BYTE* bytes, UINT size, UINT width, UINT height, Raster& raster, Raster& xorRaster, BOOL* alpha)
{
	HRESULT hr;
	BITMAPINFOHEADER header;
	BOOL nativeAlpha = FALSE, hasXor = FALSE;
	CheckIf(size < sizeof(header),E_INVALIDARG);
	CopyMemory(&header,bytes,sizeof(header));
	CheckIf(header.biSize < sizeof(header) || header.biSize > size || header.biWidth != width || header.biHeight != height * 2 || header.biPlanes != 1,E_INVALIDARG);
	CheckIf(header.biSize != sizeof(header) || header.biCompression != BI_RGB,HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED));
	UINT bits = header.biBitCount;
	CheckIf(bits != 1 && bits != 4 && bits != 8 && bits != 16 && bits != 24 && bits != 32,HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED));
	UINT colors = bits <= 8 ? 1U << bits : 0;
	CheckIf(header.biClrUsed > colors,E_INVALIDARG);
	if(header.biClrUsed) colors = header.biClrUsed;
	UINT stride = ((width * bits + 31) / 32) * 4, maskStride = ((width + 31) / 32) * 4;
	UINT start = sizeof(header) + colors * 4;
	CheckIf(start > size || (stride + maskStride) * height > size - start,E_INVALIDARG);
	const BYTE* color = bytes + start;
	const BYTE* mask = color + stride * height;
	if(bits == 32)
		for(UINT y = 0; y < height && !nativeAlpha; y++)
			for(UINT x = 0; x < width; x++) if(color[y * stride + x * 4 + 3]) { nativeAlpha = TRUE; break; }
	Check(raster.Allocate(width,height));
	for(UINT y = 0; y < height; y++)
	{
		const BYTE* row = color + (height - 1 - y) * stride;
		const BYTE* maskRow = mask + (height - 1 - y) * maskStride;
		for(UINT x = 0; x < width; x++)
		{
			BYTE b = 0, g = 0, r = 0, a = 255;
			BOOL masked = (maskRow[x / 8] & (0x80 >> (x % 8))) != 0;
			if(bits <= 8)
			{
				UINT index = bits == 8 ? row[x] : bits == 4 ? (row[x / 2] >> (x % 2 ? 0 : 4)) & 15 : (row[x / 8] >> (7 - x % 8)) & 1;
				CheckIf(index >= colors,E_INVALIDARG);
				const BYTE* entry = bytes + sizeof(header) + index * 4;
				b = entry[0]; g = entry[1]; r = entry[2];
			}
			else if(bits == 16)
			{
				WORD pixel; CopyMemory(&pixel,row + x * 2,2);
				b = static_cast<BYTE>((pixel & 31) * 255 / 31);
				g = static_cast<BYTE>(((pixel >> 5) & 31) * 255 / 31);
				r = static_cast<BYTE>(((pixel >> 10) & 31) * 255 / 31);
			}
			else
			{
				const BYTE* pixel = row + x * (bits / 8);
				b = pixel[0]; g = pixel[1]; r = pixel[2];
				if(nativeAlpha) a = pixel[3];
			}
			BYTE* output = raster.pixels + (y * width + x) * 4;
			if(masked && !nativeAlpha)
			{
				a = 0;
				// Classic AND=1/XOR!=0 pixels modify the destination, rather than
				// becoming an arbitrary transparent color in an RGBA conversion.
				if(b || g || r)
				{
					if(!xorRaster.pixels)
					{
						Check(xorRaster.Allocate(width,height));
					}
					BYTE* effect = xorRaster.pixels + (y * width + x) * 4;
					effect[0] = b; effect[1] = g; effect[2] = r; effect[3] = 255;
					hasXor = TRUE;
				}
			}
			output[0] = static_cast<BYTE>((b * a + 127) / 255);
			output[1] = static_cast<BYTE>((g * a + 127) / 255);
			output[2] = static_cast<BYTE>((r * a + 127) / 255);
			output[3] = a;
			if(a != 255 || hasXor) *alpha = TRUE;
		}
	}
	hr = S_OK;
Cleanup:
	return hr;
}
CImageResource::CImageResource() : m_natural(0), m_decoded(UINT_MAX), m_ico(FALSE), m_alpha(FALSE),
	m_cacheDC(NULL), m_cacheBitmap(NULL), m_cacheOld(NULL), m_cacheWidth(0), m_cacheHeight(0), m_cacheDPI(0),
	m_cacheMode(0), m_cacheAlign(0), m_cacheFilter(0), m_cacheOffsetX(0), m_cacheOffsetY(0),
	m_grayDC(NULL), m_grayBitmap(NULL), m_grayOld(NULL)
{
}
CImageResource::~CImageResource() { InvalidateCache(); }
HRESULT CImageResource::DecodeEntry(UINT index, Raster& pixels, Raster& xorPixels, BOOL* alpha)
{
	HRESULT hr;
	CheckIf(index >= static_cast<UINT>(m_entries.Length()),E_INVALIDARG);
	const Entry& entry = m_entries[index];
	if(entry.png)
	{
		Check(PNGPixels(m_bytes.GetReadPtr() + entry.offset,entry.size,pixels,alpha));
		CheckIf(pixels.width != entry.width || pixels.height != entry.height,E_INVALIDARG);
	}
	else
	{
		Check(DIBPixels(m_bytes.GetReadPtr() + entry.offset,entry.size,entry.width,entry.height,pixels,xorPixels,alpha));
	}
Cleanup:
	return hr;
}
UINT CImageResource::SelectEntry(UINT width, UINT height) const
{
	UINT selected = UINT_MAX;
	ULONGLONG best = ~static_cast<ULONGLONG>(0);
	for(UINT n = 0; n < static_cast<UINT>(m_entries.Length()); n++)
	{
		const Entry& entry = m_entries[n];
		if(!entry.supported) continue;
		BOOL smaller = entry.width < width || entry.height < height;
		ULONGLONG distance = static_cast<ULONGLONG>(abs(static_cast<INT>(entry.width) - static_cast<INT>(width)) + abs(static_cast<INT>(entry.height) - static_cast<INT>(height))) * 64 + 32 - min(32,entry.depth);
		if(smaller) distance += static_cast<ULONGLONG>(1) << 40;
		if(distance < best) { best = distance; selected = n; }
	}
	return selected;
}
HRESULT CImageResource::Open(PCWSTR format, const BYTE* bytes, UINT size)
{
	HRESULT hr;
	ULONG written;
	UINT total = 0;
	CheckIf(!bytes || !size || size > MaxBytes,E_INVALIDARG);
	CheckIf(!format || (lstrcmpW(format,L"png") && lstrcmpW(format,L"ico")),E_INVALIDARG);
	m_ico = !lstrcmpW(format,L"ico");
	Check(m_bytes.Write(bytes,size,&written));
	if(m_ico)
	{
		IconDirectory directory;
		Check(ValidateIconDirectory(bytes,size,MaxBytes,&directory));
		for(UINT n = 0; n < directory.count; n++)
		{
			IconEntry raw;
			CopyMemory(&raw,bytes + sizeof(directory) + n * sizeof(raw),sizeof(raw));
			Entry entry = { raw.width ? raw.width : 256,raw.height ? raw.height : 256,raw.bits,raw.offset,raw.size,IsPNG(bytes + raw.offset,raw.size),TRUE };
			CheckIf((raw.planes && raw.planes != 1) || total > MaxPixels - entry.width * entry.height,E_INVALIDARG);
			total += entry.width * entry.height;
			Check(m_entries.Append(entry));
			Raster decoded, effect;
			hr = DecodeEntry(n,decoded,effect,&m_alpha);
			if(hr == HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED)) m_entries[n].supported = FALSE;
			else
			{
				Check(hr);
				if(!entry.png)
				{
					BITMAPINFOHEADER header;
					CopyMemory(&header,bytes + raw.offset,sizeof(header));
					CheckIf(raw.bits && raw.bits != header.biBitCount,E_INVALIDARG);
					m_entries[n].depth = header.biBitCount;
				}
				else m_entries[n].depth = 32;
			}
		}
		m_natural = SelectEntry(32,32);
		CheckIf(m_natural == UINT_MAX,HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED));
		Check(DecodeEntry(m_natural,m_source,m_sourceXor,&m_alpha));
		m_decoded = m_natural;
	}
	else
	{
		Check(PNGPixels(bytes,size,m_source,&m_alpha));
		Entry entry = { m_source.width,m_source.height,32,0,size,TRUE,TRUE };
		Check(m_entries.Append(entry));
		m_decoded = 0;
	}
	hr = S_OK;
Cleanup:
	return hr;
}
HRESULT CImageResource::FromBytes(PCWSTR format, const BYTE* bytes, UINT size, CImageResource** resource)
{
	HRESULT hr;
	TStackRef<CImageResource> candidate;
	CheckIf(!resource,E_POINTER);
	*resource = NULL;
	candidate.Attach(__new CImageResource);
	CheckAlloc(candidate);
	Check(candidate->Open(format,bytes,size));
	*resource = candidate.Detach();
Cleanup:
	return hr;
}
static HRESULT StringField(IJSONObject* object, PCWSTR name, RSTRING* text)
{
	HRESULT hr;
	TStackRef<IJSONValue> value;
	Check(object->FindNonNullValueW(name,&value));
	Check(value->GetString(text));
Cleanup:
	return hr;
}
HRESULT CImageResource::Create(IJSONValue* value, CImageResource** resource)
{
	HRESULT hr = S_OK;
	TStackRef<IJSONObject> object;
	CRString format, encoding, text;
	CMemoryStream bytes, encoded;
	PSTR ascii = NULL;
	CheckIf(!resource,E_POINTER);
	*resource = NULL;
	if(!value) goto Cleanup;
	Check(value->GetObject(&object));
	CheckIf(object->Count() != 3,E_INVALIDARG);
	Check(StringField(object,L"format",&format));
	Check(StringField(object,L"encoding",&encoding));
	Check(StringField(object,L"data",&text));
	CheckIf(!(encoding == RSTRING_CAST(L"base64")),E_INVALIDARG);
	INT length = RStrLen(text);
	CheckIf(length < 4 || length % 4 || length > static_cast<INT>((MaxBytes + 2) / 3 * 4),E_INVALIDARG);
	ascii = static_cast<PSTR>(Allocate(length));
	CheckAlloc(ascii);
	for(INT n = 0; n < length; n++)
	{
		CheckIf(text.c_wstr()[n] > 127,E_INVALIDARG);
		ascii[n] = static_cast<CHAR>(text.c_wstr()[n]);
	}
	Check(ScDecodeBase64A(ascii,length,&bytes,NULL));
	CheckIf(bytes.DataRemaining() > MaxBytes,E_INVALIDARG);
	Check(ScEncodeBase64A(bytes.GetReadPtr(),bytes.DataRemaining(),&encoded,NULL));
	CheckIf(encoded.DataRemaining() != length || memcmp(ascii,encoded.GetReadPtr(),length),E_INVALIDARG);
	Check(FromBytes(format.c_wstr(),bytes.GetReadPtr(),bytes.DataRemaining(),resource));
Cleanup:
	Free(ascii);
	return hr;
}
HRESULT CreateValue(PCWSTR format, const BYTE* bytes, UINT size, IJSONValue** value)
{
	HRESULT hr;
	TStackRef<CImageResource> resource;
	CheckIf(!value,E_POINTER);
	*value = NULL;
	Check(CImageResource::FromBytes(format,bytes,size,&resource));
	Check(resource->GetValue(value));
Cleanup:
	return hr;
}
HRESULT CImageResource::GetValue(IJSONValue** value) const
{
	HRESULT hr;
	CMemoryStream encoded;
	TStackRef<IJSONObject> object;
	TStackRef<IJSONValue> field;
	CRString text;
	PWSTR write;
	CheckIf(!value,E_POINTER);
	*value = NULL;
	Check(ScEncodeBase64A(m_bytes.GetReadPtr(),m_bytes.DataRemaining(),&encoded,NULL));
	Check(RStrAllocW(encoded.DataRemaining(),&text,&write));
	for(UINT n = 0; n < encoded.DataRemaining(); n++) write[n] = encoded.GetReadPtr()[n];
	Check(JSONCreateObject(&object));
	Check(JSONCreateString(RSTRING_CAST(m_ico ? L"ico" : L"png"),&field));
	Check(object->AddValueW(L"format",field));
	field.Release();
	Check(JSONCreateString(RSTRING_CAST(L"base64"),&field));
	Check(object->AddValueW(L"encoding",field));
	field.Release();
	Check(JSONCreateString(text,&field));
	Check(object->AddValueW(L"data",field));
	Check(JSONWrapObject(object,value));
Cleanup:
	return hr;
}
HRESULT CImageResource::ColorKeyPNG(COLORREF color, CImageResource** resource)
{
	HRESULT hr;
	CMemoryStream rgba;
	CMemoryStream png;
	UINT width;
	UINT height;
	CheckIf(!resource,E_POINTER);
	*resource = NULL;
	CheckIf(m_ico || (color != CLR_INVALID && (color & 0xff000000)),E_INVALIDARG);
	Check(DecodePNG(m_bytes.GetReadPtr(),m_bytes.DataRemaining(),rgba,&width,&height));
	BYTE* pixels = const_cast<BYTE*>(rgba.GetReadPtr());
	if(color == CLR_INVALID)
	{
		color = RGB(pixels[0],pixels[1],pixels[2]);
	}
	for(UINT n = 0; n < width * height; n++)
	{
		BYTE* pixel = pixels + n * 4;
		if(RGB(pixel[0],pixel[1],pixel[2]) == color)
		{
			ZeroMemory(pixel,4);
		}
	}
	// Preserve explicit RGBA here. The bundled codec's automatic grayscale/
	// color-key conversion can otherwise discard opaque dark foreground pixels.
	Check(EncodePNG(pixels,width,height,png,FALSE));
	Check(FromBytes(L"png",png.GetReadPtr(),png.DataRemaining(),resource));
Cleanup:
	return hr;
}
HRESULT CImageResource::NormalizePNG(UINT width, UINT height, IJSONValue** value)
{
	HRESULT hr;
	Raster pixels;
	Raster effect;
	Raster rgba;
	CMemoryStream png;
	BOOL alpha = FALSE;
	CheckIf(!value,E_POINTER);
	*value = NULL;
	if(!m_ico)
	{
		Check(GetValue(value));
		goto Cleanup;
	}
	{
		UINT index = SelectEntry(width,height);
		CheckIf(index == UINT_MAX,E_INVALIDARG);
		const Entry& entry = m_entries[index];
		if(entry.png)
		{
			Check(CreateValue(L"png",m_bytes.GetReadPtr() + entry.offset,entry.size,value));
			goto Cleanup;
		}
		Check(DecodeEntry(index,pixels,effect,&alpha));
	}
	// Destination-XOR icons have no lossless RGBA representation.
	CheckIf(effect.pixels,HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED));
	Check(rgba.Allocate(pixels.width,pixels.height));
	for(UINT n = 0; n < pixels.width * pixels.height; n++)
	{
		const BYTE* source = pixels.pixels + n * 4;
		BYTE* target = rgba.pixels + n * 4;
		for(UINT c = 0; c < 3; c++)
		{
			target[c] = source[3] ? static_cast<BYTE>(min(255,(source[2 - c] * 255 + source[3] / 2) / source[3])) : 0;
		}
		target[3] = source[3];
	}
	Check(EncodePNG(rgba.pixels,rgba.width,rgba.height,png));
	Check(CreateValue(L"png",png.GetReadPtr(),png.DataRemaining(),value));
Cleanup:
	return hr;
}

}
