#include <math.h>
#include <float.h>
#include <windows.h>
#include "Geometry.h"

namespace Geometry
{
	DOUBLE dPI = 3.1415926535;
	FLOAT rPI = 3.1415f;

	DOUBLE dE = 2.7182818284;
	FLOAT rE = 2.7182f;

	DOUBLE dRadians = dPI / 180.0;
	FLOAT rRadians = rPI / 180.0f;

	FLOAT lutSin[360 * 10];
	FLOAT lutCos[360 * 10];

	const DOUBLE SQRT_2 = 1.4142135623730950488;
	const DOUBLE SQRT_3 = 1.7320508075688772935;

	VOID WINAPI InitializeTables (VOID)
	{
		for(INT i = 0; i < ARRAYSIZE(lutSin); i++)
		{
			FLOAT r = TDegreesToRadians<FLOAT, FLOAT>((FLOAT)i / 10.0f);
			lutSin[i] = sinf(r);
			lutCos[i] = cosf(r);
		}
	}

	VOID WINAPI LutRotatePoint (PFPOINT lpPoint, FLOAT fDegrees)
	{
		DOUBLE fRadius = sqrt(lpPoint->x * lpPoint->x + lpPoint->z * lpPoint->z);
		DOUBLE fAngle = VertexAngle(lpPoint->x,lpPoint->z);
		lpPoint->x = (FLOAT)(LutCos(fAngle + fDegrees) * fRadius);
		lpPoint->z = (FLOAT)(LutSin(fAngle + fDegrees) * fRadius);
	}

	VOID WINAPI LutRotatePointAround (PFPOINT lpPoint, PFPOINT lpCenter, FLOAT fDegrees)
	{
		lpPoint->x -= lpCenter->x;
		lpPoint->y -= lpCenter->y;
		lpPoint->z -= lpCenter->z;
		LutRotatePoint(lpPoint, fDegrees);
		lpPoint->x += lpCenter->x;
		lpPoint->y += lpCenter->y;
		lpPoint->z += lpCenter->z;
	}

	FLOAT WINAPI PointDistance (PFPOINT lpPoint1, PFPOINT lpPoint2)
	{
#ifdef	_USE_GEOMETRY_SSE
		__m128 Diff = _mm_sub_ps(lpPoint1->m, lpPoint2->m);
		return _mm_cvtss_f32(_mm_sqrt_ss(_mm_dp_ps(Diff, Diff, 0x71)));
#else
		FLOAT xDiff = lpPoint2->x - lpPoint1->x;
		FLOAT yDiff = lpPoint2->y - lpPoint1->y;
		FLOAT zDiff = lpPoint2->z - lpPoint1->z;
		return sqrtf(xDiff * xDiff + yDiff * yDiff + zDiff * zDiff);
#endif
	}

	DOUBLE WINAPI PointDistanceD (PDPOINT lpPoint1, PDPOINT lpPoint2)
	{
		DOUBLE xDiff = lpPoint2->x - lpPoint1->x;
		DOUBLE yDiff = lpPoint2->y - lpPoint1->y;
		DOUBLE zDiff = lpPoint2->z - lpPoint1->z;
		return sqrt(xDiff * xDiff + yDiff * yDiff + zDiff * zDiff);
	}

	BOOL WINAPI SameSide (PFPOINT lpP1, PFPOINT lpP2, PFPOINT lpA, PFPOINT lpB)
	{
		FPOINT v1, v2;
		FPOINT ba, p1a, p2a;

		ba.x = lpB->x - lpA->x;
		ba.y = lpB->y - lpA->y;
		ba.z = lpB->z - lpA->z;
		p1a.x = lpP1->x - lpA->x;
		p1a.y = lpP1->y - lpA->y;
		p1a.z = lpP1->z - lpA->z;
		p2a.x = lpP2->x - lpA->x;
		p2a.y = lpP2->y - lpA->y;
		p2a.z = lpP2->z - lpA->z;

		// v1 = CrossProduct(ba,p1a);
		v1.x = (ba.y * p1a.z) - (ba.z * p1a.y);
		v1.y = (ba.z * p1a.x) - (ba.x * p1a.z);
		v1.z = (ba.x * p1a.y) - (ba.y * p1a.x);

		// v2 = CrossProduct(ba,p2a);
		v2.x = (ba.y * p2a.z) - (ba.z * p2a.y);
		v2.y = (ba.z * p2a.x) - (ba.x * p2a.z);
		v2.z = (ba.x * p2a.y) - (ba.y * p2a.x);

		// if(v1.DotProduct(v2) >= 0)
		return ((v1.x * v2.x) + (v1.y * v2.y) + (v1.z * v2.z)) >= 0;
	}

