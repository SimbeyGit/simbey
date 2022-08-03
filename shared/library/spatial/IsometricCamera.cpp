#include <math.h>
#include <windows.h>
#include <gl\gl.h>
#include "IsometricCamera.h"

CIsometricCamera::CIsometricCamera (const CIsometricCamera& cameraSource)
{
	m_yRise = cameraSource.m_yRise;
	m_zRun = cameraSource.m_zRun;
	m_rLook = cameraSource.m_rLook;
	m_rDirection = cameraSource.m_rDirection;
	m_rTurnRate = 1.0f;
	CopyMemory(&m_dblPoint, &cameraSource.m_dblPoint, sizeof(DPOINT));
	m_dblDirRadians = cameraSource.m_dblDirRadians;
}

CIsometricCamera::CIsometricCamera ()
{
	m_yRise = 0.0f;
	m_zRun = 0.0f;
	m_rLook = 0.0f;
	m_rDirection = 0.0f;
	m_rTurnRate = 1.0f;
	ZeroMemory(&m_dblPoint, sizeof(DPOINT));
	m_dblDirRadians = 0.0;
}

CIsometricCamera::~CIsometricCamera ()
{
}

VOID CIsometricCamera::SetPosition (DOUBLE xObj, DOUBLE yObj, DOUBLE zObj)
{
	m_dblPoint.x = xObj;
	m_dblPoint.y = yObj;
	m_dblPoint.z = zObj;
}

VOID CIsometricCamera::GetPosition (DOUBLE& xObj, DOUBLE& yObj, DOUBLE& zObj)
{
	xObj = m_dblPoint.x;
	yObj = m_dblPoint.y;
	zObj = m_dblPoint.z;
}

VOID CIsometricCamera::SetPerspective (VOID)
{
	DOUBLE dblNegDirRadians = -m_dblDirRadians;
	DOUBLE dblRun = (DOUBLE)m_zRun;

	glRotatef(m_rLook, 1.0f, 0.0f, 0.0f);
	glRotatef(m_rDirection, 0.0f, 1.0f, 0.0f);

	FLOAT xCameraOffset = (FLOAT)(sin(dblNegDirRadians) * dblRun);
	FLOAT zCameraOffset = (FLOAT)(cos(dblNegDirRadians) * dblRun);
	glTranslated(-m_dblPoint.x - xCameraOffset, -(m_dblPoint.y + m_yRise), -m_dblPoint.z - zCameraOffset);
}

VOID CIsometricCamera::SetBillboardMode (VOID)
{
	glRotatef(-m_rDirection, 0.0f, 1.0f, 0.0f);
	glRotatef(-m_rLook, 1.0f, 0.0f, 0.0f);
}

VOID CIsometricCamera::SetOffset (FLOAT yRise, FLOAT zRun)
{
	m_yRise = yRise;
	m_zRun = zRun;
	if(0.0f == m_zRun)
		m_rLook = 90.0f;
	else
		m_rLook = Geometry::RadiansDblToDegreesFlt(atan((DOUBLE)m_yRise / (DOUBLE)m_zRun));
}

VOID CIsometricCamera::SetOffsetByAngle (FLOAT rDegrees, FLOAT rDistance)
{
	if(0.0f == rDegrees)
	{
		m_yRise = 0.0f;
		m_zRun = rDistance;
	}
	else if(90.0f == rDegrees)
	{
		m_yRise = rDistance;
		m_zRun = 0.0f;
	}
	else
	{
		FLOAT rRadians = Geometry::TDegreesToRadians<FLOAT, FLOAT>(rDegrees);
		m_yRise = sin(rRadians) * rDistance;
		m_zRun = cos(rRadians) * rDistance;
	}

	m_rLook = rDegrees;
}

VOID CIsometricCamera::GetOffset (FLOAT& yRise, FLOAT& zRun)
{
	yRise = m_yRise;
	zRun = m_zRun;
}

FLOAT CIsometricCamera::GetLookDirection (VOID)
{
	return m_rLook;
}

VOID CIsometricCamera::SetTurnRate (FLOAT rRate)
{
	m_rTurnRate = rRate;
}

FLOAT CIsometricCamera::GetTurnRate (VOID)
{
	return m_rTurnRate;
}

VOID CIsometricCamera::TurnLeft (VOID)
{
	SetTurnAngle(m_rDirection - m_rTurnRate);
}

VOID CIsometricCamera::TurnRight (VOID)
{
	SetTurnAngle(m_rDirection + m_rTurnRate);
}

VOID CIsometricCamera::SetTurnAngle (FLOAT rAngle)
{
	if(rAngle < 0.0f)
		rAngle += 360.0f;
	else if(rAngle >= 360.0f)
		rAngle -= 360.0f;
	m_rDirection = rAngle;
	m_dblDirRadians = Geometry::TDegreesToRadians<DOUBLE, FLOAT>(rAngle);
}

FLOAT CIsometricCamera::GetTurnAngle (VOID)
{
	return m_rDirection;
}

VOID CIsometricCamera::MoveForward (FLOAT rVelocity)
{
	m_dblPoint.x += sin(m_dblDirRadians) * rVelocity;
	m_dblPoint.z -= cos(m_dblDirRadians) * rVelocity;
}

VOID CIsometricCamera::MoveBackward (FLOAT rVelocity)
{
	m_dblPoint.x -= sin(m_dblDirRadians) * rVelocity;
	m_dblPoint.z += cos(m_dblDirRadians) * rVelocity;
}

VOID CIsometricCamera::Raise (FLOAT fAddPosition)
{
	m_dblPoint.y += fAddPosition;
}

VOID CIsometricCamera::GetLookAtPoint (FLOAT rDistance, __out DPOINT& dblLookAt)
{
	DOUBLE dblAngleX = Geometry::TDegreesToRadians<DOUBLE, FLOAT>(m_rLook);
	DOUBLE dblAngleY = -m_dblDirRadians;
	DOUBLE dblRun = (DOUBLE)m_zRun;

	DOUBLE dblCosAngleX = cos(dblAngleX);
	DOUBLE dblSinAngleY = sin(dblAngleY);
	DOUBLE dblCosAngleY = cos(dblAngleY);

	FLOAT xCameraOffset = (FLOAT)(dblSinAngleY * dblRun);
	FLOAT zCameraOffset = (FLOAT)(dblCosAngleY * dblRun);

	dblLookAt.x = m_dblPoint.x + xCameraOffset;
	dblLookAt.y = m_dblPoint.y + m_yRise;
	dblLookAt.z = m_dblPoint.z + zCameraOffset;

	dblLookAt.x -= rDistance * dblCosAngleX * dblSinAngleY;
	dblLookAt.y -= rDistance * sin(dblAngleX);
	dblLookAt.z -= rDistance * dblCosAngleX * dblCosAngleY;
}
