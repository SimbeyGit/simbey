#include <math.h>
#include <windows.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Util\Formatting.h"
#include "Library\Util\Options.h"
#include "Library\Spatial\Geometry.h"
#include "3rdParty\glew\include\GL\glew.h"
#include "Published\SIFGL.h"
#include "Package\SIFPackage.h"
#include "WallTextures.h"
#include "InfiniteWolfenstein.h"

#define	GAME_TICK_MS			17

#define	TURN_RATE				3.0f
#define	MOVE_RATE				0.1
#define	PLAYER_RADIUS			0.35

GLfloat LightAmbient[]=		{ 1.0f, 1.0f, 1.0f, 1.0f };
GLfloat LightDiffuse[]=		{ 1.0f, 1.0f, 1.0f, 1.0f };

const WCHAR c_wzInfiniteWolfenstein[] = L"InfiniteWolfensteinCls";

///////////////////////////////////////////////////////////////////////////////
// CLevelRenderer
///////////////////////////////////////////////////////////////////////////////

CLevelRenderer::CLevelRenderer (CInfiniteWolfenstein* pGame) :
	m_pGame(pGame),
	m_pGenerator(NULL),
	m_pWalls(NULL),
	m_pModels(NULL),
	m_nCompassDir(-1),
	m_pCompassFrame(NULL),
	m_xRegion(0),
	m_zRegion(0),
	m_nPoints(0),
	m_cGoldKeys(0),
	m_cSilverKeys(0),
	m_cDrawLockedDoorTicks(0)
{
	Formatting::TPrintF(m_wzKeysLabel, ARRAYSIZE(m_wzKeysLabel), NULL, L"%d GOLD KEYS   %d SILVER KEYS", m_cGoldKeys, m_cSilverKeys);
	AddPoints(0);
}

CLevelRenderer::~CLevelRenderer ()
{
	SafeRelease(m_pCompassFrame);
}

VOID CLevelRenderer::AttachLevelGenerator (CLevelGenerator* pGenerator)
{
	m_pGenerator = pGenerator;
	m_pWalls = m_pGenerator->GetWalls();
	m_pModels = m_pGenerator->GetModels();
}

HRESULT CLevelRenderer::StartGame (INT nLevel, INT xRegion, INT zRegion, BOOL fSetCamera)
{
	HRESULT hr;

	m_aCache.Clear();
	Check(PrepareLevel(nLevel, xRegion, zRegion, fSetCamera));
	GetRegionPosition(m_xRegion, m_zRegion);
	UpdateViewingTiles();

Cleanup:
	return hr;
}

HRESULT CLevelRenderer::PrepareLevel (INT nLevel, INT xRegion, INT zRegion, BOOL fSetCamera)
{
	HRESULT hr;
	TStackRef<CDungeonRegion> srRegion;

	Check(m_pGenerator->SetLevel(nLevel));
	if(-1 == nLevel)
		Check(TStrCchCpy(m_wzLevelLabel, ARRAYSIZE(m_wzLevelLabel), L"HELL"));
	else
		Check(Formatting::TPrintF(m_wzLevelLabel, ARRAYSIZE(m_wzLevelLabel), NULL, L"LEVEL %d", nLevel + 1));
	Check(GetDungeonRegion(xRegion, zRegion, &srRegion));

	if(fSetCamera)
	{
		if(-1 == srRegion->m_xStartCell || -1 == srRegion->m_xRegion)
			srRegion->FindRandomStart(m_pGenerator);

		m_camera.SetPosition((DOUBLE)(srRegion->m_xStartCell + srRegion->m_xRegion * REGION_WIDTH) + 0.5, 0.65,
			(DOUBLE)(srRegion->m_zStartCell + srRegion->m_zRegion * REGION_WIDTH) + 0.5);
		UpdateCompass();
	}

Cleanup:
	return hr;
}

VOID CLevelRenderer::RenderRegion (CDungeonRegion* pRegion)
{
	BLOCK_DATA* pbRegion = pRegion->m_bRegion;
	BLOCK_DATA* pbCell = pbRegion;
	INT xRegion = pRegion->m_xRegion * REGION_WIDTH;
	INT zRegion = pRegion->m_zRegion * REGION_WIDTH;
	sysint idxFloor, idxCeiling;

	if(FAILED(m_pWalls->Resolve(pRegion->m_pChunk->m_rstrFloorW, &idxFloor)))
		idxFloor = 0;
	if(FAILED(m_pWalls->Resolve(pRegion->m_pChunk->m_rstrCeilingW, &idxCeiling)))
		idxCeiling = 0;

	glNewList(pRegion->m_nList, GL_COMPILE);
	glBegin(GL_QUADS);

	for(INT z = 0; z < REGION_WIDTH; z++)
	{
		for(INT x = 0; x < REGION_WIDTH; x++)
		{
			if(TYPE_ANY_FLOOR == pbCell->idxBlock)
			{
				RenderFloor(static_cast<FLOAT>(xRegion + x), 0.0f, static_cast<FLOAT>(zRegion + z), idxFloor);
				RenderCeiling(static_cast<FLOAT>(xRegion + x), 0.0f, static_cast<FLOAT>(zRegion + z), idxCeiling);
			}
			else
				RenderSides(pbRegion, pbCell->idxSides, x, z, static_cast<FLOAT>(xRegion + x), static_cast<FLOAT>(zRegion + z));

			pbCell++;
		}
	}

	glEnd();
	glEndList();
}

HRESULT CLevelRenderer::PlaySound (PCWSTR pcwzSound, INT cchSound, DPOINT& dpSound)
{
	HRESULT hr;
	FLOAT rVolume = 1.0f;
	DOUBLE dblDistance = Geometry::PointDistanceD(&m_camera.m_dblPoint, &dpSound);
	if(8.0 < dblDistance)
	{
		dblDistance -= 8.0;
		CheckIf(24.0 <= dblDistance, S_FALSE);
		rVolume = static_cast<FLOAT>(24.0 - dblDistance) / 24.0f;
	}

	Check(m_pGame->PlaySound(pcwzSound, cchSound, rVolume));

Cleanup:
	return hr;
}

HRESULT CLevelRenderer::ActivateElevator (INT x, INT z)
{
	HRESULT hr;
	TStackRef<CDungeonRegion> srRegion;
	CLevelSelector* pSelector = m_pGame->GetLevelSelector();

	Check(GetDungeonRegion(m_xRegion, m_zRegion, &srRegion));

	if(m_pGenerator->CheckElevatorConnection(srRegion, x, z, 1))
		pSelector->AddLevel(m_pGenerator->GetLevel() + 1, FALSE);
	pSelector->AddLevel(m_pGenerator->GetLevel(), TRUE);
	if(m_pGenerator->CheckElevatorConnection(srRegion, x, z, -1))
		pSelector->AddLevel(m_pGenerator->GetLevel() - 1, FALSE);

	m_pGame->SelectInterface(pSelector);

Cleanup:
	return hr;
}

VOID CLevelRenderer::GetCurrentRegion (__out INT& xRegion, __out INT& zRegion)
{
	xRegion = m_xRegion;
	zRegion = m_zRegion;
}

VOID CLevelRenderer::AddPoints (INT nPoints)
{
	m_nPoints += nPoints;
	Formatting::TUInt32ToAsc(m_nPoints, m_wzScoreLabel, ARRAYSIZE(m_wzScoreLabel), 10, NULL);
}

VOID CLevelRenderer::AddGoldKey (VOID)
{
	m_cGoldKeys++;
	UpdateKeysLabel();
}

