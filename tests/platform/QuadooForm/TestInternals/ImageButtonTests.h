#include "..\..\..\..\platform\QuadooForm\QuadooForm\ImageButton.h"

static BOOL ImageButtonExpected(BOOL condition, UINT line)
{
	g_checks++;
	if(!condition)
	{
		CRString text;
		if(SUCCEEDED(RStrFormatW(&text,L"ImageButton assertion failed at line %u\r\n",line)))
		{
			Log(text.c_wstr());
		}
	}
	return condition;
}
#define IB_REQUIRE(value) CheckIf(!ImageButtonExpected(!!(value),__LINE__),E_FAIL)

class CPresentationProbe : public CBaseUnknown, public IQuadooButtonPresentation
{
public:
	BOOL selected;
	UINT calls;
	HRESULT answer;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooButtonPresentation)
	END_UNK_MAP
	CPresentationProbe() : selected(FALSE), calls(0), answer(S_OK)
	{
	}
	STDMETHOD(SetDefaultAppearance)(BOOL value)
	{
		calls++;
		selected = value;
		return answer;
	}
};

// Only published interfaces are implemented here. Handles are opaque test
// tokens: the forwarding helper must not call Win32 or inspect private classes.
class CDrawRoutingProbe : public CBaseUnknown, public IQuadooControlContainer, public IQuadooMessageHandler
{
public:
	IQuadooControl* children[2];
	UINT count;
	UINT calls;
	HWND handle;
	HRESULT handleResult;
	HRESULT messageResult;
	UINT lastMessage;
	WPARAM lastID;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooControlContainer)
		UNK_INTERFACE(IQuadooWinControl)
		UNK_INTERFACE(IQuadooControl)
		UNK_INTERFACE(IQuadooComponent)
		UNK_INTERFACE(IQuadooMessageHandler)
	END_UNK_MAP
	CDrawRoutingProbe() : count(0), calls(0), handle(NULL), handleResult(S_OK), messageResult(S_OK), lastMessage(0), lastID(0)
	{
		ZeroMemory(children,sizeof(children));
	}
	STDMETHOD(GetHandle)(HWND* value)
	{
		*value = handle;
		return handleResult;
	}
	STDMETHOD(GetChildCount)(UINT* value)
	{
		*value = count;
		return S_OK;
	}
	STDMETHOD(GetChild)(UINT index, IQuadooControl** value)
	{
		HRESULT hr = S_OK;
		CheckIf(index >= count || index >= ARRAYSIZE(children),E_INVALIDARG);
		*value = children[index];
		(*value)->AddRef();
	Cleanup:
		return hr;
	}
	STDMETHOD(HandleMessage)(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result)
	{
		HRESULT hr;
		const DRAWITEMSTRUCT* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lp);
		calls++;
		lastMessage = message;
		lastID = wp;
		CheckIf(window != handle || !item || item->hwndItem != handle,E_INVALIDARG);
		*handled = TRUE;
		*result = 73;
		hr = messageResult;
	Cleanup:
		return hr;
	}
	STDMETHOD(GetID)(GUID*)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(GetName)(RSTRING*)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(SetName)(RSTRING)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(GetTypeName)(RSTRING*)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(GetProperties)(IQuadooProperties**)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(GetState)(QuadooForm::ComponentState*)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(Destroy)()
	{
		return E_NOTIMPL;
	}
	STDMETHOD(GetParent)(IQuadooControlContainer**)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(GetBounds)(QuadooForm::Rect*)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(SetBounds)(const QuadooForm::Rect*)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(GetEffectiveBounds)(QuadooForm::Rect*)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(GetVisible)(BOOL*)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(SetVisible)(BOOL)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(GetEnabled)(BOOL*)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(SetEnabled)(BOOL)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(SetFocus)()
	{
		return E_NOTIMPL;
	}
	STDMETHOD(GetTabOrder)(INT*)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(SetTabOrder)(INT)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(FindControl)(RSTRING, IQuadooControl**)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(RegisterControl)(IQuadooControl*)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(UnregisterControl)(IQuadooControl*)
	{
		return E_NOTIMPL;
	}
	STDMETHOD(BeginUpdate)()
	{
		return E_NOTIMPL;
	}
	STDMETHOD(EndUpdate)()
	{
		return E_NOTIMPL;
	}
	STDMETHOD(UpdateLayout)()
	{
		return E_NOTIMPL;
	}
};

