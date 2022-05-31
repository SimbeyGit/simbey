#include <windows.h>
#include "..\..\Shared\Library\Core\CoreDefs.h"
#include "..\..\Shared\Library\Core\StringCore.h"
#include "BaseRibbonItem.h"

CBaseRibbonItem::CBaseRibbonItem (CSIFRibbon* pRibbon) :
	m_pRibbon(pRibbon),
	m_punkObject(NULL),
	m_pwzItem(NULL)
{
	m_pRibbon->AddRef();
}

CBaseRibbonItem::~CBaseRibbonItem ()
{
	SafeDeleteArray(m_pwzItem);
	SafeRelease(m_punkObject);
	m_pRibbon->Release();
}

VOID CBaseRibbonItem::SetObject (__in_opt IUnknown* punkObject)
{
	ReplaceInterface(m_punkObject, punkObject);
}

HRESULT CBaseRibbonItem::SetItemText (PCWSTR pcwzItem)
{
	HRESULT hr;

	CheckIf(NULL == pcwzItem, E_INVALIDARG);
	Check(TReplaceStringAssert(pcwzItem, &m_pwzItem));

Cleanup:
	return hr;
}

HRESULT CBaseRibbonItem::SetItemText (PCSTR pcszItem)
{
	HRESULT hr;
	INT cchItem;

	CheckIf(NULL == pcszItem, E_INVALIDARG);

	cchItem = MultiByteToWideChar(CP_UTF8, 0, pcszItem, -1, NULL, 0);
	CheckIf(0 == cchItem, E_INVALIDARG);

	m_pwzItem = __new WCHAR[cchItem];
	CheckAlloc(m_pwzItem);

	SideAssertCompare(MultiByteToWideChar(CP_UTF8, 0, pcszItem, -1, m_pwzItem, cchItem), cchItem);

	hr = S_OK;

Cleanup:
	return hr;
}

// IServiceProvider

HRESULT STDMETHODCALLTYPE CBaseRibbonItem::QueryService (REFGUID guidService, REFIID riid, PVOID* ppvObject)
{
	HRESULT hr;

	CheckIf(NULL == m_punkObject, E_NOTIMPL);
	Check(m_punkObject->QueryInterface(riid, ppvObject));

Cleanup:
	return hr;
}

// IUISimplePropertySet

HRESULT STDMETHODCALLTYPE CBaseRibbonItem::GetValue (REFPROPERTYKEY key, PROPVARIANT* value)
{
	HRESULT hr;

	CheckIfIgnore(UI_PKEY_Label != key, E_NOTIMPL);
	value->bstrVal = SysAllocString(m_pwzItem);
	CheckIf(NULL == value->bstrVal, E_OUTOFMEMORY);
	value->vt = VT_BSTR;
	hr = S_OK;

Cleanup:
	return hr;
}
