#pragma once

#include "uiRibbon.h"
#include "..\..\..\Shared\Library\Core\BaseUnknown.h"
#include "..\..\..\Platform\SIF\Published\SIF.h"

interface __declspec(uuid("F47EE11A-E8DC-4e92-82C9-53E7E7E008BE")) IRibbonHost : public IUIApplication
{
	virtual HRESULT WINAPI GetRibbonSettingsKey (__out_ecount(cchMaxKey) PSTR pszKeyName, INT cchMaxKey) = 0;
	virtual HRESULT WINAPI GetRibbonSettingsValue (__out_ecount(cchMaxValue) PSTR pszValueName, INT cchMaxValue) = 0;
	virtual HRESULT WINAPI GetRibbonResource (__out HMODULE* phModule, __out_ecount(cchMaxResource) PWSTR pwzResource, INT cchMaxResource) = 0;
	virtual UINT32 WINAPI TranslateGroupToImage (UINT32 nID) = 0;
	virtual UINT32 WINAPI TranslateImageToLargeImage (UINT32 nID) = 0;
};

class CSIFRibbon :
	public CBaseUnknown,
	public IUIApplication
{
protected:
	FLOAT (*m_pfnScale)(FLOAT);
	HWND m_hwndParent;
	IRibbonHost* m_pHost;

	IUIImageFromBitmap* m_pImageFactory;
	IUIFramework* m_pFramework;
	UINT32 m_nModes;
	UINT32 m_nRibbonHeight;

	ISimbeyInterchangeFile* m_pSmallImages;
	ISimbeyInterchangeFile* m_pLargeImages;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IUIApplication)
	END_UNK_MAP

	static HRESULT Create (FLOAT (*pfnScale)(FLOAT), __deref_out CSIFRibbon** ppRibbon);

	HRESULT Initialize (HWND hwnd, __in IRibbonHost* pHost);
	HRESULT SetModes (UINT32 nModes);
	VOID Unload (VOID);

	UINT32 GetHeight (VOID);
	HRESULT InvalidateEnabled (VOID);
	HRESULT InvalidateEnabled (UINT32 commandId);
	HRESULT UpdateProperty (UINT32 commandId, const PROPERTYKEY* key);
	HRESULT SaveSettings (VOID);

	HRESULT LoadImageForCommand (UINT32 commandId, REFPROPERTYKEY key, __out PROPVARIANT* pValue);
	HRESULT CreateImageFromDIB (HBITMAP hDIB, __deref_out IUIImage** ppImage);

protected:
	CSIFRibbon (FLOAT (*pfnScale)(FLOAT));
	~CSIFRibbon ();

	// IUIApplication
	virtual HRESULT STDMETHODCALLTYPE OnViewChanged (UINT32 nViewID, UI_VIEWTYPE typeID, IUnknown* pView, UI_VIEWVERB verb, INT32 uReasonCode);
	virtual HRESULT STDMETHODCALLTYPE OnCreateUICommand (UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler** ppCommandHandler);
	virtual HRESULT STDMETHODCALLTYPE OnDestroyUICommand (UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler* pCommandHandler);

	HRESULT _SaveSettings (VOID);
	HRESULT _LoadSettings (VOID);

	static HRESULT RegOpenStream (HKEY hKey, PCSTR pcszSubKey, PCSTR pcszValue, DWORD grfMode, __deref_out IStream** ppStream, __out HMODULE* phShellUtil);
};
