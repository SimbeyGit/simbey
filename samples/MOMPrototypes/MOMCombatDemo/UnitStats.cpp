#include <windows.h>
#include <gdiplus.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Util\Formatting.h"
#include "UnitStats.h"

HRESULT ResolveLink (IJSONArray* pTypes, ISimbeyInterchangeFile* pSIF, RSTRING rstrKeyW, RSTRING rstrValueW, __deref_out ISimbeyInterchangeFileLayer** ppIcon)
{
	HRESULT hr;
	TStackRef<IJSONObject> srLink;
	TStackRef<IJSONValue> srv;
	RSTRING rstrImageW = NULL;

	Check(JSONFindArrayObject(pTypes, rstrKeyW, rstrValueW, &srLink, NULL));
	Check(srLink->FindNonNullValueW(L"image", &srv));
	Check(srv->GetString(&rstrImageW));
	Check(pSIF->FindLayer(RStrToWide(rstrImageW), ppIcon, NULL));

Cleanup:
	RStrRelease(rstrImageW);
	return hr;
}

CUnitStats::CUnitStats (CSIFSurface* pSurface, ISimbeyFontCollection* pFonts, IPopupHost* pScreen) :
	m_pSurface(pSurface),
	m_pFonts(pFonts),
	m_pScreen(pScreen),
	m_pCanvas(NULL),
	m_pLayer(NULL),
	m_pSIF(NULL),
	m_pGenerated(NULL),
	m_pBaseLayer(NULL),
	m_pAbilitiesLayer(NULL),
	m_pAbilities(NULL),
	m_idxAbilities(0)
{
	m_pFonts->AddRef();
}

CUnitStats::~CUnitStats ()
{
	SafeRelease(m_pAbilities);
	SafeRelease(m_pAbilitiesLayer);
	SafeRelease(m_pBaseLayer);

	if(m_pGenerated)
	{
		m_pGenerated->Close();
		m_pGenerated->Release();
	}

	if(m_pSIF)
	{
		m_pSIF->Close();
		m_pSIF->Release();
	}

	SafeRelease(m_pFonts);

	Assert(NULL == m_pLayer);
	Assert(NULL == m_pCanvas);
}

HRESULT CUnitStats::Initialize (IJSONObject* pDef, IJSONObject* pStats, INT nLevel, ISimbeyInterchangeSprite* pUnitSprite, ISimbeyInterchangeFileLayer* pPortrait, INT cLiveHeads)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeFileLayer> srBackground, srGenerated;
	TStackRef<ISimbeyInterchangeSprite> srSprite;
	TStackRef<IJSONValue> srv;
	INT xView, yView;
	RECT rc;
	sysint idxUnitStats, idxAbilities;

	CSIFPackage* pPackage = m_pScreen->GetPackage();
	Check(pPackage->OpenSIF(L"graphics\\unit_details.sif", &m_pSIF));
	Check(m_pSIF->FindLayer(L"UnitDetails.png", &srBackground, NULL));
	Check(srBackground->GetPosition(&rc));

	m_pSurface->GetViewSize(&xView, &yView);
	Check(m_pSurface->AddCanvas(NULL, TRUE, &m_pCanvas));

	m_xStats = xView / 2 - (rc.right - rc.left) / 2;
	m_yStats = yView / 2 - (rc.bottom - rc.top) / 2;

	Check(m_pCanvas->AddLayer(FALSE, LayerRender::Masked, 0, &m_idxBackground));

	Check(sifCreateNew(&m_pGenerated));
	Check(m_pGenerated->AddLayer(static_cast<WORD>(rc.right - rc.left), static_cast<WORD>(rc.bottom - rc.top), &srGenerated, NULL));
	Check(RenderUnitStats(srGenerated, srBackground, pDef, pStats, nLevel, cLiveHeads));

	Check(sifCreateStaticSprite(srGenerated, m_xStats, m_yStats, &srSprite));
	Check(m_pCanvas->AddSprite(m_idxBackground, srSprite, &idxUnitStats));
	srSprite.Release();

	m_pBaseLayer = srGenerated.Detach();

	if(SUCCEEDED(JSONGetValueFromObject(pDef, SLP(L"base:abilities"), &srv)))
		Check(srv->GetArray(&m_pAbilities));

	Check(m_pGenerated->AddLayer(400, 90, &m_pAbilitiesLayer, NULL));
	Check(sifCreateStaticSprite(m_pAbilitiesLayer, m_xStats + 5, m_yStats + 178, &srSprite));
	Check(m_pCanvas->AddSprite(m_idxBackground, srSprite, &idxAbilities));
	Check(RenderAbilities(pDef));

	Check(static_cast<CInteractiveCanvas*>(m_pCanvas)->AddInteractiveLayer(TRUE, LayerRender::Masked, 0, this, &m_pLayer));

	if(pPortrait)
	{
		PBYTE pBits;
		DWORD cb;
		RECT rcPortrait;
		BYTE bits[62 * 62 * 4];

		Check(pPortrait->GetBitsPtr(&pBits, &cb));
		Check(pPortrait->GetPosition(&rcPortrait));

		Check(sifResizeBitsX(pBits, rcPortrait.right - rcPortrait.left, rcPortrait.bottom - rcPortrait.top, 4, (rcPortrait.right - rcPortrait.left) * 4, bits, 62, 62, 62 * 4, SIF_RESIZE_BICUBIC));

		Check(m_pBaseLayer->GetBitsPtr(&pBits, &cb));
		sifCopyBits32(pBits, 5, 5, rc.right - rc.left, rc.bottom - rc.top, bits, 0, 0, 62, 62, 62, 62);
	}
	else
	{
		Check(m_pLayer->AddSprite(pUnitSprite, NULL));
		pUnitSprite->SetPosition(m_xStats + 24, m_yStats + 35);
	}

