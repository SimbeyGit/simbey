#include "..\..\..\..\platform\QuadooForm\Designer\ImageListEditor.h"
#include "..\..\..\TestPaths.h"
#include "..\TestImages\ImageListFixture.h"

static HRESULT ImageListSessionTests(QFD::CDesignerEditor* editor, REFGUID id);
static HRESULT ImageListMaskDialogTests(QFD::CDesignerEditor* editor, HWND host, REFGUID id);

static HRESULT ImageListFixturePath(PCWSTR name, RSTRING* path)
{
	HRESULT hr;
	WCHAR module[MAX_PATH];
	DWORD length = GetModuleFileNameW(NULL,module,ARRAYSIZE(module));
	CheckIf(!length || length >= ARRAYSIZE(module),E_FAIL);
	PWSTR slash = const_cast<PWSTR>(TStrRChr(module,L'\\'));
	CheckIf(!slash,E_FAIL);
	*slash = 0;
	Check(RStrFormatW(path,L"%ls\\ImageListFixtures\\%ls",module,name));
Cleanup:
	return hr;
}
static HRESULT ImageListFromFile(PCWSTR name, UINT width, UINT height, UINT sizing, BOOL split, BOOL key, QFI::CImageListData** data)
{
	HRESULT hr;
	CRString path;
	TStackRef<IJSONArray> imported;
	TStackRef<IJSONValue> images;
	Check(ImageListFixturePath(name,&path));
	Check(QFI::ImportImageListFile(path.c_wstr(),width,height,sizing,split,key,&imported));
	Check(JSONWrapArray(imported,&images));
	Check(QFI::CImageListData::Create(images,width,height,32,TRUE,data));
Cleanup:
	return hr;
}
static HRESULT ImageListOversizedFile()
{
	HRESULT hr;
	HANDLE file = INVALID_HANDLE_VALUE;
	WCHAR folder[MAX_PATH];
	WCHAR path[MAX_PATH] = {0};
	DWORD written;
	LARGE_INTEGER size;
	TStackRef<IJSONArray> imported;
	static const BYTE signature[] = {'B','M',0,0};
	Check(TestScratchDirectory(L"QuadooFormDesigner",folder,ARRAYSIZE(folder)));
	CheckIf(!GetTempFileNameW(folder,L"QIL",0,path),E_FAIL);
	file = CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
	CheckIf(file == INVALID_HANDLE_VALUE,E_FAIL);
	CheckIf(!WriteFile(file,signature,sizeof(signature),&written,NULL) || written != sizeof(signature),E_FAIL);
	size.QuadPart = QFI::MaxBytes + 1;
	CheckIf(!SetFilePointerEx(file,size,NULL,FILE_BEGIN) || !SetEndOfFile(file),E_FAIL);
	CloseHandle(file);
	file = INVALID_HANDLE_VALUE;
	REQUIRE(FAILED(QFI::ImportImageListFile(path,16,16,QFI::ImageContain,FALSE,FALSE,&imported)) && !imported);
	hr = S_OK;
Cleanup:
	if(file != INVALID_HANDLE_VALUE)
	{
		CloseHandle(file);
	}
	if(path[0])
	{
		DeleteFileW(path);
	}
	return hr;
}
static HRESULT ImageListPixelsAndImports()
{
	HRESULT hr;
	TStackRef<QFI::CImageListData> data;
	TStackRef<QFI::CImageListData> icon;
	TStackRef<QFI::CImageListData> invalid;
	TStackRef<IJSONValue> before;
	TStackRef<IJSONValue> after;
	QFI::Raster pixels;
	UINT cx;
	UINT cy;
	UINT sizing;
	Check(ImageListOversizedFile());
	Check(ImageListFromFile(L"alpha.png",4,3,QFI::ImageCrop,FALSE,FALSE,&data));
	Check(data->Render(0,pixels));
	REQUIRE(pixels.pixels[3] == 128 && pixels.pixels[2] == 128 && pixels.pixels[7] == 255);
	REQUIRE(pixels.pixels[11] == 0 && pixels.pixels[(4 + 1) * 4 + 3] == 0);
	Check(data->Images(&before));
	Check(data->Configure(2,1,32,FALSE));
	Check(data->Render(0,pixels));
	REQUIRE(pixels.pixels[3] == 128 && pixels.pixels[2] == 128);
	Check(data->Configure(24,17,32,TRUE));
	Check(data->Render(0,pixels));
	Check(data->Configure(2,1,32,TRUE));
	Check(data->Render(0,pixels));
	REQUIRE(pixels.pixels[3] == 128 && pixels.pixels[2] == 128);
	Check(data->Images(&after));
	REQUIRE(JSONCompare(before,after) == 0);
	const UINT depths[] = {4,8,16,24,32};
	for(UINT n = 0; n < ARRAYSIZE(depths); n++)
	{
		for(UINT mask = 0; mask < 2; mask++)
		{
			Check(data->Configure(2,1,depths[n],mask));
			Check(data->Render(0,pixels));
			REQUIRE(pixels.pixels[3] == (depths[n] == 32 ? 128 : 255));
		}
	}
	REQUIRE(data->Configure(0,16,32,TRUE) == E_INVALIDARG);
	REQUIRE(data->Configure(16,257,32,TRUE) == E_INVALIDARG);
	REQUIRE(data->Configure(16,16,12,TRUE) == E_INVALIDARG);
	data.Release();
	Check(ImageListFromFile(L"alpha.png",4,3,QFI::ImageCenter,FALSE,FALSE,&data));
	Check(data->Render(0,pixels));
	REQUIRE(pixels.pixels[3] == 0 && pixels.pixels[(4 + 1) * 4 + 3] == 128 && pixels.pixels[(4 + 2) * 4 + 3] == 255);
	data.Release();
	Check(ImageListFromFile(L"alpha.png",4,3,QFI::ImageStretch,FALSE,FALSE,&data));
	Check(data->Render(0,pixels));
	REQUIRE(pixels.pixels[3] > 0 && pixels.pixels[3] < 255 && pixels.pixels[2] == pixels.pixels[3]);
	REQUIRE(pixels.pixels[(4 * 3 - 1) * 4 + 3] == 255);
	data.Release();
	Check(ImageListFromFile(L"alpha.png",4,3,QFI::ImageContain,FALSE,FALSE,&data));
	Check(data->Render(0,pixels));
	REQUIRE(pixels.pixels[(2 * 4) * 4 + 3] == 0);
	data.Release();
	Check(ImageListFromFile(L"01.bmp",16,16,QFI::ImageStretch,FALSE,FALSE,&data));
	Check(data->Render(0,pixels));
	for(UINT n = 0; n < 16 * 16; n++)
	{
		REQUIRE(pixels.pixels[n * 4 + 3] == 255);
	}
	data.Release();
	Check(ImageListFromFile(L"01.bmp",48,48,QFI::ImageCrop,FALSE,TRUE,&data));
	Check(data->Render(0,pixels));
	REQUIRE(pixels.pixels[3] == 0);
	Check(ImageListFromFile(L"resolutions.ico",16,16,QFI::ImageContain,FALSE,FALSE,&icon));
	Check(icon->SourceSize(0,&cx,&cy,&sizing));
	REQUIRE(cx == 16 && cy == 16);
	Check(icon->Render(0,pixels));
	REQUIRE(pixels.pixels[2] == 255 && pixels.pixels[0] == 0);
	icon.Release();
	Check(ImageListFromFile(L"resolutions.ico",48,48,QFI::ImageContain,FALSE,FALSE,&icon));
	Check(icon->SourceSize(0,&cx,&cy,&sizing));
	REQUIRE(cx == 64 && cy == 64);
	Check(icon->Render(0,pixels));
	REQUIRE(pixels.pixels[0] == 255 && pixels.pixels[2] == 0);
	icon.Release();
	Check(ImageListFromFile(L"classic.ico",16,16,QFI::ImageContain,FALSE,FALSE,&icon));
	Check(icon->SourceSize(0,&cx,&cy,&sizing));
	REQUIRE(cx == 20 && cy == 20);
	Check(icon->Render(0,pixels));
	data.Release();
	Check(ImageListFromFile(L"strip.bmp",16,16,QFI::ImageCrop,TRUE,FALSE,&data));
	REQUIRE(data->Count() == 10);
	Check(data->SourceSize(9,&cx,&cy,&sizing));
	REQUIRE(cx == 16 && cy == 16);
	Check(data->Render(9,pixels));
	REQUIRE(pixels.pixels[3] == 255);
	REQUIRE(FAILED(ImageListFromFile(L"alpha.png",1,1,QFI::ImageCrop,TRUE,FALSE,&invalid)) && !invalid);
	REQUIRE(FAILED(ImageListFromFile(L"invalid.png",16,16,QFI::ImageCrop,FALSE,FALSE,&invalid)) && !invalid);
	REQUIRE(FAILED(ImageListFromFile(L"truncated.ico",16,16,QFI::ImageCrop,FALSE,FALSE,&invalid)) && !invalid);
	REQUIRE(FAILED(ImageListFromFile(L"bad.bmp",16,16,QFI::ImageCrop,FALSE,FALSE,&invalid)) && !invalid);
	{
		TStackRef<IJSONValue> collection;
		TStackRef<IJSONArray> array;
		TStackRef<IJSONValue> entry;
		TStackRef<IJSONObject> object;
		TStackRef<IJSONValue> invalidSizing;
		Check(icon->Images(&collection));
		Check(collection->GetArray(&array));
		Check(array->GetValue(0,&entry));
		Check(entry->GetObject(&object));
		Check(JSONCreateString(RSTRING_CAST(L"Unknown"),&invalidSizing));
		Check(object->AddValueW(L"sizing",invalidSizing));
		REQUIRE(FAILED(QFI::CImageListData::Create(collection,16,16,32,TRUE,&invalid)) && !invalid);
	}
	// Repeated cache replacement must not retain temporary DIBs or DCs.
	Check(data->Render(0,pixels));
	DWORD gdi = GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
	for(UINT n = 0; n < 60; n++)
	{
		Check(data->Configure(n % 2 ? 16 : 32,16,32,TRUE));
		Check(data->Render(0,pixels));
	}
	GdiFlush();
	REQUIRE(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS) <= gdi);
	QFI::FailAllocationAfter(0);
	hr = data->Render(0,pixels);
	QFI::FailAllocationAfter(-1);
	REQUIRE(hr == E_OUTOFMEMORY);
	hr = S_OK;
