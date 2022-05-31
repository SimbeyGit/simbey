#pragma once

#include "BaseRibbonItem.h"

class CStringGalleryItem : public CBaseRibbonItem
{
protected:
	UINT m_nIcon;

public:
	static HRESULT Create (CSIFRibbon* pRibbon, __deref_out CStringGalleryItem** ppItem);

	VOID SetItemIcon (UINT nIcon);

protected:
	CStringGalleryItem (CSIFRibbon* pRibbon);
	~CStringGalleryItem ();

public:
	// IUISimplePropertySet
	virtual HRESULT STDMETHODCALLTYPE GetValue (REFPROPERTYKEY key, PROPVARIANT* value);
};
