#include <math.h>
#include <windows.h>
#include "HeightMapGenerator.h"

inline DOUBLE Round (DOUBLE dbl)
{
	return dbl < 0.0 ? ceil(dbl - 0.5) : floor(dbl + 0.5);
}

CHeightMapGenerator::CHeightMapGenerator (IRandomNumber* pRand, INT nZoneWidth, INT nZoneHeight, INT cRowsFromMapEdgeWhereTundraCanAppear) :
	m_pRand(pRand),
	m_nZoneWidth(nZoneWidth),
	m_nZoneHeight(nZoneHeight),
	m_cRowsFromMapEdgeWhereTundraCanAppear(cRowsFromMapEdgeWhereTundraCanAppear),
	m_pHeightMap(NULL),
	m_pZeroBasedHeightMap(NULL)
{
}

CHeightMapGenerator::~CHeightMapGenerator ()
{
	__delete_array m_pHeightMap;
	__delete_array m_pZeroBasedHeightMap;
}

HRESULT CHeightMapGenerator::Initialize (const COORD_SYSTEM& coords)
{
	HRESULT hr;

	CheckIf(NULL != m_pHeightMap || NULL != m_pZeroBasedHeightMap, E_UNEXPECTED);

	INT cCells = coords.nWidth * coords.nHeight;

	m_pHeightMap = __new HEIGHT_CELL[cCells];
	CheckAlloc(m_pHeightMap);

	m_pZeroBasedHeightMap = __new HEIGHT_CELL[cCells];
	CheckAlloc(m_pZeroBasedHeightMap);

	m_coords = coords;
	hr = S_OK;

Cleanup:
	return hr;
}

HRESULT CHeightMapGenerator::GenerateHeightMap (VOID)
{
	Assert(m_pHeightMap && m_pZeroBasedHeightMap);

	INT cCells = m_coords.nWidth * m_coords.nHeight;
	ZeroMemory(m_pHeightMap, sizeof(HEIGHT_CELL) * cCells);
	ZeroMemory(m_pZeroBasedHeightMap, sizeof(HEIGHT_CELL) * cCells);

	m_aHeightCounts.Clear();

	// Generate height-based scenery
	GenerateFractalLandscape();
	GenerateZeroBasedHeightMap();
	return CountTilesAtEachHeight();
}

BOOL CHeightMapGenerator::NearSingularity (INT x, INT y, INT nDist)
{
	return ((!m_coords.fWrapsLeftToRight && (x < nDist || x >= m_coords.nWidth - nDist)) ||
		(!m_coords.fWrapsTopToBottom && (y < nDist || y >= m_coords.nHeight - nDist)));
}

VOID CHeightMapGenerator::SetMidPoints (INT x, INT y, INT nValue)
{
	HEIGHT_CELL* pCell = m_pHeightMap + (y * m_coords.nWidth) + x;
	if(!pCell->fSet)
	{
		pCell->nValue = nValue;
		pCell->fSet = TRUE;
	}
}

VOID CHeightMapGenerator::FractalLandscapeIteration (INT nStep, INT x0, INT y0, INT x1, INT y1)
{
	// All x and y values are native
	if(((y1 - y0 <= 0) || (x1 - x0 <= 0)) || ((y1 - y0 == 1) && (x1 - x0 == 1)))
	{
		// Do nothing
	}
	else
	{
		// To wrap correctly
		INT x1wrap;
		if(x1 == m_coords.nWidth)
			x1wrap = 0;
		else
			x1wrap = x1;

		INT y1wrap;
		if(y1 == m_coords.nHeight)
			y1wrap = 0;
		else
			y1wrap = y1;

		// Read the current corner values from the height map
		// These should always be reading existing values so should never be null
		INT val00 = m_pHeightMap[y0 * m_coords.nWidth + x0].nValue;
		INT val01 = m_pHeightMap[y1wrap * m_coords.nWidth + x0].nValue;
		INT val10 = m_pHeightMap[y0 * m_coords.nWidth + x1wrap].nValue;
		INT val11 = m_pHeightMap[y1wrap * m_coords.nWidth + x1wrap].nValue;

		// Find mid points
		INT xmid = (x0 + x1) / 2;
		INT ymid = (y0 + y1) / 2;

		// Set midpoints of sides to avg of side's vertices plus a random factor
		// Unset points are null, don't reset if set
		SetMidPoints(xmid,		y0,		((val00 + val10) / 2) + m_pRand->Next(nStep) - (nStep / 2));
		SetMidPoints(xmid,		y1wrap,	((val01 + val11) / 2) + m_pRand->Next(nStep) - (nStep / 2));
		SetMidPoints(x0,		ymid,	((val00 + val01) / 2) + m_pRand->Next(nStep) - (nStep / 2));
		SetMidPoints(x1wrap,	ymid,	((val10 + val11) / 2) + m_pRand->Next(nStep) - (nStep / 2));

		// Set middle to average of midpoints plus a random factor, if not set
		SetMidPoints(xmid, ymid, ((val00 + val01 + val10 + val11) / 4) + m_pRand->Next(nStep) - (nStep / 2));

		// Now call recursively on the four subrectangles
		INT newStep = (2 * nStep) / 3;
		FractalLandscapeIteration(newStep, x0,		y0,		xmid,	ymid);
		FractalLandscapeIteration(newStep, x0,		ymid,	xmid,	y1);
		FractalLandscapeIteration(newStep, xmid,	y0,		x1,		ymid);
		FractalLandscapeIteration(newStep, xmid,	ymid,	x1,		y1);
	}
}

