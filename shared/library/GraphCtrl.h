#pragma once

#ifdef	GRAPH_CTRL_QUX
	#include "..\..\platform\QuadooUI\Published\QUX.h"
#endif

#define	GRAPH_CTRL_DEF_SCALE			2.0f
#define	GRAPH_CTRL_DEF_GRID_SPACING		16
#define	GRAPH_CTRL_DEF_GRID_MIN			2
#define	GRAPH_CTRL_DEF_GRID_MAX			8192

#ifndef	GRAPH_CTRL_QUX

#define	GRAPH_CTRL_DEF_BACKGROUND		RGB(0,0,0)
#define	GRAPH_CTRL_DEF_GRID_COLOR		RGB(0,0,128)
#define	GRAPH_CTRL_DEF_AXIS_COLOR		RGB(192,192,220)

#else

#define	GRAPH_CTRL_DEF_BACKGROUND		(0xFF << 24) | RGB(0,0,0)
#define	GRAPH_CTRL_DEF_GRID_COLOR		(0xFF << 24) | RGB(0,0,128)
#define	GRAPH_CTRL_DEF_AXIS_COLOR		(0xFF << 24) | RGB(192,192,220)

#endif

typedef enum
{
	GRAPH_INVALID,
	GRAPH_XY,
	GRAPH_XZ,
	GRAPH_ZY,
	GRAPH_PINNED_Y,
	GRAPH_INVERTED_QUAD
} EGRAPHTYPE;

#define	GRID_NONE						0x00000000
#define	GRID_POINTS						0x00000001
#define	GRID_LINES						0x00000002
#define	GRID_AXIS						0x00000003
#define	GRID_AXIS_POINTS				0x00000004

#define	GRID_TYPE_MASK					0x00000007

#define	GRID_FLAG_ZOOM_MOUSE			0x00000008
#define	GRID_FLAG_ZOOM_ON_SIZE			0x00000010
#define	GRID_FLAG_ENABLE_ZOOMING		0x00000020
#define	GRID_FLAG_ENABLE_POINT_SHADER	0x00000040
#define	GRID_FLAG_SHADE_MOUSE_POINT		0x00000080

interface IAccessible;

interface IGrapher
{
#ifndef	GRAPH_CTRL_QUX
	virtual HPEN SelectPen (HPEN hpnPen) = 0;
	virtual HBRUSH SelectBrush (HBRUSH hbrBrush) = 0;
	virtual COLORREF SetTextColor (COLORREF cr) = 0;
	virtual HFONT SelectFont (HFONT hfFont) = 0;

	virtual VOID MoveTo (FLOAT x, FLOAT y, FLOAT z) = 0;
	virtual VOID LineTo (FLOAT x, FLOAT y, FLOAT z) = 0;
	virtual VOID Rectangle (FLOAT x1, FLOAT y1, FLOAT z1, FLOAT x2, FLOAT y2, FLOAT z2) = 0;
	virtual VOID FillRect (FLOAT x1, FLOAT y1, FLOAT z1, FLOAT x2, FLOAT y2, FLOAT z2, HBRUSH hBrush) = 0;
	virtual VOID RoundRect (FLOAT x1, FLOAT y1, FLOAT z1, FLOAT x2, FLOAT y2, FLOAT z2, FLOAT fWidth, FLOAT fHeight) = 0;
	virtual VOID Ellipse (FLOAT x1, FLOAT y1, FLOAT z1, FLOAT x2, FLOAT y2, FLOAT z2) = 0;
	virtual VOID TextOut (FLOAT x, FLOAT y, FLOAT z, LPCTSTR pctzText, INT cchText) = 0;
	virtual VOID TextAlign (FLOAT x1, FLOAT y1, FLOAT z1, FLOAT x2, FLOAT y2, FLOAT z2, LPCTSTR pctzText, INT cchText, INT dtAlign) = 0;
	virtual VOID LabelText (FLOAT x, FLOAT y, FLOAT z, LPCTSTR pctzText, INT cchText) = 0;
	virtual VOID LabelTextAbove (FLOAT x, FLOAT y, FLOAT z, LPCTSTR pctzText, INT cchText) = 0;
	virtual VOID LabelCenterTextAbove (FLOAT x, FLOAT y, FLOAT z, LPCTSTR pctzText, INT cchText) = 0;

	virtual HDC GetClientDC (VOID) = 0;
#else
	virtual const DIBSURFACE* GetClientSurface (VOID) = 0;

	virtual VOID DrawLine (FLOAT x1, FLOAT y1, FLOAT z1, FLOAT x2, FLOAT y2, FLOAT z2, GpPen* pgpPen) = 0;
	virtual VOID FillRect (FLOAT x1, FLOAT y1, FLOAT z1, FLOAT x2, FLOAT y2, FLOAT z2, GpBrush* pgpFill) = 0;
	virtual VOID RoundRect (FLOAT x1, FLOAT y1, FLOAT z1, FLOAT x2, FLOAT y2, FLOAT z2, FLOAT rWidth, FLOAT rHeight, GpPen* pgpPen, GpBrush* pgpFill) = 0;
#endif

