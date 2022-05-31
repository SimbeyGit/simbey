#pragma once

#include "..\inc\BaseRibbonItem.h"

class CSwatchItem : public CBaseRibbonItem
{
protected:
	HRESULT (WINAPI* m_pfnCreateSwatchImage)(CSIFRibbon*, INT, COLORREF, PROPVARIANT*);
	COLORREF m_crSwatch;

public:
	static HRESULT Create (CSIFRibbon* pRibbon, __deref_out CSwatchItem** ppItem);

public:
	VOID SetSwatchColor (COLORREF crSwatch);
	COLORREF GetSwatchColor (VOID) { return m_crSwatch; }
	VOID SetSwatchFunction (HRESULT (WINAPI* pfnSwatch)(CSIFRibbon*, INT, COLORREF, PROPVARIANT*));

	static HRESULT WINAPI SquareSwatchImage (CSIFRibbon* pRibbon, INT nSize, COLORREF cr, __out PROPVARIANT* pValue);
	static HRESULT WINAPI CircleSwatchImage (CSIFRibbon* pRibbon, INT nSize, COLORREF cr, __out PROPVARIANT* pValue);

protected:
	CSwatchItem (CSIFRibbon* pRibbon);
	~CSwatchItem ();

public:
	// IUISimplePropertySet
	virtual HRESULT STDMETHODCALLTYPE GetValue (REFPROPERTYKEY key, PROPVARIANT* value);
};
