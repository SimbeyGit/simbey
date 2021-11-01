#pragma once

#include "BaseIONeurone.h"

class COutputNeurone : public CBaseIONeurone
{
public:
	COutputNeurone ();
	~COutputNeurone ();

	// INetObject
	virtual HRESULT GetObjectClass (LPSTR lpszClass, INT cchMaxClass, __out INT* pcchClass);
	virtual VOID GetPosition (INT& x, INT& y);
	virtual VOID ReceiveValue (FLOAT fValue, ULONG iPin);
	virtual ULONG GetInputPin (INT x, INT y);
	virtual BOOL GetInputPinPosition (ULONG iPin, INT& x, INT& y);

	// INeurone
	virtual BOOL GetCurrentValue (FLOAT& fValue);

	// INetDocObject
	virtual VOID DrawForeground (IGrapher* lpGraph);

	// IIONeurone
	virtual ULONG GetPin (VOID);
	virtual VOID SetPin (ULONG iPin);
	virtual EIO_TYPE GetIOType (VOID);

protected:
	virtual BOOL GetAxonPosition (INT& x, INT& y);
};