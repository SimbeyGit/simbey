#include "..\TestImages\ImageListFixture.h"

static HRESULT ImageListNative()
{
	HRESULT hr;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooComponentRegistry> factories;
	TStackRef<IQuadooComponentFactory> factory;
	TStackRef<IQuadooFactoryLifetime> lifetime;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IQuadooControl> root;
	TStackRef<IQuadooControl> visual;
	TStackRef<IQuadooWinControl> native;
	TStackRef<IQuadooComponentHost> host;
	TStackRef<IQuadooComponentCollection> components;
	TStackRef<IQuadooComponent> component;
	TStackRef<IQuadooNonvisualComponent> nonvisual;
	TStackRef<IQuadooProperties> properties;
	TStackRef<IQuadooFormSerializer> serializer;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONObject> saved;
	TStackRef<IJSONObject> again;
	TStackRef<IJSONValue> before;
	TStackRef<IJSONValue> after;
	TStackRef<IJSONValue> value;
	QuadooForm::ContextOptions options = {0};
	GUID id;
	GUID loaded;
	UINT baseline;
	UINT live;
	UINT index;
	INT count;
	Check(QuadooGetControlRegistry(&registry));
	Check(registry->QueryInterface(__uuidof(IQuadooComponentRegistry),reinterpret_cast<PVOID*>(&factories)));
	Check(factories->FindComponentFactory(RSTRING_CAST(L"ImageList"),&factory));
	Check(factory->QueryInterface(__uuidof(IQuadooFactoryLifetime),reinterpret_cast<PVOID*>(&lifetime)));
	Check(lifetime->GetLiveCount(&baseline));
	options.cbSize = sizeof(options);
	options.Mode = QuadooForm::Runtime;
	options.pRegistry = registry;
	Check(QuadooCreateConstructionContext(&options,&context));
	Check(Parse(ImageListNativeDefinition,&definition));
	Check(QuadooCreateControl(context,NULL,definition,&root));
	Check(root->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&host)));
	Check(host->GetComponents(&components));
	Check(components->FindComponent(RSTRING_CAST(L"ImageList1"),&component));
	Check(component->GetID(&id));
	Check(component->QueryInterface(__uuidof(IQuadooNonvisualComponent),reinterpret_cast<PVOID*>(&nonvisual)));
	REQUIRE(component->QueryInterface(__uuidof(IQuadooControl),reinterpret_cast<PVOID*>(&visual)) == E_NOINTERFACE && !visual);
	REQUIRE(component->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native)) == E_NOINTERFACE && !native);
	Check(Property(component,L"ImageCount",&properties,&index));
	Check(properties->GetValueInt(index,&count));
	REQUIRE(count == 2);
	REQUIRE(properties->SetValueInt(index,10) == E_ACCESSDENIED);
	properties.Release();
	Check(Property(component,L"Images",&properties,&index));
	Check(properties->GetValueJSON(index,&before));
	properties.Release();
	Check(Property(component,L"Width",&properties,&index));
	Check(properties->SetValueInt(index,32));
	REQUIRE(properties->SetValueInt(index,0) == E_INVALIDARG);
	properties.Release();
	Check(Property(component,L"Height",&properties,&index));
	Check(properties->SetValueInt(index,24));
	REQUIRE(properties->SetValueInt(index,257) == E_INVALIDARG);
	properties.Release();
	Check(Property(component,L"Images",&properties,&index));
	Check(properties->GetValueJSON(index,&after));
	REQUIRE(JSONCompare(before,after) == 0);
	after.Release();
	value.Release();
	{
		static const WCHAR invalid[] = L"[{\"format\":\"png\",\"encoding\":\"base64\",\"data\":\"%%%%\"}]";
		Check(JSONParse(NULL,invalid,StaticLength(invalid),&value));
		REQUIRE(FAILED(properties->SetValueJSON(index,value)));
		Check(properties->GetValueJSON(index,&after));
		REQUIRE(JSONCompare(before,after) == 0);
	}
	value.Release();
	after.Release();
	{
		static const WCHAR missing[] = L"[{\"format\":\"png\",\"encoding\":\"base64\"}]";
		Check(JSONParse(NULL,missing,StaticLength(missing),&value));
		REQUIRE(FAILED(properties->SetValueJSON(index,value)));
		value.Release();
		TStackRef<IJSONArray> entries;
		TStackRef<IJSONArray> excessive;
		TStackRef<IJSONValue> entry;
		Check(before->GetArray(&entries));
		Check(entries->GetValue(0,&entry));
		Check(JSONCreateArray(&excessive));
		for(UINT n = 0; n < 257; n++)
		{
			Check(excessive->Add(entry));
		}
		Check(JSONWrapArray(excessive,&value));
		REQUIRE(FAILED(properties->SetValueJSON(index,value)));
		Check(properties->GetValueJSON(index,&after));
		REQUIRE(JSONCompare(before,after) == 0);
	}
	value.Release();
	after.Release();
	{
		TStackRef<IJSONArray> images;
		TStackRef<IJSONObject> entry;
		TStackRef<IJSONValue> enabled;
		TStackRef<IJSONValue> invalid;
		Check(JSONClone(before,&value,TRUE));
		Check(value->GetArray(&images));
		Check(images->GetObject(0,&entry));
		Check(JSONCreateBool(true,&enabled));
		Check(entry->AddValueW(L"colorKey",enabled));
		Check(properties->SetValueJSON(index,value));
		Check(properties->GetValueJSON(index,&after));
		REQUIRE(JSONCompare(value,after) == 0);
		Check(JSONCreateInteger(-1,&invalid));
		Check(entry->AddValueW(L"maskColor",invalid));
		REQUIRE(FAILED(properties->SetValueJSON(index,value)));
		value.Release();
		Check(properties->GetValueJSON(index,&value));
		REQUIRE(JSONCompare(value,after) == 0);
	}
	value.Release();
	properties.Release();
	Check(Property(component,L"ColorDepth",&properties,&index));
	{
		const INT depths[] = {4,8,16,24,32};
		for(UINT n = 0; n < ARRAYSIZE(depths); n++)
		{
			Check(properties->SetValueInt(index,depths[n]));
			INT stored;
			Check(properties->GetValueInt(index,&stored));
			REQUIRE(stored == depths[n]);
		}
		REQUIRE(FAILED(properties->SetValueInt(index,12)));
	}
	Check(component->SetName(RSTRING_CAST(L"ImagesRenamed")));
	Check(QuadooGetFormSerializer(&serializer));
	Check(serializer->Serialize(root,&saved));
	Check(root->Destroy());
	nonvisual.Release();
	properties.Release();
	component.Release();
	components.Release();
	host.Release();
	root.Release();
	Check(QuadooCreateControl(context,NULL,saved,&root));
	Check(serializer->Serialize(root,&again));
	REQUIRE(JSONCompareObjects(saved,again) == 0);
	Check(root->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&host)));
	Check(host->GetComponents(&components));
	Check(components->FindComponent(RSTRING_CAST(L"ImagesRenamed"),&component));
	Check(component->GetID(&loaded));
	REQUIRE(IsEqualGUID(id,loaded));
	Check(lifetime->GetLiveCount(&live));
	REQUIRE(live == baseline + 1);
	Check(root->Destroy());
	component.Release();
	components.Release();
	host.Release();
	root.Release();
	Check(lifetime->GetLiveCount(&live));
	REQUIRE(live == baseline);
Cleanup:
	if(root)
	{
		root->Destroy();
	}
	return hr;
}
