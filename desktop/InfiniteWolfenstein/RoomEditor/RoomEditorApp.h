#pragma once

#include <SIFRibbon.h>
#include <StringGalleryItem.h>
#include <SwatchItem.h>
#include "Library\Core\RStrMap.h"
#include "Library\Window\BaseWindow.h"
#include "Library\GraphCtrl.h"

interface IJSONArray;
interface IJSONObject;

class CSIFPackage;
class CRooms;

struct PACKAGE_DATA
{
	CSIFPackage* pPackage;

	ISimbeyInterchangeFile* psifWalls;

	IJSONArray* pModels;
	ISimbeyInterchangeFile* psifEntities;
};

class CRoomEditorApp :
	public CBaseUnknown,
	public CBaseWindow,
	public IRibbonHost,
	public IUICommandHandler,
	public IGraphContainer,
	public IGraphClient
{
protected:
	HINSTANCE m_hInstance;

	RECT m_rcClient;
	CSIFRibbon* m_pRibbon;
	CGraphCtrl m_Graph;

	TArray<CBaseRibbonItem*> m_aTypes;
	sysint m_cDefaultTypes;
	sysint m_idxSelected;

	RSTRING m_rstrFile;
	CRooms* m_pRooms;
	BOOL m_fPainting;

	TRStrMap<PACKAGE_DATA> m_mapPackages;

	INT m_xInfo, m_zInfo;
	BOOL m_fInfoBalloon;
	UINT_PTR m_idInfoTimer;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IOleWindow)
		UNK_INTERFACE(IBaseWindow)
		UNK_INTERFACE(IRibbonHost)
		UNK_INTERFACE(IUIApplication)
		UNK_INTERFACE(IUICommandHandler)
	END_UNK_MAP

public:
	CRoomEditorApp (HINSTANCE hInstance);
	virtual ~CRoomEditorApp ();

	static HRESULT Register (HINSTANCE hInstance);

	HRESULT Initialize (LPWSTR lpCmdLine, INT nCmdShow);

	// IUIApplication
	virtual HRESULT STDMETHODCALLTYPE OnViewChanged (UINT32 viewId, UI_VIEWTYPE typeID, IUnknown* view, UI_VIEWVERB verb, INT32 uReasonCode);
	virtual HRESULT STDMETHODCALLTYPE OnCreateUICommand (UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler** commandHandler);
	virtual HRESULT STDMETHODCALLTYPE OnDestroyUICommand (UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler* commandHandler);

	// IRibbonHost
	virtual HRESULT WINAPI GetRibbonSettingsKey (__out_ecount(cchMaxKey) PSTR pszKeyName, INT cchMaxKey);
	virtual HRESULT WINAPI GetRibbonSettingsValue (__out_ecount(cchMaxValue) PSTR pszValueName, INT cchMaxValue);
	virtual HRESULT WINAPI GetRibbonResource (__out HMODULE* phModule, __out_ecount(cchMaxResource) PWSTR pwzResource, INT cchMaxResource);
	virtual UINT WINAPI TranslateGroupToImage (UINT nID);
	virtual UINT WINAPI TranslateImageToLargeImage (UINT nID);

	// IUICommandHandler
	virtual HRESULT STDMETHODCALLTYPE Execute (UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* currentValue, IUISimplePropertySet* commandExecutionProperties);
	virtual HRESULT STDMETHODCALLTYPE UpdateProperty (UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT* currentValue, PROPVARIANT* newValue);

	// IGraphContainer
	virtual VOID WINAPI OnScaleChanged (FLOAT fScale) {}
	virtual VOID WINAPI OnGridSpacingChanged (INT iSpacing) {}
	virtual HRESULT WINAPI SetFocus (__in IGrapher* pGraphCtrl);
	virtual HRESULT WINAPI ScreenToWindowless (__in IGrapher* pGraphCtrl, __inout PPOINT ppt);
	virtual HRESULT WINAPI InvalidateContainer (__in IGrapher* pGraphCtrl);
	virtual VOID WINAPI DrawDib24 (HDC hdcDest, LONG x, LONG y, HDC hdcSrc, const BYTE* pcDIB24, LONG xSize, LONG ySize, LONG lPitch);
	virtual BOOL WINAPI CaptureMouse (__in IGrapher* pGraphCtrl, BOOL fCapture);

	// IGraphClient
	virtual VOID onGraphPaint (IGrapher* lpGraph);
	virtual VOID onGraphMouseMove (DWORD dwKeys, FLOAT x, FLOAT y);
	virtual VOID onGraphLBtnDown (DWORD dwKeys, FLOAT x, FLOAT y);
	virtual VOID onGraphLBtnUp (DWORD dwKeys, FLOAT x, FLOAT y);
	virtual BOOL onGraphRBtnDown (DWORD dwKeys, FLOAT x, FLOAT y);
	virtual VOID onGraphRBtnUp (DWORD dwKeys, FLOAT x, FLOAT y);
	virtual VOID onGraphLBtnDbl (DWORD dwKeys, FLOAT x, FLOAT y);
	virtual VOID onGraphRBtnDbl (DWORD dwKeys, FLOAT x, FLOAT y);
	virtual VOID onGraphViewChanged (BOOL fZoomChanged);
	virtual BOOL onGraphKeyDown (WPARAM iKey);
	virtual BOOL onGraphKeyUp (WPARAM iKey);
	virtual BOOL onGraphChar (WPARAM iKey);
	virtual BOOL onGraphWheel (SHORT sDistance, FLOAT x, FLOAT y);
	virtual HRESULT onGraphGetAcc (IAccessible** lplpAccessible);

