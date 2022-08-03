// Reference: http://www.crownandcutlass.com/features/technicaldetails/frustum.html
#include <math.h>
#include <windows.h>
#include <gl\gl.h>
#include "Frustum.h"

CFrustum::CFrustum ()
{
}

CFrustum::~CFrustum ()
{
}

VOID CFrustum::Update (VOID)
{
	FLOAT Projection[16];
	FLOAT ModelView[16];
	FLOAT Clipping[16];

	glGetFloatv(GL_PROJECTION_MATRIX,Projection);
	glGetFloatv(GL_MODELVIEW_MATRIX,ModelView);

	// Now that we have our modelview and projection matrix,
	// if we combine these 2 matrices, it will give us our
	// clipping planes.  To combine 2 matrices, multiply them.
	Clipping[ 0] = ModelView[ 0] * Projection[ 0] + ModelView[ 1] * Projection[ 4] + ModelView[ 2] * Projection[ 8] + ModelView[ 3] * Projection[12];
	Clipping[ 1] = ModelView[ 0] * Projection[ 1] + ModelView[ 1] * Projection[ 5] + ModelView[ 2] * Projection[ 9] + ModelView[ 3] * Projection[13];
	Clipping[ 2] = ModelView[ 0] * Projection[ 2] + ModelView[ 1] * Projection[ 6] + ModelView[ 2] * Projection[10] + ModelView[ 3] * Projection[14];
	Clipping[ 3] = ModelView[ 0] * Projection[ 3] + ModelView[ 1] * Projection[ 7] + ModelView[ 2] * Projection[11] + ModelView[ 3] * Projection[15];

	Clipping[ 4] = ModelView[ 4] * Projection[ 0] + ModelView[ 5] * Projection[ 4] + ModelView[ 6] * Projection[ 8] + ModelView[ 7] * Projection[12];
	Clipping[ 5] = ModelView[ 4] * Projection[ 1] + ModelView[ 5] * Projection[ 5] + ModelView[ 6] * Projection[ 9] + ModelView[ 7] * Projection[13];
	Clipping[ 6] = ModelView[ 4] * Projection[ 2] + ModelView[ 5] * Projection[ 6] + ModelView[ 6] * Projection[10] + ModelView[ 7] * Projection[14];
	Clipping[ 7] = ModelView[ 4] * Projection[ 3] + ModelView[ 5] * Projection[ 7] + ModelView[ 6] * Projection[11] + ModelView[ 7] * Projection[15];

	Clipping[ 8] = ModelView[ 8] * Projection[ 0] + ModelView[ 9] * Projection[ 4] + ModelView[10] * Projection[ 8] + ModelView[11] * Projection[12];
	Clipping[ 9] = ModelView[ 8] * Projection[ 1] + ModelView[ 9] * Projection[ 5] + ModelView[10] * Projection[ 9] + ModelView[11] * Projection[13];
	Clipping[10] = ModelView[ 8] * Projection[ 2] + ModelView[ 9] * Projection[ 6] + ModelView[10] * Projection[10] + ModelView[11] * Projection[14];
	Clipping[11] = ModelView[ 8] * Projection[ 3] + ModelView[ 9] * Projection[ 7] + ModelView[10] * Projection[11] + ModelView[11] * Projection[15];

	Clipping[12] = ModelView[12] * Projection[ 0] + ModelView[13] * Projection[ 4] + ModelView[14] * Projection[ 8] + ModelView[15] * Projection[12];
	Clipping[13] = ModelView[12] * Projection[ 1] + ModelView[13] * Projection[ 5] + ModelView[14] * Projection[ 9] + ModelView[15] * Projection[13];
	Clipping[14] = ModelView[12] * Projection[ 2] + ModelView[13] * Projection[ 6] + ModelView[14] * Projection[10] + ModelView[15] * Projection[14];
	Clipping[15] = ModelView[12] * Projection[ 3] + ModelView[13] * Projection[ 7] + ModelView[14] * Projection[11] + ModelView[15] * Projection[15];

	// Now we actually want to get the sides of the frustum.
	// To do this we take the clipping planes we received
	// above and extract the sides from them.
	CalculateFrustumSides(Clipping);
}

