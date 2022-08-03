#ifndef	_H_CAMERA
#define	_H_CAMERA

#include "Geometry.h"

class CCamera
{
public:
	DPOINT m_dblPoint;
	DOUBLE m_dblDirRadians;
	FLOAT m_rLook;
	FLOAT m_rDirection;

	BOOL m_bFixedLook;

public:
	CCamera (const CCamera& cameraSource);
	CCamera (DOUBLE x, DOUBLE y, DOUBLE z);
	CCamera ();
	~CCamera ();

	VOID SetPosition (DOUBLE xObj, DOUBLE yObj, DOUBLE zObj);
	VOID GetPosition (DOUBLE& xObj, DOUBLE& yObj, DOUBLE& zObj);

	VOID SetPerspective (VOID);

	// For SetBillboardMode() to function correctly, the last translation must be the
	// translation to the desired 3D position in world space.  SetBillboardMode() will
	// undo the direction and look rotations as set by SetPerspective().  For the best
	// results, draw all billboards within separate model view stack frames.
	VOID SetBillboardMode (VOID);

	VOID LookAt (DOUBLE x, DOUBLE y, DOUBLE z);

	FLOAT Look (FLOAT fAddAngle);
	FLOAT Turn (FLOAT fAddAngle);

	VOID MoveForward (FLOAT rVelocity);
	VOID MoveBackward (FLOAT rVelocity);
	VOID Raise (FLOAT fAddPosition);

private:
	VOID Init (VOID);
};

#endif