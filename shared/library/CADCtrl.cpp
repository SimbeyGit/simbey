#include <math.h>
#include <float.h>
#include <windows.h>
#include "Core\CoreDefs.h"
#include "Spatial\Geometry.h"
#include "CADCtrl.h"

#define MIN_DISTANCE_FROM_LINE 2.5F

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
	m_eMouse(CAD::None)
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
	pCadLine->idVertexA  = idVertexA;
	pCadLine->idVertexB  = idVertexB;
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

bool CCADCtrl::DeleteSelectedPolygon (VOID)
{
	sysint polygonIndex = 0;
	while(polygonIndex < m_pPolygons->Length())
	{
		DWORD selectedPolygonKey = CAD_INVALID;
		CAD_POLYGON* pCadPolygon = NULL;
		m_pPolygons->GetKeyAndValue(polygonIndex, &selectedPolygonKey, &pCadPolygon);
		if(pCadPolygon->nFlags & CAD_SELECTED)
		{
			for(sysint i = 0; i < m_pPolygons->Length(); i++)
			{
				const DWORD polygonKey = m_pPolygons->GetKey(i);
				if(selectedPolygonKey == polygonKey)
					continue;
				DiffPolygon(polygonKey, selectedPolygonKey);
			}
			m_pPolygons->Remove(selectedPolygonKey, &pCadPolygon);
			__delete pCadPolygon;

			for(sysint k = 0; k < m_pLines->Length(); k++)
			{
				CAD_LINE *pCadLine = NULL;
				DWORD lineKey;

				SideAssertHr(m_pLines->GetKeyAndValue(k, &lineKey, &pCadLine));
				if((pCadLine->idPolygonA == selectedPolygonKey && pCadLine->idPolygonB == CAD_INVALID)
					|| (pCadLine->idPolygonB == selectedPolygonKey && pCadLine->idPolygonA == CAD_INVALID))
				{
					m_pLines->Remove(lineKey, &pCadLine);
					__delete pCadLine;
					continue;
				}
				if(pCadLine->idPolygonA == selectedPolygonKey)
				{
					pCadLine->idPolygonA = pCadLine->idPolygonB;
					pCadLine->idPolygonB = CAD_INVALID;
				}
				if(pCadLine->idPolygonB == selectedPolygonKey)
					pCadLine->idPolygonB = CAD_INVALID;
				k++;
			}
			continue;
		}
		polygonIndex++;
	}

	return true;
}

bool CCADCtrl::DeleteSelectedLine (VOID)
{
	TArray<DWORD> linesToDelete;
	TMap<DWORD, INT> verticesToDelete;

	for(sysint i = 0; i < m_pLines->Length(); i++)
	{
		DWORD lineKey = CAD_INVALID;
		CAD_LINE* pCadLine = NULL;
		SideAssertHr(m_pLines->GetKeyAndValue(i, &lineKey, &pCadLine));

		if(pCadLine->nFlags & CAD_SELECTED)
		{
			linesToDelete.Append(lineKey);
		}
	}

	for(sysint lineIndex = 0; lineIndex < linesToDelete.Length(); ++lineIndex)
	{
		CAD_LINE* pCadLine = NULL;
		const DWORD lineKey = linesToDelete[lineIndex];
		m_pLines->Remove(lineKey, &pCadLine);
		__delete pCadLine;
	}

	return linesToDelete.Length() > 0;
}

bool CCADCtrl::DeleteSelectedVertex (VOID)
{
	TArray<DWORD> linesToDelete;
	TArray<DWORD> vertexToDelete;

	for(sysint vertexIndex = 0; vertexIndex < m_pVertices->Length(); ++vertexIndex)
	{
		CAD_VERTEX* pCadVertex = NULL;
		SideAssertHr(m_pVertices->GetValueChecked(vertexIndex, &pCadVertex));

		if(pCadVertex->nFlags & CAD_SELECTED)
		{
			for(sysint lineIndex = 0; lineIndex < m_pLines->Length(); ++lineIndex)
			{
				CAD_LINE* pCadLine = NULL;
				SideAssertHr(m_pLines->GetValueChecked(lineIndex, &pCadLine));
				if(pCadLine->idVertexA == vertexIndex ||
					pCadLine->idVertexB == vertexIndex)
				{
					const DWORD lineKey = m_pLines->GetKey(lineIndex);
					linesToDelete.Append(lineKey);
				}
			}

			const DWORD vertexKey = m_pVertices->GetKey(vertexIndex);
			vertexToDelete.Append(vertexKey);
		}
	}

	for(sysint lineIndex = 0; lineIndex < linesToDelete.Length(); ++lineIndex)
	{
		CAD_LINE* pCadLine = NULL;
		const DWORD lineKey = linesToDelete[lineIndex];
		m_pLines->Remove(lineKey, &pCadLine);
		__delete pCadLine;
	}
	for(sysint vertexIndex = 0; vertexIndex < vertexToDelete.Length(); ++vertexIndex)
	{
		CAD_VERTEX* pCadVertex = NULL;
		const DWORD lineKey = vertexToDelete[vertexIndex];
		m_pVertices->Remove(lineKey, &pCadVertex);
		__delete pCadVertex;
	}

	// Redraw the canvas
	return vertexToDelete.Length() > 0;
}

