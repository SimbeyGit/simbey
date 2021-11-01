#pragma once

#include "Neurone.h"

class CBaseIONeurone : public CNeurone, public IIONeurone
{
protected:
	ULONG m_iPin;
	INeuralChip* m_lpParent;
	INeuralLink* m_lpLink;

public:
	CBaseIONeurone ();
	virtual ~CBaseIONeurone ();

	// IUnknown
	HRESULT WINAPI QueryInterface (REFIID iid, LPVOID* lplpvObject);
	ULONG WINAPI AddRef (VOID);
	ULONG WINAPI Release (VOID);

	// INetCycleProcessor
	virtual VOID SendPulses (VOID);
	virtual VOID CheckThresholds (VOID);

	// INetObject
	virtual HRESULT Load (INeuralFactory* pFactory, LPNLOADDATA lpLoadData, DWORD cLoadData, LPBYTE lpData, ULONG cbData);
	virtual HRESULT Save (INeuralNet* lpNet, ISequentialStream* lpStream);

	// INetDocObject
	virtual VOID NotifyRemovalOf (INetDocObject* lpObject);

	// IIONeurone
	virtual VOID AttachParentChip (INeuralChip* lpParent);
	virtual BOOL HasParentChip (VOID);
	virtual HRESULT GetLinkName (LPSTR lpszName, INT cchMaxName);
	virtual HRESULT SetLinkName (INeuralLinks* lpLinks, LPCSTR lpcszName);
};