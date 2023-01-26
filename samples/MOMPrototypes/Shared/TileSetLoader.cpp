#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Published\JSON.h"
#include "TileSetLoader.h"

namespace TileSetLoader
{
	INT CountShadowRows (ISimbeyInterchangeFileLayer* pLayer)
	{
		INT cShadow = 0;
		PBYTE pBits;
		DWORD cbBits, cbRow;

		if(SUCCEEDED(pLayer->GetBitsPtr(&pBits, &cbBits)))
		{
			SIZE size;

			SideAssertHr(pLayer->GetSize(&size));
			cbRow = size.cx * sizeof(DWORD);

			pBits += cbRow * size.cy;

			for(LONG y = size.cy - 1; y >= 0; y--)
			{
				pBits -= cbRow;

				PBYTE pbPixel = pBits;
				for(LONG x = size.cx - 1; x >= 0; x--)
				{
					if(255 == pbPixel[3])
					{
						cShadow = size.cy - (y + 1);
						goto End;
					}

					pbPixel += sizeof(DWORD);
				}
			}
		}

	End:
		return cShadow;
	}

	HRESULT LoadTileSets (CSIFPackage* pPackage, __out TRStrMap<CTileSet*>& mapTiles)
	{
		HRESULT hr;
		TStackRef<IJSONArray> srTileSets;
		sysint cTiles;
		RSTRING rstrName = NULL;

		Check(pPackage->GetDirectory(&srTileSets));
		cTiles = srTileSets->Count();

		for(sysint i = 0; i < cTiles; i++)
		{
			TStackRef<IJSONObject> srTileSetDir;
			TStackRef<IJSONValue> srvName;
			TStackRef<CSIFPackage> srTileSet;

			Check(srTileSets->GetObject(i, &srTileSetDir));
			Check(srTileSetDir->FindValueW(L"name", &srvName));
			Check(srvName->GetString(&rstrName));
			if(!mapTiles.HasItem(rstrName))
			{
				Check(pPackage->OpenDirectory(RStrToWide(rstrName), RStrLen(rstrName), &srTileSet));
				Check(LoadTileSet(srTileSet, rstrName, mapTiles));
			}

			RStrRelease(rstrName); rstrName = NULL;
		}

	Cleanup:
		RStrRelease(rstrName);
		return hr;
	}

	HRESULT LoadTileSet (CSIFPackage* pPackage, RSTRING rstrName, __out TRStrMap<CTileSet*>& mapTiles)
	{
		HRESULT hr;
		TStackRef<ISimbeyInterchangeFile> srSIF;
		TStackRef<IJSONValue> srv;
		TStackRef<IJSONObject> srData;
		TStackRef<IJSONArray> srKeys;
		sysint cKeys;
		CTileSet* pTileSet = NULL;
		RSTRING rstrKey = NULL;

		Check(pPackage->OpenSIF(L"tiles.sif", &srSIF));
		Check(pPackage->GetJSONData(SLP(L"tiles.json"), &srv));
		Check(srv->GetObject(&srData));
		srv.Release();

		Check(srData->FindNonNullValueW(L"keys", &srv));
		Check(srv->GetArray(&srKeys));
		srv.Release();

		pTileSet = __new CTileSet(rstrName);
		CheckAlloc(pTileSet);

		cKeys = srKeys->Count();
		for(sysint i = 0; i < cKeys; i++)
		{
			TStackRef<IJSONObject> srKey;
			TStackRef<IJSONArray> srVariants;
			bool fAutoOffset = false;

			Check(srKeys->GetObject(i, &srKey));

			srv.Release();
			Check(srKey->FindNonNullValueW(L"key", &srv));
			Check(srv->GetString(&rstrKey));

			srv.Release();
			Check(srKey->FindNonNullValueW(L"variants", &srv));
			Check(srv->GetArray(&srVariants));

			srv.Release();
			if(SUCCEEDED(srKey->FindNonNullValueW(L"auto-offset", &srv)))
				Check(srv->GetBoolean(&fAutoOffset));

			Check(LoadKeyVariants(srSIF, pTileSet, rstrKey, srVariants, fAutoOffset));

			RStrRelease(rstrKey); rstrKey = NULL;
		}

		Check(mapTiles.Add(rstrName, pTileSet));
		pTileSet = NULL;

	Cleanup:
		RStrRelease(rstrKey);
		__delete pTileSet;
		if(srSIF)
			srSIF->Close();
		return hr;
	}

	HRESULT LoadKeyVariants (ISimbeyInterchangeFile* pSIF, CTileSet* pTileSet, RSTRING rstrKey, IJSONArray* pVariants, bool fAutoOffset)
	{
		HRESULT hr;
		sysint cVariants = pVariants->Count();
		RSTRING rstrFrame = NULL;

		CheckIf(0 == cVariants, E_UNEXPECTED);

		for(sysint i = 0; i < cVariants; i++)
		{
			TStackRef<IJSONValue> srv;
			TStackRef<IJSONArray> srFrames;
			TStackRef<ISimbeyInterchangeFileLayer> srLayer;
			sysint cFrames;
			INT xOffset = 0, yOffset = 0;

			Check(pVariants->GetValue(i, &srv));
			Check(srv->GetArray(&srFrames));
			srv.Release();

			cFrames = srFrames->Count();
			if(1 == cFrames)
			{
				TStackRef<ISimbeyInterchangeSprite> srSprite;

				Check(srFrames->GetString(0, &rstrFrame));
				Check(pSIF->FindLayer(RStrToWide(rstrFrame), &srLayer, NULL));

				if(fAutoOffset)
				{
					SIZE size;
					SideAssertHr(srLayer->GetSize(&size));
					xOffset = size.cx / -2;
					yOffset = CountShadowRows(srLayer) - size.cy;
				}

				Check(sifCreateStaticSprite(srLayer, xOffset, yOffset, &srSprite));
				Check(pTileSet->AddVariant(rstrKey, srSprite));
				srLayer.Release();
				RStrRelease(rstrFrame); rstrFrame = NULL;
			}
			else
			{
				TStackRef<ISimbeyInterchangeAnimator> srAnimator;

				Check(sifCreateAnimator(cFrames, 1, &srAnimator));
				for(sysint n = 0; n < cFrames; n++)
				{
					Check(srFrames->GetString(n, &rstrFrame));
					Check(pSIF->FindLayer(RStrToWide(rstrFrame), &srLayer, NULL));

					if(fAutoOffset)
					{
						SIZE size;
						SideAssertHr(srLayer->GetSize(&size));
						xOffset = size.cx / -2;
						yOffset = CountShadowRows(srLayer) - size.cy;
					}

					Check(srAnimator->AddFrame(0, 8, n, xOffset, yOffset));
					Check(srAnimator->SetImage(n, FALSE, srLayer, FALSE));
					srLayer.Release();
					RStrRelease(rstrFrame); rstrFrame = NULL;
				}

				Check(pTileSet->AddVariant(rstrKey, srAnimator));
			}
		}

	Cleanup:
		RStrRelease(rstrFrame);
		return hr;
	}
}
