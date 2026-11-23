#include "..\TestImages\ImageTestSupport.h"

class CImageObserver : public CBaseUnknown, public IQuadooPropertyObserver
{
public:
	UINT pictures, diagnostics, bounds;
	CImageObserver() : pictures(0), diagnostics(0), bounds(0) {}
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooPropertyObserver)
	END_UNK_MAP
	STDMETHOD(OnPropertyChanged)(IQuadooComponent* control, UINT index)
	{
		HRESULT hr;
		TStackRef<IQuadooProperties> values;
		TStackRef<IQuadooPropertyDescriptors> descriptors;
		TStackRef<IQuadooPropertyDescriptor> descriptor;
		TStackRef<IJSONValue> value;
		CRString name;
		Check(control->GetProperties(&values));
		Check(values->GetDescriptors(&descriptors));
		Check(descriptors->GetAt(index,&descriptor));
		Check(descriptor->GetName(&name));
		Check(values->GetValueJSON(index,&value));
		if(name == RSTRING_CAST(L"Picture")) pictures++;
		else if(name == RSTRING_CAST(L"ImageWidth") || name == RSTRING_CAST(L"ImageHeight") || name == RSTRING_CAST(L"HasAlpha") || name == RSTRING_CAST(L"ImageFormat") || name == RSTRING_CAST(L"ImageSize")) diagnostics++;
Cleanup:
		return hr;
	}
	STDMETHOD(OnBoundsChanged)(IQuadooControl*, const QuadooForm::Rect*) { bounds++; return S_OK; }
};
static HRESULT ImageInteraction()
{
	HRESULT hr;
	TStackRef<CBinder> binder;
	TStackRef<IJSONObject> definition, serialized;
	TStackRef<IQuadooForm> form;
	TStackRef<IQuadooControl> image, below;
	TStackRef<IQuadooStaticControl> painted;
	TStackRef<IQuadooWinControl> windowed;
	TStackRef<IQuadooPropertyNotifications> notifications;
	TStackRef<CImageObserver> observer;
	TStackRef<IJSONValue> picture, copy, value;
	TStackRef<IQuadooFormSerializer> serializer;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooControlFactory> factory;
	TStackRef<IQuadooFactoryLifetime> lifetime;
	CImageTestCanvas canvas;
	HWND window, paintWindow;
	BOOL hit;
	RECT area = {1,1,5,3};
	binder.Attach(__new CBinder);
	observer.Attach(__new CImageObserver);
	CheckAlloc(binder);
	CheckAlloc(observer);
	Check(QuadooGetControlRegistry(&registry));
	Check(registry->FindFactory(RSTRING_CAST(L"Image"),&factory));
	Check(factory->QueryInterface(__uuidof(IQuadooFactoryLifetime),reinterpret_cast<PVOID*>(&lifetime)));
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"ImageForm\",\"properties\":{\"BackColor\":3351057},\"children\":[{\"type\":\"Image\",\"name\":\"below\",\"properties\":{\"Left\":10,\"Top\":12,\"Width\":40,\"Height\":20,\"Transparent\":false,\"BackColor\":16711680}},{\"type\":\"Image\",\"name\":\"image\",\"properties\":{\"Left\":10,\"Top\":12,\"Width\":40,\"Height\":20,\"SizeMode\":\"Stretch\",\"Interpolation\":\"Nearest\"},\"events\":{\"OnClick\":\"Click\"}}]}",&definition));
	Check(QuadooCreateForm(binder,NULL,definition,&form));
	Check(form->GetHandle(&window));
	{
		TStackRef<IQuadooChildWindowHost> host;
		Check(form->QueryInterface(__uuidof(IQuadooChildWindowHost),reinterpret_cast<PVOID*>(&host)));
		Check(host->GetChildHostHandle(&paintWindow));
	}
	Check(form->FindControl(RSTRING_CAST(L"image"),&image));
	Check(form->FindControl(RSTRING_CAST(L"below"),&below));
	Check(image->QueryInterface(__uuidof(IQuadooStaticControl),reinterpret_cast<PVOID*>(&painted)));
	REQUIRE(image->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&windowed)) == E_NOINTERFACE && !windowed);
	{
		HWND content = GetWindow(window,GW_CHILD);
		REQUIRE(content && !GetWindow(content,GW_CHILD) && !GetWindow(content,GW_HWNDNEXT));
	}
	{
		TStackRef<IUnknown> canonical, throughPaint, throughProperties;
		TStackRef<IQuadooProperties> properties;
		Check(image->QueryInterface(__uuidof(IUnknown),reinterpret_cast<PVOID*>(&canonical)));
		Check(painted->QueryInterface(__uuidof(IUnknown),reinterpret_cast<PVOID*>(&throughPaint)));
		Check(image->GetProperties(&properties));
		Check(properties->QueryInterface(__uuidof(IUnknown),reinterpret_cast<PVOID*>(&throughProperties)));
		REQUIRE(canonical == throughPaint && canonical == throughProperties);
	}
	Check(image->QueryInterface(__uuidof(IQuadooPropertyNotifications),reinterpret_cast<PVOID*>(&notifications)));
	Check(notifications->AddObserver(observer));
	REQUIRE(ImageInteger(image,L"ImageWidth") == 0 && ImageInteger(image,L"Opacity") == 255 && ImageInteger(image,L"SizeMode") == 1);
	Check(ImageFixtureValue(L"png",ImageRGBA,sizeof(ImageRGBA),&picture));
	Check(ImageSet(image,L"Picture",picture));
	REQUIRE(observer->pictures == 1 && observer->diagnostics == 5 && !binder->clicks);
	REQUIRE(ImageInteger(image,L"ImageWidth") == 4 && ImageInteger(image,L"ImageHeight") == 2 && ImageInteger(image,L"ImageSize") == sizeof(ImageRGBA));
	{
		TStackRef<IQuadooProperties> properties;
		TStackRef<IQuadooPropertyDescriptors> descriptors;
		TStackRef<IQuadooPropertyDescriptor> descriptor;
		UINT index, choices;
		INT integer;
		BOOL flag;
		CRString format;
		Check(ImageProperty(image,L"ImageWidth",&properties,&index));
		Check(properties->GetValueInt(index,&integer));
		REQUIRE(integer == 4 && properties->SetValueInt(index,9) == E_ACCESSDENIED);
		properties.Release();
		Check(ImageProperty(image,L"HasAlpha",&properties,&index));
		Check(properties->GetValueBool(index,&flag));
		REQUIRE(flag);
		properties.Release();
		Check(ImageProperty(image,L"ImageFormat",&properties,&index));
		Check(properties->GetValueString(index,&format));
		REQUIRE(format == RSTRING_CAST(L"PNG"));
		Check(properties->GetDescriptors(&descriptors));
		Check(descriptors->Find(RSTRING_CAST(L"ImageAlignment"),&descriptor));
		Check(descriptor->GetEnumCount(&choices));
		REQUIRE(choices == 9);
		descriptor.Release();
		Check(descriptors->Find(RSTRING_CAST(L"Picture"),&descriptor));
		QuadooProperty::Type type;
		Check(descriptor->GetType(&type));
		REQUIRE(type == QuadooProperty::Picture);
	}
	Check(canvas.Create(64,40));
	canvas.Fill(RGB(255,255,255));
	Check(painted->Paint(canvas.DC(),&area));
	REQUIRE(canvas.Pixel(1,1) == RGB(255,0,0) && canvas.Pixel(4,1) == RGB(255,255,255));
	REQUIRE(canvas.Pixel(0,0) == RGB(255,255,255));
	Check(ImageFlag(image,L"Transparent",FALSE));
	Check(ImageNumber(image,L"BackColor",RGB(9,41,73)));
	Check(ImageNumber(image,L"Opacity",0));
	Check(painted->Paint(canvas.DC(),&area));
	REQUIRE(canvas.Pixel(1,1) == RGB(9,41,73) && canvas.Pixel(4,1) == RGB(9,41,73));
	Check(ImageFlag(image,L"Transparent",TRUE));
	Check(ImageNumber(image,L"Opacity",255));
	REQUIRE(ImageNumber(image,L"Opacity",256) == E_INVALIDARG && ImageNumber(image,L"Opacity",-1) == E_INVALIDARG);
	Check(ImageNumber(image,L"TileOffsetX",-7));
	Check(ImageNumber(image,L"TileOffsetY",-9));
	REQUIRE(ImageInteger(image,L"TileOffsetX") == -7 && ImageInteger(image,L"TileOffsetY") == -9);
	Check(ImageRead(image,L"Picture",&copy));
	{
		TStackRef<IJSONObject> object;
		Check(copy->GetObject(&object));
		Check(JSONCreateString(RSTRING_CAST(L"broken"),&value));
		Check(object->AddValueW(L"format",value));
		value.Release();
		REQUIRE(FAILED(ImageSet(image,L"Picture",copy)));
	}
	copy.Release();
	Check(ImageRead(image,L"Picture",&copy));
	REQUIRE(!JSONCompare(copy,picture) && observer->pictures == 1);
	Check(painted->HitTest(10,12,&hit));
	REQUIRE(hit);
	Check(painted->HitTest(50,32,&hit));
	REQUIRE(!hit);
	Check(image->SetEnabled(FALSE));
	Check(painted->HitTest(20,20,&hit));
	REQUIRE(!hit);
	Check(image->SetEnabled(TRUE));
	Check(image->SetVisible(FALSE));
	Check(painted->HitTest(20,20,&hit));
	REQUIRE(!hit);
	Check(image->SetVisible(TRUE));
	canvas.Fill(RGB(0,0,0));
	SendMessageW(paintWindow,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(canvas.DC()),PRF_CLIENT);
	REQUIRE(canvas.Pixel(10,12) == RGB(255,0,0) && canvas.Pixel(45,15) == RGB(0,0,255));
	REQUIRE(canvas.Pixel(0,0) == RGB(17,34,51));
	SendMessageW(paintWindow,WM_LBUTTONUP,0,MAKELPARAM(20,17));
	REQUIRE(binder->clicks == 1);
	Check(ImageSet(image,L"Picture",NULL));
	REQUIRE(ImageInteger(image,L"ImageWidth") == 0 && observer->pictures == 2 && observer->diagnostics == 10);
	Check(ImageSet(image,L"Picture",picture));
	binder->destroySender = TRUE;
	SendMessageW(paintWindow,WM_LBUTTONUP,0,MAKELPARAM(20,17));
	REQUIRE(binder->clicks == 2 && painted->Paint(canvas.DC(),&area) == QuadooForm::EDestroyed);
	Check(form->Destroy());
	painted.Release(); image.Release(); below.Release(); notifications.Release();
	{
		UINT live;
		Check(lifetime->GetLiveCount(&live));
		REQUIRE(live == 0);
	}
	hr = S_OK;
