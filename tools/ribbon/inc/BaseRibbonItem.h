#pragma once

#include "..\inc\SIFRibbon.h"

class CBaseRibbonItem :
	public CBaseUnknown,
	public IServiceProvider,
	public IUISimplePropertySet
{
protected:
	CSIFRibbon* m_pRibbon;
	IUnknown* m_punkObject;
	PWSTR m_pwzItem;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IServiceProvider)
		UNK_INTERFACE(IUISimplePropertySet)
	END_UNK_MAP

public:
	VOID SetObject (__in_opt IUnknown* punkObject);
	HRESULT SetItemText (PCWSTR pcwzItem);
	HRESULT SetItemText (PCSTR pcszItem);

protected:
	CBaseRibbonItem (CSIFRibbon* pRibbon);
	~CBaseRibbonItem ();

public:
	// IServiceProvider
	virtual HRESULT STDMETHODCALLTYPE QueryService (REFGUID guidService, REFIID riid, PVOID* ppvObject);

	// IUISimplePropertySet
	virtual HRESULT STDMETHODCALLTYPE GetValue (REFPROPERTYKEY key, PROPVARIANT* value);
};
