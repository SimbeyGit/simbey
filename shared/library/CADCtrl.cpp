#include <math.h>
#include <float.h>
#include <windows.h>
#include "Core\CoreDefs.h"
#include "Spatial\Geometry.h"
#include "CADCtrl.h"

#define MIN_DISTANCE_FROM_LINE 2.5F

static bool BetweenWithEpsilon (float a, float b, float v, float eps)
{
	return v >= min(a, b) - eps && v <= max(a, b) + eps;
}

static bool IsPointOnSegment (const FPOINT& p, const FPOINT& a, const FPOINT& b, float eps = 0.01f)
{
	float cross = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
	if(fabsf(cross) > eps)
		return false;

	float dot = (p.x - a.x) * (b.x - a.x) + (p.y - a.y) * (b.y - a.y);
	if(dot < -eps)
		return false;

	float lenSq = (b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y);
	if(dot - lenSq > eps)
		return false;

	return true;
}

FLOAT PointDistanceFromLine (FLOAT x1, FLOAT y1, FLOAT x2, FLOAT y2, FLOAT x3, FLOAT y3)
{
	float px = x2 - x1;
	float py = y2 - y1;
	float temp = (px * px) + (py * py);

	if(temp <= 1e-20f)
	{
		float dx = x3 - x1;
		float dy = y3 - y1;
		return sqrtf(dx * dx + dy * dy);
	}

	float u = ((x3 - x1) * px + (y3 - y1) * py) / temp;
	if(u > 1.0f)
		u = 1.0f;
	else if(u < 0.0f)
		u = 0.0f;

	float x = x1 + u * px;
	float y = y1 + u * py;

	float dx = x - x3;
	float dy = y - y3;
	return sqrtf(dx * dx + dy * dy);
}

bool IsPointInPolygon (const TArray<FPOINT>& polygon, const FPOINT& pt)
{
	int count = 0;
	sysint n = polygon.Length();

	for(sysint i = 0; i < n; ++i)
	{
		const FPOINT& a = polygon[i];
		const FPOINT& b = polygon[(i + 1) % n];

		if(a.x == b.x && a.y == b.y)
			continue;

		if(IsPointOnSegment(pt, a, b))
			return true;

		if(((a.y > pt.y) != (b.y > pt.y)) && (pt.x < (b.x - a.x) * (pt.y - a.y) / (b.y - a.y + 1e-20f) + a.x))
		{
			count++;
		}
	}

	return (count % 2) == 1;
}

float PointToSegmentDistance (const FPOINT& p, const FPOINT& a, const FPOINT& b)
{
	float dx = b.x - a.x;
	float dy = b.y - a.y;

	if(dx == 0 && dy == 0)
	{
		dx = p.x - a.x;
		dy = p.y - a.y;
		return sqrtf(dx * dx + dy * dy);
	}

	float t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / (dx * dx + dy * dy);
	t = max(0.0f, min(1.0f, t));

	float projX = a.x + t * dx;
	float projY = a.y + t * dy;

	dx = p.x - projX;
	dy = p.y - projY;

	return sqrtf(dx * dx + dy * dy);
}

float MinDistanceToPolygon (const TArray<FPOINT>& polygon, const FPOINT& pt)
{
	float minDist = FLT_MAX;
	for(sysint i = 0; i < polygon.Length(); i++)
	{
		const FPOINT& a = polygon[i];
		const FPOINT& b = polygon[(i + 1) % polygon.Length()];
		float dist = PointToSegmentDistance(pt, a, b);
		if(dist < minDist)
			minDist = dist;
	}
	return minDist;
}

FLOAT Cross (const FPOINT& a, const FPOINT& b, const FPOINT& c)
{
	return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

bool IsInside (const FPOINT& a, const FPOINT& b, const FPOINT& p)
{
	return Cross(a, b, p) >= 0;
}

FPOINT ComputeIntersection (const FPOINT& a1, const FPOINT& a2, const FPOINT& b1, const FPOINT& b2)
{
	FLOAT A1 = a2.y - a1.y;
	FLOAT B1 = a1.x - a2.x;
	FLOAT C1 = A1 * a1.x + B1 * a1.y;

	FLOAT A2 = b2.y - b1.y;
	FLOAT B2 = b1.x - b2.x;
	FLOAT C2 = A2 * b1.x + B2 * b1.y;

	FLOAT det = A1 * B2 - A2 * B1;
	FPOINT result = { 0, 0, 0 };
	if(det != 0)
	{
		result.x = (B2 * C1 - B1 * C2) / det;
		result.y = (A1 * C2 - A2 * C1) / det;
	}
	return result;
}

HRESULT PolygonIntersection (const TArray<FPOINT>& subject, const TArray<FPOINT>& clip, __out TArray<FPOINT>& output)
{
	HRESULT hr = S_OK;

	output.Clear();

	for(sysint i = 0; i < subject.Length(); i++)
		Check(output.Append(subject[i]));

	for(sysint i = 0; i < clip.Length(); i++)
	{
		TArray<FPOINT> input;
		input.Swap(output);

		if(input.Length() == 0)
			break;

		FPOINT A = clip[i];
		FPOINT B = clip[(i + 1) % clip.Length()];

		for(sysint j = 0; j < input.Length(); j++)
		{
			FPOINT P = input[j];
			FPOINT Q = input[(j + 1) % input.Length()];

			if(IsInside(A, B, Q))
			{
				if(!IsInside(A, B, P))
					Check(output.Append(ComputeIntersection(P, Q, A, B)));

				Check(output.Append(Q));
			}
			else if(IsInside(A, B, P))
			{
				Check(output.Append(ComputeIntersection(P, Q, A, B)));
			}
		}
	}

Cleanup:
	return hr;
}

CCADCtrl::CCADCtrl (ICADHost* pHost, VertexMap* pVertices, LineMap* pLines,
	PolygonMap* pPolygons, IGrapher* pGraph, CAD::Mode eMode) :
	m_pHost(pHost),
	m_pVertices(pVertices),
	m_pLines(pLines),
	m_pPolygons(pPolygons),
	m_pGraph(pGraph),
	m_eMode(eMode),
	m_xMouse(0.0f),
	m_yMouse(0.0f),
	m_eMouse(CAD::None),
	m_idNewPolygon(CAD_INVALID),
	m_fDrawActive(false),
	m_idDrawStartVertex(CAD_INVALID),
	m_idDrawLastVertex(CAD_INVALID)
{
	Assert(GRAPH_XY == pGraph->GetGraphType());

	m_pnNormal = CreatePen(PS_SOLID, 2, RGB(224, 224, 224));
	m_pnTwoSided = CreatePen(PS_SOLID, 1, RGB(144, 144, 144));
	m_pnSelected = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
	m_pnHover = CreatePen(PS_SOLID, 2, RGB(0, 255, 0));
	m_pnDrawing = CreatePen(PS_SOLID, 2, RGB(128, 128, 255));

	m_pGraph->SetFlag(GRID_FLAG_ENABLE_POINT_SHADER, CAD::Polygon != eMode);
}

CCADCtrl::~CCADCtrl ()
{
	DeleteObject(m_pnNormal);
	DeleteObject(m_pnTwoSided);
	DeleteObject(m_pnSelected);
	DeleteObject(m_pnHover);
	DeleteObject(m_pnDrawing);
}

VOID CCADCtrl::SetMode (CAD::Mode eMode)
{
	if(eMode != m_eMode)
	{
		if(m_fDrawActive && CAD::Drawing != eMode)
			CancelDrawMode();

		m_pGraph->SetFlag(GRID_FLAG_ENABLE_POINT_SHADER, CAD::Polygon != eMode);
		m_eMode = eMode;
		m_pHost->InvalidateContainer(m_pGraph);
	}
}

VOID CCADCtrl::SnapCoordinate (FLOAT& x, FLOAT& y)
{
	x = m_pGraph->GetGridSnap(x);
	y = m_pGraph->GetGridSnap(y);
}

DWORD CCADCtrl::GetFreeVertex (VOID)
{
	DWORD idVertex = 0;
	sysint cVertices = m_pVertices->Length();
	for(sysint i = 0; i < cVertices; i++)
	{
		DWORD id = m_pVertices->GetKey(i);
		if(id > idVertex)
			return idVertex;
		idVertex = id + 1;
	}
	return idVertex;
}

DWORD CCADCtrl::GetFreeLine (VOID)
{
	DWORD idLine = 0;
	sysint cLines = m_pLines->Length();
	for(sysint i = 0; i < cLines; i++)
	{
		DWORD id = m_pLines->GetKey(i);
		if(id > idLine)
			return idLine;
		idLine = id + 1;
	}
	return idLine;
}

DWORD CCADCtrl::GetFreePolygon (VOID)
{
	DWORD idPolygon = 0;
	sysint cPolygons = m_pPolygons->Length();
	for(sysint i = 0; i < cPolygons; i++)
	{
		DWORD id = m_pPolygons->GetKey(i);
		if(id > idPolygon)
			return idPolygon;
		idPolygon = id + 1;
	}
	return idPolygon;
}

HRESULT CCADCtrl::AddVertex (DWORD idVertex, const FPOINT& vertex)
{
	HRESULT hr;
	CAD_VERTEX* pCadVertex = __new CAD_VERTEX;

	CheckAlloc(pCadVertex);
	pCadVertex->nFlags = 0;
	pCadVertex->vertex = vertex;
	Check(m_pVertices->Add(idVertex, pCadVertex));
	pCadVertex = NULL;

Cleanup:
	__delete pCadVertex;
	return hr;
}

HRESULT CCADCtrl::AddLine (DWORD idLine, DWORD idVertexA, DWORD idVertexB, DWORD idPolygonA, DWORD idPolygonB, ICADLine* pLineA, ICADLine* pLineB)
{
	HRESULT hr;
	CAD_LINE* pCadLine = __new CAD_LINE;

	CheckAlloc(pCadLine);
	pCadLine->idVertexA = idVertexA;
	pCadLine->idVertexB = idVertexB;
	pCadLine->idPolygonA = idPolygonA;
	pCadLine->idPolygonB = idPolygonB;
	pCadLine->nFlags = 0;
	pCadLine->pLineA = pLineA;
	pCadLine->pLineB = pLineB;
	Check(m_pLines->Add(idLine, pCadLine));
	pCadLine = NULL;

	if(pLineA)
		pLineA->AddRef();
	if(pLineB)
		pLineB->AddRef();

Cleanup:
	__delete pCadLine;
	return hr;
}

HRESULT CCADCtrl::AddPolygon (DWORD idPolygon, ICADPolygon* pPolygon)
{
	HRESULT hr;
	CAD_POLYGON* pCadPolygon = __new CAD_POLYGON;

	CheckAlloc(pCadPolygon);
	pCadPolygon->nFlags = 0;
	pCadPolygon->pPolygon = pPolygon;
	Check(m_pPolygons->Add(idPolygon, pCadPolygon));
	pCadPolygon = NULL;

	if(pPolygon)
		pPolygon->AddRef();

Cleanup:
	__delete pCadPolygon;
	return hr;
}

VOID CCADCtrl::ResetDrawState (VOID)
{
	m_fDrawActive = false;
	m_idDrawStartVertex = CAD_INVALID;
	m_idDrawLastVertex = CAD_INVALID;
	m_idNewPolygon = CAD_INVALID;
	m_aDrawVertices.Clear();
	m_aDrawLines.Clear();
	m_aNewPoints.Clear();
}

HRESULT CCADCtrl::GetVertexPoint (DWORD idVertex, __out FPOINT& pt)
{
	HRESULT hr;
	CAD_VERTEX* pVertex = NULL;

	Check(m_pVertices->Find(idVertex, &pVertex));
	pt = pVertex->vertex;

Cleanup:
	return hr;
}

DWORD CCADCtrl::FindExistingLine (DWORD idVertexA, DWORD idVertexB)
{
	for(sysint i = 0; i < m_pLines->Length(); i++)
	{
		DWORD idLine = CAD_INVALID;
		CAD_LINE* pCadLine = NULL;
		SideAssertHr(m_pLines->GetKeyAndValue(i, &idLine, &pCadLine));

		if((pCadLine->idVertexA == idVertexA && pCadLine->idVertexB == idVertexB) ||
			(pCadLine->idVertexA == idVertexB && pCadLine->idVertexB == idVertexA))
		{
			return idLine;
		}
	}
	return CAD_INVALID;
}

VOID CCADCtrl::RemoveLineById (DWORD idLine)
{
	CAD_LINE* pCadLine;
	if(SUCCEEDED(m_pLines->Remove(idLine, &pCadLine)))
		__delete pCadLine;
}

bool CCADCtrl::SegmentIntersection (
	const FPOINT& a1,
	const FPOINT& a2,
	const FPOINT& b1,
	const FPOINT& b2,
	__out FPOINT& out,
	__out_opt FLOAT* ptA)
{
	const float eps = 0.01f;

	const float x1 = a1.x, y1 = a1.y;
	const float x2 = a2.x, y2 = a2.y;
	const float x3 = b1.x, y3 = b1.y;
	const float x4 = b2.x, y4 = b2.y;

	const float den = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);

	// Normal intersection
	if(fabsf(den) >= 1e-6f)
	{
		const float pre = (x1 * y2 - y1 * x2);
		const float post = (x3 * y4 - y3 * x4);

		const float px = (pre * (x3 - x4) - (x1 - x2) * post) / den;
		const float py = (pre * (y3 - y4) - (y1 - y2) * post) / den;

		if(!BetweenWithEpsilon(x1, x2, px, eps) || !BetweenWithEpsilon(y1, y2, py, eps) ||
			!BetweenWithEpsilon(x3, x4, px, eps) || !BetweenWithEpsilon(y3, y4, py, eps))
		{
			return false;
		}

		out.x = px;
		out.y = py;
		out.z = 0.0f;

		if(ptA)
		{
			const float dx = x2 - x1;
			const float dy = y2 - y1;
			const float len2 = dx * dx + dy * dy;
			if(len2 <= 1e-8f)
				*ptA = 0.0f;
			else
				*ptA = ((px - x1) * dx + (py - y1) * dy) / len2;
		}

		return true;
	}

	// Parallel or collinear
	// Check if b1 lies on segment a
	if(IsPointOnSegment(b1, a1, a2, eps))
	{
		out = b1;
		if(ptA)
		{
			const float dx = x2 - x1;
			const float dy = y2 - y1;
			const float len2 = dx * dx + dy * dy;
			if(len2 <= 1e-8f)
				*ptA = 0.0f;
			else
				*ptA = ((b1.x - x1) * dx + (b1.y - y1) * dy) / len2;
		}
		return true;
	}

	// Check if b2 lies on segment a
	if(IsPointOnSegment(b2, a1, a2, eps))
	{
		out = b2;
		if(ptA)
		{
			const float dx = x2 - x1;
			const float dy = y2 - y1;
			const float len2 = dx * dx + dy * dy;
			if(len2 <= 1e-8f)
				*ptA = 0.0f;
			else
				*ptA = ((b2.x - x1) * dx + (b2.y - y1) * dy) / len2;
		}
		return true;
	}

	// Check if a1 lies on segment b
	if(IsPointOnSegment(a1, b1, b2, eps))
	{
		out = a1;
		if(ptA)
			*ptA = 0.0f;
		return true;
	}

	// Check if a2 lies on segment b
	if(IsPointOnSegment(a2, b1, b2, eps))
	{
		out = a2;
		if(ptA)
			*ptA = 1.0f;
		return true;
	}

	return false;
}