Cleanup:
	return hr;
}

VOID CUnitStats::Close (VOID)
{
	m_pScreen->RemovePopup(this);
}

// IPopupView

VOID CUnitStats::Destroy (VOID)
{
	if(m_pCanvas)
	{
		SafeRelease(m_pLayer);
		m_pSurface->RemoveCanvas(m_pCanvas);
		m_pCanvas = NULL;
	}
}

// ILayerInputHandler

BOOL CUnitStats::ProcessMouseInput (LayerInput::Mouse eType, WPARAM wParam, LPARAM lParam, INT xView, INT yView, LRESULT& lResult)
{
	if(LayerInput::Move == eType)
		m_pScreen->UpdateMouse(lParam);

	return FALSE;
}

BOOL CUnitStats::ProcessKeyboardInput (LayerInput::Keyboard eType, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	if(LayerInput::KeyUp == eType && VK_ESCAPE == wParam)
	{
		Close();
		return TRUE;
	}

	return FALSE;
}

HRESULT CUnitStats::RenderUnitStats (ISimbeyInterchangeFileLayer* pTarget, ISimbeyInterchangeFileLayer* pBackground, IJSONObject* pDef, IJSONObject* pStats, INT nLevel, INT cLiveHeads)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeFile> srCombatStats, srComponents;
	ISimbeyInterchangeFileLayer* rgComponentTiles[2] = {0};
	TStackRef<IJSONValue> srv, srv2;
	TStackRef<IJSONArray> srStatTypes, srUpkeep, srMoves;
	RSTRING rstrNameW = NULL, rstrTagLineW = NULL, rstrMoveW = NULL;
	DWORD cbBits;
	RECT rc;
	SIF_SURFACE sifSurface;
	PVOID pvTitle = NULL, pvTagLine = NULL, pvLabel = NULL;
	CSIFPackage* pPackage = m_pScreen->GetPackage();

	Check(pPackage->OpenSIF(L"combat stats\\combat_stats.sif", &srCombatStats));

	Check(pPackage->OpenSIF(L"combat stats\\Components\\components.sif", &srComponents));
	Check(srComponents->FindLayer(L"Default.png", &rgComponentTiles[0], NULL));
	Check(srComponents->FindLayer(L"Expanded.png", &rgComponentTiles[1], NULL));

	Check(pPackage->GetJSONData(SLP(L"combat stats\\combat_stats.json"), &srv));
	Check(JSONGetValue(srv, SLP(L"stats"), &srv2));
	Check(srv2->GetArray(&srStatTypes));
	srv2.Release();
	srv.Release();

	Check(JSONGetValueFromObject(pDef, SLP(L"base:name"), &srv));
	Check(srv->GetString(&rstrNameW));
	srv.Release();

	if(SUCCEEDED(JSONGetValueFromObject(pDef, SLP(L"base:tag_line"), &srv)))
	{
		Check(srv->GetString(&rstrTagLineW));
		srv.Release();
	}

	if(SUCCEEDED(JSONGetValueFromObject(pDef, SLP(L"base:upkeep"), &srv)))
	{
		Check(srv->GetArray(&srUpkeep));
		srv.Release();
	}

	Check(pStats->FindNonNullValueW(L"move", &srv));
	Check(srv->GetArray(&srMoves));
	srv.Release();

	Check(pTarget->GetPosition(&rc));
	Check(pTarget->GetBitsPtr(&sifSurface.pbSurface, &cbBits));
	sifSurface.cBitsPerPixel = 32;
	sifSurface.xSize = rc.right - rc.left;
	sifSurface.ySize = rc.bottom - rc.top;
	sifSurface.lPitch = sifSurface.xSize * 4;
	Check(pBackground->CopyToBits32(&sifSurface, 0, 0));

	Check(m_pFonts->CreateFont(L"Dream Orphanage Rg", 16.0f, Gdiplus::FontStyleBold, &pvTitle));
	Check(m_pFonts->CreateFont(L"Dream Orphanage Rg", 12.0f, Gdiplus::FontStyleBold, &pvTagLine));
	Check(m_pFonts->CreateFont(L"Dream Orphanage Rg", 10.0f, Gdiplus::FontStyleBold, &pvLabel));

	// Toggle into the compatible GDI format.
	sifToggleChannels(&sifSurface);

	{
		Gdiplus::SolidBrush solidBrush(Gdiplus::Color(255, 130, 220, 240));
		Gdiplus::Bitmap bitmap(sifSurface.xSize, sifSurface.ySize, sifSurface.lPitch, PixelFormat32bppARGB, sifSurface.pbSurface);
		Gdiplus::Graphics g(&bitmap);
		Gdiplus::StringFormat fmt;
		Gdiplus::PointF pt(0.0f, 0.0f);
		INT nDamage, cchDamage;
		WCHAR wzDamage[12];

		g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
		fmt.SetFormatFlags(Gdiplus::StringFormatFlagsBypassGDI);

		pt.X = 75.0f;
		pt.Y = 8.0f;
		if(rstrTagLineW)
			pt.Y -= 5.0f;
		g.DrawString(RStrToWide(rstrNameW), RStrLen(rstrNameW), reinterpret_cast<Gdiplus::Font*>(pvTitle), pt, &fmt, &solidBrush);
		if(rstrTagLineW)
		{
			pt.Y += 17.0f;
			g.DrawString(RStrToWide(rstrTagLineW), RStrLen(rstrTagLineW), reinterpret_cast<Gdiplus::Font*>(pvTagLine), pt, &fmt, &solidBrush);
		}

		pt.Y = 32.0f;
		g.DrawString(SLP(L"Upkeep"), reinterpret_cast<Gdiplus::Font*>(pvLabel), pt, &fmt, &solidBrush);

		pt.Y = 43.0f;
		g.DrawString(SLP(L"Moves"), reinterpret_cast<Gdiplus::Font*>(pvLabel), pt, &fmt, &solidBrush);

		pt.Y = 54.0f;
		g.DrawString(SLP(L"Damage"), reinterpret_cast<Gdiplus::Font*>(pvLabel), pt, &fmt, &solidBrush);

		Check(pDef->FindNonNullValueW(L"damage", &srv));
		Check(srv->GetInteger(&nDamage));
		srv.Release();

		Check(Formatting::TInt32ToAsc(nDamage, wzDamage, ARRAYSIZE(wzDamage), 10, &cchDamage));
		pt.X = 120.0f;
		g.DrawString(wzDamage, cchDamage, reinterpret_cast<Gdiplus::Font*>(pvLabel), pt, &fmt, &solidBrush);

		if(-1 != cLiveHeads)
		{
			pt.X = 160.0f;
			g.DrawString(SLP(L"Heads"), reinterpret_cast<Gdiplus::Font*>(pvLabel), pt, &fmt, &solidBrush);
			Check(Formatting::TInt32ToAsc(cLiveHeads, wzDamage, ARRAYSIZE(wzDamage), 10, &cchDamage));
			pt.X = 195.0f;
			g.DrawString(wzDamage, cchDamage, reinterpret_cast<Gdiplus::Font*>(pvLabel), pt, &fmt, &solidBrush);
		}

		pt.X = 8.0f;
		pt.Y = 71.0f;
		g.DrawString(SLP(L"Melee"), reinterpret_cast<Gdiplus::Font*>(pvTitle), pt, &fmt, &solidBrush);

		pt.X = 8.0f;
		pt.Y = 91.0f;
		g.DrawString(SLP(L"Range"), reinterpret_cast<Gdiplus::Font*>(pvTitle), pt, &fmt, &solidBrush);

		pt.X = 8.0f;
		pt.Y = 111.0f;
		g.DrawString(SLP(L"Armor"), reinterpret_cast<Gdiplus::Font*>(pvTitle), pt, &fmt, &solidBrush);

		pt.X = 8.0f;
		pt.Y = 131.0f;
		g.DrawString(SLP(L"Resist"), reinterpret_cast<Gdiplus::Font*>(pvTitle), pt, &fmt, &solidBrush);

		pt.X = 8.0f;
		pt.Y = 151.0f;
		g.DrawString(SLP(L"Hits"), reinterpret_cast<Gdiplus::Font*>(pvTitle), pt, &fmt, &solidBrush);
	}

	// Toggle back to the SIF format.
	sifToggleChannels(&sifSurface);

	if(srUpkeep)
		Check(RenderUpkeep(&sifSurface, srUpkeep, 122, 33, srCombatStats, srStatTypes));

	if(0 < srMoves->Count())
	{
		TStackRef<IJSONObject> srMove;
		TStackRef<ISimbeyInterchangeFileLayer> srMoveIcon;
		INT cMoves;

		Check(srMoves->GetObject(0, &srMove));
		Check(srMove->FindNonNullValueW(L"stat", &srv));
		Check(srv->GetString(&rstrMoveW));
		srv.Release();

		Check(srMove->FindNonNullValueW(L"value", &srv));
		srMove.Release();

		Check(srv->GetInteger(&cMoves));
		srv.Release();

		Check(ResolveLink(srStatTypes, srCombatStats, RSTRING_CAST(L"stat"), rstrMoveW, &srMoveIcon));

		for(INT i = 0; i < cMoves; i++)
			Check(srMoveIcon->DrawToBits32(&sifSurface, 120 + i * 11, 43));
	}

	Check(RenderComponents(&sifSurface, rgComponentTiles, 75, 74, pDef, pStats, L"melee", srCombatStats, srStatTypes));
	Check(RenderComponents(&sifSurface, rgComponentTiles, 75, 94, pDef, pStats, L"range", srCombatStats, srStatTypes));
	Check(RenderComponents(&sifSurface, rgComponentTiles, 75, 114, pDef, pStats, L"defense", srCombatStats, srStatTypes));
	Check(RenderComponents(&sifSurface, rgComponentTiles, 75, 134, pDef, pStats, L"resist", srCombatStats, srStatTypes));
	Check(RenderComponents(&sifSurface, rgComponentTiles, 75, 154, pDef, pStats, L"hits", srCombatStats, srStatTypes));