VOID CFrustum::UpdateFast (VOID)
{
	FLOAT Projection[16];
	FLOAT ModelView[16];
	FLOAT Clipping[16];

	glGetFloatv(GL_PROJECTION_MATRIX,Projection);
	glGetFloatv(GL_MODELVIEW_MATRIX,ModelView);

	// Combine The Two Matrices (Multiply Projection By Modelview) 
	// But Keep In Mind This Function Will Only Work If You Do NOT
	// Rotate Or Translate Your Projection Matrix
	Clipping[ 0] = ModelView[ 0] * Projection[ 0];
	Clipping[ 1] = ModelView[ 1] * Projection[ 5];
	Clipping[ 2] = ModelView[ 2] * Projection[10] + ModelView[ 3] * Projection[14];
	Clipping[ 3] = ModelView[ 2] * Projection[11];

	Clipping[ 4] = ModelView[ 4] * Projection[ 0];
	Clipping[ 5] = ModelView[ 5] * Projection[ 5];
	Clipping[ 6] = ModelView[ 6] * Projection[10] + ModelView[ 7] * Projection[14];
	Clipping[ 7] = ModelView[ 6] * Projection[11];

	Clipping[ 8] = ModelView[ 8] * Projection[ 0];
	Clipping[ 9] = ModelView[ 9] * Projection[ 5];
	Clipping[10] = ModelView[10] * Projection[10] + ModelView[11] * Projection[14];
	Clipping[11] = ModelView[10] * Projection[11];

	Clipping[12] = ModelView[12] * Projection[ 0];
	Clipping[13] = ModelView[13] * Projection[ 5];
	Clipping[14] = ModelView[14] * Projection[10] + ModelView[15] * Projection[14];
	Clipping[15] = ModelView[14] * Projection[11];

	// Now we actually want to get the sides of the frustum.
	// To do this we take the clipping planes we received
	// above and extract the sides from them.
	CalculateFrustumSides(Clipping);
}

BOOL CFrustum::PointInFrustum (FLOAT x, FLOAT y, FLOAT z)
{
	// If you remember the plane equation (A*x + B*y + C*z + D = 0), then the rest
	// of this code should be quite obvious and easy to figure out yourself.
	// In case don't know the plane equation, it might be a good idea to look
	// at our Plane Collision tutorial at www.GameTutorials.com in OpenGL Tutorials.
	// I will briefly go over it here.  (A,B,C) is the (X,Y,Z) of the normal to the plane.
	// They are the same thing... but just called ABC because you don't want to say:
	// (x*x + y*y + z*z + d = 0).  That would be wrong, so they substitute them.
	// the (x, y, z) in the equation is the point that you are testing.  The D is
	// The distance the plane is from the origin.  The equation ends with "= 0" because
	// that is true when the point (x, y, z) is ON the plane.  When the point is NOT on
	// the plane, it is either a negative number (the point is behind the plane) or a
	// positive number (the point is in front of the plane).  We want to check if the point
	// is in front of the plane, so all we have to do is go through each point and make
	// sure the plane equation goes out to a positive number on each side of the frustum.
	// The result (be it positive or negative) is the distance the point is front the plane.

	// Go through all the sides of the frustum
	for(INT i = 0; i < 6; i++)
	{
		// Calculate the plane equation and check if the point is behind a side of the frustum
		if(m_Frustum[i][0] * x + m_Frustum[i][1] * y + m_Frustum[i][2] * z + m_Frustum[i][3] <= 0)
		{
			// The point was behind a side, so it ISN'T in the frustum
			return FALSE;
		}
	}

	// The point was inside of the frustum (In front of ALL the sides of the frustum)
	return TRUE;
}

BOOL CFrustum::SphereInFrustum (FLOAT x, FLOAT y, FLOAT z, FLOAT radius)
{
	// Now this function is almost identical to the PointInFrustum(), except we
	// now have to deal with a radius around the point.  The point is the center of
	// the radius.  So, the point might be outside of the frustum, but it doesn't
	// mean that the rest of the sphere is.  It could be half and half.  So instead of
	// checking if it's less than 0, we need to add on the radius to that.  Say the
	// equation produced -2, which means the center of the sphere is the distance of
	// 2 behind the plane.  Well, what if the radius was 5?  The sphere is still inside,
	// so we would say, if(-2 < -5) then we are outside.  In that case it's false,
	// so we are inside of the frustum, but a distance of 3.  This is reflected below.

	// Go through all the sides of the frustum
	for(INT i = 0; i < 6; i++ )
	{
		// If the center of the sphere is farther away from the plane than the radius
		if(m_Frustum[i][0] * x + m_Frustum[i][1] * y + m_Frustum[i][2] * z + m_Frustum[i][3] <= -radius)
		{
			// The distance was greater than the radius so the sphere is outside of the frustum
			return FALSE;
		}
	}

	// The sphere was inside of the frustum!
	return TRUE;
}