VOID CLevelRenderer::AddSilverKey (VOID)
{
	m_cSilverKeys++;
	UpdateKeysLabel();
}

bool CLevelRenderer::ConsumeGoldKey (VOID)
{
	if(0 == m_cGoldKeys)
		return false;
	m_cGoldKeys--;
	UpdateKeysLabel();
	return true;
}

bool CLevelRenderer::ConsumeSilverKey (VOID)
{
	if(0 == m_cSilverKeys)
		return false;
	m_cSilverKeys--;
	UpdateKeysLabel();
	return true;
}

VOID CLevelRenderer::ShowLockedDoorMessage (PCWSTR pcwzMessage, BYTE bRed, BYTE bGreen, BYTE bBlue)
{
	TStrCchCpy(m_wzLockedDoorLabel, ARRAYSIZE(m_wzLockedDoorLabel), pcwzMessage);
	m_rgLockedDoorColor[0] = static_cast<FLOAT>(bRed) / 255.0f;
	m_rgLockedDoorColor[1] = static_cast<FLOAT>(bGreen) / 255.0f;
	m_rgLockedDoorColor[2] = static_cast<FLOAT>(bBlue) / 255.0f;
	m_cDrawLockedDoorTicks = 200;
}

BOOL CLevelRenderer::CheckCollisionsWithEntity (CEntity* pEntity)
{
	BOOL fCollision = FALSE;
	DPOINT dpCenter = m_camera.m_dblPoint;
	DBLRECT rcPlayer = { dpCenter.x - PLAYER_RADIUS, dpCenter.z - PLAYER_RADIUS, dpCenter.x + PLAYER_RADIUS, dpCenter.z + PLAYER_RADIUS };

	pEntity->GetCollisionSolids(&m_aSolids);

	for(sysint i = 0; i < m_aSolids.Length(); i++)
	{
		DBLRECT& rect = m_aSolids[i];
		DBLRECT rcInter;

		if(Geometry::IntersectDblRect(&rcInter, &rect, &rcPlayer))
		{
			fCollision = TRUE;
			break;
		}
	}

	m_aSolids.Clear();
	return fCollision;
}

VOID CLevelRenderer::GetPlayerPosition (__out DOUBLE* px, __out DOUBLE* pz)
{
	*px = m_camera.m_dblPoint.x;
	*pz = m_camera.m_dblPoint.z;
}

// IGameInterface

VOID CLevelRenderer::DrawFrame (VOID)
{
	INT xRegion, zRegion;
	MODEL_LIST mlData;

	m_camera.SetPerspective();

	GLfloat LightPosition[] = { static_cast<FLOAT>(m_camera.m_dblPoint.x), static_cast<FLOAT>(m_camera.m_dblPoint.y), static_cast<FLOAT>(m_camera.m_dblPoint.z), 1.0f};
	glLightfv(GL_LIGHT1, GL_POSITION, LightPosition);	// Position The Light

	mlData.dpCamera = m_camera.m_dblPoint;
	mlData.paModels = &m_aModels;

	//GetRegionPosition(xRegion, zRegion);
	xRegion = m_xRegion;
	zRegion = m_zRegion;

	glEnable(GL_LIGHTING);
	m_pWalls->BindTexture();

	for(INT z = -2; z <= 2; z++)
	{
		for(INT x = -2; x <= 2; x++)
		{
			if(m_frustum.CubeInFrustum((xRegion + x) * (FLOAT)REGION_WIDTH + (REGION_WIDTH / 2.0f), 0.0f, (zRegion + z) * (FLOAT)REGION_WIDTH + (REGION_WIDTH / 2.0f), REGION_WIDTH / 2.0f))
			{
				CDungeonRegion* pRegion;
				HRESULT hrRegion = GetDungeonRegion(xRegion + x, zRegion + z, &pRegion);
				if(SUCCEEDED(hrRegion))
				{
					glCallList(pRegion->m_nList);
					pRegion->DrawEntities(&mlData);
					pRegion->Release();
				}
			}
		}
	}

	m_pModels->BindTexture();

	sysint cModels;
	CModelEntity** prgModels;
	m_aModels.GetData(&prgModels, &cModels);
	for(sysint i = 0; i < cModels; i++)
		prgModels[i]->DrawModel();

	m_aModels.Clear();
	glDisable(GL_LIGHTING);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	SIZE* psWindow = &m_pGame->m_szWindow;
	sifPushOrthoMode(psWindow);

	if(0 < m_cDrawLockedDoorTicks)
	{
		glPushMatrix();
		glTranslated(static_cast<DOUBLE>(psWindow->cx) / 2.0, static_cast<DOUBLE>(psWindow->cy) / 2.0, 0.0);
		glColor3fv(m_rgLockedDoorColor);
		m_pGame->m_pGLFont->DrawTextGL(m_wzLockedDoorLabel, 0.75f, DT_CENTER);
		glColor3f(1.0f, 1.0f, 1.0f);
		glPopMatrix();
	}

	glColor3f(1.0f, 1.0f, 1.0f);

	glPushMatrix();

	glTranslated(static_cast<DOUBLE>(psWindow->cx) - 10.0, static_cast<DOUBLE>(psWindow->cy) - 10.0, 0.0);
	m_pGame->m_pGLFont->DrawTextGL(m_wzLevelLabel, 0.5f, DT_RIGHT);

	glPopMatrix();

	glPushMatrix();

	glTranslated(10.0, static_cast<DOUBLE>(psWindow->cy) - 10.0, 0.0);
	m_pGame->m_pGLFont->DrawTextGL(m_wzKeysLabel, 0.25f, DT_LEFT);
	glTranslated(0.0, -20, 0.0);
	m_pGame->m_pGLFont->DrawTextGL(m_wzScoreLabel, 0.5f, DT_LEFT);

#ifdef	_DEBUG
	{
		WCHAR wzPosition[100];
		glTranslated(0.0, -30, 0.0);
		Formatting::TPrintF(wzPosition, ARRAYSIZE(wzPosition), NULL, L"X%d  Z%d", static_cast<INT>(m_camera.m_dblPoint.x), static_cast<INT>(m_camera.m_dblPoint.z));
		m_pGame->m_pGLFont->DrawTextGL(wzPosition, 0.25f, DT_LEFT);
	}
#endif

	glPopMatrix();

	DrawCompass();

	sifPopOrthoMode();

	glDisable(GL_BLEND);
}

