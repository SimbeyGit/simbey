#include <math.h>
#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "3rdParty\glew\include\GL\glew.h"
#include "WallTextures.h"
#include "InfiniteWolfenstein.h"
#include "Entities.h"

CDoor::CDoor (CLevelRenderer* pLevel, CWallTextures* pWalls, bool fNorthSouth, sysint idxTexture, INT nLockedType) :
	m_pLevel(pLevel),
	m_fNorthSouth(fNorthSouth),
	m_eState(None),
	m_nTimer(0),
	m_dblPosition(0.0),
	m_nLockedType(nLockedType)
{
	pWalls->GetPosition(idxTexture, &m_rcTexture);
}

VOID CDoor::Draw (MODEL_LIST* pModels)
{
	static const FCOLOR fclrDark = { 0.7f, 0.7f, 0.7f };
	static const FCOLOR fclrLight = { 1.0f, 1.0f, 1.0f };
	DOUBLE x = m_dp.x;
	DOUBLE z = m_dp.z;

	if(m_fNorthSouth)
	{
		z -= 0.5;
		z += m_dblPosition;

		DOUBLE xWest = x - 0.06;
		DOUBLE xEast = x + 0.06;

		// West Face
		glNormal3f(-1.0f, 0.0f, 0.0f);
		glTexCoord2f(m_rcTexture.left, m_rcTexture.bottom); glVertex3d(xWest, 0.0, z);
		glTexCoord2f(m_rcTexture.right, m_rcTexture.bottom); glVertex3d(xWest, 0.0, z + 1.0);
		glTexCoord2f(m_rcTexture.right, m_rcTexture.top); glVertex3d(xWest, 1.0, z + 1.0);
		glTexCoord2f(m_rcTexture.left, m_rcTexture.top); glVertex3d(xWest, 1.0, z);

		// East Face
		glNormal3f( 1.0f, 0.0f, 0.0f);
		glTexCoord2f(m_rcTexture.left, m_rcTexture.bottom); glVertex3d(xEast, 0.0, z);
		glTexCoord2f(m_rcTexture.left, m_rcTexture.top); glVertex3d(xEast, 1.0, z);
		glTexCoord2f(m_rcTexture.right, m_rcTexture.top); glVertex3d(xEast, 1.0, z + 1.0);
		glTexCoord2f(m_rcTexture.right, m_rcTexture.bottom); glVertex3d(xEast, 0.0, z + 1.0);

		if(m_dblPosition > 0.0)
		{
			FLOAT rOnePixel = m_rcTexture.left + (m_rcTexture.right - m_rcTexture.left) / 64.0f;

			// North Face
			glNormal3f(0.0f, 0.0f, -1.0f);
			glTexCoord2f(rOnePixel, m_rcTexture.bottom); glVertex3d(xEast, 0.0, z);
			glTexCoord2f(m_rcTexture.left, m_rcTexture.bottom); glVertex3d(xWest, 0.0, z);
			glTexCoord2f(m_rcTexture.left, m_rcTexture.top); glVertex3d(xWest, 1.0, z);
			glTexCoord2f(rOnePixel, m_rcTexture.top); glVertex3d(xEast, 1.0, z);
		}
	}
	else
	{
		x -= 0.5f;
		x += m_dblPosition;

		DOUBLE zNorth = z - 0.06;
		DOUBLE zSouth = z + 0.06;

		// North Face
		glNormal3f( 0.0f, 0.0f,-1.0f);
		glTexCoord2f(m_rcTexture.right, m_rcTexture.bottom); glVertex3d(x + 1.0, 0.0, zNorth);
		glTexCoord2f(m_rcTexture.left, m_rcTexture.bottom); glVertex3d(x, 0.0, zNorth);
		glTexCoord2f(m_rcTexture.left, m_rcTexture.top); glVertex3d(x, 1.0, zNorth);
		glTexCoord2f(m_rcTexture.right, m_rcTexture.top); glVertex3d(x + 1.0, 1.0, zNorth);

		// South Face
		glNormal3f( 0.0f, 0.0f, 1.0f);
		glTexCoord2f(m_rcTexture.left, m_rcTexture.bottom); glVertex3d(x, 0.0, zSouth);
		glTexCoord2f(m_rcTexture.right, m_rcTexture.bottom); glVertex3d(x + 1.0, 0.0, zSouth);
		glTexCoord2f(m_rcTexture.right, m_rcTexture.top); glVertex3d(x + 1.0, 1.0, zSouth);
		glTexCoord2f(m_rcTexture.left, m_rcTexture.top); glVertex3d(x, 1.0, zSouth);

		if(m_dblPosition > 0.0)
		{
			FLOAT rOnePixel = m_rcTexture.left + (m_rcTexture.right - m_rcTexture.left) / 64.0f;

			// West Face
			glNormal3f(-1.0f, 0.0f, 0.0f);
			glTexCoord2f(m_rcTexture.left, m_rcTexture.bottom); glVertex3d(x, 0.0, zNorth);
			glTexCoord2f(rOnePixel, m_rcTexture.bottom); glVertex3d(x, 0.0, zSouth);
			glTexCoord2f(rOnePixel, m_rcTexture.top); glVertex3d(x, 1.0, zSouth);
			glTexCoord2f(m_rcTexture.left, m_rcTexture.top); glVertex3d(x, 1.0, zNorth);
		}
	}
}

