#include "..\TestImages\ImageTestSupport.h"
#include "..\..\..\..\platform\QuadooForm\Imaging\IconDirectory.h"

static BOOL ImageExpected(BOOL condition, UINT line)
{
	g_checks++;
	if(!condition) { CRString message; if(SUCCEEDED(RStrFormatW(&message,L"Image test failed at line %u\r\n",line))) Log(message.c_wstr()); }
	return condition;
}
#define IMAGE_REQUIRE(value) CheckIf(!ImageExpected(!!(value),__LINE__),E_FAIL)
class CImageFailStream : public CMemoryStream
{
public:
	STDMETHOD(Write)(const VOID* bytes, ULONG count, ULONG* written)
	{
		if(DataRemaining() + count > 64) { if(written) *written = 0; return E_OUTOFMEMORY; }
		return CMemoryStream::Write(bytes,count,written);
	}
};

static HRESULT ImageRenderingTests()
{
	HRESULT hr;
	TStackRef<QFI::CImageResource> stripes, rgba, icon, pattern, legacy;
	TStackRef<IJSONValue> value, again;
	CImageTestCanvas canvas;
	RECT rect = {2,3,10,11};
	const COLORREF background = RGB(21,35,49);
	Check(canvas.Create(80,80));
	Check(QFI::CImageResource::FromBytes(L"png",ImageStripes,sizeof(ImageStripes),&stripes));
	Check(QFI::CImageResource::FromBytes(L"png",ImageRGBA,sizeof(ImageRGBA),&rgba));
	Check(QFI::CImageResource::FromBytes(L"ico",ImageMultiICO,sizeof(ImageMultiICO),&icon));
	Check(QFI::CImageResource::FromBytes(L"png",ImagePattern,sizeof(ImagePattern),&pattern));
	Check(QFI::CImageResource::FromBytes(L"ico",ImageLegacyICO,sizeof(ImageLegacyICO),&legacy));
	IMAGE_REQUIRE(stripes->Width() == 6 && stripes->Height() == 2 && !stripes->HasAlpha());
	IMAGE_REQUIRE(rgba->Width() == 4 && rgba->HasAlpha() && icon->IsIcon() && icon->Width() == 32);
	for(UINT alignment = 0; alignment < 9; alignment++)
	{
		canvas.Fill(background);
		Check(stripes->Paint(canvas.DC(),&rect,96,QFI::Normal,alignment,QFI::Nearest,255));
		UINT left = 2 + alignment % 3, top = 3 + alignment / 3 * 3;
		IMAGE_REQUIRE(canvas.Pixel(left,top) == RGB(255,0,0) && canvas.Pixel(left + 5,top + 1) == RGB(0,0,255));
		IMAGE_REQUIRE(canvas.Pixel(1,3) == background && canvas.Pixel(10,11) == background);
		canvas.Fill(background);
		Check(stripes->Paint(canvas.DC(),&rect,96,QFI::Cover,alignment,QFI::Nearest,255));
		COLORREF expected = alignment % 3 == 0 ? RGB(255,0,0) : alignment % 3 == 1 ? RGB(0,255,0) : RGB(0,0,255);
		IMAGE_REQUIRE(canvas.Pixel(6,7) == expected);
	}
	canvas.Fill(background);
	Check(stripes->Paint(canvas.DC(),&rect,96,QFI::Stretch,8,QFI::Nearest,255));
	IMAGE_REQUIRE(canvas.Pixel(2,3) == RGB(255,0,0) && canvas.Pixel(9,10) == RGB(0,0,255));
	canvas.Fill(background);
	Check(stripes->Paint(canvas.DC(),&rect,96,QFI::Contain,4,QFI::Nearest,255));
	IMAGE_REQUIRE(canvas.Pixel(2,3) == background && canvas.Pixel(2,6) == RGB(255,0,0) && canvas.Pixel(9,7) == RGB(0,0,255));
	{
		RECT cropped = {0,0,4,2};
		canvas.Fill(background);
		Check(stripes->Paint(canvas.DC(),&cropped,96,QFI::Normal,2,QFI::Nearest,255));
		IMAGE_REQUIRE(canvas.Pixel(0,0) == RGB(0,255,0) && canvas.Pixel(3,0) == RGB(0,0,255));
	}
	{
		RECT tiled = {0,0,8,4};
		canvas.Fill(background);
		Check(rgba->Paint(canvas.DC(),&tiled,96,QFI::Tile,8,QFI::Nearest,255,-1,-1));
		IMAGE_REQUIRE(canvas.Pixel(0,0) == RGB(0,255,255) && canvas.Pixel(4,0) == RGB(0,255,255));
		IMAGE_REQUIRE(canvas.Pixel(3,1) == RGB(255,0,0));
	}
	for(UINT opacity = 0; opacity <= 255; opacity += opacity == 0 ? 128 : 127)
	{
		RECT area = {0,0,4,2};
		canvas.Fill(RGB(255,255,255));
		Check(rgba->Paint(canvas.DC(),&area,96,QFI::Stretch,4,QFI::Nearest,static_cast<BYTE>(opacity)));
		COLORREF red = canvas.Pixel(0,0), blue = canvas.Pixel(2,0);
		IMAGE_REQUIRE(GetRValue(red) == 255 && abs(GetGValue(red) - static_cast<INT>(255 - opacity)) <= 1);
		IMAGE_REQUIRE(GetBValue(blue) == 255 && abs(GetRValue(blue) - (255 - static_cast<INT>((128 * opacity + 127) / 255))) <= 1);
		IMAGE_REQUIRE(canvas.Pixel(3,0) == RGB(255,255,255));
		if(opacity == 255) break;
	}
	{
		RECT scaled = {0,0,3,3};
		DWORD hashes[4];
		for(UINT filter = 0; filter < 4; filter++)
		{
			canvas.Fill(RGB(255,255,255));
			Check(pattern->Paint(canvas.DC(),&scaled,96,QFI::Stretch,4,filter,255));
			hashes[filter] = canvas.Hash();
			for(UINT y = 0; y < 3; y++) for(UINT x = 0; x < 3; x++) IMAGE_REQUIRE(canvas.Pixel(x,y) != RGB(255,0,255));
		}
		for(UINT filter = 0; filter < 4; filter++) for(UINT other = 0; other < filter; other++) IMAGE_REQUIRE(hashes[filter] != hashes[other]);
	}
	for(UINT dpi = 96; dpi <= 192; dpi += 48)
	{
		UINT side = MulDiv(32,dpi,96);
		RECT bounds = {0,0,static_cast<INT>(side),static_cast<INT>(side)};
		canvas.Fill(background);
		Check(icon->Paint(canvas.DC(),&bounds,dpi,QFI::Normal,4,QFI::Nearest,255));
		COLORREF expected = dpi == 96 ? RGB(0,255,0) : dpi == 144 ? RGB(0,0,255) : RGB(255,255,0);
		IMAGE_REQUIRE(canvas.Pixel(side / 2,side / 2) == expected && icon->SelectedWidth() == (dpi == 96 ? 32U : dpi == 144 ? 48U : 256U));
	}
	{
		const BYTE pixels[] = {255,0,0,255,0,0,255,0};
		CMemoryStream png;
		TStackRef<QFI::CImageResource> halo;
		Check(QFI::EncodePNG(pixels,2,1,png));
		Check(QFI::CImageResource::FromBytes(L"png",png.GetReadPtr(),png.DataRemaining(),&halo));
		for(UINT filter = QFI::Bilinear; filter <= QFI::HighQuality; filter++)
		{
			RECT bounds = {0,0,64,2};
			canvas.Fill(RGB(255,255,255));
			Check(halo->Paint(canvas.DC(),&bounds,96,QFI::Stretch,4,filter,255));
			for(UINT x = 0; x < 64; x++)
			{
				COLORREF color = canvas.Pixel(x,0);
				IMAGE_REQUIRE(GetRValue(color) == 255 && GetGValue(color) == GetBValue(color));
			}
		}
		RECT one = {0,0,1,1};
		canvas.Fill(RGB(255,255,255));
		Check(halo->Paint(canvas.DC(),&one,96,QFI::Stretch,4,QFI::HighQuality,255));
		IMAGE_REQUIRE(GetRValue(canvas.Pixel(0,0)) == 255 && abs(GetGValue(canvas.Pixel(0,0)) - 127) <= 1);
	}
	{
		RECT bounds = {0,0,16,16};
		Check(icon->Paint(canvas.DC(),&bounds,96,QFI::Stretch,4,QFI::Nearest,255));
		IMAGE_REQUIRE(canvas.Pixel(8,8) == RGB(255,0,0) && icon->SelectedWidth() == 16);
	}
	Check(icon->GetValue(&value));
	Check(ImageFixtureValue(L"ico",ImageMultiICO,sizeof(ImageMultiICO),&again));
	IMAGE_REQUIRE(!JSONCompare(value,again));
	{
		RECT bounds = {0,0,2,2};
		canvas.Fill(background);
		Check(legacy->Paint(canvas.DC(),&bounds,96,QFI::Normal,0,QFI::Nearest,255));
		IMAGE_REQUIRE(canvas.Pixel(0,0) == RGB(255,0,0) && canvas.Pixel(1,0) == background && canvas.Pixel(1,1) == RGB(0,255,0));
		IMAGE_REQUIRE(canvas.Pixel(0,1) == RGB(234,220,206));
	}
	{
		BYTE alphaIcon[sizeof(ImageLegacyICO)];
		CopyMemory(alphaIcon,ImageLegacyICO,sizeof(alphaIcon));
		alphaIcon[73] = 128; alphaIcon[82] |= 0x80;
		TStackRef<QFI::CImageResource> image;
		Check(QFI::CImageResource::FromBytes(L"ico",alphaIcon,sizeof(alphaIcon),&image));
		RECT bounds = {0,0,2,2};
		canvas.Fill(RGB(255,255,255));
		Check(image->Paint(canvas.DC(),&bounds,96,QFI::Normal,0,QFI::Nearest,255));
		IMAGE_REQUIRE(GetRValue(canvas.Pixel(0,0)) == 255 && abs(GetGValue(canvas.Pixel(0,0)) - 127) <= 1);
	}
	hr = S_OK;
Cleanup:
	return hr;
}
static HRESULT ImageImportTests()
{
	HRESULT hr;
	WCHAR path[MAX_PATH] = {0};
	TStackRef<IJSONValue> value;
	TStackRef<QFI::CImageResource> picture;
	CImageTestCanvas canvas;
	Check(canvas.Create(8,8));
	for(UINT input = 0; input < 4; input++)
	{
		const BYTE* bytes = input == 0 ? ImageJPEG : input == 1 ? ImageBMP : input == 2 ? ImageMultiICO : ImageRGBA;
		UINT length = input == 0 ? sizeof(ImageJPEG) : input == 1 ? sizeof(ImageBMP) : input == 2 ? sizeof(ImageMultiICO) : sizeof(ImageRGBA);
		value.Release(); picture.Release();
		Check(ImageTempFile(bytes,length,path));
		Check(QFI::ImportFile(path,&value));
		IMAGE_REQUIRE(DeleteFileW(path)); path[0] = 0;
		Check(QFI::CImageResource::Create(value,&picture));
		IMAGE_REQUIRE(picture->IsIcon() == (input == 2));
		if(input < 2)
		{
			IMAGE_REQUIRE(picture->Width() == (input == 0 ? 2U : 3U) && picture->Height() == (input == 0 ? 3U : 2U));
			RECT bounds = {0,0,static_cast<INT>(picture->Width()),static_cast<INT>(picture->Height())};
			Check(picture->Paint(canvas.DC(),&bounds,96,QFI::Normal,0,QFI::Nearest,255));
			if(input == 0) IMAGE_REQUIRE(GetRValue(canvas.Pixel(0,0)) > 230 && GetGValue(canvas.Pixel(0,0)) > 230); // EXIF 6: yellow moves to top left.
			else IMAGE_REQUIRE(canvas.Pixel(0,0) == RGB(255,0,0));
		}
	}
	{
		BYTE unsupported[sizeof(ImageMultiICO)];
		CopyMemory(unsupported,ImageMultiICO,sizeof(unsupported));
		QFI::IconEntry entry;
		CopyMemory(&entry,unsupported + sizeof(QFI::IconDirectory),sizeof(entry));
		DWORD compression = BI_BITFIELDS;
		CopyMemory(unsupported + entry.offset + FIELD_OFFSET(BITMAPINFOHEADER,biCompression),&compression,sizeof(compression));
		picture.Release();
		Check(QFI::CImageResource::FromBytes(L"ico",unsupported,sizeof(unsupported),&picture));
		value.Release();
		Check(picture->GetValue(&value));
		TStackRef<IJSONValue> expected;
		Check(ImageFixtureValue(L"ico",unsupported,sizeof(unsupported),&expected));
		IMAGE_REQUIRE(!JSONCompare(value,expected));
		picture.Release();
		IMAGE_REQUIRE(FAILED(QFI::CImageResource::FromBytes(L"ico",unsupported,sizeof(unsupported) - 1,&picture)) && !picture);
	}
	{
		BYTE invalid[sizeof(ImageLegacyICO)];
		CopyMemory(invalid,ImageLegacyICO,sizeof(invalid));
		DWORD compression = BI_BITFIELDS;
		CopyMemory(invalid + 22 + FIELD_OFFSET(BITMAPINFOHEADER,biCompression),&compression,4);
		IMAGE_REQUIRE(QFI::CImageResource::FromBytes(L"ico",invalid,sizeof(invalid),&picture) == HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED) && !picture);
		IMAGE_REQUIRE(FAILED(QFI::CImageResource::FromBytes(L"png",ImageRGBA,32,&picture)) && !picture);
		IMAGE_REQUIRE(FAILED(QFI::CImageResource::FromBytes(L"png",ImageRGBA,QFI::MaxBytes + 1,&picture)) && !picture);
	}
	hr = S_OK;
