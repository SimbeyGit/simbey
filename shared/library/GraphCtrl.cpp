#ifndef	GRAPH_CTRL_QUX
	#define	_WIN32_WINNT	0x0500
#endif

#include <math.h>
#include <windows.h>
#ifdef	GRAPH_CTRL_QUX
	#include <Gdiplus.h>
	using namespace Gdiplus;
#endif
#include "Core\SDKExtras.h"
#include "Core\CoreDefs.h"
#include "DPI.h"
#include "GraphCtrl.h"

#ifdef	GRAPH_CTRL_QUX

#define	GetAValue(v)		((BYTE)((v & 0xFF000000) >> 24))

inline DWORD ColorRefToARGB (COLORREF cr)
{
	return Color::MakeARGB(GetAValue(cr),
		GetRValue(cr),
		GetGValue(cr),
		GetBValue(cr));
}

VOID GdipSetPixel (GpGraphics* pgpGraphics, RECT& rcOffset, INT x, INT y, GpBrush* pgpBrush)
{
	DllExports::GdipFillRectangleI(pgpGraphics, pgpBrush, rcOffset.left + x, rcOffset.top + y, 1, 1);
}

VOID GdipDrawLine (GpGraphics* pgpGraphics, RECT& rcOffset, INT x1, INT y1, INT x2, INT y2, GpPen* pgpPen)
{
	DllExports::GdipDrawLineI(pgpGraphics, pgpPen, rcOffset.left + x1, rcOffset.top + y1, rcOffset.left + x2, rcOffset.top + y2);
}

VOID GdipRoundRect (GpGraphics* pgpGraphics, RECT& rcOffset, INT x1, INT y1, INT x2, INT y2, FLOAT rWidth, FLOAT rHeight, GpPen* pgpPen, GpBrush* pgpFill)
{
	GpPath* pgpPath;
	DllExports::GdipCreatePath(FillModeWinding, &pgpPath);
	FLOAT xRadius = rWidth / 2.0f;
	FLOAT yRadius = rHeight / 2.0f;

	x1 += rcOffset.left;
	y1 += rcOffset.top;
	x2 += rcOffset.left;
	y2 += rcOffset.top;

	FLOAT rx1 = (FLOAT)x1;
	FLOAT ry1 = (FLOAT)y1;
	FLOAT rx2 = (FLOAT)x2;
	FLOAT ry2 = (FLOAT)y2;

	DllExports::GdipAddPathLine(pgpPath, rx1 + xRadius, ry1, rx2 - rWidth, ry1);
	DllExports::GdipAddPathArc(pgpPath, rx2 - rWidth, ry1, rWidth, rHeight, 270, 90);
	DllExports::GdipAddPathLine(pgpPath, rx2, ry1 + yRadius, rx2, ry2 - rHeight);
	DllExports::GdipAddPathArc(pgpPath, rx2 - rWidth, ry2 - rHeight, rWidth, rHeight, 0, 90);
	DllExports::GdipAddPathLine(pgpPath, rx2 - rWidth, ry2, rx1 + xRadius, ry2);
	DllExports::GdipAddPathArc(pgpPath, rx1, ry2 - rWidth, rWidth, rHeight, 90, 90);
	DllExports::GdipAddPathLine(pgpPath, rx1, ry2 - rHeight, rx1, ry1 + yRadius);
	DllExports::GdipAddPathArc(pgpPath, rx1, ry1, rWidth, rHeight, 180, 90);

	DllExports::GdipClosePathFigure(pgpPath);
	if(pgpFill)
		DllExports::GdipFillPath(pgpGraphics, pgpFill, pgpPath);
	if(pgpPen)
		DllExports::GdipDrawPath(pgpGraphics, pgpPen, pgpPath);
	DllExports::GdipDeletePath(pgpPath);
}

#endif

CGraphCtrl::CGraphCtrl ()
{
	m_gtGraphType = GRAPH_INVALID;
	m_lpContainer = NULL;
	m_lpClient = NULL;
#ifndef	GRAPH_CTRL_QUX
	m_pBackground = this;

	m_hbmGraph = NULL;
	m_hbmGraphPrev = NULL;
	m_hdcGraph = NULL;
	m_lpGraphPtr = NULL;
	m_lGraphPitch = 0;
#else
	m_pcSurface = NULL;
#endif
	m_nWidth = 0;
	m_nHeight = 0;
	ZeroMemory(&m_rcPos,sizeof(RECT));

#ifndef	GRAPH_CTRL_QUX
	m_hbBackground = CreateSolidBrush(GRAPH_CTRL_DEF_BACKGROUND);
	m_hpGrid = NULL;
#else
	m_pgpGrid = NULL;
#endif
	SetGridColor(GRAPH_CTRL_DEF_GRID_COLOR);
	m_crAxisPoint = GRAPH_CTRL_DEF_AXIS_COLOR;

	m_fZoomChange = 1.2f;
	m_nMouseWheel = 0;

	m_fIsDraggingCanvas = FALSE;
	m_pGraphDrag = NULL;

	m_iGridFlags = GRID_POINTS |
		GRID_FLAG_ZOOM_MOUSE |
		GRID_FLAG_ZOOM_ON_SIZE |
		GRID_FLAG_ENABLE_ZOOMING;
	m_iGridSpace = GRAPH_CTRL_DEF_GRID_SPACING;

	m_xOffset = 0;
	m_yOffset = 0;

	m_iMaxSpacing = GRAPH_CTRL_DEF_GRID_MAX;
	m_iMinSpacing = GRAPH_CTRL_DEF_GRID_MIN;

	ResetPosition();
}

CGraphCtrl::~CGraphCtrl ()
{
	Assert(m_lpContainer == NULL);

	__delete m_pGraphDrag;

#ifndef	GRAPH_CTRL_QUX
	DeleteObject(m_hpGrid);
	DeleteObject(m_hbBackground);

	FreeDoubleBuffers();
#else
	DllExports::GdipDeletePen(m_pgpGrid);
#endif
}

VOID CGraphCtrl::SetGraphTarget (IGraphClient* lpClient)
{
	m_lpClient = lpClient;

	if(m_lpClient)
		m_lpClient->onGraphViewChanged(TRUE);
}

#ifndef	GRAPH_CTRL_QUX

VOID CGraphCtrl::SetGraphBackground (IGraphBackground* pBackground)
{
	if(pBackground)
		m_pBackground = pBackground;
	else
		m_pBackground = this;
}

#endif

VOID CGraphCtrl::SetGraphType (EGRAPHTYPE gtGraphType)
{
	m_gtGraphType = gtGraphType;

	if(GRAPH_PINNED_Y == m_gtGraphType)
		m_iGridFlags = (m_iGridFlags & ~GRID_TYPE_MASK) | GRID_NONE;

	RebuildOffsets();
}

BOOL CGraphCtrl::SetZoomChange (FLOAT fZoomChange)
{
	BOOL fSuccess = FALSE;

	if(fZoomChange > 1.0f && fZoomChange <= DPI::Scale(2.0f))
	{
		m_fZoomChange = fZoomChange;
		fSuccess = TRUE;
	}

	return fSuccess;
}

