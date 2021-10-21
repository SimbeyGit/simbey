#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Util\DIBDrawing.h"
#include "Library\Sorting.h"
#include "SIFSurface.h"

///////////////////////////////////////////////////////////////////////////////
// CDrawText
///////////////////////////////////////////////////////////////////////////////

CDrawText::CDrawText (ISimbeyInterchangeFileFont* pFont, RSTRING rstrText, BOOL fCenter) :
	m_fCenter(fCenter)
{
	SetInterface(m_pFont, pFont);
	RStrSet(m_rstrText, rstrText);
}

CDrawText::~CDrawText ()
{
	RStrRelease(m_rstrText);
	SafeRelease(m_pFont);
}

// ISimbeyInterchangeSprite

HRESULT CDrawText::SelectAnimation (INT nAnimation, INT nFrame, INT cTicks)
{
	return S_FALSE;
}

VOID CDrawText::GetCurrentAnimation (__out INT* pnAnimation, __out INT* pnFrame, __out INT* pcTicks)
{
}

VOID CDrawText::GetCurrentFrameSize (__out INT* pxSize, __out INT* pySize)
{
}

VOID CDrawText::GetCurrentHitBox (__out RECT* prcHitBox)
{
	ZeroMemory(prcHitBox, sizeof(RECT));
}

VOID CDrawText::UpdateFrameTick (VOID)
{
}

VOID CDrawText::SetPosition (INT x, INT y)
{
	m_x = x;
	m_y = y;
}

VOID CDrawText::GetPosition (__out INT& x, __out INT& y)
{
	x = m_x;
	y = m_y;
}

BOOL CDrawText::DrawMaskedToDIB24 (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24)
{
	return SUCCEEDED(m_pFont->DrawTextToSurface(psifSurface24, RStrToWide(m_rstrText), m_x + xOffset, m_y + yOffset, m_fCenter));
}

BOOL CDrawText::DrawTileToDIB24 (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24, SIF_LINE_OFFSET* pslOffsets)
{
	return FALSE;
}

BOOL CDrawText::DrawBlendedToDIB24 (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24)
{
	return SUCCEEDED(m_pFont->DrawTextToSurface(psifSurface24, RStrToWide(m_rstrText), m_x + xOffset, m_y + yOffset, m_fCenter));
}

BOOL CDrawText::DrawColorizedToDIB24 (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24)
{
	return SUCCEEDED(m_pFont->DrawTextToSurface(psifSurface24, RStrToWide(m_rstrText), m_x + xOffset, m_y + yOffset, m_fCenter));
}

COLORREF CDrawText::GetColorized (VOID)
{
	return 0;
}

BOOL CDrawText::SetColorized (COLORREF cr)
{
	return FALSE;
}

VOID CDrawText::GetFrameOffset (__out INT& x, __out INT& y)
{
	x = 0;
	y = 0;
}

///////////////////////////////////////////////////////////////////////////////
// CDrawSolid
///////////////////////////////////////////////////////////////////////////////

CDrawSolid::CDrawSolid (COLORREF crSolid, INT nWidth, INT nHeight) :
	m_crSolid(crSolid)
{
	m_rcSolid.left = 0;
	m_rcSolid.top = 0;
	m_rcSolid.right = nWidth;
	m_rcSolid.bottom = nHeight;
}

CDrawSolid::~CDrawSolid ()
{
}

// ISimbeyInterchangeSprite

HRESULT CDrawSolid::SelectAnimation (INT nAnimation, INT nFrame, INT cTicks)
{
	return S_FALSE;
}

VOID CDrawSolid::GetCurrentAnimation (__out INT* pnAnimation, __out INT* pnFrame, __out INT* pcTicks)
{
}

VOID CDrawSolid::GetCurrentFrameSize (__out INT* pxSize, __out INT* pySize)
{
}

VOID CDrawSolid::GetCurrentHitBox (__out RECT* prcHitBox)
{
	CopyMemory(prcHitBox, &m_rcSolid, sizeof(RECT));
}

VOID CDrawSolid::UpdateFrameTick (VOID)
{
}

VOID CDrawSolid::SetPosition (INT x, INT y)
{
	INT nWidth = m_rcSolid.right - m_rcSolid.left;
	INT nHeight = m_rcSolid.bottom - m_rcSolid.top;
	m_rcSolid.left = x;
	m_rcSolid.top = y;
	m_rcSolid.right = x + nWidth;
	m_rcSolid.bottom = y + nHeight;
}

VOID CDrawSolid::GetPosition (__out INT& x, __out INT& y)
{
	x = m_rcSolid.left;
	y = m_rcSolid.top;
}

BOOL CDrawSolid::DrawMaskedToDIB24 (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24)
{
	BYTE bAlpha = (BYTE)(m_crSolid >> 24);
	return 0 < bAlpha && DIBDrawing::AlphaFill24(psifSurface24->pbSurface, psifSurface24->xSize, psifSurface24->ySize, psifSurface24->lPitch, &m_rcSolid, m_crSolid, bAlpha);
}