	virtual BOOL GetRawBuffer (LPBYTE& lpBuffer, INT& nWidth, INT& nHeight, LONG& lPitch) = 0;
	virtual VOID GraphToClient (FLOAT gx, FLOAT gy, INT& x, INT& y) = 0;
	virtual VOID GraphToClientPoint (FLOAT gx, FLOAT gy, POINT& pt) = 0;
	virtual VOID GraphToClientRect (RECT* lprcGraph, RECT* lprcClient) = 0;
	virtual BOOL IsObjectVisible (FLOAT gxLeft, FLOAT gyTop, FLOAT gxRight, FLOAT gyBottom, RECT* lprcClient) = 0;
	virtual VOID GetGraphCenter (FLOAT* lpx, FLOAT* lpy) = 0;
	virtual FLOAT GetScale (VOID) = 0;
	virtual VOID GetGraphBounds (__out FLOAT* px1, __out FLOAT* py1, __out FLOAT* px2, __out FLOAT* py2) = 0;
	virtual EGRAPHTYPE GetGraphType (VOID) = 0;
	virtual FLOAT GetGridSnap (FLOAT p) = 0;
	virtual VOID SetFlag (INT iFlag, BOOL fSet) = 0;
	virtual INT GetFlags (VOID) = 0;
	virtual VOID SetGridSpacing (INT iSpacing) = 0;
	virtual INT GetGridSpacing (VOID) = 0;
};

#ifndef	GRAPH_CTRL_QUX
	#define	KEYS	DWORD
#else
	#define	KEYS	QUXInputEvent::Modifiers
#endif

interface IGraphClient
{
	virtual VOID onGraphPaint (IGrapher* lpGraph) = 0;
	virtual VOID onGraphMouseMove (KEYS dwKeys, FLOAT x, FLOAT y) = 0;
	virtual VOID onGraphLBtnDown (KEYS dwKeys, FLOAT x, FLOAT y) = 0;
	virtual VOID onGraphLBtnUp (KEYS dwKeys, FLOAT x, FLOAT y) = 0;
	virtual BOOL onGraphRBtnDown (KEYS dwKeys, FLOAT x, FLOAT y) = 0;
	virtual VOID onGraphRBtnUp (KEYS dwKeys, FLOAT x, FLOAT y) = 0;
	virtual VOID onGraphLBtnDbl (KEYS dwKeys, FLOAT x, FLOAT y) = 0;
	virtual VOID onGraphRBtnDbl (KEYS dwKeys, FLOAT x, FLOAT y) = 0;
	virtual VOID onGraphViewChanged (BOOL fZoomChanged) = 0;
	virtual BOOL onGraphKeyDown (WPARAM iKey) = 0;
	virtual BOOL onGraphKeyUp (WPARAM iKey) = 0;
	virtual BOOL onGraphChar (WPARAM iKey) = 0;
#ifndef	GRAPH_CTRL_QUX
	virtual BOOL onGraphWheel (SHORT sDistance, FLOAT x, FLOAT y) = 0;
#else
	virtual BOOL onGraphWheel (SHORT sDistance, KEYS dwKeys, FLOAT x, FLOAT y) = 0;
#endif
	virtual HRESULT onGraphGetAcc (IAccessible** lplpAccessible) = 0;
};

interface IGraphContainer
{
	virtual VOID WINAPI OnScaleChanged (FLOAT fScale) = 0;
	virtual VOID WINAPI OnGridSpacingChanged (INT iSpacing) = 0;
	virtual HRESULT WINAPI SetFocus (__in IGrapher* pGraphCtrl) = 0;
	virtual HRESULT WINAPI InvalidateContainer (__in IGrapher* pGraphCtrl) = 0;
#ifndef	GRAPH_CTRL_QUX
	virtual HRESULT WINAPI ScreenToWindowless (__in IGrapher* pGraphCtrl, __inout PPOINT ppt) = 0;
	virtual BOOL WINAPI CaptureMouse (__in IGrapher* pGraphCtrl, BOOL fCapture) = 0;
	virtual VOID WINAPI DrawDib24 (HDC hdcDest, LONG x, LONG y, HDC hdcSrc, const BYTE* pcDIB24, LONG xSize, LONG ySize, LONG lPitch) = 0;
#endif
};

#ifndef	GRAPH_CTRL_QUX
interface IGraphBackground
{
	virtual VOID WINAPI PaintBackground (HDC hdc, RECT* prc, HBRUSH hbrDefault) = 0;
};
#endif

