// ----------------------------------------------------------------------------------------
// Name		: RectPlacement.cpp
// Description : A class that fits subrectangles into a power-of-2 rectangle
//			   (C) Copyright 2000-2002 by Javier Arevalo
//			   This code is free to use and modify for all purposes
// ----------------------------------------------------------------------------------------

/*
  You have a bunch of rectangular pieces. You need to arrange them in a 
  rectangular surface so that they don't overlap, keeping the total area of the 
  rectangle as small as possible. This is fairly common when arranging characters 
  in a bitmapped font, lightmaps for a 3D engine, and I guess other situations as 
  well.  The idea of this algorithm is that, as we add rectangles, we can pre-select 
  "interesting" places where we can try to add the next rectangles. For optimal 
  results, the rectangles should be added in order. I initially tried using area 
  as a sorting criteria, but it didn't work well with very tall or very flat 
  rectangles. I then tried using the longest dimension as a selector, and it 
  worked much better. So much for intuition...  These "interesting" places are just to the right and just below the currently 
  added rectangle. The first rectangle, obviously, goes at the top left, the next 
  one would go either to the right or below this one, and so on. It is a weird way 
  to do it, but it seems to work very nicely.  The way we search here is fairly brute-force, the fact being that for most off-
  line purposes the performance seems more than adequate. I have generated a 
  japanese font with around 8500 characters and all the time was spent generating 
  the bitmaps.  Also, for all we care, we could grow the parent rectangle in a different way 
  than power of two. It just happens that power of 2 is very convenient for 
  graphics hardware textures.  I'd be interested in hearing of other approaches to this problem. Make sure
  to post them on http://www.flipcode.com

*/

#include <Windows.h>
#include "Core\CoreDefs.h"
#include "RectPlacement.h"

HRESULT CRectPlacement::Init (int w, int h)
{
	const POINT ptZero = {0};
	End();

	m_size.x = 0;
	m_size.y = 0;
	m_size.cx = w;
	m_size.cy = h;
	m_area = 0;

	return m_vPositions.Append(ptZero);
}

void CRectPlacement::End ()
{
	m_vPositions.Clear();
	m_vRects.Clear();
	m_size.cx = 0;
	m_size.cy = 0;
}

// --------------------------------------------------------------------------------
// Name : IsFree
// Description : Check if the given rectangle is partially or totally used
// --------------------------------------------------------------------------------
bool CRectPlacement::IsFree (const POINTSIZE &r)
{
	if(!Contains(m_size, r))
		return false;

	for(sysint i = 0; i < m_vRects.Length(); i++)
	{
		if(Intersects(m_vRects[i], r))
			return false;
	}

	return true;
}

// --------------------------------------------------------------------------------
// Name : AddPosition
// Description : Add new anchor point
// --------------------------------------------------------------------------------
HRESULT CRectPlacement::AddPosition (const POINT& p)
{
	HRESULT hr;

	// Try to insert anchor as close as possible to the top left corner
	// So it will be tried first

	for(sysint i = 0; i < m_vPositions.Length(); i++)
	{
		if(p.x + p.y < m_vPositions[i].x + m_vPositions[i].y)
		{
			Check(m_vPositions.InsertAt(p, i));
			return hr;
		}
	}

	Check(m_vPositions.Append(p));

Cleanup:
	return hr;
}

// --------------------------------------------------------------------------------
// Name	: AddRect
// Description : Add the given rect and updates anchor points
// --------------------------------------------------------------------------------
HRESULT CRectPlacement::AddRect  (const POINTSIZE &r)
{
	HRESULT hr;
	POINT pt;

	Check(m_vRects.Append(r));
	m_area += r.cx * r.cy;		// Add two new anchor points

	pt.x = r.x;
	pt.y = r.y + r.cy;
	Check(AddPosition(pt));

	pt.x = r.x + r.cx;
	pt.y = r.y;
	Check(AddPosition(pt));

Cleanup:
	return hr;
}

// --------------------------------------------------------------------------------
// Name : AddAtEmptySpot
// Description : Add the given rectangle
// --------------------------------------------------------------------------------
bool CRectPlacement::AddAtEmptySpot (POINTSIZE &r)
{
	// Find a valid spot among available anchors.
	bool bFound = false;

	for(sysint i = 0; i < m_vPositions.Length(); i++)
	{
		POINTSIZE Rect;
		Rect.x = m_vPositions[i].x;
		Rect.y = m_vPositions[i].y;
		Rect.cx = r.cx;
		Rect.cy = r.cy;

		if(IsFree(Rect))
		{
			int x, y;
			r = Rect;

			// Remove the used anchor point
			m_vPositions.Remove(i, NULL);		// Sometimes, anchors end up displaced from the optimal position

			// due to irregular sizes of the subrects.
			// So, try to adjut it up & left as much as possible.
			for(x = 1; x <= r.x; x++)
			{
				POINTSIZE pos;
				pos.x = r.x - x;
				pos.y = r.y;
				pos.cx = r.cx;
				pos.cy = r.cy;
				if(!IsFree(pos))
					break;
			}
			for(y = 1; y <= r.y; y++)
			{
				POINTSIZE pos;
				pos.x = r.x;
				pos.y = r.y - y;
				pos.cx = r.cx;
				pos.cy = r.cy;
				if(!IsFree(pos))
					break;
			}
			if(y > x)
				r.y -= y-1;
			else
				r.x -= x-1;
			AddRect(r);

			bFound = true;
			break;
		}
	}

	return bFound;
}

// --------------------------------------------------------------------------------
// Name : AddAtEmptySpotAutoGrow
// Description : Add a rectangle of the given size, growing our area if needed
//			   Area grows only until the max given.
//			   Returns the placement of the rect in the rect's x,y coords
// --------------------------------------------------------------------------------
bool CRectPlacement::AddAtEmptySpotAutoGrow (__inout POINTSIZE* pRect, int maxW, int maxH)
{
	if(pRect->cx <= 0)
		return true;
	int orgW = m_size.cx;
	int orgH = m_size.cy;		// Try to add it in the existing space

	while(!AddAtEmptySpot(*pRect))
	{
		int pw = m_size.cx;
		int ph = m_size.cy;		// Sanity check - if area is complete.

		if(pw >= maxW && ph >= maxH)
		{
			m_size.cx = orgW;
			m_size.cy = orgH;
			return false;
		}
		
		// Try growing the smallest dim
		if(pw < maxW && (pw < ph || ((pw == ph) && (pRect->cx >= pRect->cy))))
			m_size.cx = pw*2;
		else
			m_size.cy = ph*2;

		if(AddAtEmptySpot(*pRect))
			break;
		
		// Try growing the other dim instead
		if(pw != m_size.cx)
		{
			m_size.cx = pw;
			if(ph < maxW)
				m_size.cy = ph*2;
		}
		else
		{
			m_size.cy = ph;
			if(pw < maxW)
				m_size.cx = pw*2;
		}

		if(pw != m_size.cx || ph != m_size.cy)
		{
			if(AddAtEmptySpot(*pRect))
				break;
		}

		// Grow both if possible, and reloop.
		m_size.cx = pw;
		m_size.cy = ph;

		if (pw < maxW)
			m_size.cx = pw*2;
		if (ph < maxH)
			m_size.cy = ph*2;
	}
	return true;
}