VOID CDoor::GetCollisionSolids (TArray<DBLRECT>* paSolids)
{
	DBLRECT rect;
	DOUBLE x = m_dp.x;
	DOUBLE z = m_dp.z;

	if(m_fNorthSouth)
	{
		z -= 0.5;
		z += m_dblPosition;

		DOUBLE xWest = x - 0.06;
		DOUBLE xEast = x + 0.06;

		// West Face
		rect.left = xWest;

		// East Face
		rect.right = xEast;

		// North Face (Edge of Door)
		rect.top = z;

		// South Face (In The Wall)
		rect.bottom = m_dp.z + 0.5;
	}
	else
	{
		x += 0.5f;
		x -= m_dblPosition;

		DOUBLE zNorth = z - 0.06;
		DOUBLE zSouth = z + 0.06;

		// North Face
		rect.top = zNorth;

		// South Face
		rect.bottom = zSouth;

		// East Face (Edge of Door)
		rect.right = x;

		// West Face (In The Wall)
		rect.left = m_dp.x - 0.5;
	}

	paSolids->Append(rect);
}

VOID CDoor::Activate (CLevelRenderer* pRenderer, CDungeonRegion* pRegion)
{
	if(None == m_eState)
	{
		if(TYPE_ANY_LOCKED_DOOR_GOLD == m_nLockedType)
		{
			if(!pRenderer->ConsumeGoldKey())
			{
				pRenderer->PlaySoundW(SLP(L"NoWay.wav"), m_dp);
				pRenderer->ShowLockedDoorMessage(L"YOU MUST FIND A GOLD KEY TO OPEN THIS DOOR!", 196, 172, 36);
				return;
			}
			m_nLockedType = TYPE_ANY_DOOR;
		}
		else if(TYPE_ANY_LOCKED_DOOR_SILVER == m_nLockedType)
		{
			if(!pRenderer->ConsumeSilverKey())
			{
				pRenderer->PlaySoundW(SLP(L"NoWay.wav"), m_dp);
				pRenderer->ShowLockedDoorMessage(L"YOU MUST FIND A SILVER KEY TO OPEN THIS DOOR!", 48, 140, 152);
				return;
			}
			m_nLockedType = TYPE_ANY_DOOR;
		}

		if(SUCCEEDED(pRegion->AddActiveEntity(this)))
		{
			pRenderer->PlaySound(SLP(L"DoorOpen.wav"), m_dp);
			m_eState = Opening;
		}
	}
	else if(Waiting == m_eState)
		StartClosingDoor(pRenderer);
}

VOID CDoor::Update (CLevelRenderer* pRenderer, CDungeonRegion* pRegion)
{
	switch(m_eState)
	{
	case Opening:
		m_dblPosition += (1.0 / 64.0);
		if(++m_nTimer == 64)
		{
			m_eState = Waiting;
			m_nTimer = 0;
		}
		break;
	case Waiting:
		if(++m_nTimer == 400)
			StartClosingDoor(pRenderer);
		break;
	case Closing:
		m_dblPosition -= (1.0 / 64.0);
		if(m_pLevel->CheckCollisionsWithEntity(this))
		{
			m_nTimer = 63 - m_nTimer;
			m_eState = Opening;
			Update(pRenderer, pRegion);
		}
		else if(++m_nTimer == 64)
		{
			m_eState = None;
			m_nTimer = 0;
			pRegion->RemoveActiveEntity(this);
		}
		break;
	}
}