class CGraphCtrl :
	public IGrapher
#ifndef	GRAPH_CTRL_QUX
	,public IGraphBackground
#endif
{
private:
	EGRAPHTYPE m_gtGraphType;
	IGraphContainer* m_lpContainer;
	IGraphClient* m_lpClient;
#ifndef	GRAPH_CTRL_QUX
	IGraphBackground* m_pBackground;

	HBITMAP m_hbmGraph;
	HBITMAP m_hbmGraphPrev;
	HDC m_hdcGraph;
	LPBYTE m_lpGraphPtr;
	LONG m_lGraphPitch;
#else
	const DIBSURFACE* m_pcSurface;
#endif
	INT m_nWidth;
	INT m_nHeight;
	RECT m_rcPos;

#ifndef	GRAPH_CTRL_QUX
	HBRUSH m_hbBackground;
	HPEN m_hpGrid;
#else
	GpPen* m_pgpGrid;
#endif
	COLORREF m_crGrid;
	COLORREF m_crAxisPoint;
	COLORREF m_crPointShade;

	INT m_iGridFlags;
	INT m_iGridSpace;

	INT m_xOffset, m_yOffset;
	FLOAT m_fScale;
	FLOAT m_fxCenter, m_fyCenter;

	FLOAT m_fZoomChange;
	INT m_nMouseWheel;

	BOOL m_fIsDraggingCanvas;
	SIZE* m_pGraphDrag;

	INT m_iMaxSpacing, m_iMinSpacing;

	// GRID_FLAG_ENABLE_POINT_SHADER and GRID_FLAG_SHADE_MOUSE_POINT
	BOOL m_fShadePoint;
	FLOAT m_fxShadePoint, m_fyShadePoint;

public:
	CGraphCtrl ();
	~CGraphCtrl ();

	VOID SetGraphTarget (IGraphClient* lpClient);
#ifndef GRAPH_CTRL_QUX
	VOID SetGraphBackground (IGraphBackground* pBackground);
#endif
	VOID SetGraphType (EGRAPHTYPE gtGraphType);
	BOOL SetZoomChange (FLOAT fZoomChange);
	VOID SetMinMaxSpacing (INT iMaxSpacing, INT iMinSpacing);
	VOID ResetPosition (VOID);

	VOID WINAPI AttachContainer (IGraphContainer* lpContainer);
#ifndef GRAPH_CTRL_QUX
	VOID WINAPI Paint (HDC hdc);
#else
	VOID WINAPI Paint (const DIBSURFACE* pcSurface, BYTE bAlpha);
#endif
	VOID WINAPI Move (const RECT* pcrcPosition);
	VOID WINAPI GetPosition (LPRECT lpPosition);
	VOID WINAPI SizeObject (INT nWidth, INT nHeight);
#ifndef GRAPH_CTRL_QUX
	BOOL WINAPI OnMessage (UINT msg, WPARAM wParam, LPARAM lParam, __out LRESULT& lResult);
#else
	VOID WINAPI OnInput (__inout QUXInputEvent* pqie);
#endif
	HRESULT WINAPI GetAccObject (IAccessible** lplpAccessible);

	// IGrapher
#ifndef	GRAPH_CTRL_QUX
	virtual HPEN SelectPen (HPEN hpnPen);
	virtual HBRUSH SelectBrush (HBRUSH hbrBrush);
	virtual COLORREF SetTextColor (COLORREF cr);
	virtual HFONT SelectFont (HFONT hfFont);

	virtual VOID MoveTo (FLOAT x, FLOAT y, FLOAT z);
	virtual VOID LineTo (FLOAT x, FLOAT y, FLOAT z);
	virtual VOID Rectangle (FLOAT x1, FLOAT y1, FLOAT z1, FLOAT x2, FLOAT y2, FLOAT z2);
	virtual VOID FillRect (FLOAT x1, FLOAT y1, FLOAT z1, FLOAT x2, FLOAT y2, FLOAT z2, HBRUSH hBrush);
	virtual VOID RoundRect (FLOAT x1, FLOAT y1, FLOAT z1, FLOAT x2, FLOAT y2, FLOAT z2, FLOAT fWidth, FLOAT fHeight);
	virtual VOID Ellipse (FLOAT x1, FLOAT y1, FLOAT z1, FLOAT x2, FLOAT y2, FLOAT z2);
	virtual VOID TextOut (FLOAT x, FLOAT y, FLOAT z, LPCTSTR pctzText, INT cchText);
	virtual VOID TextAlign (FLOAT x1, FLOAT y1, FLOAT z1, FLOAT x2, FLOAT y2, FLOAT z2, LPCTSTR pctzText, INT cchText, INT dtAlign);
	virtual VOID LabelText (FLOAT x, FLOAT y, FLOAT z, LPCTSTR pctzText, INT cchText);
	virtual VOID LabelTextAbove (FLOAT x, FLOAT y, FLOAT z, LPCTSTR pctzText, INT cchText);
	virtual VOID LabelCenterTextAbove (FLOAT x, FLOAT y, FLOAT z, LPCTSTR pctzText, INT cchText);

	virtual HDC GetClientDC (VOID);
#else
	virtual const DIBSURFACE* GetClientSurface (VOID);

	virtual VOID DrawLine (FLOAT x1, FLOAT y1, FLOAT z1, FLOAT x2, FLOAT y2, FLOAT z2, GpPen* pgpPen);
	virtual VOID FillRect (FLOAT x1, FLOAT y1, FLOAT z1, FLOAT x2, FLOAT y2, FLOAT z2, GpBrush* pgpFill);
	virtual VOID RoundRect (FLOAT x1, FLOAT y1, FLOAT z1, FLOAT x2, FLOAT y2, FLOAT z2, FLOAT rWidth, FLOAT rHeight, GpPen* pgpPen, GpBrush* pgpFill);
#endif

	virtual BOOL GetRawBuffer (LPBYTE& lpBuffer, INT& nWidth, INT& nHeight, LONG& lPitch);
	virtual VOID GraphToClient (FLOAT gx, FLOAT gy, INT& x, INT& y);
	virtual VOID GraphToClientPoint (FLOAT gx, FLOAT gy, POINT& pt);
	virtual VOID GraphToClientRect (RECT* lprcGraph, RECT* lprcClient);
	virtual BOOL IsObjectVisible (FLOAT gxLeft, FLOAT gyTop, FLOAT gxRight, FLOAT gyBottom, RECT* lprcClient);
	virtual VOID GetGraphCenter (FLOAT* lpx, FLOAT* lpy);
	virtual FLOAT GetScale (VOID);
	virtual VOID GetGraphBounds (__out FLOAT* px1, __out FLOAT* py1, __out FLOAT* px2, __out FLOAT* py2);
	virtual EGRAPHTYPE GetGraphType (VOID);
	virtual FLOAT GetGridSnap (FLOAT p);
	virtual VOID SetFlag (INT iFlag, BOOL fSet);
	virtual INT GetFlags (VOID);
	virtual VOID SetGridSpacing (INT iSpacing);
	virtual INT GetGridSpacing (VOID);

#ifndef	GRAPH_CTRL_QUX
	// IGraphBackground
	virtual VOID WINAPI PaintBackground (HDC hdc, RECT* prc, HBRUSH hbrDefault);
#endif

public:
#ifndef GRAPH_CTRL_QUX
	VOID SetBGColor (COLORREF crBackground);
#endif
	VOID SetGridColor (COLORREF crGrid);
	VOID SetAxisPointColor (COLORREF crAxisPoint);

	VOID GetClientSize (INT* lpnWidth, INT* lpnHeight);
	VOID ShiftCenter (INT xShift, INT yShift);						// Client pixels
	VOID ShiftCenter (FLOAT rxShift, FLOAT ryShift);				// Graph points
	VOID SetCenter (FLOAT rx, FLOAT ry);
	VOID GetCenter (__out_opt FLOAT* prx, __out_opt FLOAT* pry);	// Graph points
	VOID SetScale (FLOAT fScale);

	VOID ClientToGraph (INT x, INT y, FLOAT& fx, FLOAT& fy);
	VOID ClientToGraphGrid (INT x, INT y, FLOAT& fx, FLOAT& fy);

	VOID SetGridType (INT iGridType);
	INT GetGridType (VOID);
	VOID ToggleGridType (VOID);

#ifndef	GRAPH_CTRL_QUX
	VOID DrawGrid (HDC hdc, INT x, INT y);
#else
	VOID DrawGrid (INT x, INT y);
#endif

private:
	// PointToClient() is templatized in an effort to avoid creating multiple versions
	// where some GDI functions use INT and others use LONG via RECT.
	template <typename T>
	VOID PointToClient (FLOAT x, FLOAT y, FLOAT z, T& xClient, T& yClient);

#ifndef	GRAPH_CTRL_QUX
	VOID DrawGridPoints (HDC hdc, FLOAT fMove, FLOAT xStart, FLOAT yStart, COLORREF crGrid);
#else
	VOID DrawGridPoints (FLOAT fMove, FLOAT xStart, FLOAT yStart, COLORREF crGrid);
#endif
	BOOL WheelZoom (SHORT sDistance, FLOAT xGraph, FLOAT yGraph, POINT& ptClient);

#ifndef	GRAPH_CTRL_QUX
	VOID FreeDoubleBuffers (VOID);
#endif

private:
	VOID RebuildOffsets (VOID);
	VOID UpdateShadePoint (FLOAT fx, FLOAT fy);
};