bool CCADCtrl::MergeVertex (DWORD vertexToDelete, DWORD vertexToStay)
{
	TArray<DWORD> linesToDelete;

	for(sysint lineIndex = 0; lineIndex < m_pLines->Length(); ++lineIndex)
	{
		DWORD lineKey = CAD_INVALID;
		CAD_LINE* pCadLine = NULL;
		SideAssertHr(m_pLines->GetKeyAndValue(lineIndex, &lineKey, &pCadLine));
		// Remove the common line between two vertices
		if((pCadLine->idVertexA == vertexToDelete &&
			 pCadLine->idVertexB == vertexToStay) ||
			(pCadLine->idVertexB == vertexToDelete &&
			 pCadLine->idVertexA == vertexToStay))
		{
			linesToDelete.Append(lineKey);
			continue;
		}

		if(pCadLine->idVertexA == vertexToDelete)
		{
			pCadLine->idVertexA = vertexToStay;
		}
		if(pCadLine->idVertexB == vertexToDelete)
		{
			pCadLine->idVertexB = vertexToStay;
		}
	}

	for(sysint lineIndex = 0; lineIndex < linesToDelete.Length(); ++lineIndex)
	{
		CAD_LINE* pCadLine = NULL;
		const DWORD lineKey = linesToDelete[lineIndex];
		m_pLines->Remove(lineKey, &pCadLine);
		__delete pCadLine;
	}

	CAD_VERTEX* pCadVertex = NULL;
	m_pVertices->Remove(vertexToDelete, &pCadVertex);
	__delete pCadVertex;
	// Redraw the canvas
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

// Graph Handlers

VOID CCADCtrl::Paint (VOID)
{
	DrawLineMode();

	if(CAD::DrawLine == m_eMouse)
	{
		DrawVertexMode();
		DrawLineDrawing();
	}
	else if(CAD::Vertex == m_eMode || CAD::Drawing == m_eMode)
		DrawVertexMode();
}

FLOAT PointDistanceFromLine (FLOAT x1, FLOAT y1, FLOAT x2, FLOAT y2, FLOAT x3, FLOAT y3)
{
	float px = x2 - x1;
	float py = y2 - y1;
	float temp = (px * px) + (py * py);
	float u = ((x3 - x1) * px + (y3 - y1) * py) / temp;
	if(u > 1.0f)
		u = 1.0f;
	else if(u < 0.0f)
		u = 0.0f;

	float x = x1 + u * px;
	float y = y1 + u * py;

	float dx = x - x3;
	float dy = y - y3;
	return sqrt(dx * dx + dy * dy);
}

VOID CCADCtrl::LBtnDown (KEYS dwKeys, FLOAT x, FLOAT y)
{
	if(CAD::DrawLine == m_eMouse)
	{
		SnapCoordinate(x, y);
		m_xMouse = x;
		m_yMouse = y;

		// TODO - Ignore clicks on previous vertices other than the first vertex in the new points list.
		if(m_aNewPoints.Length() > 0 && m_aNewPoints[0].x == x && m_aNewPoints[0].y == y)
		{
			TArray<DWORD> aCommitted;

			for(sysint i = 0; i < m_aNewPoints.Length(); i++)
			{
				const XYPOINT& pt = m_aNewPoints[i];
				DWORD* pidVertex;

				aCommitted.AppendSlot(&pidVertex);
				FindOrCreateVertex(pt.x, pt.y, pidVertex);
			}

			for(sysint i = 0; i < aCommitted.Length(); i++)
			{
				DWORD idA = aCommitted[i];
				DWORD idB = aCommitted[(i + 1) % aCommitted.Length()];

				SideAssertHr(IntegrateLine(idA, idB));
			}

			DeselectAll();
			m_aNewPoints.Clear();
			m_eMouse = CAD::None;
			m_pHost->OnEndDrawing();
		}
		else
		{
			XYPOINT pt;
			pt.x = x;
			pt.y = y;

			m_aNewPoints.Append(pt);

			m_pHost->OnDrawLineStatus(0.0f);
			DeselectAll(CAD_SELECTED | CAD_HOVER);
		}

		m_pHost->InvalidateContainer(m_pGraph);
	}
	else
	{
		m_xMouse = x;
		m_yMouse = y;
		m_eMouse = CAD::Down;
	}
}

VOID CCADCtrl::LBtnUp (KEYS dwKeys, FLOAT x, FLOAT y)
{
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
			{
				break;
			}
		}

		if(selectedVertexKey != CAD_INVALID)
		{
			const DWORD overlapVertexKey = GetOtherVertexFromPosition(x, y, pCadVertex);
			if(overlapVertexKey != CAD_INVALID)
			{
				MergeVertex(overlapVertexKey, selectedVertexKey);
			}
		}
	}

	if(CAD::DrawLine != m_eMouse)
	{
		m_eMouse = CAD::None;
	}
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
		if(CAD::DrawLine != m_eMouse)
		{
			m_idNewPolygon = GetFreePolygon();

			TStackRef<ICADPolygon> srPolygon;
			if(m_pHost->OnCreatePolygon(m_idNewPolygon, &srPolygon) &&
				SUCCEEDED(AddPolygon(m_idNewPolygon, srPolygon)))
			{
				XYPOINT pt;

				SnapCoordinate(x, y);
				pt.x = x;
				pt.y = y;

				if(SUCCEEDED(m_aNewPoints.Append(pt)))
				{
					m_eMouse = CAD::DrawLine;
					m_pHost->OnBeginDrawing();

					m_xMouse = x;
					m_yMouse = y;

					DeselectAll();
					m_pHost->InvalidateContainer(m_pGraph);
				}
			}
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
}

