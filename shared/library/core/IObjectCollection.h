#pragma once

#if (defined(_MSC_VER) && (_MSC_VER >= 1600))	// Visual Studio 2010
#include <ObjectArray.h>
#else

interface __declspec(uuid("92CA9DCD-5622-4bba-A805-5E9F541BD8C9")) IObjectArray : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE GetCount (__out UINT* pcObjects) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetAt (UINT uiIndex, REFIID riid, __deref_out_opt PVOID* ppv) = 0;
};

interface __declspec(uuid("5632b1a4-e38a-400a-928a-d4cd63230295")) IObjectCollection : IObjectArray
{
	virtual HRESULT STDMETHODCALLTYPE AddObject (__in_opt IUnknown* punk) = 0;
	virtual HRESULT STDMETHODCALLTYPE AddFromArray (__in_opt IObjectArray* poaSource) = 0;
	virtual HRESULT STDMETHODCALLTYPE RemoveObjectAt (UINT uiIndex) = 0;
	virtual HRESULT STDMETHODCALLTYPE Clear (VOID) = 0;
};

#endif

interface __declspec(uuid("DBF76BD5-D58F-4873-A175-2C8250BB3DBB")) IObjectCollectionEx : IObjectCollection
{
	virtual HRESULT STDMETHODCALLTYPE GetItem (UINT32 index, __deref_out_opt IUnknown** item) = 0;
	virtual HRESULT STDMETHODCALLTYPE Insert (UINT32 index, IUnknown* item) = 0;
	virtual HRESULT STDMETHODCALLTYPE Replace (UINT32 indexReplaced, IUnknown* itemReplaceWith) = 0;
	virtual HRESULT STDMETHODCALLTYPE Compact (VOID) = 0;
};
