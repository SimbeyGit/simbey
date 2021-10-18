#include <windows.h>
#include "..\..\Shared\Library\Core\CoreDefs.h"
#include "..\..\Shared\Library\Core\StringCore.h"
#include "inc\StringGalleryItem.h"

HRESULT CStringGalleryItem::Create (CSIFRibbon* pRibbon, __deref_out CStringGalleryItem** ppItem)
{
	HRESULT hr;

	CheckIf(NULL == pRibbon, E_INVALIDARG);

	*ppItem = __new CStringGalleryItem(pRibbon);
	CheckAlloc(*ppItem);

	hr = S_OK;

Cleanup:
	return hr;
}

CStringGalleryItem::CStringGalleryItem (CSIFRibbon* pRibbon) :
	m_pRibbon(pRibbon),
	m_pwzItem(NULL),
	m_nIcon(0)
{
	m_pRibbon->AddRef();
}

CStringGalleryItem::~CStringGalleryItem ()
{
	SafeDeleteArray(m_pwzItem);
	m_pRibbon->Release();
}

HRESULT CStringGalleryItem::SetItemText (PCWSTR pcwzItem)
{
	HRESULT hr;

	CheckIf(NULL == pcwzItem, E_INVALIDARG);
	Check(TReplaceStringAssert(pcwzItem, &m_pwzItem));

Cleanup:
	return hr;
}

HRESULT CStringGalleryItem::SetItemText (PCSTR pcszItem)
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

VOID CStringGalleryItem::SetItemIcon (UINT nIcon)
{
	m_nIcon = nIcon;
}

// IUISimplePropertySet

HRESULT STDMETHODCALLTYPE CStringGalleryItem::GetValue (REFPROPERTYKEY key, PROPVARIANT* value)
{
	HRESULT hr = E_NOTIMPL;

	if(UI_PKEY_Label == key)
	{
		value->bstrVal = SysAllocString(m_pwzItem);
		CheckIf(NULL == value->bstrVal, E_OUTOFMEMORY);
		value->vt = VT_BSTR;
		hr = S_OK;
	}
	else if(UI_PKEY_ItemImage == key && 0 != m_nIcon)
	{
		Check(m_pRibbon->LoadImageForCommand(m_nIcon, UI_PKEY_SmallImage, value));
	}

Cleanup:
	return hr;
}