VOID CLevelRenderer::UpdateFrame (VOID)
{
	BYTE bKeyState[256];
	bool fUpdate = false;
	bool fCheckItemPickup = false;
	DOUBLE dblStrafe = 0.0;

	GetKeyboardState(bKeyState);

	if(0 < m_cDrawLockedDoorTicks)
		m_cDrawLockedDoorTicks--;

	for(sysint i = 0; i < m_aCache.Length(); i++)
		m_aCache[i]->UpdateActiveEntities(this);

	POINT pt;
	if(m_pGame->m_fTrackMouse && GetCursorPos(&pt))
	{
		HWND hwnd;
		if(SUCCEEDED(m_pGame->GetWindow(&hwnd)))
		{
			POINT ptCenter = { m_pGame->m_szWindow.cx / 2, m_pGame->m_szWindow.cy / 2 };
			ClientToScreen(hwnd, &ptCenter);

			if(pt.x != ptCenter.x || pt.y != ptCenter.y)
			{
				if(pt.x != ptCenter.x)
				{
					if(bKeyState[VK_MENU] & 0x80)
					{
						dblStrafe = static_cast<DOUBLE>(pt.x - ptCenter.x) / 2.0;
						if(dblStrafe < -MOVE_RATE)
							dblStrafe = -MOVE_RATE;
						else if(dblStrafe > MOVE_RATE)
							dblStrafe = MOVE_RATE;
					}
					else
						m_camera.Turn(static_cast<FLOAT>(pt.x - ptCenter.x) / 3.5f);
				}

				if(pt.y != ptCenter.y)
				{
					INT yDelta = ptCenter.y - pt.y;
					if(abs(yDelta) >= 3)
					{
						MoveCamera((ptCenter.y > pt.y ? MOVE_RATE : -MOVE_RATE) / 2.0, m_camera.m_dblDirRadians);
						fCheckItemPickup = true;
					}
				}

				SetCursorPos(ptCenter.x, ptCenter.y);
				fUpdate = true;
			}
		}
	}

	if(bKeyState[VK_LEFT] & 0x80)
	{
		if(bKeyState[VK_MENU] & 0x80)
		{
			if(dblStrafe <= 0.0)
				dblStrafe = -MOVE_RATE;
			else
				dblStrafe -= MOVE_RATE;
		}
		else
			m_camera.Turn(-TURN_RATE);
		fUpdate = true;
	}
	if(bKeyState[VK_RIGHT] & 0x80)
	{
		if(bKeyState[VK_MENU] & 0x80)
		{
			if(dblStrafe >= 0.0)
				dblStrafe = MOVE_RATE;
			else
				dblStrafe += MOVE_RATE;
		}
		else
			m_camera.Turn(TURN_RATE);
		fUpdate = true;
	}
	if(bKeyState[VK_UP] & 0x80)
	{
		MoveCamera(MOVE_RATE, m_camera.m_dblDirRadians);
		fUpdate = true;
		fCheckItemPickup = true;
	}
	if(bKeyState[VK_DOWN] & 0x80)
	{
		MoveCamera(-MOVE_RATE, m_camera.m_dblDirRadians);
		fUpdate = true;
		fCheckItemPickup = true;
	}

	if(fUpdate)
	{
		UpdateCompass();

		if(dblStrafe)
		{
			DOUBLE dblStrafeDir = m_camera.m_dblDirRadians + Geometry::dPI / 2.0;
			if(dblStrafeDir >= Geometry::dPI * 2.0)
				dblStrafeDir -= Geometry::dPI * 2.0;
			MoveCamera(dblStrafe, dblStrafeDir);
		}

		UpdateViewingTiles();
		if(fCheckItemPickup)
			CheckItemPickup();
	}
}

VOID CLevelRenderer::OnMusicStopped (VOID)
{
	if(0 < m_aCache.Length())
	{
		RSTRING rstrMusicW;
		if(SUCCEEDED(GetCurrentRegionMusic(&rstrMusicW)))
		{
			m_pGame->PlayMusic(rstrMusicW);
			RStrRelease(rstrMusicW);
		}
	}
}

BOOL CLevelRenderer::OnKeyDown (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	if(VK_SPACE == wParam)
	{
		DOUBLE dblScan = 0.3;

		for(INT i = 0; i < 3; i++)
		{
			TStackRef<CDungeonRegion> srRegion;
			DOUBLE x = m_camera.m_dblPoint.x + sin(m_camera.m_dblDirRadians) * dblScan;
			DOUBLE z = m_camera.m_dblPoint.z - cos(m_camera.m_dblDirRadians) * dblScan;

			INT xRegion = static_cast<INT>(x / REGION_WIDTH);
			if(x < 0.0)
				xRegion--;

			INT zRegion = static_cast<INT>(z / REGION_WIDTH);
			if(z < 0.0)
				zRegion--;

			if(SUCCEEDED(GetDungeonRegion(xRegion, zRegion, &srRegion)))
			{
				INT zBlock = static_cast<INT>(floor(z)) - (srRegion->m_zRegion * REGION_WIDTH);
				INT xBlock = static_cast<INT>(floor(x)) - (srRegion->m_xRegion * REGION_WIDTH);

				BLOCK_DATA* pBlock = srRegion->m_bRegion + zBlock * REGION_WIDTH + xBlock;

				CEntity* pEntity = pBlock->m_pEntities;
				if(pEntity)
				{
					do
					{
						pEntity->Activate(this, srRegion);
						pEntity = pEntity->m_pNext;
					} while(pEntity);
					break;
				}
			}

			dblScan += 0.3;
		}
	}

	return TRUE;
}

BOOL CLevelRenderer::OnKeyUp (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return TRUE;
}

// Private Methods

VOID CLevelRenderer::UpdateCompass (VOID)
{
	static const DOUBLE c_dblSlice = (Geometry::dPI * 2.0) / 8.0;

	INT nDir = (INT)((m_camera.m_dblDirRadians + (c_dblSlice / 2.0)) / c_dblSlice);
	if(nDir == 8)
		nDir = 0;

	if(m_nCompassDir != nDir)
	{
		m_nCompassDir = nDir;
		SafeRelease(m_pCompassFrame);
		m_pGame->GetCompassFrame(nDir, &m_pCompassFrame);
	}
}

VOID CLevelRenderer::DrawCompass (VOID)
{
	TPOINT_RECT rect;

	glBindTexture(GL_TEXTURE_2D, m_pGame->GetCompassTexture());
	sifGetGLTexturePositionF(m_pCompassFrame, &rect.uLeft, &rect.vTop, &rect.uRight, &rect.vBottom);

	glBegin(GL_QUADS);
	glNormal3f(0.0f, 0.0f, 1.0f);

	glTexCoord2f(rect.uLeft, rect.vBottom);		glVertex3f(16.0f, 16.0f, 0.0f);	// Bottom Left Of The Texture and Quad
	glTexCoord2f(rect.uRight, rect.vBottom);	glVertex3f(80.0f, 16.0, 0.0f);	// Bottom Right Of The Texture and Quad
	glTexCoord2f(rect.uRight, rect.vTop);		glVertex3f(80.0f, 80.0f, 0.0f);	// Top Right Of The Texture and Quad
	glTexCoord2f(rect.uLeft, rect.vTop);		glVertex3f(16.0f, 80.0f, 0.0f);	// Top Left Of The Texture and Quad

	glEnd();
}

VOID CLevelRenderer::UpdateViewingTiles (VOID)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();

	m_camera.SetPerspective();
	m_frustum.Update();

	if(0 < m_aCache.Length())
	{
		INT xRegion, zRegion;

		GetRegionPosition(xRegion, zRegion);
		if(xRegion != m_xRegion || zRegion != m_zRegion)
		{
			RSTRING rstrMusicW;

			m_xRegion = xRegion;
			m_zRegion = zRegion;

			if(SUCCEEDED(GetCurrentRegionMusic(&rstrMusicW)) && !m_pGame->IsPlayingMusic(rstrMusicW))
				m_pGame->StopMusic();
			RStrRelease(rstrMusicW);
		}
	}
}

VOID CLevelRenderer::GetRegionPosition (INT& xRegion, INT& zRegion)
{
	DOUBLE x, y, z;
	m_camera.GetPosition(x, y, z);

	xRegion = static_cast<INT>(x / REGION_WIDTH);
	if(x < 0.0)
		xRegion--;
	zRegion = static_cast<INT>(z / REGION_WIDTH);
	if(z < 0.0)
		zRegion--;
}

