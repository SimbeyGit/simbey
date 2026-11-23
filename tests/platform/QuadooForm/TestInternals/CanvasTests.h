#include "..\..\..\..\platform\QuadooForm\QuadooForm\CanvasDrawing.h"

static HRESULT CanvasRenderingTests()
{
	HRESULT hr;
	TStackRef<QF::CCanvasSurface> surface;
	TStackRef<QF::CCanvasSurface> larger;
	TStackRef<ISequentialStream> encoded;
	CMemoryStream bytes;
	CMemoryStream pixels;
	HFONT font = NULL;
	SIZE first;
	SIZE multi;
	SIZE space;
	SIZE tabs;
	COLORREF color;
	ULONG count;
	ULONG read;
	UINT width;
	UINT height;
	BYTE* write;
	LOGFONTW description = {0};
	Check(TStrCchCpy(description.lfFaceName,ARRAYSIZE(description.lfFaceName),L"Arial"));
	description.lfHeight = -16;
	font = CreateFontIndirectW(&description);
	CheckIf(!font,HrEnsureLastError());
	surface.Attach(__new QF::CCanvasSurface);
	larger.Attach(__new QF::CCanvasSurface);
	CheckAlloc(surface);
	CheckAlloc(larger);
	Check(surface->Create(180,100,96,RGB(10,20,30),NULL));
	VERIFY(surface->Pixels()[0] == 30 && surface->Pixels()[1] == 20 && surface->Pixels()[2] == 10 && surface->Pixels()[3] == 255);
	Check(QF::CanvasText(NULL,font,RSTRING_CAST(L"A&V "),0,0,0,&first,FALSE));
	VERIFY(first.cx > 0 && first.cy > 0);
	Check(QF::CanvasText(NULL,font,RSTRING_CAST(L"A&V \r\n\n"),0,0,0,&multi,FALSE));
	VERIFY(multi.cx == first.cx && multi.cy == first.cy * 3);
	Check(QF::CanvasText(NULL,font,RSTRING_CAST(L" "),0,0,0,&space,FALSE));
	Check(QF::CanvasText(NULL,font,RSTRING_CAST(L"\t"),0,0,0,&tabs,FALSE));
	VERIFY(tabs.cx == space.cx * 8 && tabs.cy == space.cy);
	Check(QF::CanvasText(NULL,font,RSTRING_CAST(L""),0,0,0,&multi,FALSE));
	VERIFY(!multi.cx && !multi.cy);
	Check(QF::CanvasText(NULL,font,RSTRING_CAST(L"\x03a9\x65e5\x672c"),0,0,0,&multi,FALSE));
	VERIFY(multi.cx > 0 && multi.cy == first.cy);
	Check(QF::CanvasText(surface->DC(),font,RSTRING_CAST(L"A&V \r\nTest"),2,3,RGB(255,255,255),&multi,TRUE));
	Check(surface->Opaque());
	VERIFY(surface->Pixels()[3] == 255);
	{
		CRString invalid;
		static const WCHAR embedded[] = {L'a',0,L'b'};
		Check(RStrCreateW(ARRAYSIZE(embedded),embedded,&invalid));
		VERIFY(QF::CanvasText(NULL,font,invalid,0,0,0,&multi,FALSE) == E_INVALIDARG);
	}
	Check(larger->Create(200,120,144,RGB(1,2,3),surface));
	VERIFY(!memcmp(surface->Pixels(),larger->Pixels(),180 * 4));
	VERIFY(larger->Pixels()[(119 * 200 + 199) * 4] == 3);
	VERIFY(FAILED(larger->Create(4097,1,96,0,NULL)) && larger->width == 200);
	Check(surface->Export(&encoded,&count));
	Check(bytes.TWriteAdvance<BYTE>(&write,count));
	Check(encoded->Read(write,count,&read));
	VERIFY(read == count && count > 24 && !memcmp(write,"\x89PNG\r\n\x1a\n",8));
	Check(QFI::DecodePNG(write,count,pixels,&width,&height));
	VERIFY(width == 180 && height == 100 && pixels.DataRemaining() == width * height * 4);
	for(UINT n = 0; n < width * height; n++)
	{
		const BYTE* source = surface->Pixels() + n * 4;
		const BYTE* actual = pixels.GetReadPtr() + n * 4;
		VERIFY(actual[0] == source[2] && actual[1] == source[1] && actual[2] == source[0] && actual[3] == 255);
	}
	for(LONG fault = 0; fault < 48; fault++)
	{
		encoded.Release();
		LONG baseline = QFI::AllocatedBytes();
		QFI::FailAllocationAfter(fault);
		hr = surface->Export(&encoded,&count);
		QFI::FailAllocationAfter(-1);
		if(QFI::AllocatedBytes() != baseline)
		{
			CRString failure;
			Check(RStrFormatW(&failure,L"Canvas PNG allocation case %d leaked %d bytes\r\n",fault,QFI::AllocatedBytes() - baseline));
			Log(failure.c_wstr());
			hr = E_FAIL;
			goto Cleanup;
		}
		VERIFY(QFI::AllocatedBytes() == baseline);
		if(FAILED(hr))
		{
			VERIFY(hr == E_OUTOFMEMORY && !encoded && !count);
		}
		else
		{
			CMemoryStream checkBytes;
			CMemoryStream checkPixels;
			BYTE* checkWrite;
			Check(checkBytes.TWriteAdvance<BYTE>(&checkWrite,count));
			Check(encoded->Read(checkWrite,count,&read));
			hr = QFI::DecodePNG(checkWrite,count,checkPixels,&width,&height);
			if(FAILED(hr))
			{
				CRString failure;
				Check(RStrFormatW(&failure,L"Canvas PNG allocation case %d produced invalid output\r\n",fault));
				Log(failure.c_wstr());
				hr = E_FAIL;
				goto Cleanup;
			}
			VERIFY(width == 180 && height == 100);
		}
	}
	Check(QF::CanvasColor(0x80000000 | COLOR_WINDOW,TRUE,&color));
	VERIFY(color == GetSysColor(COLOR_WINDOW));
	VERIFY(QF::CanvasColor(0x80000019,TRUE,&color) == E_INVALIDARG);
	VERIFY(QF::CanvasColor(0x80000005,FALSE,&color) == E_INVALIDARG);
	VERIFY(QF::CanvasColor(0x01020304,FALSE,&color) == E_INVALIDARG);
	hr = S_OK;
Cleanup:
	QFI::FailAllocationAfter(-1);
	if(font)
	{
		DeleteObject(font);
	}
	return hr;
}
