#pragma once

#include "..\Core\Map.h"
#include "..\Core\List.h"

#define	ADJACENT_MOVEMENT_COST				10
#define	DIAGONAL_MOVEMENT_COST				14
#define	DISTANCE_ESTIMATE_MULTIPLIER		ADJACENT_MOVEMENT_COST

interface IAStarCallback2D
{
	virtual BOOL GetPathValue (INT x, INT y, INT xFrom, INT yFrom, __out INT* pnValue) = 0;
};

class CAStar2D
{
private:
	struct PATHDATA
	{
		ULONG nParent;
		INT x, y;
		INT g, h;
	};

	TList<PATHDATA> m_listFreeNodes;

	typedef TList<PATHDATA>::LIST_NODE NODE;

	TMap<ULONG, NODE*> m_mapClosed;
	TMap<ULONG, NODE*> m_mapOpen;

	INT m_x, m_y;
	INT m_nSize;
	INT m_cxWrap;   // 0 = no horizontal wrapping

public:
	CAStar2D ();
	~CAStar2D ();

	HRESULT PreInit (VOID);
	HRESULT FindPath (INT xFrom, INT yFrom, INT xDest, INT yDest, INT nRange, IAStarCallback2D* pCallback);
	HRESULT GetPath (INT xDest, INT yDest, TArray<POINT>* paPath);

	inline VOID SetHorizontalWrapWidth (INT cxWrap) { m_cxWrap = cxWrap; }

private:
	VOID Reset (VOID);
	HRESULT AddOpenNode (ULONG nParent, INT x, INT y, INT g, INT h);
	BOOL GetAndAdjustValue (IAStarCallback2D* pCallback, INT x, INT y, NODE* p, __out INT* pnValue);

	INT MoveDistance (INT x, INT y, INT xDest, INT yDest) const;
	inline ULONG CoordToIndex (INT x, INT y) { return (y - m_y) * m_nSize + (x - m_x); }
};
