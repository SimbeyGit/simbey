#pragma once

#include "SIFRibbon.h"

class CStringGalleryItem :
	public CBaseUnknown,
	public IUISimplePropertySet
{
protected:
	CSIFRibbon* m_pRibbon;

	PWSTR m_pwzItem;
	UINT m_nIcon;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IUISimplePropertySet)
	END_UNK_MAP

	static HRESULT Create (CSIFRibbon* pRibbon, __deref_out CStringGalleryItem** ppItem);

	HRESULT SetItemText (PCWSTR pcwzItem);
	HRESULT SetItemText (PCSTR pcszItem);
	VOID SetItemIcon (UINT nIcon);

protected:
	CStringGalleryItem (CSIFRibbon* pRibbon);
	~CStringGalleryItem ();

public:
	// IUISimplePropertySet
	virtual HRESULT STDMETHODCALLTYPE GetValue (REFPROPERTYKEY key, PROPVARIANT* value);
};