#pragma once

#include <SIFRibbon.h>
#include <SIFRibbonItem.h>
#include "Library\Core\BaseUnknown.h"
#include "Library\Window\BaseWindow.h"

class CRibbonWindow :
	public CBaseUnknown,
	public CBaseWindow,
	public IRibbonHost,
	public IUICommandHandler
{
protected:
	HINSTANCE m_hInstance;

	CSIFRibbon* m_pRibbon;
	BOOL m_fActive;

	ISimbeyInterchangeFile* m_pGalleryImages;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IOleWindow)
		UNK_INTERFACE(IBaseWindow)
		UNK_INTERFACE(IRibbonHost)
		UNK_INTERFACE(IUIApplication)
		UNK_INTERFACE(IUICommandHandler)
	END_UNK_MAP

	BEGIN_WM_MAP
		HANDLE_WM(WM_CREATE, OnCreate)
		HANDLE_WM(WM_PAINT, OnPaint)
		HANDLE_WM(WM_ERASEBKGND, OnEraseBackground)
		HANDLE_WM(WM_SIZE, OnSize)
		HANDLE_WM(WM_CLOSE, OnClose)
		HANDLE_WM(WM_ACTIVATE, OnActivate)
		HANDLE_WM(WM_DESTROY, OnDestroy)
	END_WM_MAP

	CRibbonWindow (HINSTANCE hInstance);
	virtual ~CRibbonWindow ();

	static HRESULT Register (HINSTANCE hInstance);
	static HRESULT Unregister (HINSTANCE hInstance);

	HRESULT Initialize (INT nWidth, INT nHeight, INT nCmdShow);

	// IRibbonHost
	virtual HRESULT WINAPI GetRibbonSettingsKey (__out_ecount(cchMaxKey) PSTR pszKeyName, INT cchMaxKey);
	virtual HRESULT WINAPI GetRibbonSettingsValue (__out_ecount(cchMaxValue) PSTR pszValueName, INT cchMaxValue);
	virtual HRESULT WINAPI GetRibbonResource (__out HMODULE* phModule, __out_ecount(cchMaxResource) PWSTR pwzResource, INT cchMaxResource);
	virtual UINT32 WINAPI TranslateGroupToImage (UINT32 nID);
	virtual UINT32 WINAPI TranslateImageToLargeImage (UINT32 nID);

	// IUIApplication
	virtual HRESULT STDMETHODCALLTYPE OnViewChanged (UINT32 viewId, UI_VIEWTYPE typeID, IUnknown* view, UI_VIEWVERB verb, INT32 uReasonCode);
	virtual HRESULT STDMETHODCALLTYPE OnCreateUICommand (UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler** commandHandler);
	virtual HRESULT STDMETHODCALLTYPE OnDestroyUICommand (UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler* commandHandler);

	// IUICommandHandler
	virtual HRESULT STDMETHODCALLTYPE Execute (UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* currentValue, IUISimplePropertySet* commandExecutionProperties);
	virtual HRESULT STDMETHODCALLTYPE UpdateProperty (UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT* currentValue, PROPVARIANT* newValue);

protected:
	virtual HINSTANCE GetInstance (VOID);
	virtual VOID OnFinalDestroy (HWND hwnd);

	virtual HRESULT FinalizeAndShow (DWORD dwStyle, INT nCmdShow);

	DECL_WM_HANDLER(OnCreate);
	DECL_WM_HANDLER(OnPaint);
	DECL_WM_HANDLER(OnEraseBackground);
	DECL_WM_HANDLER(OnSize);
	DECL_WM_HANDLER(OnClose);
	DECL_WM_HANDLER(OnActivate);
	DECL_WM_HANDLER(OnDestroy);
};
