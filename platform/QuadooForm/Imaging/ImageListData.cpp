#include "ImageListData.h"

namespace QFI
{
	PCWSTR ImageListSizingName(UINT sizing)
	{
		static PCWSTR const names[] = { L"Crop",L"Stretch",L"Center",L"Contain" };
		return sizing < ARRAYSIZE(names) ? names[sizing] : L"";
	}
	HRESULT ValidateImageListOptions(UINT width, UINT height, UINT depth)
	{
		HRESULT hr = S_OK;
		CheckIf(!width || !height || width > MaxImageListCell || height > MaxImageListCell,E_INVALIDARG);
		CheckIf(depth != 4 && depth != 8 && depth != 16 && depth != 24 && depth != 32,E_INVALIDARG);
	Cleanup:
		return hr;
	}
	CImageListData::CImageListData() : m_bytes(0), m_pixels(0), width(16), height(16), depth(24), masked(TRUE)
	{
	}
	CImageListData::~CImageListData()
	{
		Clear();
	}
	UINT CImageListData::Count() const
	{
		return static_cast<UINT>(m_entries.Length());
	}
	VOID CImageListData::Clear()
	{
		for(sysint n = 0; n < m_entries.Length(); n++)
		{
			m_entries[n]->definition->Release();
			m_entries[n]->image->Release();
			SafeRelease(m_entries[n]->keyed);
			delete m_entries[n];
		}
		m_entries.Clear();
		m_bytes = 0;
		m_pixels = 0;
	}
	HRESULT CImageListData::Configure(UINT cx, UINT cy, UINT colorDepth, BOOL mask)
	{
		HRESULT hr;
		Check(ValidateImageListOptions(cx,cy,colorDepth));
		width = cx;
		height = cy;
		depth = colorDepth;
		masked = mask;
	Cleanup:
		return hr;
	}
	HRESULT CImageListData::Create(IJSONValue* images, UINT cx, UINT cy, UINT colorDepth, BOOL mask, CImageListData** result)
	{
		HRESULT hr;
		TStackRef<CImageListData> candidate;
		TStackRef<IJSONArray> array;
		CheckIf(!result,E_POINTER);
		*result = NULL;
		candidate.Attach(__new CImageListData);
		CheckAlloc(candidate);
		Check(candidate->Configure(cx,cy,colorDepth,mask));
		if(images)
		{
			Check(images->GetArray(&array));
			CheckIf(array->Count() > MaxImageListCount,E_INVALIDARG);
			for(sysint n = 0; n < array->Count(); n++)
			{
				TStackRef<IJSONValue> entry;
				Check(array->GetValue(n,&entry));
				Check(candidate->Append(entry));
			}
		}
		*result = candidate.Detach();
	Cleanup:
		return hr;
	}
	HRESULT ImageListEntry(IJSONValue* png, UINT sizing, IJSONValue** entry)
	{
		HRESULT hr;
		TStackRef<IJSONValue> copy;
		TStackRef<IJSONValue> mode;
		TStackRef<IJSONObject> object;
		CheckIf(!png || !entry || sizing > ImageContain,E_INVALIDARG);
		*entry = NULL;
		Check(JSONClone(png,&copy,TRUE));
		Check(copy->GetObject(&object));
		static const UINT lengths[] = {StaticLength(L"Crop"),StaticLength(L"Stretch"),StaticLength(L"Center"),StaticLength(L"Contain")};
		Check(JSONCreateStringW(ImageListSizingName(sizing),lengths[sizing],&mode));
		Check(object->AddValueW(L"sizing",mode));
		*entry = copy.Detach();
	Cleanup:
		return hr;
	}
	HRESULT CImageListData::Append(IJSONValue* value)
	{
		HRESULT hr;
		TStackRef<IJSONObject> object;
		TStackRef<IJSONObject> source;
		TStackRef<IJSONValue> field;
		TStackRef<IJSONValue> wrapped;
		TStackRef<IJSONValue> saved;
		TStackRef<CImageResource> image;
		TStackRef<CImageResource> keyed;
		CRString format;
		UINT sizing = ImageContain;
		COLORREF maskColor = CLR_INVALID;
		bool colorKey = false;
		Entry* entry = NULL;
		CheckIf(!value || Count() >= MaxImageListCount,E_INVALIDARG);
		Check(value->GetObject(&object));
		CheckIf(object->Count() != 3 + object->HasField(RSTRING_CAST(L"sizing")) +
			object->HasField(RSTRING_CAST(L"colorKey")) + object->HasField(RSTRING_CAST(L"maskColor")),E_INVALIDARG);
		if(object->HasField(RSTRING_CAST(L"colorKey")))
		{
			Check(object->FindNonNullValueW(L"colorKey",&field));
			Check(field->GetBoolean(&colorKey));
			field.Release();
		}
		if(object->HasField(RSTRING_CAST(L"maskColor")))
		{
			INT color;
			Check(object->FindNonNullValueW(L"maskColor",&field));
			Check(field->GetInteger(&color));
			CheckIf(color < 0 || color > 0xffffff,E_INVALIDARG);
			maskColor = static_cast<COLORREF>(color);
			field.Release();
		}
		if(object->HasField(RSTRING_CAST(L"sizing")))
		{
			CRString mode;
			Check(object->FindNonNullValueW(L"sizing",&field));
			Check(field->GetString(&mode));
			field.Release();
			for(sizing = 0; sizing <= ImageContain; sizing++)
			{
				if(!lstrcmpW(mode.c_wstr(),ImageListSizingName(sizing)))
				{
					break;
				}
			}
			CheckIf(sizing > ImageContain,E_INVALIDARG);
		}
		Check(JSONCreateObject(&source));
		static PCWSTR const fields[] = { L"format",L"encoding",L"data" };
		for(UINT n = 0; n < ARRAYSIZE(fields); n++)
		{
			Check(object->FindNonNullValueW(fields[n],&field));
			Check(source->AddValueW(fields[n],field));
			field.Release();
		}
		Check(source->FindNonNullValueW(L"format",&field));
		Check(field->GetString(&format));
		CheckIf(!(format == RSTRING_CAST(L"png")),E_INVALIDARG);
		Check(JSONWrapObject(source,&wrapped));
		Check(CImageResource::Create(wrapped,&image));
		CheckIf(image->ByteSize() > MaxBytes - m_bytes || image->Width() * image->Height() > MaxPixels - m_pixels,E_INVALIDARG);
		if(colorKey)
		{
			Check(image->ColorKeyPNG(maskColor,&keyed));
		}
		Check(JSONClone(value,&saved,TRUE));
		entry = __new Entry;
		CheckAlloc(entry);
		entry->sizing = sizing;
		Check(m_entries.Append(entry));
		entry->definition = saved.Detach();
		entry->image = image.Detach();
		entry->keyed = keyed.Detach();
		entry->colorKey = colorKey;
		m_bytes += entry->image->ByteSize();
		m_pixels += entry->image->Width() * entry->image->Height();
		entry = NULL;
	Cleanup:
		delete entry;
		return hr;
	}
	HRESULT CImageListData::Images(IJSONValue** images)
	{
		HRESULT hr;
		TStackRef<IJSONArray> array;
		CheckIf(!images,E_POINTER);
		*images = NULL;
		Check(JSONCreateArray(&array));
		for(sysint n = 0; n < m_entries.Length(); n++)
		{
			TStackRef<IJSONValue> copy;
			Check(JSONClone(m_entries[n]->definition,&copy,TRUE));
			Check(array->Add(copy));
		}
		Check(JSONWrapArray(array,images));
	Cleanup:
		return hr;
	}
	BOOL CImageListData::ColorKeyEnabled() const
	{
		for(sysint n = 0; n < m_entries.Length(); n++)
		{
			if(!m_entries[n]->colorKey)
			{
				return FALSE;
			}
		}
		return m_entries.Length() != 0;
	}
	HRESULT CImageListData::SetColorKey(BOOL enabled)
	{
		HRESULT hr;
		TStackRef<CImageListData> candidate;
		BOOL changed = FALSE;
		for(sysint n = 0; n < m_entries.Length(); n++)
		{
			if(m_entries[n]->colorKey != (enabled != FALSE))
			{
				changed = TRUE;
				break;
			}
		}
		CheckIfIgnore(!changed,S_FALSE);
		Check(Create(NULL,width,height,depth,masked,&candidate));
		for(sysint n = 0; n < m_entries.Length(); n++)
		{
			TStackRef<IJSONValue> copy;
			TStackRef<IJSONObject> object;
			TStackRef<IJSONValue> value;
			Check(JSONClone(m_entries[n]->definition,&copy,TRUE));
			Check(copy->GetObject(&object));
			if(enabled)
			{
				Check(JSONCreateBool(true,&value));
				Check(object->AddValueW(L"colorKey",value));
			}
			else if(object->HasField(RSTRING_CAST(L"colorKey")))
			{
				Check(object->RemoveValueW(L"colorKey"));
			}
			Check(candidate->Append(copy));
		}
		m_entries.Swap(candidate->m_entries);
		UINT bytes = m_bytes;
		m_bytes = candidate->m_bytes;
		candidate->m_bytes = bytes;
		UINT pixels = m_pixels;
		m_pixels = candidate->m_pixels;
		candidate->m_pixels = pixels;
	Cleanup:
		return hr;
	}
	HRESULT CImageListData::Move(UINT from, UINT to)
	{
		HRESULT hr = S_OK;
		CheckIf(from >= Count() || to >= Count(),E_INVALIDARG);
		Entry* entry = m_entries[from];
		if(from < to)
		{
			for(UINT n = from; n < to; n++)
			{
				m_entries[n] = m_entries[n + 1];
			}
		}
		else
		{
			for(UINT n = from; n > to; n--)
			{
				m_entries[n] = m_entries[n - 1];
			}
		}
		m_entries[to] = entry;
	Cleanup:
		return hr;
	}
	HRESULT CImageListData::Remove(UINT index)
	{
		HRESULT hr = S_OK;
		CheckIf(index >= Count(),E_INVALIDARG);
		Entry* entry = m_entries[index];
		m_bytes -= entry->image->ByteSize();
		m_pixels -= entry->image->Width() * entry->image->Height();
		entry->definition->Release();
		entry->image->Release();
		SafeRelease(entry->keyed);
		delete entry;
		m_entries.Remove(index,NULL);
	Cleanup:
		return hr;
	}
	HRESULT CImageListData::SourceSize(UINT index, UINT* cx, UINT* cy, UINT* sizing)
	{
		HRESULT hr = S_OK;
		CheckIf(index >= Count() || !cx || !cy || !sizing,E_INVALIDARG);
		*cx = m_entries[index]->image->Width();
		*cy = m_entries[index]->image->Height();
		*sizing = m_entries[index]->sizing;
	Cleanup:
		return hr;
	}
	static BYTE Quantize(BYTE value, UINT bits)
	{
		UINT levels = (1U << bits) - 1;
		return static_cast<BYTE>(((value * levels + 127) / 255) * 255 / levels);
	}
	HRESULT CImageListData::Render(UINT index, Raster& pixels, UINT cx, UINT cy)
	{
		HRESULT hr;
		Raster candidate;
		UINT width = cx ? cx : this->width;
		UINT height = cy ? cy : this->height;
		CheckIf(index >= Count(),E_INVALIDARG);
		UINT sizing = m_entries[index]->sizing;
		if((sizing == ImageCrop || sizing == ImageCenter) && (width != this->width || height != this->height))
		{
			// Crop and Center operate in logical pixels. Scale their completed
			// logical cell, preserving the crop/centering at higher device DPI.
			Raster logical;
			Check(Render(index,logical));
			Check(candidate.Allocate(width,height));
			for(UINT y = 0; y < height; y++)
			{
				for(UINT x = 0; x < width; x++)
				{
					CopyMemory(candidate.pixels + (y * width + x) * 4,
						logical.pixels + ((y * logical.height / height) * logical.width + x * logical.width / width) * 4,4);
				}
			}
			pixels.Swap(candidate);
			hr = S_OK;
			goto Cleanup;
		}
		UINT mode = sizing == ImageStretch ? Stretch : sizing == ImageContain ? Contain : Normal;
		INT x = 0;
		INT y = 0;
		if(sizing == ImageCenter)
		{
			x = static_cast<INT>(width) - static_cast<INT>(m_entries[index]->image->Width());
			y = static_cast<INT>(height) - static_cast<INT>(m_entries[index]->image->Height());
			x = (x - (x < 0 ? 1 : 0)) / 2;
			y = (y - (y < 0 ? 1 : 0)) / 2;
		}
		CImageResource* image = m_entries[index]->keyed ? m_entries[index]->keyed : m_entries[index]->image;
		Check(image->RenderPixels(width,height,mode,sizing == ImageContain ? 4 : 0,candidate,x,y));
		if(depth != 32)
		{
			static const COLORREF colors[] = { RGB(0,0,0),RGB(128,0,0),RGB(0,128,0),RGB(128,128,0),RGB(0,0,128),RGB(128,0,128),RGB(0,128,128),RGB(192,192,192),RGB(128,128,128),RGB(255,0,0),RGB(0,255,0),RGB(255,255,0),RGB(0,0,255),RGB(255,0,255),RGB(0,255,255),RGB(255,255,255) };
			for(UINT n = 0; n < width * height; n++)
			{
				BYTE* pixel = candidate.pixels + n * 4;
				UINT alpha = pixel[3];
				if(masked && alpha < 128)
				{
					ZeroMemory(pixel,4);
					continue;
				}
				for(UINT c = 0; c < 3; c++)
				{
					pixel[c] = masked ? static_cast<BYTE>(alpha ? min(255,(pixel[c] * 255 + alpha / 2) / alpha) : 0) : static_cast<BYTE>(min(255,pixel[c] + 255 - alpha));
				}
				pixel[3] = 255;
				if(depth == 4)
				{
					UINT best = UINT_MAX;
					COLORREF color = 0;
					for(UINT k = 0; k < ARRAYSIZE(colors); k++)
					{
						INT r = pixel[2] - GetRValue(colors[k]);
						INT g = pixel[1] - GetGValue(colors[k]);
						INT b = pixel[0] - GetBValue(colors[k]);
						UINT distance = r * r + g * g + b * b;
						if(distance < best)
						{
							best = distance;
							color = colors[k];
						}
					}
					pixel[0] = GetBValue(color);
					pixel[1] = GetGValue(color);
					pixel[2] = GetRValue(color);
				}
				else if(depth == 8 || depth == 16)
				{
					pixel[0] = Quantize(pixel[0],depth == 8 ? 2 : 5);
					pixel[1] = Quantize(pixel[1],depth == 8 ? 3 : 6);
					pixel[2] = Quantize(pixel[2],depth == 8 ? 3 : 5);
				}
			}
		}
		pixels.Swap(candidate);
	Cleanup:
		return hr;
	}
}
