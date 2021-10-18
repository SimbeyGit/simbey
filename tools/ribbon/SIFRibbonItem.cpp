#include <windows.h>
#include "..\..\Shared\Library\Core\CoreDefs.h"
#include "..\..\Shared\Library\Core\StringCore.h"
#include "inc\SIFRibbonItem.h"

HRESULT CSIFRibbonItem::Create (CSIFRibbon* pRibbon, ISimbeyInterchangeFile* pIcons, __deref_out CSIFRibbonItem** ppItem)
{
	HRESULT hr;

	CheckIf(NULL == pRibbon, E_INVALIDARG);

	*ppItem = __new CSIFRibbonItem(pRibbon, pIcons);
	CheckAlloc(*ppItem);

	hr = S_OK;

Cleanup:
	return hr;
}

CSIFRibbonItem::CSIFRibbonItem (CSIFRibbon* pRibbon, ISimbeyInterchangeFile* pIcons) :
	m_pRibbon(pRibbon),
	m_pIcons(pIcons),
	m_pwzItem(NULL),
	m_nIcon(0)
{
	m_pRibbon->AddRef();
	m_pIcons->AddRef();
}

CSIFRibbonItem::~CSIFRibbonItem ()
{
	SafeDeleteArray(m_pwzItem);
	m_pIcons->Release();
	m_pRibbon->Release();
}

HRESULT CSIFRibbonItem::SetItemText (PCWSTR pcwzItem)
{
	HRESULT hr;

	CheckIf(NULL == pcwzItem, E_INVALIDARG);
	Check(TReplaceStringAssert(pcwzItem, &m_pwzItem));

Cleanup:
	return hr;
}

HRESULT CSIFRibbonItem::SetItemText (PCSTR pcszItem)
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

VOID CSIFRibbonItem::SetItemIcon (UINT nIcon)
{
	m_nIcon = nIcon;
}

// IUISimplePropertySet

HRESULT STDMETHODCALLTYPE CSIFRibbonItem::GetValue (REFPROPERTYKEY key, PROPVARIANT* value)
{
	HRESULT hr = E_NOTIMPL;

	if(UI_PKEY_Label == key)
	{
		value->bstrVal = SysAllocString(m_pwzItem);
		CheckIf(NULL == value->bstrVal, E_OUTOFMEMORY);
		value->vt = VT_BSTR;
		hr = S_OK;
	}
	else if(0 != m_nIcon)
	{
		if(UI_PKEY_ItemImage == key)
			Check(LoadRibbonImage(value));
		else if(UI_PKEY_LargeImage == key || UI_PKEY_SmallImage == key)
			Check(LoadRibbonImage(value));
	}

Cleanup:
	return hr;
}

HRESULT CSIFRibbonItem::LoadRibbonImage (PROPVARIANT* pValue)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeFileLayer> srLayer;
	TStackRef<IUIImage> srImage;
	HBITMAP hDIB = NULL;

	Check(m_pIcons->GetLayer(m_nIcon, &srLayer));
	Check(srLayer->GetAsDIB(NULL, &hDIB));
	Check(m_pRibbon->CreateImageFromDIB(hDIB, &srImage));
	hDIB = NULL;	// Now owned by srImage

	pValue->punkVal = srImage.Detach();
	pValue->vt = VT_UNKNOWN;

Cleanup:
	return hr;
}
