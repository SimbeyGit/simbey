#include <windows.h>
#include <gdiplus.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Util\Formatting.h"
#include "CombatSpells.h"
#include "SpellBook.h"

///////////////////////////////////////////////////////////////////////////////
// CSpellBookPage
///////////////////////////////////////////////////////////////////////////////

CSpellBookPage::CSpellBookPage (SPELL_PAGE* pPage, PVOID pvTitle, PVOID pvLabel, INT x, INT y) :
	m_pPage(pPage),
	m_pvTitle(pvTitle),
	m_pvLabel(pvLabel),
	m_x(x),
	m_y(y)
{
	ZeroMemory(m_bits, sizeof(m_bits));
}

CSpellBookPage::~CSpellBookPage ()
{
}

HRESULT CSpellBookPage::Initialize (ISimbeyInterchangeFile* pSIF, INT nMagicPower)
{
	struct GLYPH_DATA
	{
		ISimbeyInterchangeFileLayer* pGlyph;
		INT y;
		INT cGlyphs;
		INT xFillerStart, xFillerEnd;
	};

	HRESULT hr = S_FALSE;
	RSTRING rstrName = NULL, rstrSchool = NULL;
	GLYPH_DATA data[6] = {0};

	Gdiplus::SolidBrush solidBrush(Gdiplus::Color(255, 45, 35, 16));
	Gdiplus::Bitmap bitmap(SPELL_BOOK_PAGE_WIDTH, SPELL_BOOK_PAGE_HEIGHT, SPELL_BOOK_PAGE_WIDTH * 4, PixelFormat32bppARGB, m_bits);
	Gdiplus::Graphics g(&bitmap);
	Gdiplus::StringFormat fmt;

	g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
	fmt.SetFormatFlags(Gdiplus::StringFormatFlagsBypassGDI);

	Gdiplus::PointF pt(0.0f, 0.0f);
	Gdiplus::RectF box;
	g.MeasureString(RStrToWide(m_pPage->rstrGroup), RStrLen(m_pPage->rstrGroup), reinterpret_cast<Gdiplus::Font*>(m_pvTitle), pt, &box);

	pt.X = static_cast<FLOAT>(SPELL_BOOK_PAGE_WIDTH / 2) - box.Width / 2.0f;
	g.DrawString(RStrToWide(m_pPage->rstrGroup), RStrLen(m_pPage->rstrGroup), reinterpret_cast<Gdiplus::Font*>(m_pvTitle), pt, &fmt, &solidBrush);

	pt.Y += 16.0f;
	for(INT i = 0; i < m_pPage->cSpells; i++)
	{
		TStackRef<IJSONValue> srv;
		INT nCost;
		WCHAR wzMP[8], wzGlyph[32];
		INT cchMP;

		Check(m_pPage->prgSpells[i]->FindNonNullValueW(L"name", &srv));
		Check(srv->GetString(&rstrName));
		srv.Release();

		pt.X = 0.0f;
		g.MeasureString(RStrToWide(rstrName), RStrLen(rstrName), reinterpret_cast<Gdiplus::Font*>(m_pvLabel), pt, &box);
		g.DrawString(RStrToWide(rstrName), RStrLen(rstrName), reinterpret_cast<Gdiplus::Font*>(m_pvLabel), pt, &fmt, &solidBrush);
		data[i].xFillerStart = static_cast<INT>(box.Width) + 2;

		Check(m_pPage->prgSpells[i]->FindNonNullValueW(L"cost", &srv));
		Check(srv->GetInteger(&nCost));
		srv.Release();

		Check(Formatting::TPrintF(wzMP, ARRAYSIZE(wzMP), &cchMP, L"%d MP", nCost));
		g.MeasureString(wzMP, cchMP, reinterpret_cast<Gdiplus::Font*>(m_pvLabel), pt, &box);
		pt.X = SPELL_BOOK_PAGE_WIDTH - box.Width;
		g.DrawString(wzMP, cchMP, reinterpret_cast<Gdiplus::Font*>(m_pvLabel), pt, &fmt, &solidBrush);
		data[i].xFillerEnd = static_cast<INT>(pt.X) - 2;

		Check(m_pPage->prgSpells[i]->FindNonNullValueW(L"school", &srv));
		Check(srv->GetString(&rstrSchool));
		Check(Formatting::TPrintF(wzGlyph, ARRAYSIZE(wzGlyph), NULL, L"%r.png", rstrSchool));
		wzGlyph[0] = TUpperCase(wzGlyph[0]);
		Check(pSIF->FindLayer(wzGlyph, &data[i].pGlyph, NULL));
		data[i].y = static_cast<INT>(pt.Y);
		data[i].cGlyphs = nMagicPower / nCost;
		if(data[i].cGlyphs > 38)
			data[i].cGlyphs = 38;

		RStrRelease(rstrSchool); rstrSchool = NULL;
		RStrRelease(rstrName); rstrName = NULL;
		pt.Y += 21.0f;
	}

	SIF_SURFACE sifSurface32;
	sifSurface32.cBitsPerPixel = 32;
	sifSurface32.lPitch = SPELL_BOOK_PAGE_WIDTH * 4;
	sifSurface32.pbSurface = m_bits;
	sifSurface32.xSize = SPELL_BOOK_PAGE_WIDTH;
	sifSurface32.ySize = SPELL_BOOK_PAGE_HEIGHT;

	// Format the surface for SIF.
	sifToggleChannels(&sifSurface32);

	for(INT i = 0; i < m_pPage->cSpells; i++)
	{
		ISimbeyInterchangeFileLayer* pGlyph = data[i].pGlyph;
		INT x = 2, y = data[i].y;

		DrawSpellFiller(data[i].xFillerStart, data[i].xFillerEnd, y + 5);

		y += 10;

		if(data[i].cGlyphs < 19)
			DrawSpellFiller(4 + data[i].cGlyphs * 6, SPELL_BOOK_PAGE_WIDTH - 5, y + 1);

		INT cFinalGlyphs = data[i].cGlyphs - 19;
		if(cFinalGlyphs < 0)
			cFinalGlyphs = 0;
		if(cFinalGlyphs < 19)
			DrawSpellFiller(4 + cFinalGlyphs * 6, SPELL_BOOK_PAGE_WIDTH - 5, y + 7);

		for(INT n = 0; n < data[i].cGlyphs; n++)
		{
			pGlyph->DrawToBits32(&sifSurface32, x, y);
			x += 6;

			if(n == 18)
			{
				x = 2;
				y += 6;
			}
		}
	}

Cleanup:
	for(INT i = 0; i < m_pPage->cSpells; i++)
		SafeRelease(data[i].pGlyph);

	RStrRelease(rstrSchool);
	RStrRelease(rstrName);
	return hr;
}