static HRESULT ImageButtonRoutingTests()
{
	HRESULT hr;
	TStackRef<CDrawRoutingProbe> parent;
	TStackRef<CDrawRoutingProbe> destroyed;
	TStackRef<CDrawRoutingProbe> child;
	TStackRef<IUnknown> expected;
	TStackRef<IUnknown> identity;
	TStackRef<IQuadooMessageHandler> handler;
	DRAWITEMSTRUCT item = {0};
	BOOL handled;
	LRESULT result;
	parent.Attach(__new CDrawRoutingProbe);
	destroyed.Attach(__new CDrawRoutingProbe);
	child.Attach(__new CDrawRoutingProbe);
	CheckAlloc(parent);
	CheckAlloc(destroyed);
	CheckAlloc(child);
	parent->children[0] = destroyed;
	parent->children[1] = child;
	parent->count = 2;
	destroyed->handleResult = QuadooForm::EDestroyed;
	child->handle = reinterpret_cast<HWND>(1);
	item.CtlType = ODT_BUTTON;
	item.hwndItem = child->handle;
	Check(QF::ForwardDrawItem(parent,&item,41,&handled,&result));
	IB_REQUIRE(handled && result == 73 && child->calls == 1 && !destroyed->calls);
	IB_REQUIRE(child->lastMessage == WM_DRAWITEM && child->lastID == 41);
	Check(child->QueryInterface(IID_IUnknown,reinterpret_cast<PVOID*>(&expected)));
	Check(child->QueryInterface(__uuidof(IQuadooMessageHandler),reinterpret_cast<PVOID*>(&handler)));
	Check(handler->QueryInterface(IID_IUnknown,reinterpret_cast<PVOID*>(&identity)));
	IB_REQUIRE(expected == static_cast<IUnknown*>(identity));
	item.CtlType = ODT_MENU;
	Check(QF::ForwardDrawItem(parent,&item,41,&handled,&result));
	IB_REQUIRE(!handled && !result && child->calls == 1);
	item.CtlType = ODT_BUTTON;
	item.hwndItem = reinterpret_cast<HWND>(2);
	Check(QF::ForwardDrawItem(parent,&item,41,&handled,&result));
	IB_REQUIRE(!handled && !result && child->calls == 1);
	item.hwndItem = child->handle;
	child->messageResult = E_ABORT;
	IB_REQUIRE(QF::ForwardDrawItem(parent,&item,41,&handled,&result) == E_ABORT);
	destroyed->handleResult = E_UNEXPECTED;
	IB_REQUIRE(QF::ForwardDrawItem(parent,&item,41,&handled,&result) == E_UNEXPECTED);
	hr = S_OK;
Cleanup:
	return hr;
}

// Uses the actual ImageButton metadata, properties and source resources, but
// deliberately replaces native creation. No HWND, apartment or timer is created.
class CHeadlessImageButton : public QF::CImageButton
{
protected:
	virtual HRESULT CreateNative()
	{
		return LoadPictures();
	}
	virtual HRESULT ApplyNative(PCWSTR name)
	{
		HRESULT hr = S_OK;
		if(!lstrcmpW(name,L"Picture") || !lstrcmpW(name,L"DisabledPicture"))
		{
			Check(UpdatePicture(name));
		}
	Cleanup:
		return hr;
	}
public:
	CHeadlessImageButton(QF::CFactory* factory, IQuadooConstructionContext* context) : CImageButton(factory,context,NULL)
	{
	}
	QFI::CImageResource* Picture() const
	{
		return m_picture;
	}
};

