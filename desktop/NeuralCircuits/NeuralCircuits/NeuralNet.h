#pragma once

#include "Neurone.h"

interface IGrapher;
interface INetDocObject;
interface INeuralFrame;
interface INeuralFactory;

typedef struct tagNLIST
{
	INetDocObject* lpObject;
	struct tagNLIST* Next;
	struct tagNLIST* Prev;
} NLIST, *LPNLIST;

interface INeuralNet : public INetCycleProcessor
{
	virtual HRESULT Load (__in INeuralLinks* lpLinks, LPBYTE* lplpData, DWORD* lpcbData) = 0;
	virtual HRESULT LoadBrainBox (LPBYTE* lplpData, DWORD* lpcbData, BOOL fUseBiasNeuronesForBrainBox) = 0;
	virtual HRESULT Save (ISequentialStream* lpStream) = 0;

	virtual VOID ResetNet (VOID) = 0;

	virtual INetDocObject* FindObject (INT x, INT y) = 0;
	virtual INetDocObject* GetObject (INT index) = 0;
	virtual BOOL GetFrame (INT x, INT y, INeuralFrame** lplpFrame) = 0;
	virtual BOOL GetFrame (INetDocObject* lpObject, INeuralFrame** lplpFrame) = 0;

	virtual INT GetObjectCount (VOID) = 0;
	virtual INT GetObjectIndex (INetDocObject* lpObject) = 0;

	virtual HRESULT AttachObject (INetDocObject* lpObject) = 0;
	virtual HRESULT RemoveObject (INetDocObject* lpObject) = 0;

	virtual VOID Draw (IGrapher* lpGraph, BOOL fDrawLabels) = 0;

	virtual ULONG GetUniquePin (EIO_TYPE eType) = 0;
	virtual LPNLIST GetObjects (VOID) = 0;

	virtual HRESULT GetObjectsInRange (FLOAT xLeft, FLOAT yTop, FLOAT xRight, FLOAT yBottom, INetDocObject*** lplplpObjects, INT* lpcObjects) = 0;
};

class CNeuralNet : public INeuralNet
{
private:
	ULONG m_cRef;

protected:
	INeuralFactory* m_pFactory;

	INT m_cList;
	LPNLIST m_lpList;
	LPNLIST m_lpEnd;

	LPNLIST m_lpFrames;	// Not reference counted!

public:
	CNeuralNet (INeuralFactory* pFactory);
	~CNeuralNet ();

	// IUnknown
	HRESULT WINAPI QueryInterface (REFIID iid, LPVOID* lplpvObject);
	ULONG WINAPI AddRef (VOID);
	ULONG WINAPI Release (VOID);

	// INetCycleProcessor
	virtual VOID SendPulses (VOID);
	virtual VOID CheckThresholds (VOID);

	// INeuralNet
	virtual HRESULT Load (__in INeuralLinks* lpLinks, LPBYTE* lplpData, DWORD* lpcbData);
	virtual HRESULT LoadBrainBox (LPBYTE* lplpData, DWORD* lpcbData, BOOL fUseBiasNeuronesForBrainBox);
	virtual HRESULT Save (ISequentialStream* lpStream);

	virtual VOID ResetNet (VOID);

	virtual INetDocObject* FindObject (INT x, INT y);
	virtual INetDocObject* GetObject (INT index);
	virtual BOOL GetFrame (INT x, INT y, INeuralFrame** lplpFrame);
	virtual BOOL GetFrame (INetDocObject* lpObject, INeuralFrame** lplpFrame);

	virtual INT GetObjectCount (VOID);
	virtual INT GetObjectIndex (INetDocObject* lpObject);

	virtual HRESULT AttachObject (INetDocObject* lpObject);
	virtual HRESULT RemoveObject (INetDocObject* lpObject);

	virtual VOID Draw (IGrapher* lpGraph, BOOL fDrawLabels);

	virtual ULONG GetUniquePin (EIO_TYPE eType);
	virtual LPNLIST GetObjects (VOID);

	virtual HRESULT GetObjectsInRange (FLOAT xLeft, FLOAT yTop, FLOAT xRight, FLOAT yBottom, INetDocObject*** lplplpObjects, INT* lpcObjects);

	static HRESULT LoadFromFile (__in INeuralNet* lpNet, __in INeuralLinks* lpLinks, PCSTR pcszFile, INT cchFile, BOOL fUseBiasNeuronesForBrainBox);
	static HRESULT SaveToFile (INeuralNet* lpNet, __in INeuralLinks* lpLinks, PCSTR pcszFile);

protected:
	HRESULT AddFrame (INeuralFrame* lpFrame);
	VOID CheckFrameUnload (INetDocObject* lpObject);
	VOID NotifyRemovalOf (INetDocObject* lpObject);
	VOID UnloadNeuralChip (INetDocObject* lpObject);
};