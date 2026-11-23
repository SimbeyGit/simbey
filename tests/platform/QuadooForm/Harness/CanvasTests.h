// Independent ImageList capability; no private runtime class is used.
class CCanvasTestImages : public CBaseUnknown, public IQuadooImageList, public IQuadooImageListSnapshot
{
	HIMAGELIST m_images;
	DWORD m_thread;
public:
	TStackRef<IQuadooControl> destroyDuringAcquire;
	TStackRef<IQuadooCanvas> reenterDuringAcquire;
	HRESULT nestedResult;
	UINT requestedDPI;
	UINT observers;
	BOOL destroyed;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooImageList)
		UNK_INTERFACE(IQuadooImageListSnapshot)
	END_UNK_MAP
	CCanvasTestImages() : m_images(NULL), m_thread(GetCurrentThreadId()), nestedResult(S_OK), requestedDPI(0), observers(0), destroyed(FALSE)
	{
	}
	~CCanvasTestImages()
	{
		if(m_images)
		{
			ImageList_Destroy(m_images);
		}
	}
	HRESULT Initialize(BOOL mask)
	{
		HRESULT hr;
		HBITMAP bitmap = NULL;
		BYTE* pixels;
		BITMAPINFO info = {0};
		info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		info.bmiHeader.biWidth = 3;
		info.bmiHeader.biHeight = -1;
		info.bmiHeader.biPlanes = 1;
		info.bmiHeader.biBitCount = 32;
		bitmap = CreateDIBSection(NULL,&info,DIB_RGB_COLORS,reinterpret_cast<PVOID*>(&pixels),NULL,0);
		CheckIf(!bitmap,HrEnsureLastError());
		static const BYTE alpha[] = {0,0,255,255,0,0,0,0,0,255,0,128};
		static const BYTE masked[] = {0,0,255,0,255,255,0,0,0,255,0,0};
		CopyMemory(pixels,mask ? masked : alpha,sizeof(alpha));
		m_images = ImageList_Create(3,1,(mask ? ILC_COLOR24 : ILC_COLOR32) | ILC_MASK,1,0);
		CheckIf(!m_images,HrEnsureLastError());
		CheckIf((mask ? ImageList_AddMasked(m_images,bitmap,RGB(0,255,255)) : ImageList_Add(m_images,bitmap,NULL)) != 0,E_FAIL);
		hr = S_OK;
	Cleanup:
		if(bitmap)
		{
			DeleteObject(bitmap);
		}
		return hr;
	}
	STDMETHOD(Acquire)(UINT dpi, IQuadooImageListSnapshot** snapshot)
	{
		HRESULT hr = S_OK;
		CheckIf(!snapshot,E_POINTER);
		*snapshot = NULL;
		CheckIf(m_thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
		CheckIf(destroyed,QuadooForm::EDestroyed);
		requestedDPI = dpi;
		if(reenterDuringAcquire)
		{
			nestedResult = reenterDuringAcquire->Clear(0);
			reenterDuringAcquire.Release();
		}
		if(destroyDuringAcquire)
		{
			Check(destroyDuringAcquire->Destroy());
			destroyDuringAcquire.Release();
		}
		*snapshot = static_cast<IQuadooImageListSnapshot*>(this);
		AddRef();
	Cleanup:
		return hr;
	}
	STDMETHOD(GetHandle)(HIMAGELIST* handle)
	{
		if(handle)
		{
			*handle = m_images;
		}
		return handle ? S_OK : E_POINTER;
	}
	STDMETHOD(GetSize)(UINT* width, UINT* height, UINT* dpi)
	{
		if(!width || !height || !dpi)
		{
			return E_POINTER;
		}
		*width = 3;
		*height = 1;
		*dpi = requestedDPI;
		return S_OK;
	}
	STDMETHOD(AddImageListObserver)(IQuadooImageListObserver*)
	{
		observers++;
		return S_OK;
	}
	STDMETHOD(RemoveImageListObserver)(IQuadooImageListObserver*)
	{
		return S_FALSE;
	}
};
static HRESULT CanvasBool(IQuadooControl* control, PCWSTR name, BOOL value)
{
	HRESULT hr;
	TStackRef<IQuadooProperties> properties;
	UINT index;
	Check(Property(control,name,&properties,&index));
	Check(properties->SetValueBool(index,value));
Cleanup:
	return hr;
}
class CCanvasTestBinder : public CBaseUnknown, public IQuadooEventsBinder, public IQuadooDelegate
{
public:
	UINT paints;
	UINT resizes;
	UINT clicks;
	BOOL destroyInPaint;
	BOOL resizeInPaint;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooEventsBinder)
		UNK_INTERFACE(IQuadooDelegate)
	END_UNK_MAP
	CCanvasTestBinder() : paints(0), resizes(0), clicks(0), destroyInPaint(FALSE), resizeInPaint(FALSE)
	{
	}
	STDMETHOD(BindEvent)(RSTRING, IQuadooDelegate** callback)
	{
		if(!callback)
		{
			return E_POINTER;
		}
		*callback = static_cast<IQuadooDelegate*>(this);
		AddRef();
		return S_OK;
	}
	STDMETHOD(PostBind)(IQuadooControl*)
	{
		return S_OK;
	}
	STDMETHOD(Invoke)(IQuadooEventArgs* args)
	{
		HRESULT hr;
		CRString name;
		TStackRef<IQuadooComponent> sender;
		TStackRef<IQuadooCanvas> canvas;
		TStackRef<IQuadooWinControl> control;
		HWND window;
		SIZE size;
		Check(args->GetEventName(&name));
		Check(args->GetSender(&sender));
		Check(sender->QueryInterface(&canvas));
		Check(sender->QueryInterface(&control));
		Check(control->GetHandle(&window));
		if(name == RSTRING_CAST(L"OnPaint"))
		{
			paints++;
			Check(canvas->SetFont(RSTRING_CAST(L"Arial"),10,FALSE,FALSE));
			Check(canvas->SetFont(RSTRING_CAST(L"Arial"),10,FALSE,FALSE));
			Check(canvas->MeasureText(RSTRING_CAST(L"Paint"),&size));
			REQUIRE(size.cx > 0 && size.cy > 0);
			Check(canvas->Clear(RGB(20,40,60)));
			Check(canvas->SetPixel(2,3,RGB(200,100,50)));
			REQUIRE(canvas->Invalidate() == S_FALSE);
			REQUIRE(CanvasBool(control,L"AutoRedraw",FALSE) == QuadooForm::EInvalidState);
			HDC nested = CreateCompatibleDC(NULL);
			CheckIf(!nested,HrEnsureLastError());
			SendMessageW(window,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(nested),PRF_CLIENT);
			DeleteDC(nested);
			if(resizeInPaint)
			{
				resizeInPaint = FALSE;
				Check(SetInt(control,L"Width",130));
			}
			if(destroyInPaint)
			{
				destroyInPaint = FALSE;
				Check(sender->Destroy());
			}
		}
		else if(name == RSTRING_CAST(L"OnResize"))
		{
			INT width;
			INT height;
			Check(args->GetInt(0,&width));
			Check(args->GetInt(1,&height));
			Check(canvas->GetPixelSize(&size));
			REQUIRE(width == size.cx && height == size.cy);
			resizes++;
		}
		else
		{
			clicks++;
		}
		hr = S_OK;
	Cleanup:
		return hr;
	}
};
static HRESULT PrintCanvas(HWND window, INT width, INT height, CMemoryStream& output)
{
	HRESULT hr;
	HDC dc = NULL;
	HBITMAP bitmap = NULL;
	HGDIOBJ old = NULL;
	BYTE* pixels;
	ULONG written;
	BITMAPINFO info = {0};
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = width;
	info.bmiHeader.biHeight = -height;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	dc = CreateCompatibleDC(NULL);
	CheckIf(!dc,HrEnsureLastError());
	bitmap = CreateDIBSection(dc,&info,DIB_RGB_COLORS,reinterpret_cast<PVOID*>(&pixels),NULL,0);
	CheckIf(!bitmap,HrEnsureLastError());
	old = SelectObject(dc,bitmap);
	CheckIf(!old || old == HGDI_ERROR,E_FAIL);
	ZeroMemory(pixels,width * height * 4);
	SendMessageW(window,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);
	CheckIf(!GdiFlush(),E_FAIL);
	Check(output.Write(pixels,width * height * 4,&written));
Cleanup:
	if(old && old != HGDI_ERROR)
	{
		SelectObject(dc,old);
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
struct CanvasThreadCall
{
	IQuadooCanvas* canvas;
	HRESULT result;
};
static DWORD WINAPI CanvasWrongThread(PVOID value)
{
	CanvasThreadCall* call = static_cast<CanvasThreadCall*>(value);
	SIZE size;
	call->result = call->canvas->MeasureText(RSTRING_CAST(L"Thread"),&size);
	return 0;
}
static HRESULT CanvasNativeTests()
{
	HRESULT hr;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONObject> saved;
	TStackRef<IJSONObject> again;
	TStackRef<IQuadooControl> form;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooControl> copy;
	TStackRef<IQuadooControlContainer> container;
	TStackRef<IQuadooWinControl> window;
	TStackRef<IQuadooCanvas> canvas;
	TStackRef<IQuadooFormSerializer> serializer;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooControlFactory> factory;
	TStackRef<IQuadooFactoryLifetime> lifetime;
	TStackRef<CCanvasTestImages> images;
	TStackRef<CCanvasTestImages> masks;
	TStackRef<CCanvasTestBinder> binder;
	TStackRef<ISequentialStream> png;
	HWND handle = NULL;
	HANDLE thread = NULL;
	SIZE size;
	SIZE original;
	COLORREF color;
	ULONG count;
	UINT baseline;
	UINT live;
	RECT bounds = {10,10,20,20};
	Check(QuadooGetControlRegistry(&registry));
	Check(registry->FindFactory(RSTRING_CAST(L"Canvas"),&factory));
	Check(factory->QueryInterface(&lifetime));
	Check(lifetime->GetLiveCount(&baseline));
	Check(Context(96,NULL,&context,registry));
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"CanvasForm\",\"children\":[{\"type\":\"Canvas\",\"name\":\"Drawing\",\"properties\":{\"Width\":120,\"Height\":80}}]}",&definition));
	Check(QuadooCreateControl(context,NULL,definition,&form));
	Check(form->QueryInterface(&container));
	Check(container->FindControl(RSTRING_CAST(L"Drawing"),&control));
	Check(control->QueryInterface(&canvas));
	Check(control->QueryInterface(&window));
	Check(window->GetHandle(&handle));
	{
		const IID* paths[] = {&__uuidof(IQuadooCanvas),&__uuidof(IQuadooWinControl),&__uuidof(IQuadooComponent)};
		Check(CheckCOMIdentity(control,paths,ARRAYSIZE(paths)));
		TStackRef<IQuadooControlContainer> unsupported;
		REQUIRE(control->QueryInterface(&unsupported) == E_NOINTERFACE && !unsupported);
	}
	Check(canvas->GetPixelSize(&size));
	REQUIRE(size.cx == 120 && size.cy == 80);
	Check(canvas->Clear(RGB(255,255,255)));
	Check(canvas->SetPixel(2,3,RGB(12,34,56)));
	Check(canvas->GetPixel(2,3,&color));
	REQUIRE(color == RGB(12,34,56));
	REQUIRE(canvas->SetPixel(-1,0,0) == HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
	REQUIRE(canvas->GetPixel(120,0,&color) == HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
	REQUIRE(canvas->Clear(0x80000005) == E_INVALIDARG);
	Check(canvas->ResolveColor(0x80000000 | COLOR_WINDOWTEXT,&color));
	REQUIRE(color == GetSysColor(COLOR_WINDOWTEXT));
	REQUIRE(canvas->ResolveColor(0x80000019,&color) == E_INVALIDARG);
	Check(canvas->DrawLine(4,5,9,5,RGB(255,0,0)));
	Check(canvas->GetPixel(8,5,&color));
	REQUIRE(color == RGB(255,0,0));
	Check(canvas->GetPixel(9,5,&color));
	REQUIRE(color == RGB(255,255,255));
	Check(canvas->DrawRectangle(&bounds,RGB(0,0,0),RGB(0,255,0),3));
	Check(canvas->GetPixel(15,15,&color));
	REQUIRE(color == RGB(0,255,0));
	bounds.left = 25;
	bounds.right = 40;
	Check(canvas->DrawEllipse(&bounds,RGB(0,0,0),RGB(0,0,255),3));
	Check(canvas->GetPixel(32,15,&color));
	REQUIRE(color == RGB(0,0,255));
	REQUIRE(canvas->DrawRectangle(&bounds,0,0,8) == E_INVALIDARG);
	Check(canvas->SetFont(RSTRING_CAST(L"Arial"),10,FALSE,FALSE));
	Check(canvas->MeasureText(RSTRING_CAST(L"Unicode \x03a9 & text"),&original));
	REQUIRE(original.cx > 0 && original.cy > 0);
	Check(canvas->DrawTextString(RSTRING_CAST(L"Unicode \x03a9 & text"),1,25,RGB(0,0,0)));
	Check(canvas->MeasureText(RSTRING_CAST(L"Unicode \x03a9 & text\r\n"),&size));
	REQUIRE(size.cx == original.cx && size.cy == original.cy * 2);
	REQUIRE(canvas->MeasureText(RSTRING_CAST(L""),NULL) == E_POINTER);
	images.Attach(__new CCanvasTestImages);
	masks.Attach(__new CCanvasTestImages);
	CheckAlloc(images);
	CheckAlloc(masks);
	Check(images->Initialize(FALSE));
	Check(masks->Initialize(TRUE));
	Check(canvas->DrawImageList(images,0,3,60));
	Check(canvas->GetPixel(3,60,&color));
	REQUIRE(color == RGB(255,0,0));
	Check(canvas->GetPixel(4,60,&color));
	REQUIRE(color == RGB(255,255,255));
	Check(canvas->GetPixel(5,60,&color));
	REQUIRE(GetGValue(color) == 255 && GetRValue(color) >= 126 && GetRValue(color) <= 128);
	Check(canvas->DrawImageList(masks,0,8,60));
	Check(canvas->GetPixel(9,60,&color));
	REQUIRE(color == RGB(255,255,255));
	REQUIRE(canvas->DrawImageList(images,1,0,0) == HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
	images->reenterDuringAcquire = canvas;
	Check(canvas->DrawImageList(images,0,0,70));
	REQUIRE(images->nestedResult == QuadooForm::EInvalidState && images->observers == 0);
	Check(SetInt(control,L"Width",140));
	Check(canvas->GetPixel(2,3,&color));
	REQUIRE(color == RGB(12,34,56));
	Check(canvas->GetPixel(139,79,&color));
	REQUIRE(color == GetSysColor(COLOR_WINDOW));
	Check(CanvasBool(control,L"ClearOnResize",TRUE));
	Check(SetInt(control,L"Width",150));
	Check(canvas->GetPixel(2,3,&color));
	REQUIRE(color == GetSysColor(COLOR_WINDOW));
	Check(CanvasBool(control,L"ClearOnResize",FALSE));
	Check(canvas->SetPixel(2,3,RGB(12,34,56)));
	{
		TStackRef<IQuadooLayoutControl> layout;
		QuadooForm::Rect logical;
		Check(control->QueryInterface(&layout));
		Check(control->GetBounds(&logical));
		Check(layout->Arrange(&logical,144));
		Check(canvas->GetPixelSize(&size));
		REQUIRE(size.cx == 225 && size.cy == 120);
		Check(canvas->MeasureText(RSTRING_CAST(L"Unicode \x03a9 & text"),&size));
		REQUIRE(size.cx > original.cx && size.cy > original.cy);
		Check(canvas->DrawImageList(images,0,0,0));
		REQUIRE(images->requestedDPI == 144);
		Check(layout->Arrange(&logical,96));
	}
	Check(CanvasBool(control,L"AutoRedraw",FALSE));
	REQUIRE(canvas->Clear(0) == QuadooForm::EInvalidState);
	REQUIRE(canvas->ExportPng(&png,&count) == QuadooForm::EInvalidState && !png && !count);
	Check(canvas->MeasureText(RSTRING_CAST(L"Still measurable"),&size));
	REQUIRE(size.cx > 0);
	Check(CanvasBool(control,L"AutoRedraw",TRUE));
	Check(canvas->SetPixel(2,3,RGB(12,34,56)));
	Check(SetInt(control,L"Width",0));
	Check(canvas->MeasureText(RSTRING_CAST(L"Zero size"),&size));
	REQUIRE(size.cx > 0 && canvas->ExportPng(&png,&count) == QuadooForm::EInvalidState);
	Check(SetInt(control,L"Width",150));
	Check(canvas->GetPixel(2,3,&color));
	REQUIRE(color == RGB(12,34,56));
	REQUIRE(FAILED(SetInt(control,L"Width",5000)));
	REQUIRE(FAILED(canvas->ExportPng(&png,&count)) && !png && !count);
	Check(canvas->MeasureText(RSTRING_CAST(L"Allocation failure"),&size));
	REQUIRE(size.cx > 0);
	Check(SetInt(control,L"Width",150));
	Check(canvas->GetPixel(2,3,&color));
	REQUIRE(color == RGB(12,34,56));
	{
		TStackRef<IQuadooControl> frame;
		TStackRef<IJSONObject> frameDefinition;
		TStackRef<IQuadooControlContainer> frameContainer;
		TStackRef<IQuadooControl> child;
		TStackRef<IQuadooCanvas> childCanvas;
		Check(Parse(L"{\"type\":\"Frame\",\"name\":\"DrawingFrame\",\"children\":[{\"type\":\"Canvas\",\"name\":\"Inside\"}]}",&frameDefinition));
		Check(QuadooCreateControl(context,container,frameDefinition,&frame));
		Check(frame->QueryInterface(&frameContainer));
		Check(frameContainer->FindControl(RSTRING_CAST(L"Inside"),&child));
		Check(child->QueryInterface(&childCanvas));
		Check(childCanvas->Clear(RGB(1,2,3)));
		Check(childCanvas->GetPixel(0,0,&color));
		REQUIRE(color == RGB(1,2,3));
		Check(frame->Destroy());
	}
	{
		TStackRef<IQuadooComponentHost> owner;
		TStackRef<IQuadooComponentCollection> components;
		TStackRef<IQuadooComponent> component;
		TStackRef<IQuadooImageList> source;
		TStackRef<IJSONObject> componentDefinition;
		Check(form->QueryInterface(&owner));
		Check(owner->GetComponents(&components));
		Check(Parse(L"{\"type\":\"ImageList\",\"name\":\"CanvasImages\",\"properties\":{\"Width\":3,\"Height\":1,\"ColorDepth\":\"Color32\",\"Masked\":true,\"Images\":[{\"format\":\"png\",\"encoding\":\"base64\",\"data\":\"iVBORw0KGgoAAAANSUhEUgAAAAMAAAABCAYAAAAb4BS0AAAAE0lEQVR4nGP4z8DwnwEE/jM0AAAYdQN+O93EpAAAAABJRU5ErkJggg==\",\"sizing\":\"Contain\"}]}}",&componentDefinition));
		Check(QuadooCreateComponent(context,components,componentDefinition,&component));
		Check(component->QueryInterface(&source));
		Check(canvas->Clear(RGB(255,255,255)));
		Check(canvas->DrawImageList(source,0,20,70));
		Check(canvas->GetPixel(22,70,&color));
		REQUIRE(GetGValue(color) == 255 && GetRValue(color) >= 126 && GetRValue(color) <= 128);
		Check(component->Destroy());
		Check(canvas->GetPixel(22,70,&color));
		REQUIRE(GetGValue(color) == 255);
		Check(canvas->SetPixel(2,3,RGB(12,34,56)));
	}
	{
		Check(canvas->SetFont(RSTRING_CAST(L"Arial"),11,FALSE,FALSE));
		Check(canvas->SetFont(RSTRING_CAST(L"Arial"),10,FALSE,FALSE));
		DWORD objects = GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
		for(UINT n = 0; n < 24; n++)
		{
			Check(canvas->SetFont(RSTRING_CAST(L"Arial"),10 + (n % 2),FALSE,FALSE));
			Check(canvas->MeasureText(RSTRING_CAST(L"Repeated font replacement"),&size));
			Check(canvas->DrawTextString(RSTRING_CAST(L"Font"),30,50,0));
		}
		Check(canvas->SetFont(RSTRING_CAST(L"Arial"),10,FALSE,FALSE));
		REQUIRE(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS) <= objects);
	}
	{
		CanvasThreadCall call = {canvas,S_OK};
		thread = CreateThread(NULL,0,CanvasWrongThread,&call,0,NULL);
		CheckIf(!thread,HrEnsureLastError());
		REQUIRE(WaitForSingleObject(thread,10000) == WAIT_OBJECT_0 && call.result == QuadooForm::EWrongThread);
		CloseHandle(thread);
		thread = NULL;
	}
	Check(QuadooGetFormSerializer(&serializer));
	Check(serializer->Serialize(form,&saved));
	Check(QuadooCreateControl(context,NULL,saved,&copy));
	Check(serializer->Serialize(copy,&again));
	REQUIRE(!JSONCompareObjects(saved,again));
	Check(copy->Destroy());
	copy.Release();
	images->destroyed = TRUE;
	REQUIRE(canvas->DrawImageList(images,0,0,0) == QuadooForm::EDestroyed);
	images->destroyed = FALSE;
	Check(canvas->ExportPng(&png,&count));
	REQUIRE(count > 24);
	images->destroyDuringAcquire = control;
	REQUIRE(canvas->DrawImageList(images,0,0,0) == QuadooForm::EDestroyed);
	REQUIRE(!IsWindow(handle) && canvas->MeasureText(RSTRING_CAST(L"Destroyed"),&size) == QuadooForm::EDestroyed);
	{
		BYTE signature[8];
		ULONG read;
		Check(png->Read(signature,sizeof(signature),&read));
		REQUIRE(read == 8 && !memcmp(signature,"\x89PNG\r\n\x1a\n",8));
	}
	canvas.Release();
	window.Release();
	control.Release();
	images.Release();
	masks.Release();
	Check(lifetime->GetLiveCount(&live));
	REQUIRE(live == baseline);
	Check(form->Destroy());
	form.Release();
	container.Release();
	definition.Release();
	binder.Attach(__new CCanvasTestBinder);
	CheckAlloc(binder);
	context.Release();
	Check(Context(96,binder,&context));
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"PaintForm\",\"children\":[{\"type\":\"Canvas\",\"name\":\"Drawing\",\"properties\":{\"Width\":120,\"Height\":80,\"AutoRedraw\":false},\"events\":{\"OnPaint\":\"Paint\",\"OnResize\":\"Resize\"}}]}",&definition));
	Check(QuadooCreateControl(context,NULL,definition,&form));
	Check(form->QueryInterface(&container));
	Check(container->FindControl(RSTRING_CAST(L"Drawing"),&control));
	Check(control->QueryInterface(&window));
	Check(window->GetHandle(&handle));
	for(UINT mode = 0; mode < 2; mode++)
	{
		CMemoryStream pixels;
		Check(CanvasBool(control,L"DoubleBuffered",mode != 0));
		ValidateRect(handle,NULL);
		Check(PrintCanvas(handle,120,80,pixels));
		REQUIRE(!GetUpdateRect(handle,NULL,FALSE));
		const BYTE* pixel = pixels.GetReadPtr() + (3 * 120 + 2) * 4;
		REQUIRE(pixel[0] == 50 && pixel[1] == 100 && pixel[2] == 200);
		REQUIRE(binder->paints == mode + 1);
	}
	binder->resizeInPaint = TRUE;
	{
		CMemoryStream pixels;
		Check(PrintCanvas(handle,120,80,pixels));
		REQUIRE(binder->paints == 3);
	}
	{
		MSG message;
		while(PeekMessageW(&message,NULL,0,0,PM_REMOVE))
		{
			if(message.message != WM_QUIT)
			{
				TranslateMessage(&message);
				DispatchMessageW(&message);
			}
		}
		REQUIRE(binder->resizes > 0);
	}
	binder->destroyInPaint = TRUE;
	{
		CMemoryStream pixels;
		Check(PrintCanvas(handle,130,80,pixels));
		REQUIRE(!IsWindow(handle));
	}
	Check(form->Destroy());
	form.Release();
	container.Release();
	control.Release();
	window.Release();
	context.Release();
	Check(Context(96,binder,&context,NULL,QuadooForm::Preview));
	Check(QuadooCreateControl(context,NULL,definition,&form));
	Check(form->QueryInterface(&container));
	Check(container->FindControl(RSTRING_CAST(L"Drawing"),&control));
	Check(control->QueryInterface(&window));
	Check(window->GetHandle(&handle));
	{
		UINT before = binder->paints;
		CMemoryStream pixels;
		Check(PrintCanvas(handle,120,80,pixels));
		REQUIRE(binder->paints == before);
	}
	Check(form->Destroy());
	hr = S_OK;
Cleanup:
	if(thread)
	{
		CloseHandle(thread);
	}
	if(copy)
	{
		copy->Destroy();
	}
	if(form)
	{
		form->Destroy();
	}
	return hr;
}
