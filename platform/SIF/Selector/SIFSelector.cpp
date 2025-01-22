#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "SIFSelector.h"

#define	ITEM_PADDING			6
#define	ITEM_MARGIN				4

VOID FitBitmapToRect (HDC hdc, const RECT& rcFrame, HDC hdcBitmap, LONG xBitmap, LONG yBitmap)
{
	SIZE sizeDest;
	LONG nFrameWidth = rcFrame.right - rcFrame.left;
	LONG nFrameHeight = rcFrame.bottom - rcFrame.top;

	sizeDest.cx = min(nFrameWidth, xBitmap);
	sizeDest.cy = min(nFrameHeight, yBitmap);

	FLOAT rWidthRatio = static_cast<FLOAT>(xBitmap) / static_cast<FLOAT>(sizeDest.cx);
	FLOAT rHeightRatio = static_cast<FLOAT>(yBitmap) / static_cast<FLOAT>(sizeDest.cy);

	if(rWidthRatio > rHeightRatio)
		sizeDest.cy = static_cast<LONG>(static_cast<FLOAT>(yBitmap) / rWidthRatio);
	else
		sizeDest.cx = static_cast<LONG>(static_cast<FLOAT>(xBitmap) / rHeightRatio);

	StretchBlt(hdc,
		rcFrame.left + (nFrameWidth - sizeDest.cx) / 2,
		rcFrame.top + (nFrameHeight - sizeDest.cy) / 2,
		sizeDest.cx, sizeDest.cy, hdcBitmap, 0, 0, xBitmap, yBitmap, SRCCOPY);
}

CScrollableObject::CScrollableObject () :
	m_yPosition(0),
	m_ySize(0)
{
}

BOOL CScrollableObject::IsVisible (LONG nHeight, LONG yScroll, __out LONG* pyStart)
{
	LONG yStart = m_yPosition - yScroll;
	LONG yEnd = yStart + m_ySize;
	if((yStart < 0 && yEnd > 0) || (yStart >= 0 && yStart < nHeight))
	{
		*pyStart = yStart;
		return TRUE;
	}
	return FALSE;
}

BOOL CScrollableObject::IsPositionWithin (LONG yPosition)
{
	return yPosition >= m_yPosition && yPosition < m_yPosition + m_ySize;
}

///////////////////////////////////////////////////////////////////////////////
// CSIFTitle
///////////////////////////////////////////////////////////////////////////////

CSIFTitle::CSIFTitle (RSTRING rstrTitle)
{
	RStrSet(m_rstrTitle, rstrTitle);
}

CSIFTitle::~CSIFTitle ()
{
	RStrRelease(m_rstrTitle);
}

VOID CSIFTitle::Layout (LONG nWidth, LONG yOffset, const ITEM_SIZING& sizing, __out LONG* pyFinalOffset)
{
	m_yPosition = yOffset + ITEM_MARGIN;
	m_ySize = sizing.yTextHeight * 2;
	*pyFinalOffset = m_yPosition + m_ySize + ITEM_MARGIN;
}

