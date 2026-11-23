#include "Runtime.h"
#include "Library\Core\MemoryStream.h"
#include "..\Imaging\IconDirectory.h"

namespace QF
{
	static const UINT MaxIconBytes = 1024 * 1024;
	typedef QFI::IconDirectory IconDirectory;
	typedef QFI::IconEntry IconEntry;
	// Validate every image before allocating or repairing anything. Only a complete
	// 32-bit XOR bitmap with its entire AND mask omitted is recoverable here.
	static HRESULT NormalizeICO (const BYTE* bytes, UINT size, CMemoryStream& normalized)
	{
		HRESULT hr;
		IconDirectory directory;
		IconEntry entries[64];
		UINT missing[64] = {0};
		UINT extra = 0;
		CMemoryStream result;
		BYTE* output;
		ULONG written;
		Check(QFI::ValidateIconDirectory(bytes,size,MaxIconBytes,&directory));
		for(UINT n = 0; n < directory.count; n++)
		{
			IconEntry& entry = entries[n];
			BITMAPINFOHEADER bitmap;
			CopyMemory(&entry,bytes + sizeof(directory) + n * sizeof(entry),sizeof(entry));
			CheckIf(entry.size < sizeof(bitmap),E_INVALIDARG);
			CopyMemory(&bitmap,bytes + entry.offset,sizeof(bitmap));
			UINT width = entry.width ? entry.width : 256;
			UINT height = entry.height ? entry.height : 256;
			CheckIf(bitmap.biSize != sizeof(bitmap) || bitmap.biWidth != width || bitmap.biHeight != height * 2 || bitmap.biPlanes != 1 || bitmap.biCompression != BI_RGB,E_INVALIDARG);
			CheckIf(bitmap.biBitCount != 1 && bitmap.biBitCount != 4 && bitmap.biBitCount != 8 && bitmap.biBitCount != 24 && bitmap.biBitCount != 32,E_INVALIDARG);
			CheckIf((entry.planes && entry.planes != 1) || (entry.bits && entry.bits != bitmap.biBitCount),E_INVALIDARG);
			UINT colors = bitmap.biBitCount <= 8 ? 1U << bitmap.biBitCount : 0;
			CheckIf(bitmap.biClrUsed > colors,E_INVALIDARG);
			if(bitmap.biClrUsed)
			{
				colors = bitmap.biClrUsed;
			}
			UINT colorBytes = ((width * bitmap.biBitCount + 31) / 32) * 4 * height;
			UINT maskBytes = ((width + 31) / 32) * 4 * height;
			UINT colorEnd = sizeof(bitmap) + colors * sizeof(RGBQUAD) + colorBytes;
			if(entry.size < colorEnd + maskBytes)
			{
				CheckIf(bitmap.biBitCount != 32 || entry.size != colorEnd,E_INVALIDARG);
				missing[n] = maskBytes;
				CheckIf(maskBytes > MaxIconBytes - size - extra,E_INVALIDARG);
				extra += maskBytes;
			}
		}
		if(!extra)
		{
			// Preserve complete valid files, including padding and image order.
			Check(result.Write(bytes,size,&written));
		}
		else
		{
			UINT length = sizeof(directory) + directory.count * sizeof(IconEntry);
			for(UINT n = 0; n < directory.count; n++)
			{
				length += entries[n].size + missing[n];
			}
			Check(result.WriteAdvance(&output,length));
			ZeroMemory(output,length);
			CopyMemory(output,&directory,sizeof(directory));
			UINT offset = sizeof(directory) + directory.count * sizeof(IconEntry);
			for(UINT n = 0; n < directory.count; n++)
			{
				IconEntry entry = entries[n];
				CopyMemory(output + offset,bytes + entry.offset,entry.size);
				if(missing[n])
				{
					BITMAPINFOHEADER bitmap;
					CopyMemory(&bitmap,output + offset,sizeof(bitmap));
					// BI_RGB permits zero here; old writers sometimes included the
					// header in biSizeImage, so do not retain that stale size.
					bitmap.biSizeImage = 0;
					CopyMemory(output + offset,&bitmap,sizeof(bitmap));
					UINT width = bitmap.biWidth;
					UINT height = bitmap.biHeight / 2;
					UINT stride = ((width + 31) / 32) * 4;
					const BYTE* pixels = output + offset + sizeof(bitmap);
					BYTE* mask = output + offset + entry.size;
					BOOL hasAlpha = FALSE;
					for(UINT pixel = 0; pixel < width * height; pixel++)
					{
						if(pixels[pixel * 4 + 3])
						{
							hasAlpha = TRUE;
							break;
						}
					}
					// All-zero alpha is also used by legacy opaque BGRX icons.
					// With no mask or alpha information, retain an opaque bitmap.
					if(hasAlpha)
					{
						for(UINT y = 0; y < height; y++)
						{
							for(UINT x = 0; x < width; x++)
							{
								if(!pixels[(y * width + x) * 4 + 3])
								{
									mask[y * stride + x / 8] |= 0x80 >> (x % 8);
								}
							}
						}
					}
					entry.size += missing[n];
				}
				entry.offset = offset;
				CopyMemory(output + sizeof(directory) + n * sizeof(entry),&entry,sizeof(entry));
				offset += entry.size;
			}
		}
		normalized.Swap(result);
	Cleanup:
		return hr;
	}
	static HRESULT DecodeIcon(IJSONValue* value, CMemoryStream& bytes)
	{
		HRESULT hr;
		TStackRef<IJSONObject> object;
		CRString format;
		CRString encoding;
		CRString data;
		CMemoryStream decoded;
		CMemoryStream encoded;
		PSTR ascii = NULL;
		CheckIf(!value,E_INVALIDARG);
		Check(value->GetObject(&object));
		CheckIf(object->Count() != 3,E_INVALIDARG);
		Check(ReadString(object,L"format",L"",&format));
		Check(ReadString(object,L"encoding",L"",&encoding));
		Check(ReadString(object,L"data",L"",&data));
		CheckIf(!SameString(format,RSTRING_CAST(L"ico")) || !SameString(encoding,RSTRING_CAST(L"base64")),E_INVALIDARG);
		INT length = RStrLen(data);
		CheckIf(length < 4 || length > static_cast<INT>((MaxIconBytes + 2) / 3 * 4) || length % 4,E_INVALIDARG);
		ascii = __new CHAR[length];
		CheckAlloc(ascii);
		for(INT n = 0; n < length; n++)
		{
			CheckIf(data.c_wstr()[n] > 127,E_INVALIDARG);
			ascii[n] = static_cast<CHAR>(data.c_wstr()[n]);
		}
		Check(ScDecodeBase64A(ascii,length,&decoded,NULL));
		// Check canonical Base64 against the original bytes, before any repairs.
		Check(ScEncodeBase64A(decoded.GetReadPtr(),decoded.DataRemaining(),&encoded,NULL));
		CheckIf(encoded.DataRemaining() != length || memcmp(ascii,encoded.GetReadPtr(),length),E_INVALIDARG);
		Check(NormalizeICO(decoded.GetReadPtr(),decoded.DataRemaining(),bytes));
	Cleanup:
		SafeDeleteArray(ascii);
		return hr;
	}
	static HRESULT EncodeIcon(const BYTE* bytes, UINT size, IJSONValue** value)
	{
		HRESULT hr;
		CMemoryStream stream;
		TStackRef<IJSONObject> object;
		CRString text;
		PWSTR write;
		Check(ScEncodeBase64A(bytes,size,&stream,NULL));
		Check(RStrAllocW(stream.DataRemaining(),&text,&write));
		for(UINT n = 0; n < stream.DataRemaining(); n++)
		{
			write[n] = stream.GetReadPtr()[n];
		}
		Check(JSONCreateObject(&object));
		Check(PutString(object,L"format",RSTRING_CAST(L"ico")));
		Check(PutString(object,L"encoding",RSTRING_CAST(L"base64")));
		Check(PutString(object,L"data",text));
		Check(JSONWrapObject(object,value));
	Cleanup:
		return hr;
	}
	HRESULT NormalizeIcon(IJSONValue* value, IJSONValue** normalized)
	{
		HRESULT hr = S_OK;
		CMemoryStream bytes;
		CheckIf(!normalized,E_POINTER);
		*normalized = NULL;
		if(value)
		{
			Check(DecodeIcon(value,bytes));
			Check(EncodeIcon(bytes.GetReadPtr(),bytes.DataRemaining(),normalized));
		}
	Cleanup:
		return hr;
	}
}