static HRESULT ImageButtonMetadataTests()
{
	HRESULT hr;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooControlFactory> registered;
	TStackRef<QF::CFactory> factory;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<CHeadlessImageButton> button;
	TStackRef<CHeadlessImageButton> copy;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONObject> saved;
	TStackRef<IJSONObject> again;
	TStackRef<IQuadooFormSerializer> serializer;
	TStackRef<IJSONValue> picture;
	TStackRef<IJSONValue> value;
	TStackRef<IQuadooPaletteIcon> artwork;
	TStackRef<IQuadooButtonPresentation> presentation;
	TStackRef<IUnknown> expected;
	TStackRef<IUnknown> actual;
	HICON icon = NULL;
	ICONINFO info = {0};
	QuadooForm::ContextOptions options = {0};
	Check(QuadooGetControlRegistry(&registry));
	Check(registry->FindFactory(RSTRING_CAST(L"ImageButton"),&registered));
	Check(QF::ValidateFactory(registered));
	Check(registered->QueryInterface(__uuidof(IQuadooPaletteIcon),reinterpret_cast<PVOID*>(&artwork)));
	Check(artwork->GetPaletteIcon(20,20,&icon));
	IB_REQUIRE(icon && GetIconInfo(icon,&info));
	BITMAP bitmap;
	IB_REQUIRE(GetObject(info.hbmColor,sizeof(bitmap),&bitmap) == sizeof(bitmap) && bitmap.bmWidth == 20 && bitmap.bmHeight == 20);
	options.cbSize = sizeof(options);
	options.DPI = 96;
	options.Mode = QuadooForm::Design;
	options.pRegistry = registry;
	Check(QuadooCreateConstructionContext(&options,&context));
	factory.Attach(__new QF::TFactory<QF::CImageButton>);
	CheckAlloc(factory);
	Check(factory->Initialize());
	button.Attach(__new CHeadlessImageButton(factory,context));
	CheckAlloc(button);
	Check(NativeDefinition(L"{\"type\":\"ImageButton\",\"name\":\"Save\",\"properties\":{\"Caption\":\"&Save\",\"ImagePosition\":\"Top\",\"ImageWidth\":20,\"ImageHeight\":24},\"events\":{\"OnClick\":\"Save_Click\"}}",&definition));
	Check(button->Construct(definition));
	Check(button->BindEvents(context));
	Check(button->InitializeControl());
	Check(button->ActivateControl());
	Check(QF::ValidateControlMetadata(button->Control(),registered));
	IB_REQUIRE(ImageInteger(button->Control(),L"ImagePosition") == 1 && ImageInteger(button->Control(),L"ImageWidth") == 20);
	IB_REQUIRE(ImageInteger(button->Control(),L"Spacing") == 4 && ImageInteger(button->Control(),L"Interpolation") == 3);
	Check(button->QueryInterface(__uuidof(IQuadooButtonPresentation),reinterpret_cast<PVOID*>(&presentation)));
	Check(button->Control()->QueryInterface(__uuidof(IUnknown),reinterpret_cast<PVOID*>(&expected)));
	Check(presentation->QueryInterface(__uuidof(IUnknown),reinterpret_cast<PVOID*>(&actual)));
	IB_REQUIRE(expected == static_cast<IUnknown*>(actual));
	actual.Release();
	expected.Release();
	Check(presentation->SetDefaultAppearance(TRUE));
	Check(presentation->SetDefaultAppearance(FALSE));
	Check(ImageFixtureValue(L"png",ImageRGBA,sizeof(ImageRGBA),&picture));
	Check(ImageSet(button->Control(),L"Picture",picture));
	Check(ImageSet(button->Control(),L"DisabledPicture",picture));
	Check(ImageRead(button->Control(),L"Picture",&value));
	IB_REQUIRE(!JSONCompare(picture,value));
	value.Release();
	IB_REQUIRE(ImageNumber(button->Control(),L"ImageWidth",0) == E_INVALIDARG);
	IB_REQUIRE(ImageNumber(button->Control(),L"ImageHeight",257) == E_INVALIDARG);
	IB_REQUIRE(ImageNumber(button->Control(),L"ImagePosition",2) == E_INVALIDARG);
	IB_REQUIRE(ImageNumber(button->Control(),L"Spacing",-1) == E_INVALIDARG);
	Check(QuadooGetFormSerializer(&serializer));
	Check(serializer->Serialize(button->Control(),&saved));
	{
		QuadooForm::Rect bounds;
		Check(button->GetBounds(&bounds));
		for(UINT dpi = 96; dpi <= 192; dpi += 48)
		{
			Check(button->Arrange(&bounds,dpi));
			Check(serializer->Serialize(button->Control(),&again));
			IB_REQUIRE(!JSONCompareObjects(saved,again));
			again.Release();
		}
	}
	{
		TStackRef<IJSONValue> other;
		Check(ImageFixtureValue(L"png",ImageStripes,sizeof(ImageStripes),&other));
		BOOL succeeded = FALSE;
		UINT failures = 0;
		for(LONG allocation = 0; allocation < 128; allocation++)
		{
			QFI::CImageResource* original = button->Picture();
			QFI::FailAllocationAfter(allocation);
			hr = ImageSet(button->Control(),L"Picture",other);
			QFI::FailAllocationAfter(-1);
			if(SUCCEEDED(hr))
			{
				succeeded = TRUE;
				break;
			}
			failures++;
			IB_REQUIRE(hr == E_OUTOFMEMORY && button->Picture() == original);
			Check(serializer->Serialize(button->Control(),&again));
			IB_REQUIRE(!JSONCompareObjects(saved,again));
			again.Release();
		}
		IB_REQUIRE(succeeded && failures >= 4);
		Check(ImageSet(button->Control(),L"Picture",picture));
	}
	copy.Attach(__new CHeadlessImageButton(factory,context));
	CheckAlloc(copy);
	Check(copy->Construct(saved));
	Check(serializer->Serialize(copy->Control(),&again));
	IB_REQUIRE(!JSONCompareObjects(saved,again));
	Check(ImageRead(copy->Control(),L"Picture",&value));
	IB_REQUIRE(!JSONCompare(picture,value));
	value.Release();
	Check(ImageSet(copy->Control(),L"DisabledPicture",NULL));
	Check(ImageRead(copy->Control(),L"DisabledPicture",&value));
	IB_REQUIRE(!value);
	{
		TStackRef<CPresentationProbe> independent;
		independent.Attach(__new CPresentationProbe);
		CheckAlloc(independent);
		Check(QF::SetDefaultButtonAppearance(static_cast<IQuadooButtonPresentation*>(independent),TRUE));
		IB_REQUIRE(independent->selected && independent->calls == 1);
		Check(QF::SetDefaultButtonAppearance(static_cast<IQuadooButtonPresentation*>(independent),FALSE));
		IB_REQUIRE(!independent->selected && independent->calls == 2);
		independent->answer = E_ABORT;
		IB_REQUIRE(QF::SetDefaultButtonAppearance(static_cast<IQuadooButtonPresentation*>(independent),TRUE) == E_ABORT);
		Check(QF::SetDefaultButtonAppearance(serializer,TRUE));
	}
	Check(button->Destroy());
	Check(copy->Destroy());
	presentation.Release();
	button.Release();
	copy.Release();
	UINT live;
	Check(factory->GetLiveCount(&live));
	IB_REQUIRE(!live);
	hr = S_OK;
Cleanup:
	QFI::FailAllocationAfter(-1);
	if(button)
	{
		button->Destroy();
	}
	if(copy)
	{
		copy->Destroy();
	}
	if(info.hbmColor)
	{
		DeleteObject(info.hbmColor);
	}
	if(info.hbmMask)
	{
		DeleteObject(info.hbmMask);
	}
	if(icon)
	{
		DestroyIcon(icon);
	}
	return hr;
}