VOID CGraphCtrl::SetMinMaxSpacing (INT iMaxSpacing, INT iMinSpacing)
{
	m_iMaxSpacing = iMaxSpacing;
	m_iMinSpacing = iMinSpacing;
}

VOID CGraphCtrl::ResetPosition (VOID)
{
	m_fScale = DPI::Scale(GRAPH_CTRL_DEF_SCALE);

	if(m_lpContainer)
		m_lpContainer->OnScaleChanged(m_fScale);

	m_fxCenter = 0.0f;
	m_fyCenter = 0.0f;
}

VOID WINAPI CGraphCtrl::AttachContainer (IGraphContainer* lpContainer)
{
	if(NULL == lpContainer && m_pGraphDrag)
	{
#ifndef	GRAPH_CTRL_QUX
		m_lpContainer->CaptureMouse(this, FALSE);
#endif
		m_fIsDraggingCanvas = FALSE;
		SafeDelete(m_pGraphDrag);
	}

	m_lpContainer = lpContainer;
}

#ifndef	GRAPH_CTRL_QUX

VOID WINAPI CGraphCtrl::Paint (HDC hdc)
{
	RECT rc;
	HDC hdcBuffer;
	HPEN hpnDef;
	HBRUSH hbrDef;

	if(m_hbmGraph == NULL)
	{
		BITMAPINFO bmInfo;

		ZeroMemory(&bmInfo,sizeof(BITMAPINFO));

		bmInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmInfo.bmiHeader.biBitCount = 24;
		bmInfo.bmiHeader.biWidth = m_nWidth;
		bmInfo.bmiHeader.biHeight = m_nHeight;
		bmInfo.bmiHeader.biCompression = BI_RGB;
		bmInfo.bmiHeader.biPlanes = 1;

		m_hbmGraph = CreateDIBSection(hdc,&bmInfo,DIB_RGB_COLORS,(LPVOID*)&m_lpGraphPtr,NULL,0);

		m_lGraphPitch = ((m_nWidth * 3) + 3) & ~3;

		m_hdcGraph = CreateCompatibleDC(hdc);
		m_hbmGraphPrev = (HBITMAP)SelectObject(m_hdcGraph,m_hbmGraph);
	}

	hdcBuffer = m_hdcGraph;

	rc.left = 0;
	rc.top = 0;
	rc.right = m_nWidth;
	rc.bottom = m_nHeight;

	hbrDef = (HBRUSH)SelectObject(hdcBuffer,m_hbBackground);
	m_pBackground->PaintBackground(hdcBuffer, &rc, m_hbBackground);

	hpnDef = (HPEN)SelectObject(hdcBuffer,m_hpGrid);
	DrawGrid(hdcBuffer,0,0);

	SetBkMode(hdcBuffer,TRANSPARENT);

	if(m_lpClient)
		m_lpClient->onGraphPaint(this);

	m_lpContainer->DrawDib24(hdc, m_rcPos.left, m_rcPos.top, hdcBuffer, m_lpGraphPtr, m_nWidth, m_nHeight, m_lGraphPitch);

	SelectObject(hdcBuffer,hpnDef);
	SelectObject(hdcBuffer,hbrDef);
}

#else

VOID WINAPI CGraphCtrl::Paint (const DIBSURFACE* pcSurface, BYTE bAlpha)
{
	m_pcSurface = pcSurface;
	DrawGrid(0,0);
	if(m_lpClient)
		m_lpClient->onGraphPaint(this);
	m_pcSurface = NULL;
}

#endif

VOID WINAPI CGraphCtrl::Move (const RECT* pcrcPosition)
{
	if(m_rcPos.left != pcrcPosition->left || m_rcPos.top != pcrcPosition->top || m_rcPos.right != pcrcPosition->right || m_rcPos.bottom != pcrcPosition->bottom)
	{
		INT nWidth = pcrcPosition->right - pcrcPosition->left;
		INT nHeight = pcrcPosition->bottom - pcrcPosition->top;

		m_rcPos = *pcrcPosition;

		if(nWidth != m_nWidth || nHeight != m_nHeight)
			SizeObject(nWidth,nHeight);
	}
}

VOID WINAPI CGraphCtrl::GetPosition (LPRECT lpPosition)
{
	*lpPosition = m_rcPos;
}

VOID WINAPI CGraphCtrl::SizeObject (INT nWidth, INT nHeight)
{
	if((GRID_FLAG_ZOOM_ON_SIZE & m_iGridFlags) &&
		(GRID_FLAG_ENABLE_ZOOMING & m_iGridFlags) &&
		0 < m_nWidth)
	{
		SetScale(m_fScale * ((FLOAT)nWidth / (FLOAT)m_nWidth));
	}

#ifndef	GRAPH_CTRL_QUX
	// Release the drawing buffer, but don't rebuild until the next paint
	FreeDoubleBuffers();
#endif

	m_nWidth = nWidth;
	m_nHeight = nHeight;

	RebuildOffsets();
}

#ifndef	GRAPH_CTRL_QUX

#define	GET_X_VALUE			GET_X_LPARAM(lParam)
#define	GET_Y_VALUE			GET_Y_LPARAM(lParam)
#define	GET_DISTANCE		GET_WHEEL_DELTA_WPARAM(wParam)
#define	GET_KEYVALUE		wParam
#define	GET_MODIFIERS		static_cast<DWORD>(wParam)
#define	AND_CAPTURE_MOUSE	&&m_lpContainer->CaptureMouse(this, TRUE)
#define	ADD_WHEEL_MODIFIERS

