#include <windows.h>
#include "Core\CoreDefs.h"
#include "Core\StringCore.h"
#include "Core\Endian.h"
#include "MIDIPlayer.h"

#define	TEMPO_EVT			1

namespace MIDI
{
	CPlayer::CPlayer () :
		m_hMidiOut(NULL),
		m_cTicks(0),
		m_nPPQN(0),
		m_hThread(NULL),
		m_hStop(NULL),
		m_pNotify(NULL),
		m_nResumePoint(0),
		m_msSwitchDelay(0),
		m_pmResume(NULL)
	{
		InitializeCriticalSection(&m_cs);
	}

	CPlayer::~CPlayer ()
	{
		Stop();
		Assert(NULL == m_pNotify);
		SafeCloseHandle(m_hStop);
		if(m_hMidiOut)
			midiOutClose(m_hMidiOut);
		DeleteCriticalSection(&m_cs);
	}

	HRESULT CPlayer::Initialize (VOID)
	{
		HRESULT hr;

		EnterCriticalSection(&m_cs);

		CheckIf(NULL != m_hMidiOut, E_UNEXPECTED);

		m_hStop = CreateEvent(NULL, TRUE, FALSE, NULL);
		CheckIfGetLastError(NULL == m_hStop);

		CheckIf(MMSYSERR_NOERROR != midiOutOpen(&m_hMidiOut, 0, 0, 0, CALLBACK_NULL), E_FAIL);

		hr = S_OK;

	Cleanup:
		LeaveCriticalSection(&m_cs);
		return hr;
	}

	HRESULT CPlayer::StartFromFile (INotifyFinished* pNotify, CFile* pFile, __in_opt const MIDI_RESUME* pcmResume)
	{
		HRESULT hr;
		sysint* prgIndex = NULL, cTracks;
		ULONGLONG ullLastTime = 0;

		EnterCriticalSection(&m_cs);

		CheckIf(NULL == pNotify, E_INVALIDARG);
		CheckIf(NULL != m_pNotify, E_UNEXPECTED);

		cTracks = pFile->m_aTracks.Length();
		prgIndex = __new sysint[cTracks];

		CheckAlloc(prgIndex);
		ZeroMemory(prgIndex, sizeof(sysint) * cTracks);

		m_aMessages.Clear();

		m_cTicks = pFile->m_cTicks;

		if(pcmResume)
		{
			m_nPPQN = pcmResume->nPPQN;
			m_nResumePoint = pcmResume->nResumePoint;
		}
		else
		{
			m_nPPQN = 500000 / static_cast<ULONG>(m_cTicks);
			m_nResumePoint = 0;
		}

		do
		{
			Check(ReadAndInsertMessage(pFile, prgIndex, cTracks, ullLastTime));
		} while(S_OK == hr);

		Check(StartInternal(pNotify));

	Cleanup:
		LeaveCriticalSection(&m_cs);
		SafeDeleteArray(prgIndex);
		return hr;
	}

	HRESULT CPlayer::StartFromData (INotifyFinished* pNotify, const BYTE* pcbData, ULONG cbData)
	{
		HRESULT hr;
		sysint cMessages;

		EnterCriticalSection(&m_cs);

		CheckIf(NULL == pNotify, E_INVALIDARG);
		CheckIf(NULL != m_pNotify, E_UNEXPECTED);

		CheckIf(cbData < sizeof(m_cTicks) + sizeof(m_nPPQN) + sizeof(m_nResumePoint), HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
		CopyMemory(&m_cTicks, pcbData, sizeof(m_cTicks));
		pcbData += sizeof(m_cTicks); cbData -= sizeof(m_cTicks);
		CopyMemory(&m_nPPQN, pcbData, sizeof(m_nPPQN));
		pcbData += sizeof(m_nPPQN); cbData -= sizeof(m_nPPQN);
		CopyMemory(&m_nResumePoint, pcbData, sizeof(m_nResumePoint));
		pcbData += sizeof(m_nResumePoint); cbData -= sizeof(m_nResumePoint);

		CheckIf(0 != cbData % sizeof(MERGEDMSG), HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));

		cMessages = cbData / sizeof(MERGEDMSG);
		CheckIf(0 == cMessages, S_FALSE);

		for(sysint i = 0; i < cMessages; i++)
		{
			MERGEDMSG* pMessage;

			Check(m_aMessages.AppendSlot(&pMessage));
			CopyMemory(pMessage, pcbData, sizeof(MERGEDMSG));
			pcbData += sizeof(MERGEDMSG);
			cbData -= sizeof(MERGEDMSG);
		}

		Check(StartInternal(pNotify));

	Cleanup:
		LeaveCriticalSection(&m_cs);
		return hr;
	}