// ISimbeyInterchangeSprite

IFACEMETHODIMP CSpellBookPage::SelectAnimation (INT nAnimation, INT nFrame, INT cTicks)
{
	return S_FALSE;
}

IFACEMETHODIMP_(VOID) CSpellBookPage::GetCurrentAnimation (__out INT* pnAnimation, __out INT* pnFrame, __out INT* pcTicks)
{
}

IFACEMETHODIMP_(VOID) CSpellBookPage::GetCurrentFrameSize (__out INT* pxSize, __out INT* pySize)
{
}

IFACEMETHODIMP_(VOID) CSpellBookPage::GetCurrentHitBox (__out RECT* prcHitBox)
{
}

IFACEMETHODIMP_(VOID) CSpellBookPage::UpdateFrameTick (VOID)
{
}

IFACEMETHODIMP_(VOID) CSpellBookPage::SetPosition (INT x, INT y)
{
	m_x = x;
	m_y = y;
}

IFACEMETHODIMP_(VOID) CSpellBookPage::GetPosition (__out INT& x, __out INT& y)
{
	x = m_x;
	y = m_y;
}

IFACEMETHODIMP_(BOOL) CSpellBookPage::DrawMaskedToDIB24 (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24)
{
	return sifDrawBits32ToDIB24(psifSurface24->pbSurface, m_x, m_y, psifSurface24->xSize, psifSurface24->ySize, m_bits, SPELL_BOOK_PAGE_WIDTH, SPELL_BOOK_PAGE_HEIGHT);
}