static HRESULT ImageButtonRasterTests()
{
	HRESULT hr;
	TStackRef<QFI::CImageResource> picture;
	TStackRef<QFI::CImageResource> icon;
	TStackRef<QFI::CImageResource> legacy;
	TStackRef<IJSONValue> before;
	TStackRef<IJSONValue> after;
	QFI::Raster pixels;
	CImageTestCanvas canvas;
	BYTE samples[256 * 4];
	RECT area = {0,0,4,2};
	for(UINT alpha = 0; alpha < 256; alpha++)
	{
		samples[alpha * 4] = static_cast<BYTE>(alpha);
		samples[alpha * 4 + 1] = static_cast<BYTE>(alpha / 2);
		samples[alpha * 4 + 2] = 0;
		samples[alpha * 4 + 3] = static_cast<BYTE>(alpha);
	}
	QFI::GrayscalePixels(samples,256);
	for(UINT alpha = 0; alpha < 256; alpha++)
	{
		const BYTE* pixel = samples + alpha * 4;
		IB_REQUIRE(pixel[3] == alpha && pixel[0] == pixel[1] && pixel[1] == pixel[2] && pixel[0] <= alpha);
	}
	{
		BYTE background[] = {207,133,11,255,207,133,11,255};
		const BYTE normal[] = {0,0,0,0,0,0,0,0};
		const BYTE effect[] = {0,0,0,0,128,128,128,128};
		QFI::CompositeGrayscaleXor(background,normal,effect,2);
		IB_REQUIRE(background[0] == 207 && background[1] == 133 && background[2] == 11);
		// Partial XOR coverage retains the colored background contribution.
		// Grayscaling the whole composed pixel would incorrectly make R=G=B.
		IB_REQUIRE(background[4] != background[5] && background[5] != background[6]);
	}
	Check(canvas.Create(64,64));
	Check(QFI::CImageResource::FromBytes(L"png",ImageRGBA,sizeof(ImageRGBA),&picture));
	Check(QFI::CImageResource::FromBytes(L"ico",ImageMultiICO,sizeof(ImageMultiICO),&icon));
	Check(QFI::CImageResource::FromBytes(L"ico",ImageLegacyICO,sizeof(ImageLegacyICO),&legacy));
	Check(picture->GetValue(&before));
	// Fail both normal-raster preparation and disabled-cache allocation. A
	// later paint must recover using the unchanged source and release all DIBs.
	DWORD cacheGDI = GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
	for(LONG allocation = 0; allocation < 2; allocation++)
	{
		picture->InvalidateCache();
		QFI::FailAllocationAfter(allocation);
		hr = picture->PaintGrayscale(canvas.DC(),&area,96,QFI::Nearest);
		QFI::FailAllocationAfter(-1);
		IB_REQUIRE(hr == E_OUTOFMEMORY);
		canvas.Fill(RGB(255,255,255));
		Check(picture->PaintGrayscale(canvas.DC(),&area,96,QFI::Nearest));
		IB_REQUIRE(canvas.Pixel(0,0) == RGB(77,77,77));
	}
	picture->InvalidateCache();
	GdiFlush();
	IB_REQUIRE(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS) == cacheGDI);
	canvas.Fill(RGB(255,255,255));
	Check(picture->PaintGrayscale(canvas.DC(),&area,96,QFI::Nearest));
	IB_REQUIRE(canvas.Pixel(0,0) == RGB(77,77,77));
	COLORREF translucent = canvas.Pixel(2,0);
	IB_REQUIRE(GetRValue(translucent) == GetGValue(translucent) && GetGValue(translucent) == GetBValue(translucent));
	Check(picture->RenderGlyphPixels(4,2,96,QFI::Nearest,pixels));
	IB_REQUIRE(pixels.pixels[2 * 4 + 3] == 128);
	QFI::GrayscalePixels(pixels.pixels,8);
	IB_REQUIRE(pixels.pixels[2 * 4 + 3] == 128 && pixels.pixels[2 * 4] == pixels.pixels[2 * 4 + 2]);
	// A warm disabled cache performs no further imaging allocations.
	QFI::FailAllocationAfter(0);
	Check(picture->PaintGrayscale(canvas.DC(),&area,96,QFI::Nearest));
	QFI::FailAllocationAfter(-1);
	Check(picture->GetValue(&after));
	IB_REQUIRE(!JSONCompare(before,after));
	for(UINT dpi = 96; dpi <= 192; dpi += 48)
	{
		UINT size = MulDiv(16,dpi,96);
		Check(icon->RenderGlyphPixels(size,size,dpi,QFI::HighQuality,pixels));
		IB_REQUIRE(pixels.width == size && pixels.height == size && icon->SelectedWidth() >= size);
		RECT bounds = {0,0,size,size};
		Check(icon->PaintGrayscale(canvas.DC(),&bounds,dpi,QFI::HighQuality));
	}
	for(UINT background = 0; background < 3; background++)
	{
		COLORREF color = background == 0 ? RGB(21,35,49) : background == 1 ? RGB(240,240,240) : RGB(11,133,207);
		RECT bounds = {0,0,2,2};
		canvas.Fill(color);
		Check(legacy->PaintGrayscale(canvas.DC(),&bounds,96,QFI::Nearest));
		IB_REQUIRE(canvas.Pixel(1,0) == color);
		IB_REQUIRE(canvas.Pixel(0,0) == RGB(77,77,77));
		COLORREF inverted = canvas.Pixel(0,1);
		IB_REQUIRE(GetRValue(inverted) == GetGValue(inverted) && GetGValue(inverted) == GetBValue(inverted));
	}
	Check(picture->Paint(canvas.DC(),&area,96,QFI::Contain,4,QFI::Nearest,255));
	IB_REQUIRE(canvas.Pixel(0,0) == RGB(255,0,0));
	hr = S_OK;
