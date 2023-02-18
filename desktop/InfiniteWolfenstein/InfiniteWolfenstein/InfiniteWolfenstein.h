#pragma once

#include "Library\Core\RefArray.h"
#include "Library\Core\BaseUnknown.h"
#include "Library\Util\RString.h"
#include "Library\Window\BaseGLWindow.h"
#include "Library\Spatial\Frustum.h"
#include "Library\Spatial\Camera.h"
#include "Library\MIDIPlayer.h"
#include "fmod.hpp"
#include "fmod_errors.h"
#include "Rooms.h"
#include "Models.h"
#include "LevelGenerator.h"

interface ISimbeyInterchangeFile;
interface ISimbeyInterchangeFileGLFont;

class CSIFPackage;
class CInfiniteWolfenstein;

interface IGameInterface
{
	virtual VOID DrawFrame (VOID) = 0;
	virtual VOID UpdateFrame (VOID) = 0;
	virtual VOID OnMusicStopped (VOID) = 0;
	virtual BOOL OnKeyDown (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult) = 0;
	virtual BOOL OnKeyUp (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult) = 0;
};

class CLevelRenderer : public IGameInterface
{
private:
	CInfiniteWolfenstein* m_pGame;

	CLevelGenerator* m_pGenerator;
	CWallTextures* m_pWalls;
	CModels* m_pModels;
	TRefArray<CDungeonRegion> m_aCache;
	CFrustum m_frustum;
	CCamera m_camera;
	INT m_nCompassDir;
	ISimbeyInterchangeFileLayer* m_pCompassFrame;

	TArray<DBLRECT> m_aSolids;			// Temporary Cache
	TArray<CModelEntity*> m_aModels;	// Temporary Cache

	WCHAR m_wzLevelLabel[32];
	WCHAR m_wzScoreLabel[40];
	WCHAR m_wzKeysLabel[40];
	INT m_xRegion, m_zRegion;

	ULONG m_nPoints;
	INT m_cGoldKeys, m_cSilverKeys;

	GLfloat m_rgLockedDoorColor[3];
	WCHAR m_wzLockedDoorLabel[64];
	INT m_cDrawLockedDoorTicks;

public:
	CLevelRenderer (CInfiniteWolfenstein* pGame);
	~CLevelRenderer ();

	VOID AttachLevelGenerator (CLevelGenerator* pGenerator);
	HRESULT StartGame (INT nLevel, INT xRegion, INT zRegion, BOOL fSetCamera);
	HRESULT PrepareLevel (INT nLevel, INT xRegion, INT zRegion, BOOL fSetCamera);
	VOID RenderRegion (CDungeonRegion* pRegion);
	HRESULT PlaySound (PCWSTR pcwzSound, INT cchSound, DPOINT& dpSound);
	HRESULT ActivateElevator (INT x, INT z);
	VOID GetCurrentRegion (__out INT& xRegion, __out INT& zRegion);
	VOID AddPoints (INT nPoints);
	VOID AddGoldKey (VOID);
	VOID AddSilverKey (VOID);
	bool ConsumeGoldKey (VOID);
	bool ConsumeSilverKey (VOID);
	VOID ShowLockedDoorMessage (PCWSTR pcwzMessage, BYTE bRed, BYTE bGreen, BYTE bBlue);
	BOOL CheckCollisionsWithEntity (CEntity* pEntity);

	// IGameInterface
	virtual VOID DrawFrame (VOID);
	virtual VOID UpdateFrame (VOID);
	virtual VOID OnMusicStopped (VOID);
	virtual BOOL OnKeyDown (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult);
	virtual BOOL OnKeyUp (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

private:
	VOID UpdateCompass (VOID);
	VOID DrawCompass (VOID);
	VOID UpdateViewingTiles (VOID);
	VOID GetRegionPosition (INT& xRegion, INT& zRegion);
	HRESULT GetDungeonRegion (INT xRegion, INT zRegion, __deref_out CDungeonRegion** ppRegion);
	HRESULT GetCurrentRegionMusic (RSTRING* prstrMusicW);
	HRESULT UpdateKeysLabel (VOID);
	VOID MoveCamera (DOUBLE dblMove, DOUBLE dblDirRadians);
	VOID CheckItemPickup (VOID);
	VOID RenderFloor (FLOAT x, FLOAT y, FLOAT z, sysint idxFloor);
	VOID RenderCeiling (FLOAT x, FLOAT y, FLOAT z, sysint idxCeiling);
	VOID RenderSides (BLOCK_DATA* pbRegion, sysint* pidxSides, INT xBlock, INT zBlock, FLOAT x, FLOAT z);
};

class CLevelSelector : public IGameInterface
{
private:
	CInfiniteWolfenstein* m_pGame;
	TArray<INT> m_aLevels;
	sysint m_idxSelected;

public:
	CLevelSelector (CInfiniteWolfenstein* pGame);
	~CLevelSelector ();

