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
	struct SEGMENT_INTERSECTION
	{
		FPOINT pt;
		DWORD idLine;
		FLOAT t;
	};

private:
	ICADHost* m_pHost;

	VertexMap* m_pVertices;
	LineMap* m_pLines;
	PolygonMap* m_pPolygons;

	IGrapher* m_pGraph;
	CAD::Mode m_eMode;

	FLOAT m_xMouse, m_yMouse;
	CAD::Mouse m_eMouse;

	DWORD m_idNewPolygon;
	TArray<XYPOINT> m_aNewPoints;

	bool m_fDrawActive;
	DWORD m_idDrawStartVertex;
	DWORD m_idDrawLastVertex;
	TArray<DWORD> m_aDrawVertices;
	TArray<DWORD> m_aDrawLines;

	HPEN m_pnNormal;
	HPEN m_pnTwoSided;
	HPEN m_pnSelected;
	HPEN m_pnHover;
	HPEN m_pnDrawing;

public:
	CCADCtrl(ICADHost* pHost, VertexMap* pVertices, LineMap* pLines, PolygonMap* pPolygons, IGrapher* pGraph, CAD::Mode eMode);
	~CCADCtrl();

	VOID SetMode (CAD::Mode eMode);

	CAD::Mode GetMode (VOID) const { return m_eMode; }

	DWORD GetFreeVertex (VOID);
	DWORD GetFreeLine (VOID);
	DWORD GetFreePolygon(VOID);

	HRESULT AddVertex (DWORD idVertex, const FPOINT& vertex);
	HRESULT AddLine (DWORD idLine, DWORD idVertexA, DWORD idVertexB, DWORD idPolygonA, DWORD idPolygonB, ICADLine* pLineA, ICADLine* pLineB);
	HRESULT AddPolygon (DWORD idPolygon, ICADPolygon* pPolygon);

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
	DWORD GetPolygonFromLine (FPOINT a, FPOINT b, DWORD idIgnore);
	DWORD GetPolygonFromPoint (FLOAT x, FLOAT y, DWORD idIgnore);
	VOID DeselectAll (UINT nRemove = CAD_SELECTED | CAD_HOVER | CAD_DRAWING);
	HRESULT SplitLine (FLOAT x, FLOAT y, __out_opt DWORD* pidVertex);
	HRESULT FindOrCreateVertex (FLOAT x, FLOAT y, __out DWORD* pidVertex);

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
	static void ToggleSelection (UINT& nFlags, BOOL fSelect, __inout BOOL& fChanged);
	static FLOAT CrossProduct (const FPOINT& a, const FPOINT& b, FLOAT x, FLOAT y);

	HRESULT DiffPolygon (DWORD subPolygonKey, DWORD clipPolygonKey);
	HRESULT GetPolygonPoints (DWORD polygonId, __out TArray<FPOINT>& polygonPoints);
	HRESULT OrderPolygonPoints (DWORD polygonId, TArray<DWORD>& polygon, __out TArray<DWORD>& orderedPolygon);
	VOID SetPolygonIDToLine (DWORD idVertexA, DWORD idVertexB, DWORD subPolygonKey, DWORD clipPoligonKey);

	VOID ResetDrawState (VOID);
	HRESULT StartDrawMode (FLOAT x, FLOAT y);
	HRESULT CommitDrawClick (FLOAT x, FLOAT y);
	HRESULT FinishDrawMode (VOID);
	VOID CancelDrawMode (VOID);
	BOOL UndoLastDrawStep (VOID);

	HRESULT IntegrateDrawSegment (DWORD idVertexA, DWORD idVertexB, __out_opt DWORD* pidLine = NULL);
	HRESULT GetVertexPoint (DWORD idVertex, __out FPOINT& pt);
	bool SegmentIntersection (const FPOINT& a1, const FPOINT& a2, const FPOINT& b1, const FPOINT& b2, __out FPOINT& out, __out_opt FLOAT* ptA = NULL);
	HRESULT FindSegmentIntersections (const FPOINT& a, const FPOINT& b, DWORD idVertexA, DWORD idVertexB, __out TArray<SEGMENT_INTERSECTION>& intersections);
	HRESULT SortAndUniqueIntersections (__inout TArray<SEGMENT_INTERSECTION>& intersections);
	DWORD FindExistingLine (DWORD idVertexA, DWORD idVertexB);
	VOID RemoveLineById (DWORD idLine);
	bool IsLineUsedByPolygon (const CAD_LINE* pCadLine, DWORD polygonId) const;
	FLOAT PolygonSignedArea (const TArray<FPOINT>& pts) const;
	VOID EnsureClockwise (__inout TArray<FPOINT>& pts);
	bool PolygonExists (DWORD polygonId) const;
	TArray<DWORD> FindIntersectingLines (const FPOINT& vFrom, const FPOINT& vTo);
	FLOAT PolygonArea (const TArray<FPOINT>& polygon);
	void DeleteSoloVertexes (TArray<DWORD> &usedVertices);
	HRESULT IntegrateLine (DWORD idVertexA, DWORD idVertexB, __out_opt DWORD* pidLastLine);
	bool IsSegmentOnPolygonBoundary (DWORD polygonId, TArray<FPOINT> &newPolyPts, const FPOINT& a, const FPOINT& b, __out_opt int* pId);
	HRESULT GetPolygonLines (DWORD polygonId, __out TArray<DWORD>& polygonLines);
	bool IsPointInPolygonLines (const TArray<DWORD>& polygonLines, const FPOINT& pt);

	void TraceLoop (TArray<DWORD>& lines, TArray<DWORD>& usedLines,  __out_opt TArray<FPOINT>& points);

	HRESULT BuildPolygonLoops (DWORD polygonId, TArray<DWORD>& allLines, __out_opt TArray<FPOINT>& outer, __out_opt TArray<TArray<FPOINT>*>& holes);

	bool IsPointInPolygonWithHoles (DWORD polygonId, const FPOINT& pt);
};
