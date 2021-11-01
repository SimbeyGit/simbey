#include <math.h>
#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "NeuralAPI.h"
#include "SigmoidNeurone.h"

CSigmoidNeurone::CSigmoidNeurone ()
{
}

CSigmoidNeurone::~CSigmoidNeurone ()
{
}

// INetCycleProcessor

VOID CSigmoidNeurone::SendPulses (VOID)
{
	if(m_dwState & NSTATE_EXCITED)
		m_dwState &= ~NSTATE_EXCITED;

	// Always send the pulses for sigmoid.
	m_Connections.SendPulses(1.0f / (1.0f + exp(-m_fFinalValue)));
	m_fFinalValue = 0.0f;
}

// INetObject

HRESULT CSigmoidNeurone::GetObjectClass (LPSTR lpszClass, INT cchMaxClass, __out INT* pcchClass)
{
	return TStrCchCpyLen(lpszClass, cchMaxClass, SLP("CSigmoidNeurone"), pcchClass);
}