Cleanup:
	if(form) form->Destroy();
	return hr;
}
static HRESULT ImagePersistenceAndLayout()
{
	HRESULT hr;
	TStackRef<IJSONObject> definition, saved, imageDefinition, properties;
	TStackRef<IJSONArray> children;
	TStackRef<IJSONValue> picture, icon, value, roundTrip;
	TStackRef<IQuadooForm> form, reopened;
	TStackRef<IQuadooControl> image, docked, client, iconControl, other;
	TStackRef<IQuadooStaticControl> painted;
	TStackRef<IQuadooLayoutControl> layout;
	TStackRef<IQuadooFormSerializer> serializer;
	QuadooForm::Rect requested, effective, rootBounds;
	CImageTestCanvas canvas;
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"ImageLayout\",\"properties\":{\"Width\":300,\"Height\":220},\"children\":[{\"type\":\"ScrollingPanel\",\"name\":\"host\",\"properties\":{\"Align\":\"Client\",\"Padding\":{\"Left\":5,\"Top\":7}},\"children\":[{\"type\":\"Image\",\"name\":\"image\",\"properties\":{\"Left\":4,\"Top\":6,\"Width\":111,\"Height\":112,\"AutoSize\":true,\"SizeMode\":\"Normal\"}},{\"type\":\"Image\",\"name\":\"docked\",\"properties\":{\"Align\":\"Top\",\"AutoSize\":true}},{\"type\":\"Image\",\"name\":\"client\",\"properties\":{\"Align\":\"Client\",\"AutoSize\":true}},{\"type\":\"Image\",\"name\":\"icon\",\"properties\":{\"Left\":500,\"Top\":500,\"AutoSize\":true}}]}]}",&definition));
	Check(QuadooCreateForm(NULL,NULL,definition,&form));
	Check(form->FindControl(RSTRING_CAST(L"image"),&image));
	Check(form->FindControl(RSTRING_CAST(L"docked"),&docked));
	Check(form->FindControl(RSTRING_CAST(L"client"),&client));
	Check(form->FindControl(RSTRING_CAST(L"icon"),&iconControl));
	Check(ImageFixtureValue(L"png",ImageRGBA,sizeof(ImageRGBA),&picture));
	Check(ImageFixtureValue(L"ico",ImageMultiICO,sizeof(ImageMultiICO),&icon));
	Check(ImageSet(image,L"Picture",picture));
	Check(ImageSet(docked,L"Picture",picture));
	Check(ImageSet(client,L"Picture",picture));
	Check(ImageSet(iconControl,L"Picture",icon));
	Check(ImageNumber(image,L"Interpolation",0));
	Check(image->QueryInterface(__uuidof(IQuadooStaticControl),reinterpret_cast<PVOID*>(&painted)));
	Check(form->QueryInterface(__uuidof(IQuadooLayoutControl),reinterpret_cast<PVOID*>(&layout)));
	Check(form->GetBounds(&rootBounds));
	Check(image->GetBounds(&requested));
	REQUIRE(requested.Width == 111 && requested.Height == 112);
	Check(canvas.Create(20,12));
	for(UINT pass = 0; pass < 5; pass++)
		for(UINT dpi = 96; dpi <= 192; dpi += 48)
		{
			Check(layout->Arrange(&rootBounds,dpi));
			Check(image->GetEffectiveBounds(&effective));
			REQUIRE(effective.Width == 4 && effective.Height == 2);
			Check(image->GetBounds(&effective));
			REQUIRE(EqualBounds(requested,effective));
			Check(docked->GetEffectiveBounds(&effective));
			REQUIRE(effective.Width > 4 && effective.Height == 2);
			Check(client->GetEffectiveBounds(&effective));
			REQUIRE(effective.Width > 4 && effective.Height > 2);
			Check(iconControl->GetEffectiveBounds(&effective));
			REQUIRE(effective.Width == 32 && effective.Height == 32);
			RECT physical = {0,0,MulDiv(4,dpi,96),MulDiv(2,dpi,96)};
			canvas.Fill(RGB(255,255,255));
			Check(painted->Paint(canvas.DC(),&physical));
			REQUIRE(canvas.Pixel(0,0) == RGB(255,0,0) && ImageInteger(image,L"ImageWidth") == 4);
		}
	Check(QuadooGetFormSerializer(&serializer));
	Check(serializer->Serialize(image,&imageDefinition));
	Check(imageDefinition->FindNonNullValueW(L"properties",&value));
	Check(value->GetObject(&properties));
	value.Release();
	REQUIRE(!properties->HasField(RSTRING_CAST(L"ImageWidth")) && !properties->HasField(RSTRING_CAST(L"ImageFormat")) && !properties->HasField(RSTRING_CAST(L"HasAlpha")));
	Check(ImageRead(iconControl,L"Picture",&roundTrip));
	REQUIRE(!JSONCompare(roundTrip,icon)); roundTrip.Release();
	Check(serializer->Serialize(form,&saved));
	Check(QuadooCreateForm(NULL,NULL,saved,&reopened));
	Check(reopened->FindControl(RSTRING_CAST(L"icon"),&other));
	Check(ImageRead(other,L"Picture",&roundTrip));
	REQUIRE(!JSONCompare(roundTrip,icon));
	Check(reopened->Destroy());
	reopened.Release(); other.Release();
	{
		TStackRef<IJSONObject> corrupt;
		TStackRef<IJSONValue> bad;
		Check(JSONCreateObject(&corrupt));
		Check(JSONCreateString(RSTRING_CAST(L"png"),&bad));
		Check(corrupt->AddValueW(L"format",bad));
		bad.Release();
		Check(JSONCreateString(RSTRING_CAST(L"base64"),&bad));
		Check(corrupt->AddValueW(L"encoding",bad));
		bad.Release();
		Check(JSONCreateString(RSTRING_CAST(L"AAAA"),&bad));
		Check(corrupt->AddValueW(L"data",bad));
		bad.Release();
		Check(JSONWrapObject(corrupt,&bad));
		REQUIRE(FAILED(ImageSet(image,L"Picture",bad)) && ImageInteger(image,L"ImageWidth") == 4);
		REQUIRE(FAILED(ImageSet(iconControl,L"Picture",bad)) && ImageInteger(iconControl,L"ImageWidth") == 32);
	}
	Check(form->Destroy());
	hr = S_OK;
Cleanup:
	if(reopened) reopened->Destroy();
	if(form) form->Destroy();
	return hr;
}
