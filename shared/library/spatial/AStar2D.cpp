#include <windows.h>
#include "AStar2D.h"

CAStar2D::CAStar2D ()
{
}

CAStar2D::~CAStar2D ()
{
	Reset();
}

HRESULT CAStar2D::PreInit (VOID)
{
	HRESULT hr = S_OK;

	for(INT i = 0; i < 128; i++)
	{
		NODE* pNode = __new NODE;
		CheckAlloc(pNode);
		pNode->pNext = NULL;
		m_listFreeNodes.AppendNode(pNode);
	}

Cleanup:
	return hr;
}

HRESULT CAStar2D::FindPath (INT xFrom, INT yFrom, INT xDest, INT yDest, INT nRange, IAStarCallback2D* pCallback)
{
	HRESULT hr;
	INT xMax = (xFrom + nRange) - 1;
	INT yMax = (yFrom + nRange) - 1;

	m_x = xFrom - nRange;
	m_y = yFrom - nRange;
	m_nSize = nRange << 1;

	Reset();

	Check(AddOpenNode((ULONG)-1, xFrom, yFrom, 0, MoveDistance(xFrom, yFrom, xDest, yDest)));

	for(;;)
	{
		NODE* p = *(m_mapOpen.GetValuePtr(0));
		INT f = p->vItem.g + p->vItem.h;
		sysint nIndex = 0;
		ULONG nCurrent;

		// Find the node with the lowest F score.
		for(sysint i = 1; i < m_mapOpen.Length(); i++)
		{
			INT fCompare;

			p = *(m_mapOpen.GetValuePtr(i));
			fCompare = p->vItem.g + p->vItem.h;

			if(fCompare < f)
			{
				f = fCompare;
				nIndex = i;
			}
		}

		p = *(m_mapOpen.GetValuePtr(nIndex));
		nCurrent = m_mapOpen.GetKey(nIndex);

		// Switch the current node to the closed list (and remove it from the open list).
		Check(m_mapClosed.Add(nCurrent, p));
		SideAssertHr(m_mapOpen.RemoveByIndex(nIndex, NULL));

		if(p->vItem.x == xDest && p->vItem.y == yDest)
		{
			// Found it!
			break;
		}

		// For each of the nodes around the current node...
		INT xEnd = min(p->vItem.x + 1, xMax);
		INT yEnd = min(p->vItem.y + 1, yMax);
		for(INT y = max(p->vItem.y - 1, m_y); y <= yEnd; y++)
		{
			for(INT x = max(p->vItem.x - 1, m_x); x <= xEnd; x++)
			{
				ULONG nNode = CoordToIndex(x, y);
				NODE* pOpen;

				if(S_OK == m_mapOpen.Find(nNode, &pOpen))	// If it's already on the open list...
				{
					INT nValue;

					// If the G score is better from this direction, update the G score and parent.
					if(GetAndAdjustValue(pCallback, x, y, p, &nValue) && nValue < pOpen->vItem.g)
					{
						pOpen->vItem.g = nValue;
						pOpen->vItem.nParent = nCurrent;
					}
				}
				else if(!m_mapClosed.HasItem(nNode))		// Ignore if it's on the closed list.
				{
					INT nValue;

					// If we can get a value for this path, then add the node to the open list.
					if(GetAndAdjustValue(pCallback, x, y, p, &nValue))
					{
						Check(AddOpenNode(nCurrent, x, y, nValue, MoveDistance(x, y, xDest, yDest)));
					}
				}
			}
		}

		CheckIf(0 == m_mapOpen.Length(), HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
	}

Cleanup:
	return hr;
}

HRESULT CAStar2D::GetPath (INT xDest, INT yDest, TArray<POINT>* paPath)
{
	HRESULT hr;
	NODE* p;
	POINT pt;
	ULONG nIndex = CoordToIndex(xDest, yDest);

	for(;;)
	{
		Check(m_mapClosed.Find(nIndex, &p));
		pt.x = p->vItem.x;
		pt.y = p->vItem.y;
		Check(paPath->Append(&pt));
		nIndex = p->vItem.nParent;
		CheckIf((ULONG)-1 == nIndex, S_OK);
	}

Cleanup:
	return hr;
}

VOID CAStar2D::Reset (VOID)
{
	NODE* p;

	for(sysint i = m_mapOpen.Length() - 1; i >= 0; i--)
	{
		SideAssertHr(m_mapOpen.RemoveByIndex(i, &p));
		p->pNext = NULL;
		m_listFreeNodes.AppendNode(p);
	}

	for(sysint i = m_mapClosed.Length() - 1; i >= 0; i--)
	{
		SideAssertHr(m_mapClosed.RemoveByIndex(i, &p));
		p->pNext = NULL;
		m_listFreeNodes.AppendNode(p);
	}
}

HRESULT CAStar2D::AddOpenNode (ULONG nParent, INT x, INT y, INT g, INT h)
{
	HRESULT hr;
	ULONG nIndex = CoordToIndex(x, y);
	NODE* pNode = m_listFreeNodes.UnlinkHead();

	if(NULL == pNode)
	{
		pNode = __new NODE;
		CheckAlloc(pNode);
		pNode->pNext = NULL;
	}

	hr = m_mapOpen.Add(nIndex, pNode);
	if(FAILED(hr))
	{
		m_listFreeNodes.AppendNode(pNode);
		Check(hr);
	}

	pNode->vItem.nParent = nParent;
	pNode->vItem.x = x;
	pNode->vItem.y = y;
	pNode->vItem.g = g;
	pNode->vItem.h = h;

Cleanup:
	return hr;
}

BOOL CAStar2D::GetAndAdjustValue (IAStarCallback2D* pCallback, INT x, INT y, NODE* p, __out INT* pnValue)
{
	if(pCallback->GetPathValue(x, y, p->vItem.x, p->vItem.y, pnValue))
	{
		*pnValue += p->vItem.g;
		if(x == p->vItem.x || y == p->vItem.y)
			*pnValue += ADJACENT_MOVEMENT_COST;
		else
			*pnValue += DIAGONAL_MOVEMENT_COST;
		return TRUE;
	}
	return FALSE;
}
