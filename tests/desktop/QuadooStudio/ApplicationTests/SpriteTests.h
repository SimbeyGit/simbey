#include "..\..\..\..\platform\QuadooForm\ScriptAdapter\SpriteObjects.h"
#include "..\..\..\TestPaths.h"
#include "..\..\..\..\platform\QuadooForm\ScriptAdapter\SpritePixels.h"
#include "..\..\..\..\platform\QuadooForm\Imaging\ImageSurface.h"
#include "..\..\..\platform\QuadooForm\TestImages\ImageFixtures.h"
#include "Library\Core\LockableStream.h"

static HRESULT SpriteBinary(const BYTE* bytes, UINT count, QuadooVM::QVARIANT* result)
{
	HRESULT hr;
	TStackRef<CMemoryStream> stream;
	TStackRef<CLockableStream> locked;
	ULONG written;
	stream.Attach(__new CMemoryStream);
	CheckAlloc(stream);
	Check(stream->Write(bytes,count,&written));
	locked.Attach(__new CLockableStream(stream,count));
	CheckAlloc(locked);
	Check(QVMCreateBinaryFile(NULL,NULL,locked,0,count,result));
Cleanup:
	return hr;
}

static HRESULT SpriteTemp(const BYTE* bytes, UINT count, WCHAR* path)
{
	HRESULT hr = S_OK;
	WCHAR folder[MAX_PATH];
	HANDLE file = INVALID_HANDLE_VALUE;
	DWORD written;
	Check(TestScratchDirectory(L"QuadooApplication",folder,ARRAYSIZE(folder)));
	CheckIf(!GetTempFileNameW(folder,L"QSP",0,path),HrEnsureLastError());
	file = CreateFileW(path,GENERIC_WRITE,0,NULL,TRUNCATE_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	CheckIf(file == INVALID_HANDLE_VALUE,HrEnsureLastError());
	CheckIf(!WriteFile(file,bytes,count,&written,NULL) || written != count,HrEnsureLastError());
Cleanup:
	if(file != INVALID_HANDLE_VALUE)
	{
		CloseHandle(file);
	}
	return hr;
}

static HRESULT SpriteClipTests()
{
	HRESULT hr;
	TStackRef<AQVM::CSpriteImage> image;
	BYTE source[7 * 5 * 4];
	BYTE target[2 * 20 + 16];
	SIF_SURFACE surface = {target + 8,32,3,2,20};
	static const POINT positions[] =
	{
		{0,0},{-1,0},{2,0},{0,-1},{0,1},{-2,-2},{-6,-4},{3,0},{0,2},
		{-7,0},{0,-5},{-1,1},{2,-1},{INT_MIN,0},{INT_MAX,0},
		{0,INT_MIN},{0,INT_MAX},{INT_MIN,INT_MAX},{1000001,-1000001}
	};
	BOOL changed;
	for(UINT n = 0; n < 7 * 5; n++)
	{
		source[n * 4] = static_cast<BYTE>(n * 5 + 1);
		source[n * 4 + 1] = static_cast<BYTE>(n * 3 + 2);
		source[n * 4 + 2] = static_cast<BYTE>(n * 7 + 3);
		source[n * 4 + 3] = 255;
	}
	image.Attach(__new AQVM::CSpriteImage);
	CheckAlloc(image);
	Check(image->LoadPixels(7,5,source,sizeof(source),AQVM::SpriteBGRA32,0));
	for(UINT n = 0; n < ARRAYSIZE(positions); n++)
	{
		BOOL expectedChange = FALSE;
		FillMemory(target,sizeof(target),0x5a);
		Check(image->Composite(&surface,positions[n].x,positions[n].y,&changed));
		for(INT y = 0; y < 2; y++)
		{
			for(INT x = 0; x < 3; x++)
			{
				LONGLONG sx = static_cast<LONGLONG>(x) - positions[n].x;
				LONGLONG sy = static_cast<LONGLONG>(y) - positions[n].y;
				const BYTE* pixel = surface.pbSurface + y * surface.lPitch + x * 4;
				if(sx >= 0 && sx < 7 && sy >= 0 && sy < 5)
				{
					REQUIRE(!memcmp(pixel,source + (static_cast<UINT>(sy) * 7 + static_cast<UINT>(sx)) * 4,4));
					expectedChange = TRUE;
				}
				else
				{
					REQUIRE(pixel[0] == 0x5a && pixel[1] == 0x5a && pixel[2] == 0x5a && pixel[3] == 0x5a);
				}
			}
			for(UINT padding = 12; padding < 20; padding++)
			{
				REQUIRE(surface.pbSurface[y * surface.lPitch + padding] == 0x5a);
			}
		}
		REQUIRE(changed == expectedChange);
		for(UINT guard = 0; guard < 8; guard++)
		{
			REQUIRE(target[guard] == 0x5a && target[sizeof(target) - guard - 1] == 0x5a);
		}
	}
	{
		// Destination surfaces need not obey the source decoder's 4096/4M caps.
		static const SIZE sizes[] = {{5001,2},{2,5001},{2049,2048}};
		for(UINT n = 0; n < ARRAYSIZE(sizes); n++)
		{
			CMemoryStream storage;
			BYTE* pixels;
			UINT pitch = sizes[n].cx * 4 + 4;
			UINT count = pitch * sizes[n].cy;
			Check(storage.TWriteAdvance<BYTE>(&pixels,count + 16));
			FillMemory(pixels,count + 16,0x5a);
			SIF_SURFACE large = {pixels + 8,32,sizes[n].cx,sizes[n].cy,static_cast<LONG>(pitch)};
			Check(image->Composite(&large,large.xSize - 1,large.ySize - 1,&changed));
			REQUIRE(changed && !memcmp(large.pbSurface + (large.ySize - 1) * pitch + (large.xSize - 1) * 4,source,4));
			REQUIRE(large.pbSurface[(large.ySize - 1) * pitch + large.xSize * 4] == 0x5a);
			REQUIRE(pixels[0] == 0x5a && pixels[count + 15] == 0x5a);
		}
	}
	surface.lPitch = 11;
	REQUIRE(image->Composite(&surface,-1,-1,&changed) == E_INVALIDARG && !changed);
	surface.lPitch = -20;
	REQUIRE(image->Composite(&surface,0,0,&changed) == E_INVALIDARG && !changed);
	surface.lPitch = 20;
	surface.xSize = INT_MAX;
	REQUIRE(image->Composite(&surface,INT_MIN,0,&changed) == E_INVALIDARG && !changed);
	hr = S_OK;
Cleanup:
	return hr;
}

static HRESULT SpritePixelTests()
{
	HRESULT hr;
	LONG baseline = AQVM::SpriteAllocatedBytes();
	TStackRef<AQVM::CSpriteImage> image;
	TStackRef<AQVM::CSpriteImage> decoded;
	CMemoryStream png;
	BYTE rgba[40];
	BYTE target[7 * 64 + 16];
	static const BYTE alpha[] = {0,1,127,254,255};
	static const POINT positions[] = {{2,2},{-2,2},{11,2},{2,-1},{2,6},{-1000000,0},{1000000,0},{0,1000000},{0,-1000000}};
	static const COLORREF backgrounds[] = {RGB(0,0,0),RGB(255,255,255),RGB(17,33,49)};
	SIF_SURFACE surface = {target + 8,32,13,7,64};
	WCHAR path[MAX_PATH] = {0};
	Check(SpriteClipTests());
	image.Attach(__new AQVM::CSpriteImage);
	CheckAlloc(image);
	for(UINT n = 0; n < 10; n++)
	{
		rgba[n * 4] = 203;
		rgba[n * 4 + 1] = 87;
		rgba[n * 4 + 2] = 29;
		rgba[n * 4 + 3] = alpha[n % 5];
	}
	Check(image->LoadPixels(5,2,rgba,sizeof(rgba),AQVM::SpriteRGBA32,0));
	for(UINT background = 0; background < ARRAYSIZE(backgrounds); background++)
	{
		for(UINT position = 0; position < ARRAYSIZE(positions); position++)
		{
			BOOL changed;
			FillMemory(target,sizeof(target),0x5a);
			BYTE base[] = {GetBValue(backgrounds[background]),GetGValue(backgrounds[background]),GetRValue(backgrounds[background])};
			for(UINT y = 0; y < 7; y++)
			{
				for(UINT x = 0; x < 13; x++)
				{
					CopyMemory(surface.pbSurface + y * surface.lPitch + x * 4,base,3);
					surface.pbSurface[y * surface.lPitch + x * 4 + 3] = 255;
				}
			}
			Check(image->Composite(&surface,positions[position].x,positions[position].y,&changed));
			for(INT y = 0; y < 7; y++)
			{
				for(INT x = 0; x < 13; x++)
				{
					INT sx = x - positions[position].x;
					INT sy = y - positions[position].y;
					UINT a = sx >= 0 && sx < 5 && sy >= 0 && sy < 2 ? alpha[sx] : 0;
					const BYTE* pixel = surface.pbSurface + y * surface.lPitch + x * 4;
					REQUIRE(pixel[0] == (29 * a + base[0] * (255 - a) + 127) / 255 &&
						pixel[1] == (87 * a + base[1] * (255 - a) + 127) / 255 &&
						pixel[2] == (203 * a + base[2] * (255 - a) + 127) / 255 && pixel[3] == 255);
				}
				for(UINT byte = 52; byte < 64; byte++)
				{
					REQUIRE(surface.pbSurface[y * surface.lPitch + byte] == 0x5a);
				}
			}
			for(UINT guard = 0; guard < 8; guard++)
			{
				REQUIRE(target[guard] == 0x5a && target[sizeof(target) - guard - 1] == 0x5a);
			}
		}
	}
	Check(QFI::EncodePNG(rgba,5,2,png,FALSE));
	decoded.Attach(__new AQVM::CSpriteImage);
	CheckAlloc(decoded);
	Check(decoded->LoadPng(png.GetReadPtr(),png.DataRemaining()));
	REQUIRE(decoded->width == 5 && decoded->height == 2 && !memcmp(image->Pixels(),decoded->Pixels(),40));
	decoded.Release();
	Check(SpriteTemp(png.GetReadPtr(),png.DataRemaining(),path));
	decoded.Attach(__new AQVM::CSpriteImage);
	CheckAlloc(decoded);
	Check(decoded->LoadFile(RSTRING_CAST(path)));
	REQUIRE(!memcmp(image->Pixels(),decoded->Pixels(),40));
	DeleteFileW(path);
	path[0] = 0;
	decoded.Release();
	Check(SpriteTemp(ImageBMP,sizeof(ImageBMP),path));
	decoded.Attach(__new AQVM::CSpriteImage);
	CheckAlloc(decoded);
	Check(decoded->LoadFile(RSTRING_CAST(path)));
	REQUIRE(decoded->width == 3 && decoded->height == 2 && decoded->Pixels()[2] == 255 && decoded->Pixels()[3] == 255);
	DeleteFileW(path);
	path[0] = 0;
	decoded.Release();
	{
		BYTE icon[sizeof(ImageLegacyICO)];
		CopyMemory(icon,ImageLegacyICO,sizeof(icon));
		icon[73] = 128;
		Check(SpriteTemp(icon,sizeof(icon),path));
		decoded.Attach(__new AQVM::CSpriteImage);
		CheckAlloc(decoded);
		Check(decoded->LoadFile(RSTRING_CAST(path)));
		REQUIRE(decoded->width == 2 && decoded->height == 2 && decoded->Pixels()[2] == 255 && decoded->Pixels()[3] == 128);
		DeleteFileW(path);
		path[0] = 0;
	}
	decoded.Release();
	{
		static const BYTE padded[] = {1,2,3,4,5,6,99,99,7,8,9,10,11,12,99,99};
		decoded.Attach(__new AQVM::CSpriteImage);
		CheckAlloc(decoded);
		Check(decoded->LoadPixels(2,2,padded,sizeof(padded),AQVM::SpriteRGB24,8));
		REQUIRE(decoded->Pixels()[0] == 3 && decoded->Pixels()[2] == 1 && decoded->Pixels()[3] == 255 && decoded->Pixels()[8] == 9);
		decoded.Release();
		decoded.Attach(__new AQVM::CSpriteImage);
		CheckAlloc(decoded);
		REQUIRE(decoded->LoadPixels(2,2,padded,sizeof(padded) - 1,AQVM::SpriteRGB24,8) == E_INVALIDARG);
		REQUIRE(decoded->LoadPixels(2,2,padded,sizeof(padded),AQVM::SpriteRGB24,5) == E_INVALIDARG);
		REQUIRE(FAILED(decoded->LoadPixels(4097,1,padded,sizeof(padded),AQVM::SpriteRGB24,0)));
	}
	decoded.Release();
	for(LONG fault = 0; fault < 20; fault++)
	{
		decoded.Attach(__new AQVM::CSpriteImage);
		CheckAlloc(decoded);
		QFI::FailAllocationAfter(fault);
		hr = decoded->LoadPng(png.GetReadPtr(),png.DataRemaining());
		QFI::FailAllocationAfter(-1);
		REQUIRE(SUCCEEDED(hr) || hr == E_OUTOFMEMORY);
		decoded.Release();
	}
	image.Release();
	REQUIRE(AQVM::SpriteAllocatedBytes() == baseline);
	{
		CMemoryStream source;
		BYTE* pixels;
		TStackRef<AQVM::CSpriteImage> reservations[5];
		Check(source.TWriteAdvance<BYTE>(&pixels,4096 * 1024 * 4));
		ZeroMemory(pixels,source.DataRemaining());
		for(UINT n = 0; n < 5; n++)
		{
			reservations[n].Attach(__new AQVM::CSpriteImage);
			CheckAlloc(reservations[n]);
			hr = reservations[n]->LoadPixels(4096,1024,pixels,source.DataRemaining(),AQVM::SpriteBGRA32,0);
			REQUIRE(n < 4 ? SUCCEEDED(hr) : hr == E_OUTOFMEMORY);
		}
	}
	REQUIRE(AQVM::SpriteAllocatedBytes() == baseline);
	hr = S_OK;
Cleanup:
	QFI::FailAllocationAfter(-1);
	if(path[0])
	{
		DeleteFileW(path);
	}
	return hr;
}

struct SpriteThreadProbe
{
	IQuadooObject* sprite;
	HRESULT result;
};

static DWORD WINAPI SpriteWrongThread(PVOID parameter)
{
	SpriteThreadProbe* probe = static_cast<SpriteThreadProbe*>(parameter);
	QuadooVM::QVARIANT value = {QuadooVM::Null};
	probe->result = probe->sprite->GetProperty(NULL,RSTRING_CAST(L"Width"),&value);
	QVMClearVariant(&value);
	return 0;
}

class CReentrantSpriteBytes : public CBaseUnknown, public CQuadooObjectImpl, public ILockableStream
{
public:
	IQuadooObject* sprite;
	TStackRef<IQuadooControl> destroyOnUnlock;
	HRESULT nested;
	HRESULT destroyed;
	UINT locks;
	UINT unlocks;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooObject)
		UNK_INTERFACE(ILockableStream)
		UNK_INTERFACE(ISequentialStream)
	END_UNK_MAP
	CReentrantSpriteBytes(IQuadooObject* owner) : sprite(owner), nested(S_OK), destroyed(S_OK), locks(0), unlocks(0)
	{
	}
	STDMETHOD(Read)(VOID*, ULONG, ULONG*)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(Write)(const VOID*, ULONG, ULONG*)
	{
		return E_ACCESSDENIED;
	}
	STDMETHOD(Lock)(BYTE** bytes, DWORD* count)
	{
		QuadooVM::QVPARAMS empty = {0};
		QuadooVM::QVARIANT result = {QuadooVM::Null};
		locks++;
		nested = sprite->Invoke(NULL,RSTRING_CAST(L"Clear"),&empty,&result);
		QVMClearVariant(&result);
		*bytes = const_cast<BYTE*>(ImageRGBA);
		*count = sizeof(ImageRGBA);
		return S_OK;
	}
	STDMETHOD_(VOID, Unlock)(VOID)
	{
		unlocks++;
		if(destroyOnUnlock)
		{
			destroyed = destroyOnUnlock->Destroy();
			destroyOnUnlock.Release();
		}
	}
};

