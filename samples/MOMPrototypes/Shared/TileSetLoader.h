#pragma once

#include "TileSet.h"
#include "Package\SIFPackage.h"

namespace TileSetLoader
{
	HRESULT LoadTileSets (CSIFPackage* pPackage, __out TRStrMap<CTileSet*>& mapTiles);
	HRESULT LoadTileSet (CSIFPackage* pPackage, RSTRING rstrName, __out TRStrMap<CTileSet*>& mapTiles);
	HRESULT LoadKeyVariants (ISimbeyInterchangeFile* pSIF, CTileSet* pTileSet, RSTRING rstrKey, IJSONArray* pVariants, bool fAutoOffset);
}
