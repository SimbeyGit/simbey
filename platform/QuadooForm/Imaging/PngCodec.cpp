#include "ImageResource.h"
#include <math.h>
#include <string.h>

namespace QFI
{
static volatile LONG s_failure = -1;
static volatile LONG s_allocated;
static volatile LONG s_encodeTLS = -1;
class CEncodeAllocations
{
	DWORD m_slot;
	PVOID m_previous;
	BOOL m_entered;
public:
	BOOL failed;
	CEncodeAllocations() : m_slot(TLS_OUT_OF_INDEXES), m_previous(NULL), m_entered(FALSE), failed(FALSE)
	{
	}
	~CEncodeAllocations()
	{
		if(m_entered)
		{
			TlsSetValue(m_slot,m_previous);
		}
	}
	HRESULT Initialize()
	{
		HRESULT hr = S_OK;
		LONG slot = InterlockedCompareExchange(&s_encodeTLS,-1,-1);
		if(slot == -1)
		{
			DWORD candidate = TlsAlloc();
			CheckIf(candidate == TLS_OUT_OF_INDEXES,E_OUTOFMEMORY);
			slot = InterlockedCompareExchange(&s_encodeTLS,static_cast<LONG>(candidate),-1);
			if(slot == -1)
			{
				slot = static_cast<LONG>(candidate);
			}
			else
			{
				TlsFree(candidate);
			}
		}
		m_slot = static_cast<DWORD>(slot);
		m_previous = TlsGetValue(m_slot);
		CheckIf(!TlsSetValue(m_slot,this),HrEnsureLastError());
		m_entered = TRUE;
	Cleanup:
		return hr;
	}
};
static PVOID AllocationFailed()
{
	LONG slot = InterlockedCompareExchange(&s_encodeTLS,-1,-1);
	if(slot != -1)
	{
		CEncodeAllocations* scope = static_cast<CEncodeAllocations*>(TlsGetValue(static_cast<DWORD>(slot)));
		if(scope)
		{
			scope->failed = TRUE;
		}
	}
	return NULL;
}
VOID ReleaseCodecTLS()
{
	LONG slot = InterlockedExchange(&s_encodeTLS,-1);
	if(slot != -1)
	{
		TlsFree(static_cast<DWORD>(slot));
	}
}
LONG AllocatedBytes()
{
	return InterlockedCompareExchange(&s_allocated,0,0);
}
VOID FailAllocationAfter(LONG count) { InterlockedExchange(&s_failure,count); }
PVOID Allocate(SIZE_T size)
{
	LONG countdown = InterlockedCompareExchange(&s_failure,-1,-1);
	while(countdown >= 0)
	{
		LONG next = countdown ? countdown - 1 : -1;
		LONG previous = InterlockedCompareExchange(&s_failure,next,countdown);
		if(previous == countdown)
		{
			if(!countdown)
			{
				return AllocationFailed();
			}
			break;
		}
		countdown = previous;
	}
	if(!size)
	{
		return NULL;
	}
	if(size > 80 * 1024 * 1024)
	{
		return AllocationFailed();
	}
	LONG used = InterlockedExchangeAdd(&s_allocated,static_cast<LONG>(size));
	if(used > 96 * 1024 * 1024 - static_cast<LONG>(size))
	{
		InterlockedExchangeAdd(&s_allocated,-static_cast<LONG>(size));
		return AllocationFailed();
	}
	SIZE_T* memory = static_cast<SIZE_T*>(HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(SIZE_T) + size));
	if(!memory)
	{
		InterlockedExchangeAdd(&s_allocated,-static_cast<LONG>(size));
		return AllocationFailed();
	}
	*memory = size;
	return memory + 1;
}
VOID Free(PVOID memory)
{
	if(memory)
	{
		SIZE_T* header = static_cast<SIZE_T*>(memory) - 1;
		InterlockedExchangeAdd(&s_allocated,-static_cast<LONG>(*header));
		HeapFree(GetProcessHeap(),0,header);
	}
}
static PVOID Reallocate(PVOID memory, SIZE_T size)
{
	if(!size) { Free(memory); return NULL; }
	PVOID result = Allocate(size);
	if(result && memory) { CopyMemory(result,memory,min(size,*(static_cast<SIZE_T*>(memory) - 1))); Free(memory); }
	return result;
}
}
#define LODEPNG_NO_COMPILE_ANCILLARY_CHUNKS
#define malloc QFI::Allocate
#define realloc QFI::Reallocate
#define free QFI::Free
// The bundled C codec uses unsigned sizes internally. Imaging allocations and
// input/output dimensions are bounded well below UINT_MAX by this wrapper.
#pragma warning(push)
#pragma warning(disable:4267)
namespace QFPNG
{
	#include "3rdparty\lodepng\lodepng.cpp"
}
#pragma warning(pop)
#undef malloc
#undef realloc
#undef free

