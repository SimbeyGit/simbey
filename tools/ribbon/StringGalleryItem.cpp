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
	CBaseRibbonItem(pRibbon),
	m_nIcon(0)
{
}

CStringGalleryItem::~CStringGalleryItem ()
{
}

VOID CStringGalleryItem::SetItemIcon (UINT nIcon)
{
	m_nIcon = nIcon;
}

// IUISimplePropertySet

HRESULT STDMETHODCALLTYPE CStringGalleryItem::GetValue (REFPROPERTYKEY key, PROPVARIANT* value)
{
	HRESULT hr = __super::GetValue(key, value);

	if(E_NOTIMPL == hr && 0 != m_nIcon)
	{
		if(UI_PKEY_ItemImage == key)
			hr = m_pRibbon->LoadImageForCommand(m_nIcon, UI_PKEY_SmallImage, value);
		else if(UI_PKEY_SmallImage == key || UI_PKEY_LargeImage == key)
			hr = m_pRibbon->LoadImageForCommand(m_nIcon, key, value);
	}

	return hr;
}