HRESULT CLevelRenderer::GetDungeonRegion (INT xRegion, INT zRegion, __deref_out CDungeonRegion** ppRegion)
{
	HRESULT hr = S_OK;
	CDungeonRegion* pRegion = NULL;

	for(sysint i = 0; i < m_aCache.Length(); i++)
	{
		pRegion = m_aCache[i];
		if(pRegion->m_xRegion == xRegion && pRegion->m_zRegion == zRegion)
		{
			pRegion->AddRef();
			*ppRegion = pRegion;
			pRegion = NULL;
			goto Cleanup;
		}
	}

	if(MAX_CACHE_SIZE == m_aCache.Length())
	{
		m_aCache.Remove(0, &pRegion);
		pRegion->Reset();
		pRegion->m_xRegion = xRegion;
		pRegion->m_zRegion = zRegion;
	}
	else
	{
		pRegion = __new CDungeonRegion(xRegion, zRegion);
		CheckAlloc(pRegion);
	}

	Check(m_pGenerator->GenerateRegion(pRegion, this));

	RenderRegion(pRegion);

	Check(m_aCache.Append(pRegion));
	*ppRegion = pRegion;
	pRegion = NULL;

Cleanup:
	SafeRelease(pRegion);
	return hr;
}

HRESULT CLevelRenderer::GetCurrentRegionMusic (RSTRING* prstrMusicW)
{
	HRESULT hr;
	INT xRegion, zRegion;
	TStackRef<CDungeonRegion> srRegion;

	GetRegionPosition(xRegion, zRegion);
	Check(GetDungeonRegion(xRegion, zRegion, &srRegion));
	RStrSet(*prstrMusicW, srRegion->m_rstrMusic);

Cleanup:
	return hr;
}

HRESULT CLevelRenderer::UpdateKeysLabel (VOID)
{
	return Formatting::TPrintF(m_wzKeysLabel, ARRAYSIZE(m_wzKeysLabel), NULL, L"%d GOLD KEYS   %d SILVER KEYS", m_cGoldKeys, m_cSilverKeys);
}

VOID CLevelRenderer::MoveCamera (DOUBLE dblMove, DOUBLE dblDirRadians)
{
	DOUBLE xCenter, yCenter, zCenter;
	INT xRegion, zRegion;

	m_camera.GetPosition(xCenter, yCenter, zCenter);
	xCenter = floor(xCenter);
	zCenter = floor(zCenter);

	for(INT z = -1; z <= 1; z++)
	{
		DOUBLE zCell = zCenter + z;

		if(zCell < 0.0)
			zRegion = static_cast<INT>((zCell + 1.0) / REGION_WIDTH) - 1;
		else
			zRegion = static_cast<INT>(zCell / REGION_WIDTH);

		for(INT x = -1; x <= 1; x++)
		{
			DOUBLE xCell = xCenter + x;
			TStackRef<CDungeonRegion> srRegion;

			if(xCell < 0.0)
				xRegion = static_cast<INT>((xCell + 1.0) / REGION_WIDTH) - 1;
			else
				xRegion = static_cast<INT>(xCell / REGION_WIDTH);

			if(SUCCEEDED(GetDungeonRegion(xRegion, zRegion, &srRegion)))
			{
				INT zBlock = static_cast<INT>(zCell) - (srRegion->m_zRegion * REGION_WIDTH);
				INT xBlock = static_cast<INT>(xCell) - (srRegion->m_xRegion * REGION_WIDTH);

				BLOCK_DATA* pBlock = srRegion->m_bRegion + zBlock * REGION_WIDTH + xBlock;
				CEntity* pEntity = pBlock->m_pEntities;

				if(TYPE_ANY_FLOOR != pBlock->idxBlock)
				{
					DBLRECT rect;

					// North Face
					rect.top = zCell;

					// West Face
					rect.left = xCell;

					// South Face
					rect.bottom = zCell + 1.0;

					// East Face
					rect.right = xCell + 1.0;

					m_aSolids.Append(rect);
				}

				while(pEntity)
				{
					pEntity->GetCollisionSolids(&m_aSolids);
					pEntity = pEntity->m_pNext;
				}
			}
		}
	}

	DPOINT dpCenter = m_camera.m_dblPoint;

	dpCenter.x += sin(dblDirRadians) * dblMove;
	for(sysint i = 0; i < m_aSolids.Length(); i++)
	{
		DBLRECT& rect = m_aSolids[i];
		DBLRECT rcInter;
		DBLRECT rcPlayer = { dpCenter.x - PLAYER_RADIUS, dpCenter.z - PLAYER_RADIUS, dpCenter.x + PLAYER_RADIUS, dpCenter.z + PLAYER_RADIUS };

		if(Geometry::IntersectDblRect(&rcInter, &rect, &rcPlayer))
		{
			if(m_camera.m_dblPoint.x < rect.left)
				dpCenter.x = rect.left - (PLAYER_RADIUS + 0.001);
			else
				dpCenter.x = rect.right + (PLAYER_RADIUS + 0.001);
		}
	}

	dpCenter.z -= cos(dblDirRadians) * dblMove;
	for(sysint i = 0; i < m_aSolids.Length(); i++)
	{
		DBLRECT& rect = m_aSolids[i];
		DBLRECT rcInter;
		DBLRECT rcPlayer = { dpCenter.x - PLAYER_RADIUS, dpCenter.z - PLAYER_RADIUS, dpCenter.x + PLAYER_RADIUS, dpCenter.z + PLAYER_RADIUS };

		if(Geometry::IntersectDblRect(&rcInter, &rect, &rcPlayer))
		{
			if(m_camera.m_dblPoint.z < rect.top)
				dpCenter.z = rect.top - (PLAYER_RADIUS + 0.001);
			else
				dpCenter.z = rect.bottom + (PLAYER_RADIUS + 0.001);
		}
	}

	m_camera.m_dblPoint = dpCenter;
	m_aSolids.Clear();
}

VOID CLevelRenderer::CheckItemPickup (VOID)
{
	TStackRef<CDungeonRegion> srRegion;

	if(SUCCEEDED(GetDungeonRegion(m_xRegion, m_zRegion, &srRegion)))
	{
		DOUBLE zCell = floor(m_camera.m_dblPoint.z);
		DOUBLE xCell = floor(m_camera.m_dblPoint.x);
		INT zBlock = static_cast<INT>(zCell) - (srRegion->m_zRegion * REGION_WIDTH);
		INT xBlock = static_cast<INT>(xCell) - (srRegion->m_xRegion * REGION_WIDTH);
		BLOCK_DATA* pBlock = srRegion->m_bRegion + (zBlock * REGION_WIDTH + xBlock);
		if(pBlock->m_pEntities)
		{
			bool fHasSpear = false;
			CEntity* pEntities = pBlock->m_pEntities, *pPrev = NULL;
			while(pEntities)
			{
				CEntity* pNext = pEntities->m_pNext;
				if(pEntities->DoPickup(this))
				{
					if(pEntities->IsSpear())
						fHasSpear = true;

					if(pPrev)
						pPrev->m_pNext = pNext;
					else
						pBlock->m_pEntities = pNext;

					sysint idxRemove;
					SideAssert(srRegion->m_aEntities.IndexOf(pEntities, idxRemove));
					srRegion->m_aEntities.Remove(idxRemove, NULL);
					__delete pEntities;
				}
				else
					pPrev = pEntities;
				pEntities = pNext;
			}

			if(fHasSpear)
			{
				INT xOffset = -m_xRegion;
				INT zOffset = -m_zRegion;

				m_camera.m_dblPoint.x += static_cast<DOUBLE>(xOffset) * REGION_WIDTH;
				m_camera.m_dblPoint.z += static_cast<DOUBLE>(zOffset) * REGION_WIDTH;

				m_pGenerator->SetHellStart(static_cast<INT>(m_camera.m_dblPoint.x), static_cast<INT>(m_camera.m_dblPoint.z));

				StartGame(-1, 0, 0, FALSE);
				m_pGame->StopMusic();
			}
		}
	}
}

