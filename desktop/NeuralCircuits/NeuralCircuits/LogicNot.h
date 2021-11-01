#pragma once

#include "LogicGate.h"

class CLogicNot : public CLogicGate
{
public:
	CLogicNot ();
	virtual ~CLogicNot ();

	// INetCycleProcessor
	virtual VOID CheckThresholds (VOID);

	// INetObject
	virtual HRESULT GetObjectClass (LPSTR lpszClass, INT cchMaxClass, __out INT* pcchClass);

	// INetDocObject
	virtual VOID DrawForeground (IGrapher* lpGraph);

protected:
    virtual ULONG GetInputPinCount (VOID);
};
