#pragma once

#include "SIFRibbon.h"

class CSIFRibbonItem :
	public CBaseUnknown,
	public IUISimplePropertySet
{
protected:
	CSIFRibbon* m_pRibbon;
	ISimbeyInterchangeFile* m_pIcons;

	PWSTR m_pwzItem;
	UINT m_nIcon;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IUISimplePropertySet)
	END_UNK_MAP

	static HRESULT Create (CSIFRibbon* pRibbon, ISimbeyInterchangeFile* pIcons, __deref_out CSIFRibbonItem** ppItem);

	HRESULT SetItemText (PCWSTR pcwzItem);
	HRESULT SetItemText (PCSTR pcszItem);
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