Cleanup:
	if(pvLabel)
		m_pFonts->DeleteFont(pvLabel);
	if(pvTagLine)
		m_pFonts->DeleteFont(pvTagLine);
	if(pvTitle)
		m_pFonts->DeleteFont(pvTitle);
	RStrRelease(rstrMoveW);
	RStrRelease(rstrTagLineW);
	RStrRelease(rstrNameW);
	if(srComponents)
	{
		for(INT i = 0; i < ARRAYSIZE(rgComponentTiles); i++)
			SafeRelease(rgComponentTiles[i]);
		srComponents->Close();
	}
	if(srCombatStats)
		srCombatStats->Close();
	return hr;
}

HRESULT CUnitStats::RenderComponents (SIF_SURFACE* psifSurface, ISimbeyInterchangeFileLayer** prgComponentTiles, INT xBase, INT yBase, IJSONObject* pDef, IJSONObject* pStats, PCWSTR pcwzStat, ISimbeyInterchangeFile* pCombatStats, IJSONArray* pStatTypes)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeFileLayer> srIcon;
	TStackRef<IJSONValue> srv;
	WCHAR wzBaseStat[32];
	INT cchBaseStat;
	RSTRING rstrStatW = NULL;

	Check(Formatting::TPrintF(wzBaseStat, ARRAYSIZE(wzBaseStat), &cchBaseStat, L"base:stats:%ls", pcwzStat));
	if(SUCCEEDED(JSONGetValueFromObject(pDef, wzBaseStat, cchBaseStat, &srv)))
	{
		INT nBaseValue, nExpanded;
		INT x = xBase, y = yBase;
		INT idxTile = 0;

		if(JSON::Object == srv->GetType())
		{
			TStackRef<IJSONObject> srStat;

			Check(srv->GetObject(&srStat));
			srv.Release();

			Check(srStat->FindNonNullValueW(L"value", &srv));
		}

		Check(srv->GetInteger(&nBaseValue));
		srv.Release();

		Check(pStats->FindNonNullValueW(pcwzStat, &srv));
		if(JSON::Object == srv->GetType())
		{
			TStackRef<IJSONObject> srStat;

			Check(srv->GetObject(&srStat));
			srv.Release();

			Check(srStat->FindNonNullValueW(L"stat", &srv));
			Check(srv->GetString(&rstrStatW));
			srv.Release();

			Check(srStat->FindNonNullValueW(L"value", &srv));
		}
		else
			Check(RStrFormatW(&rstrStatW, L"normal %ls", pcwzStat));
		Check(srv->GetInteger(&nExpanded));
		srv.Release();

		if(FAILED(ResolveLink(pStatTypes, pCombatStats, RSTRING_CAST(L"stat"), rstrStatW, &srIcon)))
		{
			RStrRelease(rstrStatW); rstrStatW = NULL;
			Check(RStrCreateW(TStrLenAssert(pcwzStat), pcwzStat, &rstrStatW));
			Check(ResolveLink(pStatTypes, pCombatStats, RSTRING_CAST(L"stat"), rstrStatW, &srIcon));
		}

		Check(DrawComponents(psifSurface, prgComponentTiles[0], srIcon, xBase, yBase, x, y, idxTile, nBaseValue));
		Check(DrawComponents(psifSurface, prgComponentTiles[1], srIcon, xBase, yBase, x, y, idxTile, nExpanded - nBaseValue));
	}

