#pragma once

typedef struct
{
	INeurone* lpTarget;
	ULONG iPin;
	FLOAT fWeight;
} CLIST, *LPCLIST;

class CConnList
{
private:
	INT m_iHighlightIndex;
	INT m_cConn;
	LPCLIST m_lpConn;

public:
	CConnList ();
	~CConnList ();

	INT Count (VOID);
	VOID EnumConnections (INeurone* pSource, ENUMCONNLIST lpfnCallback, LPVOID lpParam);
	VOID SendPulses (VOID);
    VOID SendPulses (FLOAT fMultiplier);
	HRESULT ConnectTo (INetDocObject* lpTarget, ULONG iTargetPin, FLOAT fWeight);
	BOOL ClearConnection (INT index);
	BOOL SetConnectionWeight (INT index, FLOAT fWeight);
	HRESULT SaveConnections (INeuralNet* lpNet, ISequentialStream* lpStream);
    BOOL Highlight (INT index);
	VOID RemoveConnectionsTo (INeurone* lpNeurone);
	VOID RunTrainer (DWORD dwTrainer, FLOAT fInput, FLOAT fOutput, ULONG iOutputPin);
	VOID Draw (IGrapher* lpGraph, INT xAxon, INT yAxon, BOOL fExcited);

	static HRESULT ConnectTo (LPCLIST* lplpList, INT* lpcList, INetDocObject* lpTarget, ULONG iTargetPin, FLOAT fWeight);
	static BOOL ClearConnection (LPCLIST* lplpList, INT* lpcList, INT index);
	static HRESULT LoadConnections (INetDocObject* lpSource, ULONG iSourcePin, LPNLOADDATA lpLoadData, DWORD cLoadData, LPBYTE* lplpData, ULONG* lpcbData);
	static HRESULT SaveConnections (INeuralNet* lpNet, ISequentialStream* lpStream, LPCLIST lpConn, INT cConn);
};