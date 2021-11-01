#pragma once

#include "BiasNeurone.h"

class CSigmoidNeurone : public CBiasNeurone
{
private:
	FLOAT m_rBias;

public:
	CSigmoidNeurone ();
	~CSigmoidNeurone ();

	// INetCycleProcessor
	virtual VOID SendPulses (VOID);

	// INetObject
	virtual HRESULT GetObjectClass (LPSTR lpszClass, INT cchMaxClass, __out INT* pcchClass);
};