HRESULT CCADCtrl::FindSegmentIntersections (
	const FPOINT& a,
	const FPOINT& b,
	DWORD idVertexA,
	DWORD idVertexB,
	__out TArray<SEGMENT_INTERSECTION>& intersections)
{
	HRESULT hr = S_OK;
	intersections.Clear();

	for(sysint i = 0; i < m_pLines->Length(); i++)
	{
		DWORD idLine = CAD_INVALID;
		CAD_LINE* pCadLine;
		CAD_VERTEX* pV1 = NULL;
		CAD_VERTEX* pV2 = NULL;

		Check(m_pLines->GetKeyAndValue(i, &idLine, &pCadLine));

		if((pCadLine->idVertexA == idVertexA && pCadLine->idVertexB == idVertexB) ||
			(pCadLine->idVertexA == idVertexB && pCadLine->idVertexB == idVertexA))
		{
			continue;
		}

		Check(m_pVertices->Find(pCadLine->idVertexA, &pV1));
		Check(m_pVertices->Find(pCadLine->idVertexB, &pV2));

		// 1. Normal crossing intersection
		{
			FPOINT hit;
			FLOAT t = 0.0f;
			if(SegmentIntersection(a, b, pV1->vertex, pV2->vertex, hit, &t))
			{
				if(!IsPointEqual(hit, a) && !IsPointEqual(hit, b) &&
					!IsPointEqual(hit, pV1->vertex) && !IsPointEqual(hit, pV2->vertex))
				{
					SEGMENT_INTERSECTION* pItem = NULL;
					Check(intersections.AppendSlot(&pItem));
					pItem->pt = hit;
					pItem->idLine = idLine;
					pItem->t = t;
				}
			}
		}

		// 2. Existing line endpoints lying on new segment
		if(IsPointOnSegment(pV1->vertex, a, b) &&
			!IsPointEqual(pV1->vertex, a) &&
			!IsPointEqual(pV1->vertex, b))
		{
			SEGMENT_INTERSECTION* pItem = NULL;
			Check(intersections.AppendSlot(&pItem));
			pItem->pt = pV1->vertex;
			pItem->idLine = idLine;

			const FLOAT dx = b.x - a.x;
			const FLOAT dy = b.y - a.y;
			const FLOAT len2 = dx * dx + dy * dy;
			if(len2 <= 1e-8f)
				pItem->t = 0.0f;
			else
				pItem->t = ((pV1->vertex.x - a.x) * dx + (pV1->vertex.y - a.y) * dy) / len2;
		}

		if(IsPointOnSegment(pV2->vertex, a, b) &&
			!IsPointEqual(pV2->vertex, a) &&
			!IsPointEqual(pV2->vertex, b))
		{
			SEGMENT_INTERSECTION* pItem = NULL;
			Check(intersections.AppendSlot(&pItem));
			pItem->pt = pV2->vertex;
			pItem->idLine = idLine;

			const FLOAT dx = b.x - a.x;
			const FLOAT dy = b.y - a.y;
			const FLOAT len2 = dx * dx + dy * dy;
			if(len2 <= 1e-8f)
				pItem->t = 0.0f;
			else
				pItem->t = ((pV2->vertex.x - a.x) * dx + (pV2->vertex.y - a.y) * dy) / len2;
		}

		// 3. NEW SEGMENT endpoints lying on existing line
		if(IsPointOnSegment(a, pV1->vertex, pV2->vertex) &&
			!IsPointEqual(a, pV1->vertex) &&
			!IsPointEqual(a, pV2->vertex))
		{
			SEGMENT_INTERSECTION* pItem = NULL;
			Check(intersections.AppendSlot(&pItem));
			pItem->pt = a;
			pItem->idLine = idLine;
			pItem->t = 0.0f;
		}

		if(IsPointOnSegment(b, pV1->vertex, pV2->vertex) &&
			!IsPointEqual(b, pV1->vertex) &&
			!IsPointEqual(b, pV2->vertex))
		{
			SEGMENT_INTERSECTION* pItem = NULL;
			Check(intersections.AppendSlot(&pItem));
			pItem->pt = b;
			pItem->idLine = idLine;
			pItem->t = 1.0f;
		}
	}

Cleanup:
	return hr;
}

HRESULT CCADCtrl::SortAndUniqueIntersections (__inout TArray<SEGMENT_INTERSECTION>& intersections)
{
	HRESULT hr = S_OK;

	for(sysint i = 0; i < intersections.Length(); i++)
	{
		for(sysint j = i + 1; j < intersections.Length(); j++)
		{
			if(intersections[j].t < intersections[i].t)
				SwapData(intersections[i], intersections[j]);
		}
	}

	for(sysint i = 1; i < intersections.Length(); )
	{
		if(IsPointEqual(intersections[i - 1].pt, intersections[i].pt))
			Check(intersections.RemoveChecked(i, NULL));
		else
			i++;
	}

Cleanup:
	return hr;
}

HRESULT CCADCtrl::StartDrawMode (FLOAT x, FLOAT y)
{
	HRESULT hr = S_OK;
	DWORD idStart = CAD_INVALID;

	if(m_fDrawActive)
		return S_FALSE;

	ResetDrawState();

	m_idNewPolygon = GetFreePolygon();

	TStackRef<ICADPolygon> srPolygon;
	CheckIf(!m_pHost->OnCreatePolygon(m_idNewPolygon, &srPolygon), E_FAIL);
	Check(AddPolygon(m_idNewPolygon, srPolygon));
	
	Check(FindOrCreateVertex(x, y, &idStart));

	m_fDrawActive = true;
	m_idDrawStartVertex = idStart;
	m_idDrawLastVertex = idStart;

	Check(m_aDrawVertices.Append(idStart));

	m_eMouse = CAD::DrawLine;
	m_pHost->OnBeginDrawing();
	
	FPOINT pt;
	Check(GetVertexPoint(idStart, pt));
	m_xMouse = pt.x;
	m_yMouse = pt.y;

	XYPOINT xy;
	xy.x = pt.x;
	xy.y = pt.y;
	Check(m_aNewPoints.Append(xy));
	DeselectAll();
	m_pHost->InvalidateContainer(m_pGraph);

Cleanup:
	return hr;
}

HRESULT CCADCtrl::IntegrateDrawSegment (DWORD idVertexA, DWORD idVertexB, __out_opt DWORD* pidLine)
{
	HRESULT hr = S_OK;

	if(pidLine)
		*pidLine = CAD_INVALID;

	if(idVertexA == idVertexB)
		return S_FALSE;

	const DWORD existingLine = FindExistingLine(idVertexA, idVertexB);
	if(existingLine != CAD_INVALID)
	{
		if(pidLine)
			*pidLine = existingLine;
	}
	else
	{
		Check(IntegrateLine(idVertexA, idVertexB, pidLine));
	}

Cleanup:
	return hr;
}

HRESULT CCADCtrl::CommitDrawClick (FLOAT x, FLOAT y)
{
	HRESULT hr = S_OK;
	DWORD idCurrent = CAD_INVALID;

	CheckIf(!m_fDrawActive, E_FAIL);

	Check(FindOrCreateVertex(x, y, &idCurrent));

	if(idCurrent == m_idDrawStartVertex && m_aDrawVertices.Length() >= 2)
	{
		DWORD idLine = CAD_INVALID;
		Check(IntegrateDrawSegment(m_idDrawLastVertex, m_idDrawStartVertex, &idLine));
		Check(m_aDrawVertices.Append(m_idDrawStartVertex));
		Check(FinishDrawMode());
		return S_OK;
	}

	if(idCurrent == m_idDrawLastVertex)
		return S_FALSE;

	DWORD idLine = CAD_INVALID;
	Check(IntegrateDrawSegment(m_idDrawLastVertex, idCurrent, &idLine));

	m_idDrawLastVertex = idCurrent;
	Check(m_aDrawVertices.Append(idCurrent));

	FPOINT pt;
	Check(GetVertexPoint(idCurrent, pt));
	XYPOINT xy;
	xy.x = pt.x;
	xy.y = pt.y;
	Check(m_aNewPoints.Append(xy));

Cleanup:
	return hr;
}

