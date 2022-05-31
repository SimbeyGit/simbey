#include <math.h>
#include <windows.h>
#include "..\..\..\Shared\Library\Core\CoreDefs.h"
#include "..\..\..\Shared\Library\Core\StringCore.h"
#include "..\..\..\Shared\Library\DPI.h"
#include "SwatchItem.h"

HRESULT CSwatchItem::Create (CSIFRibbon* pRibbon, __deref_out CSwatchItem** ppItem)
{
	HRESULT hr;

	CheckIf(NULL == pRibbon, E_INVALIDARG);

	*ppItem = __new CSwatchItem(pRibbon);
	CheckAlloc(*ppItem);

	hr = S_OK;

Cleanup:
	return hr;
}

CSwatchItem::CSwatchItem (CSIFRibbon* pRibbon) :
	CBaseRibbonItem(pRibbon),
	m_pfnCreateSwatchImage(SquareSwatchImage),
	m_crSwatch(0)
{
}

CSwatchItem::~CSwatchItem ()
{
}

VOID CSwatchItem::SetSwatchColor (COLORREF crSwatch)
{
	m_crSwatch = crSwatch;
}

VOID CSwatchItem::SetSwatchFunction (HRESULT (WINAPI* pfnSwatch)(CSIFRibbon*, INT, COLORREF, PROPVARIANT*))
{
	m_pfnCreateSwatchImage = pfnSwatch;
}

// IUISimplePropertySet

HRESULT STDMETHODCALLTYPE CSwatchItem::GetValue (REFPROPERTYKEY key, PROPVARIANT* value)
{
	HRESULT hr = __super::GetValue(key, value);

	if(E_NOTIMPL == hr)
	{
		if(UI_PKEY_ItemImage == key || UI_PKEY_SmallImage == key)
			hr = m_pfnCreateSwatchImage(m_pRibbon, (INT)DPI::Scale(16.0f), m_crSwatch, value);
		else if(UI_PKEY_LargeImage == key)
			hr = m_pfnCreateSwatchImage(m_pRibbon, (INT)DPI::Scale(32.0f), m_crSwatch, value);
	}

	return hr;
}

HRESULT CSwatchItem::SquareSwatchImage (CSIFRibbon* pRibbon, INT nSize, COLORREF cr, __out PROPVARIANT* pValue)
{
	HRESULT hr;
	BITMAPINFO bmInfo = {0};
	HBITMAP hDIB;
	PBYTE pbDIB;
	IUIImage* pImage;

	BYTE bBlue = GetBValue(cr);
	BYTE bGreen = GetGValue(cr);
	BYTE bRed = GetRValue(cr);

	bmInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmInfo.bmiHeader.biBitCount = 32;
	bmInfo.bmiHeader.biWidth = nSize;
	bmInfo.bmiHeader.biHeight = nSize;
	bmInfo.bmiHeader.biCompression = BI_RGB;
	bmInfo.bmiHeader.biPlanes = 1;

	hDIB = CreateDIBSection(NULL, &bmInfo, DIB_RGB_COLORS, (PVOID*)&pbDIB, NULL, 0);
	CheckIfGetLastError(NULL == hDIB);
	Check(pRibbon->CreateImageFromDIB(hDIB, &pImage));
	pValue->punkVal = pImage;
	pValue->vt = VT_UNKNOWN;
	hDIB = NULL;

	for(INT y = 0; y < nSize; y++)
	{
		for(INT x = 0; x < nSize; x++)
		{
			pbDIB[0] = bBlue;
			pbDIB[1] = bGreen;
			pbDIB[2] = bRed;
			pbDIB[3] = 255;	// Alpha
			pbDIB += 4;
		}
	}

Cleanup:
	SafeDeleteGdiObject(hDIB);
	return hr;
}

HRESULT WINAPI CSwatchItem::CircleSwatchImage (CSIFRibbon* pRibbon, INT nSize, COLORREF cr, __out PROPVARIANT* pValue)
{
	HRESULT hr;
	BITMAPINFO bmInfo = {0};
	HBITMAP hDIB;
	PBYTE pbDIB;
	IUIImage* pImage;
	INT nOffset = nSize >> 1;
	DOUBLE rRadius = (DOUBLE)nOffset - 0.5;

	BYTE bBlue = GetBValue(cr);
	BYTE bGreen = GetGValue(cr);
	BYTE bRed = GetRValue(cr);

	bmInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmInfo.bmiHeader.biBitCount = 32;
	bmInfo.bmiHeader.biWidth = nSize;
	bmInfo.bmiHeader.biHeight = nSize;
	bmInfo.bmiHeader.biCompression = BI_RGB;
	bmInfo.bmiHeader.biPlanes = 1;

	hDIB = CreateDIBSection(NULL, &bmInfo, DIB_RGB_COLORS, (PVOID*)&pbDIB, NULL, 0);
	CheckIfGetLastError(NULL == hDIB);
	Check(pRibbon->CreateImageFromDIB(hDIB, &pImage));
	pValue->punkVal = pImage;
	pValue->vt = VT_UNKNOWN;
	hDIB = NULL;

	for(INT y = 0; y < nSize; y++)
	{
		DOUBLE yCoord = (DOUBLE)(y - nOffset);
		for(INT x = 0; x < nSize; x++)
		{
			DOUBLE xCoord = (DOUBLE)(x - nOffset);

			DOUBLE rDistance = sqrt(pow(xCoord, 2.0) + pow(yCoord, 2.0));
			if(rDistance <= rRadius)
			{
				pbDIB[0] = bBlue;
				pbDIB[1] = bGreen;
				pbDIB[2] = bRed;
				pbDIB[3] = 255;	// Alpha
			}
			else
				ZeroMemory(pbDIB, sizeof(DWORD));
			pbDIB += 4;
		}
	}

Cleanup:
	SafeDeleteGdiObject(hDIB);
	return hr;
}