Cleanup:
	QFI::FailAllocationAfter(-1);
	return hr;
}
static HRESULT ImageListResourceCycles()
{
	HRESULT hr;
	QFI::Raster pixels;
	static PCWSTR const files[] = {L"alpha.png",L"01.bmp",L"classic.ico",L"resolutions.ico"};
	// Warm each codec before measuring, including its process-wide resources.
	for(UINT n = 0; n < ARRAYSIZE(files); n++)
	{
		TStackRef<QFI::CImageListData> data;
		Check(ImageListFromFile(files[n],16,16,QFI::ImageContain,FALSE,FALSE,&data));
		Check(data->Render(0,pixels));
	}
	GdiFlush();
	DWORD gdi = GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
	DWORD user = GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS);
	for(UINT n = 0; n < 40; n++)
	{
		TStackRef<QFI::CImageListData> data;
		Check(ImageListFromFile(files[n % ARRAYSIZE(files)],24,16,QFI::ImageContain,FALSE,FALSE,&data));
		Check(data->Render(0,pixels));
	}
	GdiFlush();
	REQUIRE(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS) <= gdi);
	REQUIRE(GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS) <= user);
Cleanup:
	return hr;
}
static HRESULT ImageListMasking(PCWSTR reportedStrip)
{
	HRESULT hr;
	TStackRef<QFI::CImageListData> data;
	TStackRef<QFI::CImageListData> restored;
	TStackRef<IJSONValue> before;
	TStackRef<IJSONValue> after;
	QFI::Raster pixels;
	Check(ImageListFromFile(L"mask-strip.bmp",16,16,QFI::ImageContain,TRUE,FALSE,&data));
	REQUIRE(data->Count() == 2 && !data->ColorKeyEnabled());
	Check(data->Render(0,pixels));
	REQUIRE(pixels.pixels[0] == 255 && pixels.pixels[1] == 252 && pixels.pixels[3] == 255);
	Check(data->Images(&before));
	QFI::FailAllocationAfter(0);
	hr = data->SetColorKey(TRUE);
	QFI::FailAllocationAfter(-1);
	REQUIRE(hr == E_OUTOFMEMORY && !data->ColorKeyEnabled());
	Check(data->Images(&after));
	REQUIRE(JSONCompare(before,after) == 0);
	after.Release();
	Check(data->SetColorKey(TRUE));
	REQUIRE(data->ColorKeyEnabled());
	const UINT depths[] = {24,32};
	for(UINT n = 0; n < ARRAYSIZE(depths); n++)
	{
		Check(data->Configure(16,16,depths[n],TRUE));
		Check(data->Render(0,pixels));
		REQUIRE(pixels.pixels[3] == 0 && pixels.pixels[(4 * 16 + 4) * 4 + 3] == 255);
		Check(data->Render(1,pixels));
		// The second cell's top-left is red, but the key belongs to the strip.
		REQUIRE(pixels.pixels[2] == 255 && pixels.pixels[3] == 255 && pixels.pixels[7] == 0);
	}
	Check(data->Images(&after));
	Check(QFI::CImageListData::Create(after,16,16,32,TRUE,&restored));
	REQUIRE(restored->ColorKeyEnabled());
	Check(restored->Render(0,pixels));
	REQUIRE(pixels.pixels[3] == 0);
	Check(data->Configure(32,24,32,TRUE));
	Check(data->Render(0,pixels));
	BOOL cleanEdges = TRUE;
	for(UINT n = 0; n < pixels.width * pixels.height; n++)
	{
		if(pixels.pixels[n * 4] || pixels.pixels[n * 4 + 1])
		{
			cleanEdges = FALSE;
			break;
		}
	}
	REQUIRE(cleanEdges);
	Check(data->SetColorKey(FALSE));
	REQUIRE(!data->ColorKeyEnabled());
	after.Release();
	Check(data->Images(&after));
	REQUIRE(JSONCompare(before,after) == 0);
	Check(data->Configure(16,16,24,TRUE));
	Check(data->Render(0,pixels));
	REQUIRE(pixels.pixels[0] == 255 && pixels.pixels[1] == 252 && pixels.pixels[3] == 255);
	{
		TStackRef<QFI::CImageListData> grayscale;
		Check(ImageListFromFile(L"mask-gray.bmp",16,16,QFI::ImageCrop,FALSE,TRUE,&grayscale));
		Check(grayscale->Render(0,pixels));
		// Opaque black must survive beside transparent pixels with RGB zero.
		REQUIRE(pixels.pixels[3] == 0 && pixels.pixels[(4 * 16 + 4) * 4 + 3] == 255);
	}
	if(reportedStrip)
	{
		TStackRef<IJSONArray> imported;
		TStackRef<IJSONValue> images;
		Check(QFI::ImportImageListFile(reportedStrip,16,16,QFI::ImageCrop,TRUE,FALSE,&imported));
		Check(JSONWrapArray(imported,&images));
		data.Release();
		Check(QFI::CImageListData::Create(images,16,16,24,TRUE,&data));
		REQUIRE(data->Count() == 61);
		before.Release();
		Check(data->Images(&before));
		UINT existingTransparent = 0;
		UINT cyan = 0;
		for(UINT n = 0; n < data->Count(); n++)
		{
			Check(data->Render(n,pixels));
			for(UINT p = 0; p < pixels.width * pixels.height; p++)
			{
				const BYTE* pixel = pixels.pixels + p * 4;
				if(!pixel[3])
				{
					existingTransparent++;
				}
				else if(pixel[0] == 255 && pixel[1] == 252 && !pixel[2])
				{
					cyan++;
				}
			}
		}
		Check(data->SetColorKey(TRUE));
		UINT transparent = 0;
		for(UINT n = 0; n < data->Count(); n++)
		{
			Check(data->Render(n,pixels));
			for(UINT p = 0; p < pixels.width * pixels.height; p++)
			{
				if(!pixels.pixels[p * 4 + 3])
				{
					transparent++;
				}
			}
		}
		CRString result;
		Check(RStrFormatW(&result,L"Reported bitmap: %u cells, %u cyan and %u initially transparent pixels; %u transparent after masking.\r\n",data->Count(),cyan,existingTransparent,transparent));
		Log(result.c_wstr());
		REQUIRE(transparent == 7453);
		Check(data->SetColorKey(FALSE));
		after.Release();
		Check(data->Images(&after));
		REQUIRE(JSONCompare(before,after) == 0);
	}
Cleanup:
	QFI::FailAllocationAfter(-1);
	return hr;
}

