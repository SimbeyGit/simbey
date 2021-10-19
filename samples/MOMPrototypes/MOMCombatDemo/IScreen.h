#pragma once

#include "Library\MIDIPlayer.h"
#include "fmod.hpp"
#include "fmod_errors.h"
#include "Package\SIFPackage.h"
#include "InteractiveSurface.h"

interface IQuadooVM;

interface __declspec(uuid("6F377562-14FA-4c57-B41F-DBC24E3F4491")) IScreen : IUnknown
{
	virtual VOID OnDestroy (VOID) = 0;
	virtual VOID OnUpdateFrame (VOID) = 0;
	virtual VOID OnNotifyFinished (BOOL fCompleted) = 0;
	virtual VOID OnChangeActive (BOOL fActive) = 0;
};

interface __declspec(uuid("08274826-E9B1-4b4b-86F3-638940FA06C2")) IScreenHost : IUnknown
{
	virtual bool IsPressed (BYTE nKey) = 0;
	virtual HRESULT LoadMIDI (ISequentialStream* pstmMIDI, const ULARGE_INTEGER* puliSize, MIDI::CFile* pmidiFile) = 0;
	virtual HRESULT PlayMIDI (MIDI::CFile* pmidiFile, __in_opt MIDI::MIDI_RESUME* pcmResume = NULL) = 0;
	virtual HRESULT StopMIDI (__out_opt MIDI::MIDI_RESUME* pmResume = NULL) = 0;
	virtual HRESULT FindSound (RSTRING rstrValue, FMOD::Sound** ppSound) = 0;
	virtual HRESULT FindIntroSound (RSTRING rstrValue, FMOD::Sound** ppSound) = 0;
	virtual VOID PlaySound (FMOD_CHANNELINDEX idxChannel, FMOD::Sound* pSound, bool fPaused, FMOD::Channel** ppChannel) = 0;
	virtual VOID SwitchToScreen (IScreen* pScreen) = 0;
	virtual IQuadooVM* GetVM (VOID) = 0;
	virtual HRESULT GetData (PCWSTR pcwzName, __deref_out IJSONObject** ppData) = 0;
};