	BOOL WINAPI PointInTriangleOld (PFPOINT lpTest, PFPOINT lpTriangle)
	{
		if(SameSide(lpTest,lpTriangle,lpTriangle+1,lpTriangle+2) &&
			SameSide(lpTest,lpTriangle+1,lpTriangle+2,lpTriangle) &&
			SameSide(lpTest,lpTriangle+2,lpTriangle,lpTriangle+1))
			return TRUE;
		return FALSE;
	}

	BOOL WINAPI PointInTriangleOld (PFPOINT lpTest, PFPOINT lpT1, PFPOINT lpT2, PFPOINT lpT3)
	{
		if(SameSide(lpTest,lpT1,lpT2,lpT3) &&
			SameSide(lpTest,lpT2,lpT3,lpT1) &&
			SameSide(lpTest,lpT3,lpT1,lpT2))
			return TRUE;
		return FALSE;
	}

	BOOL WINAPI PointInTriangleNew (const FPOINT* pcfptTest, PFPOINT lpT1, PFPOINT lpT2, PFPOINT lpT3)
	{
		FLOAT total_angles = 0.0f;
		FPOINT v1, v2, v3;

		v1.x = pcfptTest->x - lpT1->x;
		v1.y = pcfptTest->y - lpT1->y;
		v1.z = pcfptTest->z - lpT1->z;
		Normalize(&v1);
		v2.x = pcfptTest->x - lpT2->x;
		v2.y = pcfptTest->y - lpT2->y;
		v2.z = pcfptTest->z - lpT2->z;
		Normalize(&v2);
		v3.x = pcfptTest->x - lpT3->x;
		v3.y = pcfptTest->y - lpT3->y;
		v3.z = pcfptTest->z - lpT3->z;
		Normalize(&v3);

		total_angles += (FLOAT)acos(DotProduct(&v1,&v2));
		total_angles += (FLOAT)acos(DotProduct(&v2,&v3));
		total_angles += (FLOAT)acos(DotProduct(&v3,&v1));

		return (fabs(total_angles - 2.0f * rPI) <= 0.005) && (1 == _finite(total_angles));
	}

	BOOL WINAPI PointInSquare (DOUBLE x, DOUBLE y, DOUBLE xSquare, DOUBLE ySquare, DOUBLE dblSize)
	{
		return x >= xSquare - dblSize && y >= ySquare - dblSize && x < xSquare + dblSize && y < ySquare + dblSize;
	}

	INT WINAPI PointInArray (const FPOINT* pcfpt, __in_ecount(cList) const FPOINT* pcrgList, INT cList)
	{
		for(INT i = 0; i < cList; i++)
		{
			const FPOINT* pcfptVertex = pcrgList + i;
			if(fabs(pcfpt->x - pcfptVertex->x) <= 0.005 && fabs(pcfpt->y - pcfptVertex->y) <= 0.005 && fabs(pcfpt->z - pcfptVertex->z) <= 0.005)
				return i;
		}

		return -1;
	}

	VOID WINAPI RotatePoint (PFPOINT lpPoint, FLOAT fDegrees)
	{
		DOUBLE fRadius = sqrt(lpPoint->x * lpPoint->x + lpPoint->z * lpPoint->z);
		DOUBLE fAngle = VertexAngle(lpPoint->x,lpPoint->z);
		lpPoint->x = (FLOAT)(cos(TDegreesToRadians<DOUBLE>(fAngle + fDegrees)) * fRadius);
		lpPoint->z = (FLOAT)(sin(TDegreesToRadians<DOUBLE>(fAngle + fDegrees)) * fRadius);
	}

