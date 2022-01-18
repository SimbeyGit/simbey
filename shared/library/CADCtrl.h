#pragma once

#include "Core\Map.h"
#include "Spatial\GeometryTypes.h"
#include "GraphCtrl.h"

#define	CAD_SELECTED	1
#define	CAD_HOVER		2
#define	CAD_DRAWING		4

#define	CAD_INVALID		(DWORD)-1

namespace CAD
{
	enum Mode
	{
		Vertex,
		Line,
		Polygon,
		Drawing
	};

	enum Mouse
	{
		None,
		Down,
		Dragging,
		DrawLine
	};
}

interface ICADLine : IUnknown
{
	virtual HRESULT Edit (VOID) = 0;
};

interface ICADPolygon : IUnknown
{
	virtual HRESULT Edit (VOID) = 0;
};

struct CAD_VERTEX
{
	UINT nFlags;
	FPOINT vertex;
};

struct CAD_LINE
{
	DWORD idVertexA;
	DWORD idVertexB;
	DWORD idPolygonA;
	DWORD idPolygonB;
	ICADLine* pLineA;
	ICADLine* pLineB;
	UINT nFlags;
};

struct CAD_POLYGON
{
	UINT nFlags;
	ICADPolygon* pPolygon;
};

struct XYPOINT
{
	FLOAT x;
	FLOAT y;
};

typedef TMap<DWORD, CAD_VERTEX*> VertexMap;
typedef TMap<DWORD, CAD_LINE*> LineMap;
typedef TMap<DWORD, CAD_POLYGON*> PolygonMap;

interface ICADHost : IGraphContainer
{
	virtual BOOL OnSplitLine (const CAD_LINE* pcLine, const FPOINT& fpSplit, ICADLine* pSplit, __deref_out ICADLine** ppNewSplit) = 0;
	virtual BOOL OnCreateLine (DWORD idPolygon, DWORD idNewLine, __deref_out ICADLine** ppLine) = 0;
	virtual BOOL OnCreatePolygon (DWORD idPolygon, __deref_out ICADPolygon** ppPolygon) = 0;

	virtual VOID OnBeginDrawing (VOID) = 0;
	virtual VOID OnDrawLineStatus (FLOAT rLength) = 0;
	virtual VOID OnEndDrawing (VOID) = 0;
};

class CCADCtrl
{
private:
	ICADHost* m_pHost;

	VertexMap* m_pVertices;
	LineMap* m_pLines;
	PolygonMap* m_pPolygons;

	IGrapher* m_pGraph;
	CAD::Mode m_eMode;

	FLOAT m_xMouse, m_yMouse;
	CAD::Mouse m_eMouse;

	DWORD m_idDrawFromVertex;
	DWORD m_idNewPolygon;
	TArray<XYPOINT> m_aNewPoints;

	HPEN m_pnNormal;
	HPEN m_pnTwoSided;
	HPEN m_pnSelected;
	HPEN m_pnHover;
	HPEN m_pnDrawing;

public:
	CCADCtrl(ICADHost* pHost, VertexMap* pVertices, LineMap* pLines, PolygonMap* pPolygons, IGrapher* pGraph, CAD::Mode eMode);
	~CCADCtrl();

	VOID SetMode(CAD::Mode eMode);

	CAD::Mode GetMode (VOID) const { return m_eMode; }

	DWORD GetFreeVertex(VOID);
	DWORD GetFreeLine(VOID);
	DWORD GetFreePolygon(VOID);

	HRESULT AddVertex(DWORD idVertex, const FPOINT& vertex);
	HRESULT AddLine(DWORD idLine, DWORD idVertexA, DWORD idVertexB, DWORD idPolygonA, DWORD idPolygonB, ICADLine* pLineA, ICADLine* pLineB);
	HRESULT AddPolygon(DWORD idPolygon, ICADPolygon* pPolygon);

	bool DeleteSelectedPolygon (VOID);
	bool DeleteSelectedLine (VOID);
	bool DeleteSelectedVertex (VOID);

	bool MergeVertex (DWORD vertexToDelete, DWORD vertexToStay);

	VOID FlipSelectedLines (VOID);

	// Graph Handlers
	VOID Paint (VOID);

	VOID MouseMove (KEYS dwKeys, FLOAT x, FLOAT y);
	VOID LBtnDown (KEYS dwKeys, FLOAT x, FLOAT y);
	VOID LBtnUp (KEYS dwKeys, FLOAT x, FLOAT y);
	BOOL RBtnDown (KEYS dwKeys, FLOAT x, FLOAT y);
	VOID RBtnUp (KEYS dwKeys, FLOAT x, FLOAT y);
	VOID LBtnDbl (KEYS dwKeys, FLOAT x, FLOAT y);
	BOOL KeyUp (WPARAM iKey);

	VOID SnapCoordinate (FLOAT& x, FLOAT& y);

private:
	DWORD GetPolygonFromPoint(FLOAT x, FLOAT y, DWORD idIgnore);
	VOID DeselectAll(UINT nRemove = CAD_SELECTED | CAD_HOVER | CAD_DRAWING);
	HRESULT SplitLine(FLOAT x, FLOAT y, __out_opt DWORD* pidVertex);
	HRESULT FindOrCreateVertex(FLOAT x, FLOAT y, __out DWORD* pidVertex);
	HRESULT IntegrateLine(DWORD idVertexA, DWORD idVertexB);

	bool IsPolygonClosed (DWORD idPolygon, DWORD idVertex);

	VOID CheckAdjacentPolygon (DWORD idPolygon);
	BOOL VertexHover (FLOAT x, FLOAT y);
	BOOL LineHover (FLOAT x, FLOAT y);
	BOOL PolygonHover (FLOAT x, FLOAT y);
	VOID VertexSelect(FLOAT x, FLOAT y, bool fDeselectOthers, bool fAllowToggle);
	VOID LineSelect (FLOAT x, FLOAT y, bool fDeselectOthers, bool fAllowToggle);
	VOID PolygonSelect (FLOAT x, FLOAT y, bool fDeselectOthers, bool fAllowToggle);

	VOID DrawVertexMode (VOID);
	VOID DrawLineMode (VOID);
	VOID DrawLineDrawing (VOID);
	VOID DrawLineSegment (const FPOINT& vFrom, const FPOINT& vTo);

	DWORD GetOtherVertexFromPosition (FLOAT x, FLOAT y, CAD_VERTEX* pCurrentVertex);

	bool FindVertex (const FPOINT& vertex, __out sysint* pidx);
	bool IsPointOver (FLOAT x, FLOAT y, const FPOINT* pVertex);

	static void AdjustVertex (CAD_VERTEX* pCadVertex, FLOAT xDelta, FLOAT yDelta);
	static bool IsPointEqual (const FPOINT& fpA, const FPOINT& fpB);
	static bool AnalyzeLine (CAD_LINE* pCadLine, DWORD idIgnore);
	static void ToggleSelection (UINT& nFlags, BOOL fSelect, __inout BOOL& fChanged);
};