HRESULT CCADCtrl::FinishDrawMode (VOID)
{
	HRESULT hr = S_OK;
	TArray<FPOINT> newPolyPts;
	TArray<DWORD> singleSidedLines;
	TArray<DWORD> enclodingPolygons;

	TArray<DWORD> doubleSidedLines;
	TArray<DWORD> doubleSidedEnclosingPolygons;
	CheckIf(!m_fDrawActive, E_FAIL);

	// Build polygon points from m_aNewPoints, not m_aDrawVertices
	for(sysint i = 0; i < m_aNewPoints.Length(); i++)
	{
		FPOINT pPt;
		pPt.x = m_aNewPoints[i].x;
		pPt.y = m_aNewPoints[i].y;
		pPt.z = 0.0f;
		if(newPolyPts.Length() > 0 && IsPointEqual(pPt, newPolyPts[newPolyPts.Length() - 1]))
			continue;
		Check(newPolyPts.Append(pPt));
	}

	for(sysint i = 0; i < m_pLines->Length(); i++)
	{
		DWORD idLine = CAD_INVALID;
		CAD_LINE* pLine;
		CAD_VERTEX* pVA = NULL;
		CAD_VERTEX* pVB = NULL;

		Check(m_pLines->GetKeyAndValue(i, &idLine, &pLine));
		Check(m_pVertices->Find(pLine->idVertexA, &pVA));
		Check(m_pVertices->Find(pLine->idVertexB, &pVB));

		const FPOINT& a = pVA->vertex;
		const FPOINT& b = pVB->vertex;

		FPOINT mid;
		mid.x = (a.x + b.x) * 0.5f;
		mid.y = (a.y + b.y) * 0.5f;
		mid.z = 0.0f;

		DWORD idLeft = CAD_INVALID;
		DWORD idEnclosing = GetPolygonFromLine(a, b, m_idNewPolygon);
		if(newPolyPts.Length() >= 3 && IsPointInPolygon(newPolyPts, mid))
			idLeft = m_idNewPolygon;
		if(idLeft != CAD_INVALID)
		{
			if(idEnclosing != CAD_INVALID && idEnclosing != m_idNewPolygon)
			{
				int pId = -1;
				bool fSharedWithEnclosingBoundary = IsSegmentOnPolygonBoundary(idEnclosing, newPolyPts, a, b, &pId);
				if(fSharedWithEnclosingBoundary)
				{
					singleSidedLines.Append(idLine);
					enclodingPolygons.Append(pId > 0 ? m_idNewPolygon : idEnclosing);
				}
				else
				{
					doubleSidedLines.Append(idLine);
					doubleSidedEnclosingPolygons.Append(idEnclosing);
				}
			}
			else
			{
				singleSidedLines.Append(idLine);
				enclodingPolygons.Append(m_idNewPolygon);
			}
		}
	
		if(pLine->idPolygonA != CAD_INVALID && pLine->idPolygonB == CAD_INVALID)
		{
			SwapData(pLine->idPolygonA, pLine->idPolygonB);
		}

		if(pLine->idPolygonA == pLine->idPolygonB)
		{
			pLine->idPolygonA = CAD_INVALID;
		}
	}

	for(sysint i = 0; i < singleSidedLines.Length(); i++)
	{
		CAD_LINE* pLine;
		Check(m_pLines->Find(singleSidedLines[i], &pLine));
		pLine->idPolygonB = enclodingPolygons[i];
		pLine->idPolygonA = CAD_INVALID;
	}

	for(sysint i = 0; i < doubleSidedLines.Length(); i++)
	{
		CAD_LINE* pLine;
		Check(m_pLines->Find(doubleSidedLines[i], &pLine));
		pLine->idPolygonB = doubleSidedEnclosingPolygons[i];
		pLine->idPolygonA = m_idNewPolygon;
	}

	DeselectAll();
	m_fDrawActive = false;
	m_eMouse = CAD::None;
	m_pHost->OnEndDrawing();
	m_pHost->InvalidateContainer(m_pGraph);

Cleanup:
	m_aNewPoints.Clear();
	m_aDrawVertices.Clear();
	m_aDrawLines.Clear();
	m_idDrawStartVertex = CAD_INVALID;
	m_idDrawLastVertex = CAD_INVALID;
	m_idNewPolygon = CAD_INVALID;
	return hr;
}

VOID CCADCtrl::CancelDrawMode(VOID)
{
	if(!m_fDrawActive)
		return;

	for(sysint i = 0; i < m_aDrawLines.Length(); i++)
		RemoveLineById(m_aDrawLines[i]);

	if(m_idNewPolygon != CAD_INVALID)
	{
		CAD_POLYGON* pCadPolygon = NULL;
		if(SUCCEEDED(m_pPolygons->Remove(m_idNewPolygon, &pCadPolygon)))
			__delete pCadPolygon;
	}

	DeselectAll();
	m_pHost->OnEndDrawing();
	ResetDrawState();
	m_eMouse = CAD::None;
	m_pHost->InvalidateContainer(m_pGraph);
}

BOOL CCADCtrl::UndoLastDrawStep(VOID)
{
	if(!m_fDrawActive)
		return FALSE;

	if(m_aDrawLines.Length() == 0)
	{
		CancelDrawMode();
		return TRUE;
	}

	const DWORD idLastLine = m_aDrawLines[m_aDrawLines.Length() - 1];
	RemoveLineById(idLastLine);
	m_aDrawLines.RemoveChecked(m_aDrawLines.Length() - 1, NULL);

	if(m_aDrawVertices.Length() > 1)
	{
		m_aDrawVertices.RemoveChecked(m_aDrawVertices.Length() - 1, NULL);
		m_idDrawLastVertex = m_aDrawVertices[m_aDrawVertices.Length() - 1];
	}

	if(m_aNewPoints.Length() > 1)
		m_aNewPoints.RemoveChecked(m_aNewPoints.Length() - 1, NULL);

	m_pHost->InvalidateContainer(m_pGraph);
	return TRUE;
}

bool CCADCtrl::DeleteSelectedPolygon(VOID)
{
	sysint polygonIndex = 0;

	while(polygonIndex < m_pPolygons->Length())
	{
		DWORD selectedPolygonKey = CAD_INVALID;
		CAD_POLYGON* pCadPolygon = NULL;
		m_pPolygons->GetKeyAndValue(polygonIndex, &selectedPolygonKey, &pCadPolygon);

		if((pCadPolygon->nFlags & CAD_SELECTED) == 0)
		{
			++polygonIndex;
			continue;
		}

		m_pPolygons->Remove(selectedPolygonKey, &pCadPolygon);
		__delete pCadPolygon;

		TArray<DWORD> usedVertices;

		for(sysint k = 0; k < m_pLines->Length(); )
		{
			CAD_LINE* pCadLine = NULL;
			DWORD lineKey = CAD_INVALID;

			SideAssertHr(m_pLines->GetKeyAndValue(k, &lineKey, &pCadLine));

			if(pCadLine->idPolygonA != selectedPolygonKey && pCadLine->idPolygonB != selectedPolygonKey)
			{
				++k;
				continue;
			}
			bool doubleSided = false;
			if(pCadLine->idPolygonA != CAD_INVALID && pCadLine->idPolygonB != CAD_INVALID)
				doubleSided = true;

			if(pCadLine->idPolygonA == selectedPolygonKey)
			{
				pCadLine->idPolygonA = CAD_INVALID;
				pCadLine->pLineA = NULL;
			}

			if(pCadLine->idPolygonB == selectedPolygonKey)
			{
				pCadLine->idPolygonB = CAD_INVALID;
				pCadLine->pLineB = NULL;
			}

			const bool hasA = (pCadLine->idPolygonA != CAD_INVALID);
			const bool hasB = (pCadLine->idPolygonB != CAD_INVALID);

			if(!hasA && !hasB)
			{
				sysint pos = -1;
				if(!usedVertices.IndexOf(pCadLine->idVertexA, pos))
					usedVertices.Append(pCadLine->idVertexA);
				if(!usedVertices.IndexOf(pCadLine->idVertexB, pos))
					usedVertices.Append(pCadLine->idVertexB);

				m_pLines->Remove(lineKey, &pCadLine);
				__delete pCadLine;
				continue;
			}

			if(hasA && !hasB)
			{
				pCadLine->idPolygonB = pCadLine->idPolygonA;
				pCadLine->idPolygonA = CAD_INVALID;
			}

			if(doubleSided)
			{
				CAD_VERTEX *pVertexA, *pVertexB;
				m_pVertices->Find(pCadLine->idVertexA, &pVertexA);
				m_pVertices->Find(pCadLine->idVertexB, &pVertexB);

				const FPOINT a = pVertexA->vertex;
				const FPOINT b = pVertexB->vertex;

				FPOINT mid;
				mid.x = (a.x + b.x) / 2;
				mid.y = (a.y + b.y) / 2;

				FLOAT dx = b.x - a.x;
				FLOAT dy = b.y - a.y;

				FLOAT nx = -dy;
				FLOAT ny = dx;
				FLOAT length = sqrt(nx*nx + ny*ny);
				nx /= length;
				ny /= length;
				float d = 10.0f;

				FPOINT verticalPoint;
				verticalPoint.x = mid.x + nx * d;
				verticalPoint.y = mid.y + ny * d;
				if(IsPointInPolygonWithHoles(pCadLine->idPolygonB, verticalPoint))
				{
					SwapData(pCadLine->idVertexA, pCadLine->idVertexB);
				}
			}
			++k;
		}

		DeleteSoloVertexes(usedVertices);
	}

	return true;
}

bool CCADCtrl::DeleteSelectedLine (VOID)
{
	TArray<DWORD> linesToDelete;
	TArray<DWORD> usedVertices;

	for(sysint i = 0; i < m_pLines->Length(); i++)
	{
		DWORD lineKey = CAD_INVALID;
		CAD_LINE* pCadLine;

		SideAssertHr(m_pLines->GetKeyAndValue(i, &lineKey, &pCadLine));

		if(pCadLine->nFlags & CAD_SELECTED)
			linesToDelete.Append(lineKey);
		else
		{
			// track vertices that are still used
			sysint pos = -1;
			if(!usedVertices.IndexOf(pCadLine->idVertexA, pos))
				usedVertices.Append(pCadLine->idVertexA);
			if(!usedVertices.IndexOf(pCadLine->idVertexB, pos))
				usedVertices.Append(pCadLine->idVertexB);
		}
	}

	// delete selected lines
	for(sysint lineIndex = 0; lineIndex < linesToDelete.Length(); ++lineIndex)
	{
		CAD_LINE* pCadLine = NULL;
		const DWORD lineKey = linesToDelete[lineIndex];

		SideAssertHr(m_pLines->GetValueChecked(lineIndex, &pCadLine));

		// track vertices before deleting
		sysint pos = -1;
		if(!usedVertices.IndexOf(pCadLine->idVertexA, pos))
			usedVertices.Append(pCadLine->idVertexA);
		if(!usedVertices.IndexOf(pCadLine->idVertexB, pos))
			usedVertices.Append(pCadLine->idVertexB);

		m_pLines->Remove(lineKey, &pCadLine);
		__delete pCadLine;
	}

	DeleteSoloVertexes(usedVertices);

	return linesToDelete.Length() > 0;
}