	HRESULT CPlayer::Stop (__out_opt MIDI_RESUME* pmResume)
	{
		HRESULT hr;
		HANDLE hThread;

		EnterCriticalSection(&m_cs);
		hThread = m_hThread;
		m_hThread = NULL;
		LeaveCriticalSection(&m_cs);

		if(hThread)
		{
			Assert(NULL == m_pmResume);
			m_pmResume = pmResume;

			SetEvent(m_hStop);
			WaitForSingleObject(hThread, INFINITE);
			CloseHandle(hThread);
			hr = S_OK;
		}
		else
		{
			if(pmResume)
			{
				pmResume->nPPQN = 0;
				pmResume->nResumePoint = 0;
			}

			hr = S_FALSE;
		}

		return hr;
	}

	HRESULT CPlayer::SwitchTo (CFile* pFile, DWORD msDelay, __in_opt const MIDI_RESUME* pcmResume)
	{
		HRESULT hr;
		TStackRef<INotifyFinished> srNotify;

		EnterCriticalSection(&m_cs);
		if(m_pNotify)
		{
			m_msSwitchDelay = msDelay;
			srNotify.Attach(m_pNotify);
			m_pNotify = NULL;
			LeaveCriticalSection(&m_cs);

			// The current notification callback won't receive a
			// call until the new file is completed.
			Check(Stop());
			Check(StartFromFile(srNotify, pFile, pcmResume));

	Cleanup:
			if(FAILED(hr))
			{
				EnterCriticalSection(&m_cs);
				m_msSwitchDelay = 0;
				LeaveCriticalSection(&m_cs);
			}
		}
		else
		{
			LeaveCriticalSection(&m_cs);

			// Can't switch to a file if nothing is currently playing!
			hr = S_FALSE;
		}
		return hr;
	}

	HRESULT CPlayer::PersistToStream (ISequentialStream* pStream)
	{
		HRESULT hr;
		ULONG cb;
		MERGEDMSG* pMessages;
		sysint cMessages;

		EnterCriticalSection(&m_cs);

		CheckIf(m_pNotify, HRESULT_FROM_WIN32(ERROR_DEVICE_IN_USE));

		Check(pStream->Write(&m_cTicks, sizeof(m_cTicks), &cb));
		Check(pStream->Write(&m_nPPQN, sizeof(m_nPPQN), &cb));
		Check(pStream->Write(&m_nResumePoint, sizeof(m_nResumePoint), &cb));

		m_aMessages.GetData(&pMessages, &cMessages);
		Check(pStream->Write(pMessages, static_cast<ULONG>(cMessages * sizeof(MERGEDMSG)), &cb));

	Cleanup:
		LeaveCriticalSection(&m_cs);
		return hr;
	}

	HRESULT CPlayer::StartInternal (INotifyFinished* pNotify)
	{
		HRESULT hr;
		DWORD idThread;
		BOOL fSetNotifyPtr = FALSE;

		CheckIf(NULL == m_hMidiOut, E_UNEXPECTED);

		SetInterface(m_pNotify, pNotify);
		fSetNotifyPtr = TRUE;

		ResetEvent(m_hStop);

		m_hThread = CreateThread(NULL, 0, _PlayAsync, this, CREATE_SUSPENDED, &idThread);
		CheckIfGetLastError(NULL == m_hThread);

		// Boost the thread priority by 1.
		SetThreadPriority(m_hThread, GetThreadPriority(m_hThread) + 1);

		ResumeThread(m_hThread);

		hr = S_OK;

	Cleanup:
		if(FAILED(hr) && fSetNotifyPtr)
			SafeRelease(m_pNotify);
		return hr;
	}

