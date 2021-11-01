#pragma once

#include "Library\Core\Map.h"

typedef struct
{
	TArray<INT>* lpaInputs;
	TArray<FLOAT>* lpaWeights;
	CNeurone* lpNeurone;
} BBDATA;

class CBrainBox
{
protected:
	INeuralNet* m_lpNet;

public:
	CBrainBox (INeuralNet* lpNet);
	~CBrainBox ();

	HRESULT Load (LPBYTE lpData, DWORD cbData, BOOL fUseBiasNeuronesForBrainBox);

protected:
	static HRESULT ReadInputs (LPCSTR lpcszInputs, BBDATA* lpbbData);
	static HRESULT ReadWeights (LPCSTR lpcszWeights, BBDATA* lpbbData);
	static HRESULT ReadLine (LPBYTE* lplpData, DWORD* lpcbData, ISequentialStream* lpLine);
};