	VOID WINAPI RotatePointAround (PFPOINT lpPoint, PFPOINT lpCenter, FLOAT fDegrees)
	{
		lpPoint->x -= lpCenter->x;
		lpPoint->y -= lpCenter->y;
		lpPoint->z -= lpCenter->z;
		RotatePoint(lpPoint, fDegrees);
		lpPoint->x += lpCenter->x;
		lpPoint->y += lpCenter->y;
		lpPoint->z += lpCenter->z;
	}

	DOUBLE WINAPI VertexAngle (DOUBLE x, DOUBLE y)
	{
		DOUBLE fAngle = 0.0;
		INT iQuadrant = PointQuadrant(x,y);
		DOUBLE fSide = sqrt(x * x + y * y);
		if(iQuadrant == 1)
		{
			if(fSide == 0.0)
				fAngle = 0.0;
			else
				fAngle = asin(y / fSide) * 180.0 / dPI;
		}
		else if(iQuadrant == 2)
		{
			if(fSide == 0.0)
				fAngle = 90.0;
			else
				fAngle = asin(fabs(x) / fSide) * 180.0 / dPI + 90.0;
		}
		else if(iQuadrant == 3)
		{
			if(fSide == 0.0)
				fAngle = 180.0;
			else
				fAngle = asin(fabs(y) / fSide) * 180.0 / dPI + 180.0;
		}
		else
		{
			if(fSide == 0.0)
				fAngle = 270.0;
			else
				fAngle = asin(x / fSide) * 180.0 / dPI + 270.0;
		}
		return fAngle;
	}

	INT WINAPI PointQuadrant (DOUBLE x, DOUBLE y)
	{
		INT iQuadrant;
		if(x >= 0.0)
		{
			if(y >= 0.0)
				iQuadrant = 1;
			else
				iQuadrant = 4;
		}
		else
		{
			if(y >= 0.0)
				iQuadrant = 2;
			else
				iQuadrant = 3;
		}
		return iQuadrant;
	}

	VOID WINAPI Normalize (PFPOINT lpNormalized, PFPOINT lpVector)
	{
#ifdef	_USE_GEOMETRY_SSE
		__m128 norm = _mm_sqrt_ps(_mm_dp_ps(lpVector->m, lpVector->m, 0x7F));
		lpNormalized->m = _mm_div_ps(lpVector->m, norm);
#else
		FLOAT x = lpVector->x, y = lpVector->y, z = lpVector->z;
		FLOAT f = 1.0f / sqrtf((x * x) + (y * y) + (z * z));
		lpNormalized->x = x * f;
		lpNormalized->y = y * f;
		lpNormalized->z = z * f;
#endif
	}

	VOID WINAPI Normalize (PFPOINT lpVector)
	{
#ifdef	_USE_GEOMETRY_SSE
		__m128 norm = _mm_sqrt_ps(_mm_dp_ps(lpVector->m, lpVector->m, 0x7F));
		lpVector->m = _mm_div_ps(lpVector->m, norm);
#else
		FLOAT x = lpVector->x, y = lpVector->y, z = lpVector->z;
		FLOAT f = 1.0f / sqrtf((x * x) + (y * y) + (z * z));
		lpVector->x *= f;
		lpVector->y *= f;
		lpVector->z *= f;
#endif
	}

	FLOAT WINAPI VectorLength (PFPOINT lpVector)
	{
#ifdef	_USE_GEOMETRY_SSE
		return _mm_cvtss_f32(_mm_sqrt_ss(_mm_dp_ps(lpVector->m, lpVector->m, 0x71)));
#else
		FLOAT x = lpVector->x, y = lpVector->y, z = lpVector->z;
		return sqrtf((x * x) + (y * y) + (z * z));
#endif
	}

	FLOAT WINAPI DotProduct (PFPOINT lpPoint1, PFPOINT lpPoint2)
	{
#ifdef	_USE_GEOMETRY_SSE
		return _mm_cvtss_f32(_mm_dp_ps(lpPoint1->m, lpPoint2->m, 0x71));
#else
		return (lpPoint1->x * lpPoint2->x) + (lpPoint1->y * lpPoint2->y) + (lpPoint1->z * lpPoint2->z);
#endif
	}

