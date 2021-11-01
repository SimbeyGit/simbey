#pragma once

#include "LogicOr.h"

class CLogicXor : public CLogicOr
{
public:
    CLogicXor ();
    virtual ~CLogicXor ();

	// INetCycleProcessor
	virtual VOID CheckThresholds (VOID);

	// INetObject
	virtual HRESULT GetObjectClass (LPSTR lpszClass, INT cchMaxClass, __out INT* pcchClass);

	// INetDocObject
	virtual VOID DrawForeground (IGrapher* lpGraph);

protected:
	virtual ULONG GetInputPinCount (VOID) { return 2; }
};

class CLogicXorN : public CLogicXor
{
private:
	ULONG m_cPins;

public:
    CLogicXorN (ULONG cPins);
    virtual ~CLogicXorN ();

	// INetCycleProcessor
	virtual VOID CheckThresholds (VOID);

	// INetObject
	virtual HRESULT GetObjectClass (LPSTR lpszClass, INT cchMaxClass, __out INT* pcchClass);

protected:
	virtual ULONG GetInputPinCount (VOID) { return m_cPins; }
};