	HRESULT CPlayer::ReadAndInsertMessage (CFile* pFile, sysint* prgIndex, sysint cTracks, __inout ULONGLONG& ullLastTime)
	{
		HRESULT hr;
		ULONGLONG absNextTime = (ULONGLONG)-1;
		sysint nNextTrack = -1;
		MERGEDMSG* pMsg;

		for(sysint i = 0; i < cTracks; i++)
		{
			if(prgIndex[i] < pFile->m_aTracks[i]->m_aMessages.Length())
			{
				ULONGLONG absTime = pFile->m_aTracks[i]->m_aMessages[prgIndex[i]].absTime;
				if(absTime < absNextTime)
				{
					absNextTime = absTime;
					nNextTrack = i;
				}
			}
		}

		CheckIfIgnore(-1 == nNextTrack, S_FALSE);

		Check(m_aMessages.AppendSlot(&pMsg));
		pMsg->msg = pFile->m_aTracks[nNextTrack]->m_aMessages[prgIndex[nNextTrack]++].msg;
		pMsg->time = static_cast<ULONG>(absNextTime - ullLastTime);
		pMsg->track = nNextTrack;

		ullLastTime = absNextTime;

	Cleanup:
		return hr;
	}

	VOID CPlayer::NotifyDone (BOOL fCompleted)
	{
		INotifyFinished* pNotify;
		EnterCriticalSection(&m_cs);
		pNotify = m_pNotify;
		m_pNotify = NULL;
		LeaveCriticalSection(&m_cs);
		if(pNotify)
		{
			pNotify->OnNotifyFinished(this, fCompleted);
			pNotify->Release();
		}
	}

	VOID CPlayer::PlayAsync (VOID)
	{
		HMIDIOUT hMidiOut = m_hMidiOut;
		MERGEDMSG* pMessages;
		sysint nStartPoint = m_nResumePoint, cMessages;
		BOOL fCompleted = TRUE;

		m_nResumePoint = 0;
		m_aMessages.GetData(&pMessages, &cMessages);

		timeBeginPeriod(1);

		EnterCriticalSection(&m_cs);
		if(0 < m_msSwitchDelay)
		{
			DWORD msDelay = m_msSwitchDelay;
			m_msSwitchDelay = 0;

			LeaveCriticalSection(&m_cs);

			if(WAIT_TIMEOUT != WaitForSingleObject(m_hStop, msDelay))
			{
				fCompleted = FALSE;
				goto Cleanup;
			}
		}
		else
			LeaveCriticalSection(&m_cs);

		for(sysint n = nStartPoint; n < cMessages; n++)
		{
			ULONG msg = pMessages[n].msg;

			if(0 != pMessages[n].time)
			{
				if(WAIT_TIMEOUT != WaitForSingleObject(m_hStop, MulDiv(pMessages[n].time, m_nPPQN, 1000)))
				{
					m_nResumePoint = n;
					fCompleted = FALSE;
					goto Cleanup;
				}
			}

			if(msg & 0xff000000)	// Tempo change
			{
				msg = msg & 0x00ffffff;
				m_nPPQN = msg / static_cast<ULONG>(m_cTicks);
			}
			else
				midiOutShortMsg(hMidiOut, msg);
		}

	Cleanup:
		timeEndPeriod(1);

		midiOutReset(hMidiOut);

		if(m_pmResume)
		{
			m_pmResume->nPPQN = m_nPPQN;
			m_pmResume->nResumePoint = m_nResumePoint;

			// Set m_pmResume back to NULL before calling NotifyDone().
			m_pmResume = NULL;
		}

		NotifyDone(fCompleted);
	}

