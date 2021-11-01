#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "NeuralAPI.h"
#include "Splitter.h"

CSplitter::CSplitter ()
{
	m_iRadius = SPLITTER_RADIUS;
}

CSplitter::~CSplitter ()
{
}

// INetCycleProcessor

VOID CSplitter::SendPulses (VOID)
{
	m_dwState &= ~NSTATE_EXCITED;
	m_fFinalValue = 0.0f;
}

VOID CSplitter::CheckThresholds (VOID)
{
	if(m_fAccumulator != 0.0f)
		m_dwState |= NSTATE_EXCITED;
	m_fFinalValue = m_fAccumulator;
	m_fAccumulator = 0.0f;
}

// INetObject

HRESULT CSplitter::GetObjectClass (LPSTR lpszClass, INT cchMaxClass, __out INT* pcchClass)
{
	return TStrCchCpyLen(lpszClass, cchMaxClass, SLP("CSplitter"), pcchClass);
}

VOID CSplitter::ReceiveValue (FLOAT fValue, ULONG iPin)
{
	UNREFERENCED_PARAMETER(iPin);

	m_fAccumulator += fValue;
	m_Connections.SendPulses(fValue);
}