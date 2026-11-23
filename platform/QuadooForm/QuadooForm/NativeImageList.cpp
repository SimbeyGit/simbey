#include "Runtime.h"
#include "NativeImageList.h"
#include <shlwapi.h>

namespace QF
{
	static BOOL StraightImageListInput()
	{
		HMODULE module = NULL;
		DLLVERSIONINFO version = {sizeof(version)};
		if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<PCWSTR>(&ImageList_Add),&module))
		{
			HRESULT (CALLBACK* getVersion)(DLLVERSIONINFO*) = reinterpret_cast<HRESULT (CALLBACK*)(DLLVERSIONINFO*)>(GetProcAddress(module,"DllGetVersion"));
			if(getVersion && SUCCEEDED(getVersion(&version)))
			{
				return version.dwMajorVersion >= 6;
			}
		}
		return FALSE;
	}
	HRESULT AppendNativeImage(HIMAGELIST images, const QFI::Raster& pixels, INT index)
	{
		HRESULT hr;
		UINT width = pixels.width;
		UINT height = pixels.height;
		BITMAPINFO info = {0};
		HBITMAP bitmap = NULL;
		HBITMAP mask = NULL;
		BYTE* bits = NULL;
		BYTE* maskBits = NULL;
		UINT stride = ((width + 15) / 16) * 2;
		info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		info.bmiHeader.biWidth = width;
		info.bmiHeader.biHeight = -static_cast<LONG>(height);
		info.bmiHeader.biPlanes = 1;
		info.bmiHeader.biBitCount = 32;
		info.bmiHeader.biCompression = BI_RGB;
		bitmap = CreateDIBSection(NULL,&info,DIB_RGB_COLORS,reinterpret_cast<PVOID*>(&bits),NULL,0);
		CheckIf(!bitmap,E_OUTOFMEMORY);
		// Common-controls v6 premultiplies ImageList_Add's 32-bit DIB input.
		// QFI rasters are already premultiplied; supply straight BGRA here so
		// translucent edges are not multiplied a second time.
		CopyMemory(bits,pixels.pixels,width * height * 4);
		if(StraightImageListInput())
		{
			for(UINT n = 0; n < width * height; n++)
			{
				UINT alpha = bits[n * 4 + 3];
				for(UINT channel = 0; channel < 3; channel++)
				{
					bits[n * 4 + channel] = alpha ? static_cast<BYTE>(min(255,(bits[n * 4 + channel] * 255 + alpha / 2) / alpha)) : 0;
				}
			}
		}
		maskBits = static_cast<BYTE*>(QFI::Allocate(stride * height));
		CheckAlloc(maskBits);
		ZeroMemory(maskBits,stride * height);
		for(UINT y = 0; y < height; y++)
		{
			for(UINT x = 0; x < width; x++)
			{
				if(pixels.pixels[(y * width + x) * 4 + 3] == 0)
				{
					maskBits[y * stride + x / 8] |= 0x80 >> (x % 8);
				}
			}
		}
		mask = CreateBitmap(width,height,1,1,maskBits);
		CheckIf(!mask,E_OUTOFMEMORY);
		CheckIf(ImageList_Add(images,bitmap,mask) != static_cast<INT>(index),E_OUTOFMEMORY);
		hr = S_OK;
	Cleanup:
		if(bitmap)
		{
			DeleteObject(bitmap);
		}
		if(mask)
		{
			DeleteObject(mask);
		}
		QFI::Free(maskBits);
		return hr;
	}
	class CNativeImages : public CBaseUnknown, public IQuadooImageListSnapshot
	{
		HIMAGELIST m_images;
		UINT m_width;
		UINT m_height;
		UINT m_dpi;
		UINT m_thread;
		IUnknown* m_factory;
		UINT* m_live;
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooImageListSnapshot)
		END_UNK_MAP
		CNativeImages(UINT width, UINT height, UINT dpi, IUnknown* factory, UINT* live) :
			m_images(NULL), m_width(width), m_height(height), m_dpi(dpi),
			m_thread(GetCurrentThreadId()), m_live(live)
		{
			SetInterface(m_factory,factory);
			if(m_live)
			{
				++*m_live;
			}
		}
		~CNativeImages()
		{
			if(m_images)
			{
				ImageList_Destroy(m_images);
			}
			if(m_live)
			{
				--*m_live;
			}
			SafeRelease(m_factory);
		}
		HRESULT Initialize(QFI::CImageListData* data)
		{
			HRESULT hr;
			CheckIf(static_cast<ULONGLONG>(m_width) * m_height * max(1,data->Count()) > 16 * 1024 * 1024,E_OUTOFMEMORY);
			m_images = ImageList_Create(m_width,m_height,ILC_COLOR32 | ILC_MASK,max(1,data->Count()),1);
			CheckIf(!m_images,E_OUTOFMEMORY);
			for(UINT n = 0; n < data->Count(); n++)
			{
				Check(Append(data,n));
			}
			hr = S_OK;
		Cleanup:
			return hr;
		}
		HRESULT Append(QFI::CImageListData* data, UINT index)
		{
			HRESULT hr;
			QFI::Raster pixels;
			Check(data->Render(index,pixels,m_width,m_height));
			Check(AppendNativeImage(m_images,pixels,index));
		Cleanup:
			return hr;
		}
		STDMETHOD(GetHandle)(HIMAGELIST* images)
		{
			HRESULT hr = S_OK;
			CheckIf(!images,E_POINTER);
			*images = NULL;
			CheckIf(m_thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
			*images = m_images;
		Cleanup:
			return hr;
		}
		STDMETHOD(GetSize)(UINT* width, UINT* height, UINT* dpi)
		{
			HRESULT hr = S_OK;
			CheckIf(!width || !height || !dpi,E_POINTER);
			CheckIf(m_thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
			*width = m_width;
			*height = m_height;
			*dpi = m_dpi;
		Cleanup:
			return hr;
		}
	};
	HRESULT BuildNativeImageList(QFI::CImageListData* data, UINT dpi, IUnknown* factory,
		UINT* live, IQuadooImageListSnapshot** result)
	{
		HRESULT hr;
		TStackRef<CNativeImages> images;
		CheckIf(!result,E_POINTER);
		*result = NULL;
		CheckIf(!data || dpi < 48 || dpi > 768,E_INVALIDARG);
		images.Attach(__new CNativeImages(max(1,MulDiv(data->width,dpi,96)),
			max(1,MulDiv(data->height,dpi,96)),dpi,factory,live));
		CheckAlloc(images);
		Check(images->Initialize(data));
		*result = images.Detach();
	Cleanup:
		return hr;
	}
}