bool CCADCtrl::DeleteSelectedVertex (VOID)
{
	TArray<DWORD> verticesToDelete;

	for(sysint vertexIndex = 0; vertexIndex < m_pVertices->Length();)
	{
		CAD_VERTEX* pCadVertex = NULL;
		DWORD vertexKey = CAD_INVALID;
		SideAssertHr(m_pVertices->GetKeyAndValue(vertexIndex, &vertexKey, &pCadVertex));

		if(pCadVertex->nFlags & CAD_SELECTED)
		{
			TArray<DWORD> connectedLines;
			TArray<DWORD> connectedVertices;
			for(sysint lineIndex = 0; lineIndex < m_pLines->Length(); ++lineIndex)
			{
				CAD_LINE* pCadLine;
				SideAssertHr(m_pLines->GetValueChecked(lineIndex, &pCadLine));

				if(pCadLine->idVertexA == vertexKey)
				{
					connectedLines.Append(m_pLines->GetKey(lineIndex));
					connectedVertices.Append(pCadLine->idVertexB);
				}
				else if(pCadLine->idVertexB == vertexKey)
				{
					connectedLines.Append(m_pLines->GetKey(lineIndex));
					connectedVertices.Append(pCadLine->idVertexA);
				}
			}

			if(connectedLines.Length() == 2)
			{
				MergeVertex(vertexKey, connectedVertices[0]);
			}
			else
			{
				for(sysint i = 0; i < connectedLines.Length(); ++i)
				{
					CAD_LINE* pLine;
					if(SUCCEEDED(m_pLines->Remove(connectedLines[i], &pLine)))
						__delete pLine;
				}

				CAD_VERTEX* pVertex;
				if(SUCCEEDED(m_pVertices->Remove(vertexKey, &pVertex)))
					__delete pVertex;
			}
			continue;
		}
		++vertexIndex;
	}
	
	return TRUE;
}

bool CCADCtrl::MergeVertex (DWORD vertexToDelete, DWORD vertexToStay)
{
	TArray<DWORD> linesToDelete;

	for(sysint lineIndex = 0; lineIndex < m_pLines->Length(); ++lineIndex)
	{
		DWORD lineKey = CAD_INVALID;
		CAD_LINE* pCadLine;
		SideAssertHr(m_pLines->GetKeyAndValue(lineIndex, &lineKey, &pCadLine));

		if((pCadLine->idVertexA == vertexToDelete && pCadLine->idVertexB == vertexToStay) ||
			(pCadLine->idVertexB == vertexToDelete && pCadLine->idVertexA == vertexToStay))
		{
			linesToDelete.Append(lineKey);
			continue;
		}

		if(pCadLine->idVertexA == vertexToDelete)
			pCadLine->idVertexA = vertexToStay;
		if(pCadLine->idVertexB == vertexToDelete)
			pCadLine->idVertexB = vertexToStay;
	}

	for(sysint lineIndex = 0; lineIndex < linesToDelete.Length(); ++lineIndex)
	{
		CAD_LINE* pCadLine;
		if(SUCCEEDED(m_pLines->Remove(linesToDelete[lineIndex], &pCadLine)))
			__delete pCadLine;
	}

	CAD_VERTEX* pCadVertex;
	if(SUCCEEDED(m_pVertices->Remove(vertexToDelete, &pCadVertex)))
		__delete pCadVertex;

	return TRUE;
}

VOID CCADCtrl::FlipSelectedLines (VOID)
{
	for(sysint i = 0; i < m_pLines->Length(); i++)
	{
		CAD_LINE* pCadLine;
		SideAssertHr(m_pLines->GetValueChecked(i, &pCadLine));

		if(pCadLine->nFlags & CAD_SELECTED)
		{
			SwapData(pCadLine->idPolygonA, pCadLine->idPolygonB);
			SwapData(pCadLine->idVertexA, pCadLine->idVertexB);
			SwapData(pCadLine->pLineA, pCadLine->pLineB);
		}
	}
}

VOID CCADCtrl::Paint (VOID)
{
	DrawLineMode();

	if(m_fDrawActive)
	{
		DrawVertexMode();
		DrawLineDrawing();
	}
	else if(CAD::Vertex == m_eMode || CAD::Drawing == m_eMode)
	{
		DrawVertexMode();
	}
}

VOID CCADCtrl::LBtnDown(KEYS dwKeys, FLOAT x, FLOAT y)
{
	if(m_fDrawActive && CAD::Drawing == m_eMode)
	{
		SnapCoordinate(x, y);
		m_xMouse = x;
		m_yMouse = y;

		CommitDrawClick(x, y);
		m_pHost->InvalidateContainer(m_pGraph);
		return;
	}

	m_xMouse = x;
	m_yMouse = y;
	m_eMouse = CAD::Down;
}

VOID CCADCtrl::LBtnUp (KEYS dwKeys, FLOAT x, FLOAT y)
{
	if(m_fDrawActive || CAD::Drawing == m_eMode)
		return;

	if(CAD::Down == m_eMouse)
	{
		bool fDeselectOthers = (dwKeys & MK_SHIFT) == 0;

		switch(m_eMode)
		{
		case CAD::Vertex:
			VertexSelect(x, y, fDeselectOthers, true);
			break;
		case CAD::Line:
			LineSelect(x, y, fDeselectOthers, true);
			break;
		case CAD::Polygon:
			PolygonSelect(x, y, fDeselectOthers, true);
			break;
		}
		m_pHost->InvalidateContainer(m_pGraph);
	}
	else if(CAD::Dragging == m_eMouse)
	{
		SnapCoordinate(x, y);

		CAD_VERTEX* pCadVertex = NULL;
		DWORD selectedVertexKey = CAD_INVALID;
		for(sysint i = 0; i < m_pVertices->Length(); i++)
		{
			SideAssertHr(m_pVertices->GetKeyAndValue(i, &selectedVertexKey, &pCadVertex));

			if(pCadVertex->nFlags & CAD_SELECTED)
				break;

			selectedVertexKey = CAD_INVALID;
		}

		if(selectedVertexKey != CAD_INVALID)
		{
			const DWORD overlapVertexKey = GetOtherVertexFromPosition(x, y, pCadVertex);
			if(overlapVertexKey != CAD_INVALID)
				MergeVertex(overlapVertexKey, selectedVertexKey);
		}
	}

	m_eMouse = CAD::None;
}

VOID CCADCtrl::LBtnDbl (KEYS dwKeys, FLOAT x, FLOAT y)
{
	switch(m_eMode)
	{
	case CAD::Vertex:
		SnapCoordinate(x, y);
		LineSelect(x, y, true, false);
		SplitLine(x, y, NULL);
		DeselectAll();
		m_pHost->InvalidateContainer(m_pGraph);
		break;

	case CAD::Drawing:
		if(!m_fDrawActive)
		{
			SnapCoordinate(x, y);
			StartDrawMode(x, y);
		}
		break;
	}
}

BOOL CCADCtrl::RBtnDown (KEYS dwKeys, FLOAT x, FLOAT y)
{
	return FALSE;
}

VOID CCADCtrl::RBtnUp (KEYS dwKeys, FLOAT x, FLOAT y)
{
	FPOINT pt;
	pt.x = x;
	pt.y = y;

	for(sysint i = 0; i < m_pPolygons->Length(); i++)
	{
		DWORD polygonKey;
		CAD_POLYGON* pCadPolygon;
		SideAssertHr(m_pPolygons->GetKeyAndValue(i, &polygonKey, &pCadPolygon));
		IsPointInPolygonWithHoles(polygonKey, pt);
	}
}

VOID CCADCtrl::MouseMove(KEYS dwKeys, FLOAT x, FLOAT y)
{
	if(m_fDrawActive)
	{
		m_xMouse = x;
		m_yMouse = y;
		SnapCoordinate(m_xMouse, m_yMouse);

		VertexHover(m_xMouse, m_yMouse);

		FPOINT ptPrev;
		if(SUCCEEDED(GetVertexPoint(m_idDrawLastVertex, ptPrev)))
		{
			FLOAT xDiff = m_xMouse - ptPrev.x;
			FLOAT yDiff = m_yMouse - ptPrev.y;
			m_pHost->OnDrawLineStatus(sqrtf(xDiff * xDiff + yDiff * yDiff));
		}

		m_pHost->InvalidateContainer(m_pGraph);
		return;
	}

	if(CAD::None == m_eMouse)
	{
		BOOL fRedraw = FALSE;
		switch(m_eMode)
		{
		case CAD::Vertex:
		case CAD::Drawing:
			{
				BOOL fVertexHover = VertexHover(x, y);
				BOOL fLineHover = FALSE;

				bool fAnyVertexHover = false;
				for(sysint i = 0; i < m_pVertices->Length(); i++)
				{
					CAD_VERTEX* pCadVertex = NULL;
					SideAssertHr(m_pVertices->GetValueChecked(i, &pCadVertex));
					if(pCadVertex->nFlags & CAD_HOVER)
					{
						fAnyVertexHover = true;
						break;
					}
				}

				if(!fAnyVertexHover)
					fLineHover = LineHover(x, y);
				else
				{
					for(sysint i = 0; i < m_pLines->Length(); i++)
					{
						CAD_LINE* pCadLine;
						SideAssertHr(m_pLines->GetValueChecked(i, &pCadLine));
						pCadLine->nFlags &= ~CAD_HOVER;
					}
				}

				fRedraw = (fVertexHover || fLineHover);
			}
			break;

		case CAD::Line:
			fRedraw = LineHover(x, y);
			break;

		case CAD::Polygon:
			fRedraw = PolygonHover(x, y);
			break;
		}

		if(fRedraw)
			m_pHost->InvalidateContainer(m_pGraph);
	}
	else
	{
		if(CAD::Down == m_eMouse)
		{
			if(fabsf(m_xMouse - x) > 2.5f || fabsf(m_yMouse - y) > 2.5f)
			{
				bool fDeselectOthers = (dwKeys & MK_SHIFT) == 0;

				switch(m_eMode)
				{
				case CAD::Vertex:
					VertexSelect(m_xMouse, m_yMouse, fDeselectOthers, false);
					break;
				case CAD::Line:
					LineSelect(m_xMouse, m_yMouse, fDeselectOthers, false);
					break;
				case CAD::Polygon:
					PolygonSelect(m_xMouse, m_yMouse, fDeselectOthers, false);
					break;
				}

				SnapCoordinate(m_xMouse, m_yMouse);
				m_eMouse = CAD::Dragging;
			}
		}

		if(CAD::Dragging == m_eMouse)
		{
			SnapCoordinate(x, y);

			FLOAT xDelta = x - m_xMouse;
			FLOAT yDelta = y - m_yMouse;

			m_xMouse = x;
			m_yMouse = y;

			switch(m_eMode)
			{
			case CAD::Vertex:
				{
					for(sysint i = 0; i < m_pVertices->Length(); i++)
					{
						CAD_VERTEX* pCadVertex;
						SideAssertHr(m_pVertices->GetValueChecked(i, &pCadVertex));

						if(pCadVertex->nFlags & CAD_SELECTED)
							AdjustVertex(pCadVertex, xDelta, yDelta);
					}
				}
				break;

			case CAD::Line:
			case CAD::Polygon:
				{
					TArray<DWORD> aVertices;

					for(sysint i = 0; i < m_pLines->Length(); i++)
					{
						CAD_LINE* pCadLine;
						SideAssertHr(m_pLines->GetValueChecked(i, &pCadLine));

						if(pCadLine->nFlags & CAD_SELECTED)
						{
							sysint nPos;

							if(!aVertices.IndexOf(pCadLine->idVertexA, nPos))
								aVertices.Append(pCadLine->idVertexA);
							if(!aVertices.IndexOf(pCadLine->idVertexB, nPos))
								aVertices.Append(pCadLine->idVertexB);
						}
					}

					for(sysint i = 0; i < aVertices.Length(); i++)
					{
						CAD_VERTEX* pVertex;
						SideAssertHr(m_pVertices->Find(aVertices[i], &pVertex));
						AdjustVertex(pVertex, xDelta, yDelta);
					}
				}
				break;
			}

			m_pHost->InvalidateContainer(m_pGraph);
		}
	}
}

