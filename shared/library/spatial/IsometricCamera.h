#pragma once

#include "Geometry.h"

class CIsometricCamera
{
protected:
	DPOINT m_dblPoint;
	DOUBLE m_dblDirRadians;
	FLOAT m_rLook;
	FLOAT m_rDirection;
	FLOAT m_rTurnRate;

	FLOAT m_yRise, m_zRun;

public:
	CIsometricCamera (const CIsometricCamera& cameraSource);
	CIsometricCamera ();
	~CIsometricCamera ();

	VOID SetPosition (DOUBLE xObj, DOUBLE yObj, DOUBLE zObj);
	VOID GetPosition (DOUBLE& xObj, DOUBLE& yObj, DOUBLE& zObj);

	VOID SetPerspective (VOID);

	// For SetBillboardMode() to function correctly, the last translation must be the
	// translation to the desired 3D position in world space.  SetBillboardMode() will
	// undo the direction and look rotations as set by SetPerspective().  For the best
	// results, draw all billboards within separate model view stack frames.
	VOID SetBillboardMode (VOID);

	VOID SetOffset (FLOAT yRise, FLOAT zRun);
	VOID SetOffsetByAngle (FLOAT rDegrees, FLOAT rDistance);
	VOID GetOffset (FLOAT& yRise, FLOAT& zRun);
	FLOAT GetLookDirection (VOID);

	VOID SetTurnRate (FLOAT rRate);
	FLOAT GetTurnRate (VOID);

	VOID TurnLeft (VOID);
	VOID TurnRight (VOID);
	VOID SetTurnAngle (FLOAT rAngle);
	FLOAT GetTurnAngle (VOID);

	VOID MoveForward (FLOAT rVelocity);
	VOID MoveBackward (FLOAT rVelocity);
	VOID Raise (FLOAT fAddPosition);

	VOID GetLookAtPoint (FLOAT rDistance, __out DPOINT& dblLookAt);
};
