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
	bool m_fReverseTexture;
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
	static HRESULT CreateDoor (CLevelRenderer* pLevel, CWallTextures* pWalls, bool fNorthSouth, sysint idxTexture, INT nLockedType, __deref_out CDoor** ppDoor);

public:
	CDoor (CLevelRenderer* pLevel, CWallTextures* pWalls, bool fNorthSouth, sysint idxTexture, INT nLockedType);

	virtual VOID Draw (MODEL_LIST* pModels);
	virtual VOID GetCollisionSolids (TArray<DBLRECT>* paSolids);
	virtual VOID Activate (CLevelRenderer* pRenderer, CDungeonRegion* pRegion);
	virtual VOID Update (CLevelRenderer* pRenderer, CDungeonRegion* pRegion);

protected:
	virtual VOID PlayDoorOpen (CLevelRenderer* pRenderer);
	virtual VOID PlayDoorClose (CLevelRenderer* pRenderer);

	VOID StartClosingDoor (CLevelRenderer* pRenderer);
};

class CSplitDoor : public CDoor
{
public:
	static HRESULT CreateDoor (CLevelRenderer* pLevel, CWallTextures* pWalls, bool fNorthSouth, sysint idxTexture, INT nLockedType, __deref_out CDoor** ppDoor);

public:
	CSplitDoor (CLevelRenderer* pLevel, CWallTextures* pWalls, bool fNorthSouth, sysint idxTexture, INT nLockedType);

	virtual VOID Draw (MODEL_LIST* pModels);
	virtual VOID GetCollisionSolids (TArray<DBLRECT>* paSolids);

protected:
	VOID DrawHalfDoor (DOUBLE dblOffset, FLOAT rTexStart, FLOAT rTexEnd);

	virtual VOID PlayDoorOpen (CLevelRenderer* pRenderer);
	virtual VOID PlayDoorClose (CLevelRenderer* pRenderer);
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

struct ACTIVE_SECRET
{
	INT cBlocks;
	DPOINT dpBlock;
	INT nTravel;
	sysint idxTravelBlock;
	sysint idxSides[4];

	enum
	{
		TRAVEL_NORTH,
		TRAVEL_EAST,
		TRAVEL_SOUTH,
		TRAVEL_WEST
	} eDir;
};

class CSecretDoor : public CEntity
{
public:
	CWallTextures* m_pWalls;
	ACTIVE_SECRET* m_pActive;

public:
	CSecretDoor (CWallTextures* pWalls);
	~CSecretDoor ();

	virtual VOID Draw (MODEL_LIST* pModels);
	virtual VOID GetCollisionSolids (TArray<DBLRECT>* paSolids);
	virtual VOID Activate (CLevelRenderer* pRenderer, CDungeonRegion* pRegion);
	virtual VOID Update (CLevelRenderer* pRenderer, CDungeonRegion* pRegion);

	VOID GetDirection (__out DOUBLE& x, __out DOUBLE& z);
	VOID PrepareNextBlockMovement (BLOCK_DATA* pBlock);
	sysint GetNextBlockType (CDungeonRegion* pRegion);
};