	FLOAT WINAPI DotProduct (PFPOINT lpPoint)
	{
#ifdef	_USE_GEOMETRY_SSE
		return _mm_cvtss_f32(_mm_dp_ps(lpPoint->m, lpPoint->m, 0x71));
#else
		return (lpPoint->x * lpPoint->x) + (lpPoint->y * lpPoint->y) + (lpPoint->z * lpPoint->z);
#endif
	}

	VOID WINAPI CrossProduct (PFPOINT lpProduct, PFPOINT lpPoint1, PFPOINT lpPoint2)
	{
#ifdef	_USE_GEOMETRY_SSE
		lpProduct->m = _mm_sub_ps(
			_mm_mul_ps(
				_mm_shuffle_ps(lpPoint1->m, lpPoint1->m, _MM_SHUFFLE(3, 0, 2, 1)),
				_mm_shuffle_ps(lpPoint2->m, lpPoint2->m, _MM_SHUFFLE(3, 1, 0, 2))
			),
			_mm_mul_ps(
				_mm_shuffle_ps(lpPoint1->m, lpPoint1->m, _MM_SHUFFLE(3, 1, 0, 2)),
				_mm_shuffle_ps(lpPoint2->m, lpPoint2->m, _MM_SHUFFLE(3, 0, 2, 1))
			));
#else
		lpProduct->x = (lpPoint1->y * lpPoint2->z) - (lpPoint1->z * lpPoint2->y);
		lpProduct->y = (lpPoint1->z * lpPoint2->x) - (lpPoint1->x * lpPoint2->z);
		lpProduct->z = (lpPoint1->x * lpPoint2->y) - (lpPoint1->y * lpPoint2->x);
#endif
	}

	VOID WINAPI GetPlaneNormal (const FPOINT& A, const FPOINT& B, const FPOINT& C, __out PFPOINT pN)
	{
		FPOINT diffCA, diffBA;

		diffCA.x = C.x - A.x;
		diffCA.y = C.y - A.y;
		diffCA.z = C.z - A.z;

		diffBA.x = B.x - A.x;
		diffBA.y = B.y - A.y;
		diffBA.z = B.z - A.z;

		CrossProduct(pN, &diffCA, &diffBA);
		Normalize(pN);
	}

	VOID WINAPI CalculateSpherePoint (DOUBLE r, DOUBLE degS, DOUBLE degT, __out FPOINT& fpt)
	{
		DOUBLE s = TDegreesToRadians<DOUBLE, DOUBLE>(degS);
		DOUBLE t = TDegreesToRadians<DOUBLE, DOUBLE>(degT);

		fpt.x = (FLOAT)(r * cos(s) * sin(t));
		fpt.y = (FLOAT)(r * sin(s) * sin(t));
		fpt.z = (FLOAT)(r * cos(t));
	}

	BOOL WINAPI GetLineIntersectionF (FLOAT p0_x, FLOAT p0_y, FLOAT p1_x, FLOAT p1_y,
		FLOAT p2_x, FLOAT p2_y, FLOAT p3_x, FLOAT p3_y, FLOAT* i_x, FLOAT* i_y)
	{
		FLOAT s1_x, s1_y, s2_x, s2_y;
		s1_x = p1_x - p0_x;     s1_y = p1_y - p0_y;
		s2_x = p3_x - p2_x;     s2_y = p3_y - p2_y;

		FLOAT s, t;
		s = (-s1_y * (p0_x - p2_x) + s1_x * (p0_y - p2_y)) / (-s2_x * s1_y + s1_x * s2_y);
		t = ( s2_x * (p0_y - p2_y) - s2_y * (p0_x - p2_x)) / (-s2_x * s1_y + s1_x * s2_y);

		if(s >= 0 && s <= 1 && t >= 0 && t <= 1)
		{
			// Collision detected
			*i_x = p0_x + (t * s1_x);
			*i_y = p0_y + (t * s1_y);
			return TRUE;
		}

		return FALSE; // No collision
	}

