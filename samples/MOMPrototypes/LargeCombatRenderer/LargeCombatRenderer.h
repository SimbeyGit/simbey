#pragma once

#include "Library\Core\BaseUnknown.h"
#include "Library\Core\RStrMap.h"
#include "Library\Window\BaseWindow.h"
#include "Surface\SIFSurface.h"
#include "Package\SIFPackage.h"
#include "..\Shared\TileRules.h"
#include "..\Shared\TileSet.h"

namespace MapFeature
{
	enum Type
	{
		None,
		Decoration,
		Building
	};
}

struct MAPTILE
{
	CTile* pTile;

	MapFeature::Type eType;
	CTile* pFeature;
};

class CCombatTiles : public ITileMap
{
private:
	ULONG m_cRef;

public:
	MAPTILE* m_pWorld;

public:
	CCombatTiles ();
	~CCombatTiles ();

	HRESULT Initialize (VOID);

	// IUnknown
	IFACEMETHODIMP QueryInterface (REFIID riid, __deref_out PVOID* ppvObj) { return E_NOTIMPL; }
	IFACEMETHODIMP_(ULONG) AddRef ();
	IFACEMETHODIMP_(ULONG) Release ();

	// ITileMap
	virtual VOID GetSize (__out INT* pxTiles, __out INT* pyTiles);
	virtual CTile* GetTile (INT idxTile);
	virtual VOID SetTile (INT idxTile, CTile* pTile);
};

class CLargeCombatRenderer :
	public CBaseUnknown,
	public CBaseWindow
{
protected:
	HINSTANCE m_hInstance;

	CSIFSurface m_Surface;
	CSIFCanvas* m_pMain;
	CIsometricTranslator m_Isometric;

	BOOL m_fActive;

	bool m_fKeys[256];

	CSIFPackage* m_pPackage;

	TRStrMap<CSmoothingSystem*> m_mapSmoothingSystems;
	CTileRules* m_pTileRules;
	IJSONArray* m_pGenerators;
	TRStrMap<CTileSet*> m_mapCombatTiles;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IOleWindow)
		UNK_INTERFACE(IBaseWindow)
	END_UNK_MAP

	BEGIN_WM_MAP
		HANDLE_WM(WM_CREATE, OnCreate)
		HANDLE_WM(WM_PAINT, OnPaint)
		HANDLE_WM(WM_ERASEBKGND, OnEraseBackground)
		HANDLE_WM(WM_SIZE, OnSize)
		HANDLE_WM(WM_CLOSE, OnClose)
		HANDLE_WM(WM_ACTIVATE, OnActivate)
		HANDLE_WM(WM_MOUSEMOVE, OnMouseMove)
		HANDLE_WM(WM_LBUTTONDOWN, OnLButtonDown)
		HANDLE_WM(WM_LBUTTONUP, OnLButtonUp)
		HANDLE_WM(WM_RBUTTONDOWN, OnRButtonDown)
		HANDLE_WM(WM_KEYDOWN, OnKeyDown)
		HANDLE_WM(WM_KEYUP, OnKeyUp)
		HANDLE_WM(WM_SETCURSOR, OnSetCursor)
	END_WM_MAP

	CLargeCombatRenderer (HINSTANCE hInstance);
	virtual ~CLargeCombatRenderer ();

	static HRESULT Register (HINSTANCE hInstance);
	static HRESULT Unregister (HINSTANCE hInstance);

	HRESULT Initialize (INT nWidth, INT nHeight, PCWSTR pcwzCmdLine, INT nCmdShow);

	VOID Run (VOID);

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
	DECL_WM_HANDLER(OnMouseMove);
	DECL_WM_HANDLER(OnLButtonDown);
	DECL_WM_HANDLER(OnLButtonUp);
	DECL_WM_HANDLER(OnRButtonDown);
	DECL_WM_HANDLER(OnKeyDown);
	DECL_WM_HANDLER(OnKeyUp);
	DECL_WM_HANDLER(OnSetCursor);

	HRESULT PlaceTile (CSIFCanvas* pCanvas, INT xTile, INT yTile, sysint nLayer, CTile* pTile, __deref_out_opt ISimbeyInterchangeSprite** ppSprite);
	HRESULT FindBuildingBottom (ISimbeyInterchangeFileLayer* pLayer, __out POINT* ppt);
	VOID PlaceBuilding (CCombatTiles* pTiles, INT x, INT y, CTile* pBuilding);

	HRESULT AllocateCombatWorld (__deref_out CCombatTiles** ppTiles);
	HRESULT GenerateCombatWorld (CCombatTiles* pTiles, IJSONObject* pGenerator, DWORD dwSeed);

	HRESULT GenerateMap (DWORD dwSeed, RSTRING rstrWorld, RSTRING rstrGenerator, RSTRING rstrHouseSprites);
};