VOID CCADCtrl::MouseMove (KEYS dwKeys, FLOAT x, FLOAT y)
{
	if(CAD::None == m_eMouse)
	{
		BOOL fRedraw = FALSE;
		switch(m_eMode)
		{
		case CAD::Vertex:
		case CAD::Drawing:
			{
				BOOL fLineHover = LineHover(x, y);
				BOOL fVertexHover = VertexHover(x, y);
				fRedraw = (fLineHover || fVertexHover);
			}
			break;
		case CAD::Line:
			fRedraw = LineHover(x, y);
			m_pHost->InvalidateContainer(m_pGraph);
			break;
		case CAD::Polygon:
			fRedraw = PolygonHover(x, y);
			break;
		}

		if(fRedraw)
			m_pHost->InvalidateContainer(m_pGraph);
	}
	else if(CAD::DrawLine == m_eMouse)
	{
		m_xMouse = x;
		m_yMouse = y;
		SnapCoordinate(m_xMouse, m_yMouse);
		VertexHover(m_xMouse, m_yMouse);

		XYPOINT& ptPrev = m_aNewPoints[m_aNewPoints.Length() - 1];
		FLOAT xDiff = m_xMouse - ptPrev.x;
		FLOAT yDiff = m_yMouse - ptPrev.y;
		m_pHost->OnDrawLineStatus(sqrtf(xDiff * xDiff + yDiff * yDiff));

		m_pHost->InvalidateContainer(m_pGraph);
	}
	else
	{
		if(CAD::Down == m_eMouse)
		{
			if(fabs(m_xMouse - x) > 2.5f || fabs(m_yMouse - y) > 2.5f)
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
						{
							AdjustVertex(pCadVertex, xDelta, yDelta);
						}
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

BOOL CCADCtrl::KeyUp (WPARAM iKey)
{
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
	else if(VK_ESCAPE == iKey && CAD::DrawLine == m_eMouse)
	{
		DeselectAll();
		m_aNewPoints.Clear();
		m_eMouse = CAD::None;
		m_pHost->OnEndDrawing();
		m_pHost->InvalidateContainer(m_pGraph);
		return TRUE;
	}
	else if('F' == iKey && CAD::Line == m_eMode)
	{
		FlipSelectedLines();
		m_pHost->InvalidateContainer(m_pGraph);
		return TRUE;
	}
	return FALSE;
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
		// Ray intersects edge
		if (((a.y > pt.y) != (b.y > pt.y)) &&
			(pt.x < (b.x - a.x) * (pt.y - a.y) / (b.y - a.y + 1e-20f) + a.x))
		{
			count++;
		}
	}

	return (count % 2) == 1;
}

float PointToSegmentDistance(const FPOINT& p, const FPOINT& a, const FPOINT& b)
{
	float dx = b.x - a.x;
	float dy = b.y - a.y;

	if(dx == 0 && dy == 0)
	{
		// a == b
		dx = p.x - a.x;
		dy = p.y - a.y;
		return sqrt(dx * dx + dy * dy);
	}

	// Parametric projection t
	float t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / (dx * dx + dy * dy);
	t = max(0.0f, min(1.0f, t));

	float projX = a.x + t * dx;
	float projY = a.y + t * dy;

	dx = p.x - projX;
	dy = p.y - projY;

	return sqrt(dx * dx + dy * dy);
}

float MinDistanceToPolygon (const TArray<FPOINT>& polygon, const FPOINT& pt)
{
	float minDist = FLT_MAX;
	for(sysint i = 0; i < polygon.Length(); i++)
	{
		const FPOINT& a = polygon[i];
		const FPOINT& b = polygon[(i + 1) % polygon.Length()];
		float dist = PointToSegmentDistance(pt, a, b);
		if (dist < minDist)
			minDist = dist;
	}
	return minDist;
}

DWORD CCADCtrl::GetPolygonFromLine (FPOINT a, FPOINT b, DWORD idIgnore)
{
	DWORD idBestPoly = CAD_INVALID;
	FLOAT minDistance = FLT_MAX;

	for(int i = 0; i < m_pPolygons->Length(); i++)
	{
		DWORD polygonKey;
		CAD_POLYGON* pCadPolygon = NULL;
		m_pPolygons->GetKeyAndValue(i, &polygonKey, &pCadPolygon);
		if(idIgnore == polygonKey)
			continue;

		TArray<FPOINT> polygon;
		GetPolygonPoints(polygonKey, polygon);
		if(IsPointInPolygon(polygon, a) && IsPointInPolygon(polygon, b))
		{
			FPOINT pt;
			pt.x = (a.x + b.x) / 2.0f;
			pt.y = (a.y + b.y) / 2.0f;
			pt.z = 0.f;
			FLOAT distanceToPolygon = MinDistanceToPolygon(polygon, pt);
			if(minDistance > distanceToPolygon)
			{
				idBestPoly = polygonKey;
				minDistance = distanceToPolygon;
			}
		}
	}
	return idBestPoly;
}

DWORD CCADCtrl::GetPolygonFromPoint (FLOAT x, FLOAT y, DWORD idIgnore)
{
	DWORD idBestPoly = CAD_INVALID;
	FLOAT minDistance = FLT_MAX;
	FPOINT pt;
	pt.x = x;
	pt.y = y;
	pt.z = 0.0f;

	for(int i = 0; i < m_pPolygons->Length(); i++)
	{
		DWORD polygonKey;
		CAD_POLYGON* pCadPolygon = NULL;

		SideAssertHr(m_pPolygons->GetKeyAndValue(i, &polygonKey, &pCadPolygon));
		if(idIgnore == polygonKey)
			continue;

		TArray<FPOINT> polygon;
		GetPolygonPoints(polygonKey, polygon);
		if(IsPointInPolygon(polygon, pt))
		{
			FLOAT distanceToPolygon = MinDistanceToPolygon(polygon, pt);
			if(minDistance > distanceToPolygon)
			{
				idBestPoly = polygonKey;
				minDistance = distanceToPolygon;
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

HRESULT CCADCtrl::SplitLine (FLOAT x, FLOAT y, __out_opt DWORD* pidVertex)
{
	HRESULT hr = S_FALSE;
	TStackRef<ICADLine> srNewSplitA, srNewSplitB;
	FPOINT vertex;
	vertex.x = x;
	vertex.y = y;
	vertex.z = 0.f;
	sysint idxVertex = -1;
	DWORD idNewVertex;
	if(FindVertex(vertex, &idxVertex))
	{
		idNewVertex = m_pVertices->GetKey(idxVertex);
	}
	else
	{
		idNewVertex = GetFreeVertex();
		Check(AddVertex(idNewVertex, vertex));
	}

	sysint i = 0;
	while(i < m_pLines->Length())
	{
		CAD_LINE* pCadLine;
		m_pLines->GetValueChecked(i, &pCadLine);
		CAD_VERTEX *vertexA, *vertexB;
		m_pVertices->Find(pCadLine->idVertexA, &vertexA);
		m_pVertices->Find(pCadLine->idVertexB, &vertexB);

		if(pCadLine->nFlags & CAD_SELECTED)
		{
			if(pCadLine->idVertexA == idNewVertex || pCadLine->idVertexB == idNewVertex)
			{
				i++;
				continue;
			}
			if(CAD_INVALID != pCadLine->idPolygonA)
				CheckIf(!m_pHost->OnSplitLine(pCadLine, vertex, pCadLine->pLineA, &srNewSplitA), E_FAIL);
			if(CAD_INVALID != pCadLine->idPolygonB)
				CheckIf(!m_pHost->OnSplitLine(pCadLine, vertex, pCadLine->pLineB, &srNewSplitB), E_FAIL);
			DWORD idNewLine = GetFreeLine();
			FLOAT midX = (vertex.x + vertexB->vertex.x) / 2.0f;
			FLOAT midY = (vertex.y + vertexB->vertex.y) / 2.0f;

			DWORD idEnclosing = GetPolygonFromLine(vertex, vertexB->vertex, pCadLine->idPolygonA);
			Check(AddLine(idNewLine, idNewVertex, pCadLine->idVertexB, pCadLine->idPolygonA, idEnclosing, srNewSplitA, srNewSplitB));

			midX = (vertex.x + vertexA->vertex.x) / 2.0f;
			midY = (vertex.y + vertexA->vertex.y) / 2.0f;

			idEnclosing = GetPolygonFromLine(vertex, vertexA->vertex, pCadLine->idPolygonA);
			pCadLine->idVertexB = idNewVertex;
			pCadLine->idPolygonB = idEnclosing;
		}
		i++;
	}
	if(pidVertex)
		*pidVertex = idNewVertex;

Cleanup:
	return hr;
}

HRESULT CCADCtrl::FindOrCreateVertex (FLOAT x, FLOAT y, __out DWORD* pidVertex)
{
	HRESULT hr;
	FPOINT vertex;

	SnapCoordinate(x, y);

	LineSelect(x, y, true, false);
	Check(SplitLine(x, y, pidVertex));

	if(S_FALSE == hr)
	{
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
	}

Cleanup:
	return hr;
}

HRESULT CCADCtrl::IntegrateLine (DWORD idVertexA, DWORD idVertexB)
{
	HRESULT hr;
	CAD_VERTEX* pA; CAD_VERTEX* pB;

	Check(m_pVertices->Find(idVertexA, &pA));
	Check(m_pVertices->Find(idVertexB, &pB));

	DWORD idEnclosing = GetPolygonFromLine(pA->vertex, pB->vertex, m_idNewPolygon);

	// Look for existing line to reuse
	bool fReused = false;

	for(sysint j = 0; j < m_pLines->Length(); j++)
	{
		CAD_LINE* pCadLine;

		Check(m_pLines->GetValueChecked(j, &pCadLine));
		if((pCadLine->idVertexA == idVertexA && pCadLine->idVertexB == idVertexB) ||
			(pCadLine->idVertexA == idVertexB && pCadLine->idVertexB == idVertexA))
		{
			// If line already belongs to one polygon, make it double-sided
			if (pCadLine->idPolygonA == CAD_INVALID)
				pCadLine->idPolygonA = m_idNewPolygon;
			else if (pCadLine->idPolygonB == CAD_INVALID)
				pCadLine->idPolygonB = m_idNewPolygon;
			else
				continue; // already double-sided

			fReused = true;
			break;
		}
	}

	if(!fReused)
	{
		DWORD idLine = GetFreeLine();
		TStackRef<ICADLine> srLine;

		Check(m_pHost->OnCreateLine(m_idNewPolygon, idLine, &srLine));
		Check(AddLine(idLine, idVertexA, idVertexB, m_idNewPolygon, idEnclosing, srLine, NULL));
	}

Cleanup:
	return hr;
}

BOOL CCADCtrl::VertexHover (FLOAT x, FLOAT y)
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

BOOL CCADCtrl::LineHover (FLOAT x, FLOAT y)
{
	BOOL fChanged = FALSE;

	for(sysint i = 0; i < m_pLines->Length(); i++)
	{
		CAD_LINE* pCadLine;
		SideAssertHr(m_pLines->GetValueChecked(i, &pCadLine));

		CAD_VERTEX* pVertexA, * pVertexB;
		SideAssertHr(m_pVertices->Find(pCadLine->idVertexA, &pVertexA));
		SideAssertHr(m_pVertices->Find(pCadLine->idVertexB, &pVertexB));

		const BOOL selected = PointDistanceFromLine(pVertexA->vertex.x, pVertexA->vertex.y, pVertexB->vertex.x, pVertexB->vertex.y, x, y) < 2.5f;
		ToggleSelection(pCadLine->nFlags, selected, fChanged);
	}

	return fChanged;
}

BOOL CCADCtrl::PolygonHover (FLOAT x, FLOAT y)
{
	BOOL fChanged = FALSE;
	DWORD idPolygon = GetPolygonFromPoint(x, y, CAD_INVALID);

	for(sysint i = 0; i < m_pPolygons->Length(); i++)
	{
		CAD_POLYGON* pCadPolygon;

		SideAssertHr(m_pPolygons->GetValueChecked(i, &pCadPolygon));
		ToggleSelection(pCadPolygon->nFlags, idPolygon == m_pPolygons->GetKey(i), fChanged);
	}

	for(sysint i = 0; i < m_pLines->Length(); i++)
	{
		CAD_LINE* pCadLine;

		SideAssertHr(m_pLines->GetValueChecked(i, &pCadLine));
		const BOOL bSelected = (idPolygon != CAD_INVALID && (pCadLine->idPolygonA == idPolygon || pCadLine->idPolygonB == idPolygon));
		ToggleSelection(pCadLine->nFlags, bSelected, fChanged);
	}

	return fChanged;
}

VOID CCADCtrl::VertexSelect (FLOAT x, FLOAT y, bool fDeselectOthers, bool fAllowToggle)
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
				pCadVertex->nFlags |= CAD_SELECTED;
		}
		else if(pCadVertex->nFlags & CAD_SELECTED)
		{
			if(fDeselectOthers)
				pCadVertex->nFlags &= ~CAD_SELECTED;
		}
	}
}

VOID CCADCtrl::LineSelect (FLOAT x, FLOAT y, bool fDeselectOthers, bool fAllowToggle)
{
	for(sysint i = 0; i < m_pLines->Length(); ++i)
	{
		CAD_LINE *pCadLine;
		SideAssertHr(m_pLines->GetValueChecked(i, &pCadLine));

		CAD_VERTEX *pVertexA, *pVertexB;
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
				{
					pCadLine->nFlags &= ~CAD_SELECTED;
				}
			}
			else
			{
				pCadLine->nFlags |= CAD_SELECTED;
			}
		}
		else if(pCadLine->nFlags & CAD_SELECTED)
		{
			if(fDeselectOthers)
			{
				pCadLine->nFlags &= ~CAD_SELECTED;
			}
		}
	}
}

VOID CCADCtrl::PolygonSelect (FLOAT x, FLOAT y, bool fDeselectOthers, bool fAllowToggle)
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
					pCadPolygon->nFlags |= CAD_SELECTED;
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
					pCadLine->nFlags |= CAD_SELECTED;
			}
			else if(pCadLine->nFlags & CAD_SELECTED)
			{
				if(fDeselectOthers)
					pCadLine->nFlags &= ~CAD_SELECTED;
			}
		}
	}
}