BOOL CDrawSolid::DrawTileToDIB24 (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24, SIF_LINE_OFFSET* pslOffsets)
{
	return FALSE;
}

BOOL CDrawSolid::DrawBlendedToDIB24 (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24)
{
	BYTE bAlpha = (BYTE)(m_crSolid >> 24);
	return 0 < bAlpha && DIBDrawing::AlphaFill24(psifSurface24->pbSurface, psifSurface24->xSize, psifSurface24->ySize, psifSurface24->lPitch, &m_rcSolid, m_crSolid, bAlpha);
}

BOOL CDrawSolid::DrawColorizedToDIB24 (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24)
{
	return FALSE;
}

COLORREF CDrawSolid::GetColorized (VOID)
{
	return 0;
}

BOOL CDrawSolid::SetColorized (COLORREF cr)
{
	return FALSE;
}

COLORREF CDrawSolid::GetSolidColor (VOID)
{
	return m_crSolid;
}

VOID CDrawSolid::SetSolidColor (COLORREF cr)
{
	m_crSolid = cr;
}

VOID CDrawSolid::GetFrameOffset (__out INT& x, __out INT& y)
{
	x = 0;
	y = 0;
}

///////////////////////////////////////////////////////////////////////////////
// CDrawPattern
///////////////////////////////////////////////////////////////////////////////

CDrawPattern::CDrawPattern (ISimbeyInterchangeFileLayer* pLayer, INT nWidth, INT nHeight, BOOL fPinned) :
	m_pLayer(pLayer),
	m_pbTile(NULL),
	m_x(0),
	m_y(0),
	m_nWidth(nWidth),
	m_nHeight(nHeight),
	m_fPinned(fPinned)
{
	m_pLayer->AddRef();
}

CDrawPattern::~CDrawPattern ()
{
	m_pLayer->Release();
}

HRESULT CDrawPattern::Initialize (VOID)
{
	DWORD cbTile;

	// Pre-load the tile.
	return m_pLayer->GetBitsPtr(&m_pbTile, &cbTile);
}

// ISimbeyInterchangeSprite

HRESULT CDrawPattern::SelectAnimation (INT nAnimation, INT nFrame, INT cTicks)
{
	return S_FALSE;
}

VOID CDrawPattern::GetCurrentAnimation (__out INT* pnAnimation, __out INT* pnFrame, __out INT* pcTicks)
{
}

VOID CDrawPattern::GetCurrentFrameSize (__out INT* pxSize, __out INT* pySize)
{
}

VOID CDrawPattern::GetCurrentHitBox (__out RECT* prcHitBox)
{
	ZeroMemory(prcHitBox, sizeof(RECT));
}

VOID CDrawPattern::UpdateFrameTick (VOID)
{
}

VOID CDrawPattern::SetPosition (INT x, INT y)
{
	m_x = x;
	m_y = y;
}

VOID CDrawPattern::GetPosition (__out INT& x, __out INT& y)
{
	x = m_x;
	y = m_y;
}

BOOL CDrawPattern::DrawMaskedToDIB24 (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24)
{
	PBYTE pbView;
	INT xView, yView, xTile, yTile;
	CalculateView(psifSurface24, &pbView, xOffset, yOffset, xView, yView, xTile, yTile);

	PBYTE pbTile = m_pbTile;
	for(INT y = yOffset; y < yView; y += yTile)
	{
		for(INT x = xOffset; x < xView; x += xTile)
		{
			sifDrawMaskedBits32ToDIB24Pitch(pbView, x, y, xView, yView, psifSurface24->lPitch, pbTile, xTile, yTile);
		}
	}

	return TRUE;
}

BOOL CDrawPattern::DrawTileToDIB24 (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24, SIF_LINE_OFFSET* pslOffsets)
{
	return FALSE;
}

BOOL CDrawPattern::DrawBlendedToDIB24 (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24)
{
	return FALSE;
}

BOOL CDrawPattern::DrawColorizedToDIB24 (INT xOffset, INT yOffset, SIF_SURFACE* psifSurface24)
{
	PBYTE pbView;
	INT xView, yView, xTile, yTile;
	CalculateView(psifSurface24, &pbView, xOffset, yOffset, xView, yView, xTile, yTile);

	PBYTE pbTile = m_pbTile;
	for(INT y = yOffset; y < yView; y += yTile)
	{
		for(INT x = xOffset; x < xView; x += xTile)
		{
			sifDrawColorizedBits32ToDIB24Pitch(pbView, x, y, xView, yView, psifSurface24->lPitch, pbTile, xTile, yTile, m_crColorize);
		}
	}

	return TRUE;
}