	HRESULT AddLevel (INT nLevel, BOOL fSelected);

	// IGameInterface
	virtual VOID DrawFrame (VOID);
	virtual VOID UpdateFrame (VOID);
	virtual VOID OnMusicStopped (VOID);
	virtual BOOL OnKeyDown (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult);
	virtual BOOL OnKeyUp (UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult);
};

class CInfiniteWolfenstein :
	public CBaseUnknown,
	public CBaseGLWindow,
	public MIDI::INotifyFinished
{
public:
	static HRESULT Register (HINSTANCE hInstance);

private:
	HINSTANCE m_hInstance;
	BOOL m_fActive;

	MIDI::CPlayer m_player;
	UINT m_msgNotifyFinished;

	CWallTextures* m_pWalls;
	CWallThemes* m_pWallThemes;
	CChunkThemes* m_pChunkThemes;
	CLevels* m_pLevels;
	CRooms* m_pRooms, *m_pSpecial;
	CModels* m_pModels;
	CLevelGenerator* m_pGenerator;

	TRStrMap<MIDI::CFile*> m_mapMusic;
	RSTRING m_rstrMusic;

	FMOD::System* m_pSystem;
	TRStrMap<FMOD::Sound*> m_mapSounds;

	ISimbeyInterchangeFile* m_pFont;
	ISimbeyInterchangeFile* m_pCompass;
	UINT m_nCompass;

	CLevelRenderer m_renderer;
	CLevelSelector m_selector;
	IGameInterface* m_pActive;

public:
	SIZE m_szWindow;
	BOOL m_fTrackMouse;
	ISimbeyInterchangeFileGLFont* m_pGLFont;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IOleWindow)
		UNK_INTERFACE(IBaseWindow)
	END_UNK_MAP

	BEGIN_WM_MAP
		HANDLE_WM(WM_KEYDOWN, OnKeyDown)
		HANDLE_WM(WM_KEYUP, OnKeyUp)
		HANDLE_WM(WM_RBUTTONDOWN, OnRButtonDown)
		HANDLE_WM(WM_ACTIVATE, OnActivate)
		HANDLE_WM(WM_SIZE, OnSize)
		HANDLE_WM(WM_CLOSE, OnClose)
		HANDLE_WM(WM_SYSCOMMAND, OnSysCommand)
		HANDLE_WM(m_msgNotifyFinished, OnNotifyFinishedMsg)
		DELEGATE_PARENT(__super)
	END_WM_MAP

	CInfiniteWolfenstein (HINSTANCE hInstance);
	virtual ~CInfiniteWolfenstein ();

	HRESULT Initialize (LPWSTR lpCmdLine, INT nCmdShow);
	VOID Run (VOID);

	HRESULT PlayMusic (RSTRING rstrMusicW);
	BOOL IsPlayingMusic (RSTRING rstrMusicW);
	HRESULT StopMusic (VOID);

	HRESULT PlaySound (PCWSTR pcwzSound, INT cchSound, FLOAT rVolume);

	CLevelRenderer* GetLevelRenderer (VOID) { return &m_renderer; }
	CLevelSelector* GetLevelSelector (VOID) { return &m_selector; }
	VOID SelectInterface (IGameInterface* pInterface);

	HRESULT GetCompassFrame (INT nDir, __deref_out ISimbeyInterchangeFileLayer** ppCompassFrame);
	inline UINT GetCompassTexture (VOID) { return m_nCompass; }

protected:
	// CBaseWindow
	virtual HINSTANCE GetInstance (VOID);
	virtual VOID OnFinalDestroy (HWND hwnd);

	// CBaseGLWindow
	virtual HRESULT CreateTextures (VOID);
	virtual VOID Draw (VOID);
	virtual VOID RequestModeChange (BOOL fFullScreen);

	// INotifyFinished
	virtual VOID OnNotifyFinished (MIDI::CPlayer* pPlayer, BOOL fCompleted);

	DECL_WM_HANDLER(OnKeyDown);
	DECL_WM_HANDLER(OnKeyUp);
	DECL_WM_HANDLER(OnRButtonDown);
	DECL_WM_HANDLER(OnActivate);
	DECL_WM_HANDLER(OnSize);
	DECL_WM_HANDLER(OnClose);
	DECL_WM_HANDLER(OnSysCommand);
	DECL_WM_HANDLER(OnNotifyFinishedMsg);

	HRESULT LoadPackage (PCWSTR pcwzPackage, BOOL fRequireAll);
	HRESULT CreateGLFont (ISimbeyInterchangeFile* pFont);
};
