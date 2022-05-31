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
	CBaseRibbonItem(pRibbon),
	m_pIcons(pIcons),
	m_nIcon(0)
{
	m_pIcons->AddRef();
}

CSIFRibbonItem::~CSIFRibbonItem ()
{
	m_pIcons->Release();
}

VOID CSIFRibbonItem::SetItemIcon (UINT nIcon)
{
	m_nIcon = nIcon;
}

// IUISimplePropertySet

HRESULT STDMETHODCALLTYPE CSIFRibbonItem::GetValue (REFPROPERTYKEY key, PROPVARIANT* value)
{
	HRESULT hr = __super::GetValue(key, value);

	if(E_NOTIMPL != hr && 0 != m_nIcon)
	{
		if(UI_PKEY_ItemImage == key)
			hr = LoadRibbonImage(value);
		else if(UI_PKEY_LargeImage == key || UI_PKEY_SmallImage == key)
			hr = LoadRibbonImage(value);
	}

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