COLORREF CDrawPattern::GetColorized (VOID)
{
	return m_crColorize;
}

BOOL CDrawPattern::SetColorized (COLORREF cr)
{
	m_crColorize = cr;
	return TRUE;
}

VOID CDrawPattern::GetFrameOffset (__out INT& x, __out INT& y)
{
	x = 0;
	y = 0;
}

VOID CDrawPattern::CalculateView (SIF_SURFACE* psifSurface24, __out PBYTE* ppbView, INT& xOffset, INT& yOffset, INT& xView, INT& yView, INT& xTile, INT& yTile)
{
	INT xStart = m_x + xOffset;
	INT yStart = m_y + yOffset;
	INT xSurface = psifSurface24->xSize;
	INT ySurface = psifSurface24->ySize;
	PBYTE pbSurface = psifSurface24->pbSurface + psifSurface24->lPitch * (ySurface - (yStart + m_nHeight)) + xStart * 3;

	RECT rcLayer;
	m_pLayer->GetPosition(&rcLayer);

	xTile = rcLayer.right - rcLayer.left;
	yTile = rcLayer.bottom - rcLayer.top;

	if(m_fPinned)
	{
		xOffset = 0;
		yOffset = 0;
	}
	else
	{
		xOffset = 0 - (xStart % xTile);
		yOffset = 0 - (yStart % yTile);
	}

	xView = m_nWidth;
	yView = m_nHeight;

	if(xStart < 0)
	{
		pbSurface += -xStart * 3;

		xView += xStart;
		xOffset += xStart;

		xStart = 0;
	}
	if(yStart < 0)
	{
		// No adjustment to pbSurface, because it's upside down.

		yView += yStart;
		yOffset += yStart;

		yStart = 0;
	}

	if(xStart + xView > xSurface)
		xView = xSurface - xStart;
	if(yStart + yView > ySurface)
	{
		yView = ySurface - yStart;

		// We do need an adjustment here, because it's upside down.
		pbSurface += psifSurface24->lPitch * (m_nHeight - yView);
	}

	*ppbView = pbSurface;
}

///////////////////////////////////////////////////////////////////////////////
// CSIFCanvas
///////////////////////////////////////////////////////////////////////////////

CSIFCanvas::CSIFCanvas () :
	m_xScroll(0),
	m_yScroll(0)
{
	ZeroMemory(&m_sifSurface, sizeof(m_sifSurface));
}

CSIFCanvas::~CSIFCanvas ()
{
	for(sysint i = 0; i < m_aLayers.Length(); i++)
	{
		LAYER* pLayer = m_aLayers[i];
		for(sysint n = 0; n < pLayer->aSprites.Length(); n++)
			pLayer->aSprites[n]->Release();
		__delete pLayer;
	}
}

VOID CSIFCanvas::Configure (const SIF_SURFACE* pcsifSurface)
{
	CopyMemory(&m_sifSurface, pcsifSurface, sizeof(SIF_SURFACE));
}

VOID CSIFCanvas::Redraw (VOID)
{
	for(sysint i = 0; i < m_aLayers.Length(); i++)
		DrawLayer(m_aLayers[i]);
}

VOID CSIFCanvas::SetScroll (INT xScroll, INT yScroll)
{
	m_xScroll = xScroll;
	m_yScroll = yScroll;
}

VOID CSIFCanvas::GetScroll (__out INT* pxScroll, __out INT* pyScroll)
{
	*pxScroll = m_xScroll;
	*pyScroll = m_yScroll;
}

VOID CSIFCanvas::OffsetPoint (__inout POINT& pt)
{
	pt.x += m_xScroll;
	pt.y += m_yScroll;
}

VOID CSIFCanvas::GetViewSize (__out INT* pxView, __out INT* pyView)
{
	*pxView = m_sifSurface.xSize;
	*pyView = m_sifSurface.ySize;
}

VOID CSIFCanvas::Tick (VOID)
{
	for(sysint i = 0; i < m_aLayers.Length(); i++)
	{
		LAYER* pLayer = m_aLayers[i];
		if(pLayer->fPerformTicks)
		{
			sysint cSprites = pLayer->aSprites.Length();
			for(sysint i = 0; i < cSprites; i++)
			{
				pLayer->aSprites[i]->UpdateFrameTick();
			}
		}
	}
}

HRESULT CSIFCanvas::AddLayer (BOOL fPerformTicks, BOOL fColorized, COLORREF crFill, __out_opt sysint* pnLayer)
{
	HRESULT hr;
	LAYER* pLayer = __new LAYER;

	CheckAlloc(pLayer);

	if(pnLayer)
		*pnLayer = m_aLayers.Length();

	pLayer->fPerformTicks = fPerformTicks;
	pLayer->fColorized = fColorized;
	pLayer->crFill = crFill;
	Check(m_aLayers.Append(pLayer));
	pLayer = NULL;

Cleanup:
	__delete pLayer;
	return hr;
}