BOOL CFrustum::CubeInFrustum (FLOAT x, FLOAT y, FLOAT z, FLOAT size)
{
	// This test is a bit more work, but not too much more complicated.
	// Basically, what is going on is, that we are given the center of the cube,
	// and half the length.  Think of it like a radius.  Then we checking each point
	// in the cube and seeing if it is inside the frustum.  If a point is found in front
	// of a side, then we skip to the next side.  If we get to a plane that does NOT have
	// a point in front of it, then it will return false.

	// *Note* - This will sometimes say that a cube is inside the frustum when it isn't.
	// This happens when all the corners of the bounding box are not behind any one plane.
	// This is rare and shouldn't affect the overall rendering speed.
	for(INT i = 0; i < 6; i++)
	{
		if(m_Frustum[i][0] * (x - size) + m_Frustum[i][1] * (y - size) + m_Frustum[i][2] * (z - size) + m_Frustum[i][3] > 0)
			continue;
		if(m_Frustum[i][0] * (x + size) + m_Frustum[i][1] * (y - size) + m_Frustum[i][2] * (z - size) + m_Frustum[i][3] > 0)
			continue;
		if(m_Frustum[i][0] * (x - size) + m_Frustum[i][1] * (y + size) + m_Frustum[i][2] * (z - size) + m_Frustum[i][3] > 0)
			continue;
		if(m_Frustum[i][0] * (x + size) + m_Frustum[i][1] * (y + size) + m_Frustum[i][2] * (z - size) + m_Frustum[i][3] > 0)
			continue;
		if(m_Frustum[i][0] * (x - size) + m_Frustum[i][1] * (y - size) + m_Frustum[i][2] * (z + size) + m_Frustum[i][3] > 0)
			continue;
		if(m_Frustum[i][0] * (x + size) + m_Frustum[i][1] * (y - size) + m_Frustum[i][2] * (z + size) + m_Frustum[i][3] > 0)
			continue;
		if(m_Frustum[i][0] * (x - size) + m_Frustum[i][1] * (y + size) + m_Frustum[i][2] * (z + size) + m_Frustum[i][3] > 0)
			continue;
		if(m_Frustum[i][0] * (x + size) + m_Frustum[i][1] * (y + size) + m_Frustum[i][2] * (z + size) + m_Frustum[i][3] > 0)
			continue;

		// If we get here, it isn't in the frustum
		return FALSE;
	}

	return TRUE;
}

BOOL CFrustum::RectInFrustum (FLOAT x, FLOAT y, FLOAT z, FLOAT width, FLOAT height, FLOAT length)
{
	// Like a cube but may have different width values
	INT i;
	for(i = 0; i < 6; i++)
	{
		if(m_Frustum[i][0] * (x - width) + m_Frustum[i][1] * (y - height) + m_Frustum[i][2] * (z - length) + m_Frustum[i][3] > 0)
			continue;
		if(m_Frustum[i][0] * (x + width) + m_Frustum[i][1] * (y - height) + m_Frustum[i][2] * (z - length) + m_Frustum[i][3] > 0)
			continue;
		if(m_Frustum[i][0] * (x - width) + m_Frustum[i][1] * (y + height) + m_Frustum[i][2] * (z - length) + m_Frustum[i][3] > 0)
			continue;
		if(m_Frustum[i][0] * (x + width) + m_Frustum[i][1] * (y + height) + m_Frustum[i][2] * (z - length) + m_Frustum[i][3] > 0)
			continue;
		if(m_Frustum[i][0] * (x - width) + m_Frustum[i][1] * (y - height) + m_Frustum[i][2] * (z + length) + m_Frustum[i][3] > 0)
			continue;
		if(m_Frustum[i][0] * (x + width) + m_Frustum[i][1] * (y - height) + m_Frustum[i][2] * (z + length) + m_Frustum[i][3] > 0)
			continue;
		if(m_Frustum[i][0] * (x - width) + m_Frustum[i][1] * (y + height) + m_Frustum[i][2] * (z + length) + m_Frustum[i][3] > 0)
			continue;
		if(m_Frustum[i][0] * (x + width) + m_Frustum[i][1] * (y + height) + m_Frustum[i][2] * (z + length) + m_Frustum[i][3] > 0)
			continue;

		// If we get here, it isn't in the frustum
		return FALSE;
	}

	return TRUE;
}