VOID CCADCtrl::DrawVertexMode (VOID)
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
		else if(CAD_INVALID != pCadLine->idPolygonB)
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

VOID CCADCtrl::DrawLineDrawing (VOID)
{
	HPEN hpnDefault = m_pGraph->SelectPen(m_pnDrawing);
	FPOINT fptB;

	if(1 < m_aNewPoints.Length())
	{
		for(sysint i = 1; i < m_aNewPoints.Length(); i++)
		{
			XYPOINT& ptA = m_aNewPoints[i - 1];
			XYPOINT& ptB = m_aNewPoints[i];
			FPOINT fptA;

			fptA.x = ptA.x;
			fptA.y = ptA.y;
			fptA.z = 0.0f;

			m_pGraph->Rectangle(fptA.x - 2.5f, fptA.y - 2.5f, fptA.z - 2.5f,
				fptA.x + 2.5f, fptA.y + 2.5f, fptA.z + 2.5f);

			fptB.x = ptB.x;
			fptB.y = ptB.y;
			fptB.z = 0.0f;

			DrawLineSegment(fptA, fptB);
		}
	}
	else
	{
		Assert(1 == m_aNewPoints.Length());

		XYPOINT& ptB = m_aNewPoints[0];
		fptB.x = ptB.x;
		fptB.y = ptB.y;
		fptB.z = 0.0f;
	}

	m_pGraph->Rectangle(fptB.x - 2.5f, fptB.y - 2.5f, fptB.z - 2.5f,
		fptB.x + 2.5f, fptB.y + 2.5f, fptB.z + 2.5f);

	FPOINT fpTo;
	fpTo.x = m_xMouse;
	fpTo.y = m_yMouse;
	fpTo.z = 0.0f;

	DrawLineSegment(fptB, fpTo);

	m_pGraph->SelectPen(hpnDefault);
}