static HRESULT CaptureImageListDialog(HWND window)
{
	HRESULT hr;
	HDC dc = NULL;
	HBITMAP bitmap = NULL;
	HGDIOBJ previous = NULL;
	HANDLE file = INVALID_HANDLE_VALUE;
	PVOID bits;
	RECT rect;
	BITMAPINFO info = {0};
	QFI::Raster rgba;
	CMemoryStream png;
	WCHAR path[MAX_PATH];
	DWORD written;
	GetWindowRect(window,&rect);
	UINT width = rect.right - rect.left;
	UINT height = rect.bottom - rect.top;
	Check(rgba.Allocate(width,height));
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = width;
	info.bmiHeader.biHeight = -static_cast<INT>(height);
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	dc = CreateCompatibleDC(NULL);
	CheckIf(!dc,E_FAIL);
	bitmap = CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,NULL,0);
	CheckIf(!bitmap,E_FAIL);
	previous = SelectObject(dc,bitmap);
	CheckIf(!previous || previous == HGDI_ERROR,E_FAIL);
	RedrawWindow(window,NULL,NULL,RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);
	{
		HDC source = GetWindowDC(window);
		CheckIf(!source,E_FAIL);
		BOOL copied = BitBlt(dc,0,0,width,height,source,0,0,SRCCOPY);
		ReleaseDC(window,source);
		CheckIf(!copied,E_FAIL);
	}
	GdiFlush();
	for(UINT n = 0; n < width * height; n++)
	{
		const BYTE* source = static_cast<const BYTE*>(bits) + n * 4;
		BYTE* target = rgba.pixels + n * 4;
		target[0] = source[2];
		target[1] = source[1];
		target[2] = source[0];
		target[3] = 255;
	}
	Check(QFI::EncodePNG(rgba.pixels,width,height,png));
	CheckIf(!GetModuleFileNameW(NULL,path,ARRAYSIZE(path)),E_FAIL);
	PWSTR slash = const_cast<PWSTR>(TStrRChr(path,L'\\'));
	CheckIf(!slash,E_FAIL);
	Check(TStrCchCpy(slash + 1,static_cast<INT>(ARRAYSIZE(path) - (slash + 1 - path)),L"ImageListEditor.png"));
	file = CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
	CheckIf(file == INVALID_HANDLE_VALUE,E_FAIL);
	CheckIf(!WriteFile(file,png.GetReadPtr(),png.DataRemaining(),&written,NULL) || written != png.DataRemaining(),E_FAIL);
