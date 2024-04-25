#include <math.h>
#include <windows.h>
#include "Core\CoreDefs.h"
#include "Spatial\Geometry.h"
#include "CADCtrl.h"

#define MIN_DISTANCE_FROM_LINE 2.5F

static void reorderLinesOnPolygon (DWORD polygonID, LineMap* pLines, VertexMap* pVertices)
{
	if(polygonID == CAD_INVALID)
	{
		const WCHAR message[] = L"Invalid polygon\n";
		OutputDebugString(message);
		return;
	}

	TArray<DWORD> linesOnPolygon;
	TMap<DWORD, DWORD> vertices;
	for(sysint lineIndex = 0; lineIndex < pLines->Length(); ++lineIndex)
	{
		DWORD lineKey = CAD_INVALID;
		CAD_LINE* pLine = NULL;
		pLines->GetKeyAndValue(lineIndex, &lineKey, &pLine);

		if(pLine->idPolygonA == polygonID || pLine->idPolygonB == polygonID)
		{
			linesOnPolygon.Append(lineKey);
			vertices.Add(pLine->idVertexA, pLine->idVertexB);
		}
	}

	/*
	WCHAR message[512];
	swprintf(message, L"Polygon %d\n", polygonID);
	OutputDebugString(message);
	*/

	for(sysint lineIndex = 0; lineIndex < linesOnPolygon.Length(); ++lineIndex)
	{
		const DWORD lineKey = linesOnPolygon[lineIndex];
		CAD_LINE* pLine = *(*pLines)[lineKey];

		CAD_VERTEX* pVertexA = *(*pVertices)[pLine->idVertexA];
		CAD_VERTEX* pVertexB = *(*pVertices)[pLine->idVertexB];

		/*
		swprintf(message, L"%d = (%F, %F) ---> %d = (%F, %F)\n",
			pLine->idVertexA, pVertexA->vertex.x, pVertexA->vertex.y,
			pLine->idVertexB, pVertexB->vertex.x, pVertexB->vertex.y);
		OutputDebugString(message);
		*/
	}
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
	TArray<DWORD> polygonToDelete;
	TArray<DWORD> selectedPolygon;

	for(int polygonIndex = 0; polygonIndex < m_pPolygons->Length(); ++polygonIndex)
	{
		DWORD polygonKey = CAD_INVALID;
		CAD_POLYGON* pCadPolygon = NULL;
		m_pPolygons->GetKeyAndValue(polygonIndex, &polygonKey, &pCadPolygon);
		if(pCadPolygon->nFlags & CAD_SELECTED)
		{
			selectedPolygon.Append(polygonKey);
		}
	}

	for(sysint polygonIndex = 0; polygonIndex < selectedPolygon.Length(); ++polygonIndex)
	{
		const DWORD polygonKey = selectedPolygon[polygonIndex];
		CAD_POLYGON* pCadPolygon = NULL;
		m_pPolygons->Remove(polygonKey, &pCadPolygon);
		__delete pCadPolygon;
	}

	return polygonToDelete.Length() > 0;
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
			 pCadLine->idVertexA == vertexToStay)) {
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

FLOAT PointRelationship (FLOAT x1, FLOAT y1, FLOAT x2, FLOAT y2, FLOAT x3, FLOAT y3)
{
	return ((x2 - x1) * (y3 - y1) - (y2 - y1) * (x3 - x1));
}

VOID CCADCtrl::LBtnDown (KEYS dwKeys, FLOAT x, FLOAT y)
{
	if(CAD::DrawLine == m_eMouse)
	{
		DWORD idVertex;

		if(SUCCEEDED(FindOrCreateVertex(x, y, &idVertex)))
		{
			CAD_VERTEX* pVertex;
			SideAssertHr(m_pVertices->Find(idVertex, &pVertex));

			m_xMouse = pVertex->vertex.x;
			m_yMouse = pVertex->vertex.y;

			if(SUCCEEDED(IntegrateLine(m_idDrawFromVertex, idVertex)))
			{
				m_idDrawFromVertex = idVertex;

				if(IsPolygonClosed(m_idNewPolygon, idVertex))
				{
					CheckAdjacentPolygon(m_idNewPolygon);
					DeselectAll();
					m_aNewPoints.Clear();
					m_eMouse = CAD::None;
					m_pHost->OnEndDrawing();
				}
				else
				{
					XYPOINT pt;
					pt.x = m_xMouse;
					pt.y = m_yMouse;
					m_aNewPoints.Append(pt);
					m_pHost->OnDrawLineStatus(0.0f);
					DeselectAll(CAD_SELECTED | CAD_HOVER);
				}

				m_pHost->InvalidateContainer(m_pGraph);

				// reorderLinesOnPolygon(m_idNewPolygon, m_pLines, m_pVertices);
			}
		}
		else
		{
			// TODO
		}
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
		LineSelect(x, y, true, false);
		SplitLine(x, y, NULL);
		DeselectAll();
		m_pHost->InvalidateContainer(m_pGraph);
		break;
	case CAD::Drawing:
		if(CAD::DrawLine != m_eMouse && SUCCEEDED(FindOrCreateVertex(x, y, &m_idDrawFromVertex)))
		{
			m_idNewPolygon = GetFreePolygon();

			TStackRef<ICADPolygon> srPolygon;
			if(m_pHost->OnCreatePolygon(m_idNewPolygon, &srPolygon) &&
				SUCCEEDED(AddPolygon(m_idNewPolygon, srPolygon)))
			{
				m_eMouse = CAD::DrawLine;
				m_pHost->OnBeginDrawing();

				CAD_VERTEX* pVertex;
				SideAssertHr(m_pVertices->Find(m_idDrawFromVertex, &pVertex));

				m_xMouse = pVertex->vertex.x;
				m_yMouse = pVertex->vertex.y;

				XYPOINT pt;
				pt.x = m_xMouse;
				pt.y = m_yMouse;
				m_aNewPoints.Append(pt);

				DeselectAll();
				m_pHost->InvalidateContainer(m_pGraph);
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
		case CAD::Polygon:
			deletingResult = DeleteSelectedLine();
			break;
		//case CAD::Polygon:
			// return DeleteSelectedPolygon() == TRUE;
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

DWORD CCADCtrl::GetPolygonFromPoint (FLOAT x, FLOAT y, DWORD idIgnore)
{
	FLOAT rClosest = 0.0f;
	CAD_LINE* pClosest = NULL;
	DWORD idPolygon = CAD_INVALID;

	for(sysint i = 0; i < m_pLines->Length(); i++)
	{
		CAD_LINE* pCadLine;
		SideAssertHr(m_pLines->GetValueChecked(i, &pCadLine));

		if(AnalyzeLine(pCadLine, idIgnore))
		{
			CAD_VERTEX* pVertexA, *pVertexB;
			SideAssertHr(m_pVertices->Find(pCadLine->idVertexA, &pVertexA));
			SideAssertHr(m_pVertices->Find(pCadLine->idVertexB, &pVertexB));

			FLOAT rDistance = PointDistanceFromLine(pVertexA->vertex.x, pVertexA->vertex.y, pVertexB->vertex.x, pVertexB->vertex.y, x, y);
			if(NULL == pClosest || rDistance < rClosest)
			{
				rClosest = rDistance;
				pClosest = pCadLine;
			}
		}
	}

	if(pClosest)
	{
		CAD_VERTEX* pVertexA, *pVertexB;
		SideAssertHr(m_pVertices->Find(pClosest->idVertexA, &pVertexA));
		SideAssertHr(m_pVertices->Find(pClosest->idVertexB, &pVertexB));

		if(PointRelationship(pVertexA->vertex.x, pVertexA->vertex.y, pVertexB->vertex.x, pVertexB->vertex.y, x, y) <= 0.0f)
		{
			idPolygon = pClosest->idPolygonA;
			if(idPolygon == idIgnore && CAD_INVALID != idIgnore)
				idPolygon = CAD_INVALID;
		}
		else
		{
			idPolygon = pClosest->idPolygonB;
			if(idPolygon == idIgnore && CAD_INVALID != idIgnore)
				idPolygon = CAD_INVALID;
		}
	}

	return idPolygon;
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
	HRESULT hr;
	TStackRef<ICADLine> srNewSplitA, srNewSplitB;
	CAD_LINE* pSelected = NULL;

	for(sysint i = 0; i < m_pLines->Length(); i++)
	{
		CAD_LINE* pCadLine;

		Check(m_pLines->GetValueChecked(i, &pCadLine));
		if(pCadLine->nFlags & CAD_SELECTED)
		{
			CheckIf(pSelected, S_FALSE);
			pSelected = pCadLine;
		}
	}

	CheckIf(NULL == pSelected, S_FALSE);

	CAD_VERTEX* pVertexA, *pVertexB;
	FPOINT fpSnapped;

	SnapCoordinate(x, y);
	Check(m_pVertices->Find(pSelected->idVertexA, &pVertexA));
	Check(m_pVertices->Find(pSelected->idVertexB, &pVertexB));

	fpSnapped.x = x;
	fpSnapped.y = y;
	fpSnapped.z = 0.0f;

	CheckIf(IsPointEqual(pVertexA->vertex, fpSnapped) || IsPointEqual(pVertexB->vertex, fpSnapped), S_FALSE);

	if(CAD_INVALID != pSelected->idPolygonA)
		CheckIf(!m_pHost->OnSplitLine(pSelected, fpSnapped, pSelected->pLineA, &srNewSplitA), E_FAIL);
	if(CAD_INVALID != pSelected->idPolygonB)
		CheckIf(!m_pHost->OnSplitLine(pSelected, fpSnapped, pSelected->pLineB, &srNewSplitB), E_FAIL);

	DWORD idNewVertex = GetFreeVertex();
	DWORD idNewLine = GetFreeLine();

	Check(AddVertex(idNewVertex, fpSnapped));
	Check(AddLine(idNewLine, idNewVertex, pSelected->idVertexB, pSelected->idPolygonA, pSelected->idPolygonB, srNewSplitA, srNewSplitB));
	pSelected->idVertexB = idNewVertex;

	if(pidVertex)
		*pidVertex = idNewVertex;

Cleanup:
	return hr;
}

HRESULT CCADCtrl::FindOrCreateVertex (FLOAT x, FLOAT y, __out DWORD* pidVertex)
{
	HRESULT hr;
	FPOINT vertex;

	LineSelect(x, y, true, false);
	Check(SplitLine(x, y, pidVertex));

	if(S_FALSE == hr)
	{
		SnapCoordinate(x, y);

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
	TStackRef<ICADLine> srLine;
	CAD_LINE* pCadLine;

	for(sysint i = 0; i < m_pLines->Length(); i++)
	{
		Check(m_pLines->GetValueChecked(i, &pCadLine));

		if(pCadLine->idVertexA == idVertexA && pCadLine->idVertexB == idVertexB)
		{
			CheckIf(CAD_INVALID != pCadLine->idPolygonA, E_FAIL);
			pCadLine->idPolygonA = m_idNewPolygon;
			goto SetDrawing;
		}
		else if(pCadLine->idVertexA == idVertexB && pCadLine->idVertexB == idVertexA)
		{
			CheckIf(CAD_INVALID != pCadLine->idPolygonB, E_FAIL);
			pCadLine->idPolygonB = m_idNewPolygon;
			goto SetDrawing;
		}
	}

	DWORD idLine = GetFreeLine();

	CheckIf(!m_pHost->OnCreateLine(m_idNewPolygon, idLine, &srLine), E_FAIL);
	Check(AddLine(idLine, idVertexA, idVertexB, m_idNewPolygon, CAD_INVALID, srLine, NULL));
	Check(m_pLines->Find(idLine, &pCadLine));

SetDrawing:
	pCadLine->nFlags |= CAD_DRAWING;

Cleanup:
	return hr;
}

bool CCADCtrl::IsPolygonClosed (DWORD idPolygon, DWORD idVertex)
{
	sysint cLines = m_pLines->Length(), cSegments = 0;
	for(sysint i = 0; i < cLines; i++)
	{
		CAD_LINE* pCadLine;
		SideAssertHr(m_pLines->GetValueChecked(i, &pCadLine));

		if((pCadLine->idVertexA == idVertex || pCadLine->idVertexB == idVertex) && (pCadLine->idPolygonA == idPolygon || pCadLine->idPolygonB == idPolygon))
		{
			if(2 == ++cSegments)
				return true;
		}
	}
	return false;
}

VOID CCADCtrl::CheckAdjacentPolygon (DWORD idPolygon)
{
	FLOAT rWindingSum = 0.0f;
	sysint cLines = m_pLines->Length();

	for(sysint i = 0; i < m_aNewPoints.Length(); i++)
	{
		XYPOINT& ptA = m_aNewPoints[i];
		XYPOINT& ptB = (i + 1 < m_aNewPoints.Length()) ? m_aNewPoints[i + 1] : m_aNewPoints[0];
		rWindingSum += (ptA.x * ptB.y) - (ptB.x * ptA.y);
	}

	for(sysint i = 0; i < cLines; i++)
	{
		CAD_LINE* pCadLine;
		SideAssertHr(m_pLines->GetValueChecked(i, &pCadLine));

		if((idPolygon == pCadLine->idPolygonA || idPolygon == pCadLine->idPolygonB) &&
			(CAD_INVALID == pCadLine->idPolygonA || CAD_INVALID == pCadLine->idPolygonB))
		{
			CAD_VERTEX* pVertexA, * pVertexB;
			SideAssertHr(m_pVertices->Find(pCadLine->idVertexA, &pVertexA));
			SideAssertHr(m_pVertices->Find(pCadLine->idVertexB, &pVertexB));

			if(0.0f < rWindingSum)
			{
				SwapData(pCadLine->idPolygonB, pCadLine->idPolygonA);
				SwapData(pCadLine->pLineB, pCadLine->pLineA);
			}

			DWORD idEnclosing = GetPolygonFromPoint((pVertexB->vertex.x + pVertexA->vertex.x) / 2.0f, (pVertexB->vertex.y + pVertexA->vertex.y) / 2.0f, idPolygon);
			if(CAD_INVALID != idEnclosing)
			{
				if(CAD_INVALID == pCadLine->idPolygonA)
					pCadLine->idPolygonA = idEnclosing;
				else
					pCadLine->idPolygonB = idEnclosing;
			}
		}
	}
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
	if(idPolygon == CAD_INVALID)
	{
		return;
	}

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

	FPOINT fpTo;
	fpTo.x = m_xMouse;
	fpTo.y = m_yMouse;
	fpTo.z = 0.0f;

	CAD_VERTEX* pVertex;
	if(SUCCEEDED(m_pVertices->Find(m_idDrawFromVertex, &pVertex)))
		DrawLineSegment(pVertex->vertex, fpTo);

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

bool CCADCtrl::AnalyzeLine (CAD_LINE* pCadLine, DWORD idIgnore)
{
	if(CAD_INVALID == idIgnore)
		return true;
	if((pCadLine->idPolygonA == idIgnore && CAD_INVALID == pCadLine->idPolygonB) ||
		(pCadLine->idPolygonB == idIgnore && CAD_INVALID == pCadLine->idPolygonA))
		return false;
	return true;
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
