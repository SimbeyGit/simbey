#pragma once

#include "Library\Core\BaseUnknown.h"
#include "Library\Core\RStrMap.h"
#include "Library\Window\BaseWindow.h"
#include "Published\QuadooVM.h"
#include "IScreen.h"

interface IJSONValue;
interface IJSONObject;
interface IJSONArray;

class CMOMCombatDemo :
	public CBaseUnknown,
	public CBaseWindow,
	public CQuadooObjectImpl,
	public MIDI::INotifyFinished,
	public IScreenHost
{
protected:
	HINSTANCE m_hInstance;

	IJSONObject* m_pPlacements;

	CInteractiveSurface* m_pSurface;
	CSIFPackage* m_pPackage;

	MIDI::CPlayer m_player;
	BOOL m_fActive;

	FMOD::System* m_pSystem;
	TRStrMap<FMOD::Sound*> m_mapSounds;
	TRStrMap<FMOD::Sound*> m_mapIntro;

	bool m_fKeys[256];

	ISimbeyInterchangeFileFont* m_pYellowFont;
	ISimbeyInterchangeFileFont* m_pSmallYellowFont;

	IQuadooVM* m_pVM;
	IScreen* m_pScreen;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IOleWindow)
		UNK_INTERFACE(IBaseWindow)
		UNK_INTERFACE(IQuadooObject)
		UNK_INTERFACE(IScreenHost)
	END_UNK_MAP

	BEGIN_WM_MAP
		HANDLE_WM(WM_CREATE, OnCreate)
		HANDLE_WM(WM_PAINT, OnPaint)
		HANDLE_WM(WM_ERASEBKGND, OnEraseBackground)
		HANDLE_WM(WM_SIZE, OnSize)
		HANDLE_WM(WM_CLOSE, OnClose)
		HANDLE_WM(WM_ACTIVATE, OnActivate)
		HANDLE_WM(WM_MOUSEMOVE, m_pSurface->ProcessLayerInput)
		HANDLE_WM(WM_LBUTTONDOWN, m_pSurface->ProcessLayerInput)
		HANDLE_WM(WM_LBUTTONUP, m_pSurface->ProcessLayerInput)
		HANDLE_WM(WM_RBUTTONDOWN, m_pSurface->ProcessLayerInput)
		HANDLE_WM(WM_RBUTTONUP, m_pSurface->ProcessLayerInput)
		HANDLE_WM(WM_KEYDOWN, OnKeyDown)
		HANDLE_WM(WM_KEYUP, OnKeyUp)
		HANDLE_WM(WM_SETCURSOR, OnSetCursor)
	END_WM_MAP

	CMOMCombatDemo (HINSTANCE hInstance);
	virtual ~CMOMCombatDemo ();

	static HRESULT Register (HINSTANCE hInstance);
	static HRESULT Unregister (HINSTANCE hInstance);

	HRESULT Initialize (INT nWidth, INT nHeight, INT nCmdShow);

	VOID Run (VOID);

	// IQuadooObject
	virtual HRESULT STDMETHODCALLTYPE Invoke (__in_opt IQuadooVM* pVM, RSTRING rstrMethod, QuadooVM::QVPARAMS* pqvParams, __out QuadooVM::QVARIANT* pqvResult);

	// IScreenHost
	virtual bool IsPressed (BYTE nKey);
	virtual HRESULT LoadMIDI (ISequentialStream* pstmMIDI, const ULARGE_INTEGER* puliSize, MIDI::CFile* pmidiFile);
	virtual HRESULT PlayMIDI (MIDI::CFile* pmidiFile, __in_opt MIDI::MIDI_RESUME* pcmResume);
	virtual HRESULT StopMIDI (__out_opt MIDI::MIDI_RESUME* pmResume);
	virtual HRESULT FindSound (RSTRING rstrValue, FMOD::Sound** ppSound);
	virtual HRESULT FindIntroSound (RSTRING rstrValue, FMOD::Sound** ppSound);
	virtual VOID PlaySound (FMOD_CHANNELINDEX idxChannel, FMOD::Sound* pSound, bool fPaused, FMOD::Channel** ppChannel);
	virtual VOID SwitchToScreen (IScreen* pScreen);
	virtual IQuadooVM* GetVM (VOID) { return m_pVM; }
	virtual HRESULT GetData (PCWSTR pcwzName, __deref_out IJSONObject** ppData);

protected:
	virtual HINSTANCE GetInstance (VOID);
	virtual VOID OnFinalDestroy (HWND hwnd);

	virtual HRESULT FinalizeAndShow (DWORD dwStyle, INT nCmdShow);

	DECL_WM_HANDLER(OnCreate);
	DECL_WM_HANDLER(OnPaint);
	DECL_WM_HANDLER(OnEraseBackground);
	DECL_WM_HANDLER(OnSize);
	DECL_WM_HANDLER(OnClose);
	DECL_WM_HANDLER(OnActivate);
	DECL_WM_HANDLER(OnKeyDown);
	DECL_WM_HANDLER(OnKeyUp);
	DECL_WM_HANDLER(OnSetCursor);

	// INotifyFinished
	virtual VOID OnNotifyFinished (MIDI::CPlayer* pPlayer, BOOL fCompleted);

	HRESULT LoadPackage (VOID);
	HRESULT LoadSounds (VOID);
	HRESULT LoadSoundFiles (PCWSTR pcwzSounds, INT cchSounds, TRStrMap<FMOD::Sound*>& mapSounds);
	HRESULT LoadScript (VOID);
};