VOID CSIFTitle::Paint (HDC hdc, LONG nWidth, LONG nHeight, LONG yScroll, LOGFONT& logFont)
{
	LONG yStart;
	if(IsVisible(nHeight, yScroll, &yStart))
	{
		HFONT hOldFont;
		RECT rcText;

		logFont.lfHeight = m_ySize - 2;
		logFont.lfWeight = FW_SEMIBOLD;
		hOldFont = (HFONT)SelectObject(hdc, CreateFontIndirect(&logFont));

		rcText.left = ITEM_MARGIN;
		rcText.top = yStart;
		rcText.right = nWidth - ITEM_MARGIN;
		rcText.bottom = yStart + m_ySize;
		DrawText(hdc, RStrToWide(m_rstrTitle), RStrLen(m_rstrTitle), &rcText, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

		DeleteObject(SelectObject(hdc, hOldFont));
	}
}

///////////////////////////////////////////////////////////////////////////////
// CSIFRow
///////////////////////////////////////////////////////////////////////////////

CSIFRow::CSIFRow () :
	m_cItems(0),
	m_pItems(NULL)
{
}

CSIFRow::~CSIFRow ()
{
	for(LONG n = 0; n < m_cItems; n++)
	{
		SIF_ITEM* pItem = m_pItems + n;
		SafeDeleteGdiObject(pItem->hbmCache);
		RStrRelease(pItem->rstrLabel);
		SafeRelease(pItem->pLayer);
	}
	SafeDeleteArray(m_pItems);
}

HRESULT CSIFRow::Layout (LONG nWidth, LONG yOffset, const ITEM_SIZING& sizing, ISimbeyInterchangeFile* pSIF, DWORD nOffset, __out LONG* pyFinalOffset)
{
	HRESULT hr;
	LONG x = ITEM_MARGIN;

	m_sizing = sizing;

	m_pItems = __new SIF_ITEM[m_cItems];
	CheckAlloc(m_pItems);
	ZeroMemory(m_pItems, sizeof(SIF_ITEM) * m_cItems);

	for(LONG n = 0; n < m_cItems; n++)
	{
		SIF_ITEM* pItem = m_pItems + n;
		WCHAR wzLabel[MAX_PATH];

		Check(pSIF->GetLayerByIndex(nOffset + n, &pItem->pLayer));
		Check(pItem->pLayer->GetName(wzLabel, ARRAYSIZE(wzLabel)));
		Check(RStrCreateW(TStrLenAssert(wzLabel), wzLabel, &pItem->rstrLabel));

		pItem->xPos = x;
		x += sizing.xItemSize + ITEM_PADDING * 2 + ITEM_MARGIN;
	}

	m_yPosition = yOffset;
	m_ySize = sizing.yItemSize + sizing.yTextHeight + ITEM_PADDING * 2;

	*pyFinalOffset = m_yPosition + m_ySize + ITEM_MARGIN;

Cleanup:
	return hr;
}

VOID CSIFRow::Paint (HDC hdc, LONG nHeight, LONG yScroll, SIF_ITEM* pSelected)
{
	LONG yStart;
	if(IsVisible(nHeight, yScroll, &yStart))
	{
		HDC hdcBuffer = CreateCompatibleDC(hdc);
		RECT rcItem, rcLabel;

		rcItem.top = yStart + ITEM_PADDING;
		rcItem.bottom = yStart + m_sizing.yItemSize;

		rcLabel.top = rcItem.bottom;
		rcLabel.bottom = rcLabel.top + m_sizing.yTextHeight;

		for(LONG i = 0; i < m_cItems; i++)
		{
			SIF_ITEM* pItem = m_pItems + i;
			if(NULL == pItem->hbmCache)
			{
				if(FAILED(pItem->pLayer->GetAsDDB(hdc, &pItem->hbmCache)))
					break;
			}

			RECT rcLayer;
			SideAssertHr(pItem->pLayer->GetPosition(&rcLayer));

			rcItem.left = pItem->xPos + ITEM_PADDING;
			rcItem.right = rcItem.left + m_sizing.xItemSize;

			if(pItem == pSelected)
			{
				HPEN hPenPrev = (HPEN)SelectObject(hdc, CreatePen(PS_SOLID, 2, RGB(0, 162, 232)));
				HBRUSH hBrushPrev = (HBRUSH)SelectObject(hdc, CreateSolidBrush(RGB(120, 200, 255)));
				RoundRect(hdc, rcItem.left - ITEM_PADDING, rcItem.top - ITEM_PADDING, rcItem.right + ITEM_PADDING, rcLabel.bottom + ITEM_PADDING, ITEM_PADDING * 2, ITEM_PADDING * 2);
				DeleteObject(SelectObject(hdc, hBrushPrev));
				DeleteObject(SelectObject(hdc, hPenPrev));
			}

			HBITMAP hbmPrev = (HBITMAP)SelectObject(hdcBuffer, pItem->hbmCache);
			FitBitmapToRect(hdc, rcItem, hdcBuffer, rcLayer.right - rcLayer.left, rcLayer.bottom - rcLayer.top);
			SelectObject(hdcBuffer, hbmPrev);

			rcLabel.left = rcItem.left;
			rcLabel.right = rcItem.right;
			DrawText(hdc, RStrToWide(pItem->rstrLabel), RStrLen(pItem->rstrLabel), &rcLabel, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
		}

		DeleteDC(hdcBuffer);
	}
}

BOOL CSIFRow::SelectItem (INT x, INT y, __deref_out SIF_ITEM** ppSelected)
{
	if(IsPositionWithin(y))
	{
		for(LONG n = 0; n < m_cItems; n++)
		{
			SIF_ITEM* pItem = m_pItems + n;
			if(x >= pItem->xPos && x < pItem->xPos + m_sizing.xItemSize + ITEM_PADDING * 2)
			{
				*ppSelected = pItem;
				return TRUE;
			}
		}
	}

	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
// CSIFGroup
///////////////////////////////////////////////////////////////////////////////

CSIFGroup::CSIFGroup (RSTRING rstrTitle, ISimbeyInterchangeFile* pSIF) :
	m_title(rstrTitle),
	m_pSIF(pSIF),
	m_pSelected(NULL)
{
	m_pSIF->AddRef();
}

CSIFGroup::~CSIFGroup ()
{
	m_aRows.DeleteAll();
	m_pSIF->Release();
}

HRESULT CSIFGroup::Layout (LONG nWidth, LONG yOffset, const ITEM_SIZING& sizing, __out LONG* pyFinalOffset)
{
	HRESULT hr = S_OK;
	DWORD cPerRow = (nWidth + ITEM_MARGIN) / (sizing.xItemSize + ITEM_PADDING * 2 + ITEM_MARGIN);
	if(cPerRow == 0)
		cPerRow = 1;

	m_aRows.DeleteAll();

	m_yPosition = yOffset;
	m_ySize = 0;

	m_title.Layout(nWidth, yOffset, sizing, &yOffset);

	DWORD cLayers = m_pSIF->GetLayerCount();
	for(DWORD i = 0; i < cLayers; i += cPerRow)
	{
		DWORD cRowItems = cLayers - i;
		if(cRowItems > cPerRow)
			cRowItems = cPerRow;

		CSIFRow* pRow;
		Check(m_aRows.AppendNew(&pRow));

		pRow->m_cItems = cRowItems;
		Check(pRow->Layout(nWidth, yOffset, sizing, m_pSIF, i, &yOffset));
	}

	m_ySize = yOffset - m_yPosition;
	*pyFinalOffset = yOffset;

Cleanup:
	return hr;
}

VOID CSIFGroup::Paint (HDC hdc, LONG nWidth, LONG nHeight, LONG yScroll, const ITEM_SIZING& sizing)
{
	LONG yStart;
	if(IsVisible(nHeight, yScroll, &yStart))
	{
		LOGFONT logFont = {0};
		HFONT hOldFont;
		INT y = yStart + ITEM_MARGIN;

		SideAssertHr(TStrCchCpy(logFont.lfFaceName, ARRAYSIZE(logFont.lfFaceName), L"Arial"));

		m_title.Paint(hdc, nWidth, nHeight, yScroll, logFont);

		logFont.lfHeight = sizing.yTextHeight - 2;
		logFont.lfWeight = FW_NORMAL;
		hOldFont = (HFONT)SelectObject(hdc, CreateFontIndirect(&logFont));

		for(sysint i = 0; i < m_aRows.Length(); i++)
			m_aRows[i]->Paint(hdc, nHeight, yScroll, m_pSelected);

		DeleteObject(SelectObject(hdc, hOldFont));
	}
}

BOOL CSIFGroup::SelectItem (INT x, INT y)
{
	if(IsPositionWithin(y))
	{
		for(sysint i = 0; i < m_aRows.Length(); i++)
		{
			if(m_aRows[i]->SelectItem(x, y, &m_pSelected))
				return TRUE;
		}
	}

	return FALSE;
}

BOOL CSIFGroup::SelectItemByIndex (sysint idxItem)
{
	if(0 <= idxItem && 0 < m_aRows.Length())
	{
		sysint cPerRow = m_aRows[0]->m_cItems;
		sysint nRow = idxItem / cPerRow;
		if(nRow < m_aRows.Length())
		{
			idxItem -= nRow * cPerRow;
			if(idxItem < m_aRows[nRow]->m_cItems)
			{
				m_pSelected = m_aRows[nRow]->m_pItems + idxItem;
				return TRUE;
			}
		}
	}

	return FALSE;
}

BOOL CSIFGroup::SelectItemByID (DWORD idLayer)
{
	DWORD cLayers = m_pSIF->GetLayerCount();

	for(DWORD i = 0; i < cLayers; i++)
	{
		TStackRef<ISimbeyInterchangeFileLayer> srLayer;

		if(SUCCEEDED(m_pSIF->GetLayerByIndex(i, &srLayer)) && idLayer == srLayer->GetLayerID())
			return SelectItemByIndex(i);
	}

	return FALSE;
}

BOOL CSIFGroup::NavigateSelection (WPARAM wKey)
{
	BOOL fSuccess = FALSE;
	sysint idxRow, idxItem;

	if(GetSelected(&idxRow, &idxItem))
	{
		switch(wKey)
		{
		case VK_LEFT:
			fSuccess = SelectItemByIndex(idxItem - 1);
			break;
		case VK_UP:
			fSuccess = SelectItemByIndex(idxItem - m_aRows[0]->m_cItems);
			break;
		case VK_RIGHT:
			fSuccess = SelectItemByIndex(idxItem + 1);
			break;
		case VK_DOWN:
			fSuccess = SelectItemByIndex(idxItem + m_aRows[0]->m_cItems);
			break;
		}
	}

	return fSuccess;
}

BOOL CSIFGroup::GetSelected (__out sysint* pidxRow, __out sysint* pidxItem)
{
	for(sysint i = 0; i < m_aRows.Length(); i++)
	{
		CSIFRow* pRow = m_aRows[i];
		for(LONG n = 0; n < pRow->m_cItems; n++)
		{
			if(m_pSelected == pRow->m_pItems + n)
			{
				*pidxRow = i;
				*pidxItem = m_aRows[0]->m_cItems * i + n;
				return TRUE;
			}
		}
	}

	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
// CSIFSelector
///////////////////////////////////////////////////////////////////////////////

CSIFSelector::CSIFSelector () :
	m_pWindow(NULL),
	m_xClientSize(0),
	m_yClientSize(0),
	m_yTotalSize(0),
	m_rstrSelection(NULL),
	m_idSelection(0)
{
	m_sizing.xItemSize = 100;
	m_sizing.yItemSize = 80;
	m_sizing.yTextHeight = 18;
}

CSIFSelector::~CSIFSelector ()
{
	Assert(NULL == m_pWindow);

	m_aGroups.DeleteAll();
	RStrRelease(m_rstrSelection);
}

HRESULT CSIFSelector::ResizeItems (LONG xItemSize, LONG yItemSize, LONG yTextHeight)
{
	m_sizing.xItemSize = xItemSize;
	m_sizing.yItemSize = yItemSize;
	m_sizing.yTextHeight = yTextHeight;

	return Relayout();
}

HRESULT CSIFSelector::AddSIF (RSTRING rstrTitle, ISimbeyInterchangeFile* pSIF)
{
	HRESULT hr;
	CSIFGroup* pGroup = __new CSIFGroup(rstrTitle, pSIF);

	if(0 < m_xClientSize)
		Check(pGroup->Layout(m_xClientSize, m_yTotalSize, m_sizing, &m_yTotalSize));
	Check(m_aGroups.Append(pGroup));
	pGroup = NULL;

	if(m_pWindow)
	{
		Check(UpdateScrollRange());
		Check(m_pWindow->Invalidate(FALSE));
	}

Cleanup:
	__delete pGroup;
	return hr;
}

BOOL CSIFSelector::HasSelection (VOID)
{
	return NULL != FindSelectedGroup();
}

VOID CSIFSelector::DeferSelection (RSTRING rstrTitle, DWORD idLayer)
{
	RStrReplace(&m_rstrSelection, rstrTitle);
	m_idSelection = idLayer;
}

VOID CSIFSelector::LoadSelection (VOID)
{
	CSIFGroup* pSelected = FindSelectedGroup();
	if(pSelected)
	{
		RStrReplace(&m_rstrSelection, pSelected->m_title.m_rstrTitle);
		m_idSelection = pSelected->m_pSelected->pLayer->GetLayerID();
	}
	else
	{
		RStrRelease(m_rstrSelection); m_rstrSelection = NULL;
		m_idSelection = 0;
	}
}

HRESULT CSIFSelector::GetSelected (__out RSTRING* prstrTitle, __out DWORD* pidSelection)
{
	HRESULT hr;

	CheckIfIgnore(NULL == m_rstrSelection, E_FAIL);
	RStrSet(*prstrTitle, m_rstrSelection);
	*pidSelection = m_idSelection;
	hr = S_OK;

Cleanup:
	return hr;
}

// IAdapterWindowCallback

BOOL CSIFSelector::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	BOOL fHandled = FALSE;
	HWND hwnd;

	switch(message)
	{
	case WM_PAINT:
		if(SUCCEEDED(m_pWindow->GetWindow(&hwnd)))
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);
			INT nPos;

			FillRect(hdc, &ps.rcPaint, (HBRUSH)GetStockObject(WHITE_BRUSH));

			if(SUCCEEDED(GetScrollPos(&nPos, FALSE)))
			{
				INT nPrevMode = SetBkMode(hdc, TRANSPARENT);
				for(sysint i = 0; i < m_aGroups.Length(); i++)
					m_aGroups[i]->Paint(hdc, m_xClientSize, m_yClientSize, nPos, m_sizing);
				SetBkMode(hdc, nPrevMode);
			}

			if(GetFocus() == hwnd)
			{
				RECT rc;
				GetClientRect(hwnd, &rc);
				DrawFocusRect(hdc, &rc);
			}

			EndPaint(hwnd, &ps);
		}
		break;

	case WM_VSCROLL:
		if(SUCCEEDED(m_pWindow->GetWindow(&hwnd)))
		{
			WORD wScroll = LOWORD(wParam);
			SCROLLINFO si;
			si.cbSize = sizeof(si);
			si.fMask = SIF_POS;

			switch(wScroll)
			{
			case SB_LINEUP:
			case SB_LINEDOWN:
				if(SUCCEEDED(GetScrollPos(&si.nPos, FALSE)))
				{
					if(SB_LINEDOWN == wScroll)
						si.nPos += m_sizing.yTextHeight;
					else
						si.nPos -= m_sizing.yTextHeight;
					SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
					InvalidateRect(hwnd, NULL, FALSE);
				}
				break;
			case SB_PAGEUP:
			case SB_PAGEDOWN:
				if(SUCCEEDED(GetScrollPos(&si.nPos, FALSE)))
				{
					INT nPage = m_sizing.yItemSize + m_sizing.yTextHeight + ITEM_PADDING * 2 + ITEM_MARGIN;
					if(SB_PAGEDOWN == wScroll)
						si.nPos += nPage;
					else
						si.nPos -= nPage;
					SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
					InvalidateRect(hwnd, NULL, FALSE);
				}
				break;
			case SB_THUMBPOSITION:
			case SB_THUMBTRACK:
				if(SUCCEEDED(GetScrollPos(&si.nPos, TRUE)))
				{
					SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
					InvalidateRect(hwnd, NULL, FALSE);
				}
				break;
			case SB_TOP:
				if(m_yTotalSize > m_yClientSize)
				{
					si.nPos = 0;
					SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
					InvalidateRect(hwnd, NULL, FALSE);
				}
				break;
			case SB_BOTTOM:
				if(m_yTotalSize > m_yClientSize)
				{
					si.nPos = (m_yTotalSize - m_yClientSize) + 1;
					SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
					InvalidateRect(hwnd, NULL, FALSE);
				}
				break;
			}
		}
		break;

	case WM_SIZE:
		m_xClientSize = LOWORD(lParam);
		m_yClientSize = HIWORD(lParam);
		Relayout();
		break;

	case WM_LBUTTONDOWN:
		Click(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;

	case WM_GETDLGCODE:
		switch(wParam)
		{
		case VK_LEFT:
		case VK_UP:
		case VK_RIGHT:
		case VK_DOWN:
			if(S_OK == NavigateSelection(wParam))
			{
				lResult = DLGC_WANTARROWS | DLGC_WANTMESSAGE;
				fHandled = TRUE;
			}
			break;
		}
		break;

	case WM_MOUSEWHEEL:
		if(SUCCEEDED(m_pWindow->GetWindow(&hwnd)))
		{
			SHORT sDistance = GET_WHEEL_DELTA_WPARAM(wParam);
			SCROLLINFO si;
			si.cbSize = sizeof(si);
			si.fMask = SIF_POS;
			if(SUCCEEDED(GetScrollPos(&si.nPos, FALSE)))
			{
				if(0 > sDistance)
					si.nPos += m_sizing.yTextHeight;
				else if(0 < sDistance)
					si.nPos -= m_sizing.yTextHeight;

				SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
				InvalidateRect(hwnd, NULL, FALSE);
			}
		}
		break;

	case WM_SETFOCUS:
	case WM_KILLFOCUS:
		if(SUCCEEDED(m_pWindow->GetWindow(&hwnd)))
			InvalidateRect(hwnd, NULL, FALSE);
		break;
	}

	return fHandled;
}

VOID CSIFSelector::OnAttachingAdapter (IBaseWindow* pAdapter)
{
	SetInterface(m_pWindow, pAdapter);

	HWND hwnd;
	if(SUCCEEDED(pAdapter->GetWindow(&hwnd)))
	{
		RECT rc;
		GetClientRect(hwnd, &rc);
		m_xClientSize = rc.right - rc.left;
		m_yClientSize = rc.bottom - rc.top;
		Relayout();
	}
}

VOID CSIFSelector::OnDetachingAdapter (IBaseWindow* pAdapter)
{
	SafeRelease(m_pWindow);
}

HRESULT CSIFSelector::SetSelection (RSTRING rstrTitle, DWORD idLayer)
{
	CSIFGroup* pPrevGroup = FindSelectedGroup();

	for(sysint i = 0; i < m_aGroups.Length(); i++)
	{
		CSIFGroup* pGroup = m_aGroups[i];
		INT nResult;

		if(SUCCEEDED(RStrCompareRStr(pGroup->m_title.m_rstrTitle, rstrTitle, &nResult)) && 0 == nResult)
		{
			if(pGroup->SelectItemByID(idLayer))
			{
				if(pPrevGroup && pPrevGroup != pGroup)
					pPrevGroup->m_pSelected = NULL;
				return S_OK;
			}
			return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
		}
	}

	return HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND);
}

HRESULT CSIFSelector::GetScrollPos (__out INT* pnPos, BOOL fTrackPos)
{
	HRESULT hr;
	HWND hwnd;
	SCROLLINFO si;

	CheckIf(NULL == m_pWindow, E_FAIL);
	Check(m_pWindow->GetWindow(&hwnd));

	si.cbSize = sizeof(si);
	si.fMask = fTrackPos ? SIF_TRACKPOS : SIF_POS;
	CheckIf(!GetScrollInfo(hwnd, SB_VERT, &si), E_FAIL);
	*pnPos = fTrackPos ? si.nTrackPos : si.nPos;

Cleanup:
	return hr;
}

HRESULT CSIFSelector::UpdateScrollRange (VOID)
{
	HRESULT hr;
	HWND hwnd;
	SCROLLINFO si;

	CheckIf(NULL == m_pWindow, E_FAIL);
	Check(m_pWindow->GetWindow(&hwnd));
	si.cbSize = sizeof(si);
	si.fMask = SIF_RANGE | SIF_PAGE;
	si.nMin = 0;
	si.nMax = m_yTotalSize;
	si.nPage = m_yClientSize;
	SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

Cleanup:
	return hr;
}

HRESULT CSIFSelector::Relayout (VOID)
{
	HRESULT hr = S_OK;

	m_yTotalSize = 0;

	if(0 < m_xClientSize)
	{
		if(NULL == m_rstrSelection)
			LoadSelection();

		for(sysint i = 0; i < m_aGroups.Length(); i++)
			Check(m_aGroups[i]->Layout(m_xClientSize, m_yTotalSize, m_sizing, &m_yTotalSize));

		if(m_rstrSelection)
		{
			SetSelection(m_rstrSelection, m_idSelection);
			RStrRelease(m_rstrSelection);
			m_rstrSelection = NULL;
		}
	}

	if(m_pWindow)
	{
		Check(UpdateScrollRange());
		Check(m_pWindow->Invalidate(FALSE));
	}

Cleanup:
	return hr;
}

CSIFGroup* CSIFSelector::FindSelectedGroup (__out_opt sysint* pidxGroup)
{
	for(sysint i = 0; i < m_aGroups.Length(); i++)
	{
		CSIFGroup* pGroup = *m_aGroups.GetItemPtr(i);
		if(pGroup->m_pSelected)
		{
			if(pidxGroup)
				*pidxGroup = i;
			return pGroup;
		}
	}
	return NULL;
}

HRESULT CSIFSelector::Click (INT xClient, INT yClient)
{
	HRESULT hr;
	CSIFGroup* pPrevGroup = FindSelectedGroup(), *pNewGroup = NULL;
	INT yScroll;
	SIF_ITEM* pPrevSelected = NULL;
	HWND hwnd;

	Check(m_pWindow->GetWindow(&hwnd));
	SetFocus(hwnd);

	if(pPrevGroup)
	{
		pPrevSelected = pPrevGroup->m_pSelected;
		pPrevGroup->m_pSelected = NULL;
	}

	Check(GetScrollPos(&yScroll, FALSE));

	for(sysint i = 0; i < m_aGroups.Length(); i++)
	{
		CSIFGroup* pGroup = *m_aGroups.GetItemPtr(i);
		if(pGroup->SelectItem(xClient, yClient + yScroll))
		{
			pNewGroup = pGroup;
			break;
		}
	}

	if((pNewGroup != pPrevGroup) || (pNewGroup && pNewGroup->m_pSelected != pPrevSelected))
		Check(m_pWindow->Invalidate(FALSE));

Cleanup:
	return hr;
}

HRESULT CSIFSelector::NavigateSelection (WPARAM wParam)
{
	HRESULT hr = S_FALSE;

	if(VK_LEFT == wParam || VK_UP == wParam || VK_RIGHT == wParam || VK_DOWN == wParam)
	{
		sysint idxGroup, idxRow, idxItem;
		CSIFGroup* pSelected = FindSelectedGroup(&idxGroup);

		if(pSelected)
		{
			if(!pSelected->NavigateSelection(wParam))
			{
				switch(wParam)
				{
				case VK_LEFT:
				case VK_UP:
					CheckIfIgnore(0 == idxGroup, S_FALSE);

					pSelected->m_pSelected = NULL;
					pSelected = m_aGroups[idxGroup - 1];
					CheckIf(!pSelected->SelectItemByIndex(pSelected->m_pSIF->GetLayerCount() - 1), E_FAIL);
					break;
				case VK_RIGHT:
				case VK_DOWN:
					CheckIfIgnore(idxGroup == m_aGroups.Length() - 1, S_FALSE);

					pSelected->m_pSelected = NULL;
					pSelected = m_aGroups[idxGroup + 1];
					CheckIf(!pSelected->SelectItemByIndex(0), E_FAIL);
					break;
				}
			}
		}
		else
		{
			CheckIf(0 == m_aGroups.Length(), E_FAIL);
			pSelected = m_aGroups[0];
			CheckIf(!pSelected->SelectItemByIndex(0), E_FAIL);
		}

		CheckIf(!pSelected->GetSelected(&idxRow, &idxItem), E_FAIL);

		INT yScroll;
		if(SUCCEEDED(GetScrollPos(&yScroll, FALSE)))
		{
			LONG yPosition = pSelected->m_aRows[idxRow]->m_yPosition;
			LONG ySize = pSelected->m_aRows[idxRow]->m_ySize;
			HWND hwnd;

			SCROLLINFO si;
			si.cbSize = sizeof(si);
			si.fMask = SIF_POS;

			if(yPosition + ySize / 2 < yScroll)
			{
				Check(m_pWindow->GetWindow(&hwnd));
				si.nPos = yPosition;
				SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
			}
			else if(yPosition + ySize / 2 > yScroll + m_yClientSize)
			{
				Check(m_pWindow->GetWindow(&hwnd));
				si.nPos = (yPosition + ySize) - m_yClientSize;
				SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
			}
		}

		Check(m_pWindow->Invalidate(FALSE));
	}

Cleanup:
	return hr;
}
