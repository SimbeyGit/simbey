#ifndef	_H_GEOMETRY
#define	_H_GEOMETRY

#include "GeometryTypes.h"

namespace Geometry
{
	extern DOUBLE dPI;
	extern FLOAT rPI;

	extern DOUBLE dE;
	extern FLOAT rE;

	extern DOUBLE dRadians;
	extern FLOAT rRadians;

	extern const DOUBLE SQRT_2;	// Square root of 2
	extern const DOUBLE SQRT_3;	// Square root of 3

	extern FLOAT lutSin[360 * 10];
	extern FLOAT lutCos[360 * 10];

	template <typename T>
	inline T TRadiansConversion (VOID)
	{
		return (T)0;	// This must not be used!
	}
	template <>
	inline DOUBLE TRadiansConversion<DOUBLE> (VOID)
	{
		return dRadians;
	}
	template <>
	inline FLOAT TRadiansConversion<FLOAT> (VOID)
	{
		return rRadians;
	}

	template <typename TReturn, typename TDegrees>
	inline TReturn TDegreesToRadians (TDegrees cDegrees)
	{
		return (TReturn)cDegrees * TRadiansConversion<TReturn>();
	}

	inline INT RadiansDblToDegrees (DOUBLE cRadians)
	{
		return (INT)((cRadians * 180.0) / dPI);
	}

	inline INT RadiansFltToDegrees (FLOAT cRadians)
	{
		return (INT)((cRadians * 180.0f) / rPI);
	}

	inline FLOAT RadiansDblToDegreesFlt (DOUBLE cRadians)
	{
		return (FLOAT)((cRadians * 180.0f) / dPI);
	}

	inline FLOAT LutSin (DOUBLE rDegrees)
	{
		INT nDegreesIndex = (INT)(rDegrees * 10.0) % ARRAYSIZE(lutSin);
		return 0 <= nDegreesIndex ? lutSin[nDegreesIndex] : lutSin[nDegreesIndex + ARRAYSIZE(lutSin)];
	}

	inline FLOAT LutCos (DOUBLE rDegrees)
	{
		INT nDegreesIndex = (INT)(rDegrees * 10.0) % ARRAYSIZE(lutCos);
		return 0 <= nDegreesIndex ? lutCos[nDegreesIndex] : lutCos[nDegreesIndex + ARRAYSIZE(lutCos)];
	}

	VOID WINAPI InitializeTables (VOID);
	VOID WINAPI LutRotatePoint (PFPOINT lpPoint, FLOAT fDegrees);
	VOID WINAPI LutRotatePointAround (PFPOINT lpPoint, PFPOINT lpCenter, FLOAT fDegrees);

	FLOAT WINAPI PointDistance (PFPOINT lpPoint1, PFPOINT lpPoint2);
	DOUBLE WINAPI PointDistanceD (PDPOINT lpPoint1, PDPOINT lpPoint2);
	BOOL WINAPI SameSide (PFPOINT lpP1, PFPOINT lpP2, PFPOINT lpA, PFPOINT lpB);
	BOOL WINAPI PointInTriangleOld (PFPOINT lpTest, PFPOINT lpTriangle);
	BOOL WINAPI PointInTriangleOld (PFPOINT lpTest, PFPOINT lpT1, PFPOINT lpT2, PFPOINT lpT3);
	BOOL WINAPI PointInTriangleNew (PFPOINT lpTest, PFPOINT lpT1, PFPOINT lpT2, PFPOINT lpT3);

	VOID WINAPI RotatePoint (PFPOINT lpPoint, FLOAT fDegrees);
	VOID WINAPI RotatePointAround (PFPOINT lpPoint, PFPOINT lpCenter, FLOAT fDegrees);
	DOUBLE WINAPI VertexAngle (DOUBLE x, DOUBLE y);
	INT WINAPI PointQuadrant (DOUBLE x, DOUBLE y);

	VOID WINAPI Normalize (PFPOINT lpNormalized, PFPOINT lpVector);
	VOID WINAPI Normalize (PFPOINT lpVector);
	FLOAT WINAPI VectorLength (PFPOINT lpVector);
	FLOAT WINAPI DotProduct (PFPOINT lpPoint1, PFPOINT lpPoint2);
	FLOAT WINAPI DotProduct (PFPOINT lpPoint);
	VOID WINAPI CrossProduct (PFPOINT lpProduct, PFPOINT lpPoint1, PFPOINT lpPoint2);

	// Specify A, B, and C in clock-wise order.
	VOID WINAPI GetPlaneNormal (const FPOINT& A, const FPOINT& B, const FPOINT& C, __out PFPOINT pN);

	VOID WINAPI CalculateSpherePoint (DOUBLE r, DOUBLE degS, DOUBLE degT, __out FPOINT& fpt);

	BOOL WINAPI GetLineIntersectionF (FLOAT p0_x, FLOAT p0_y, FLOAT p1_x, FLOAT p1_y,
		FLOAT p2_x, FLOAT p2_y, FLOAT p3_x, FLOAT p3_y, FLOAT* i_x, FLOAT* i_y);
	BOOL WINAPI GetLineIntersectionD (DOUBLE p0_x, DOUBLE p0_y, DOUBLE p1_x, DOUBLE p1_y,
		DOUBLE p2_x, DOUBLE p2_y, DOUBLE p3_x, DOUBLE p3_y, DOUBLE* i_x, DOUBLE* i_y);
};

#endif