VOID CHeightMapGenerator::GenerateFractalLandscape (VOID)
{
	// Session description contains zone width/height - we need the number of zones
	INT xBlocks = (INT)Round((DOUBLE)m_coords.nWidth / (DOUBLE)m_nZoneWidth);		// a.k.a. zonesHorizontally
	INT yBlocks = (INT)Round((DOUBLE)m_coords.nHeight / (DOUBLE)m_nZoneHeight);		// a.k.a. zonesVertically

	// If the edge wraps, the last block ends with the first point, so there's the same number of blocks and dividing points
	// If the edge doesn't wrap, we need a point on the right/bottom of it as well to end the last block
	INT xPoints;
	if(m_coords.fWrapsLeftToRight)
		xPoints = xBlocks;
	else
		xPoints = xBlocks + 1;

	INT yPoints;
	if(m_coords.fWrapsTopToBottom)
		yPoints = yBlocks;
	else
		yPoints = yBlocks + 1;

	// If the edge wraps, the last pretend block will be the first pixel off the edge of the map.
	// If the edge doesn't wrap, the last block is the actual last pixel
	INT xMax;
	if(m_coords.fWrapsLeftToRight)
		xMax = m_coords.nWidth;
	else
		xMax = m_coords.nWidth - 1;

	INT yMax;
	if(m_coords.fWrapsTopToBottom)
		yMax = m_coords.nHeight;
	else
		yMax = m_coords.nHeight - 1;

	// Just need something > log(max(xsize, ysize)) for the recursion???
	INT step = m_coords.nWidth + m_coords.nHeight;

	// Edges are avoided more strongly as this increases
	INT avoidEdge = step / 2;

	// Now generate a plasma fractal to use as the height map
	// Set initial points
	for(INT xn = 0; xn < xPoints; xn++)
	{
		for(INT yn = 0; yn < yPoints; yn++)
		{
			const INT x = (xn * xMax) / xBlocks;
			const INT y = (yn * yMax) / yBlocks;

			// Randomize initial point
			INT thisHeight = m_pRand->Next(2 * step) - step;

			// Avoid edges (topological singularities)
			if(NearSingularity(x, y, 7))		// cannot find actual value for CITY_MAP_RADIUS
				thisHeight = thisHeight - avoidEdge;

			// Separate poles and avoid too much land at poles
			// This is basically repeating the above... but it's how the MoM IME map generator has been since 0.1 and it gives nice results so I kept it as is
			if(NearSingularity(x, y, m_cRowsFromMapEdgeWhereTundraCanAppear))
				thisHeight = thisHeight - avoidEdge;

			// Set it
			HEIGHT_CELL* pCell = m_pHeightMap + y * m_coords.nWidth + x;
			pCell->nValue = thisHeight;
			pCell->fSet = TRUE;
		}
	}

	// Calculate recursively on each block
	for(INT xn = 0; xn < xBlocks; xn++)
	{
		for(INT yn = 0; yn < yBlocks; yn++)
			FractalLandscapeIteration(step, (xn * xMax) / xBlocks, (yn * yMax) / yBlocks, ((xn + 1) * xMax) / xBlocks, ((yn + 1) * yMax) / yBlocks);
	}	
}

