#pragma once

#include <SIFRibbon.h>
#include <StringGalleryItem.h>
#include <SwatchItem.h>
#include "Library\Core\MemoryStream.h"
#include "Library\Core\RStrMap.h"
#include "Library\Window\BaseWindow.h"
#include "Library\GraphCtrl.h"
#include "Published\SimbeyZip.h"
#include "PaintItems.h"
#include "ConfigDlg.h"
#include "LoaderDlg.h"

class CBlockMap;

struct PAINT_ITEM
{
	CPaintItem* pItem;

	enum Type
	{
		Wall,
		Object,
		Special
	} eType;
};

class CActorDef
{
public:
	CActorDef* m_pParent;
	RSTRING m_rstrParent;
	RSTRING m_rstrName;
	RSTRING m_rstrDef;
	INT m_idActor;
	bool m_fMonster;

public:
	CActorDef (RSTRING rstrName);
	~CActorDef ();
};

class CBlockMapEditorApp :
	public CBaseUnknown,
	public CBaseWindow,
	public IRibbonHost,
	public IUICommandHandler,
	public IGraphContainer,
	public IGraphClient,
	public ILoaderThread,
	public IResolveItemPalette
{
protected:
	HINSTANCE m_hInstance;

	RECT m_rcClient;
	CSIFRibbon* m_pRibbon;
	CGraphCtrl m_Graph;

	CConfigDlg m_dlgConfig;

	RSTRING m_rstrFile;
	CBlockMap* m_pBlockMap;
	BOOL m_fPainting;

	INT m_xInfo, m_zInfo;
	BOOL m_fInfoBalloon;
	UINT_PTR m_idInfoTimer;

	TRStrMap<TEXTURE*> m_mapTextures;
	TRStrMap<TEXTURE*> m_mapSprites;
	TRStrMap<TEXTURE*> m_mapExternal;
	TRStrMap<ACTOR*> m_mapActors;

	TArray<PAINT_ITEM> m_aPaintItems;
	CPaintItem* m_pWall;
	CPaintItem* m_pActor;
	CPaintItem* m_pSelection;

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
	CBlockMapEditorApp (HINSTANCE hInstance);
	virtual ~CBlockMapEditorApp ();

	static HRESULT Register (HINSTANCE hInstance);

	HRESULT Initialize (LPWSTR lpCmdLine, INT nCmdShow);

	HRESULT AddFloorItem (USHORT nFloor);
	VOID ClearPaintItems (VOID);

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

	// ILoaderThread
	virtual HRESULT LoaderCallback (HWND hwndStatus);

	// IResolveItemPalette
	virtual HRESULT InitializePaintItems (CBlockMap* pMap);
	virtual HRESULT ResolveItemPalette (MapCell::Type eType, const BYTE* pcb, DWORD cb, __deref_out CPaintItem** ppItem);

protected:
	VOID SizeWindow (VOID);
	VOID PaintWindow (HDC hdc, const RECT* prc);

	HRESULT CreateBlockMap (__deref_out CBlockMap** ppBlockMap);
	HRESULT SelectPaintItem (const PROPVARIANT* currentValue, UINT32 commandId, CPaintItem** ppItem);
	HRESULT RegisterPaintItem (CPaintItem* pItem, PAINT_ITEM::Type eType);
	HRESULT AddFakeSpriteTexture (RSTRING rstrName, __deref_out TEXTURE** ppTexture);

	HRESULT OpenMap (VOID);
	HRESULT SaveMap (VOID);
	HRESULT ShowProperties (VOID);
	HRESULT ExportMap (VOID);
	HRESULT LoadPackage (HWND hwndStatus, PCWSTR pcwzPackage);
	HRESULT LoadActors (PCWSTR pcwzText, PCWSTR pcwzDeco);
	HRESULT LoadExternal (PCWSTR pcwzSearchPath);
	HRESULT FindSpriteTexture (PCWSTR pcwzText, PCWSTR pcwzSprite, __deref_out TEXTURE** ppTexture);
	HRESULT AddDirectionalActor (RSTRING rstrName, INT nDirection, ACTOR* pActor, bool fUseThisActor);
	HRESULT LoadExternalTexture (RSTRING rstrName, __deref_out TEXTURE** ppTexture);

	static HRESULT LoadFile (ISimbeyUnzip* pPackage, PCWSTR pcwzFile, __out CMemoryStream* pstmFile);
	static HRESULT LoadTextEntry (ISimbeyUnzip* pPackage, PCWSTR pcwzName, PWSTR* ppwzText, INT* pcchText);
	static HRESULT LoadTextures (ISimbeyUnzip* pPackage, PCWSTR pcwzFolder, INT cchFolder, BOOL (WINAPI* pfnLoadTextures)(PCWSTR pcwzFile, INT cchFile, unz_file_info64* pufi, PVOID pvParam), TRStrMap<TEXTURE*>* pmapTextures);
	static HRESULT LoadTexture (TRStrMap<TEXTURE*>* pmapTextures, RSTRING rstrFileW, const BYTE* pcbPNG, DWORD cbPNG);

	HRESULT DrawInfoPart (HDC hdc, CPaintItem* pInfo, LONG x, LONG y);
	HRESULT SetCellData (FLOAT x, FLOAT z);

	HRESULT UpdateAppTitle (VOID);

	virtual HINSTANCE GetInstance (VOID);
	virtual VOID OnFinalDestroy (HWND hwnd);

	virtual HRESULT FinalizeAndShow (DWORD dwStyle, INT nCmdShow);
	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

	static BOOL WINAPI _LoadTextures (PCWSTR pcwzFile, INT cchFile, unz_file_info64* pufi, PVOID pvParam);
	static BOOL WINAPI _LoadSprites (PCWSTR pcwzFile, INT cchFile, unz_file_info64* pufi, PVOID pvParam);

	static BOOL ReadToken (__inout PCWSTR& pcwzTokens, __deref_out PCWSTR* ppcwzToken, INT* pcchToken);
};
