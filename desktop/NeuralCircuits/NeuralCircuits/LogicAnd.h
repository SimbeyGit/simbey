#pragma once

#include "LogicGate.h"

class CLogicAnd : public CLogicGate
{
public:
    CLogicAnd ();
    virtual ~CLogicAnd ();

	// INetCycleProcessor
	virtual VOID CheckThresholds (VOID);

	// INetObject
	virtual HRESULT GetObjectClass (LPSTR lpszClass, INT cchMaxClass, __out INT* pcchClass);

	// INetDocObject
	virtual VOID DrawForeground (IGrapher* lpGraph);

protected:
    virtual ULONG GetInputPinCount (VOID);
};