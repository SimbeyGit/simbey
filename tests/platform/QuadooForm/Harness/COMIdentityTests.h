#pragma once

static HRESULT CheckCOMIdentity(IUnknown* object, const IID** interfaces, UINT count)
{
	HRESULT hr;
	TStackRef<IUnknown> identity;
	Check(object->QueryInterface(IID_IUnknown, reinterpret_cast<PVOID*>(&identity)));
	for(UINT n = 0; n < count; n++)
	{
		TStackRef<IUnknown> path, same;
		ULONG baseline = identity->AddRef(); identity->Release();
		Check(object->QueryInterface(*interfaces[n], reinterpret_cast<PVOID*>(&path)));
		ULONG withQuery = identity->AddRef(); identity->Release(); REQUIRE(withQuery == baseline + 1);
		Check(path->QueryInterface(IID_IUnknown, reinterpret_cast<PVOID*>(&same))); REQUIRE(static_cast<IUnknown*>(same) == static_cast<IUnknown*>(identity)); same.Release();
		for(UINT other = 0; other < count; other++)
		{
			TStackRef<IUnknown> cross, crossIdentity;
			Check(path->QueryInterface(*interfaces[other], reinterpret_cast<PVOID*>(&cross)));
			Check(cross->QueryInterface(IID_IUnknown, reinterpret_cast<PVOID*>(&crossIdentity)));
			REQUIRE(static_cast<IUnknown*>(crossIdentity) == static_cast<IUnknown*>(identity));
		}
		REQUIRE(path->QueryInterface(IID_IUnknown, NULL) == E_POINTER);
		PVOID missing = NULL;
		REQUIRE(path->QueryInterface(__uuidof(IQuadooPropertyDescriptor), &missing) == E_NOINTERFACE && !missing);
		path.Release(); ULONG after = identity->AddRef(); identity->Release(); REQUIRE(after == baseline);
	}
Cleanup:
	return hr;
}