BOOL WINAPI CGraphCtrl::OnMessage (UINT msg, WPARAM wParam, LPARAM lParam, __out LRESULT& lResult)
{
	UNREFERENCED_PARAMETER(lResult);
#else

#define	GET_X_VALUE			pqme->ptClientPxl.x
#define	GET_Y_VALUE			pqme->ptClientPxl.y
#define	GET_DISTANCE		pqmwe->sDistance
#define	GET_KEYVALUE		pqke->nValue
#define	GET_MODIFIERS		pqme->eModifiers
#define	AND_CAPTURE_MOUSE
#define	ADD_WHEEL_MODIFIERS	,GET_MODIFIERS

VOID WINAPI CGraphCtrl::OnInput (__inout QUXInputEvent* pqie)
{
	if(QUXEvent::Direct == pqie->eStage)
	{
#endif
	BOOL fHandled = FALSE;
	FLOAT fx, fy;

#ifndef	GRAPH_CTRL_QUX
	switch(msg)
	{
	case WM_MOUSEMOVE:
#else
		if(QUXInputEvent::Mouse == pqie->eDevice)
		{
			QUXMouseEvent* pqme = static_cast<QUXMouseEvent*>(pqie);
			switch(pqme->eCode)
			{
			case QUXMouseEvent::Drag:
			case QUXMouseEvent::Move:
#endif
				if(m_pGraphDrag)
				{
					INT x = GET_X_VALUE;
					INT y = GET_Y_VALUE;
					INT yShift;

					if(GRAPH_INVERTED_QUAD == m_gtGraphType)
						yShift = m_pGraphDrag->cy - y;
					else
					{
						yShift = y - m_pGraphDrag->cy;
						if(GRAPH_PINNED_Y == m_gtGraphType)
							y = m_pGraphDrag->cy;
					}

					// Shift the center using client pixel offsets.
					ShiftCenter(m_pGraphDrag->cx - x, yShift);

					m_pGraphDrag->cx = x;
					m_pGraphDrag->cy = y;

					m_fIsDraggingCanvas = TRUE;
					fHandled = TRUE;
				}
				else if(m_lpClient)
				{
					ClientToGraph(GET_X_VALUE - m_rcPos.left, GET_Y_VALUE - m_rcPos.top, fx, fy);
					m_lpClient->onGraphMouseMove(GET_MODIFIERS, fx, fy);
					fHandled = TRUE;
				}
				break;

#ifndef	GRAPH_CTRL_QUX
	case WM_LBUTTONDOWN:
#else
			case QUXMouseEvent::Down:
				if(QUXMouseEvent::LeftButton == pqme->eButton)
				{
#endif
					m_lpContainer->SetFocus(this);
					if(m_lpClient)
					{
						ClientToGraph(GET_X_VALUE - m_rcPos.left, GET_Y_VALUE - m_rcPos.top,fx,fy);
						m_lpClient->onGraphLBtnDown(GET_MODIFIERS, fx, fy);
						fHandled = TRUE;
					}
#ifndef	GRAPH_CTRL_QUX
		break;
	case WM_RBUTTONDOWN:
#else
				}
				else if(QUXMouseEvent::RightButton == pqme->eButton)
				{
#endif
					m_lpContainer->SetFocus(this);
					if(m_lpClient)
					{
						ClientToGraph(GET_X_VALUE - m_rcPos.left, GET_Y_VALUE - m_rcPos.top, fx, fy);
						fHandled = m_lpClient->onGraphRBtnDown(GET_MODIFIERS, fx, fy);
					}
					if(!fHandled AND_CAPTURE_MOUSE)
					{
						if(NULL == m_pGraphDrag)
							m_pGraphDrag = __new SIZE;
						if(m_pGraphDrag)
						{
							m_pGraphDrag->cx = GET_X_VALUE;
							m_pGraphDrag->cy = GET_Y_VALUE;
							fHandled = TRUE;
						}
					}
#ifndef	GRAPH_CTRL_QUX
		break;
	case WM_LBUTTONUP:
#else
				}
				break;
			case QUXMouseEvent::Up:
				if(QUXMouseEvent::LeftButton == pqme->eButton)
				{
#endif
					if(m_lpClient)
					{
						ClientToGraph(GET_X_VALUE - m_rcPos.left, GET_Y_VALUE - m_rcPos.top, fx, fy);
						m_lpClient->onGraphLBtnUp(GET_MODIFIERS, fx, fy);
						fHandled = TRUE;
					}
#ifndef	GRAPH_CTRL_QUX
		break;
	case WM_RBUTTONUP:
#else
				}
				else if(QUXMouseEvent::RightButton == pqme->eButton)
				{
#endif
					if(m_pGraphDrag)
					{
						Assert(m_lpContainer);

#ifndef	GRAPH_CTRL_QUX
			m_lpContainer->CaptureMouse(this, FALSE);
#endif
						SafeDelete(m_pGraphDrag);

						if(m_fIsDraggingCanvas)
						{
							m_fIsDraggingCanvas = FALSE;
							fHandled = TRUE;
						}
						else if(m_lpClient)
						{
							ClientToGraph(GET_X_VALUE - m_rcPos.left, GET_Y_VALUE - m_rcPos.top, fx, fy);
							m_lpClient->onGraphRBtnUp(GET_MODIFIERS, fx, fy);
							fHandled = TRUE;
						}
					}
					else if(m_lpClient)
					{
						ClientToGraph(GET_X_VALUE - m_rcPos.left, GET_Y_VALUE - m_rcPos.top, fx, fy);
						m_lpClient->onGraphRBtnUp(GET_MODIFIERS, fx, fy);
						fHandled = TRUE;
					}
#ifndef	GRAPH_CTRL_QUX
		break;
	case WM_LBUTTONDBLCLK:
		if(m_lpClient)
		{
#else
				}
				break;
			case QUXMouseEvent::DblClk:
				if(m_lpClient)
				{
					if(QUXMouseEvent::LeftButton == pqme->eButton)
					{
#endif
						ClientToGraph(GET_X_VALUE - m_rcPos.left, GET_Y_VALUE - m_rcPos.top, fx, fy);
						m_lpClient->onGraphLBtnDbl(GET_MODIFIERS, fx, fy);
						fHandled = TRUE;
#ifndef	GRAPH_CTRL_QUX
		}
		break;
	case WM_RBUTTONDBLCLK:
		if(m_lpClient)
		{
#else
					}
					else if(QUXMouseEvent::RightButton == pqme->eButton)
					{
#endif
						ClientToGraph(GET_X_VALUE - m_rcPos.left, GET_Y_VALUE - m_rcPos.top, fx, fy);
						m_lpClient->onGraphRBtnDbl(GET_MODIFIERS, fx, fy);
						fHandled = TRUE;
		}
#ifdef	GRAPH_CTRL_QUX
					}
#endif
				break;

#ifndef	GRAPH_CTRL_QUX
	case WM_MOUSEWHEEL:
		{
#else
			case QUXMouseEvent::Wheel:
				{
					QUXMouseWheelEvent* pqmwe = static_cast<QUXMouseWheelEvent*>(pqme);
#endif
					POINT pt = {GET_X_VALUE, GET_Y_VALUE};
#ifndef	GRAPH_CTRL_QUX
			if(SUCCEEDED(m_lpContainer->ScreenToWindowless(this, &pt)))
			{
#endif
					POINT ptLocal = { pt.x - m_rcPos.left, pt.y - m_rcPos.top };
					SHORT sDistance = GET_DISTANCE;
					ClientToGraph(ptLocal.x, ptLocal.y, fx, fy);
					if(m_lpClient)
						fHandled = m_lpClient->onGraphWheel(sDistance ADD_WHEEL_MODIFIERS, fx, fy);
					if(!fHandled)
						fHandled = WheelZoom(sDistance, fx, fy, ptLocal);
				}
#ifndef	GRAPH_CTRL_QUX
			}
#endif
			break;

#ifndef	GRAPH_CTRL_QUX
	case WM_KEYDOWN:
#else
			}
		}
		else if(QUXInputEvent::Keyboard == pqie->eDevice)
		{
			QUXKeyboardEvent* pqke = static_cast<QUXKeyboardEvent*>(pqie);

			switch(pqke->eCode)
			{
			case QUXKeyboardEvent::Down:
#endif
				if(m_lpClient)
					fHandled = m_lpClient->onGraphKeyDown(GET_KEYVALUE);
				if(!fHandled)
				{
					switch(GET_KEYVALUE)
					{
					case VK_LEFT:
						ShiftCenter(-16, 0);
						fHandled = TRUE;
						break;
					case VK_UP:
						ShiftCenter(0, GRAPH_INVERTED_QUAD == m_gtGraphType ? -16 : 16);
						fHandled = TRUE;
						break;
					case VK_RIGHT:
						ShiftCenter(16, 0);
						fHandled = TRUE;
						break;
					case VK_DOWN:
						ShiftCenter(0, GRAPH_INVERTED_QUAD == m_gtGraphType ? 16 : -16);
						fHandled = TRUE;
						break;
					}
				}
				break;

#ifndef	GRAPH_CTRL_QUX
	case WM_KEYUP:
#else
			case QUXKeyboardEvent::Up:
#endif
				if(m_lpClient)
					fHandled = m_lpClient->onGraphKeyUp(GET_KEYVALUE);
				break;

#ifndef	GRAPH_CTRL_QUX
	case WM_CHAR:
#else
			case QUXKeyboardEvent::Char:
#endif
				if(m_lpClient)
					fHandled = m_lpClient->onGraphChar(GET_KEYVALUE);
				if(!fHandled)
				{
					switch(GET_KEYVALUE)
					{
					case '+':
					case '=':
						if(GRID_FLAG_ENABLE_ZOOMING & m_iGridFlags)
						{
							// Zoom in
							SetScale(GetScale() * m_fZoomChange);
							fHandled = TRUE;
						}
						break;
					case '-':
						if(GRID_FLAG_ENABLE_ZOOMING & m_iGridFlags)
						{
							// Zoom out
							SetScale(GetScale() / m_fZoomChange);
							fHandled = TRUE;
						}
						break;
					case '[':
						SetGridSpacing(GetGridSpacing() * 2);
						fHandled = TRUE;
						break;
					case ']':
						SetGridSpacing(GetGridSpacing() / 2);
						fHandled = TRUE;
						break;
					case 'g':
					case 'G':
						ToggleGridType();
						fHandled = TRUE;
					}

					if(fHandled)
						m_lpContainer->InvalidateContainer(this);
				}
				break;

#ifndef	GRAPH_CTRL_QUX
	case WM_CAPTURECHANGED:
		SafeDelete(m_pGraphDrag);
		break;
#endif
			}

#ifndef	GRAPH_CTRL_QUX
	return fHandled;
#else
		}

		pqie->fHandled = !!fHandled;
	}
#endif
}

HRESULT WINAPI CGraphCtrl::GetAccObject (IAccessible** lplpAccessible)
{
	HRESULT hr = E_FAIL;
	if(m_lpClient)
		hr = m_lpClient->onGraphGetAcc(lplpAccessible);
	return hr;
}

// IGrapher

#ifndef	GRAPH_CTRL_QUX

HPEN CGraphCtrl::SelectPen (HPEN hpnPen)
{
	return (HPEN)SelectObject(m_hdcGraph,hpnPen);
}

HBRUSH CGraphCtrl::SelectBrush (HBRUSH hbrBrush)
{
	return (HBRUSH)SelectObject(m_hdcGraph,hbrBrush);
}

COLORREF CGraphCtrl::SetTextColor (COLORREF cr)
{
	return ::SetTextColor(m_hdcGraph,cr);
}

HFONT CGraphCtrl::SelectFont (HFONT hfFont)
{
	return (HFONT)::SelectObject(m_hdcGraph,hfFont);
}

VOID CGraphCtrl::MoveTo (FLOAT x, FLOAT y, FLOAT z)
{
	INT xClient, yClient;
	PointToClient(x,y,z,xClient,yClient);
	::MoveToEx(m_hdcGraph,xClient,yClient,NULL);
}

VOID CGraphCtrl::LineTo (FLOAT x, FLOAT y, FLOAT z)
{
	INT xClient, yClient;
	PointToClient(x,y,z,xClient,yClient);
	::LineTo(m_hdcGraph,xClient,yClient);
}

VOID CGraphCtrl::Rectangle (FLOAT x1, FLOAT y1, FLOAT z1, FLOAT x2, FLOAT y2, FLOAT z2)
{
	INT xClient1, yClient1, xClient2, yClient2;
	PointToClient(x1,y1,z1,xClient1,yClient1);
	PointToClient(x2,y2,z2,xClient2,yClient2);
	::Rectangle(m_hdcGraph,xClient1,yClient1,xClient2,yClient2);
}

VOID CGraphCtrl::FillRect (FLOAT x1, FLOAT y1, FLOAT z1, FLOAT x2, FLOAT y2, FLOAT z2, HBRUSH hBrush)
{
	RECT rc;
	PointToClient(x1,y1,z1,rc.left,rc.top);
	PointToClient(x2,y2,z2,rc.right,rc.bottom);
	::FillRect(m_hdcGraph,&rc,hBrush);
}

VOID CGraphCtrl::RoundRect (FLOAT x1, FLOAT y1, FLOAT z1, FLOAT x2, FLOAT y2, FLOAT z2, FLOAT fWidth, FLOAT fHeight)
{
	INT xClient1, yClient1, xClient2, yClient2;
	PointToClient(x1,y1,z1,xClient1,yClient1);
	PointToClient(x2,y2,z2,xClient2,yClient2);
	::RoundRect(m_hdcGraph,xClient1,yClient1,xClient2,yClient2,(INT)(fWidth * m_fScale),(INT)(fHeight * m_fScale));
}

VOID CGraphCtrl::Ellipse (FLOAT x1, FLOAT y1, FLOAT z1, FLOAT x2, FLOAT y2, FLOAT z2)
{
	INT xClient1, yClient1, xClient2, yClient2;
	PointToClient(x1,y1,z1,xClient1,yClient1);
	PointToClient(x2,y2,z2,xClient2,yClient2);
	::Ellipse(m_hdcGraph,xClient1,yClient1,xClient2,yClient2);
}

VOID CGraphCtrl::TextOut (FLOAT x, FLOAT y, FLOAT z, LPCTSTR pctzText, INT cchText)
{
	INT xClient, yClient;
	PointToClient(x,y,z,xClient,yClient);
	::TextOut(m_hdcGraph,xClient,yClient,pctzText,cchText);
}

VOID CGraphCtrl::TextAlign (FLOAT x1, FLOAT y1, FLOAT z1, FLOAT x2, FLOAT y2, FLOAT z2, LPCTSTR pctzText, INT cchText, INT dtAlign)
{
	INT xClient1, yClient1, xClient2, yClient2;
	INT x, y;
	SIZE sText;
	PointToClient(x1, y1, z1, xClient1, yClient1);
	PointToClient(x2, y2, z2, xClient2, yClient2);
	GetTextExtentPoint32(m_hdcGraph,pctzText,cchText,&sText);

	if(dtAlign & DT_CENTER)
		x = xClient1 + ((xClient2 - xClient1) >> 1) - (sText.cx >> 1);
	else if(dtAlign & DT_RIGHT)
		x = xClient2 - sText.cx;
	else
		x = xClient1;

	if(dtAlign & DT_VCENTER)
		y = yClient1 + ((yClient2 - yClient1) >> 1) - (sText.cy >> 1);
	else if(dtAlign & DT_BOTTOM)
		y = yClient2 - sText.cy;
	else
		y = yClient1;

	::TextOut(m_hdcGraph, x, y, pctzText, cchText);
}

VOID CGraphCtrl::LabelText (FLOAT x, FLOAT y, FLOAT z, LPCTSTR pctzText, INT cchText)
{
	INT xClient, yClient;
	SIZE sText;
	PointToClient(x,y,z,xClient,yClient);
	GetTextExtentPoint32(m_hdcGraph,pctzText,cchText,&sText);
	::RoundRect(m_hdcGraph,xClient - 3,yClient - 2,xClient + sText.cx + 4,yClient + sText.cy + 3,5,5);
	::TextOut(m_hdcGraph,xClient + 1,yClient,pctzText,cchText);
}

VOID CGraphCtrl::LabelTextAbove (FLOAT x, FLOAT y, FLOAT z, LPCTSTR pctzText, INT cchText)
{
	INT xClient, yClient;
	SIZE sText;
	PointToClient(x,y,z,xClient,yClient);
	GetTextExtentPoint32(m_hdcGraph,pctzText,cchText,&sText);
	yClient -= sText.cy;
	::RoundRect(m_hdcGraph,xClient - 3,yClient - 2,xClient + sText.cx + 4,yClient + sText.cy + 3,5,5);
	::TextOut(m_hdcGraph,xClient + 1,yClient,pctzText,cchText);
}

VOID CGraphCtrl::LabelCenterTextAbove (FLOAT x, FLOAT y, FLOAT z, LPCTSTR pctzText, INT cchText)
{
	INT xClient, yClient;
	SIZE sText;
	PointToClient(x,y,z,xClient,yClient);
	GetTextExtentPoint32(m_hdcGraph,pctzText,cchText,&sText);
	xClient -= sText.cx / 2;
	yClient -= sText.cy;
	::RoundRect(m_hdcGraph,xClient - 3,yClient - 2,xClient + sText.cx + 4,yClient + sText.cy + 3,5,5);
	::TextOut(m_hdcGraph,xClient + 1,yClient,pctzText,cchText);
}

HDC CGraphCtrl::GetClientDC (VOID)
{
	return m_hdcGraph;
}

#else

const DIBSURFACE* CGraphCtrl::GetClientSurface (VOID)
{
	return m_pcSurface;
}

VOID CGraphCtrl::DrawLine (FLOAT x1, FLOAT y1, FLOAT z1, FLOAT x2, FLOAT y2, FLOAT z2, GpPen* pgpPen)
{
	INT xClient1, yClient1, xClient2, yClient2;
	PointToClient(x1,y1,z1,xClient1,yClient1);
	PointToClient(x2,y2,z2,xClient2,yClient2);
	GdipDrawLine(m_pcSurface->pgpGraphics, m_rcPos, xClient1, yClient1, xClient2, yClient2, pgpPen);
}

VOID CGraphCtrl::RoundRect (FLOAT x1, FLOAT y1, FLOAT z1, FLOAT x2, FLOAT y2, FLOAT z2, FLOAT rWidth, FLOAT rHeight, GpPen* pgpPen, GpBrush* pgpFill)
{
	INT xClient1, yClient1, xClient2, yClient2;
	PointToClient(x1,y1,z1,xClient1,yClient1);
	PointToClient(x2,y2,z2,xClient2,yClient2);
	GdipRoundRect(m_pcSurface->pgpGraphics, m_rcPos, xClient1, yClient1, xClient2, yClient2, rWidth * m_fScale, rHeight * m_fScale, pgpPen, pgpFill);
}

#endif

BOOL CGraphCtrl::GetRawBuffer (LPBYTE& lpBuffer, INT& nWidth, INT& nHeight, LONG& lPitch)
{
	BOOL fHaveBuffer = FALSE;
#ifndef	GRAPH_CTRL_QUX
	if(m_lpGraphPtr)
#else
	if(m_pcSurface)
#endif
	{
#ifndef	GRAPH_CTRL_QUX
		lpBuffer = m_lpGraphPtr;
		lPitch = m_lGraphPitch;
#else
		lpBuffer = m_pcSurface->pDIB32;
		lPitch = m_pcSurface->lPitch;
#endif
		nWidth = m_nWidth;
		nHeight = m_nHeight;
		fHaveBuffer = TRUE;
	}
	return fHaveBuffer;
}

VOID CGraphCtrl::GraphToClient (FLOAT gx, FLOAT gy, INT& x, INT& y)
{
	x = (INT)((gx - m_fxCenter) * m_fScale) + m_xOffset;
	switch(m_gtGraphType)
	{
	case GRAPH_XZ:
		y = (INT)((m_fyCenter + gy) * m_fScale) + m_yOffset;
		break;
	case GRAPH_PINNED_Y:
		y = m_nHeight - (INT)gy;
		break;
	case GRAPH_INVERTED_QUAD:
		y = (INT)((gy - m_fyCenter) * m_fScale);
		break;
	default:
		y = (INT)((m_fyCenter - gy) * m_fScale) + m_yOffset;
		break;
	}
}

VOID CGraphCtrl::GraphToClientPoint (FLOAT gx, FLOAT gy, POINT& pt)
{
	pt.x = (LONG)((gx - m_fxCenter) * m_fScale) + m_xOffset;
	switch(m_gtGraphType)
	{
	case GRAPH_XZ:
		pt.y = (LONG)((m_fyCenter + gy) * m_fScale) + m_yOffset;
		break;
	case GRAPH_PINNED_Y:
		pt.y = m_nHeight - (LONG)gy;
		break;
	case GRAPH_INVERTED_QUAD:
		pt.y = (LONG)((gy - m_fyCenter) * m_fScale);
		break;
	default:
		pt.y = (LONG)((m_fyCenter - gy) * m_fScale) + m_yOffset;
		break;
	}
}

VOID CGraphCtrl::GraphToClientRect (RECT* lprcGraph, RECT* lprcClient)
{
	lprcClient->left = (LONG)((lprcGraph->left - m_fxCenter) * m_fScale + m_xOffset);
	lprcClient->right = (LONG)((lprcGraph->right - m_fxCenter) * m_fScale + m_xOffset);

	switch(m_gtGraphType)
	{
	case GRAPH_XZ:
		lprcClient->top = (LONG)((m_fyCenter + lprcGraph->top) * m_fScale) + m_yOffset;
		lprcClient->bottom = (LONG)((m_fyCenter + lprcGraph->bottom) * m_fScale) + m_yOffset;
		break;
	case GRAPH_PINNED_Y:
		lprcClient->top = m_nHeight - lprcGraph->top;
		lprcClient->bottom = m_nHeight - lprcGraph->bottom;
		break;
	case GRAPH_INVERTED_QUAD:
		lprcClient->top = (LONG)((lprcGraph->top - m_fyCenter) * m_fScale) + m_yOffset;
		lprcClient->bottom = (LONG)((lprcGraph->bottom - m_fyCenter) * m_fScale) + m_yOffset;
		break;
	default:
		lprcClient->top = (LONG)((m_fyCenter - lprcGraph->top) * m_fScale) + m_yOffset;
		lprcClient->bottom = (LONG)((m_fyCenter - lprcGraph->bottom) * m_fScale) + m_yOffset;
		break;
	}
}

BOOL CGraphCtrl::IsObjectVisible (FLOAT gxLeft, FLOAT gyTop, FLOAT gxRight, FLOAT gyBottom, RECT* lprcClient)
{
	BOOL fVisible;
	INT xLeft, yTop, xRight, yBottom;
	RECT rcClient, rcObject, rcIntersection;

	rcClient.left = 0;
	rcClient.top = 0;
	rcClient.right = m_nWidth;
	rcClient.bottom = m_nHeight;

	GraphToClient(gxLeft,gyTop,xLeft,yTop);
	GraphToClient(gxRight,gyBottom,xRight,yBottom);
	rcObject.left = xLeft;
	rcObject.top = yTop;
	rcObject.right = xRight;
	rcObject.bottom = yBottom;

	if(IntersectRect(&rcIntersection,&rcClient,&rcObject))
	{
		*lprcClient = rcObject;
		fVisible = TRUE;
	}
	else
		fVisible = FALSE;

	return fVisible;
}

VOID CGraphCtrl::GetGraphCenter (FLOAT* lpx, FLOAT* lpy)
{
	*lpx = m_fxCenter;
	*lpy = m_fyCenter;
}

FLOAT CGraphCtrl::GetScale (VOID)
{
	return m_fScale;
}

VOID CGraphCtrl::GetGraphBounds (__out FLOAT* px1, __out FLOAT* py1, __out FLOAT* px2, __out FLOAT* py2)
{
	ClientToGraph(0, 0, *px1, *py1);
	ClientToGraph(m_nWidth, m_nHeight, *px2, *py2);
}

EGRAPHTYPE CGraphCtrl::GetGraphType (VOID)
{
	return m_gtGraphType;
}

FLOAT CGraphCtrl::GetGridSnap (FLOAT p)
{
	return GridPoint(p,m_iGridSpace);
}

#ifndef GRAPH_CTRL_QUX

// IGraphBackground

VOID WINAPI CGraphCtrl::PaintBackground (HDC hdc, RECT* prc, HBRUSH hbrDefault)
{
	::FillRect(hdc, prc, hbrDefault);
}

#endif

//

#ifndef GRAPH_CTRL_QUX

VOID CGraphCtrl::SetBGColor (COLORREF crBackground)
{
	DeleteObject(m_hbBackground);
	m_hbBackground = CreateSolidBrush(crBackground);
}

#endif

VOID CGraphCtrl::SetGridColor (COLORREF crGrid)
{
#ifndef	GRAPH_CTRL_QUX
	DeleteObject(m_hpGrid);
	m_hpGrid = CreatePen(PS_SOLID, 0, crGrid);
#else
	DllExports::GdipCreatePen1(ColorRefToARGB(crGrid), 2.0f, UnitPixel, &m_pgpGrid);
#endif
	m_crGrid = crGrid;
}

VOID CGraphCtrl::SetAxisPointColor (COLORREF crAxisPoint)
{
	m_crAxisPoint = crAxisPoint;
}

VOID CGraphCtrl::GetClientSize (INT* lpnWidth, INT* lpnHeight)
{
	if(lpnWidth)
		*lpnWidth = m_nWidth;
	if(lpnHeight)
		*lpnHeight = m_nHeight;
}

VOID CGraphCtrl::ShiftCenter (INT xShift, INT yShift)
{
	ShiftCenter((FLOAT)xShift / m_fScale, (FLOAT)yShift / m_fScale);
}

VOID CGraphCtrl::ShiftCenter (FLOAT rxShift, FLOAT ryShift)
{
	SetCenter(m_fxCenter + rxShift, m_fyCenter + ryShift);
}

VOID CGraphCtrl::SetCenter (FLOAT rx, FLOAT ry)
{
	if(GRAPH_INVERTED_QUAD == m_gtGraphType)
	{
		if(rx < 0.0f) rx = 0.0f;
		if(ry < 0.0f) ry = 0.0f;
	}
	if(rx != m_fxCenter || ry != m_fyCenter)
	{
		m_fxCenter = rx;
		m_fyCenter = ry;

		if(m_lpClient)
			m_lpClient->onGraphViewChanged(FALSE);
		if(m_lpContainer)
			m_lpContainer->InvalidateContainer(this);
	}
}

VOID CGraphCtrl::GetCenter (__out_opt FLOAT* prx, __out_opt FLOAT* pry)
{
	if(prx)
		*prx = m_fxCenter;
	if(pry)
		*pry = m_fyCenter;
}

VOID CGraphCtrl::SetScale (FLOAT fScale)
{
	FLOAT fPrevScale = m_fScale;

	if(fScale >= (FLOAT).001)
	{
		FLOAT fMaxZoom = DPI::Scale(4.0f);
		if(fScale <= fMaxZoom)
			m_fScale = fScale;
		else
			m_fScale = fMaxZoom;
	}
	else
		m_fScale = 0.001f;

	if(fPrevScale != m_fScale)
	{
		if(m_lpClient)
			m_lpClient->onGraphViewChanged(TRUE);
		if(m_lpContainer)
		{
			m_lpContainer->OnScaleChanged(m_fScale);
			m_lpContainer->InvalidateContainer(this);
		}
	}
}

VOID CGraphCtrl::ClientToGraph (INT x, INT y, FLOAT& fx, FLOAT& fy)
{
	fx = (FLOAT)(x - m_xOffset) / m_fScale + m_fxCenter;
	switch(m_gtGraphType)
	{
	case GRAPH_XZ:
		fy = (FLOAT)(y - m_yOffset) / m_fScale - m_fyCenter;
		break;
	case GRAPH_PINNED_Y:
		fy = (FLOAT)(m_nHeight - y);
		break;
	case GRAPH_INVERTED_QUAD:
		fy = (FLOAT)(y - m_yOffset) / m_fScale + m_fyCenter;
		break;
	default:
		fy = m_fyCenter - (FLOAT)(y - m_yOffset) / m_fScale;
		break;
	}
}

VOID CGraphCtrl::ClientToGraphGrid (INT x, INT y, FLOAT& fx, FLOAT& fy)
{
	ClientToGraph(x,y,fx,fy);

	fx = GridPoint(fx,m_iGridSpace);
	fy = GridPoint(fy,m_iGridSpace);
}

VOID CGraphCtrl::SetGridType (INT iGridType)
{
	if(GRAPH_PINNED_Y != m_gtGraphType && 0 == (iGridType & ~GRID_TYPE_MASK))
	{
		m_iGridFlags = (m_iGridFlags & ~GRID_TYPE_MASK) | iGridType;

		if(m_lpContainer)
			m_lpContainer->InvalidateContainer(this);
	}
}

INT CGraphCtrl::GetGridType (VOID)
{
	return m_iGridFlags & GRID_TYPE_MASK;
}

VOID CGraphCtrl::ToggleGridType (VOID)
{
	INT iGrid = GetGridType() + 1;
	if(iGrid > GRID_AXIS_POINTS) iGrid = GRID_NONE;
	SetGridType(iGrid);
}

VOID CGraphCtrl::SetFlag (INT iFlag, BOOL fSet)
{
	if(fSet)
		m_iGridFlags |= iFlag;
	else
		m_iGridFlags &= ~iFlag;
}

INT CGraphCtrl::GetFlags (VOID)
{
	return m_iGridFlags & ~GRID_TYPE_MASK;
}

VOID CGraphCtrl::SetGridSpacing (INT iSpacing)
{
	if(iSpacing >= m_iMinSpacing && iSpacing <= m_iMaxSpacing)
	{
		m_iGridSpace = iSpacing;

		if(m_lpContainer)
		{
			m_lpContainer->OnGridSpacingChanged(iSpacing);
			m_lpContainer->InvalidateContainer(this);
		}
	}
}

INT CGraphCtrl::GetGridSpacing (VOID)
{
	return m_iGridSpace;
}

#ifndef	GRAPH_CTRL_QUX
VOID CGraphCtrl::DrawGrid (HDC hdc, INT x, INT y)
#else
VOID CGraphCtrl::DrawGrid (INT x, INT y)
#endif
{
	FLOAT fMove = m_iGridSpace * m_fScale;
	INT nGridType = (m_iGridFlags & GRID_TYPE_MASK);
	if(fMove >= 3.0f || GRID_AXIS == nGridType || GRID_AXIS_POINTS == nGridType)
	{
		INT iTemp;
		FLOAT xStart, yStart, xMap, yMap;
		INT xOrigin = 0, yOrigin = 0;
		GraphToClient(0,0,xOrigin,yOrigin);

		xOrigin += x;
		yOrigin += y;

		iTemp = (INT)((FLOAT)xOrigin / fMove);
		xStart = (FLOAT)xOrigin - (FLOAT)iTemp * fMove;

		iTemp = (INT)((FLOAT)yOrigin / fMove);
		yStart = (FLOAT)yOrigin - (FLOAT)iTemp * fMove;

		switch(nGridType)
		{
		case GRID_POINTS:
#ifndef	GRAPH_CTRL_QUX
			DrawGridPoints(hdc, fMove, xStart, yStart, m_crGrid);
#else
			DrawGridPoints(fMove, xStart, yStart, m_crGrid);
#endif
			break;
		case GRID_LINES:
			for(yMap = yStart; yMap < m_nHeight; yMap += fMove)
			{
#ifndef	GRAPH_CTRL_QUX
				MoveToEx(hdc,0,(INT)yMap,NULL);
				::LineTo(hdc,m_nWidth,(INT)yMap);
#else
				GdipDrawLine(m_pcSurface->pgpGraphics, m_rcPos, 0, (INT)yMap, m_nWidth, (INT)yMap, m_pgpGrid);
#endif
			}
			for(xMap = xStart; xMap < m_nWidth; xMap += fMove)
			{
#ifndef	GRAPH_CTRL_QUX
				MoveToEx(hdc,(INT)xMap,0,NULL);
				::LineTo(hdc,(INT)xMap,m_nHeight);
#else
				GdipDrawLine(m_pcSurface->pgpGraphics, m_rcPos, (INT)xMap, 0, (INT)xMap, m_nHeight, m_pgpGrid);
#endif
			}
			break;
		case GRID_AXIS:
		case GRID_AXIS_POINTS:
#ifndef	GRAPH_CTRL_QUX
			MoveToEx(hdc,0,(INT)yOrigin,NULL);
			::LineTo(hdc,m_nWidth,(INT)yOrigin);
			MoveToEx(hdc,xOrigin,0,NULL);
			::LineTo(hdc,xOrigin,m_nHeight);
#else
			GdipDrawLine(m_pcSurface->pgpGraphics, m_rcPos, 0, (INT)yOrigin, m_nWidth, (INT)yOrigin, m_pgpGrid);
			GdipDrawLine(m_pcSurface->pgpGraphics, m_rcPos, xOrigin, 0, xOrigin, m_nHeight, m_pgpGrid);
#endif
			if(GRID_AXIS_POINTS == nGridType && fMove >= 3.0f)
#ifndef	GRAPH_CTRL_QUX
				DrawGridPoints(hdc, fMove, xStart, yStart, m_crAxisPoint);
#else
				DrawGridPoints(fMove, xStart, yStart, m_crAxisPoint);
#endif
			break;
		}
	}
}

template <typename T>
VOID CGraphCtrl::PointToClient (FLOAT x, FLOAT y, FLOAT z, T& xClient, T& yClient)
{
	switch(m_gtGraphType)
	{
	case GRAPH_XY:
		xClient = (T)((FLOAT)(x - m_fxCenter) * m_fScale);
		yClient = (T)((FLOAT)(-1 * y + m_fyCenter) * m_fScale);
		break;
	case GRAPH_XZ:
		xClient = (T)((FLOAT)(x - m_fxCenter) * m_fScale);
		yClient = (T)((FLOAT)(z + m_fyCenter) * m_fScale);
		break;
	case GRAPH_ZY:
		xClient = (T)((FLOAT)(z - m_fxCenter) * m_fScale);
		yClient = (T)((FLOAT)(-1 * y + m_fyCenter) * m_fScale);
		break;
	case GRAPH_PINNED_Y:
		xClient = (T)((FLOAT)(x - m_fxCenter) * m_fScale);
		yClient = (T)((FLOAT)m_nHeight - y);
		break;
	case GRAPH_INVERTED_QUAD:
		xClient = (T)((x - m_fxCenter) * m_fScale);
		yClient = (T)((y - m_fyCenter) * m_fScale);
		break;
	}
	xClient += (T)m_xOffset;
	yClient += (T)m_yOffset;
}

//

#ifndef	GRAPH_CTRL_QUX
VOID CGraphCtrl::DrawGridPoints (HDC hdc, FLOAT fMove, FLOAT xStart, FLOAT yStart, COLORREF crGrid)
#else
VOID CGraphCtrl::DrawGridPoints (FLOAT fMove, FLOAT xStart, FLOAT yStart, COLORREF crGrid)
#endif
{
	if(fMove >= 5.0f && m_fScale >= DPI::Scale(2.0))
	{
#ifdef	GRAPH_CTRL_QUX
		GpGraphics* pgpGraphics = m_pcSurface->pgpGraphics;
		GpSolidFill* pgpFill;
		DllExports::GdipCreateSolidFill(ColorRefToARGB(crGrid), &pgpFill);
#endif

		for(FLOAT yMap = yStart; yMap < m_nHeight; yMap += fMove)
		{
			for(FLOAT xMap = xStart; xMap < m_nWidth; xMap += fMove)
			{
				INT x = (INT)xMap;
				INT y = (INT)yMap;

#ifndef	GRAPH_CTRL_QUX
				SetPixelV(hdc, x, y - 1, crGrid);
				SetPixelV(hdc, x - 1, y, crGrid);
				SetPixelV(hdc, x, y, crGrid);
				SetPixelV(hdc, x + 1, y, crGrid);
				SetPixelV(hdc, x, y + 1, crGrid);
#else
				GdipSetPixel(pgpGraphics, m_rcPos, x, y - 1, pgpFill);
				GdipSetPixel(pgpGraphics, m_rcPos, x - 1, y, pgpFill);
				GdipSetPixel(pgpGraphics, m_rcPos, x, y, pgpFill);
				GdipSetPixel(pgpGraphics, m_rcPos, x + 1, y, pgpFill);
				GdipSetPixel(pgpGraphics, m_rcPos, x, y + 1, pgpFill);
#endif
			}
		}

#ifdef	GRAPH_CTRL_QUX
		DllExports::GdipDeleteBrush(pgpFill);
#endif
	}
	else
	{
#ifdef	GRAPH_CTRL_QUX
		GpGraphics* pgpGraphics = m_pcSurface->pgpGraphics;
		GpSolidFill* pgpFill;
		DllExports::GdipCreateSolidFill(ColorRefToARGB(crGrid), &pgpFill);
#endif

		for(FLOAT yMap = yStart; yMap < m_nHeight; yMap += fMove)
		{
			for(FLOAT xMap = xStart; xMap < m_nWidth; xMap += fMove)
#ifndef	GRAPH_CTRL_QUX
				SetPixelV(hdc, (INT)xMap, (INT)yMap, crGrid);
#else
				GdipSetPixel(pgpGraphics, m_rcPos, (INT)xMap, (INT)yMap, pgpFill);
#endif
		}

#ifdef	GRAPH_CTRL_QUX
		DllExports::GdipDeleteBrush(pgpFill);
#endif
	}
}

BOOL CGraphCtrl::WheelZoom (SHORT sDistance, FLOAT xGraph, FLOAT yGraph, POINT& ptClient)
{
	BOOL fHandled = FALSE;

	if(GRID_FLAG_ENABLE_ZOOMING & m_iGridFlags)
	{
		m_nMouseWheel += sDistance;
		if(m_nMouseWheel != 0)
		{
			UINT cScrollSize = 0;

			if(SystemParametersInfo(SPI_GETWHEELSCROLLLINES,0,&cScrollSize,0) == FALSE)
			{
				cScrollSize = 3;
			}

			if(cScrollSize > 0 && cScrollSize != 0xFFFFFFFF /* WHEEL_PAGE */)
			{
				FLOAT fScrollSize = (FLOAT)cScrollSize;
				FLOAT fDetent = (FLOAT)WHEEL_DELTA / fScrollSize;
				FLOAT fTicks = (FLOAT)abs(m_nMouseWheel) / fDetent;
				FLOAT fZoomTick = m_fZoomChange / fScrollSize;
				FLOAT fTotalZoom = fTicks * fZoomTick;
				FLOAT fScale = GetScale();

				if(sDistance < 0)
				{
					m_nMouseWheel += (INT)(fTicks * fDetent);
					SetScale(fScale / fTotalZoom);
				}
				else if(sDistance > 0)
				{
					m_nMouseWheel -= (INT)(fTicks * fDetent);
					SetScale(fScale * fTotalZoom);
				}

				if(GetScale() != fScale)
				{
					if(GRID_FLAG_ZOOM_MOUSE & m_iGridFlags)
					{
						FLOAT xGraphAfter, yGraphAfter;
						FLOAT xShift, yShift;

						ClientToGraph(ptClient.x, ptClient.y, xGraphAfter, yGraphAfter);
						xShift = GridPoint(xGraph, m_iGridSpace) - xGraphAfter;
						if(GRAPH_XZ == m_gtGraphType)
							yShift = yGraphAfter - GridPoint(yGraph, m_iGridSpace);
						else if(GRAPH_PINNED_Y == m_gtGraphType)
							yShift = 0;
						else
							yShift = GridPoint(yGraph, m_iGridSpace) - yGraphAfter;
						SetCenter(m_fxCenter + xShift, m_fyCenter + yShift);
					}

					fHandled = TRUE;
				}
			}
			else
				m_nMouseWheel = 0;
		}
	}

	return fHandled;
}

#ifndef	GRAPH_CTRL_QUX

VOID CGraphCtrl::FreeDoubleBuffers (VOID)
{
	if(m_hbmGraph)
	{
		if(m_hdcGraph)
		{
			SelectObject(m_hdcGraph,m_hbmGraphPrev);
			m_hbmGraphPrev = NULL;
			DeleteDC(m_hdcGraph);
			m_hdcGraph = NULL;
		}
		DeleteObject(m_hbmGraph);
		m_hbmGraph = NULL;

		m_lpGraphPtr = NULL;
		m_lGraphPitch = 0;
	}
}

#endif

//

VOID CGraphCtrl::RebuildOffsets (VOID)
{
	if(GRAPH_PINNED_Y == m_gtGraphType || GRAPH_INVERTED_QUAD == m_gtGraphType)
	{
		m_xOffset = 0;
		m_yOffset = 0;
	}
	else
	{
		m_xOffset = m_nWidth >> 1;
		m_yOffset = m_nHeight >> 1;
	}
}

INT CGraphCtrl::CloserTo (INT i, INT a, INT b)
{
	INT da = abs(i - a);
	INT db = abs(i - b);
	if(da < db)
		return -1;
	if(da > db)
		return 1;
	return 0;
}

FLOAT CGraphCtrl::GridPoint (FLOAT p, INT iGrid)
{
	INT i;
	FLOAT dp = p - (FLOAT)((INT)p);

	if(fabs(dp) < 0.5)
		p -= dp;
	else if(p > 0)
		p += (1 - (FLOAT)fabs(dp));
	else
		p -= (1 - (FLOAT)fabs(dp)); 

	if(p < 0)
	{
		i = abs((INT)p) % iGrid;
		if(CloserTo(i,0,iGrid) >= 0)
			p -= (iGrid - i);
		else
			p += i;
	}
	else if(p > 0)
	{
		i = (INT)p % iGrid;
		if(CloserTo(i,0,iGrid) < 0)
			p -= i;
		else
			p += (iGrid - i);
	}

	return p;
}