HRESULT WINAPI QuadooCreateIconValue (const BYTE* bytes, UINT size, IJSONValue** value)
{
	HRESULT hr;
	CMemoryStream normalized;
	CheckIf(!value,E_POINTER);
	*value = NULL;
	Check(QF::NormalizeICO(bytes,size,normalized));
	Check(QF::EncodeIcon(normalized.GetReadPtr(),normalized.DataRemaining(),value));
Cleanup:
	return hr;
}

HRESULT WINAPI QuadooLoadIconValue (IJSONValue* value, UINT width, UINT height, HICON* icon)
{
	HRESULT hr = S_OK;
	CMemoryStream bytes;
	CMemoryStream resource;
	QF::IconDirectory directory; QF::IconEntry selected;
	ULONG written;
	UINT score = UINT_MAX;
	CheckIf(!icon,E_POINTER); *icon = NULL; CheckIf(!width || !height || width > 256 || height > 256,E_INVALIDARG);
	if(!value) goto Cleanup;
	Check(QF::DecodeIcon(value,bytes)); CopyMemory(&directory,bytes.GetReadPtr(),sizeof(directory));
	for(UINT n = 0; n < directory.count; n++)
	{
		QF::IconEntry entry; CopyMemory(&entry,bytes.GetReadPtr() + sizeof(directory) + n * sizeof(entry),sizeof(entry));
		INT cx = entry.width ? entry.width : 256, cy = entry.height ? entry.height : 256;
		UINT distance = (abs(cx - static_cast<INT>(width)) + abs(cy - static_cast<INT>(height))) * 64 + 32 - min(32,entry.bits);
		if(distance < score) { selected = entry; score = distance; }
	}
	// ICO offsets need not be DWORD aligned; the resource loader requires it.
	Check(resource.Write(bytes.GetReadPtr() + selected.offset,selected.size,&written));
	*icon = CreateIconFromResourceEx(const_cast<PBYTE>(resource.GetReadPtr()),selected.size,TRUE,0x00030000,width,height,LR_DEFAULTCOLOR);
	CheckIf(!*icon,HrEnsureLastError());
Cleanup:
	return hr;
}