BOOL CFrustum::SphereInFrustumDistance (FLOAT x, FLOAT y, FLOAT z, FLOAT radius, __out FLOAT& rDistance)
{
	FLOAT d;

	// Go through all the sides of the frustum
	for(INT i = 0; i < 6; i++)
	{
		d = m_Frustum[i][0] * x + m_Frustum[i][1] * y + m_Frustum[i][2] * z + m_Frustum[i][3];
		if(d <= -radius)
			return FALSE;
	}

	// Return distance from NEAR plane.
	rDistance = d + radius;

	return TRUE;
}

FLOAT CFrustum::Get (INT nPlane, INT nValue)
{
	return m_Frustum[nPlane][nValue];
}

VOID CFrustum::CalculateFrustumSides (FLOAT* Clipping)
{
	// This will extract the RIGHT side of the frustum
	m_Frustum[FR_RIGHT][0] = Clipping[ 3] - Clipping[ 0];
	m_Frustum[FR_RIGHT][1] = Clipping[ 7] - Clipping[ 4];
	m_Frustum[FR_RIGHT][2] = Clipping[11] - Clipping[ 8];
	m_Frustum[FR_RIGHT][3] = Clipping[15] - Clipping[12];
	NormalizePlane(m_Frustum[FR_RIGHT]);

	// This will extract the LEFT side of the frustum
	m_Frustum[FR_LEFT][0] = Clipping[ 3] + Clipping[ 0];
	m_Frustum[FR_LEFT][1] = Clipping[ 7] + Clipping[ 4];
	m_Frustum[FR_LEFT][2] = Clipping[11] + Clipping[ 8];
	m_Frustum[FR_LEFT][3] = Clipping[15] + Clipping[12];
	NormalizePlane(m_Frustum[FR_LEFT]);

	// This will extract the BOTTOM side of the frustum
	m_Frustum[FR_BOTTOM][0] = Clipping[ 3] + Clipping[ 1];
	m_Frustum[FR_BOTTOM][1] = Clipping[ 7] + Clipping[ 5];
	m_Frustum[FR_BOTTOM][2] = Clipping[11] + Clipping[ 9];
	m_Frustum[FR_BOTTOM][3] = Clipping[15] + Clipping[13];
	NormalizePlane(m_Frustum[FR_BOTTOM]);

	// This will extract the TOP side of the frustum
	m_Frustum[FR_TOP][0] = Clipping[ 3] - Clipping[ 1];
	m_Frustum[FR_TOP][1] = Clipping[ 7] - Clipping[ 5];
	m_Frustum[FR_TOP][2] = Clipping[11] - Clipping[ 9];
	m_Frustum[FR_TOP][3] = Clipping[15] - Clipping[13];
	NormalizePlane(m_Frustum[FR_TOP]);

	// This will extract the FAR side of the frustum
	m_Frustum[FR_FAR][0] = Clipping[ 3] - Clipping[ 2];
	m_Frustum[FR_FAR][1] = Clipping[ 7] - Clipping[ 6];
	m_Frustum[FR_FAR][2] = Clipping[11] - Clipping[10];
	m_Frustum[FR_FAR][3] = Clipping[15] - Clipping[14];
	NormalizePlane(m_Frustum[FR_FAR]);

	// This will extract the NEAR side of the frustum
	m_Frustum[FR_NEAR][0] = Clipping[ 3] + Clipping[ 2];
	m_Frustum[FR_NEAR][1] = Clipping[ 7] + Clipping[ 6];
	m_Frustum[FR_NEAR][2] = Clipping[11] + Clipping[10];
	m_Frustum[FR_NEAR][3] = Clipping[15] + Clipping[14];
	NormalizePlane(m_Frustum[FR_NEAR]);
}

VOID CFrustum::NormalizePlane (FLOAT* fPlane)
{
	// Here we calculate the magnitude of the normal to the plane (point A B C)
	// Remember that (A, B, C) is that same thing as the normal's (X, Y, Z).
	// To calculate magnitude you use the equation:  magnitude = sqrt( x^2 + y^2 + z^2)
	FLOAT fMagnitude = (FLOAT)sqrt(fPlane[0] * fPlane[0] + 
		fPlane[1] * fPlane[1] + 
		fPlane[2] * fPlane[2]);

	// Then we divide the plane's values by its magnitude.
	// This makes it easier to work with.
	fPlane[0] /= fMagnitude;
	fPlane[1] /= fMagnitude;
	fPlane[2] /= fMagnitude;
	fPlane[3] /= fMagnitude; 
}