static HRESULT SpriteNativeTests()
{
	HRESULT hr;
	TStackRef<IJSONObject> definition;
	TStackRef<IQuadooForm> form;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooCanvas> canvas;
	TStackRef<IQuadooObject> sprite;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooControlFactory> factory;
	TStackRef<IQuadooFactoryLifetime> lifetime;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IQuadooControl> root;
	QuadooForm::ContextOptions options = {0};
	QuadooVM::QVARIANT binary = {QuadooVM::Null};
	QuadooVM::QVARIANT value = {QuadooVM::Null};
	QuadooVM::QVARIANT args[5] = {{QuadooVM::Null},{QuadooVM::Null},{QuadooVM::Null},{QuadooVM::Null},{QuadooVM::Null}};
	QuadooVM::QVPARAMS parameters = {0};
	HANDLE thread = NULL;
	LONG objects = AQVM::SpriteLiveObjects();
	LONG bytes = AQVM::SpriteAllocatedBytes();
	HDC dc;
	HBITMAP bitmap;
	SIF_SURFACE* surface;
	COLORREF color;
	UINT initialLive;
	UINT live;
	static const BYTE pixels[] = {255,0,0,255,0,255,0,127};
	Check(QuadooGetControlRegistry(&registry));
	Check(registry->FindFactory(RSTRING_CAST(L"Canvas"),&factory));
	Check(factory->QueryInterface(&lifetime));
	Check(lifetime->GetLiveCount(&initialLive));
	Check(AQVM::ParseObject(SLP(L"{\"type\":\"Form\",\"name\":\"SpriteHost\",\"children\":[{\"type\":\"Canvas\",\"name\":\"Drawing\",\"properties\":{\"Width\":40,\"Height\":30}}]}"),&definition));
	options.cbSize = sizeof(options);
	options.DPI = 96;
	options.pRegistry = registry;
	Check(QuadooCreateConstructionContext(&options,&context));
	Check(QuadooCreateControl(context,NULL,definition,&root));
	Check(root->QueryInterface(&form));
	root.Release();
	Check(form->FindControl(RSTRING_CAST(L"Drawing"),&control));
	Check(control->QueryInterface(&canvas));
	dc = reinterpret_cast<HDC>(1);
	bitmap = reinterpret_cast<HBITMAP>(1);
	REQUIRE(canvas->GetSurface(&dc,&bitmap,NULL) == E_POINTER && !dc && !bitmap);
	bitmap = reinterpret_cast<HBITMAP>(1);
	surface = reinterpret_cast<SIF_SURFACE*>(1);
	REQUIRE(canvas->GetSurface(NULL,&bitmap,&surface) == E_POINTER && !bitmap && !surface);
	dc = reinterpret_cast<HDC>(1);
	surface = reinterpret_cast<SIF_SURFACE*>(1);
	REQUIRE(canvas->GetSurface(&dc,NULL,&surface) == E_POINTER && !dc && !surface);
	Check(canvas->GetSurface(&dc,&bitmap,&surface));
	REQUIRE(dc && bitmap && surface && surface->xSize == 40 && surface->lPitch == 160);
	Check(AQVM::CreateSpriteObject(canvas,&sprite));
	Check(sprite->GetProperty(NULL,RSTRING_CAST(L"Width"),&value));
	REQUIRE(value.lVal == 0);
	args[0].eType = QuadooVM::I4;
	args[0].lVal = 3;
	args[1].eType = QuadooVM::I4;
	args[1].lVal = 2;
	parameters.cArgs = 2;
	parameters.pqvArgs = args;
	REQUIRE(sprite->Invoke(NULL,NULL,&parameters,&value) == QuadooForm::EInvalidState);
	Check(SpriteBinary(pixels,sizeof(pixels),&binary));
	args[0].eType = QuadooVM::String;
	args[0].rstrVal = RSTRING_CAST(L"RGBA32");
	args[1] = binary;
	args[2].eType = QuadooVM::I4;
	args[2].lVal = 1;
	args[3].eType = QuadooVM::I4;
	args[3].lVal = 2;
	parameters.cArgs = 4;
	Check(sprite->Invoke(NULL,RSTRING_CAST(L"LoadPixels"),&parameters,&value));
	parameters.cArgs = 0;
	Check(sprite->Invoke(NULL,RSTRING_CAST(L"LoadPng"),&parameters,&value) == DISP_E_BADPARAMCOUNT ? S_OK : E_FAIL);
	parameters.cArgs = 1;
	args[0] = binary;
	REQUIRE(FAILED(sprite->Invoke(NULL,RSTRING_CAST(L"LoadPng"),&parameters,&value)));
	Check(sprite->GetProperty(NULL,RSTRING_CAST(L"Width"),&value));
	REQUIRE(value.lVal == 2);
	parameters.cArgs = 2;
	args[0].eType = QuadooVM::I4;
	args[0].lVal = 3;
	args[1].eType = QuadooVM::I4;
	args[1].lVal = 2;
	Check(canvas->Clear(RGB(255,255,255)));
	Check(sprite->Invoke(NULL,NULL,&parameters,&value));
	Check(canvas->GetPixel(2,3,&color));
	REQUIRE(color == RGB(255,0,0));
	Check(canvas->GetPixel(3,3,&color));
	REQUIRE(color == RGB(128,255,128));
	{
		DWORD resources = GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
		for(UINT n = 0; n < 100; n++)
		{
			TStackRef<IQuadooObject> repeated;
			QuadooVM::QVARIANT load[4] = {{QuadooVM::String},binary,{QuadooVM::I4},{QuadooVM::I4}};
			QuadooVM::QVPARAMS input = {load,4};
			load[0].rstrVal = RSTRING_CAST(L"RGBA32");
			load[2].lVal = 1;
			load[3].lVal = 2;
			Check(AQVM::CreateSpriteObject(canvas,&repeated));
			Check(repeated->Invoke(NULL,RSTRING_CAST(L"LoadPixels"),&input,&value));
			Check(repeated->Invoke(NULL,NULL,&parameters,&value));
		}
		REQUIRE(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS) <= resources);
		REQUIRE(AQVM::SpriteLiveObjects() == objects + 1);
	}
	{
		SpriteThreadProbe probe = {sprite,S_OK};
		thread = CreateThread(NULL,0,SpriteWrongThread,&probe,0,NULL);
		CheckIf(!thread,HrEnsureLastError());
		REQUIRE(WaitForSingleObject(thread,10000) == WAIT_OBJECT_0 && probe.result == QuadooForm::EWrongThread);
		CloseHandle(thread);
		thread = NULL;
	}
	{
		TStackRef<IQuadooLayoutControl> layout;
		QuadooForm::Rect bounds;
		Check(control->GetBounds(&bounds));
		Check(control->QueryInterface(&layout));
		Check(layout->Arrange(&bounds,144));
		Check(sprite->Invoke(NULL,RSTRING_CAST(L"Draw"),&parameters,&value));
		Check(canvas->GetSurface(&dc,&bitmap,&surface));
		REQUIRE(surface->xSize == 60 && surface->ySize == 45);
		Check(layout->Arrange(&bounds,96));
	}
	{
		TStackRef<CReentrantSpriteBytes> provider;
		QuadooVM::QVARIANT argument = {QuadooVM::Object};
		QuadooVM::QVPARAMS load = {&argument,1};
		provider.Attach(__new CReentrantSpriteBytes(sprite));
		CheckAlloc(provider);
		argument.pObject = provider;
		Check(sprite->Invoke(NULL,RSTRING_CAST(L"LoadPng"),&load,&value));
		REQUIRE(provider->nested == QuadooForm::EInvalidState && provider->locks == 1 && provider->unlocks == 1);
		provider->destroyOnUnlock = control;
		REQUIRE(sprite->Invoke(NULL,RSTRING_CAST(L"LoadPng"),&load,&value) == QuadooForm::EDestroyed);
		REQUIRE(SUCCEEDED(provider->destroyed) && provider->locks == 2 && provider->unlocks == 2);
		Check(sprite->GetProperty(NULL,RSTRING_CAST(L"Width"),&value));
		REQUIRE(value.lVal == 4);
	}
	Check(form->Destroy());
	REQUIRE(sprite->Invoke(NULL,NULL,&parameters,&value) == QuadooForm::EDestroyed);
	dc = reinterpret_cast<HDC>(1);
	bitmap = reinterpret_cast<HBITMAP>(1);
	surface = reinterpret_cast<SIF_SURFACE*>(1);
	REQUIRE(canvas->GetSurface(&dc,&bitmap,&surface) == QuadooForm::EDestroyed && !dc && !bitmap && !surface);
	canvas.Release();
	control.Release();
	form.Release();
	Check(lifetime->GetLiveCount(&live));
	REQUIRE(live == initialLive + 1);
	parameters.cArgs = 0;
	Check(sprite->Invoke(NULL,RSTRING_CAST(L"Clear"),&parameters,&value));
	sprite.Release();
	Check(lifetime->GetLiveCount(&live));
	REQUIRE(live == initialLive);
	REQUIRE(AQVM::SpriteLiveObjects() == objects && AQVM::SpriteAllocatedBytes() == bytes);
	hr = S_OK;