IFACEMETHODIMP_(BOOL) CSpellBookPage::DrawTileToDIB24 (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24, SIF_LINE_OFFSET* pslOffsets)
{
	return FALSE;
}

IFACEMETHODIMP_(BOOL) CSpellBookPage::DrawBlendedToDIB24 (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24)
{
	return DrawMaskedToDIB24(xOffset, yOffset, psifSurface24);
}

IFACEMETHODIMP_(BOOL) CSpellBookPage::DrawColorizedToDIB24 (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24)
{
	return DrawMaskedToDIB24(xOffset, yOffset, psifSurface24);
}

IFACEMETHODIMP_(COLORREF) CSpellBookPage::GetColorized (VOID)
{
	return 0;
}

IFACEMETHODIMP_(BOOL) CSpellBookPage::SetColorized (COLORREF cr)
{
	return FALSE;
}

IFACEMETHODIMP_(VOID) CSpellBookPage::GetFrameOffset (__out INT& x, __out INT& y)
{
	x = 0;
	y = 0;
}

VOID CSpellBookPage::DrawSpellFiller (INT xFillerStart, INT xFillerEnd, INT yFiller)
{
	INT xRange = xFillerEnd - xFillerStart;

	for(INT y = 0; y < 3; y++)
	{
		PBYTE pbRow = m_bits + (SPELL_BOOK_PAGE_WIDTH * 4) * (yFiller + y * 2) + xFillerStart * 4;
		INT x = 0;

		while(x < xRange)
		{
			INT xSize = (rand() % 8) + 1;
			if(x + xSize > xRange)
				xSize = xRange - x;
			for(INT i = 0; i < xSize; i++)
			{
				pbRow[3] = 255;
				pbRow[0] = 90;
				pbRow[1] = 70;
				pbRow[2] = 32;
				pbRow += sizeof(DWORD);
			}
			x += xSize + 1;
			pbRow += sizeof(DWORD);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// CSpellBook
///////////////////////////////////////////////////////////////////////////////

CSpellBook::CSpellBook (CSIFSurface* pSurface, IPopupHost* pScreen, IJSONObject* pWizard, ISimbeyFontCollection* pFonts, INT nMagicPower) :
	m_pSurface(pSurface),
	m_pScreen(pScreen),
	m_pWizard(pWizard),
	m_pFonts(pFonts),
	m_nMagicPower(nMagicPower),
	m_pvTitle(NULL),
	m_pvLabel(NULL),
	m_pLeft(NULL),
	m_pRight(NULL),
	m_pCanvas(NULL),
	m_pLayer(NULL),
	m_pSIF(NULL),
	m_pCloseButton(NULL),
	m_pTurnLeft(NULL),
	m_pTurnRight(NULL),
	m_pLeftPage(NULL),
	m_pRightPage(NULL)
{
	m_pWizard->AddRef();
	m_pFonts->AddRef();
}

CSpellBook::~CSpellBook ()
{
	Assert(NULL == m_pLeftPage);
	Assert(NULL == m_pRightPage);

	for(sysint i = 0; i < m_aPages.Length(); i++)
	{
		SPELL_PAGE& page = m_aPages[i];
		for(INT n = 0; n < page.cSpells; n++)
			page.prgSpells[n]->Release();
		RStrRelease(page.rstrGroup);
	}

	if(m_pvLabel)
		m_pFonts->DeleteFont(m_pvLabel);
	if(m_pvTitle)
		m_pFonts->DeleteFont(m_pvTitle);

	SafeRelease(m_pCloseButton);
	SafeRelease(m_pFonts);
	SafeRelease(m_pWizard);

	if(m_pSIF)
	{
		m_pSIF->Close();
		m_pSIF->Release();
	}

	Assert(NULL == m_pLayer);
	Assert(NULL == m_pCanvas);
}

HRESULT CSpellBook::Initialize (IJSONArray* pSpells)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeFileLayer> srBackground, srCloseButton;
	TStackRef<ISimbeyInterchangeSprite> srSprite;
	INT xView, yView, xClose, yClose;
	RECT rc;
	sysint idxSpellBook;

	CSIFPackage* pPackage = m_pScreen->GetPackage();
	Check(pPackage->OpenSIF(L"spell book\\spellbook.sif", &m_pSIF));
	Check(m_pSIF->FindLayer(L"background.png", &srBackground, NULL));
	Check(srBackground->GetPosition(&rc));

	m_pSurface->GetViewSize(&xView, &yView);
	Check(m_pSurface->AddCanvas(NULL, TRUE, &m_pCanvas));

	m_xBook = xView / 2 - (rc.right - rc.left) / 2;
	m_yBook = yView / 2 - (rc.bottom - rc.top) / 2;

	Check(m_pCanvas->AddLayer(FALSE, LayerRender::Masked, 0, &m_idxBackground));
	Check(sifCreateStaticSprite(srBackground, m_xBook, m_yBook, &srSprite));
	Check(m_pCanvas->AddSprite(m_idxBackground, srSprite, &idxSpellBook));

	xClose = m_xBook + 159;
	yClose = m_yBook + 168;

	Check(m_pSIF->FindLayer(L"closeButtonPressedInline.png", &srCloseButton, NULL));
	Check(srCloseButton->GetPosition(&rc));
	Check(sifCreateStaticSprite(srCloseButton, xClose, yClose, &m_pCloseButton));

	Check(static_cast<CInteractiveCanvas*>(m_pCanvas)->AddInteractiveLayer(TRUE, LayerRender::Masked, 0, this, &m_pLayer));

	Check(BuildPageList(pSpells));

	if(0 < m_aPages.Length())
	{
		m_pLeft = &m_aPages[0];
		if(1 < m_aPages.Length())
			m_pRight = &m_aPages[1];
	}

	Check(UpdatePageNav());

	Check(m_pFonts->CreateFont(L"Dream Orphanage Rg", 12.5f, Gdiplus::FontStyleRegular, &m_pvTitle));
	Check(m_pFonts->CreateFont(L"Dream Orphanage Rg", 9.0f, Gdiplus::FontStyleRegular, &m_pvLabel));

	if(m_pLeft)
		Check(CreateSpellBookPage(m_pLeft, m_xBook + 14, m_yBook + 4, &m_pLeftPage));

	if(m_pRight)
		Check(CreateSpellBookPage(m_pRight, m_xBook + 147, m_yBook + 4, &m_pRightPage));

Cleanup:
	return hr;
}

HRESULT CSpellBook::UpdatePageNav (VOID)
{
	HRESULT hr = S_FALSE;
	TStackRef<ISimbeyInterchangeFileLayer> srTurnLeft, srTurnRight;

	Assert(NULL == m_pTurnLeft);
	Assert(NULL == m_pTurnRight);

	if(m_pLeft && m_pLeft > &m_aPages[0])
	{
		Check(m_pSIF->FindLayer(L"turnPageLeft.png", &srTurnLeft, NULL));
		Check(sifCreateStaticSprite(srTurnLeft, m_xBook + 13, m_yBook + 3, &m_pTurnLeft));
		Check(m_pLayer->AddSprite(m_pTurnLeft, NULL));
	}

	if(m_pRight && 2 < m_aPages.Length() && m_pRight < &m_aPages[m_aPages.Length() - 1])
	{
		Check(m_pSIF->FindLayer(L"turnPageRight.png", &srTurnRight, NULL));
		Check(sifCreateStaticSprite(srTurnRight, m_xBook + 258, m_yBook + 3, &m_pTurnRight));
		Check(m_pLayer->AddSprite(m_pTurnRight, NULL));
	}

Cleanup:
	return hr;
}

HRESULT CSpellBook::StartPageNav (BOOL fForward)
{
	HRESULT hr;

	RemovePageNav();

	if(m_pLeftPage)
	{
		m_pLayer->RemoveSprite(m_pLeftPage);
		SafeRelease(m_pLeftPage);
	}

	if(m_pRightPage)
	{
		m_pLayer->RemoveSprite(m_pRightPage);
		SafeRelease(m_pRightPage);
	}

	if(fForward)
		m_pLeft += 2;
	else
		m_pLeft -= 2;

	Assert(m_pLeft >= &m_aPages[0]);
	m_pRight = m_pLeft + 1;
	if(m_pRight > &m_aPages[m_aPages.Length() - 1])
		m_pRight = NULL;

	if(m_pLeft)
		Check(CreateSpellBookPage(m_pLeft, m_xBook + 14, m_yBook + 4, &m_pLeftPage));

	if(m_pRight)
		Check(CreateSpellBookPage(m_pRight, m_xBook + 147, m_yBook + 4, &m_pRightPage));

	Check(UpdatePageNav());

Cleanup:
	return hr;
}

VOID CSpellBook::RemovePageNav (VOID)
{
	if(m_pTurnLeft)
	{
		m_pLayer->RemoveSprite(m_pTurnLeft);
		SafeRelease(m_pTurnLeft);
	}

	if(m_pTurnRight)
	{
		m_pLayer->RemoveSprite(m_pTurnRight);
		SafeRelease(m_pTurnRight);
	}
}

VOID CSpellBook::Close (VOID)
{
	m_pScreen->RemovePopup(this);
}

// IPopupView

VOID CSpellBook::Destroy (VOID)
{
	if(m_pCanvas)
	{
		if(m_pLeftPage)
		{
			m_pLayer->RemoveSprite(m_pLeftPage);
			SafeRelease(m_pLeftPage);
		}

		if(m_pRightPage)
		{
			m_pLayer->RemoveSprite(m_pRightPage);
			SafeRelease(m_pRightPage);
		}

		RemovePageNav();

		SafeRelease(m_pLayer);
		m_pSurface->RemoveCanvas(m_pCanvas);
		m_pCanvas = NULL;
	}
}

// ILayerInputHandler

BOOL CSpellBook::ProcessMouseInput (LayerInput::Mouse eType, WPARAM wParam, LPARAM lParam, INT xView, INT yView, LRESULT& lResult)
{
	if(LayerInput::Move == eType)
		m_pScreen->UpdateMouse(lParam);
	else if(LayerInput::LButtonDown == eType)
	{
		if(MouseOverSprite(xView, yView, m_pCloseButton))
			m_pCanvas->AddSprite(m_idxBackground, m_pCloseButton, NULL);
		return TRUE;
	}
	else if(LayerInput::LButtonUp == eType)
	{
		if(S_OK == m_pCanvas->FindSprite(m_idxBackground, m_pCloseButton, NULL))
		{
			m_pCanvas->RemoveSprite(m_idxBackground, m_pCloseButton);
			if(MouseOverSprite(xView, yView, m_pCloseButton))
				Close();
		}
		else if(m_pTurnLeft && MouseOverSprite(xView, yView, m_pTurnLeft))
			StartPageNav(FALSE);
		else if(m_pTurnRight && MouseOverSprite(xView, yView, m_pTurnRight))
			StartPageNav(TRUE);
		else
		{
			TStackRef<IJSONObject> srSpell;

			if(FindSpellAt(m_pLeft, xView, yView, m_xBook + 14, m_yBook + 4, &srSpell) ||
				FindSpellAt(m_pRight, xView, yView, m_xBook + 147, m_yBook + 4, &srSpell))
			{
				TStackRef<IJSONValue> srvCaster, srvSpawn;
				RSTRING rstrCasterW;

				SideAssertHr(m_pWizard->FindNonNullValueW(L"name", &srvCaster));
				SideAssertHr(srvCaster->GetString(&rstrCasterW));

				if(SUCCEEDED(srSpell->FindNonNullValueW(L"spawn", &srvSpawn)))
				{
					RSTRING rstrSpawn;

					SideAssertHr(srvSpawn->GetString(&rstrSpawn));
					m_pScreen->AttachSpellCaster(__new CCastSummonSpell(rstrCasterW, rstrSpawn));
					RStrRelease(rstrSpawn);
				}
				else
				{
					TStackRef<IJSONValue> srvName, srvFriendly;
					RSTRING rstrSpellW;
					bool fFriendly;

					SideAssertHr(srSpell->FindNonNullValueW(L"name", &srvName));
					srvName->GetString(&rstrSpellW);

					if(SUCCEEDED(srSpell->FindNonNullValueW(L"friendly", &srvFriendly)) && SUCCEEDED(srvFriendly->GetBoolean(&fFriendly)) && fFriendly)
						m_pScreen->AttachSpellCaster(__new CCastFriendlySpell(rstrCasterW, rstrSpellW));
					else
						m_pScreen->AttachSpellCaster(__new CCastTargetSpell(rstrCasterW, rstrSpellW));
					RStrRelease(rstrSpellW);
				}

				RStrRelease(rstrCasterW);
				Close();
			}
		}

		return TRUE;
	}

	return FALSE;
}

BOOL CSpellBook::ProcessKeyboardInput (LayerInput::Keyboard eType, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	if(LayerInput::KeyUp == eType && VK_ESCAPE == wParam)
	{
		Close();
		return TRUE;
	}

	return FALSE;
}

HRESULT CSpellBook::BuildPageList (IJSONArray* pSpellGroups)
{
	HRESULT hr;
	sysint cSpellGroups = pSpellGroups->Count();
	RSTRING rstrGroup = NULL;

	for(sysint i = 0; i < cSpellGroups; i++)
	{
		TStackRef<IJSONObject> srGroup;
		TStackRef<IJSONValue> srv;
		TStackRef<IJSONArray> srSpells;

		Check(pSpellGroups->GetObject(i, &srGroup));
		Check(srGroup->FindNonNullValueW(L"group", &srv));
		Check(srv->GetString(&rstrGroup));
		srv.Release();

		Check(srGroup->FindNonNullValueW(L"spells", &srv));
		Check(srv->GetArray(&srSpells));
		Check(AddSpellGroup(rstrGroup, srSpells));

		RStrRelease(rstrGroup); rstrGroup = NULL;
	}

Cleanup:
	RStrRelease(rstrGroup);
	return hr;
}

HRESULT CSpellBook::AddSpellGroup (RSTRING rstrGroup, IJSONArray* pSpells)
{
	HRESULT hr = S_FALSE;
	SPELL_PAGE page;
	sysint cSpells = pSpells->Count();

	page.rstrGroup = rstrGroup;
	page.cSpells = 0;

	for(sysint i = 0; i < cSpells; i++)
	{
		Check(pSpells->GetObject(i, page.prgSpells + page.cSpells));
		if(++page.cSpells == ARRAYSIZE(page.prgSpells))
		{
			Check(m_aPages.Append(page));
			RStrAddRef(page.rstrGroup);
			page.cSpells = 0;
		}
	}

	if(0 < page.cSpells)
	{
		Check(m_aPages.Append(page));
		RStrAddRef(page.rstrGroup);
	}

Cleanup:
	return hr;
}

HRESULT CSpellBook::CreateSpellBookPage (SPELL_PAGE* pPage, INT x, INT y, __deref_opt_out CSpellBookPage** ppPage)
{
	HRESULT hr;
	TStackRef<CSpellBookPage> srPage;
	
	srPage.Attach(__new CSpellBookPage(pPage, m_pvTitle, m_pvLabel, x, y));
	CheckAlloc(srPage);
	Check(srPage->Initialize(m_pSIF, m_nMagicPower));
	Check(m_pLayer->AddSprite(srPage, NULL));
	if(ppPage)
		*ppPage = srPage.Detach();

Cleanup:
	return hr;
}

BOOL CSpellBook::MouseOverSprite (INT x, INT y, ISimbeyInterchangeSprite* pSprite)
{
	INT xBtn, yBtn, xSize, ySize;

	pSprite->GetFrameOffset(xBtn, yBtn);
	pSprite->GetCurrentFrameSize(&xSize, &ySize);

	return x >= xBtn && y >= yBtn && x < xBtn + xSize && y < yBtn + ySize;
}

BOOL CSpellBook::FindSpellAt (SPELL_PAGE* pPage, INT x, INT y, INT xPage, INT yPage, __deref_out IJSONObject** ppSpell)
{
	if(pPage && x >= xPage && x < xPage + SPELL_BOOK_PAGE_WIDTH && y >= yPage && y < yPage + SPELL_BOOK_PAGE_HEIGHT)
	{
		y -= (yPage + 16);

		if(y >= 0)
		{
			y /= 21;

			if(y < pPage->cSpells)
			{
				SetInterface(*ppSpell, pPage->prgSpells[y]);
				return TRUE;
			}
		}
	}

	return FALSE;
}