BOOL CCADCtrl::KeyUp(WPARAM iKey)
{
	if(m_fDrawActive)
	{
		if(VK_ESCAPE == iKey)
		{
			CancelDrawMode();
			return TRUE;
		}
		else if(VK_BACK == iKey)
		{
			return UndoLastDrawStep();
		}
	}

	if(VK_DELETE == iKey)
	{
		BOOL deletingResult = FALSE;
		switch(m_eMode)
		{
		case CAD::Vertex:
			deletingResult = DeleteSelectedVertex();
			break;
		case CAD::Line:
			deletingResult = DeleteSelectedLine();
			break;
		case CAD::Polygon:
			deletingResult = DeleteSelectedPolygon();
			break;
		case CAD::Drawing:
			break;
		default:
			break;
		}
		m_pHost->InvalidateContainer(m_pGraph);
		return deletingResult;
	}
	else if('F' == iKey && CAD::Line == m_eMode)
	{
		FlipSelectedLines();
		m_pHost->InvalidateContainer(m_pGraph);
		return TRUE;
	}
	return FALSE;
}

DWORD CCADCtrl::GetPolygonFromLine(FPOINT a, FPOINT b, DWORD idIgnore)
{
	DWORD idBestPoly = CAD_INVALID;
	FLOAT minDistance = FLT_MAX;

	FPOINT mid;
	mid.x = (a.x + b.x) / 2.0f;
	mid.y = (a.y + b.y) / 2.0f;
	mid.z = 0.0f;

	for(sysint i = 0; i < m_pPolygons->Length(); i++)
	{
		DWORD polygonKey = CAD_INVALID;
		CAD_POLYGON* pCadPolygon = NULL;
		m_pPolygons->GetKeyAndValue(i, &polygonKey, &pCadPolygon);

		if(idIgnore == polygonKey)
			continue;

		TArray<FPOINT> polygon;
		GetPolygonPoints(polygonKey, polygon);

		if(IsPointInPolygon(polygon, mid))
		{
			FLOAT distanceToPolygon = MinDistanceToPolygon(polygon, mid);
			if(distanceToPolygon < minDistance)
			{
				idBestPoly = polygonKey;
				minDistance = distanceToPolygon;
			}
		}
	}
	return idBestPoly;
}

DWORD CCADCtrl::GetPolygonFromPoint(FLOAT x, FLOAT y, DWORD idIgnore)
{
	DWORD idBestPoly = CAD_INVALID;
	FLOAT minArea = FLT_MAX;
	FPOINT pt = { x, y, 0.0f };

	for(sysint i = 0; i < m_pPolygons->Length(); i++)
	{
		DWORD polygonKey;
		CAD_POLYGON* pCadPolygon = NULL;
		SideAssertHr(m_pPolygons->GetKeyAndValue(i, &polygonKey, &pCadPolygon));

		if(polygonKey == idIgnore)
			continue;
		TArray<FPOINT> polygon;
		GetPolygonPoints(polygonKey, polygon);
		if(IsPointInPolygonWithHoles(polygonKey, pt))
		{
			FLOAT area = PolygonArea(polygon);
			if(area < minArea)
			{
				idBestPoly = polygonKey;
				minArea = area;
			}
		}
	}

	return idBestPoly;
}
VOID CCADCtrl::DeselectAll (UINT nRemove)
{
	for(sysint i = 0; i < m_pVertices->Length(); i++)
	{
		CAD_VERTEX* pCadVertex;
		SideAssertHr(m_pVertices->GetValueChecked(i, &pCadVertex));
		pCadVertex->nFlags &= ~nRemove;
	}
	for(sysint i = 0; i < m_pLines->Length(); i++)
	{
		CAD_LINE* pCadLine;
		SideAssertHr(m_pLines->GetValueChecked(i, &pCadLine));
		pCadLine->nFlags &= ~nRemove;
	}
	for(sysint i = 0; i < m_pPolygons->Length(); i++)
	{
		CAD_POLYGON* pCadPolygon;
		SideAssertHr(m_pPolygons->GetValueChecked(i, &pCadPolygon));
		pCadPolygon->nFlags &= ~nRemove;
	}
}

HRESULT CCADCtrl::SplitLine(FLOAT x, FLOAT y, __out_opt DWORD* pidVertex)
{
	HRESULT hr = S_FALSE;

	FPOINT vertex;
	vertex.x = x;
	vertex.y = y;
	vertex.z = 0.f;

	sysint idxVertex = -1;
	DWORD idNewVertex = CAD_INVALID;
	if(FindVertex(vertex, &idxVertex))
	{
		idNewVertex = m_pVertices->GetKey(idxVertex);
	}
	else
	{
		idNewVertex = GetFreeVertex();
		Check(AddVertex(idNewVertex, vertex));
	}

	for(sysint i = 0; i < m_pLines->Length(); ++i)
	{
		CAD_LINE* pCadLine;
		DWORD idLine = CAD_INVALID;

		SideAssertHr(m_pLines->GetKeyAndValue(i, &idLine, &pCadLine));

		if((pCadLine->nFlags & CAD_SELECTED) == 0)
			continue;

		if(pCadLine->idVertexA == idNewVertex || pCadLine->idVertexB == idNewVertex)
			continue;

		TStackRef<ICADLine> srNewSplitA, srNewSplitB;

		if(pCadLine->idPolygonA != CAD_INVALID)
			CheckIf(!m_pHost->OnSplitLine(pCadLine, vertex, pCadLine->pLineA, &srNewSplitA), E_FAIL);
		if(pCadLine->idPolygonB != CAD_INVALID)
			CheckIf(!m_pHost->OnSplitLine(pCadLine, vertex, pCadLine->pLineB, &srNewSplitB), E_FAIL);

		const DWORD oldVertexB = pCadLine->idVertexB;
		const DWORD oldPolygonA = pCadLine->idPolygonA;
		const DWORD oldPolygonB = pCadLine->idPolygonB;
		ICADLine* pOldLineA = pCadLine->pLineA;
		ICADLine* pOldLineB = pCadLine->pLineB;

		DWORD idNewLine = GetFreeLine();
		Check(AddLine(idNewLine, idNewVertex, oldVertexB, oldPolygonA, oldPolygonB, srNewSplitA, srNewSplitB));

		pCadLine->idVertexB = idNewVertex;
		pCadLine->idPolygonA = oldPolygonA;
		pCadLine->idPolygonB = oldPolygonB;
		pCadLine->pLineA = pOldLineA;
		pCadLine->pLineB = pOldLineB;

		hr = S_OK;
		break;
	}

	if(pidVertex)
		*pidVertex = idNewVertex;

Cleanup:
	return hr;
}

HRESULT CCADCtrl::FindOrCreateVertex(FLOAT x, FLOAT y, __out DWORD* pidVertex)
{
	HRESULT hr = S_OK;
	FPOINT vertex;

	SnapCoordinate(x, y);

	LineSelect(x, y, true, false);
	vertex.x = x;
	vertex.y = y;
	vertex.z = 0.0f;
	sysint idxVertex;
	if(FindVertex(vertex, &idxVertex))
	{
		*pidVertex = m_pVertices->GetKey(idxVertex);
	}
	else
	{
		*pidVertex = GetFreeVertex();
		Check(AddVertex(*pidVertex, vertex));
	}
	
Cleanup:
	return hr;
}


BOOL CCADCtrl::VertexHover(FLOAT x, FLOAT y)
{
	BOOL fChanged = FALSE;

	for(sysint i = 0; i < m_pVertices->Length(); i++)
	{
		CAD_VERTEX* pCadVertex;
		SideAssertHr(m_pVertices->GetValueChecked(i, &pCadVertex));

		FPOINT* pVertex = &pCadVertex->vertex;
		ToggleSelection(pCadVertex->nFlags, IsPointOver(x, y, pVertex), fChanged);
	}

	return fChanged;
}

BOOL CCADCtrl::LineHover(FLOAT x, FLOAT y)
{
	BOOL fChanged = FALSE;

	for(sysint i = 0; i < m_pLines->Length(); i++)
	{
		CAD_LINE* pCadLine;
		SideAssertHr(m_pLines->GetValueChecked(i, &pCadLine));

		CAD_VERTEX* pVertexA, * pVertexB;
		SideAssertHr(m_pVertices->Find(pCadLine->idVertexA, &pVertexA));
		SideAssertHr(m_pVertices->Find(pCadLine->idVertexB, &pVertexB));

		const BOOL selected = PointDistanceFromLine(
			pVertexA->vertex.x, pVertexA->vertex.y,
			pVertexB->vertex.x, pVertexB->vertex.y,
			x, y) < MIN_DISTANCE_FROM_LINE;

		ToggleSelection(pCadLine->nFlags, selected, fChanged);
	}

	return fChanged;
}

BOOL CCADCtrl::PolygonHover(FLOAT x, FLOAT y)
{
	BOOL fChanged = FALSE;

	DWORD idPolygon = GetPolygonFromPoint(x, y, CAD_INVALID);

	for(sysint i = 0; i < m_pPolygons->Length(); i++)
	{
		CAD_POLYGON* pCadPolygon;
		SideAssertHr(m_pPolygons->GetValueChecked(i, &pCadPolygon));
		ToggleSelection(pCadPolygon->nFlags, idPolygon == m_pPolygons->GetKey(i), fChanged);
	}

	// toggle line selection
	for(sysint i = 0; i < m_pLines->Length(); i++)
	{
		CAD_LINE* pCadLine;
		SideAssertHr(m_pLines->GetValueChecked(i, &pCadLine));
		const BOOL bSelected = (idPolygon != CAD_INVALID &&
			(pCadLine->idPolygonA == idPolygon || pCadLine->idPolygonB == idPolygon));
		ToggleSelection(pCadLine->nFlags, bSelected, fChanged);
	}

	return fChanged;
}

VOID CCADCtrl::VertexSelect(FLOAT x, FLOAT y, bool fDeselectOthers, bool fAllowToggle)
{
	for(sysint i = 0; i < m_pVertices->Length(); i++)
	{
		CAD_VERTEX* pCadVertex;
		SideAssertHr(m_pVertices->GetValueChecked(i, &pCadVertex));

		FPOINT* pVertex = &pCadVertex->vertex;
		if(IsPointOver(x, y, pVertex))
		{
			if(pCadVertex->nFlags & CAD_SELECTED)
			{
				if(fAllowToggle)
					pCadVertex->nFlags &= ~CAD_SELECTED;
			}
			else
			{
				pCadVertex->nFlags |= CAD_SELECTED;
			}
		}
		else if(pCadVertex->nFlags & CAD_SELECTED)
		{
			if(fDeselectOthers)
				pCadVertex->nFlags &= ~CAD_SELECTED;
		}
	}
}