Cleanup:
	if(thread)
	{
		CloseHandle(thread);
	}
	if(form)
	{
		form->Destroy();
	}
	QVMClearVariant(&binary);
	QVMClearVariant(&value);
	return hr;
}

class CSpriteTestInputs : public CBaseUnknown, public CQuadooObjectImpl
{
	TStackRef<IJSONObject> m_input;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooObject)
	END_UNK_MAP
	CSpriteTestInputs(IJSONObject* input) : m_input(input)
	{
	}
	STDMETHOD(GetProperty)(IQuadooVM*, RSTRING name, QuadooVM::QVARIANT* result)
	{
		HRESULT hr;
		TStackRef<IJSONValue> value;
		result->eType = QuadooVM::Null;
		Check(m_input->FindNonNullValue(name,&value));
		Check(value->GetString(&result->rstrVal));
		result->eType = QuadooVM::String;
	Cleanup:
		return hr;
	}

};

static HRESULT SpriteScriptChecks(IQuadooVM* vm)
{
	HRESULT hr;
	WCHAR paths[3][MAX_PATH] = {{0},{0},{0}};
	TStackRef<IJSONObject> input;
	TStackRef<ILockableStream> binary;
	TStackRef<CSpriteTestInputs> inputs;
	QuadooVM::QVARIANT value = {QuadooVM::Null};
	QFI::CImageSurface output;
	CMemoryStream decoded;
	HWND window = NULL;
	BYTE* bytes;
	DWORD count;
	BOOL locked = FALSE;
	LONG objects = AQVM::SpriteLiveObjects();
	LONG allocated = AQVM::SpriteAllocatedBytes();
	BYTE icon[sizeof(ImageLegacyICO)];
	CopyMemory(icon,ImageLegacyICO,sizeof(icon));
	icon[73] = 128;
	Check(SpriteTemp(ImageRGBA,sizeof(ImageRGBA),paths[0]));
	Check(SpriteTemp(ImageBMP,sizeof(ImageBMP),paths[1]));
	Check(SpriteTemp(icon,sizeof(icon),paths[2]));
	Check(JSONCreateObject(&input));
	static PCWSTR const fields[] = {L"Png",L"Bmp",L"Icon"};
	for(UINT n = 0; n < ARRAYSIZE(paths); n++)
	{
		Check(AQVM::PutString(input,fields[n],RSTRING_CAST(paths[n])));
	}
	inputs.Attach(__new CSpriteTestInputs(input));
	CheckAlloc(inputs);
	Check(vm->AddGlobal(RSTRING_CAST(L"SpriteInputs"),inputs));
	Check(Call(vm,L"SpriteChecks",&value));
	if(value.eType != QuadooVM::I4 && value.eType != QuadooVM::I8)
	{
		CRString text;
		CHAR detail[4096];
		if(SUCCEEDED(QVMVariantToString(&value,&text)))
		{
			WideCharToMultiByte(CP_UTF8,0,text.c_wstr(),-1,detail,ARRAYSIZE(detail),NULL,NULL);
			Stage(detail);
		}
	}
	if(value.eType != QuadooVM::I4 && value.eType != QuadooVM::I8)
	{
		QuadooVM::QVARIANT stage = {QuadooVM::Null};
		CHAR report[100];
		if(SUCCEEDED(Call(vm,L"SpriteWhere",&stage)) && stage.eType == QuadooVM::I4)
		{
			wsprintfA(report," at sprite stage %d\r\n",stage.lVal);
			Stage(report);
		}
		QVMClearVariant(&stage);
	}
	LONGLONG handle;
	Check(AQVM::VariantInteger(&value,&handle));
	window = reinterpret_cast<HWND>(static_cast<UINT_PTR>(handle));
	REQUIRE(IsWindow(window));
	QVMClearVariant(&value);
	{
		LARGE_INTEGER frequency;
		LONGLONG totals[2] = {0,0};
		CheckIf(!QueryPerformanceFrequency(&frequency),E_FAIL);
		// Three alternating batches of 10,000 clipped 16x16 alpha sprites.
		// Include actual VM dispatch; compare results, never demand a speed ratio.
		for(UINT round = 0; round < 3; round++)
		{
			INT expected = 0;
			for(UINT kind = 0; kind < 2; kind++)
			{
				LARGE_INTEGER start;
				LARGE_INTEGER end;
				CheckIf(!QueryPerformanceCounter(&start),E_FAIL);
				Check(Call(vm,kind ? L"SpriteBenchDefault" : L"SpriteBenchExplicit",&value));
				CheckIf(!QueryPerformanceCounter(&end),E_FAIL);
				REQUIRE(value.eType == QuadooVM::I4);
				if(!kind)
				{
					expected = value.lVal;
				}
				REQUIRE(value.lVal == expected);
				totals[kind] += end.QuadPart - start.QuadPart;
				QVMClearVariant(&value);
			}
		}
		CHAR report[256];
		wsprintfA(report,"Sprite VM benchmark: 30000 16x16 draws per route; explicit %I64d us, default %I64d us\r\n",
			totals[0] * 1000000 / frequency.QuadPart,totals[1] * 1000000 / frequency.QuadPart);
		Stage(report);
	}
	Check(Call(vm,L"SpriteTransient",&value));
	QVMClearVariant(&value);
	Check(output.Create(40,30));
	for(UINT mode = 0; mode < 2; mode++)
	{
		SendMessageW(window,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(output.DC()),PRF_CLIENT);
		CheckIf(!GdiFlush(),E_FAIL);
		const BYTE* pixel = output.Pixels() + (4 * 40 + 4) * 4;
		REQUIRE(pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 255);
		REQUIRE(output.Pixels()[0] == 0x56 && output.Pixels()[1] == 0x34 && output.Pixels()[2] == 0x12);
		Check(Call(vm,L"SpriteUnbuffered",&value));
		QVMClearVariant(&value);
	}
	Check(Call(vm,L"SpriteFinish",&value));
	REQUIRE(value.eType == QuadooVM::Object && !IsWindow(window));
	Check(value.pObject->QueryInterface(&binary));
	Check(binary->Lock(&bytes,&count));
	locked = TRUE;
	UINT width;
	UINT height;
	Check(QFI::DecodePNG(bytes,count,decoded,&width,&height));
	REQUIRE(width == 40 && height == 30);
	{
		const BYTE* pixel = decoded.GetReadPtr() + (3 * width + 2) * 4;
		REQUIRE(pixel[0] == 255 && pixel[1] == 0 && pixel[2] == 0 && pixel[3] == 255);
		REQUIRE(pixel[4] == 128 && pixel[5] == 255 && pixel[6] == 128 && pixel[7] == 255);
	}
	// This fixture has not entered the application message loop. The runtime
	// intentionally retains the form script and its OnCreate Sprite until its
	// normal sweep/shutdown. TestRuntime verifies final release after shutdown.
	REQUIRE(AQVM::SpriteLiveObjects() == objects + 1 && AQVM::SpriteAllocatedBytes() == allocated + 8);
	hr = S_OK;
Cleanup:
	if(locked)
	{
		binary->Unlock();
	}
	QVMClearVariant(&value);
	for(UINT n = 0; n < ARRAYSIZE(paths); n++)
	{
		if(paths[n][0])
		{
			DeleteFileW(paths[n]);
		}
	}
	return hr;
}

static HRESULT SpriteResourcesReleased(LONG objects, LONG bytes)
{
	HRESULT hr;
	REQUIRE(AQVM::SpriteLiveObjects() == objects);
	REQUIRE(AQVM::SpriteAllocatedBytes() == bytes);
	hr = S_OK;
Cleanup:
	return hr;
}