Cleanup:
	if(path[0]) DeleteFileW(path);
	return hr;
}
static HRESULT ImageValidationTests()
{
	HRESULT hr;
	TStackRef<QFI::CImageResource> picture;
	CImageTestCanvas canvas;
	Check(canvas.Create(8,8));
	for(UINT length = 0; length < sizeof(ImageRGBA); length++)
	{
		IMAGE_REQUIRE(FAILED(QFI::CImageResource::FromBytes(L"png",ImageRGBA,length,&picture)) && !picture);
	}
	{
		BYTE invalid[sizeof(ImageRGBA) + 1];
		CopyMemory(invalid,ImageRGBA,sizeof(ImageRGBA)); invalid[sizeof(ImageRGBA)] = 0;
		IMAGE_REQUIRE(FAILED(QFI::CImageResource::FromBytes(L"png",invalid,sizeof(invalid),&picture)) && !picture);
		invalid[sizeof(ImageRGBA) - 1] ^= 1;
		IMAGE_REQUIRE(FAILED(QFI::CImageResource::FromBytes(L"png",invalid,sizeof(ImageRGBA),&picture)) && !picture);
	}
	{
		// Empty tEXt is ignored by the pixel decoder but its CRC must be checked.
		BYTE invalid[sizeof(ImageRGBA) + 12];
		CopyMemory(invalid,ImageRGBA,33);
		ZeroMemory(invalid + 33,12);
		CopyMemory(invalid + 37,"tEXt",4);
		CopyMemory(invalid + 45,ImageRGBA + 33,sizeof(ImageRGBA) - 33);
		IMAGE_REQUIRE(FAILED(QFI::CImageResource::FromBytes(L"png",invalid,sizeof(invalid),&picture)) && !picture);
	}
	Check(QFI::CImageResource::FromBytes(L"png",ImageRGBA,sizeof(ImageRGBA),&picture));
	{
		RECT huge = {INT_MIN,0,INT_MAX,1}, tooMany = {0,0,4096,4096}, boundedRect = {0,0,4,2};
		IMAGE_REQUIRE(picture->Paint(canvas.DC(),&huge,96,QFI::Normal,4,QFI::Nearest,255) == HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED));
		IMAGE_REQUIRE(picture->Paint(canvas.DC(),&tooMany,96,QFI::Stretch,4,QFI::HighQuality,255) == HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED));
		IMAGE_REQUIRE(picture->Paint(canvas.DC(),&boundedRect,96,5,4,QFI::Nearest,255) == E_INVALIDARG);
		IMAGE_REQUIRE(picture->Paint(canvas.DC(),&boundedRect,96,QFI::Contain,9,QFI::Nearest,255) == E_INVALIDARG);
		IMAGE_REQUIRE(picture->Paint(canvas.DC(),&boundedRect,96,QFI::Contain,4,4,255) == E_INVALIDARG);
		Check(picture->Paint(canvas.DC(),&huge,96,QFI::Normal,4,QFI::Nearest,0));
		Check(picture->Paint(canvas.DC(),&boundedRect,96,QFI::Stretch,4,QFI::Nearest,255));
		IMAGE_REQUIRE(canvas.Pixel(0,0) == RGB(255,0,0));
	}
	{
		const DWORD stored = QuadooForm::PropertySerializable;
		const QF::PropertySpec invalidRange[] = {{L"Extra",QuadooProperty::Int,stored,0,40,NULL,0,NULL,9,4,TRUE}};
		const QF::TypeSpec type = {L"ImageRangeProbe",&QF::FormType,invalidRange,1,NULL,0};
		Check(MetadataFailure(&type));
	}
	hr = S_OK;
