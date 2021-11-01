#pragma once

#include "Neurone.h"

#define	SPLITTER_RADIUS				(DEFAULT_RADIUS - 1)

class CSplitter : public CNeurone
{
public:
	CSplitter ();
	~CSplitter ();

	// INetCycleProcessor
	virtual VOID SendPulses (VOID);
	virtual VOID CheckThresholds (VOID);

	// INetObject
	virtual HRESULT GetObjectClass (LPSTR lpszClass, INT cchMaxClass, __out INT* pcchClass);
	virtual VOID ReceiveValue (FLOAT fValue, ULONG iPin);
};