VOID CCADCtrl::LineSelect(FLOAT x, FLOAT y, bool fDeselectOthers, bool fAllowToggle)
{
	for(sysint i = 0; i < m_pLines->Length(); ++i)
	{
		CAD_LINE* pCadLine;
		SideAssertHr(m_pLines->GetValueChecked(i, &pCadLine));

		CAD_VERTEX* pVertexA, *pVertexB;
		SideAssertHr(m_pVertices->Find(pCadLine->idVertexA, &pVertexA));
		SideAssertHr(m_pVertices->Find(pCadLine->idVertexB, &pVertexB));

		const float xFromVertexA = pVertexA->vertex.x;
		const float yFromVertexA = pVertexA->vertex.y;
		const float xFromVertexB = pVertexB->vertex.x;
		const float yFromVertexB = pVertexB->vertex.y;

		if(PointDistanceFromLine(xFromVertexA, yFromVertexA, xFromVertexB, yFromVertexB, x, y) < MIN_DISTANCE_FROM_LINE)
		{
			if(pCadLine->nFlags & CAD_SELECTED)
			{
				if(fAllowToggle)
					pCadLine->nFlags &= ~CAD_SELECTED;
			}
			else
			{
				pCadLine->nFlags |= CAD_SELECTED;
			}
		}
		else if(pCadLine->nFlags & CAD_SELECTED)
		{
			if(fDeselectOthers)
				pCadLine->nFlags &= ~CAD_SELECTED;
		}
	}
}

VOID CCADCtrl::PolygonSelect(FLOAT x, FLOAT y, bool fDeselectOthers, bool fAllowToggle)
{
	DWORD idPolygon = GetPolygonFromPoint(x, y, CAD_INVALID);

	if(idPolygon != CAD_INVALID)
	{
		for(sysint i = 0; i < m_pPolygons->Length(); i++)
		{
			CAD_POLYGON* pCadPolygon;

			SideAssertHr(m_pPolygons->GetValueChecked(i, &pCadPolygon));
			if(idPolygon == m_pPolygons->GetKey(i))
			{
				if(pCadPolygon->nFlags & CAD_SELECTED)
				{
					if(fAllowToggle)
						pCadPolygon->nFlags &= ~CAD_SELECTED;
				}
				else
				{
					pCadPolygon->nFlags |= CAD_SELECTED;
				}
			}
			else if(pCadPolygon->nFlags & CAD_SELECTED)
			{
				if(fDeselectOthers)
					pCadPolygon->nFlags &= ~CAD_SELECTED;
			}
		}

		for(sysint i = 0; i < m_pLines->Length(); i++)
		{
			CAD_LINE* pCadLine;

			SideAssertHr(m_pLines->GetValueChecked(i, &pCadLine));

			if(idPolygon != CAD_INVALID && (pCadLine->idPolygonA == idPolygon || pCadLine->idPolygonB == idPolygon))
			{
				if(pCadLine->nFlags & CAD_SELECTED)
				{
					if(fAllowToggle)
						pCadLine->nFlags &= ~CAD_SELECTED;
				}
				else
				{
					pCadLine->nFlags |= CAD_SELECTED;
				}
			}
			else if(pCadLine->nFlags & CAD_SELECTED)
			{
				if(fDeselectOthers)
					pCadLine->nFlags &= ~CAD_SELECTED;
			}
		}
	}
	else if(fDeselectOthers)
	{
		for(sysint i = 0; i < m_pPolygons->Length(); i++)
		{
			CAD_POLYGON* pCadPolygon;
			SideAssertHr(m_pPolygons->GetValueChecked(i, &pCadPolygon));
			pCadPolygon->nFlags &= ~CAD_SELECTED;
		}

		for(sysint i = 0; i < m_pLines->Length(); i++)
		{
			CAD_LINE* pCadLine;
			SideAssertHr(m_pLines->GetValueChecked(i, &pCadLine));
			pCadLine->nFlags &= ~CAD_SELECTED;
		}
	}
}

VOID CCADCtrl::DrawVertexMode(VOID)
{
	HPEN hpnDefault = m_pGraph->SelectPen(m_pnNormal);

	for(sysint i = 0; i < m_pVertices->Length(); i++)
	{
		HPEN hpnPrev = NULL;
		CAD_VERTEX* pCadVertex;
		SideAssertHr(m_pVertices->GetValueChecked(i, &pCadVertex));

		if(pCadVertex->nFlags & CAD_HOVER)
			hpnPrev = m_pGraph->SelectPen(m_pnHover);
		else if(pCadVertex->nFlags & CAD_SELECTED)
			hpnPrev = m_pGraph->SelectPen(m_pnSelected);

		FPOINT* pVertex = &pCadVertex->vertex;
		m_pGraph->Rectangle(pVertex->x - 2.5f, pVertex->y - 2.5f, pVertex->z - 2.5f,
			pVertex->x + 2.5f, pVertex->y + 2.5f, pVertex->z + 2.5f);

		if(hpnPrev)
			m_pGraph->SelectPen(hpnPrev);
	}

	m_pGraph->SelectPen(hpnDefault);
}

VOID CCADCtrl::DrawLineMode (VOID)
{
	HPEN hpnDefault = m_pGraph->SelectPen(m_pnNormal);

	for(sysint i = 0; i < m_pLines->Length(); i++)
	{
		HPEN hpnPrev = NULL;
		CAD_LINE* pCadLine;

		SideAssertHr(m_pLines->GetValueChecked(i, &pCadLine));

		if(pCadLine->nFlags & CAD_DRAWING)
			hpnPrev = m_pGraph->SelectPen(m_pnDrawing);
		else if(pCadLine->nFlags & CAD_HOVER)
			hpnPrev = m_pGraph->SelectPen(m_pnHover);
		else if(pCadLine->nFlags & CAD_SELECTED)
			hpnPrev = m_pGraph->SelectPen(m_pnSelected);
		else if(CAD_INVALID != pCadLine->idPolygonB && pCadLine->idPolygonA != CAD_INVALID)
			hpnPrev = m_pGraph->SelectPen(m_pnTwoSided);

		CAD_VERTEX* pCadFrom;
		CAD_VERTEX* pCadTo;

		SideAssertHr(m_pVertices->Find(pCadLine->idVertexA, &pCadFrom));
		SideAssertHr(m_pVertices->Find(pCadLine->idVertexB, &pCadTo));

		DrawLineSegment(pCadFrom->vertex, pCadTo->vertex);

		if(hpnPrev)
			m_pGraph->SelectPen(hpnPrev);
	}

	m_pGraph->SelectPen(hpnDefault);
}

VOID CCADCtrl::DrawLineDrawing(VOID)
{
	if(!m_fDrawActive || m_aDrawVertices.Length() == 0)
		return;

	HPEN hpnDefault = m_pGraph->SelectPen(m_pnDrawing);

	for(sysint i = 0; i < m_aDrawVertices.Length(); i++)
	{
		FPOINT pt;
		if(SUCCEEDED(GetVertexPoint(m_aDrawVertices[i], pt)))
		{
			m_pGraph->Rectangle(pt.x - 2.5f, pt.y - 2.5f, pt.z - 2.5f,
				pt.x + 2.5f, pt.y + 2.5f, pt.z + 2.5f);
		}
	}

	FPOINT fromPt;
	if(SUCCEEDED(GetVertexPoint(m_idDrawLastVertex, fromPt)))
	{
		FPOINT toPt;
		toPt.x = m_xMouse;
		toPt.y = m_yMouse;
		toPt.z = 0.0f;

		DrawLineSegment(fromPt, toPt);
	}

	m_pGraph->SelectPen(hpnDefault);
}

VOID CCADCtrl::DrawLineSegment(const FPOINT& vFrom, const FPOINT& vTo)
{
	m_pGraph->MoveTo(vFrom.x, vFrom.y, vFrom.z);
	m_pGraph->LineTo(vTo.x, vTo.y, vTo.z);

	if(!IsPointEqual(vFrom, vTo))
	{
		FPOINT mid, product;
		mid.x = (vTo.x + vFrom.x) / 2.0f;
		mid.y = (vTo.y + vFrom.y) / 2.0f;
		mid.z = (vTo.z + vFrom.z) / 2.0f;

		product.x = vTo.y - vFrom.y;
		product.y = vFrom.x - vTo.x;
		product.z = 0.0f;

		Geometry::Normalize(&product);

		m_pGraph->MoveTo(mid.x, mid.y, mid.z);
		product.x *= 3.0f;
		product.y *= 3.0f;
		product.z *= 3.0f;
		m_pGraph->LineTo(mid.x + product.x, mid.y + product.y, mid.z + product.z);
	}
}

DWORD CCADCtrl::GetOtherVertexFromPosition(FLOAT x, FLOAT y, CAD_VERTEX* pCurrentVertex)
{
	FPOINT vertex;
	vertex.x = x;
	vertex.y = y;
	vertex.z = 0.0f;

	for(sysint i = 0; i < m_pVertices->Length(); ++i)
	{
		DWORD vertexKey;
		CAD_VERTEX* pVertex;
		m_pVertices->GetKeyAndValue(i, &vertexKey, &pVertex);
		if(pVertex != pCurrentVertex && IsPointEqual(pVertex->vertex, vertex))
			return vertexKey;
	}
	return CAD_INVALID;
}

bool CCADCtrl::FindVertex(const FPOINT& vertex, __out sysint* pidx)
{
	for(sysint i = 0; i < m_pVertices->Length(); i++)
	{
		CAD_VERTEX* pVertex;
		SideAssertHr(m_pVertices->GetValueChecked(i, &pVertex));
		if(IsPointEqual(pVertex->vertex, vertex))
		{
			*pidx = i;
			return true;
		}
	}
	return false;
}

bool CCADCtrl::IsPointOver(FLOAT x, FLOAT y, const FPOINT* pVertex)
{
	return fabsf(pVertex->x - x) < 2.5f && fabsf(pVertex->y - y) < 2.5f;
}

VOID CCADCtrl::AdjustVertex(CAD_VERTEX* pCadVertex, FLOAT xDelta, FLOAT yDelta)
{
	pCadVertex->vertex.x += xDelta;
	pCadVertex->vertex.y += yDelta;
}

bool CCADCtrl::IsPointEqual(const FPOINT& fpA, const FPOINT& fpB)
{
	return fabsf(fpA.x - fpB.x) < 0.01f &&
		fabsf(fpA.y - fpB.y) < 0.01f &&
		fabsf(fpA.z - fpB.z) < 0.01f;
}

VOID CCADCtrl::ToggleSelection(UINT& nFlags, BOOL fSelect, __inout BOOL& fChanged)
{
	if(fSelect)
	{
		if(0 == (nFlags & CAD_HOVER))
		{
			nFlags |= CAD_HOVER;
			fChanged = TRUE;
		}
	}
	else if(nFlags & CAD_HOVER)
	{
		nFlags &= ~CAD_HOVER;
		fChanged = TRUE;
	}
}