VOID CDoor::StartClosingDoor (CLevelRenderer* pRenderer)
{
	pRenderer->PlaySound(SLP(L"DoorClose.wav"), m_dp);
	m_eState = Closing;
	m_nTimer = 0;
}

CElevatorSwitch::CElevatorSwitch (BLOCK_DATA* pBlock, INT x, INT z, sysint idxUp) :
	m_pBlock(pBlock),
	m_x(x),
	m_z(z),
	m_nTimer(0),
	m_idxUp(idxUp)
{
}

VOID CElevatorSwitch::Draw (MODEL_LIST* pModels)
{
}

VOID CElevatorSwitch::Activate (CLevelRenderer* pRenderer, CDungeonRegion* pRegion)
{
	if(SUCCEEDED(pRegion->AddActiveEntity(this)))
	{
		pRenderer->PlaySound(SLP(L"Elevator.wav"), m_dp);
		for(INT i = 0; i < ARRAYSIZE(m_pBlock->idxSides); i++)
			m_pBlock->idxSides[i] = m_idxUp;
		pRenderer->RenderRegion(pRegion);
		m_nTimer = 40;
	}
}

VOID CElevatorSwitch::Update (CLevelRenderer* pRenderer, CDungeonRegion* pRegion)
{
	if(0 == --m_nTimer)
	{
		pRegion->RemoveActiveEntity(this);
		pRenderer->ActivateElevator(m_x, m_z);
	}
}

CModelEntity::CModelEntity (CModel* pModel) :
	m_pModel(pModel)
{
}

CModelEntity::~CModelEntity ()
{
}

VOID CModelEntity::Draw (MODEL_LIST* pModels)
{
	if(fabs(m_dp.x - pModels->dpCamera.x) <= 24.0f && fabs(m_dp.z - pModels->dpCamera.z) <= 24.0f)
		pModels->paModels->Append(this);
}

VOID CModelEntity::Activate (CLevelRenderer* pRenderer, CDungeonRegion* pRegion)
{
}

VOID CModelEntity::Update (CLevelRenderer* pRenderer, CDungeonRegion* pRegion)
{
}

VOID CModelEntity::DrawModel (VOID)
{
	glPushMatrix();
	glTranslated(m_dp.x, m_dp.y, m_dp.z);
	glCallList(m_pModel->m_nDisplayList);
	glPopMatrix();
}

CModelObstacle::CModelObstacle (CModel* pModel) :
	CModelEntity(pModel)
{
}

CModelObstacle::~CModelObstacle ()
{
}

VOID CModelObstacle::GetCollisionSolids (TArray<DBLRECT>* paSolids)
{
	DBLRECT rect;
	DOUBLE xCell = m_dp.x - 0.5;
	DOUBLE zCell = m_dp.z - 0.5;

	// North Face
	rect.top = zCell;

	// West Face
	rect.left = xCell;

	// South Face
	rect.bottom = zCell + 1.0;

	// East Face
	rect.right = xCell + 1.0;

	paSolids->Append(rect);
}

CModelItem::CModelItem (CModel* pModel) :
	CModelEntity(pModel)
{
}

CModelItem::~CModelItem ()
{
}

bool CModelItem::DoPickup (CLevelRenderer* pRenderer)
{
	RSTRING rstrSoundW = m_pModel->m_rstrPickupW;
	pRenderer->PlaySoundW(RStrToWide(rstrSoundW), RStrLen(rstrSoundW), m_dp);
	if(0 < m_pModel->m_nPoints)
		pRenderer->AddPoints(m_pModel->m_nPoints);
	if(CModel::Gold == m_pModel->m_eKey)
		pRenderer->AddGoldKey();
	else if(CModel::Silver == m_pModel->m_eKey)
		pRenderer->AddSilverKey();
	return true;
}

bool CModelItem::IsSpear (VOID)
{
	return m_pModel->m_fSpear;
}