Cleanup:
	if(file != INVALID_HANDLE_VALUE)
	{
		CloseHandle(file);
	}
	if(previous && previous != HGDI_ERROR)
	{
		SelectObject(dc,previous);
	}
	if(bitmap)
	{
		DeleteObject(bitmap);
	}
	if(dc)
	{
		DeleteDC(dc);
	}
	return hr;
}
static BOOL s_imageListDialogSeen;
static UINT s_imageListDialogTicks;
static BOOL s_imageListDialogNoChange;
static HRESULT s_imageListDialogResult;
static BOOL CALLBACK DriveImageListDialog(HWND window, LPARAM)
{
	if(!IsWindowVisible(window) || !GetDlgItem(window,700) || !GetDlgItem(window,735))
	{
		return TRUE;
	}
	if(s_imageListDialogSeen)
	{
		return FALSE;
	}
	if(++s_imageListDialogTicks < 4)
	{
		return FALSE;
	}
	if(s_imageListDialogNoChange)
	{
		s_imageListDialogSeen = TRUE;
		s_imageListDialogResult = S_OK;
		PostMessageW(window,WM_COMMAND,IDOK,0);
		return FALSE;
	}
	if(s_imageListDialogTicks == 4)
	{
		HWND grid = GetDlgItem(window,700);
		// Use actual dialog/grid messages for cross-row dragging and the buttons.
		RECT cell = {0,0,56,56};
		MapDialogRect(window,&cell);
		INT cx = cell.right;
		INT cy = cell.bottom;
		SendMessageW(grid,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(cx / 2,cy / 2));
		SendMessageW(grid,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(cx + cx / 2,cy + cy / 2));
		SendMessageW(grid,WM_LBUTTONUP,0,MAKELPARAM(cx + cx / 2,cy + cy / 2));
		WCHAR text[256];
		GetDlgItemTextW(window,702,text,ARRAYSIZE(text));
		s_imageListDialogResult = !TStrCmpNAssert(text,L"Index: 6 ",StaticLength(L"Index: 6 ")) ? S_OK : E_FAIL;
		SendMessageW(window,WM_COMMAND,734,0);
		GetDlgItemTextW(window,702,text,ARRAYSIZE(text));
		if(TStrCmpNAssert(text,L"Index: 5 ",StaticLength(L"Index: 5 ")))
		{
			s_imageListDialogResult = E_FAIL;
		}
		SendMessageW(window,WM_COMMAND,735,0);
		if(!IsWindowEnabled(GetDlgItem(window,734)) || !IsWindowEnabled(GetDlgItem(window,735)))
		{
			s_imageListDialogResult = E_FAIL;
		}
		return FALSE;
	}
	if(s_imageListDialogTicks < 10)
	{
		return FALSE;
	}
	s_imageListDialogSeen = TRUE;
	if(SUCCEEDED(s_imageListDialogResult))
	{
		s_imageListDialogResult = CaptureImageListDialog(window);
	}
	SendMessageW(window,WM_COMMAND,0x3021,0);
	// A deletion after Apply is discarded by Cancel.
	SendMessageW(window,WM_COMMAND,732,0);
	PostMessageW(window,WM_COMMAND,IDCANCEL,0);
	return FALSE;
}
static VOID CALLBACK ImageListDialogTimer(HWND owner, UINT, UINT_PTR identity, DWORD)
{
	EnumThreadWindows(GetCurrentThreadId(),DriveImageListDialog,0);
	if(s_imageListDialogSeen)
	{
		KillTimer(owner,identity);
	}
}
static HRESULT ImageListPreviewColor(HWND dialog, COLORREF* color)
{
	HRESULT hr = S_OK;
	HWND preview = GetDlgItem(dialog,701);
	HDC target = GetDC(preview);
	HDC dc = target ? CreateCompatibleDC(target) : NULL;
	HBITMAP bitmap = NULL;
	HGDIOBJ previous = NULL;
	DRAWITEMSTRUCT draw = {0};
	CheckIf(!target || !dc,E_FAIL);
	GetClientRect(preview,&draw.rcItem);
	bitmap = CreateCompatibleBitmap(target,draw.rcItem.right,draw.rcItem.bottom);
	CheckIf(!bitmap,E_FAIL);
	previous = SelectObject(dc,bitmap);
	CheckIf(!previous || previous == HGDI_ERROR,E_FAIL);
	draw.CtlType = ODT_STATIC;
	draw.CtlID = 701;
	draw.hwndItem = preview;
	draw.hDC = dc;
	SendMessageW(dialog,WM_DRAWITEM,701,reinterpret_cast<LPARAM>(&draw));
	*color = GetPixel(dc,draw.rcItem.right / 2,2);
	CheckIf(*color == CLR_INVALID,E_FAIL);
Cleanup:
	if(previous && previous != HGDI_ERROR)
	{
		SelectObject(dc,previous);
	}
	if(bitmap)
	{
		DeleteObject(bitmap);
	}
	if(dc)
	{
		DeleteDC(dc);
	}
	if(target)
	{
		ReleaseDC(preview,target);
	}
	return hr;
}
static BOOL s_imageListMaskSeen;
static BOOL s_imageListMaskInitial;
static BOOL s_imageListMaskEnable;
static BOOL s_imageListMaskApply;
static HRESULT s_imageListMaskResult;
static BOOL CALLBACK DriveImageListMask(HWND window, LPARAM)
{
	if(s_imageListMaskSeen || !IsWindowVisible(window) || !GetDlgItem(window,726) || !GetDlgItem(window,700))
	{
		return TRUE;
	}
	s_imageListMaskSeen = TRUE;
	COLORREF color = CLR_INVALID;
	s_imageListMaskResult = ImageListPreviewColor(window,&color);
	if((IsDlgButtonChecked(window,726) == BST_CHECKED) != (s_imageListMaskInitial != FALSE) ||
		((color == RGB(0,252,255)) == (s_imageListMaskInitial != FALSE)))
	{
		s_imageListMaskResult = E_FAIL;
	}
	CheckDlgButton(window,726,s_imageListMaskEnable ? BST_CHECKED : BST_UNCHECKED);
	SendMessageW(window,WM_COMMAND,MAKEWPARAM(726,BN_CLICKED),0);
	HRESULT rendered = ImageListPreviewColor(window,&color);
	if(FAILED(rendered) || ((color == RGB(0,252,255)) == (s_imageListMaskEnable != FALSE)))
	{
		s_imageListMaskResult = E_FAIL;
	}
	if(s_imageListMaskApply)
	{
		SendMessageW(window,WM_COMMAND,0x3021,0);
	}
	PostMessageW(window,WM_COMMAND,IDCANCEL,0);
	return FALSE;
}
static VOID CALLBACK ImageListMaskTimer(HWND owner, UINT, UINT_PTR identity, DWORD)
{
	EnumThreadWindows(GetCurrentThreadId(),DriveImageListMask,0);
	if(s_imageListMaskSeen)
	{
		KillTimer(owner,identity);
	}
}
static HRESULT ImageListMaskDialogTests(QFD::CDesignerEditor* editor, HWND host, REFGUID id)
{
	HRESULT hr;
	TStackRef<IJSONObject> original;
	TStackRef<QFD::CImageListEditSession> session;
	TStackRef<IJSONValue> before;
	TStackRef<IJSONValue> after;
	CRString path;
	CRString menuType;
	GUID menuID;
	HMENU menu = NULL;
	Check(editor->Document().Serialize(&original));
	Check(RStrCreateW(LSP(L"MainMenu"),&menuType));
	Check(editor->AddComponent(menuType,&menuID));
	REQUIRE(editor->CanExecute(QFD::MenuEditor));
	Check(editor->SelectID(id));
	REQUIRE(!editor->CanExecute(QFD::MenuEditor));
	menu = QFD::CreateControlMenu();
	CheckIf(!menu,E_FAIL);
	editor->UpdateCommandMenu(menu);
	REQUIRE((GetMenuState(menu,QFD::MenuEditor,MF_BYCOMMAND) & MF_GRAYED) != 0);
	REQUIRE((GetMenuState(menu,QFD::ImageListEditor,MF_BYCOMMAND) & MF_GRAYED) == 0);
	Check(ImageListFixturePath(L"mask-strip.bmp",&path));
	session.Attach(__new QFD::CImageListEditSession);
	CheckAlloc(session);
	Check(session->Initialize(editor,id));
	session->data->Clear();
	PCWSTR files[] = {path.c_wstr()};
	BOOL split[] = {TRUE};
	Check(session->Import(files,1,QFI::ImageCrop,split,FALSE));
	Check(session->Apply());
	Check(session->data->Images(&before));
	session.Release();
	for(UINT n = 0; n < 3; n++)
	{
		editor->Document().SetDirty(FALSE);
		s_imageListMaskSeen = FALSE;
		s_imageListMaskInitial = n != 0;
		s_imageListMaskEnable = n == 0;
		s_imageListMaskApply = n != 1;
		s_imageListMaskResult = E_FAIL;
		CheckIf(!SetTimer(host,7022,30,ImageListMaskTimer),E_FAIL);
		Check(editor->EditImageList());
		KillTimer(host,7022);
		REQUIRE(s_imageListMaskSeen);
		Check(s_imageListMaskResult);
		REQUIRE((editor->Document().Dirty() != FALSE) == (s_imageListMaskApply != FALSE));
		session.Attach(__new QFD::CImageListEditSession);
		CheckAlloc(session);
		Check(session->Initialize(editor,id));
		REQUIRE((session->data->ColorKeyEnabled() != FALSE) == (n != 2));
		QFI::Raster pixels;
		Check(session->data->Render(0,pixels));
		REQUIRE(pixels.pixels[3] == (n == 2 ? 255 : 0));
		if(n == 2)
		{
			Check(session->data->Images(&after));
			REQUIRE(JSONCompare(before,after) == 0);
		}
		session.Release();
	}
	Check(editor->LoadDefinition(original));
	Check(editor->SelectID(id));
Cleanup:
	KillTimer(host,7022);
	if(menu)
	{
		DestroyMenu(menu);
	}
	return hr;
}
static HRESULT DesignerImageLists(PCWSTR reportedStrip = NULL)
{
	HRESULT hr;
	HWND host = Host();
	TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONObject> saved;
	TStackRef<IJSONObject> again;
	TStackRef<IQuadooComponent> component;
	TStackRef<QFD::CImageListEditSession> session;
	CRString savedPath;
	GUID id;
	GUID loaded;
	CheckIf(!host,E_FAIL);
	Check(ImageListPixelsAndImports());
	Check(ImageListResourceCycles());
	Check(ImageListMasking(reportedStrip));
	Check(CreateEditor(host,&editor));
	Check(Parse(ImageListNativeDefinition,&definition));
	Check(editor->LoadDefinition(definition));
	{
		TStackRef<IQuadooComponentHost> owner;
		TStackRef<IQuadooComponentCollection> components;
		Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&owner)));
		Check(owner->GetComponents(&components));
		Check(components->FindComponent(RSTRING_CAST(L"ImageList1"),&component));
	}
	Check(component->GetID(&id));
	Check(editor->SelectID(id));
	REQUIRE(editor->CanExecute(QFD::ImageListEditor));
	REQUIRE(ListView_GetItemCount(editor->TrayWindow()) == 1);
	REQUIRE(InspectorRow(editor,L"Images") >= 0 && InspectorRow(editor,L"ImageCount") >= 0);
	Check(ImageListMaskDialogTests(editor,host,id));
	Check(ImageListSessionTests(editor,id));
	s_imageListDialogSeen = FALSE;
	s_imageListDialogTicks = 0;
	s_imageListDialogNoChange = FALSE;
	s_imageListDialogResult = E_FAIL;
	CheckIf(!SetTimer(host,7021,30,ImageListDialogTimer),E_FAIL);
	{
		NMITEMACTIVATE click = {0};
		click.hdr.hwndFrom = editor->TrayWindow();
		click.hdr.idFrom = GetDlgCtrlID(editor->TrayWindow());
		click.hdr.code = NM_DBLCLK;
		click.iItem = 0;
		SendMessageW(editor->Window(),WM_NOTIFY,click.hdr.idFrom,reinterpret_cast<LPARAM>(&click));
	}
	KillTimer(host,7021);
	REQUIRE(s_imageListDialogSeen);
	Check(s_imageListDialogResult);
	session.Attach(__new QFD::CImageListEditSession);
	Check(session->Initialize(editor,id));
	REQUIRE(session->data->Count() == 16);
	Check(editor->Document().Serialize(&saved));
	Check(ImageListFixturePath(L"..\\ImageListExample.qform",&savedPath));
	Check(editor->SaveDocument(savedPath.c_wstr()));
	REQUIRE(!editor->Document().Dirty());
	Check(editor->OpenDocument(savedPath.c_wstr()));
	Check(editor->Document().Serialize(&again));
	REQUIRE(JSONCompareObjects(saved,again) == 0);
	Check(editor->SelectID(id));
	// Opening the Images inspector editor and accepting an unchanged collection
	// must preserve both the serialized definition and the clean state.
	s_imageListDialogSeen = FALSE;
	s_imageListDialogTicks = 0;
	s_imageListDialogNoChange = TRUE;
	CheckIf(!SetTimer(host,7021,30,ImageListDialogTimer),E_FAIL);
	StartInline(editor,L"Images");
	KillTimer(host,7021);
	REQUIRE(s_imageListDialogSeen && !editor->Document().Dirty());
	Check(s_imageListDialogResult);
	again.Release();
	Check(editor->Document().Serialize(&again));
	REQUIRE(JSONCompareObjects(saved,again) == 0);
	component.Release();
	Check(editor->Document().FindComponent(id,&component));
	Check(component->GetID(&loaded));
	REQUIRE(IsEqualGUID(id,loaded));
	// Frames share the ordinary nonvisual factory, tray and persistence paths.
	Check(editor->NewDocument(FALSE));
	Check(PickTool(editor,L"ImageList"));
	Check(editor->GetSelectedID(&id));
	component.Release();
	Check(editor->Document().FindComponent(id,&component));
	Check(component->SetName(RSTRING_CAST(L"FrameImages")));
	saved.Release();
	Check(editor->Document().Serialize(&saved));
	Check(editor->LoadDefinition(saved));
	component.Release();
	Check(editor->Document().FindComponent(id,&component));
	Check(editor->SelectID(id));
	Check(editor->Delete());
	component.Release();
	REQUIRE(FAILED(editor->Document().FindComponent(id,&component)));