FLOAT CCADCtrl::CrossProduct(const FPOINT& a, const FPOINT& b, FLOAT x, FLOAT y)
{
	return (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
}

HRESULT CCADCtrl::DiffPolygon(DWORD subPolygonKey, DWORD clipPolygonKey)
{
	HRESULT hr;
	TArray<FPOINT> subPoints, clipPoints, intersectionPoints;

	Check(GetPolygonPoints(subPolygonKey, subPoints));
	Check(GetPolygonPoints(clipPolygonKey, clipPoints));

	Check(PolygonIntersection(subPoints, clipPoints, intersectionPoints));

	if(intersectionPoints.Length() == 0)
	{
		CheckIf(clipPoints.Length() == 0 || subPoints.Length() == 0, S_FALSE);

		if(IsPointInPolygon(subPoints, clipPoints[0]))
		{
			for(sysint i = 0; i < m_pLines->Length(); i++)
			{
				CAD_LINE* pCadLine;

				SideAssertHr(m_pLines->GetValueChecked(i, &pCadLine));

				bool usesClip =
					(pCadLine->idPolygonA == clipPolygonKey) ||
					(pCadLine->idPolygonB == clipPolygonKey);

				if(!usesClip)
					continue;

				if(pCadLine->idPolygonA == subPolygonKey || pCadLine->idPolygonB == subPolygonKey)
					continue;

				if(pCadLine->idPolygonA == CAD_INVALID)
					pCadLine->idPolygonA = subPolygonKey;
				else if(pCadLine->idPolygonB == CAD_INVALID)
					pCadLine->idPolygonB = subPolygonKey;
			}
		}
	}
	else
	{
		for(sysint i = 0; i < m_pLines->Length(); i++)
		{
			CAD_LINE* pCadLine;
			CAD_VERTEX* pV1 = NULL;
			CAD_VERTEX* pV2 = NULL;
			FPOINT mid;

			Check(m_pLines->GetValueChecked(i, &pCadLine));
			Check(m_pVertices->Find(pCadLine->idVertexA, &pV1));
			Check(m_pVertices->Find(pCadLine->idVertexB, &pV2));

			mid.x = (pV1->vertex.x + pV2->vertex.x) * 0.5f;
			mid.y = (pV1->vertex.y + pV2->vertex.y) * 0.5f;
			mid.z = 0.0f;

			const bool onSubBoundary =
				(pCadLine->idPolygonA == subPolygonKey || pCadLine->idPolygonB == subPolygonKey);

			const bool onClipBoundary =
				(pCadLine->idPolygonA == clipPolygonKey || pCadLine->idPolygonB == clipPolygonKey);

			// If a line belongs to clip and lies inside sub, keep sub on the other side too.
			if(onClipBoundary && IsPointInPolygon(subPoints, mid))
			{
				if(pCadLine->idPolygonA != subPolygonKey && pCadLine->idPolygonB != subPolygonKey)
				{
					if(pCadLine->idPolygonA == CAD_INVALID)
						pCadLine->idPolygonA = subPolygonKey;
					else if(pCadLine->idPolygonB == CAD_INVALID)
						pCadLine->idPolygonB = subPolygonKey;
				}
			}

			// If a sub line lies completely inside clip, remove sub ownership from it.
			if(onSubBoundary && IsPointInPolygon(clipPoints, mid))
			{
				if(pCadLine->idPolygonA == subPolygonKey)
					pCadLine->idPolygonA = CAD_INVALID;
				if(pCadLine->idPolygonB == subPolygonKey)
					pCadLine->idPolygonB = CAD_INVALID;
			}
		}
	}

Cleanup:
	return hr;
}

HRESULT CCADCtrl::GetPolygonPoints (DWORD polygonId, __out TArray<FPOINT>& polygonPoints)
{
	HRESULT hr;
	TArray<DWORD> polygonVertices;
	TArray<DWORD> orderedVertices;

	polygonPoints.Clear();

	for(sysint i = 0; i < m_pLines->Length(); i++)
	{
		CAD_LINE* pCadLine = NULL;
		Check(m_pLines->GetValueChecked(i, &pCadLine));

		if(!IsLineUsedByPolygon(pCadLine, polygonId))
			continue;

		sysint pos = -1;
		if(!polygonVertices.IndexOf(pCadLine->idVertexA, pos))
			Check(polygonVertices.Append(pCadLine->idVertexA));

		if(!polygonVertices.IndexOf(pCadLine->idVertexB, pos))
			Check(polygonVertices.Append(pCadLine->idVertexB));
	}

	CheckIf(polygonVertices.Length() < 3, E_FAIL);
	Check(OrderPolygonPoints(polygonId, polygonVertices, orderedVertices));
	CheckIf(orderedVertices.Length() < 3, E_FAIL);

	for(sysint i = 0; i < orderedVertices.Length(); i++)
	{
		CAD_VERTEX* pCadVertex = NULL;
		Check(m_pVertices->Find(orderedVertices[i], &pCadVertex));
		Check(polygonPoints.Append(pCadVertex->vertex));
	}

	if(polygonPoints.Length() >= 2 && IsPointEqual(polygonPoints[0], polygonPoints[polygonPoints.Length() - 1]))
	{
		Check(polygonPoints.RemoveChecked(polygonPoints.Length() - 1, NULL));
	}

	EnsureClockwise(polygonPoints);

Cleanup:
	return hr;
}

HRESULT CCADCtrl::OrderPolygonPoints (DWORD polygonId, TArray<DWORD>& polygon, __out TArray<DWORD>& orderedPolygon)
{
	HRESULT hr;

	orderedPolygon.Clear();
	CheckIf(polygon.Length() < 3, E_FAIL);

	DWORD start = polygon[0];
	DWORD prev = CAD_INVALID;
	DWORD current = start;

	Check(orderedPolygon.Append(start));

	for(;;)
	{
		DWORD next = CAD_INVALID;

		for(sysint i = 0; i < m_pLines->Length(); i++)
		{
			CAD_LINE* pCadLine;

			SideAssertHr(m_pLines->GetValueChecked(i, &pCadLine));

			if(!IsLineUsedByPolygon(pCadLine, polygonId))
				continue;

			DWORD candidate = CAD_INVALID;

			if(pCadLine->idVertexA == current)
				candidate = pCadLine->idVertexB;
			else if(pCadLine->idVertexB == current)
				candidate = pCadLine->idVertexA;
			else
				continue;

			sysint posPolygon = -1;
			if(!polygon.IndexOf(candidate, posPolygon))
				continue;

			if(candidate == prev)
				continue;

			next = candidate;
			break;
		}

		if(next == CAD_INVALID || next == start)
			break;

		sysint posOrdered = -1;
		if(orderedPolygon.IndexOf(next, posOrdered))
			break;

		Check(orderedPolygon.Append(next));
		prev = current;
		current = next;
	}

	CheckIf(orderedPolygon.Length() < 3, E_FAIL);

Cleanup:
	return hr;
}

VOID CCADCtrl::SetPolygonIDToLine(DWORD idVertexA, DWORD idVertexB, DWORD subPolygonKey, DWORD clipPoligonKey)
{
	CAD_LINE* pCadLine;

	for(sysint i = 0; i < m_pLines->Length(); i++)
	{
		DWORD lineKey;
		m_pLines->GetKeyAndValue(i, &lineKey, &pCadLine);

		if((pCadLine->idVertexA == idVertexA && pCadLine->idVertexB == idVertexB) ||
			(pCadLine->idVertexA == idVertexB && pCadLine->idVertexB == idVertexA))
		{
			if((pCadLine->idPolygonA == subPolygonKey && pCadLine->idPolygonB == CAD_INVALID) ||
				(pCadLine->idPolygonB == subPolygonKey && pCadLine->idPolygonA == CAD_INVALID))
			{
				m_pLines->Remove(lineKey, &pCadLine);
				__delete pCadLine;
				break;
			}

			if(pCadLine->idPolygonA == subPolygonKey && pCadLine->idPolygonB == clipPoligonKey)
			{
				pCadLine->idPolygonA = clipPoligonKey;
				pCadLine->idPolygonB = CAD_INVALID;
				break;
			}

			if(pCadLine->idPolygonB == subPolygonKey && pCadLine->idPolygonA == clipPoligonKey)
			{
				pCadLine->idPolygonB = CAD_INVALID;
				break;
			}

			if(pCadLine->idPolygonA == subPolygonKey && pCadLine->idPolygonB != CAD_INVALID)
				break;
			if(pCadLine->idPolygonB == subPolygonKey && pCadLine->idPolygonA != CAD_INVALID)
				break;

			if(pCadLine->idPolygonA == CAD_INVALID)
				pCadLine->idPolygonA = subPolygonKey;
			else if(pCadLine->idPolygonB == CAD_INVALID)
			{
				pCadLine->idPolygonB = pCadLine->idPolygonA;
				pCadLine->idPolygonA = subPolygonKey;
			}

			break;
		}
	}
}

bool CCADCtrl::IsLineUsedByPolygon (const CAD_LINE* pCadLine, DWORD polygonId) const
{
	return pCadLine->idPolygonA == polygonId || pCadLine->idPolygonB == polygonId;
}

FLOAT CCADCtrl::PolygonSignedArea (const TArray<FPOINT>& pts) const
{
	FLOAT area = 0.0f;

	if(pts.Length() < 3)
		return 0.0f;

	for(sysint i = 0; i < pts.Length(); i++)
	{
		const FPOINT& a = pts[i];
		const FPOINT& b = pts[(i + 1) % pts.Length()];
		area += (a.x * b.y) - (b.x * a.y);
	}

	return area * 0.5f;
}

VOID CCADCtrl::EnsureClockwise (__inout TArray<FPOINT>& pts)
{
	if(pts.Length() < 3)
		return;

	// screen-space style: enforce one consistent direction
	if(PolygonSignedArea(pts) > 0.0f)
	{
		for(sysint i = 0, j = pts.Length() - 1; i < j; i++, j--)
			SwapData(pts[i], pts[j]);
	}
}

bool CCADCtrl::PolygonExists (DWORD polygonId) const
{
	if(polygonId == CAD_INVALID)
		return false;

	CAD_POLYGON* pPolygon = NULL;
	return SUCCEEDED(m_pPolygons->Find(polygonId, &pPolygon)) && pPolygon != NULL;
}

FLOAT CCADCtrl::PolygonArea (const TArray<FPOINT>& polygon)
{
	if(polygon.Length() < 3)
		return FLT_MAX;

	FLOAT area = 0.0f;
	const sysint n = polygon.Length();

	for(sysint i = 0; i < n; i++)
	{
		const FPOINT& p0 = polygon[i];
		const FPOINT& p1 = polygon[(i + 1) % n];
		area += (p0.x * p1.y) - (p1.x * p0.y);
	}

	area = fabs(area) * 0.5f;
	return area;
}

void CCADCtrl::DeleteSoloVertexes (TArray<DWORD>& usedVertices)
{
	// iterate backward to safely remove entries while looping
	for(sysint v = usedVertices.Length() - 1; v >= 0; --v)
	{
		DWORD vertexKey = usedVertices[v];
		CAD_VERTEX* pVertex = NULL;
		SideAssertHr(m_pVertices->GetValueChecked(v, &pVertex));

		// check if this vertex is used by any line
		BOOL bUsed = FALSE;
		for(sysint l = 0; l < m_pLines->Length(); ++l)
		{
			CAD_LINE* pLine = NULL;
			SideAssertHr(m_pLines->GetValueChecked(l, &pLine));

			if(pLine->idVertexA == vertexKey || pLine->idVertexB == vertexKey)
			{
				bUsed = TRUE;
				break;
			}
		}

		// if vertex is not used in any line -> delete it
		if(!bUsed)
		{
			m_pVertices->Remove(vertexKey, &pVertex);
			__delete pVertex;
		}
	}
}

HRESULT CCADCtrl::IntegrateLine (DWORD idVertexA, DWORD idVertexB, __out_opt DWORD* pidLastLine)
{
	HRESULT hr;
	CAD_VERTEX* pA = NULL;
	CAD_VERTEX* pB = NULL;
	TArray<DWORD> orderedVertices;
	TArray<SEGMENT_INTERSECTION> intersections;
	DWORD vA, vB;
	CAD_LINE* pLine = NULL;
	DWORD existingLine = CAD_INVALID;
	DWORD idLastCreatedLine = CAD_INVALID;

	if(pidLastLine)
		*pidLastLine = CAD_INVALID;

	Check(m_pVertices->Find(idVertexA, &pA));
	Check(m_pVertices->Find(idVertexB, &pB));

	Check(FindSegmentIntersections(pA->vertex, pB->vertex, idVertexA, idVertexB, intersections));
	Check(SortAndUniqueIntersections(intersections));

	Check(orderedVertices.Append(idVertexA));

	for(sysint i = 0; i < intersections.Length(); i++)
	{
		DWORD idSplitVertex = CAD_INVALID;

		DeselectAll(CAD_SELECTED);
		LineSelect(intersections[i].pt.x, intersections[i].pt.y, true, false);
		Check(SplitLine(intersections[i].pt.x, intersections[i].pt.y, &idSplitVertex));
		DeselectAll(CAD_SELECTED);

		if(idSplitVertex != CAD_INVALID)
		{
			sysint pos = -1;
			if(!orderedVertices.IndexOf(idSplitVertex, pos))
				Check(orderedVertices.Append(idSplitVertex));
		}
	}

	Check(orderedVertices.Append(idVertexB));

	for(sysint i = 1; i + 1 < orderedVertices.Length(); i++)
	{
		DWORD idMid = orderedVertices[i];

		if(m_aDrawVertices.Length() == 0 || m_aDrawVertices[m_aDrawVertices.Length() - 1] != idMid)
		{
			FPOINT ptMid;
			XYPOINT xy;

			Check(m_aDrawVertices.Append(idMid));
			Check(GetVertexPoint(idMid, ptMid));

			xy.x = ptMid.x;
			xy.y = ptMid.y;
			Check(m_aNewPoints.Append(xy));
		}
	}

	for(sysint i = 0; i + 1 < orderedVertices.Length(); i++)
	{
		vA = orderedVertices[i];
		vB = orderedVertices[i + 1];

		if(vA == vB)
			continue;

		existingLine = FindExistingLine(vA, vB);

		if(existingLine != CAD_INVALID)
		{
			Check(m_pLines->Find(existingLine, &pLine));
			idLastCreatedLine = existingLine;
		}
		else
		{
			DWORD idLine = GetFreeLine();
			TStackRef<ICADLine> srLine;
			Check(m_pHost->OnCreateLine(m_idNewPolygon, idLine, &srLine));
			Check(AddLine(idLine, vA, vB, CAD_INVALID, CAD_INVALID, srLine, NULL));
			Check(m_aDrawLines.Append(idLine));
			idLastCreatedLine = idLine;
		}
	}

	if(pidLastLine)
		*pidLastLine = idLastCreatedLine;

Cleanup:
	return hr;
}

bool CCADCtrl::IsSegmentOnPolygonBoundary (DWORD polygonId, TArray<FPOINT> &newPolyPts, const FPOINT& a, const FPOINT& b, __out_opt int* pId)
{
	TArray<FPOINT> polyPts;
	if(FAILED(GetPolygonPoints(polygonId, polyPts)))
		return false;

	if(polyPts.Length() < 2)
		return false;

	*pId = 0;
	bool isBoundary = false;

	for(sysint i = 0; i < polyPts.Length(); i++)
	{
		const FPOINT& p1 = polyPts[i];
		const FPOINT& p2 = polyPts[(i + 1) % polyPts.Length()];

		if(!IsPointInPolygon(newPolyPts, p1) || !IsPointInPolygon(newPolyPts, p2))
			*pId = -1;

		if(IsPointOnSegment(a, p1, p2) && IsPointOnSegment(b, p1, p2))
			isBoundary = true;
	}
	if(isBoundary == false)
		return isBoundary;

	int tempPId = *pId;
	*pId = 1;
	isBoundary = false;
	for(sysint i = 0; i < newPolyPts.Length(); i++)
	{
		const FPOINT& p1 = newPolyPts[i];
		const FPOINT& p2 = newPolyPts[(i + 1) % newPolyPts.Length()];
		
		if(!IsPointInPolygon(polyPts, p1) || !IsPointInPolygon(polyPts, p2))
			*pId = -1;
		if(IsPointOnSegment(a, p1, p2) && IsPointOnSegment(b, p1, p2))
			isBoundary = true;
	}
	*pId = *pId > -1 ? *pId : tempPId;
	return isBoundary && *pId > -1;
}

HRESULT CCADCtrl::GetPolygonLines (DWORD polygonId, __out TArray<DWORD>& polygonLines)
{
	HRESULT hr = S_FALSE;

	polygonLines.Clear();

	for(sysint i = 0; i < m_pLines->Length(); i++)
	{
		CAD_LINE* pCadLine;
		DWORD idLine = CAD_INVALID;

		SideAssertHr(m_pLines->GetKeyAndValue(i, &idLine, &pCadLine));

		if(!IsLineUsedByPolygon(pCadLine, polygonId))
			continue;

		sysint pos = -1;
		if(!polygonLines.IndexOf(idLine, pos))
			Check(polygonLines.Append(idLine));
	}

Cleanup:
	return hr;
}

bool CCADCtrl::IsPointInPolygonLines (const TArray<DWORD>& polygonLines, const FPOINT& pt)
{
	TArray<FPOINT> polygonPts;

    // Reconstruct polygon vertices from the ordered lines
    for(sysint i = 0; i < polygonLines.Length(); i++)
    {
        DWORD idLine = polygonLines[i];
        CAD_LINE* pLine = NULL;
        if(FAILED(m_pLines->Find(idLine, &pLine)))
            continue;

        CAD_VERTEX* pVA = NULL;
        CAD_VERTEX* pVB = NULL;
        if(FAILED(m_pVertices->Find(pLine->idVertexA, &pVA)))
            continue;
        if(FAILED(m_pVertices->Find(pLine->idVertexB, &pVB)))
            continue;

        // Append vertex A if first line or not duplicate
        if(polygonPts.Length() == 0 || !IsPointEqual(polygonPts[polygonPts.Length() - 1], pVA->vertex))
            polygonPts.Append(pVA->vertex);

        // Append vertex B
        polygonPts.Append(pVB->vertex);
    }

    // Remove duplicate closing point if exists
    if(polygonPts.Length() >= 2 &&
        IsPointEqual(polygonPts[0], polygonPts[polygonPts.Length() - 1]))
    {
        polygonPts.RemoveChecked(polygonPts.Length() - 1, NULL);
    }

    // Ray-casting algorithm (same as IsPointInPolygon)
    int count = 0;
    sysint n = polygonPts.Length();
    for(sysint i = 0; i < n; ++i)
    {
        const FPOINT& a = polygonPts[i];
        const FPOINT& b = polygonPts[(i + 1) % n];

        if(a.x == b.x && a.y == b.y)
            continue;

        if(IsPointOnSegment(pt, a, b))
            return true; // on boundary is inside

        if(((a.y > pt.y) != (b.y > pt.y)) &&
            (pt.x < (b.x - a.x) * (pt.y - a.y) / (b.y - a.y + 1e-20f) + a.x))
        {
            count++;
        }
    }

    return (count % 2) == 1;
}

HRESULT CCADCtrl::BuildPolygonLoops (DWORD polygonId, TArray<DWORD>& allLines, __out_opt TArray<FPOINT>& outer, __out_opt TArray<TArray<FPOINT>*>& holes)
{
	HRESULT hr = S_FALSE;

	allLines.Clear();
	outer.Clear();

	for(sysint i = 0; i < m_pLines->Length(); i++)
	{
		DWORD idLine = CAD_INVALID;
		CAD_LINE* pCadLine;

		SideAssertHr(m_pLines->GetKeyAndValue(i, &idLine, &pCadLine));
		if(pCadLine->idPolygonA == polygonId || pCadLine->idPolygonB == polygonId)
			Check(allLines.Append(idLine));
	}

	if(allLines.Length() > 0)
	{
		TArray<DWORD> usedLines;
		FLOAT maxArea = 0.f;
		while(usedLines.Length() < allLines.Length())
		{
			TArray<FPOINT> loopPoints, *paTempPoints;
			TraceLoop(allLines, usedLines, loopPoints);

			if(loopPoints.Length() < 3)
				continue;
			FLOAT area = PolygonArea(loopPoints);

			if(area > maxArea)
			{
				if(outer.Length() > 0 && maxArea != 0.f)
				{
					Check(holes.AppendNew(&paTempPoints));

					for(sysint i = 0; i < outer.Length(); i++)
						Check(paTempPoints->Append(outer[i]));
				}

				outer.Clear();
				for(sysint i = 0; i < loopPoints.Length(); i++)
					Check(outer.Append(loopPoints[i]));
				maxArea = area;
			}
			else
			{
				Check(holes.AppendNew(&paTempPoints));

				for(sysint i = 0; i < loopPoints.Length(); i++)
					Check(paTempPoints->Append(loopPoints[i]));
			}
		}
	}

Cleanup:
	return hr;
}

void CCADCtrl::TraceLoop (TArray<DWORD>& lines, TArray<DWORD>& usedLines, __out_opt TArray<FPOINT>& points)
{
	if(lines.Length() == 0)
		return;

	DWORD idLine = CAD_INVALID;
	for(sysint i = 0; i < lines.Length(); i++)
	{
		sysint pos = -1;
		if(!usedLines.IndexOf(lines[i], pos))
		{
			idLine = lines[i];
			break;
		}
	}

	if(idLine == CAD_INVALID)
		return;

	points.Clear();

	CAD_LINE* pLine;
	if(FAILED(m_pLines->Find(idLine, &pLine)))
		return;

	FPOINT pt;
	CAD_VERTEX* pVertex;
	DWORD startV = pLine->idVertexA;
	DWORD currentV = pLine->idVertexB;

	SideAssertHr(m_pVertices->Find(startV, &pVertex));
	pt = pVertex->vertex;
	points.Append(pt);

	for(;;)
	{
		m_pVertices->Find(currentV, &pVertex);
		pt = pVertex->vertex;
		points.Append(pt);

		usedLines.Append(idLine);

		DWORD nextV = CAD_INVALID;
		DWORD idNextLine = CAD_INVALID;
		for(sysint i = 0; i < lines.Length(); i++)
		{
			DWORD candidateLineID = lines[i];
			sysint pos = -1;
			if(!usedLines.IndexOf(candidateLineID, pos))
			{
				CAD_LINE* pCandidateLine;
				if(FAILED(m_pLines->Find(candidateLineID, &pCandidateLine)))
					continue;

				if(pCandidateLine->idVertexA == currentV)
				{
					nextV = pCandidateLine->idVertexB;
					idNextLine = candidateLineID;
					break;
				}
				else if(pCandidateLine->idVertexB == currentV)
				{
					nextV = pCandidateLine->idVertexA;
					idNextLine = candidateLineID;
					break;
				}
			}
		}
		if(idNextLine == CAD_INVALID || nextV == startV)
		{
			if(nextV != CAD_INVALID && nextV != startV)
			{
				m_pVertices->Find(nextV, &pVertex);
				pt = pVertex->vertex;
				points.Append(pt);
			}
			usedLines.Append(idNextLine);
			break;
		}

		idLine = idNextLine;
		currentV = nextV;
	}
}

bool CCADCtrl::IsPointInPolygonWithHoles (DWORD polygonId, const FPOINT& pt)
{
	bool fInPolygonWithHoles;
	TArray<FPOINT> outer;
	TArray<TArray<FPOINT>*> holes;
	TArray<DWORD> allLines;

	BuildPolygonLoops(polygonId, allLines, outer, holes);

	fInPolygonWithHoles = IsPointInPolygon(outer, pt);
	
	int iHolesCount = 0;
	for(sysint i = 0; i < holes.Length(); i++)
	{
		TArray<FPOINT>* paTempHoles = holes[i], polygonPoints;
		for(sysint j = 0; j < paTempHoles->Length(); j++)
			polygonPoints.Append((*paTempHoles)[j]);
		if(IsPointInPolygon(polygonPoints, pt))
		{
			iHolesCount++;
		}
	}
	

	holes.DeleteAll();

	return (!fInPolygonWithHoles && iHolesCount > 0) || (fInPolygonWithHoles && iHolesCount == 0);
}
