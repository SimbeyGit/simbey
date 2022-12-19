#pragma once

#include "Library\Core\Array.h"
#include "Library\Spatial\GeometryTypes.h"

class CLevelRenderer;
class CDungeonRegion;
class CWallTextures;
class CModel;
class CModelEntity;

struct BLOCK_DATA;

struct MODEL_LIST
{
	DPOINT dpCamera;
	TArray<CModelEntity*>* paModels;
};

class CEntity
{
public:
	CEntity* m_pNext;
	DPOINT m_dp;

public:
	CEntity ()
	{
		m_pNext = NULL;
	}

	virtual ~CEntity ()
	{
	}

	virtual bool DoPickup (CLevelRenderer* pRenderer) { return false; }
	virtual bool IsSpear (VOID) { return false; }
	virtual VOID Draw (MODEL_LIST* pModels) = 0;
	virtual VOID GetCollisionSolids (TArray<DBLRECT>* paSolids) = 0;
	virtual VOID Activate (CLevelRenderer* pRenderer, CDungeonRegion* pRegion) = 0;
	virtual VOID Update (CLevelRenderer* pRenderer, CDungeonRegion* pRegion) = 0;
};

class CDoor : public CEntity
{
public:
	CLevelRenderer* m_pLevel;

	bool m_fNorthSouth;
	FRECT m_rcTexture;

	enum State
	{
		None,
		Opening,
		Waiting,
		Closing
	} m_eState;
	INT m_nTimer;
	DOUBLE m_dblPosition;

	INT m_nLockedType;

public:
	CDoor (CLevelRenderer* pLevel, CWallTextures* pWalls, bool fNorthSouth, sysint idxTexture, INT nLockedType);

	virtual VOID Draw (MODEL_LIST* pModels);
	virtual VOID GetCollisionSolids (TArray<DBLRECT>* paSolids);
	virtual VOID Activate (CLevelRenderer* pRenderer, CDungeonRegion* pRegion);
	virtual VOID Update (CLevelRenderer* pRenderer, CDungeonRegion* pRegion);

private:
	VOID StartClosingDoor (CLevelRenderer* pRenderer);
};

class CElevatorSwitch : public CEntity
{
private:
	BLOCK_DATA* m_pBlock;
	INT m_x, m_z;
	INT m_nTimer;
	sysint m_idxUp;

public:
	CElevatorSwitch (BLOCK_DATA* pBlock, INT x, INT z, sysint idxUp);

	virtual VOID Draw (MODEL_LIST* pModels);
	virtual VOID GetCollisionSolids (TArray<DBLRECT>* paSolids) { }
	virtual VOID Activate (CLevelRenderer* pRenderer, CDungeonRegion* pRegion);
	virtual VOID Update (CLevelRenderer* pRenderer, CDungeonRegion* pRegion);
};

class CModelEntity : public CEntity
{
protected:
	CModel* m_pModel;

public:
	CModelEntity (CModel* pModel);
	~CModelEntity ();

	virtual VOID Draw (MODEL_LIST* pModels);
	virtual VOID Activate (CLevelRenderer* pRenderer, CDungeonRegion* pRegion);
	virtual VOID Update (CLevelRenderer* pRenderer, CDungeonRegion* pRegion);

	VOID DrawModel (VOID);
};

class CModelObstacle : public CModelEntity
{
public:
	CModelObstacle (CModel* pModel);
	~CModelObstacle ();

	virtual VOID GetCollisionSolids (TArray<DBLRECT>* paSolids);
};

class CModelItem : public CModelEntity
{
public:
	CModelItem (CModel* pModel);
	~CModelItem ();

	virtual bool DoPickup (CLevelRenderer* pRenderer);
	virtual bool IsSpear (VOID);
	virtual VOID GetCollisionSolids (TArray<DBLRECT>* paSolids) { }
};