Cleanup:
	RStrRelease(rstrStatW);
	return hr;
}

HRESULT CUnitStats::RenderUpkeep (SIF_SURFACE* psifSurface, IJSONArray* pUpkeep, INT x, INT y, ISimbeyInterchangeFile* pCombatStats, IJSONArray* pStatTypes)
{
	HRESULT hr = S_FALSE;
	RSTRING rstrTypeW = NULL, rstrIconW = NULL;

	for(sysint i = 0; i < pUpkeep->Count(); i++)
	{
		TStackRef<IJSONObject> srUpkeep;
		TStackRef<IJSONValue> srv;
		INT nAmount;

		Check(pUpkeep->GetObject(i, &srUpkeep));
		Check(srUpkeep->FindNonNullValueW(L"type", &srv));
		Check(srv->GetString(&rstrTypeW));
		srv.Release();

		Check(srUpkeep->FindNonNullValueW(L"amount", &srv));
		Check(srv->GetInteger(&nAmount));

		while(0 < nAmount)
		{
			TStackRef<IJSONObject> srStat;
			TStackRef<ISimbeyInterchangeFileLayer> srIcon;
			INT xMove;

			if(10 <= nAmount)
			{
				Check(RStrFormatW(&rstrIconW, L"%r10", rstrTypeW));
				nAmount -= 10;
				xMove = 21;
			}
			else
			{
				Check(RStrFormatW(&rstrIconW, L"%r1", rstrTypeW));
				nAmount--;
				xMove = 10;
			}

			Check(ResolveLink(pStatTypes, pCombatStats, RSTRING_CAST(L"stat"), rstrIconW, &srIcon));
			Check(srIcon->DrawToBits32(psifSurface, x, y));
			x += xMove;

			RStrRelease(rstrIconW); rstrIconW = NULL;
		}

		x += 5;

		RStrRelease(rstrTypeW); rstrTypeW = NULL;
	}

Cleanup:
	RStrRelease(rstrIconW);
	RStrRelease(rstrTypeW);
	return hr;
}

