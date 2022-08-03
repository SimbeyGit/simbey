#ifndef	_H_FRUSTUM
#define	_H_FRUSTUM

#define	FR_RIGHT		0
#define	FR_LEFT			1
#define	FR_BOTTOM		2
#define	FR_TOP			3
#define	FR_FAR			4
#define	FR_NEAR			5

class CFrustum
{
private:
	FLOAT m_Frustum[6][4];

public:
	CFrustum ();
	~CFrustum ();

	VOID Update (VOID);
	VOID UpdateFast (VOID);

	BOOL PointInFrustum (FLOAT x, FLOAT y, FLOAT z);
	BOOL SphereInFrustum (FLOAT x, FLOAT y, FLOAT z, FLOAT radius);
	BOOL CubeInFrustum (FLOAT x, FLOAT y, FLOAT z, FLOAT size);
	BOOL RectInFrustum (FLOAT x, FLOAT y, FLOAT z, FLOAT width, FLOAT height, FLOAT length);

	BOOL SphereInFrustumDistance (FLOAT x, FLOAT y, FLOAT z, FLOAT radius, __out FLOAT& rDistance);

	FLOAT Get (INT nPlane, INT nValue);

protected:
	VOID CalculateFrustumSides (FLOAT* Clipping);
	VOID NormalizePlane (FLOAT* fPlane);
};

#endif