VOID CLevelRenderer::RenderFloor (FLOAT x, FLOAT y, FLOAT z, sysint idxFloor)
{
	FRECT rc;
	if(SUCCEEDED(m_pWalls->GetPosition(idxFloor, &rc)))
	{
		glNormal3f(0.0f, 1.0f, 0.0f);
		glTexCoord2f(rc.left, rc.top); glVertex3f(x, y, z);
		glTexCoord2f(rc.left, rc.bottom); glVertex3f(x, y, z + 1.0f);
		glTexCoord2f(rc.right, rc.bottom); glVertex3f(x + 1.0f, y, z + 1.0f);
		glTexCoord2f(rc.right, rc.top); glVertex3f(x + 1.0f, y, z);
	}
}

VOID CLevelRenderer::RenderCeiling (FLOAT x, FLOAT y, FLOAT z, sysint idxCeiling)
{
	FRECT rc;
	if(SUCCEEDED(m_pWalls->GetPosition(idxCeiling, &rc)))
	{
		y += 1.0f;

		glNormal3f(0.0f, -1.0f, 0.0f);
		glTexCoord2f(rc.left, rc.bottom); glVertex3f(x, y, z);
		glTexCoord2f(rc.right, rc.bottom); glVertex3f(x + 1.0f, y, z);
		glTexCoord2f(rc.right, rc.top); glVertex3f(x + 1.0f, y, z + 1.0f);
		glTexCoord2f(rc.left, rc.top); glVertex3f(x, y, z + 1.0f);
	}
}

