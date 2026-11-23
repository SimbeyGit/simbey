#include "..\TestImages\ImageTestSupport.h"
#include "..\..\..\..\platform\QuadooForm\Imaging\ImageResource.h"

static HWND g_pictureOwner;
static BOOL g_pictureClear, g_pictureAccept, g_pictureSeen, g_pictureCorrect;
static UINT g_pictureTicks;
static PCWSTR g_pictureCaption;
static UINT g_pictureChoices, g_pictureChoice;
static BOOL CALLBACK ImageDialog(HWND window, LPARAM)
{
	WCHAR type[32], title[64];
	GetClassNameW(window,type,ARRAYSIZE(type)); GetWindowTextW(window,title,ARRAYSIZE(title));
	if(lstrcmpW(type,L"#32770") || lstrcmpW(title,g_pictureCaption) || GetWindow(window,GW_OWNER) != g_pictureOwner) return TRUE;
	g_pictureSeen = TRUE;
	if(g_pictureChoices)
	{
		HWND combo = FindWindowExW(window,NULL,L"COMBOBOX",NULL);
		g_pictureCorrect = combo && SendMessageW(combo,CB_GETCOUNT,0,0) == g_pictureChoices;
		if(combo) SendMessageW(combo,CB_SETCURSEL,g_pictureChoice,0);
		SendMessageW(window,WM_COMMAND,IDOK,0);
		return FALSE;
	}
	HWND preview = GetDlgItem(window,12);
	g_pictureCorrect = preview && (GetWindowLongW(preview,GWL_STYLE) & SS_TYPEMASK) == SS_OWNERDRAW && GetDlgItem(window,10) && GetDlgItem(window,11) && !FindWindowExW(window,NULL,L"EDIT",NULL);
	RedrawWindow(window,NULL,NULL,RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
	if(g_pictureClear) SendMessageW(window,WM_COMMAND,11,0);
	SendMessageW(window,WM_COMMAND,g_pictureAccept ? IDOK : IDCANCEL,0);
	return FALSE;
}
static VOID CALLBACK ImageDialogTimer(HWND window, UINT, UINT_PTR timer, DWORD)
{
	if(++g_pictureTicks > 100) { KillTimer(window,timer); PostQuitMessage(1); return; }
	EnumThreadWindows(GetCurrentThreadId(),ImageDialog,0);
}
static HRESULT DrivePictureDialog(QFD::CDesignerEditor* editor, HWND host, BOOL clear, BOOL accept)
{
	HRESULT hr = S_OK;
	g_pictureOwner = host; g_pictureClear = clear; g_pictureAccept = accept;
	g_pictureCaption = L"Picture"; g_pictureChoices = 0;
	g_pictureSeen = g_pictureCorrect = FALSE; g_pictureTicks = 0;
	CheckIf(!SetTimer(host,5410,10,ImageDialogTimer),HrEnsureLastError());
	StartInline(editor,L"Picture");
	REQUIRE(g_pictureSeen && g_pictureCorrect);
Cleanup:
	KillTimer(host,5410);
	return hr;
}
static HWND g_imagePreviewOwner, g_imagePreviewWindow;
static CImageTestCanvas* g_imagePreviewCanvas;
static BOOL g_imagePreviewCorrect;
static BOOL CALLBACK ImagePreview(HWND window, LPARAM)
{
	WCHAR type[64];
	GetClassNameW(window,type,ARRAYSIZE(type));
	if(lstrcmpW(type,L"QuadooForm.Native.v2") || GetWindow(window,GW_OWNER) != g_imagePreviewOwner) return TRUE;
	g_imagePreviewWindow = window;
	HWND content = GetWindow(window,GW_CHILD);
	if(content)
	{
		g_imagePreviewCanvas->Fill(RGB(99,88,77));
		SendMessageW(content,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(g_imagePreviewCanvas->DC()),PRF_CLIENT);
		g_imagePreviewCorrect = g_imagePreviewCanvas->Pixel(25,65) == RGB(255,0,0) && g_imagePreviewCanvas->Pixel(95,65) == RGB(17,34,51);
	}
	EnumChildWindows(window,ClickPreviewButton,0);
	return FALSE;
}
static VOID CALLBACK ImagePreviewTimer(HWND window, UINT, UINT_PTR timer, DWORD)
{
	if(++g_pictureTicks > 100) { KillTimer(window,timer); PostQuitMessage(1); return; }
	EnumThreadWindows(GetCurrentThreadId(),ImagePreview,0);
}
static HRESULT DesignerImages()
{
	HRESULT hr;
	HWND host = Host();
	TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IJSONObject> definition, before, after;
	TStackRef<IQuadooControlContainer> root;
	TStackRef<IQuadooControl> image, button;
	TStackRef<IQuadooStaticControl> painted;
	TStackRef<IJSONValue> value, picture, icon, stored;
	CImageTestCanvas design, preview;
	WCHAR path[MAX_PATH] = {0}, imported[MAX_PATH] = {0};
	GUID id, selected, stable;
	QuadooForm::Rect bounds;
	CheckIf(!host,HrEnsureLastError());
	Check(CreateEditor(host,&editor));
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"ImageDesigner\",\"properties\":{\"Width\":280,\"Height\":180,\"BackColor\":3351057},\"events\":{\"OnCreate\":\"DoNotInvoke\"},\"children\":[{\"type\":\"Button\",\"name\":\"OK\",\"properties\":{\"Caption\":\"OK\",\"Left\":10,\"Top\":10,\"ModalResult\":1}}]}",&definition));
	Check(editor->LoadDefinition(definition));
	Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root)));
	HWND tools = GetDlgItem(editor->Window(),102);
	INT row = FindTool(tools,L"Image");
	REQUIRE(row > 0);
	{
		LVITEMW item = {0}; item.mask = LVIF_IMAGE; item.iItem = row;
		REQUIRE(ListView_GetItem(tools,&item) && item.iImage >= 4);
	}
	Check(PickTool(editor,L"Image"));
	{
		POINT point = {30,60};
		MapWindowPoints(editor->Document().Host(),editor->CanvasWindow(),&point,1);
		SendMessageW(editor->OverlayWindow(),WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(point.x,point.y));
		SendMessageW(editor->OverlayWindow(),WM_LBUTTONUP,0,MAKELPARAM(point.x,point.y));
	}
	Check(root->FindControl(RSTRING_CAST(L"Image1"),&image));
	Check(image->GetID(&id));
	Check(editor->GetSelectedID(&selected));
	REQUIRE(IsEqualGUID(id,selected) && FindTree(editor->TreeWindow(),TreeView_GetRoot(editor->TreeWindow()),L"Image1 (Image)") != NULL);
	Check(image->QueryInterface(__uuidof(IQuadooStaticControl),reinterpret_cast<PVOID*>(&painted)));
	bounds.Left = 20; bounds.Top = 60; bounds.Width = 80; bounds.Height = 40;
	Check(editor->SetSelectionBounds(&bounds));
	{
		RECT selection;
		DWORD capabilities;
		Check(editor->SelectionCapabilities(&capabilities));
		REQUIRE(capabilities == (QuadooForm::DesignMoveX | QuadooForm::DesignMoveY | QuadooForm::DesignResizeWidth | QuadooForm::DesignResizeHeight));
		Check(editor->SelectionRect(&selection));
		SendMessageW(editor->OverlayWindow(),WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(selection.right,selection.bottom));
		SendMessageW(editor->OverlayWindow(),WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(selection.right + 20,selection.bottom + 10));
		SendMessageW(editor->OverlayWindow(),WM_LBUTTONUP,0,MAKELPARAM(selection.right + 20,selection.bottom + 10));
		QuadooForm::Rect resized;
		Check(image->GetBounds(&resized));
		REQUIRE(resized.Width == 100 && resized.Height == 50);
		Check(editor->SetSelectionBounds(&bounds));
		Check(editor->SelectionRect(&selection));
		editor->SelectAncestor(TRUE);
		SendMessageW(editor->OverlayWindow(),WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(selection.left + 10,selection.top + 10));
		SendMessageW(editor->OverlayWindow(),WM_LBUTTONUP,0,MAKELPARAM(selection.left + 10,selection.top + 10));
		Check(editor->GetSelectedID(&selected));
		REQUIRE(IsEqualGUID(id,selected));
	}
	Check(ImageTempFile(ImageRGBA,sizeof(ImageRGBA),imported));
	Check(QFI::ImportFile(imported,&picture));
	REQUIRE(DeleteFileW(imported)); imported[0] = 0;
	Check(Edit(editor,image,L"Picture",picture));
	for(UINT format = 0; format < 2; format++)
	{
		TStackRef<IJSONValue> converted;
		Check(ImageTempFile(format ? ImageBMP : ImageJPEG,format ? sizeof(ImageBMP) : sizeof(ImageJPEG),imported));
		Check(QFI::ImportFile(imported,&converted));
		REQUIRE(DeleteFileW(imported)); imported[0] = 0;
		Check(Edit(editor,image,L"Picture",converted));
		REQUIRE(ImageInteger(image,L"ImageWidth") == (format ? 3 : 2) && ImageInteger(image,L"ImageHeight") == (format ? 2 : 3));
	}
	Check(Edit(editor,image,L"Picture",picture));
	Check(ChooseProperty(editor,L"SizeMode",5,3));
	Check(ChooseProperty(editor,L"ImageAlignment",9,8));
	Check(ChooseProperty(editor,L"Interpolation",4,2));
	REQUIRE(ImageInteger(image,L"SizeMode") == 3 && ImageInteger(image,L"ImageAlignment") == 8 && ImageInteger(image,L"Interpolation") == 2);
	Check(JSONCreateInteger(1,&value));
	Check(Edit(editor,image,L"SizeMode",value));
	value.Release();
	Check(JSONCreateInteger(0,&value));
	Check(Edit(editor,image,L"Interpolation",value));
	value.Release();
	REQUIRE(editor->Document().Dirty() && ImageInteger(image,L"ImageWidth") == 4);
	Check(NewTemporary(path,ARRAYSIZE(path)));
	Check(editor->SaveDocument(path));
	Check(editor->Document().Serialize(&before));
	Check(DrivePictureDialog(editor,host,TRUE,FALSE));
	Check(editor->Document().Serialize(&after));
	REQUIRE(!JSONCompareObjects(before,after) && !editor->Document().Dirty()); after.Release();
	Check(DrivePictureDialog(editor,host,TRUE,TRUE));
	Check(ImageRead(image,L"Picture",&stored));
	REQUIRE(!stored && editor->Document().Dirty() && ImageInteger(image,L"ImageWidth") == 0);
	Check(Edit(editor,image,L"Picture",picture));
	Check(editor->GetSelectedID(&selected));
	REQUIRE(IsEqualGUID(id,selected));
	{
		TStackRef<IJSONValue> bad;
		Check(ImageFixtureValue(L"png",ImageRGBA,20,&bad));
		REQUIRE(FAILED(Edit(editor,image,L"Picture",bad)) && ImageInteger(image,L"ImageWidth") == 4);
	}
	Check(design.Create(320,200));
	Check(preview.Create(320,200));
	{
		TStackRef<IQuadooChildWindowHost> hostInterface;
		HWND content;
		Check(root->QueryInterface(__uuidof(IQuadooChildWindowHost),reinterpret_cast<PVOID*>(&hostInterface)));
		Check(hostInterface->GetChildHostHandle(&content));
		design.Fill(RGB(99,88,77));
		SendMessageW(content,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(design.DC()),PRF_CLIENT);
		REQUIRE(design.Pixel(25,65) == RGB(255,0,0) && design.Pixel(95,65) == RGB(17,34,51));
	}
	g_imagePreviewOwner = host; g_imagePreviewWindow = NULL; g_imagePreviewCanvas = &preview; g_imagePreviewCorrect = FALSE; g_pictureTicks = 0;
	CheckIf(!SetTimer(host,5411,10,ImagePreviewTimer),HrEnsureLastError());
	{
		LRESULT result;
		hr = editor->PreviewForm(host,&result);
		KillTimer(host,5411);
		Check(hr);
		REQUIRE(result == IDOK && g_imagePreviewWindow && !IsWindow(g_imagePreviewWindow) && g_imagePreviewCorrect && IsWindowEnabled(host));
	}
	REQUIRE(design.Pixel(25,65) == preview.Pixel(25,65) && design.Pixel(95,65) == preview.Pixel(95,65));
	Check(editor->GetSelectedID(&selected));
	REQUIRE(IsEqualGUID(id,selected));
	Check(ImageFixtureValue(L"ico",ImageMultiICO,sizeof(ImageMultiICO),&icon));
	Check(Edit(editor,image,L"Picture",icon));
	Check(editor->SaveDocument(path));
	painted.Release(); image.Release(); root.Release();
	Check(editor->OpenDocument(path));
	Check(editor->Document().Find(id,&image));
	Check(image->GetID(&stable));
	Check(ImageRead(image,L"Picture",&stored));
	REQUIRE(IsEqualGUID(id,stable) && !JSONCompare(stored,icon) && ImageInteger(image,L"ImageWidth") == 32);
	Check(editor->SelectID(id));
	Check(DrivePictureDialog(editor,host,TRUE,TRUE));
	Check(editor->SaveDocument(path));
	image.Release();
	Check(editor->OpenDocument(path));
	Check(editor->Document().Find(id,&image));
	stored.Release();
	Check(ImageRead(image,L"Picture",&stored));
	REQUIRE(!stored && ImageInteger(image,L"ImageWidth") == 0);
	{
		WCHAR example[MAX_PATH];
		DWORD length = GetModuleFileNameW(NULL,example,ARRAYSIZE(example));
		CheckIf(!length || length >= ARRAYSIZE(example),E_INVALIDARG);
		while(length && example[length - 1] != L'\\') length--;
		Check(TStrCchCpy(example + length,ARRAYSIZE(example) - length,L"DesignerExamples\\ImageGallery.qform.json"));
		image.Release();
		Check(editor->OpenDocument(example));
		Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root)));
		Check(root->FindControl(RSTRING_CAST(L"photograph"),&image));
		REQUIRE(ImageInteger(image,L"SizeMode") == 3 && ImageInteger(image,L"ImageWidth") == 590);
		image.Release();
		Check(root->FindControl(RSTRING_CAST(L"pixelArt"),&image));
		REQUIRE(ImageInteger(image,L"Interpolation") == 0 && ImageInteger(image,L"ImageWidth") == 16);
	}
	hr = S_OK;
Cleanup:
	if(host) { KillTimer(host,5410); KillTimer(host,5411); DestroyWindow(host); }
	editor.Release();
	if(path[0]) DeleteFileW(path);
	if(imported[0]) DeleteFileW(imported);
	g_imagePreviewCanvas = NULL;
	return hr;
}