VOID CSIFCanvas::ClearLayer (sysint nLayer)
{
	LAYER* pLayer;

	pLayer = m_aLayers[nLayer];
	for(sysint i = 0; i < pLayer->aSprites.Length(); i++)
		pLayer->aSprites[i]->Release();
	pLayer->aSprites.Clear();
}

HRESULT CSIFCanvas::AddSprite (sysint nLayer, ISimbeyInterchangeSprite* pSprite, __out_opt sysint* pnSprite)
{
	HRESULT hr;
	LAYER* pLayer;

	CheckIf(nLayer >= m_aLayers.Length(), E_INVALIDARG);
	pLayer = m_aLayers[nLayer];

	if(pnSprite)
		*pnSprite = pLayer->aSprites.Length();

	Check(pLayer->aSprites.Append(pSprite));
	pSprite->AddRef();

Cleanup:
	return hr;
}

HRESULT CSIFCanvas::AddSpriteAfter (sysint nLayer, ISimbeyInterchangeSprite* pSprite, ISimbeyInterchangeSprite* pInsertAfter, __out_opt sysint* pnSprite)
{
	HRESULT hr;
	LAYER* pLayer;
	sysint nInsert;

	if(pInsertAfter)
	{
		Check(FindSprite(nLayer, pInsertAfter, &nInsert));
		CheckIf(S_OK != hr, HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
		nInsert++;
	}
	else
		nInsert = 0;
	pLayer = m_aLayers[nLayer];

	if(pnSprite)
		*pnSprite = nInsert;

	Check(pLayer->aSprites.InsertAt(pSprite, nInsert));
	pSprite->AddRef();

Cleanup:
	return hr;
}

HRESULT CSIFCanvas::RemoveSprite (sysint nLayer, ISimbeyInterchangeSprite* pSprite)
{
	HRESULT hr;
	sysint nSprite;

	Check(FindSprite(nLayer, pSprite, &nSprite));
	if(S_OK == hr)
	{
		m_aLayers[nLayer]->aSprites.Remove(nSprite, &pSprite);
		pSprite->Release();
	}

Cleanup:
	return hr;
}

HRESULT CSIFCanvas::FindSprite (sysint nLayer, ISimbeyInterchangeSprite* pSprite, __out_opt sysint* pnSprite)
{
	HRESULT hr;
	TArray<ISimbeyInterchangeSprite*>* paSprites;

	CheckTrue(0 <= nLayer && nLayer < m_aLayers.Length(), E_INVALIDARG);

	paSprites = &m_aLayers[nLayer]->aSprites;
	for(sysint i = 0; i < paSprites->Length(); i++)
	{
		if(pSprite == (*paSprites)[i])
		{
			if(pnSprite)
				*pnSprite = i;
			hr = S_OK;
			goto Cleanup;
		}
	}

	hr = S_FALSE;

Cleanup:
	return hr;
}

HRESULT CSIFCanvas::FindSpriteAt (sysint nLayer, INT x, INT y, __deref_out ISimbeyInterchangeSprite** ppSprite)
{
	HRESULT hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	TArray<ISimbeyInterchangeSprite*>* paSprites;

	CheckTrue(0 <= nLayer && nLayer < m_aLayers.Length(), E_INVALIDARG);

	paSprites = &m_aLayers[nLayer]->aSprites;
	for(sysint i = 0; i < paSprites->Length(); i++)
	{
		ISimbeyInterchangeSprite* pSprite = (*paSprites)[i];
		INT xSprite, ySprite;

		pSprite->GetPosition(xSprite, ySprite);
		if(xSprite == x && ySprite == y)
		{
			if(ppSprite)
			{
				*ppSprite = pSprite;
				pSprite->AddRef();
			}
			hr = S_OK;
		}
	}

Cleanup:
	return hr;
}

HRESULT CSIFCanvas::BringSpriteToTop (sysint nLayer, ISimbeyInterchangeSprite* pSprite)
{
	HRESULT hr;
	TArray<ISimbeyInterchangeSprite*>* paSprites;

	CheckTrue(0 <= nLayer && nLayer < m_aLayers.Length(), E_INVALIDARG);

	paSprites = &m_aLayers[nLayer]->aSprites;
	for(sysint i = 0; i < paSprites->Length(); i++)
	{
		if(pSprite == (*paSprites)[i])
		{
			if(i == paSprites->Length() - 1)
				break;
			Check(paSprites->Append(pSprite));
			paSprites->Remove(i, NULL);
			goto Cleanup;
		}
	}

	hr = S_FALSE;

Cleanup:
	return hr;
}

HRESULT CSIFCanvas::RemoveHiddenSprites (sysint nLayer)
{
	HRESULT hr;

	TArray<ISimbeyInterchangeSprite*>* paSprites;

	CheckTrue(0 <= nLayer && nLayer < m_aLayers.Length(), E_INVALIDARG);

	paSprites = &m_aLayers[nLayer]->aSprites;
	for(sysint i = paSprites->Length() - 1; i >= 0; i--)
	{
		INT x, y, xOffset, yOffset, xSize, ySize;
		ISimbeyInterchangeSprite* pSprite = (*paSprites)[i];

		pSprite->GetPosition(x, y);
		pSprite->GetFrameOffset(xOffset, yOffset);

		x -= xOffset;
		y -= yOffset;

		pSprite->GetCurrentFrameSize(&xSize, &ySize);

		x -= m_xScroll;
		y -= m_yScroll;

		if(x + xSize <= 0 || y + ySize <= 0 || x >= m_sifSurface.xSize || y >= m_sifSurface.ySize)
		{
			paSprites->Remove(i, NULL);
			pSprite->Release();
		}
	}

	hr = S_OK;

Cleanup:
	return hr;
}

HRESULT CSIFCanvas::ReplaceSprite (sysint nLayer, ISimbeyInterchangeSprite* pReplace, ISimbeyInterchangeSprite* pSprite)
{
	HRESULT hr;
	sysint nSprite;
	TArray<ISimbeyInterchangeSprite*>* paSprites;

	Check(FindSprite(nLayer, pReplace, &nSprite));
	paSprites = &m_aLayers[nLayer]->aSprites;
	ReplaceInterface((*paSprites)[nSprite], pSprite);

Cleanup:
	return hr;
}

HRESULT CSIFCanvas::SortLayer (sysint nLayer, INT (WINAPI* pfnCallback)(ISimbeyInterchangeSprite** ppSpriteA, ISimbeyInterchangeSprite** ppSpriteB, PVOID pParam), PVOID pvParam)
{
	HRESULT hr;

	CheckTrue(0 <= nLayer && nLayer < m_aLayers.Length(), E_INVALIDARG);
	Sorting::TQuickSortTArrayPtr<ISimbeyInterchangeSprite>(&m_aLayers[nLayer]->aSprites, pfnCallback, pvParam);
	hr = S_OK;

Cleanup:
	return hr;
}

VOID CSIFCanvas::DrawLayer (LAYER* pLayer)
{
	sysint cSprites = pLayer->aSprites.Length();

	if(pLayer->crFill)
	{
		RECT rc = { 0, 0, m_sifSurface.xSize, m_sifSurface.ySize };
		DIBDrawing::AlphaFill24(m_sifSurface.pbSurface, m_sifSurface.xSize, m_sifSurface.ySize, m_sifSurface.lPitch, &rc, pLayer->crFill, pLayer->crFill >> 24);
	}

	if(pLayer->fColorized)
	{
		for(sysint i = 0; i < cSprites; i++)
			pLayer->aSprites[i]->DrawColorizedToDIB24(m_xScroll, m_yScroll, &m_sifSurface);
	}
	else
	{
		for(sysint i = 0; i < cSprites; i++)
			pLayer->aSprites[i]->DrawMaskedToDIB24(m_xScroll, m_yScroll, &m_sifSurface);
	}
}

///////////////////////////////////////////////////////////////////////////////
// CSIFSurface
///////////////////////////////////////////////////////////////////////////////

CSIFSurface::CSIFSurface (INT xSize, INT ySize) :
	m_xSurface(0),
	m_ySurface(0),
	m_xStretchSize(0),
	m_yStretchSize(0),
	m_yPosition(0),
	m_pvBits(NULL),
	m_hbmBuffer(NULL),
	m_fClear(FALSE),
	m_crClear(0),
	m_cUnpainted(0)
{
	ZeroMemory(&m_bmi, sizeof(m_bmi));
	m_bmi.bmiHeader.biWidth = xSize;
	m_bmi.bmiHeader.biHeight = ySize;
}

CSIFSurface::~CSIFSurface ()
{
	Destroy();
}

VOID CSIFSurface::EnableClear (COLORREF crClear)
{
	m_fClear = TRUE;
	m_crClear = crClear;
}

VOID CSIFSurface::DisableClear (VOID)
{
	m_fClear = FALSE;
}

VOID CSIFSurface::SetOffset (INT yPosition)
{
	m_yPosition = yPosition;
}

VOID CSIFSurface::Position (INT xWin, INT yWin)
{
	INT xSize, ySize;

	ySize = yWin;
	xSize = ySize * m_bmi.bmiHeader.biWidth / m_bmi.bmiHeader.biHeight;

	if(xSize > xWin)
	{
		xSize = xWin;
		ySize = xSize * m_bmi.bmiHeader.biHeight / m_bmi.bmiHeader.biWidth;
	}

	m_xSurface = xWin / 2 - xSize / 2;
	m_ySurface = yWin / 2 - ySize / 2;
	m_xStretchSize = xSize;
	m_yStretchSize = ySize;

	m_cUnpainted = 0;
	if(xSize < xWin)
	{
		m_rcUnpainted[m_cUnpainted].left = 0;
		m_rcUnpainted[m_cUnpainted].right = m_xSurface;
		m_rcUnpainted[m_cUnpainted].top = 0;
		m_rcUnpainted[m_cUnpainted].bottom = ySize;
		if(m_rcUnpainted[m_cUnpainted].right > m_rcUnpainted[m_cUnpainted].left)
			m_cUnpainted++;

		m_rcUnpainted[m_cUnpainted].left = m_xSurface + xSize;
		m_rcUnpainted[m_cUnpainted].right = xWin;
		m_rcUnpainted[m_cUnpainted].top = 0;
		m_rcUnpainted[m_cUnpainted].bottom = ySize;
		if(m_rcUnpainted[m_cUnpainted].right > m_rcUnpainted[m_cUnpainted].left)
			m_cUnpainted++;
	}
	else if(ySize < yWin)
	{
		m_rcUnpainted[m_cUnpainted].top = 0;
		m_rcUnpainted[m_cUnpainted].bottom = m_ySurface;
		m_rcUnpainted[m_cUnpainted].left = 0;
		m_rcUnpainted[m_cUnpainted].right = xSize;
		if(m_rcUnpainted[m_cUnpainted].bottom > m_rcUnpainted[m_cUnpainted].top)
			m_cUnpainted++;

		m_rcUnpainted[m_cUnpainted].top = m_ySurface + ySize;
		m_rcUnpainted[m_cUnpainted].bottom = yWin;
		m_rcUnpainted[m_cUnpainted].left = 0;
		m_rcUnpainted[m_cUnpainted].right = xSize;
		if(m_rcUnpainted[m_cUnpainted].bottom > m_rcUnpainted[m_cUnpainted].top)
			m_cUnpainted++;
	}

	for(INT i = 0; i < m_cUnpainted; i++)
	{
		m_rcUnpainted[i].top += m_yPosition;
		m_rcUnpainted[i].bottom += m_yPosition;
	}

	m_ySurface += m_yPosition;
}

VOID CSIFSurface::Tick (VOID)
{
	for(sysint i = 0; i < m_aCanvas.Length(); i++)
		m_aCanvas[i].pCanvas->Tick();
}

VOID CSIFSurface::Redraw (HWND hwnd, __in_opt HDC hdc)
{
	BOOL fOpenedDC = FALSE;

	if(hdc)
	{
		if(NULL == m_hbmBuffer)
			BuildSurface(hdc);
	}
	else
	{
		fOpenedDC = TRUE;
		hdc = GetDC(hwnd);
	}

	if(m_fClear)
	{
		RECT rc = { 0, 0, m_bmi.bmiHeader.biWidth, m_bmi.bmiHeader.biHeight };
		DIBDrawing::AlphaFill24(reinterpret_cast<PBYTE>(m_pvBits), m_bmi.bmiHeader.biWidth, m_bmi.bmiHeader.biHeight,
			((m_bmi.bmiHeader.biWidth * 3) + 3) & ~3, &rc, m_crClear, 255);
	}

	for(sysint i = 0; i < m_aCanvas.Length(); i++)
		m_aCanvas[i].pCanvas->Redraw();

	StretchDIBits(hdc, m_xSurface, m_ySurface, m_xStretchSize, m_yStretchSize, 0, 0,
		m_bmi.bmiHeader.biWidth, m_bmi.bmiHeader.biHeight,
		m_pvBits, &m_bmi, DIB_RGB_COLORS, SRCCOPY);

	if(fOpenedDC)
		ReleaseDC(hwnd, hdc);
}

VOID CSIFSurface::Destroy (VOID)
{
	for(sysint i = m_aCanvas.Length() - 1; i >= 0; i--)
		RemoveCanvas(m_aCanvas[i].pCanvas);

	m_pvBits = NULL;
	SafeDeleteGdiObject(m_hbmBuffer);
}

VOID CSIFSurface::GetViewSize (__out INT* pxView, __out INT* pyView)
{
	*pxView = m_bmi.bmiHeader.biWidth;
	*pyView = m_bmi.bmiHeader.biHeight;
}

VOID CSIFSurface::GetUnpaintedRects (__deref_out const RECT** pprcUnpainted, __out INT* pcUnpainted)
{
	*pprcUnpainted = m_rcUnpainted;
	*pcUnpainted = m_cUnpainted;
}

VOID CSIFSurface::TranslateClientPointToView (INT x, INT y, __deref_out_opt CSIFCanvas** ppCanvas, __out INT* pxView, __out INT* pyView)
{
	POINT pt;
	CSIFCanvas* pCanvas = NULL;
	pt.x = MulDiv(x - m_xSurface, m_bmi.bmiHeader.biWidth, m_xStretchSize);
	pt.y = MulDiv(y - m_ySurface, m_bmi.bmiHeader.biHeight, m_yStretchSize);

	for(sysint i = m_aCanvas.Length() - 1; i >= 0; i--)
	{
		CANVAS_DESC& cd = m_aCanvas[i];
		if(cd.fInteractive && PtInRect(&cd.rcCanvas, pt))
		{
			// Translate for the canvas position.
			pt.x -= cd.rcCanvas.left;
			pt.y -= cd.rcCanvas.top;

			// Add the scroll offset.
			cd.pCanvas->OffsetPoint(pt);

			pCanvas = cd.pCanvas;
			break;
		}
	}

	*pxView = pt.x;
	*pyView = pt.y;
	*ppCanvas = pCanvas;
}

VOID CSIFSurface::TranslateClientPointToCanvas (INT x, INT y, CSIFCanvas* pCanvas, __out INT* pxView, __out INT* pyView)
{
	POINT pt;
	pt.x = MulDiv(x - m_xSurface, m_bmi.bmiHeader.biWidth, m_xStretchSize);
	pt.y = MulDiv(y - m_ySurface, m_bmi.bmiHeader.biHeight, m_yStretchSize);

	for(sysint i = m_aCanvas.Length() - 1; i >= 0; i--)
	{
		CANVAS_DESC& cd = m_aCanvas[i];
		if(pCanvas == cd.pCanvas)
		{
			// Translate for the canvas position.
			pt.x -= cd.rcCanvas.left;
			pt.y -= cd.rcCanvas.top;

			// Add the scroll offset.
			cd.pCanvas->OffsetPoint(pt);
			break;
		}
	}

	*pxView = pt.x;
	*pyView = pt.y;
}

HRESULT CSIFSurface::AddCanvas (__in_opt const RECT* pcrcCanvas, BOOL fInteractive, __out CSIFCanvas** ppCanvas)
{
	HRESULT hr;
	CANVAS_DESC* pcd;
	CSIFCanvas* pCanvas = NULL;
	RECT rcView;

	if(pcrcCanvas)
	{
		CheckIf(pcrcCanvas->left < 0 || pcrcCanvas->top < 0, E_INVALIDARG);
		CheckIf(pcrcCanvas->right > m_bmi.bmiHeader.biWidth || pcrcCanvas->bottom > m_bmi.bmiHeader.biHeight, E_INVALIDARG);
	}
	else
	{
		rcView.left = 0;
		rcView.top = 0;
		rcView.right = m_bmi.bmiHeader.biWidth;
		rcView.bottom = m_bmi.bmiHeader.biHeight;
		pcrcCanvas = &rcView;
	}

	Check(AllocCanvas(&pCanvas));

	Check(m_aCanvas.AppendSlot(&pcd));
	CopyMemory(&pcd->rcCanvas, pcrcCanvas, sizeof(RECT));
	pcd->fInteractive = fInteractive;
	pcd->pCanvas = pCanvas;

	*ppCanvas = pCanvas;
	pCanvas = NULL;

	ReconfigureAll();

Cleanup:
	if(pCanvas)
		FreeCanvas(pCanvas);
	return hr;
}

HRESULT CSIFSurface::RemoveCanvas (CSIFCanvas* pCanvas)
{
	HRESULT hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

	for(sysint i = m_aCanvas.Length() - 1; i >= 0; i--)
	{
		if(pCanvas == m_aCanvas[i].pCanvas)
		{
			m_aCanvas.Remove(i, NULL);
			FreeCanvas(pCanvas);
			hr = S_OK;
			break;
		}
	}

	return hr;
}

DOUBLE CSIFSurface::GetStretchScale (VOID)
{
	return (DOUBLE)m_xStretchSize / (DOUBLE)m_bmi.bmiHeader.biWidth;
}

HRESULT CSIFSurface::BuildSurface (HDC hdc)
{
	HRESULT hr = sifCreateBlankDIB(hdc, m_bmi.bmiHeader.biWidth, m_bmi.bmiHeader.biHeight, 24, &m_pvBits, &m_hbmBuffer);
	if(SUCCEEDED(hr))
	{
		m_bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		m_bmi.bmiHeader.biPlanes = 1;
		m_bmi.bmiHeader.biCompression = BI_RGB;
		m_bmi.bmiHeader.biBitCount = 24;

		ReconfigureAll();
	}
	return hr;
}

VOID CSIFSurface::ReconfigureAll (VOID)
{
	SIF_SURFACE sifSurface;
	sifSurface.cBitsPerPixel = m_bmi.bmiHeader.biBitCount;
	sifSurface.lPitch = ((m_bmi.bmiHeader.biWidth * 3) + 3) & ~3;
	for(sysint i = 0; i < m_aCanvas.Length(); i++)
	{
		CANVAS_DESC& cd = m_aCanvas[i];
		sifSurface.xSize = cd.rcCanvas.right - cd.rcCanvas.left;
		sifSurface.ySize = cd.rcCanvas.bottom - cd.rcCanvas.top;
		sifSurface.pbSurface = reinterpret_cast<PBYTE>(m_pvBits) + sifSurface.lPitch * (m_bmi.bmiHeader.biHeight - cd.rcCanvas.bottom) + cd.rcCanvas.left * 3;
		cd.pCanvas->Configure(&sifSurface);
	}
}

HRESULT CSIFSurface::AllocCanvas (__deref_out CSIFCanvas** ppCanvas)
{
	HRESULT hr;

	*ppCanvas = __new CSIFCanvas;
	CheckAlloc(*ppCanvas);
	hr = S_OK;

Cleanup:
	return hr;
}

VOID CSIFSurface::FreeCanvas (CSIFCanvas* pCanvas)
{
	__delete pCanvas;
}

///////////////////////////////////////////////////////////////////////////////
// CIsometricTranslator
///////////////////////////////////////////////////////////////////////////////

CIsometricTranslator::CIsometricTranslator (INT xTile, INT yTile) :
	m_xTile(xTile),
	m_xHalf(xTile / 2),
	m_yTile(yTile),
	m_yHalf(yTile / 2)
{
}

VOID CIsometricTranslator::ViewToIsometric (CSIFCanvas* pCanvas, INT xView, INT yView, __out INT* pxIso, __out INT* pyIso)
{
	INT xSize, ySize;

	pCanvas->GetViewSize(&xSize, &ySize);
	*pxIso = xView - (xSize >> 1);
	*pyIso = yView - (ySize >> 1);
	*pxIso -= m_xHalf;
}

VOID CIsometricTranslator::IsometricToView (CSIFCanvas* pCanvas, INT xIso, INT yIso, __out INT* pxView, __out INT* pyView)
{
	INT xSize, ySize;

	pCanvas->GetViewSize(&xSize, &ySize);
	*pxView = xIso + (xSize >> 1);
	*pyView = yIso + (ySize >> 1);
}

VOID CIsometricTranslator::TileToView (int x, int y, __out int* pxView, __out int* pyView)
{
	*pxView = x * m_xHalf + y * -m_yTile;
	*pyView = x * m_yHalf + y * m_yHalf;
}

VOID CIsometricTranslator::ViewToTile (int x, int y, __out int* pxTile, __out int* pyTile)
{
	*pxTile = (x + 2 * y) / m_xTile;
	*pyTile = (2 * y - x) / m_xTile;
}

VOID CIsometricTranslator::GetTileRange (CSIFCanvas* pCanvas, __out INT* pxTileStart, __out INT* pyTileStart, __out INT* pxTileEnd, __out INT* pyTileEnd)
{
	INT xSize, ySize, xScroll, yScroll;

	pCanvas->GetViewSize(&xSize, &ySize);

	pCanvas->GetScroll(&xScroll, &yScroll);
	xScroll -= xSize >> 1;
	yScroll -= ySize >> 1;

	ViewToTile(xScroll + xSize, yScroll, pxTileStart, pyTileStart);
	ViewToTile(xScroll, yScroll + ySize, pxTileEnd, pyTileEnd);
	*pxTileStart -= 2;
	*pxTileEnd += 2;
	*pyTileStart--;
	*pyTileEnd += 2;
}

HRESULT CIsometricTranslator::SortIsometricLayer (CSIFCanvas* pCanvas, sysint nLayer)
{
	return pCanvas->SortLayer(nLayer, SortIsometric, this);
}

INT WINAPI CIsometricTranslator::SortIsometric (ISimbeyInterchangeSprite** ppSpriteA, ISimbeyInterchangeSprite** ppSpriteB, PVOID pvParam)
{
	CIsometricTranslator* pIso = reinterpret_cast<CIsometricTranslator*>(pvParam);
	INT xA, yA, xB, yB, xTileA, yTileA, xTileB, yTileB;

	(*ppSpriteA)->GetPosition(xA, yA);
	pIso->ViewToTile(xA, yA, &xTileA, &yTileA);

	(*ppSpriteB)->GetPosition(xB, yB);
	pIso->ViewToTile(xB, yB, &xTileB, &yTileB);

	// Compare the tile positions
	if(yTileA < yTileB)
		return -1;
	if(yTileA > yTileB)
		return 1;
	if(xTileA < xTileB)
		return -1;
	if(xTileA > xTileB)
		return 1;

	// Compare the raw Y coordinates
	if(yA < yB)
		return -1;
	if(yA > yB)
		return 1;

	// Compare the raw X coodinates (but use reversed results)
	if(xA < xB)
		return 1;
	if(xA > xB)
		return -1;

	return 0;
}