Cleanup:
	return hr;
}
static HRESULT ImageRollbackTests()
{
	HRESULT hr;
	TStackRef<IQuadooForm> form;
	TStackRef<IQuadooControl> image;
	TStackRef<IQuadooStaticControl> painted;
	TStackRef<IJSONObject> definition, before, after;
	TStackRef<IQuadooFormSerializer> serializer;
	TStackRef<IJSONValue> original, replacement;
	CImageTestCanvas canvas;
	BOOL succeeded = FALSE;
	UINT failures = 0;
	RECT bounds = {0,0,4,2};
	Check(NativeDefinition(L"{\"type\":\"Form\",\"name\":\"ImageRollback\",\"children\":[{\"type\":\"Image\",\"name\":\"image\"}]}",&definition));
	Check(QuadooCreateForm(NULL,NULL,definition,&form));
	Check(form->FindControl(RSTRING_CAST(L"image"),&image));
	Check(image->QueryInterface(__uuidof(IQuadooStaticControl),reinterpret_cast<PVOID*>(&painted)));
	Check(ImageFixtureValue(L"png",ImageRGBA,sizeof(ImageRGBA),&original));
	Check(ImageFixtureValue(L"png",ImagePattern,sizeof(ImagePattern),&replacement));
	Check(ImageSet(image,L"Picture",original));
	Check(QuadooGetFormSerializer(&serializer));
	Check(serializer->Serialize(form,&before));
	Check(canvas.Create(8,8));
	for(LONG allocation = 0; allocation < 256; allocation++)
	{
		QFI::FailAllocationAfter(allocation);
		hr = ImageSet(image,L"Picture",replacement);
		QFI::FailAllocationAfter(-1);
		if(SUCCEEDED(hr)) { succeeded = TRUE; break; }
		IMAGE_REQUIRE(hr == E_OUTOFMEMORY);
		failures++;
		Check(serializer->Serialize(form,&after));
		IMAGE_REQUIRE(!JSONCompareObjects(before,after) && ImageInteger(image,L"ImageWidth") == 4);
		after.Release();
		canvas.Fill(RGB(255,255,255));
		Check(painted->Paint(canvas.DC(),&bounds));
		IMAGE_REQUIRE(canvas.Pixel(0,0) == RGB(255,0,0));
	}
	IMAGE_REQUIRE(succeeded && failures > 4);
	Check(ImageSet(image,L"Picture",original));
	Check(painted->Paint(canvas.DC(),&bounds));
	{
		RECT changed = {0,0,7,5};
		QFI::FailAllocationAfter(0);
		hr = painted->Paint(canvas.DC(),&changed);
		QFI::FailAllocationAfter(-1);
		IMAGE_REQUIRE(hr == E_OUTOFMEMORY);
		Check(painted->Paint(canvas.DC(),&bounds));
		Check(serializer->Serialize(form,&after));
		IMAGE_REQUIRE(!JSONCompareObjects(before,after));
	}
	{
		CImageFailStream failed;
		IMAGE_REQUIRE(JSONSerialize(original,&failed) == E_OUTOFMEMORY && failed.DataRemaining() > 0);
		after.Release();
		Check(serializer->Serialize(form,&after));
		IMAGE_REQUIRE(!JSONCompareObjects(before,after));
	}
	{
		DWORD gdi = GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
		for(UINT n = 0; n < 40; n++)
		{
			Check(ImageSet(image,L"Picture",replacement));
			RECT area = {0,0,static_cast<INT>(2 + n % 6),static_cast<INT>(2 + n % 5)};
			Check(painted->Paint(canvas.DC(),&area));
			Check(ImageSet(image,L"Picture",original));
			Check(painted->Paint(canvas.DC(),&bounds));
		}
		GdiFlush();
		IMAGE_REQUIRE(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS) == gdi);
	}
	Check(form->Destroy());
	hr = S_OK;
Cleanup:
	QFI::FailAllocationAfter(-1);
	if(form) form->Destroy();
	return hr;
}
static HRESULT ImagePrivateTests()
{
	HRESULT hr;
	Log(L"Image decoding, scaling, alpha, import and fault injection...\r\n");
	Check(ImageRenderingTests());
	Check(ImageImportTests());
	Check(ImageValidationTests());
	Check(ImageRollbackTests());
Cleanup:
	return hr;
}