VOID CCADCtrl::DrawLineSegment (const FPOINT& vFrom, const FPOINT& vTo)
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
	const FPOINT vertex = { x, y };
	TArray<DWORD> vertices;
	for(sysint i = 0; i < m_pVertices->Length(); ++i)
	{
		DWORD vertexKey;
		CAD_VERTEX* pVertex;
		m_pVertices->GetKeyAndValue(i, &vertexKey, &pVertex);
		if(pVertex != pCurrentVertex && IsPointEqual(pVertex->vertex, vertex))
		{
			return vertexKey;
		}
	}
	return CAD_INVALID;
}

bool CCADCtrl::FindVertex (const FPOINT& vertex, __out sysint* pidx)
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

bool CCADCtrl::IsPointOver (FLOAT x, FLOAT y, const FPOINT* pVertex)
{
	return fabsf(pVertex->x - x) < 2.5f && fabsf(pVertex->y - y) < 2.5f;
}

VOID CCADCtrl::AdjustVertex (CAD_VERTEX* pCadVertex, FLOAT xDelta, FLOAT yDelta)
{
	pCadVertex->vertex.x += xDelta;
	pCadVertex->vertex.y += yDelta;
}

bool CCADCtrl::IsPointEqual (const FPOINT& fpA, const FPOINT& fpB)
{
	return fabs(fpA.x - fpB.x) < 0.01f &&
		fabs(fpA.y - fpB.y) < 0.01f &&
		fabs(fpA.z - fpB.z) < 0.01f;
}