VOID CLevelRenderer::RenderSides (BLOCK_DATA* pbRegion, sysint* pidxSides, INT xBlock, INT zBlock, FLOAT x, FLOAT z)
{
	FRECT rc;

	if(zBlock == 0 || TYPE_ANY_FLOOR == pbRegion[(zBlock - 1) * REGION_WIDTH + xBlock].idxBlock)
	{
		if(SUCCEEDED(m_pWalls->GetPosition(pidxSides[1], &rc)))
		{
			// North Face
			glNormal3f(0.0f, 0.0f, -1.0f);
			glTexCoord2f(rc.right, rc.bottom); glVertex3f(x, 0.0f, z);
			glTexCoord2f(rc.right, rc.top); glVertex3f(x, 1.0f, z);
			glTexCoord2f(rc.left, rc.top); glVertex3f(x + 1.0f, 1.0f, z);
			glTexCoord2f(rc.left, rc.bottom); glVertex3f(x + 1.0f, 0.0f, z);
		}
	}

	if(xBlock == 0 || TYPE_ANY_FLOOR == pbRegion[zBlock * REGION_WIDTH + (xBlock - 1)].idxBlock)
	{
		if(SUCCEEDED(m_pWalls->GetPosition(pidxSides[0], &rc)))
		{
			// West Face
			glNormal3f(-1.0f, 0.0f, 0.0f);
			glTexCoord2f(rc.left, rc.bottom); glVertex3f(x, 0.0f, z);
			glTexCoord2f(rc.right, rc.bottom); glVertex3f(x, 0.0f, z + 1.0f);
			glTexCoord2f(rc.right, rc.top); glVertex3f(x, 1.0f, z + 1.0f);
			glTexCoord2f(rc.left, rc.top); glVertex3f(x, 1.0f, z);
		}
	}

	if(zBlock == (REGION_WIDTH - 1) || TYPE_ANY_FLOOR == pbRegion[(zBlock + 1) * REGION_WIDTH + xBlock].idxBlock)
	{
		if(SUCCEEDED(m_pWalls->GetPosition(pidxSides[3], &rc)))
		{
			// South Face
			glNormal3f(0.0f, 0.0f, 1.0f);
			glTexCoord2f(rc.left, rc.bottom); glVertex3f(x, 0.0f, z + 1.0f);
			glTexCoord2f(rc.right, rc.bottom); glVertex3f(x + 1.0f, 0.0f, z + 1.0f);
			glTexCoord2f(rc.right, rc.top); glVertex3f(x + 1.0f, 1.0f, z + 1.0f);
			glTexCoord2f(rc.left, rc.top); glVertex3f(x, 1.0f, z + 1.0f);
		}
	}

	if(xBlock == (REGION_WIDTH - 1) || TYPE_ANY_FLOOR == pbRegion[zBlock * REGION_WIDTH + (xBlock + 1)].idxBlock)
	{
		if(SUCCEEDED(m_pWalls->GetPosition(pidxSides[2], &rc)))
		{
			// East Face
			glNormal3f(1.0f, 0.0f, 0.0f);
			glTexCoord2f(rc.right, rc.bottom); glVertex3f(x + 1.0f, 0.0f, z);
			glTexCoord2f(rc.right, rc.top); glVertex3f(x + 1.0f, 1.0f, z);
			glTexCoord2f(rc.left, rc.top); glVertex3f(x + 1.0f, 1.0f, z + 1.0f);
			glTexCoord2f(rc.left, rc.bottom); glVertex3f(x + 1.0f, 0.0f, z + 1.0f);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// CLevelSelector
///////////////////////////////////////////////////////////////////////////////

CLevelSelector::CLevelSelector (CInfiniteWolfenstein* pGame) :
	m_pGame(pGame),
	m_idxSelected(-1)
{
}

CLevelSelector::~CLevelSelector ()
{
}

HRESULT CLevelSelector::AddLevel (INT nLevel, BOOL fSelected)
{
	if(fSelected)
		m_idxSelected = m_aLevels.Length();
	return m_aLevels.Append(nLevel);
}

// IGameInterface

VOID CLevelSelector::DrawFrame (VOID)
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	SIZE* psWindow = &m_pGame->m_szWindow;
	sifPushOrthoMode(psWindow);

	glBegin(GL_QUADS);

	glColor3f(0.0f, 0.5f, 0.5f);
	glNormal3f(0.0f, 0.0f, 1.0f);
	glVertex3f(0.0f, 0.0f, 0.0f);
	glVertex3f(static_cast<FLOAT>(psWindow->cx), 0.0f, 0.0f);
	glVertex3f(static_cast<FLOAT>(psWindow->cx), static_cast<FLOAT>(psWindow->cy), 0.0f);
	glVertex3f(0.0f, static_cast<FLOAT>(psWindow->cy), 0.0f);

	glEnd();

	glTranslated(static_cast<DOUBLE>(psWindow->cx) / 2.0, static_cast<DOUBLE>(psWindow->cy) - 50.0, 0.0);

	glColor3f(0.0f, 0.5f, 0.5f);
	m_pGame->m_pGLFont->DrawTextGL(L"SELECT LEVEL", 1.0f, DT_CENTER);

	glTranslated(0.0, -75.0, 0.0);

	for(sysint i = 0; i < m_aLevels.Length(); i++)
	{
		WCHAR wzLevel[32];
		INT nLevel = m_aLevels[i];
		BOOL fSelected = i == m_idxSelected;
		glColor3f(0.0f, 0.5f, 0.5f);
		Formatting::TPrintF(wzLevel, ARRAYSIZE(wzLevel), NULL, L"LEVEL %d", nLevel + 1);
		if(fSelected)
			glColor3f(0.2f, 0.8f, 0.8f);
		m_pGame->m_pGLFont->DrawTextGL(wzLevel, 0.8f, DT_LEFT);
		if(fSelected)
			m_pGame->m_pGLFont->DrawTextGL(L"X  ", 0.8f, DT_RIGHT);
		glTranslated(0.0, -50.0, 0.0);
	}

	sifPopOrthoMode();

	glDisable(GL_BLEND);
}

VOID CLevelSelector::UpdateFrame (VOID)
{
}

VOID CLevelSelector::OnMusicStopped (VOID)
{
	RSTRING rstrMusicW;
	if(SUCCEEDED(RStrCreateW(LSP(L"End of Level.MID"), &rstrMusicW)))
	{
		m_pGame->PlayMusic(rstrMusicW);
		RStrRelease(rstrMusicW);
	}
}

BOOL CLevelSelector::OnKeyDown (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	if(VK_UP == wParam)
	{
		m_idxSelected--;
		if(m_idxSelected < 0)
			m_idxSelected = m_aLevels.Length() - 1;
	}
	else if(VK_DOWN == wParam)
	{
		m_idxSelected++;
		if(m_idxSelected == m_aLevels.Length())
			m_idxSelected = 0;
	}
	else if(VK_RETURN == wParam)
	{
		INT xRegion, zRegion;
		CLevelRenderer* pRenderer = m_pGame->GetLevelRenderer();
		pRenderer->GetCurrentRegion(xRegion, zRegion);
		pRenderer->StartGame(m_aLevels[m_idxSelected], xRegion, zRegion, FALSE);
		m_pGame->SelectInterface(pRenderer);
		m_aLevels.Clear();
		m_idxSelected = -1;
	}

	return TRUE;
}

BOOL CLevelSelector::OnKeyUp (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
// CInfiniteWolfenstein
///////////////////////////////////////////////////////////////////////////////

HRESULT CInfiniteWolfenstein::Register (HINSTANCE hInstance)
{
	WNDCLASSEX wnd = {0};

	wnd.cbSize = sizeof(WNDCLASSEX);
	wnd.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;
	wnd.cbClsExtra = 0;
	wnd.cbWndExtra = 0;
	wnd.hInstance = hInstance;
	wnd.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAIN));
	wnd.hCursor = LoadCursor(NULL, IDC_ARROW);
	wnd.hbrBackground = NULL;
	wnd.lpszMenuName = NULL;
	wnd.lpszClassName = c_wzInfiniteWolfenstein;

	return RegisterClass(&wnd, NULL);
}

CInfiniteWolfenstein::CInfiniteWolfenstein (HINSTANCE hInstance) :
	m_hInstance(hInstance),
	m_fActive(FALSE),
	m_fTrackMouse(TRUE),
	m_rstrMusic(NULL),
	m_pWalls(NULL),
	m_pWallThemes(NULL),
	m_pChunkThemes(NULL),
	m_pLevels(NULL),
	m_pRooms(NULL),
	m_pSpecial(NULL),
	m_pModels(NULL),
	m_pSystem(NULL),
	m_pFont(NULL),
	m_pCompass(NULL),
	m_nCompass(0),
	m_pGLFont(NULL),
	m_pGenerator(NULL),
	m_renderer(this),
	m_selector(this),
	m_pActive(&m_renderer)
{
}

CInfiniteWolfenstein::~CInfiniteWolfenstein ()
{
	m_mapMusic.DeleteAll();
	RStrRelease(m_rstrMusic);

	SafeDelete(m_pGenerator);
	SafeDelete(m_pLevels);
	SafeDelete(m_pChunkThemes);
	SafeDelete(m_pWallThemes);
	SafeDelete(m_pWalls);
	SafeDelete(m_pRooms);
	SafeDelete(m_pSpecial);
	SafeDelete(m_pModels);

	if(m_pCompass)
	{
		glDeleteTextures(1, &m_nCompass);

		m_pCompass->Close();
		SafeRelease(m_pCompass);
	}

	if(m_pFont)
	{
		SafeRelease(m_pGLFont);

		m_pFont->Close();
		SafeRelease(m_pFont);
	}

	if(m_pSystem)
	{
		for(sysint i = 0; i < m_mapSounds.Length(); i++)
			(*m_mapSounds.GetValuePtr(i))->release();
		m_pSystem->release();
	}
}

HRESULT CInfiniteWolfenstein::Initialize (LPWSTR lpCmdLine, INT nCmdShow)
{
	HRESULT hr;
	COptions options;
	sysint idxPkgCmd;
	INT cDrivers;
	DWORDLONG dwlSeed;

	Check(options.Parse(lpCmdLine));

	RSTRING rstrSeedW;
	if(options.GetParamValue(L"seed", &rstrSeedW))
	{
		PCWSTR pcwzSeed = RStrToWide(rstrSeedW);
		if(L'-' == *pcwzSeed)
			dwlSeed = Formatting::TAscToInt64(pcwzSeed);
		else
			dwlSeed = Formatting::TAscToXUInt64(pcwzSeed, 10);
		RStrRelease(rstrSeedW);
	}
	else
		dwlSeed = GetTickCount();

	m_pWalls = __new CWallTextures;
	CheckAlloc(m_pWalls);
	Check(m_pWalls->Initialize());

	m_pWallThemes = __new CWallThemes;
	CheckAlloc(m_pWallThemes);

	m_pChunkThemes = __new CChunkThemes;
	CheckAlloc(m_pChunkThemes);

	m_pLevels = __new CLevels;
	CheckAlloc(m_pLevels);

	m_pRooms = __new CRooms;
	CheckAlloc(m_pRooms);

	m_pSpecial = __new CRooms;
	CheckAlloc(m_pSpecial);

	m_pModels = __new CModels;
	CheckAlloc(m_pModels);
	Check(m_pModels->Initialize());

	CheckIf(FMOD_OK != FMOD::System_Create(&m_pSystem), E_FAIL);
	CheckIf(FMOD_OK != m_pSystem->getNumDrivers(&cDrivers), E_FAIL);
	CheckIf(0 == cDrivers, E_FAIL);
	CheckIf(FMOD_OK != m_pSystem->init(8, FMOD_INIT_NORMAL, NULL), E_FAIL);

	if(GetFileAttributes(L"InfiniteWolfenstein.pkg") != INVALID_FILE_ATTRIBUTES)
		Check(LoadPackage(L"InfiniteWolfenstein.pkg", TRUE));
	else if(GetFileAttributes(L"..\\InfiniteWolfenstein.pkg") != INVALID_FILE_ATTRIBUTES)
		Check(LoadPackage(L"..\\InfiniteWolfenstein.pkg", TRUE));
	else
	{
		MessageBox(GetDesktopWindow(), L"Could not find InfiniteWolfenstein.pkg!", L"Missing Data Package", MB_ICONERROR | MB_OK);
		Check(HRESULT_FROM_WIN32(ERROR_MISSING_SYSTEMFILE));
	}

	if(options.FindParam(L"pkg", &idxPkgCmd))
	{
		for(sysint i = idxPkgCmd + 1; i < options.Length(); i++)
		{
			RSTRING rstrArg = options[i];
			PCWSTR pcwzArg = RStrToWide(rstrArg);

			if(L'-' == pcwzArg[0] || L'/' == pcwzArg[0])
				break;

			Check(LoadPackage(pcwzArg, FALSE));
		}
	}

	Check(CreateGL(0, FALSE, c_wzInfiniteWolfenstein, L"Infinite Wolfenstein Demo", CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, NULL, nCmdShow));
	Check(RegisterUserMessage(&m_msgNotifyFinished));

	CheckIf(GLEW_OK != glewInit(), E_FAIL);
	CheckIf(!GLEW_VERSION_1_2, E_FAIL);

	Check(m_player.Initialize());

	m_pGenerator = __new CLevelGenerator(m_pWalls, m_pWallThemes, m_pChunkThemes, m_pLevels, m_pRooms, m_pSpecial, m_pModels, dwlSeed);
	CheckAlloc(m_pGenerator);
	m_renderer.AttachLevelGenerator(m_pGenerator);

	INT nLevel = 0;
	RSTRING rstrLevelW;
	if(options.GetParamValue(L"floor", &rstrLevelW))
	{
		nLevel = Formatting::TAscToInt32(RStrToWide(rstrLevelW)) - 1;
		RStrRelease(rstrLevelW);
		CheckIf(nLevel < 0, E_INVALIDARG);
	}

	if(options.GetParamValue(L"level", &rstrLevelW))
	{
		for(;;)
		{
			INT nResult;
			Check(m_pGenerator->SetLevel(nLevel));
			Check(RStrCompareIRStr(m_pGenerator->GetLevelName(), rstrLevelW, &nResult));
			if(0 == nResult)
				break;
			CheckIf(INT_MAX == nLevel, HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
			nLevel++;
		}
		RStrRelease(rstrLevelW);
	}

	Check(m_renderer.StartGame(nLevel, 0, 0, TRUE));

	OnNotifyFinished(&m_player, TRUE);

Cleanup:
	return hr;
}

VOID CInfiniteWolfenstein::Run (VOID)
{
	MSG msg;
	DWORD dwTimer = 0;

	if(m_fTrackMouse)
		ShowCursor(FALSE);

	for(;;)
	{
		while(PeekMessage(&msg,NULL,0,0,PM_REMOVE))
		{
			if(msg.message == WM_QUIT)
				goto Cleanup;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if(m_fActive)
		{
			DWORD dwNow = GetTickCount();
			DWORD dwFrame = dwNow - dwTimer;
			if(dwFrame >= GAME_TICK_MS || WAIT_TIMEOUT == MsgWaitForMultipleObjects(0,NULL,FALSE,GAME_TICK_MS - dwFrame,QS_ALLINPUT))
			{
				dwTimer = dwNow;
				m_pActive->UpdateFrame();
				m_pSystem->update();
				Redraw();
			}
		}
		else
			WaitMessage();
	}

Cleanup:
	if(m_fTrackMouse)
	{
		ShowCursor(TRUE);
		m_fTrackMouse = FALSE;
	}
}

HRESULT CInfiniteWolfenstein::PlayMusic (RSTRING rstrMusicW)
{
	MIDI::CFile* pMIDIFile;
	HRESULT hr = m_mapMusic.Find(rstrMusicW, &pMIDIFile);
	if(SUCCEEDED(hr))
	{
		hr = m_player.StartFromFile(this, pMIDIFile);
		RStrReplace(&m_rstrMusic, rstrMusicW);
	}
	return hr;
}

BOOL CInfiniteWolfenstein::IsPlayingMusic (RSTRING rstrMusicW)
{
	INT nResult;
	return SUCCEEDED(RStrCompareRStr(m_rstrMusic, rstrMusicW, &nResult)) && 0 == nResult;
}

HRESULT CInfiniteWolfenstein::StopMusic (VOID)
{
	return m_player.Stop();
}

HRESULT CInfiniteWolfenstein::PlaySound (PCWSTR pcwzSound, INT cchSound, FLOAT rVolume)
{
	HRESULT hr;
	RSTRING rstrSound = NULL;
	FMOD::Sound* pSound;
	FMOD::Channel* pChannel;

	Check(RStrCreateW(cchSound, pcwzSound, &rstrSound));
	Check(m_mapSounds.Find(rstrSound, &pSound));

	CheckIf(FMOD_OK != m_pSystem->playSound(FMOD_CHANNEL_FREE, pSound, true, &pChannel), E_FAIL);
	pChannel->setVolume(rVolume);
	pChannel->setPaused(false);

Cleanup:
	RStrRelease(rstrSound);
	return hr;
}

VOID CInfiniteWolfenstein::SelectInterface (IGameInterface* pInterface)
{
	m_pActive = pInterface;
	m_player.Stop();
}

HRESULT CInfiniteWolfenstein::GetCompassFrame (INT nDir, __deref_out ISimbeyInterchangeFileLayer** ppCompassFrame)
{
	return m_pCompass->GetLayerByIndex(nDir, ppCompassFrame);
}

// CBaseWindow

HINSTANCE CInfiniteWolfenstein::GetInstance (VOID)
{
	return m_hInstance;
}

VOID CInfiniteWolfenstein::OnFinalDestroy (HWND hwnd)
{
	m_player.Stop();

	UnregisterUserMessage(m_msgNotifyFinished);

	__super::OnFinalDestroy(hwnd);

	PostQuitMessage(0);
}

// CBaseGLWindow

HRESULT CInfiniteWolfenstein::CreateTextures (VOID)
{
	HRESULT hr;

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);

	glLightfv(GL_LIGHT1, GL_AMBIENT, LightAmbient);		// Setup The Ambient Light
	glLightfv(GL_LIGHT1, GL_DIFFUSE, LightDiffuse);		// Setup The Diffuse Light
	glEnable(GL_LIGHT1);								// Enable Light One

	Check(m_pWalls->RebuildTextures());
	Check(m_pModels->RebuildModels());

	SafeRelease(m_pGLFont);
	Check(sifCreateFontGLFromSIF(m_pFont, FALSE, &m_pGLFont));

	if(0 != m_nCompass)
	{
		glDeleteTextures(1, &m_nCompass);
		m_nCompass = 0;
	}

	Check(sifMergeCanvasToOpenGLTexture32(m_pCompass, 0, 0, 0, 0, &m_nCompass));

Cleanup:
	return hr;
}

VOID CInfiniteWolfenstein::Draw (VOID)
{
	m_pActive->DrawFrame();
}

VOID CInfiniteWolfenstein::RequestModeChange (BOOL fFullScreen)
{
}

// INotifyFinished

VOID CInfiniteWolfenstein::OnNotifyFinished (MIDI::CPlayer* pPlayer, BOOL fCompleted)
{
	PostMessage(m_hwnd, m_msgNotifyFinished, fCompleted, 0);
}

BOOL CInfiniteWolfenstein::OnKeyDown (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	if(L'M' == wParam)
	{
		m_fTrackMouse = !m_fTrackMouse;
		ShowCursor(!m_fTrackMouse);

		if(m_fTrackMouse)
		{
			POINT ptCenter = { m_szWindow.cx / 2, m_szWindow.cy / 2 };
			ClientToScreen(m_hwnd, &ptCenter);
			SetCursorPos(ptCenter.x, ptCenter.y);
		}
	}

	return m_pActive->OnKeyDown(uMsg, wParam, lParam, lResult);
}

BOOL CInfiniteWolfenstein::OnKeyUp (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return m_pActive->OnKeyUp(uMsg, wParam, lParam, lResult);
}

BOOL CInfiniteWolfenstein::OnRButtonDown (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return m_pActive->OnKeyDown(uMsg, VK_SPACE, lParam, lResult);
}

BOOL CInfiniteWolfenstein::OnActivate (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	m_fActive = (WA_INACTIVE != LOWORD(wParam));
	return FALSE;
}

BOOL CInfiniteWolfenstein::OnSize (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	m_szWindow.cx = LOWORD(lParam);
	m_szWindow.cy = HIWORD(lParam);

	if(m_fTrackMouse)
	{
		POINT ptCenter = { m_szWindow.cx / 2, m_szWindow.cy / 2 };
		ClientToScreen(m_hwnd, &ptCenter);
		SetCursorPos(ptCenter.x, ptCenter.y);
	}

	return FALSE;
}

BOOL CInfiniteWolfenstein::OnClose (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	return FALSE;
}

BOOL CInfiniteWolfenstein::OnSysCommand (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	if(SC_KEYMENU == wParam)
	{
		lResult = FALSE;
		return TRUE;
	}
	return FALSE;
}

BOOL CInfiniteWolfenstein::OnNotifyFinishedMsg (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	m_pActive->OnMusicStopped();
	return TRUE;
}

HRESULT CInfiniteWolfenstein::LoadPackage (PCWSTR pcwzPackage, BOOL fRequireAll)
{
	HRESULT hr;
	TStackRef<CSIFPackage> srPackage, srMusic, srSounds;
	TStackRef<IJSONValue> srv;
	RSTRING rstrFile = NULL;
	MIDI::CFile* pMIDIFile = NULL;

	srPackage.Attach(__new CSIFPackage);
	CheckAlloc(srPackage);
	Check(srPackage->OpenPackage(pcwzPackage));

	Check(m_pWalls->Load(srPackage, fRequireAll));
	Check(m_pWallThemes->Load(srPackage, m_pWalls, SLP(L"wallthemes.json"), fRequireAll));
	Check(m_pChunkThemes->Load(srPackage, m_pWallThemes, SLP(L"chunkthemes.json"), fRequireAll));
	Check(m_pLevels->Load(srPackage, m_pChunkThemes, SLP(L"levels.json"), fRequireAll));
	Check(m_pModels->Load(srPackage, fRequireAll));

	if(SUCCEEDED(srPackage->OpenDirectory(SLP(L"music"), &srMusic)))
	{
		TStackRef<IJSONArray> srDirectory;

		Check(srMusic->GetDirectory(&srDirectory));
		for(sysint i = 0; i < srDirectory->Count(); i++)
		{
			TStackRef<IJSONObject> srFile;
			TStackRef<IPersistedFile> srData;
			ULARGE_INTEGER uliSize;

			Check(srDirectory->GetObject(i, &srFile));
			Check(srFile->FindNonNullValueW(L"name", &srv));
			Check(srv->GetString(&rstrFile));
			Check(srMusic->OpenFile(RStrToWide(rstrFile), RStrLen(rstrFile), &srData, &uliSize));

			pMIDIFile = __new MIDI::CFile;
			CheckAlloc(pMIDIFile);

			Check(MIDI::LoadMIDI(srData, &uliSize, pMIDIFile));
			Check(m_mapMusic.Add(rstrFile, pMIDIFile));
			pMIDIFile = NULL;

			RStrRelease(rstrFile); rstrFile = NULL;
			srv.Release();
		}
	}

	if(SUCCEEDED(srPackage->OpenDirectory(SLP(L"sounds"), &srSounds)))
	{
		TStackRef<IJSONArray> srDirectory;
		FMOD_CREATESOUNDEXINFO exInfo;

		ZeroMemory(&exInfo, sizeof(exInfo));
		exInfo.cbsize = sizeof(exInfo);

		Check(srSounds->GetDirectory(&srDirectory));
		for(sysint i = 0; i < srDirectory->Count(); i++)
		{
			TStackRef<IJSONObject> srFile;
			CMemoryStream stmData;
			FMOD::Sound* pSound;
			FMOD_RESULT eResult;

			Check(srDirectory->GetObject(i, &srFile));
			Check(srFile->FindNonNullValueW(L"name", &srv));
			Check(srv->GetString(&rstrFile));
			Check(srSounds->ReadFile(RStrToWide(rstrFile), RStrLen(rstrFile), &stmData));
			exInfo.length = stmData.DataRemaining();

			eResult = m_pSystem->createSound(stmData.TGetReadPtr<CHAR>(), FMOD_HARDWARE | FMOD_OPENMEMORY, &exInfo, &pSound);
			CheckIf(FMOD_OK != eResult, E_FAIL);

			pSound->setMode(FMOD_LOOP_OFF);
			hr = m_mapSounds.Add(rstrFile, pSound);
			if(FAILED(hr))
			{
				pSound->release();
				Check(hr);
			}

			RStrRelease(rstrFile); rstrFile = NULL;
			srv.Release();
		}
	}

	if(fRequireAll)
	{
		TStackRef<ISimbeyInterchangeFile> srFont;

		Check(srPackage->OpenSIF(L"fonts\\font.sif", &srFont));
		Check(CreateGLFont(srFont));
		srFont->Close();

		Check(srPackage->OpenSIF(L"compass\\compass.sif", &m_pCompass));
	}

	if(SUCCEEDED(srPackage->GetJSONData(SLP(L"rooms.json"), &srv)))
	{
		TStackRef<IJSONArray> srData;
		Check(srv->GetArray(&srData));
		Check(m_pRooms->Add(m_pWalls, srData));
		srv.Release();
	}

	if(SUCCEEDED(srPackage->GetJSONData(SLP(L"special.json"), &srv)))
	{
		TStackRef<IJSONArray> srData;
		Check(srv->GetArray(&srData));
		Check(m_pSpecial->Add(m_pWalls, srData));
	}

Cleanup:
	if(FAILED(hr) && fRequireAll)
	{
		WCHAR wzError[MAX_PATH];
		Formatting::TPrintF(wzError, ARRAYSIZE(wzError), NULL, L"Failed to load \"%ls\" with error: 0x%.8X", pcwzPackage, hr);
		MessageBox(GetDesktopWindow(), wzError, L"Package Loader Error", MB_ICONERROR | MB_OK);
	}

	__delete pMIDIFile;
	RStrRelease(rstrFile);
	return hr;
}

HRESULT CInfiniteWolfenstein::CreateGLFont (ISimbeyInterchangeFile* pFont)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeFile> srFont;
	TStackRef<ISimbeyInterchangeFileLayer> srDefaultSize;
	USHORT nCanvas, xDefault, yDefault;
	RECT rcSize;

	Check(pFont->FindLayer(L"!.png", &srDefaultSize, NULL));
	Check(srDefaultSize->GetPosition(&rcSize));
	xDefault = static_cast<USHORT>(rcSize.right - rcSize.left);
	yDefault = static_cast<USHORT>(rcSize.bottom - rcSize.top);

	Check(sifCreateNew(&srFont));
	pFont->GetCanvasPixelSize(&nCanvas);
	srFont->SetCanvasPixelSize(nCanvas);

	for(INT i = 0; i < 94; i++)
	{
		WCHAR wzGlyph[8];
		TStackRef<ISimbeyInterchangeFileLayer> srGlyph, srNew;

		Check(Formatting::TPrintF(wzGlyph, ARRAYSIZE(wzGlyph), NULL, L"%c.png", i + 32));
		if(SUCCEEDED(pFont->FindLayer(wzGlyph, &srGlyph, NULL)))
			Check(srFont->ImportLayer(srGlyph, &srNew));
		else
		{
			Check(srFont->AddLayer(xDefault, yDefault, &srNew, NULL));
			Check(srNew->SetName(wzGlyph));
			srNew->SetPosition(nCanvas - xDefault, nCanvas - yDefault);
		}

		DWORD idLayer = srNew->GetLayerID();
		if(idLayer != i + 1)
			Check(srFont->ChangeLayerID(idLayer, i + 1));

		PBYTE pBits;
		DWORD cb;
		Check(srNew->GetBitsPtr(&pBits, &cb));
		for(DWORD i = 0; i < cb; i += 4)
		{
			if(0 < pBits[i+3])
			{
				pBits[i+0] = 255;
				pBits[i+1] = 255;
				pBits[i+2] = 255;
			}
		}
	}

	m_pFont = srFont.Detach();

Cleanup:
	if(srFont)
		srFont->Close();
	return hr;
}