VOID CHeightMapGenerator::GenerateZeroBasedHeightMap (VOID)
{
	// Find the minimum height
	INT minimumHeight = m_pHeightMap->nValue;

	for(INT y = 0; y < m_coords.nHeight; y++)
	{
		INT idxRow = y * m_coords.nWidth;
		for(INT x = 0; x < m_coords.nWidth; x++)
		{
			INT thisHeight = m_pHeightMap[idxRow + x].nValue;
			if(thisHeight < minimumHeight)
				minimumHeight = thisHeight;
		}
	}

	// Copy + adjust the height map
	for(INT y = 0; y < m_coords.nHeight; y++)
	{
		INT idxRow = y * m_coords.nWidth;
		for(INT x = 0; x < m_coords.nWidth; x++)
		{
			INT nCell = idxRow + x;
			m_pZeroBasedHeightMap[nCell].nValue = m_pHeightMap[nCell].nValue - minimumHeight;
			m_pZeroBasedHeightMap[nCell].fSet = TRUE;
		}
	}
}

HRESULT CHeightMapGenerator::CountTilesAtEachHeight (VOID)
{
	HRESULT hr = S_FALSE;
	INT cCells = m_coords.nWidth * m_coords.nHeight;

	// This relies on the fact that there are no entries in the height map with negative values
	// so by starting at zero and working up, we're guaranteed to evenually hit every tile on the map
	INT tilesDone = 0;
	while(tilesDone < cCells)
	{
		// Count tiles with this height
		INT count = 0;

		for(INT y = 0; y < m_coords.nHeight; y++)
		{
			INT idxRow = y * m_coords.nWidth;

			for(INT x = 0; x < m_coords.nWidth; x++)
			{
				if(m_pZeroBasedHeightMap[idxRow + x].nValue == m_aHeightCounts.Length())
					count++;
			}
		}

		// Add to the list
		Check(m_aHeightCounts.Append(count));
		tilesDone += count;
	}

Cleanup:
	return hr;
}

INT CHeightMapGenerator::CountTilesAboveThreshold (INT nThreshold)
{
	INT count = 0;
	for(sysint index = nThreshold; index < m_aHeightCounts.Length(); index++)
		count = count + m_aHeightCounts[index];
	return count;
}

INT CHeightMapGenerator::CountTilesBelowThreshold (INT nThreshold)
{
	INT count = 0;
	for(INT index = 0; index <= nThreshold; index++)
		count = count + m_aHeightCounts[index];
	return count;
}

HRESULT CHeightMapGenerator::SetHighestTiles (INT nDesiredTileCount, TArray<POINT>* paTiles)
{
	HRESULT hr = S_FALSE;
	POINT pt;

	// Check zero first
	INT bestThreshold = 0;
	INT bestValue = abs(CountTilesAboveThreshold(bestThreshold) - nDesiredTileCount);

	// Then try all others
	for(sysint thisThreshold = 1; thisThreshold < m_aHeightCounts.Length(); thisThreshold++)
	{
		INT thisValue = abs(CountTilesAboveThreshold(thisThreshold) - nDesiredTileCount);
		if(thisValue < bestValue)
		{
			bestThreshold = thisThreshold;
			bestValue = thisValue;
		}
	}

	// Then set scenery above this threshold
	for(pt.y = 0; pt.y < m_coords.nHeight; pt.y++)
	{
		INT idxRow = pt.y * m_coords.nWidth;

		for(pt.x = 0; pt.x < m_coords.nWidth; pt.x++)
		{
			if(m_pZeroBasedHeightMap[idxRow + pt.x].nValue >= bestThreshold)
				Check(paTiles->Append(pt));
		}
	}

Cleanup:
	return hr;
}

HRESULT CHeightMapGenerator::SetLowestTiles (INT nDesiredTileCount, TArray<POINT>* paTiles)
{
	HRESULT hr = S_FALSE;
	POINT pt;

	// Check zero first
	INT bestThreshold = 0;
	INT bestValue = abs(CountTilesBelowThreshold(bestThreshold) - nDesiredTileCount);

	// Then try all others
	for(sysint thisThreshold = 1; thisThreshold < m_aHeightCounts.Length(); thisThreshold++)
	{
		INT thisValue = abs(CountTilesBelowThreshold(thisThreshold) - nDesiredTileCount);
		if(thisValue < bestValue)
		{
			bestThreshold = thisThreshold;
			bestValue = thisValue;
		}
	}

	// Then set scenery below this threshold
	for(pt.y = 0; pt.y < m_coords.nHeight; pt.y++)
	{
		INT idxRow = pt.y * m_coords.nWidth;

		for(pt.x = 0; pt.x < m_coords.nWidth; pt.x++)
		{
			if(m_pZeroBasedHeightMap[idxRow + pt.x].nValue <= bestThreshold)
				Check(paTiles->Append(pt));
		}
	}

Cleanup:
	return hr;
}

VOID CHeightMapGenerator::GetCoordinateSystem (__out COORD_SYSTEM* pCoords)
{
	CopyMemory(pCoords, &m_coords, sizeof(COORD_SYSTEM));
}