Cleanup:
	if(host)
	{
		KillTimer(host,7021);
	}
	editor.Release();
	if(host)
	{
		DestroyWindow(host);
	}
	return hr;
}
static HRESULT ImageListSessionTests(QFD::CDesignerEditor* editor, REFGUID id)
{
	HRESULT hr;
	TStackRef<QFD::CImageListEditSession> session;
	TStackRef<QFD::CImageListEditSession> restored;
	TStackRef<IJSONObject> original;
	TStackRef<IJSONObject> after;
	TStackRef<IJSONValue> beforeImages;
	TStackRef<IJSONValue> afterImages;
	CRString paths[16];
	PCWSTR files[16];
	static PCWSTR const extensions[] = {L"png",L"bmp",L"ico"};
	Check(editor->Document().Serialize(&original));
	editor->Document().SetDirty(FALSE);
	session.Attach(__new QFD::CImageListEditSession);
	CheckAlloc(session);
	Check(session->Initialize(editor,id));
	REQUIRE(session->Apply() == S_FALSE && !editor->Document().Dirty());
	session->data->Clear();
	for(UINT n = 0; n < 16; n++)
	{
		CRString name;
		Check(RStrFormatW(&name,L"%.2u.%ls",n,extensions[n % 3]));
		Check(ImageListFixturePath(name.c_wstr(),&paths[n]));
		files[n] = paths[n].c_wstr();
	}
	Check(session->Import(files,16,QFI::ImageContain,NULL,FALSE));
	REQUIRE(session->data->Count() == 16 && !editor->Document().Dirty());
	Check(session->data->Images(&beforeImages));
	{
		CRString invalid;
		Check(ImageListFixturePath(L"invalid.png",&invalid));
		PCWSTR batch[] = {files[0],invalid.c_wstr()};
		REQUIRE(FAILED(session->Import(batch,2,QFI::ImageCrop,NULL,FALSE)));
		REQUIRE(FAILED(session->Import(batch + 1,1,QFI::ImageCrop,NULL,FALSE,3)));
		Check(session->data->Images(&afterImages));
		REQUIRE(JSONCompare(beforeImages,afterImages) == 0);
		afterImages.Release();
	}
	Check(session->Import(files + 1,1,QFI::ImageStretch,NULL,FALSE,3));
	REQUIRE(session->data->Count() == 16);
	Check(session->data->Move(5,4));
	Check(session->data->Move(4,10));
	{
		TStackRef<IJSONArray> before;
		TStackRef<IJSONArray> after;
		TStackRef<IJSONValue> expected;
		TStackRef<IJSONValue> moved;
		Check(beforeImages->GetArray(&before));
		Check(before->GetValue(5,&expected));
		Check(session->data->Images(&afterImages));
		Check(afterImages->GetArray(&after));
		Check(after->GetValue(10,&moved));
		REQUIRE(JSONCompare(expected,moved) == 0);
		afterImages.Release();
	}
	Check(session->data->Remove(15));
	REQUIRE(session->data->Count() == 15);
	Check(editor->Document().Serialize(&after));
	REQUIRE(JSONCompareObjects(original,after) == 0);
	after.Release();
	// Discarding a working collection is Cancel, including its dimension changes.
	Check(session->data->Configure(24,32,8,FALSE));
	session.Release();
	Check(editor->Document().Serialize(&after));
	REQUIRE(JSONCompareObjects(original,after) == 0 && !editor->Document().Dirty());
	after.Release();
	session.Attach(__new QFD::CImageListEditSession);
	Check(session->Initialize(editor,id));
	session->data->Clear();
	Check(session->Import(files,16,QFI::ImageContain,NULL,FALSE));
	Check(session->data->Configure(24,32,32,TRUE));
	QFI::FailAllocationAfter(0);
	hr = session->Apply();
	QFI::FailAllocationAfter(-1);
	REQUIRE(hr == E_OUTOFMEMORY && !editor->Document().Dirty());
	{
		struct RejectImageListEdit : QFD::ISourceSite
		{
			virtual HRESULT PrepareDefinition(IJSONObject*, QFD::ISourceEdit** edit)
			{
				*edit = NULL;
				return E_ABORT;
			}
			virtual HRESULT OpenHandler(REFGUID, RSTRING)
			{
				return E_NOTIMPL;
			}
		} rejected;
		editor->Document().SetSourceSite(&rejected);
		hr = session->Apply();
		editor->Document().SetSourceSite(NULL);
		REQUIRE(hr == E_ABORT && !editor->Document().Dirty());
		Check(editor->Document().Serialize(&after));
		REQUIRE(JSONCompareObjects(original,after) == 0);
		after.Release();
	}
	Check(session->Apply());
	REQUIRE(session->committed && editor->Document().Dirty());
	Check(editor->Document().Serialize(&after));
	session->data->Clear();
	session.Release();
	// Apply followed by further edits/Cancel retains the last applied state.
	restored.Attach(__new QFD::CImageListEditSession);
	Check(restored->Initialize(editor,id));
	REQUIRE(restored->data->Count() == 16 && restored->data->width == 24 && restored->data->height == 32);
	REQUIRE(restored->Apply() == S_FALSE);
	Check(editor->LoadDefinition(after));
	original.Release();
	Check(editor->Document().Serialize(&original));
	REQUIRE(JSONCompareObjects(original,after) == 0);
	Check(editor->SelectID(id));
Cleanup:
	QFI::FailAllocationAfter(-1);
	return hr;
}