VOID CCADCtrl::ToggleSelection (UINT& nFlags, BOOL fSelect, __inout BOOL& fChanged)
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

FLOAT CCADCtrl::CrossProduct (const FPOINT& a, const FPOINT& b, FLOAT x, FLOAT y)
{
    return (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
}

// Cross product to check relative position
FLOAT Cross (const FPOINT& a, const FPOINT& b, const FPOINT& c)
{
	return (b.x - a.x)*(c.y - a.y) - (b.y - a.y)*(c.x - a.x);
}

// Check if point p is inside the edge AB
bool IsInside (const FPOINT& a, const FPOINT& b, const FPOINT& p)
{
	return Cross(a, b, p) >= 0;
}

// Line segment intersection: return intersection of segment AB and PQ
FPOINT ComputeIntersection(const FPOINT& a1, const FPOINT& a2, const FPOINT& b1, const FPOINT& b2)
{
	FLOAT A1 = a2.y - a1.y;
	FLOAT B1 = a1.x - a2.x;
	FLOAT C1 = A1 * a1.x + B1 * a1.y;

	FLOAT A2 = b2.y - b1.y;
	FLOAT B2 = b1.x - b2.x;
	FLOAT C2 = A2 * b1.x + B2 * b1.y;

	FLOAT det = A1 * B2 - A2 * B1;
	FPOINT result = {0, 0, 0};
	if (det != 0)
	{
		result.x = (B2 * C1 - B1 * C2) / det;
		result.y = (A1 * C2 - A2 * C1) / det;
	}
	// z stays 0
	return result;
}

HRESULT PolygonIntersection (const TArray<FPOINT>& subject, const TArray<FPOINT>& clip, __out TArray<FPOINT> output)
{
	HRESULT hr;

	for(sysint i = 0; i < subject.Length(); i++)
		Check(output.Append(subject[i]));

	for(sysint i = 0; i < clip.Length(); i++)
	{
		TArray<FPOINT> input;

		input.Swap(output);

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

HRESULT CCADCtrl::DiffPolygon (DWORD subPolygonKey, DWORD clipPolygonKey)
{
	HRESULT hr;
	TArray<FPOINT> subPoints, clipPoints, intersectionPoints;

	Check(GetPolygonPoints(subPolygonKey, subPoints));
	Check(GetPolygonPoints(clipPolygonKey, clipPoints));
	Check(PolygonIntersection(subPoints, clipPoints, intersectionPoints));

	if(intersectionPoints.Length() > 0)
	{
		TArray<DWORD> newVertexs, orderedVertexs;

		for(sysint j = 0; j < intersectionPoints.Length(); j++) 
		{
			DWORD idVertex;
			sysint nPosition;

			Check(FindOrCreateVertex(intersectionPoints[j].x, intersectionPoints[j].y, &idVertex));
			DeselectAll();
			if(!newVertexs.IndexOf(idVertex, nPosition))
				Check(newVertexs.Append(idVertex));
		}
		
		Check(OrderPolygonPoints(newVertexs, orderedVertexs));
		for(sysint j = 0; j < newVertexs.Length(); j++) 
		{
			SetPolygonIDToLine(newVertexs[j], newVertexs[(j + 1) % newVertexs.Length()], subPolygonKey, clipPolygonKey);
		}
	}

Cleanup:
	return hr;
}

HRESULT CCADCtrl::GetPolygonPoints (DWORD polygonId, __out TArray<FPOINT>& polygonPoints)
{
	struct EDGE
	{
		DWORD first;
		DWORD second;
	};

	HRESULT hr;
	TArray<DWORD> orderedVertices;

	// This will store the edge connections: A → B
	TArray<EDGE> edges;

	sysint count = m_pLines->Length();
	for(sysint i = 0; i < count; i++)
	{
		DWORD lineKey;
		CAD_LINE* pCadLine;

		SideAssertHr(m_pLines->GetKeyAndValue(i, &lineKey, &pCadLine));

		if(pCadLine->idPolygonB == polygonId || pCadLine->idPolygonA == polygonId)
		{
			EDGE* pEdge;
			Check(edges.AppendSlot(&pEdge));
			pEdge->first = pCadLine->idVertexA;
			pEdge->second = pCadLine->idVertexB;
		}
	}

	CheckIfIgnore(edges.Length() == 0, S_FALSE);

	// Reconstruct the polygon path
	// Start from first edge
	Check(orderedVertices.Append(edges[0].first));
	DWORD current = edges[0].second;

	for(;;)
	{
		Check(orderedVertices.Append(current));

		bool foundNext = false;
		for(sysint i = 1; i < edges.Length(); i++)
		{
			Assert(2 <= orderedVertices.Length());

			if(edges[i].first == current && edges[i].second != orderedVertices[orderedVertices.Length() - 2])
			{
				current = edges[i].second;
				foundNext = true;
				break;
			}
			else if(edges[i].second == current && edges[i].first != orderedVertices[orderedVertices.Length() - 2])
			{
				current = edges[i].first;
				foundNext = true;
				break;
			}
		}

		if(!foundNext || current == orderedVertices[0])
			break;
	}

	// Convert vertex IDs to IntPoint
	for(sysint i = 0; i < orderedVertices.Length(); ++i)
	{
		CAD_VERTEX* pVertex;
		if(SUCCEEDED(m_pVertices->Find(orderedVertices[i], &pVertex)))
			Check(polygonPoints.Append(pVertex->vertex));
	}

Cleanup:
	return hr;
}

HRESULT CCADCtrl::OrderPolygonPoints (TArray<DWORD>& polygon, __out TArray<DWORD>& orderedPolygon)
{
	HRESULT hr;

	Check(orderedPolygon.Append(polygon[0]));
	Check(polygon.RemoveChecked(0, NULL));

	while(polygon.Length() > 0)
	{
		DWORD lastVertexkey = orderedPolygon[orderedPolygon.Length() - 1];

		for(int i = 0; i < m_pLines->Length(); i++)
		{
			CAD_LINE* pCadLine = NULL;

			m_pLines->GetValueChecked(i, &pCadLine);
			if(pCadLine->idVertexA == lastVertexkey)
			{
				int j = 0;
				for(; j < polygon.Length(); j++)
				{
					if(pCadLine->idVertexB == polygon[j])
						break;
				}
				if(j < polygon.Length())
				{
					Check(orderedPolygon.Append(polygon[j]));
					Check(polygon.RemoveChecked(j, NULL));
					break;
				}
			}
			else if(pCadLine->idVertexB == lastVertexkey)
			{
				int j = 0;
				for(; j < polygon.Length(); j++)
				{
					if(pCadLine->idVertexA == polygon[j])
						break;
				}
				if(j < polygon.Length())
				{
					Check(orderedPolygon.Append(polygon[j]));
					Check(polygon.RemoveChecked(j, NULL));
					break;
				}
			}
		}
	}

Cleanup:
	return hr;
}

VOID CCADCtrl::SetPolygonIDToLine (DWORD idVertexA, DWORD idVertexB, DWORD subPolygonKey, DWORD clipPoligonKey)
{
	CAD_LINE* pCadLine;

	for(sysint i = 0; i < m_pLines->Length(); i++)
	{
		DWORD lineKey;
		m_pLines->GetKeyAndValue(i,&lineKey, &pCadLine);
		if((pCadLine->idVertexA == idVertexA && pCadLine->idVertexB == idVertexB)
			|| (pCadLine->idVertexA == idVertexB && pCadLine->idVertexB == idVertexA))
		{
			if((pCadLine->idPolygonA == subPolygonKey && pCadLine->idPolygonB == CAD_INVALID)
				|| (pCadLine->idPolygonB == subPolygonKey && pCadLine->idPolygonA == CAD_INVALID))
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

			if(pCadLine->idPolygonA == subPolygonKey && pCadLine->idPolygonB != CAD_INVALID)
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