protected:
	VOID SizeWindow (VOID);
	VOID PaintWindow (HDC hdc, const RECT* prc);

	HRESULT CreateRooms (__deref_out CRooms** ppRooms);
	HRESULT RenameRoom (VOID);
	HRESULT OpenRooms (VOID);
	HRESULT SaveRooms (VOID);
	HRESULT CloneRoom (VOID);

	HRESULT CreateSwatchItem (PCWSTR pcwzLabel, INT cchLabel, PCWSTR pcwzType, INT cchType, COLORREF crSwatch, BOOL fSquare = TRUE);
	HRESULT CreateIconItem (PCWSTR pcwzLabel, INT cchLabel, PCWSTR pcwzType, INT cchType, UINT nIcon);

	HRESULT FillPaintTypes (const PROPVARIANT* pVar);
	HRESULT UpdatePaintType (VOID);
	HRESULT SelectPaintType (ULONG idxType);
	HRESULT SetCellData (FLOAT x, FLOAT z);
	HRESULT ShowPickWallDialog (__out ULONG* pidxType);
	HRESULT ShowPickEntityDialog (__out ULONG* pidxType);

	HRESULT LoadPackage (PCWSTR pcwzPackage, BOOL fRequireAll);
	HRESULT DrawInfoPart (HDC hdc, IJSONObject* pInfo, LONG x, LONG y, BOOL fEntity);

	static HRESULT LoadPackageWalls (CSIFPackage* pPackage, __deref_out ISimbeyInterchangeFile** ppSIF, __out RSTRING* prstrNamespace);
	static HRESULT LoadPackageEntities (CSIFPackage* pPackage, __deref_out IJSONArray** ppModels, __deref_out ISimbeyInterchangeFile** ppSIF, __out RSTRING* prstrNamespace);
	static VOID FreePackage (PACKAGE_DATA& data);

	static HRESULT CalculateAveragePixelColor (ISimbeyInterchangeFileLayer* pLayer, __out COLORREF* pcr);

	HRESULT UpdateAppTitle (VOID);

	virtual HINSTANCE GetInstance (VOID);
	virtual VOID OnFinalDestroy (HWND hwnd);

	virtual HRESULT FinalizeAndShow (DWORD dwStyle, INT nCmdShow);
	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);
};
