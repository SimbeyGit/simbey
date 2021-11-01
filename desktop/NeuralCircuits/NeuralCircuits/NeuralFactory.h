#pragma once

interface INeuralNet;
interface INetDocObject;

class CLogicGate;

class CNeuralFactory : public INeuralFactory
{
private:
	ULONG m_cRef;

public:
	CNeuralFactory ();
	~CNeuralFactory ();

	// IUnknown
	HRESULT WINAPI QueryInterface (REFIID iid, LPVOID* lplpvObject);
	ULONG WINAPI AddRef (VOID);
	ULONG WINAPI Release (VOID);

	// INeuralFactory
	virtual HRESULT Create (__in_opt INeuralNet* pNet, PCSTR pcszClass, __deref_out INetDocObject** ppObject);

private:
	HRESULT InitializeLogicGate (CLogicGate* pNewGate, __deref_out INetDocObject** ppObject);
};