namespace QFI
{
static HRESULT PngError(UINT error)
{
	return !error ? S_OK : error == 83 ? E_OUTOFMEMORY : HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
}
static UINT PassSize(UINT size, UINT start, UINT step) { return size <= start ? 0 : (size - start + step - 1) / step; }
static HRESULT CheckScanlines(const BYTE* bytes, UINT size, const QFPNG::LodePNGState* state, UINT width, UINT height)
{
	HRESULT hr;
	CMemoryStream compressed; ULONG written;
	BYTE* inflated = NULL; lodepng_size_t inflatedSize = 0;
	UINT bits = QFPNG::lodepng_get_bpp(&state->info_png.color), expected = 0;
	BOOL ended = FALSE;
	if(state->info_png.interlace_method)
	{
		static const UINT x[] = { 0,4,0,2,0,1,0 }, y[] = { 0,0,4,0,2,0,1 };
		static const UINT dx[] = { 8,8,4,4,2,2,1 }, dy[] = { 8,8,8,4,4,2,2 };
		for(UINT pass = 0; pass < 7; pass++)
		{
			UINT w = PassSize(width,x[pass],dx[pass]), h = PassSize(height,y[pass],dy[pass]);
			if(w && h) expected += ((w * bits + 7) / 8 + 1) * h;
		}
	}
	else expected = ((width * bits + 7) / 8 + 1) * height;
	for(UINT offset = 33; offset < size; )
	{
		CheckIf(size - offset < 12,HRESULT_FROM_WIN32(ERROR_INVALID_DATA));
		UINT length = QFPNG::lodepng_chunk_length(bytes + offset);
		CheckIf(length > size - offset - 12,HRESULT_FROM_WIN32(ERROR_INVALID_DATA));
		// The old codec skips CRC checks for ignored ancillary chunks. A Picture
		// still has to be a valid PNG even when its metadata is not interpreted.
		CheckIf(QFPNG::lodepng_chunk_check_crc(bytes + offset),HRESULT_FROM_WIN32(ERROR_INVALID_DATA));
		if(QFPNG::lodepng_chunk_type_equals(bytes + offset,"IDAT"))
		{
			Check(compressed.Write(bytes + offset + 8,length,&written));
		}
		if(QFPNG::lodepng_chunk_type_equals(bytes + offset,"IEND"))
		{
			CheckIf(length || offset + 12 != size,HRESULT_FROM_WIN32(ERROR_INVALID_DATA));
			ended = TRUE;
			break;
		}
		offset += length + 12;
	}
	CheckIf(!ended || !compressed.DataRemaining(),HRESULT_FROM_WIN32(ERROR_INVALID_DATA));
	Check(PngError(QFPNG::lodepng_zlib_decompress(&inflated,&inflatedSize,compressed.GetReadPtr(),compressed.DataRemaining(),&state->decoder.zlibsettings)));
	// This bundled decoder predates an exact scanline-length check. Validate
	// before its pixel conversion, including Adam7 row padding and filter bytes.
	CheckIf(inflatedSize != expected,HRESULT_FROM_WIN32(ERROR_INVALID_DATA));
Cleanup:
	QFI::Free(inflated);
	return hr;
}
HRESULT DecodePNG(const BYTE* bytes, UINT size, CMemoryStream& pixels, UINT* width, UINT* height)
{
	HRESULT hr;
	QFPNG::LodePNGState state; BYTE* decoded = NULL; UINT w = 0, h = 0; ULONG written;
	QFPNG::lodepng_state_init(&state);
	CheckIf(!bytes || !width || !height,E_INVALIDARG);
	*width = *height = 0;
	CheckIf(size > MaxBytes,HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE));
	// The bundled inspect routine reads the IHDR CRC after checking only 29
	// bytes. Require the complete, fixed-size IHDR before calling it.
	CheckIf(size < 33 || bytes[8] || bytes[9] || bytes[10] || bytes[11] != 13,HRESULT_FROM_WIN32(ERROR_INVALID_DATA));
	Check(PngError(QFPNG::lodepng_inspect(&w,&h,&state,bytes,size)));
	CheckIf(!w || !h || w > MaxDimension || h > MaxDimension || w > MaxPixels / h,HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED));
	Check(CheckScanlines(bytes,size,&state,w,h));
	Check(PngError(QFPNG::lodepng_decode(&decoded,&w,&h,&state,bytes,size)));
	Check(pixels.Write(decoded,w * h * 4,&written));
	*width = w; *height = h;
Cleanup:
	QFI::Free(decoded); QFPNG::lodepng_state_cleanup(&state);
	return hr;
}

HRESULT EncodePNG(const BYTE* rgba, UINT width, UINT height, CMemoryStream& bytes, BOOL optimizeColor)
{
	HRESULT hr;
	BYTE* encoded = NULL;
	lodepng_size_t length = 0;
	ULONG written;
	CEncodeAllocations allocations;
	Check(allocations.Initialize());
	CheckIf(!rgba || !width || !height || width > MaxDimension || height > MaxDimension || width > MaxPixels / height,E_INVALIDARG);
	hr = PngError(QFPNG::lodepng_encode32(&encoded,&length,rgba,width,height,optimizeColor != FALSE));
	// Some legacy void/vector helpers do not propagate allocation errors.
	// Never publish a truncated PNG merely because the encoder returned zero.
	CheckIf(allocations.failed,E_OUTOFMEMORY);
	Check(hr);
	CheckIf(length > MaxBytes,HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE));
	Check(bytes.Write(encoded,static_cast<ULONG>(length),&written));
Cleanup:
	QFI::Free(encoded);
	return hr;
}
}
