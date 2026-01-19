#pragma once

#include "Core\Array.h"

namespace MIDI
{
#pragma pack(push, 1)

	struct RIFF_CHUNK
	{
		CHAR id[4];
		DWORD cbSize;
	};

	struct RIFF_CHUNK_HEADER
	{
		UINT cbChunkSize;
		CHAR format[4];
		RIFF_CHUNK Chunk;
	};

	struct MIDI_HEADER
	{
		UINT size;		// always 6 in big-endian format
		USHORT format;	// big-endian format
		USHORT tracks;	// number of tracks, big-endian
		USHORT ticks;	// number of ticks per quarter note, big-endian
	};

	struct MIDI_TRACK
	{
		CHAR id[4];		// identifier "MTrk"
		DWORD cbSize;	// track length, big-endian
	};

	struct MIDI_RESUME
	{
		sysint nResumePoint;
		ULONG nPPQN;
	};

#pragma pack(pop)

	struct TRACKMSG
	{
		ULONGLONG absTime;
		ULONG msg;
	};

	struct MERGEDMSG
	{
		ULONG time;
		ULONG msg;
		ULONG_PTR track;
	};

	class CPlayer;

	interface __declspec(uuid("7A32A34D-5162-43ca-95A2-2A9BF75FA686")) INotifyFinished : IUnknown
	{
		virtual VOID OnNotifyFinished (CPlayer* pPlayer, BOOL fCompleted) = 0;
	};

	class CTrack
	{
	public:
		TArray<TRACKMSG> m_aMessages;
	};

	class CFile
	{
	public:
		USHORT m_cTicks;
		TArray<CTrack*> m_aTracks;

	public:
		~CFile ()
		{
			for(sysint i = 0; i < m_aTracks.Length(); i++)
				__delete m_aTracks[i];
		}
	};

	class CPlayer
	{
	private:
		CRITICAL_SECTION m_cs;
		HMIDIOUT m_hMidiOut;
		TArray<MERGEDMSG> m_aMessages;
		USHORT m_cTicks;
		ULONG m_nPPQN;
		HANDLE m_hThread;
		HANDLE m_hStop;
		INotifyFinished* m_pNotify;
		sysint m_nResumePoint;
		DWORD m_msSwitchDelay;
		MIDI_RESUME* m_pmResume;

	public:
		CPlayer ();
		~CPlayer ();

		HRESULT Initialize (VOID);
		HRESULT StartFromFile (INotifyFinished* pNotify, CFile* pFile, __in_opt const MIDI_RESUME* pcmResume = NULL);
		HRESULT StartFromData (INotifyFinished* pNotify, const BYTE* pcbData, ULONG cbData);
		HRESULT Stop (__out_opt MIDI_RESUME* pmResume = NULL);
		HRESULT SwitchTo (CFile* pFile, DWORD msDelay, __in_opt const MIDI_RESUME* pcmResume = NULL);
		HRESULT PersistToStream (ISequentialStream* pStream);

	private:
		HRESULT StartInternal (INotifyFinished* pNotify);
		HRESULT ReadAndInsertMessage (CFile* pFile, sysint* prgIndex, sysint cTracks, __inout ULONGLONG& ullLastTime);
		VOID NotifyDone (BOOL fCompleted);
		VOID PlayAsync (VOID);
		static DWORD CALLBACK _PlayAsync (PVOID pvParam);
	};

	HRESULT LoadMIDI (ISequentialStream* pStream, const ULARGE_INTEGER* puliSize, CFile* pFile);
}
