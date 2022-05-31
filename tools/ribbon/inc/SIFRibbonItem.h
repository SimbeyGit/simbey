#pragma once

#include "BaseRibbonItem.h"

class CSIFRibbonItem : public CBaseRibbonItem
{
protected:
	ISimbeyInterchangeFile* m_pIcons;
	UINT m_nIcon;

public:
	static HRESULT Create (CSIFRibbon* pRibbon, ISimbeyInterchangeFile* pIcons, __deref_out CSIFRibbonItem** ppItem);

	VOID SetItemIcon (UINT nIcon);

protected:
	CSIFRibbonItem (CSIFRibbon* pRibbon, ISimbeyInterchangeFile* pIcons);
	~CSIFRibbonItem ();

public:
	// IUISimplePropertySet
	virtual HRESULT STDMETHODCALLTYPE GetValue (REFPROPERTYKEY key, PROPVARIANT* value);

private:
	HRESULT LoadRibbonImage (PROPVARIANT* pValue);
};