	BOOL WINAPI GetLineIntersectionD (DOUBLE p0_x, DOUBLE p0_y, DOUBLE p1_x, DOUBLE p1_y,
		DOUBLE p2_x, DOUBLE p2_y, DOUBLE p3_x, DOUBLE p3_y, DOUBLE* i_x, DOUBLE* i_y)
	{
		DOUBLE s1_x, s1_y, s2_x, s2_y;
		s1_x = p1_x - p0_x;     s1_y = p1_y - p0_y;
		s2_x = p3_x - p2_x;     s2_y = p3_y - p2_y;

		DOUBLE s, t;
		s = (-s1_y * (p0_x - p2_x) + s1_x * (p0_y - p2_y)) / (-s2_x * s1_y + s1_x * s2_y);
		t = ( s2_x * (p0_y - p2_y) - s2_y * (p0_x - p2_x)) / (-s2_x * s1_y + s1_x * s2_y);

		if(s >= 0 && s <= 1 && t >= 0 && t <= 1)
		{
			// Collision detected
			*i_x = p0_x + (t * s1_x);
			*i_y = p0_y + (t * s1_y);
			return TRUE;
		}

		return FALSE; // No collision
	}

	BOOL WINAPI LineSegmentIntersectsCircle (DOUBLE x1, DOUBLE y1, DOUBLE x2, DOUBLE y2, DOUBLE cx, DOUBLE cy, DOUBLE r)
	{
		DOUBLE x_linear = x2 - x1;
		DOUBLE x_constant = x1 - cx;
		DOUBLE y_linear = y2 - y1;
		DOUBLE y_constant = y1 - cy;
		DOUBLE a = x_linear * x_linear + y_linear * y_linear;
		DOUBLE half_b = x_linear * x_constant + y_linear * y_constant;
		DOUBLE c = x_constant * x_constant + y_constant * y_constant - r * r;
		return half_b * half_b >= a * c &&
			(-half_b <= a || c + half_b + half_b + a <= 0) &&
			(half_b <= 0 || c <= 0);
	}

	VOID WINAPI FindNearestPointOnLineSegment (DOUBLE x1, DOUBLE y1, DOUBLE x2, DOUBLE y2, DOUBLE cx, DOUBLE cy, __out DOUBLE* px, __out DOUBLE* py)
	{
		DOUBLE xHeading = x2 - x1, yHeading = y2 - y1;
		DOUBLE rMagnitude = sqrt((xHeading * xHeading) + (yHeading * yHeading));
		DOUBLE rScale = 1.0 / rMagnitude;
		xHeading *= rScale;
		yHeading *= rScale;

		DOUBLE xProjection = cx - x1, yProjection = cy - y1;
		DOUBLE rDotProduct = (xProjection * xHeading) + (yProjection * yHeading);

		if(rDotProduct < 0.0)
			rDotProduct = 0.0;
		else if(rDotProduct > rMagnitude)
			rDotProduct = rMagnitude;

		*px = x1 + xHeading * rDotProduct;
		*py = y1 + yHeading * rDotProduct;
	}

	BOOL WINAPI IsDblRectEmpty (const DBLRECT* pdblRect)
	{
		return NULL == pdblRect || (pdblRect->left >= pdblRect->right) || (pdblRect->top >= pdblRect->bottom);
	}

	BOOL WINAPI IntersectDblRect (__out PDBLRECT pdblRect, const DBLRECT* pdblSrc1, const DBLRECT* pdblSrc2)
	{
		if(NULL == pdblRect)
			return FALSE;

		if(IsDblRectEmpty(pdblSrc1) || IsDblRectEmpty(pdblSrc2) ||
			pdblSrc1->left >= pdblSrc2->right ||
			pdblSrc2->left >= pdblSrc1->right ||
			pdblSrc1->top >= pdblSrc2->bottom ||
			pdblSrc2->top >= pdblSrc1->bottom)
		{
			pdblRect->left = 0.0;
			pdblRect->top = 0.0;
			pdblRect->right = 0.0;
			pdblRect->bottom = 0.0;
			return FALSE;
		}

		pdblRect->left = max(pdblSrc1->left, pdblSrc2->left);
		pdblRect->right = min(pdblSrc1->right, pdblSrc2->right);
		pdblRect->top = max(pdblSrc1->top, pdblSrc2->top);
		pdblRect->bottom = min(pdblSrc1->bottom, pdblSrc2->bottom);

		return TRUE;
	}
};
