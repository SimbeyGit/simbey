#pragma once

#include "Neurone.h"

class CBiasNeurone : public CNeurone, public IBiasNeurone
{
private:
	FLOAT m_rBias;

public:
	CBiasNeurone ();
	~CBiasNeurone ();

	// IUnknown
	HRESULT WINAPI QueryInterface (REFIID iid, LPVOID* lplpvObject);
	ULONG WINAPI AddRef (VOID);
	ULONG WINAPI Release (VOID);

	// INetCycleProcessor
	virtual VOID SendPulses (VOID);
	virtual VOID CheckThresholds (VOID);

	// INetObject
	virtual HRESULT GetObjectClass (LPSTR lpszClass, INT cchMaxClass, __out INT* pcchClass);
	virtual VOID GetPosition (INT& x, INT& y);
	virtual VOID ReceiveValue (FLOAT fValue, ULONG iPin);
	virtual ULONG GetInputPin (INT x, INT y);
	virtual BOOL GetInputPinPosition (ULONG iPin, INT& x, INT& y);
	virtual HRESULT Load (INeuralFactory* pFactory, LPNLOADDATA lpLoadData, DWORD cLoadData, LPBYTE lpData, ULONG cbData);
	virtual HRESULT Save (INeuralNet* lpNet, ISequentialStream* lpStream);

	// IBiasNeurone
	virtual FLOAT GetBias (VOID) { return m_rBias; }
	virtual VOID SetBias (FLOAT rBias) { m_rBias = rBias; }
};
