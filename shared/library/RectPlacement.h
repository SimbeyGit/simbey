// --------------------------------------------------------------------------------
// Name		: RectPlacement.h
// Description : A class that allocates subrectangles into power-of-2 rectangles
//			   (C) Copyright 2000-2002 by Javier Arevalo
//			   This code is free to use and modify for all purposes
// --------------------------------------------------------------------------------

#ifndef _RECT_PLACEMENT_H_
#define _RECT_PLACEMENT_H_

#include "Library\Core\Array.h"

class CRectPlacement
{
public:
	struct POINTSIZE : POINT, SIZE {};

	CRectPlacement() {}
	~CRectPlacement() { End(); }

	HRESULT Init (int w = 1, int h = 1);
	void End ();
	bool IsOk () const { return m_size.cx > 0; }
	int GetW () const { return m_size.cx; }
	int GetH () const { return m_size.cy; }
	long GetArea () const { return m_area; }
	long GetTotalArea () const { return m_size.cx * m_size.cy; }
	bool AddAtEmptySpotAutoGrow (__inout POINTSIZE* pRect, int maxW, int maxH);

	static bool Contains (const POINTSIZE& ps, const POINTSIZE& r)
	{
		return (r.x >= ps.x && r.y >= ps.y && (r.x + r.cx) <= (ps.x + ps.cx) && (r.y + r.cy) <= (ps.y + ps.cy)); 
	}

	static bool Intersects (const POINTSIZE& ps, const POINTSIZE& r)
	{
		return ps.cx > 0 && ps.cy > 0 && r.cx > 0 && r.cy > 0 && ((r.x + r.cx) > ps.x && r.x < (ps.x + ps.cx) && (r.y + r.cy) > ps.y && r.y < (ps.y + ps.cy));
	}

private:
	POINTSIZE m_size;
	TArray<POINTSIZE> m_vRects;
	TArray<POINT> m_vPositions;
	long m_area;

	bool IsFree (const POINTSIZE& r);
	HRESULT AddPosition (const POINT& p);
	HRESULT AddRect (const POINTSIZE& r);
	bool AddAtEmptySpot (POINTSIZE& r);
};

#endif //_RECT_PLACEMENT_H_
