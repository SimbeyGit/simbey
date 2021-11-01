#pragma once

#include "LogicGate.h"

class CLogicOr : public CLogicGate
{
public:
	CLogicOr ();
	virtual ~CLogicOr ();

	// INetCycleProcessor
	virtual VOID CheckThresholds (VOID);

	// INetObject
	virtual HRESULT GetObjectClass (LPSTR lpszClass, INT cchMaxClass, __out INT* pcchClass);

	// INetDocObject
	virtual VOID DrawForeground (IGrapher* lpGraph);

protected:
	virtual ULONG GetInputPinCount (VOID);

	VOID CalculateShape (FLOAT* fx, FLOAT* fy);
};