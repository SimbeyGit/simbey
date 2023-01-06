#pragma once

#include "Library\Core\Array.h"

struct COORD_SYSTEM
{
	INT nWidth;
	INT nHeight;
	BOOL fWrapsLeftToRight;
	BOOL fWrapsTopToBottom;
};

struct HEIGHT_CELL
{
	INT nValue;
	BOOL fSet;
};

interface IRandomNumber
{
	virtual INT Next (VOID) = 0;
	virtual INT Next (INT nMax) = 0;
};

class CHeightMapGenerator
{
private:
	COORD_SYSTEM m_coords;
	IRandomNumber* m_pRand;

	// Width of each "zone" - small values produce more random jagged landscape, larger zones produce more gradual height changes
	INT m_nZoneWidth;

	// Height of each "zone" - small values produce more random jagged landscape, larger zones produce more gradual height changes
	INT m_nZoneHeight;

	// Number of rows from non-wrapping edges were we may place tundra
	INT m_cRowsFromMapEdgeWhereTundraCanAppear;

	// Generated height map
	HEIGHT_CELL* m_pHeightMap;

	// Height map with minimum value on each plane adjusted to zero
	HEIGHT_CELL* m_pZeroBasedHeightMap;

	TArray<INT> m_aHeightCounts;

public:
	CHeightMapGenerator (IRandomNumber* pRand, INT nZoneWidth, INT nZoneHeight, INT cRowsFromMapEdgeWhereTundraCanAppear);
	~CHeightMapGenerator ();

	HRESULT Initialize (const COORD_SYSTEM& coords);

	HRESULT GenerateHeightMap (VOID);
	BOOL NearSingularity (INT x, INT y, INT nDist);
	VOID SetMidPoints (INT x, INT y, INT nValue);
	VOID FractalLandscapeIteration (INT nStep, INT x0, INT y0, INT x1, INT y1);
	VOID GenerateFractalLandscape (VOID);
	VOID GenerateZeroBasedHeightMap (VOID);
	HRESULT CountTilesAtEachHeight (VOID);
	INT CountTilesAboveThreshold (INT nThreshold);
	INT CountTilesBelowThreshold (INT nThreshold);
	HRESULT SetHighestTiles (INT nDesiredTileCount, TArray<POINT>* paTiles);
	HRESULT SetLowestTiles (INT nDesiredTileCount, TArray<POINT>* paTiles);

	VOID GetCoordinateSystem (__out COORD_SYSTEM* pCoords);
	HEIGHT_CELL* GetHeightMap (VOID) { return m_pHeightMap; }
	HEIGHT_CELL* GetZeroBasedHeightMap (VOID) { return m_pZeroBasedHeightMap; }
};