HRESULT CUnitStats::DrawComponents (SIF_SURFACE* psifSurface, ISimbeyInterchangeFileLayer* pComponent, ISimbeyInterchangeFileLayer* pIcon, INT& xBase, INT& yBase, INT& x, INT& y, INT& idxTile, INT cTiles)
{
	HRESULT hr = S_FALSE;

	for(INT i = 0; i < cTiles; i++)
	{
		Check(pComponent->DrawToBits32(psifSurface, x, y));
		Check(pIcon->DrawToBits32(psifSurface, x, y));
		x += 15;

		idxTile++;
		if((idxTile % 20) == 0)
		{
			xBase += 3;
			x = xBase;

			yBase += 3;
			y = yBase;
		}
		else if((idxTile % 5) == 0)
			x += 4;
	}

Cleanup:
	return hr;
}

HRESULT CUnitStats::RenderAbilities (IJSONObject* pDef)
{
	HRESULT hr = S_OK;
	TStackRef<ISimbeyInterchangeFile> srSIF;
	TStackRef<IJSONObject> srAbilityMaps;
	TStackRef<IJSONArray> srHeroAbilities, srUnitAbilities;
	TStackRef<IJSONValue> srv;
	SIF_SURFACE sifSurface;
	PBYTE pbSource;
	DWORD cbBits;
	RECT rc;
	INT nSourceWidth, nSourceHeight;
	RSTRING rstrTypeW = NULL, rstrAbilityW = NULL, rstrLabelW = NULL;
	PVOID pvLabel = NULL;

	CheckIf(NULL == m_pAbilities, S_FALSE);

	Check(JSONGetValueFromObject(pDef, SLP(L"base:type"), &srv));
	Check(srv->GetString(&rstrTypeW));
	srv.Release();

	Check(m_pFonts->CreateFont(L"Dream Orphanage Rg", 10.0f, Gdiplus::FontStyleBold, &pvLabel));

	CSIFPackage* pPackage = m_pScreen->GetPackage();
	Check(pPackage->OpenSIF(L"abilities\\abilities.sif", &srSIF));
	Check(pPackage->GetJSONData(SLP(L"abilities\\abilities.json"), &srv));
	Check(srv->GetObject(&srAbilityMaps));

	if(0 == TStrCmpAssert(RStrToWide(rstrTypeW), L"hero") || 0 == TStrCmpAssert(RStrToWide(rstrTypeW), L"champion"))
	{
		srv.Release();
		Check(srAbilityMaps->FindNonNullValueW(L"hero_abilities", &srv));
		Check(srv->GetArray(&srHeroAbilities));
	}

	srv.Release();
	Check(srAbilityMaps->FindNonNullValueW(L"unit_abilities", &srv));
	Check(srv->GetArray(&srUnitAbilities));

	Check(m_pBaseLayer->GetBitsPtr(&pbSource, &cbBits));
	Check(m_pBaseLayer->GetPosition(&rc));
	nSourceWidth = rc.right - rc.left;
	nSourceHeight = rc.bottom - rc.top;

	Check(m_pAbilitiesLayer->GetBitsPtr(&sifSurface.pbSurface, &cbBits));
	Check(m_pAbilitiesLayer->GetPosition(&rc));

	sifSurface.cBitsPerPixel = 32;
	sifSurface.xSize = rc.right - rc.left;
	sifSurface.ySize = rc.bottom - rc.top;
	sifSurface.lPitch = sifSurface.xSize * 4;
	sifCopyBits32(sifSurface.pbSurface, 0, 0, sifSurface.xSize, sifSurface.ySize, pbSource, 5, 179, nSourceWidth, nSourceHeight, sifSurface.xSize, sifSurface.ySize);

	INT x = 3, y = 2;

	for(INT i = 0; i < 10; i++)
	{
		TStackRef<IJSONObject> srAbility;
		TStackRef<ISimbeyInterchangeFileLayer> srIcon;
		Gdiplus::SolidBrush solidBrush(Gdiplus::Color(255, 130, 220, 240));
		Gdiplus::Bitmap bitmap(sifSurface.xSize, sifSurface.ySize, sifSurface.lPitch, PixelFormat32bppARGB, sifSurface.pbSurface);
		Gdiplus::Graphics g(&bitmap);
		Gdiplus::StringFormat fmt;
		Gdiplus::PointF pt;

		g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
		fmt.SetFormatFlags(Gdiplus::StringFormatFlagsBypassGDI);

		sysint idxAbility = m_idxAbilities + i;
		if(idxAbility >= m_pAbilities->Count())
			break;
		Check(m_pAbilities->GetObject(idxAbility, &srAbility));

		srv.Release();
		Check(srAbility->FindNonNullValueW(L"name", &srv));
		Check(srv->GetString(&rstrAbilityW));

		if(NULL == srHeroAbilities || FAILED(ResolveLink(srHeroAbilities, srSIF, RSTRING_CAST(L"name"), rstrAbilityW, &srIcon)))
			Check(ResolveLink(srUnitAbilities, srSIF, RSTRING_CAST(L"name"), rstrAbilityW, &srIcon));

		Check(srIcon->DrawToBits32(&sifSurface, x, y));

		srv.Release();
		if(SUCCEEDED(srAbility->FindNonNullValueW(L"value", &srv)))
		{
			INT nValue;
			if(SUCCEEDED(srv->GetInteger(&nValue)))
			{
				if(0 < nValue)
					Check(RStrFormatW(&rstrLabelW, L"%r x %d", rstrAbilityW, nValue));
				else
					RStrSet(rstrLabelW, rstrAbilityW);
			}
			else
			{
				DOUBLE dblValue;
				Check(srv->GetDouble(&dblValue));
				if(0.0 < dblValue)
					Check(RStrFormatW(&rstrLabelW, L"%r x %.1f", rstrAbilityW, dblValue));
				else
					RStrSet(rstrLabelW, rstrAbilityW);
			}
		}
		else
			RStrSet(rstrLabelW, rstrAbilityW);

		sifToggleChannels(&sifSurface);
		pt.X = static_cast<FLOAT>(x + 20);
		pt.Y = static_cast<FLOAT>(y + 1);
		g.DrawString(RStrToWide(rstrLabelW), RStrLen(rstrLabelW), reinterpret_cast<Gdiplus::Font*>(pvLabel), pt, &fmt, &solidBrush);
		sifToggleChannels(&sifSurface);

		if(i == 5)
		{
			x = 200;
			y = 2;
		}
		else
			y += 18;

		RStrRelease(rstrLabelW); rstrLabelW = NULL;
		RStrRelease(rstrAbilityW); rstrAbilityW = NULL;
	}

Cleanup:
	RStrRelease(rstrLabelW);
	if(rstrAbilityW)
	{
#ifdef	_DEBUG
		OutputDebugStringW(L"Could not resolve ability: ");
		OutputDebugStringW(RStrToWide(rstrAbilityW));
		OutputDebugStringW(L"\r\n");
#endif

		RStrRelease(rstrAbilityW);
	}
	RStrRelease(rstrTypeW);
	if(srSIF)
		srSIF->Close();
	if(pvLabel)
		m_pFonts->DeleteFont(pvLabel);
	return hr;
}