Cleanup:
	QFI::FailAllocationAfter(-1);
	return hr;
}

static HRESULT ImageButtonDrawingTests()
{
	HRESULT hr;
	QF::ImageButtonAppearance a = {0};
	TStackRef<QFI::CImageResource> picture;
	TStackRef<QFI::CImageResource> disabled;
	CImageTestCanvas canvas;
	RECT content = {0,0,100,50};
	RECT tiny = {0,0,1,1};
	RECT bounds = {4,4,104,60};
	RECT glyph;
	RECT text;
	SIZE caption = {30,12};
	Check(QF::ImageButtonLayout(&content,&caption,0,16,16,4,TRUE,&glyph,&text));
	IB_REQUIRE(glyph.right - glyph.left == 16 && text.left - glyph.right == 4 && text.right <= 100);
	Check(QF::ImageButtonLayout(&content,&caption,1,16,16,4,TRUE,&glyph,&text));
	IB_REQUIRE(text.top - glyph.bottom == 4 && glyph.left == 42 && text.left == 35);
	for(UINT position = 0; position < 2; position++)
	{
		Check(QF::ImageButtonLayout(&tiny,&caption,position,16,16,4,TRUE,&glyph,&text));
		IB_REQUIRE(glyph.left >= 0 && glyph.top >= 0 && glyph.right <= 1 && glyph.bottom <= 1 &&
			text.left >= 0 && text.top >= 0 && text.right <= 1 && text.bottom <= 1);
	}
	Check(canvas.Create(128,96));
	Check(QFI::CImageResource::FromBytes(L"png",ImageStripes,sizeof(ImageStripes),&picture));
	Check(QFI::CImageResource::FromBytes(L"png",ImageRGBA,sizeof(ImageRGBA),&disabled));
	a.dpi = 96;
	a.width = 16;
	a.height = 16;
	a.spacing = 4;
	a.filter = QFI::Nearest;
	a.enabled = TRUE;
	a.fore = RGB(0,0,0);
	a.back = GetSysColor(COLOR_BTNFACE);
	a.picture = picture;
	const COLORREF surround = RGB(17,33,49);
	canvas.Fill(surround);
	HGDIOBJ font = GetCurrentObject(canvas.DC(),OBJ_FONT);
	COLORREF color = GetTextColor(canvas.DC());
	Check(QF::PaintImageButton(canvas.DC(),&bounds,&a));
	IB_REQUIRE(canvas.Pixel(48,31) == RGB(255,0,0));
	IB_REQUIRE(canvas.Pixel(0,0) == surround && GetCurrentObject(canvas.DC(),OBJ_FONT) == font && GetTextColor(canvas.DC()) == color);
	a.enabled = FALSE;
	Check(QF::PaintImageButton(canvas.DC(),&bounds,&a));
	COLORREF gray = canvas.Pixel(48,31);
	IB_REQUIRE(GetRValue(gray) == GetGValue(gray) && GetGValue(gray) == GetBValue(gray));
	a.disabledPicture = disabled;
	Check(QF::PaintImageButton(canvas.DC(),&bounds,&a));
	IB_REQUIRE(canvas.Pixel(48,31) != gray);
	a.caption = L"&Save";
	a.length = StaticLength(L"&Save");
	for(UINT position = 0; position < 2; position++)
	{
		for(UINT dpi = 96; dpi <= 192; dpi += 48)
		{
			a.position = position;
			a.dpi = dpi;
			for(UINT state = 0; state < 6; state++)
			{
				a.enabled = state != 3;
				a.hot = state == 1;
				a.pressed = state == 2;
				a.focused = state == 4;
				a.isDefault = state == 5;
				Check(QF::PaintImageButton(canvas.DC(),&bounds,&a));
				IB_REQUIRE(canvas.Pixel(0,0) == surround);
			}
		}
	}
	IB_REQUIRE(QF::PaintImageButton(NULL,&bounds,&a) == E_POINTER);
	DWORD gdi = GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
	for(UINT n = 0; n < 30; n++)
	{
		a.enabled = (n & 1) != 0;
		Check(QF::PaintImageButton(canvas.DC(),&bounds,&a));
	}
	GdiFlush();
	IB_REQUIRE(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS) == gdi);
	hr = S_OK;
Cleanup:
	return hr;
}

static HRESULT ImageButtonHeadlessTests()
{
	HRESULT hr;
	Log(L"ImageButton independent public-interface draw routing...\r\n");
	Check(ImageButtonRoutingTests());
	Log(L"ImageButton metadata, properties, JSON, icon and presentation...\r\n");
	Check(ImageButtonMetadataTests());
	Log(L"ImageButton alpha, ICO selection, grayscale and XOR...\r\n");
	Check(ImageButtonRasterTests());
	Log(L"ImageButton non-windowed layout, GDI painting and cleanup...\r\n");
	Check(ImageButtonDrawingTests());
	Log(L"Existing non-windowed image renderer regression...\r\n");
	Check(ImageRenderingTests());
Cleanup:
	return hr;
}