	DWORD CALLBACK CPlayer::_PlayAsync (PVOID pvParam)
	{
		CPlayer* pThis = reinterpret_cast<CPlayer*>(pvParam);
		pThis->PlayAsync();
		return 0;
	}

	HRESULT ReadMIDIVar (__inout PBYTE& pbPtr, const BYTE* pcbEnd, __out ULONG* pnValue)
	{
		HRESULT hr = S_OK;
		ULONG var = 0;
		BYTE c;

		do
		{
			CheckIf(pbPtr == pcbEnd, HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
			c = *pbPtr++;
			var = (var << 7) + (c & 0x7F);
		} while(c & 0x80);

		*pnValue = var;

	Cleanup:
		return hr;
	}

	HRESULT LoadMIDITrack (ISequentialStream* pStream, DWORD cbSize, CFile* pFile)
	{
		HRESULT hr;
		CTrack* pTrack = __new CTrack;
		PBYTE pbData = NULL, pbPtr, pbEnd;
		ULONGLONG absTime = 0;
		TRACKMSG* pMessage;
		DWORD cb;
		BOOL fInserted = FALSE;
		BYTE bLastCmd = 0;

		CheckAlloc(pTrack);
		Check(pFile->m_aTracks.Append(pTrack));
		fInserted = TRUE;

		pbData = __new BYTE[cbSize];
		CheckAlloc(pbData);
		Check(pStream->Read(pbData, cbSize, &cb));
		CheckIf(cb != cbSize, HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));

		pbPtr = pbData;
		pbEnd = pbData + cbSize;

		while(pbPtr < pbEnd)
		{
			BYTE cmd;
			ULONG nTime, msg;

			Check(ReadMIDIVar(pbPtr, pbEnd, &nTime));

			// If we're at the end of the stream, but the time is zero
			// without any command, then just consider the track loaded.
			if(pbPtr == pbEnd && 0 == nTime)
				break;

			CheckIf(pbPtr >= pbEnd, HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
			cmd = *pbPtr;

			if(cmd == 0xFF)					// Meta event
			{
				ULONG len;
				BYTE meta;

				pbPtr++;

				CheckIf(pbPtr >= pbEnd, HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
				meta = *pbPtr++;

				switch(meta)
				{
				case 0x51:
					{
						BYTE a, b, c;

						Check(ReadMIDIVar(pbPtr, pbEnd, &len));	// Get the length byte, should be 3
						CheckIf(len != 3, HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
						CheckIf(pbEnd - pbPtr < 3, HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));

						a = *pbPtr++;
						b = *pbPtr++;
						c = *pbPtr++;

						msg = static_cast<ULONG>(TEMPO_EVT << 24) |
							static_cast<ULONG>(a) << 16 |
							static_cast<ULONG>(b) << 8 |
							static_cast<ULONG>(c) << 0;

						Check(pTrack->m_aMessages.AppendSlot(&pMessage));

						pMessage->absTime = absTime + nTime;
						pMessage->msg = msg;
						absTime += nTime;
					}
					break;
				case 0x2f: // end of track
					Check(ReadMIDIVar(pbPtr, pbEnd, &len));	// pbPtr advances past the VLQ
					CheckIf((ULONGLONG)(pbEnd - pbPtr) < len, HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
					absTime += nTime;
					pbPtr = pbEnd;
					break;
				case 0x00:
				case 0x01:
				case 0x02:
				case 0x03:
				case 0x04:
				case 0x05:
				case 0x06:
				case 0x07:
				case 0x21:
				case 0x54:
				case 0x58: // time signature
				case 0x59: // key signature
				case 0x7f:
				default:
					Check(ReadMIDIVar(pbPtr, pbEnd, &len));	// pbPtr advances past the VLQ
					CheckIf((ULONGLONG)(pbEnd - pbPtr) < len, HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
					pbPtr += len;							// skip payload
					break;
				}
			}
			else if(0 == (cmd & 0x80))		// Running mode
			{
				CheckIf(pbPtr >= pbEnd, HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));

				msg = static_cast<ULONG>(bLastCmd) | (static_cast<ULONG>(*pbPtr++) << 8);

				if((bLastCmd & 0xF0) != 0xC0 && (bLastCmd & 0xF0) != 0xD0)
				{
					CheckIf(pbPtr >= pbEnd, HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
					msg |= static_cast<ULONG>(*pbPtr++) << 16;
				}

				Check(pTrack->m_aMessages.AppendSlot(&pMessage));

				pMessage->absTime = absTime + nTime;
				pMessage->msg = msg;
				absTime += nTime;
			}
			else if(0xF0 != (cmd & 0xF0))	// Normal command
			{
				pbPtr++;

				CheckIf(pbPtr >= pbEnd, HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
				msg = static_cast<ULONG>(cmd) | (static_cast<ULONG>(*pbPtr++) << 8);

				if(!((cmd & 0xf0) == 0xc0 || (cmd & 0xf0) == 0xd0))
				{
					CheckIf(pbPtr >= pbEnd, HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
					msg |= static_cast<ULONG>(*pbPtr++) << 16;
				}

				Check(pTrack->m_aMessages.AppendSlot(&pMessage));

				pMessage->absTime = absTime + nTime;
				pMessage->msg = msg;
				absTime += nTime;

				bLastCmd = cmd;
			}
			else
				Check(HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
		}

	Cleanup:
		SafeDeleteArray(pbData);
		if(!fInserted)
			SafeDelete(pTrack);
		return hr;
	}

	HRESULT LoadMIDI (ISequentialStream* pStream, const ULARGE_INTEGER* puliSize, CFile* pFile)
	{
		HRESULT hr;
		DWORD cb;
		CHAR id[4];
		ULARGE_INTEGER uliSize;

		uliSize.QuadPart = puliSize->QuadPart;

		Check(pStream->Read(id, sizeof(id), &cb));
		if(TStrMatchLeftAssert(id, SLP("RIFF")))
		{
			RIFF_CHUNK_HEADER Chunk;

			CheckIf(uliSize.QuadPart <= sizeof(Chunk), HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
			Check(pStream->Read(&Chunk, sizeof(Chunk), &cb));
			uliSize.QuadPart -= sizeof(Chunk);

			CheckIf(!TStrMatchLeftAssert(Chunk.format, SLP("RMID")), HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
			CheckIf(!TStrMatchLeftAssert(Chunk.Chunk.id, SLP("data")), HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));

			CheckIf(uliSize.QuadPart < Chunk.Chunk.cbSize, HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
			uliSize.QuadPart = Chunk.Chunk.cbSize;

			Check(LoadMIDI(pStream, &uliSize, pFile));
		}
		else if(TStrMatchLeftAssert(id, SLP("MThd")))
		{
			MIDI_HEADER Header;

			CheckIf(uliSize.QuadPart <= sizeof(Header), HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
			Check(pStream->Read(&Header, sizeof(Header), &cb));
			uliSize.QuadPart -= sizeof(Header);

			if(!IsBigEndian())
			{
				EndianSwap(Header.size);
				EndianSwap(Header.format);
				EndianSwap(Header.tracks);
				EndianSwap(Header.ticks);
			}

			pFile->m_cTicks = Header.ticks;

			for(USHORT nTrack = 0; nTrack < Header.tracks; nTrack++)
			{
				MIDI_TRACK Track;

				CheckIf(uliSize.QuadPart < sizeof(Track), HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
				Check(pStream->Read(&Track, sizeof(Track), &cb));
				uliSize.QuadPart -= sizeof(Track);

				CheckIf(!TStrMatchLeftAssert(Track.id, SLP("MTrk")), HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));

				if(!IsBigEndian())
					EndianSwap(Track.cbSize);

				Check(LoadMIDITrack(pStream, Track.cbSize, pFile));
				uliSize.QuadPart -= Track.cbSize;
			}
		}
		else
			Check(HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));

	Cleanup:
		return hr;
	}
}
