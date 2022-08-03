#include <math.h>
#include <windows.h>
#include <gl\gl.h>
#include "Camera.h"

CCamera::CCamera (const CCamera& cameraSource)
{
	// No need to call Init() - everything's being explicitly copied.

	m_bFixedLook = cameraSource.m_bFixedLook;
	m_rLook = cameraSource.m_rLook;
	m_rDirection = cameraSource.m_rDirection;
	m_dblDirRadians = cameraSource.m_dblDirRadians;

	m_dblPoint = cameraSource.m_dblPoint;
}

CCamera::CCamera (DOUBLE x, DOUBLE y, DOUBLE z)
{
	Init();
	m_dblPoint.x = x;
	m_dblPoint.y = y;
	m_dblPoint.z = z;
}

CCamera::CCamera ()
{
	Init();
	m_dblPoint.x = 0;
	m_dblPoint.y = 0;
	m_dblPoint.z = 0;
}

CCamera::~CCamera ()
{
}

VOID CCamera::SetPosition (DOUBLE xObj, DOUBLE yObj, DOUBLE zObj)
{
	m_dblPoint.x = xObj;
	m_dblPoint.y = yObj;
	m_dblPoint.z = zObj;
}

VOID CCamera::GetPosition (DOUBLE& xObj, DOUBLE& yObj, DOUBLE& zObj)
{
	xObj = m_dblPoint.x;
	yObj = m_dblPoint.y;
	zObj = m_dblPoint.z;
}

VOID CCamera::SetPerspective (VOID)
{
	glRotatef(-m_rLook,1,0,0);
	glRotatef(m_rDirection,0,1,0);
	glTranslated(-m_dblPoint.x, -m_dblPoint.y, -m_dblPoint.z);
}

VOID CCamera::SetBillboardMode (VOID)
{
	glRotatef(-m_rDirection,0,1,0);
	glRotatef(m_rLook,1,0,0);
}

VOID CCamera::LookAt (DOUBLE x, DOUBLE y, DOUBLE z)
{
	FLOAT fSide;
	DOUBLE xDiff = m_dblPoint.x - x;
	DOUBLE yDiff = m_dblPoint.y - y;
	DOUBLE zDiff = m_dblPoint.z - z;

	m_rDirection = (FLOAT)Geometry::VertexAngle(xDiff, zDiff) - 90;
	if(m_rDirection < 0)
		m_rDirection += 360;
	m_dblDirRadians = Geometry::TDegreesToRadians<DOUBLE>(m_rDirection);

	fSide = (FLOAT)sqrt(xDiff * xDiff + zDiff * zDiff);
	m_rLook = -(FLOAT)Geometry::VertexAngle(fSide,yDiff);
}

FLOAT CCamera::Look (FLOAT fAddAngle)
{
	m_rLook += fAddAngle;
	if(m_bFixedLook)
	{
		if(m_rLook >= 85.0f)
			m_rLook = 85.0f;
		else if(m_rLook <= -85.0f)
			m_rLook = -85.0f;
	}
	return m_rLook;
}

FLOAT CCamera::Turn (FLOAT fAddAngle)
{
	m_rDirection += fAddAngle;
	if(m_rDirection >= 360)
		m_rDirection -= 360;
	else if(m_rDirection < 0)
		m_rDirection += 360;
	m_dblDirRadians = Geometry::TDegreesToRadians<DOUBLE>(m_rDirection);
	return m_rDirection;
}

VOID CCamera::MoveForward (FLOAT rVelocity)
{
	m_dblPoint.x += sin(m_dblDirRadians) * rVelocity;
	m_dblPoint.z -= cos(m_dblDirRadians) * rVelocity;
}

VOID CCamera::MoveBackward (FLOAT rVelocity)
{
	m_dblPoint.x -= sin(m_dblDirRadians) * rVelocity;
	m_dblPoint.z += cos(m_dblDirRadians) * rVelocity;
}

VOID CCamera::Raise (FLOAT fAddPosition)
{
	m_dblPoint.y += fAddPosition;
}

//

VOID CCamera::Init (VOID)
{
	m_bFixedLook = FALSE;
	m_rLook = 0.0f;
	m_rDirection = 0.0f;
	m_dblDirRadians = 0.0;
}
