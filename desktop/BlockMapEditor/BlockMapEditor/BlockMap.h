#pragma once

#include "Library\Core\Map.h"
#include "Library\Core\MemoryStream.h"
#include "Library\GraphCtrl.h"

interface IResolveItemPalette;

class CPaintItem;

#define	MAP_WIDTH		256
#define	MAP_HEIGHT		256

#define	CELL_SCALE		16.0f

struct MAP_BLOCK
{
	CPaintItem* pBlock;
	CPaintItem* pObject;
	SHORT nObjectFlags;		// 1 - Easy, 2 - Medium, 4 - Hard
};

class CBlockMap
{
private:
	INT m_nLighting;
	USHORT m_nHighestFloor;
	MAP_BLOCK* m_pMap;

public:
	CBlockMap ();
	~CBlockMap ();

	HRESULT Initialize (VOID);
	INT GetLighting (VOID) { return m_nLighting; }
	VOID SetLighting (INT nLighting) { m_nLighting = nLighting; }
	VOID Paint (IGrapher* pGraph);
	HRESULT Load (IResolveItemPalette* pResolve, PCWSTR pcwzFile, PWSTR pwzCeiling, PWSTR pwzFloor);
	HRESULT Save (PCWSTR pcwzFile, PCWSTR pcwzCeiling, PCWSTR pcwzFloor);
	USHORT GetHighestFloor (VOID);
	USHORT AddFloor (VOID);
	HRESULT SetCellData (FLOAT x, FLOAT z, CPaintItem* pType);
	HRESULT GetCellData (INT xCell, INT zCell, __deref_out CPaintItem** ppBlock, CPaintItem** ppObject);
	HRESULT GetObjectFlags (INT xCell, INT zCell, __out SHORT* pnObjectFlags);
	HRESULT SetObjectFlags (INT xCell, INT zCell, SHORT nObjectFlags);
	BOOL IsSolid (INT xCell, INT zCell);
	USHORT GetFloor (INT xCell, INT zCell);
	HRESULT FindStartSpot (__out INT& xCell, __out INT& zCell);

private:
	HRESULT LoadItemPalette (IResolveItemPalette* pResolve, ISequentialStream* pPalette, DWORD cbPalette, TArray<CPaintItem*>& aPalette);

	static HRESULT InsertStream (ISequentialStream* pstmTarget, CMemoryStream* pstmSource);
	static HRESULT SerializeItem (TMap<CPaintItem*, DWORD>& mapPalette, TArray<CPaintItem*>& aPalette, CMemoryStream* pstmData, CPaintItem* pItem);
};
