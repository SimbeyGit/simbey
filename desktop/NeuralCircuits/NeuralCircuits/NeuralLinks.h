#pragma once

#include "Library\Core\SortedArray.h"
#include "Library\Core\Map.h"
#include "Neurone.h"

class CNeuralLinks;

class CNeuralLink : public INeuralLink
{
private:
	ULONG m_cRef;

protected:
	TSortedArray<IIONeurone*> m_aIO;
	LPCSTR m_lpcszName;

	CNeuralLinks* m_lpLinks;
	INeuralSource* m_lpSource;
	PVOID m_pvSourceData;

public:
	CNeuralLink (CNeuralLinks* lpLinks);
	virtual ~CNeuralLink ();

	VOID SetLinkName (LPCSTR lpcszName);
	VOID SetSource (__in INeuralSource* lpSource);
	VOID SendPulses (VOID);

	// IUnknown
	HRESULT WINAPI QueryInterface (REFIID iid, LPVOID* lplpvObject);
	ULONG WINAPI AddRef (VOID);
	ULONG WINAPI Release (VOID);

	// INeuralLink
	virtual HRESULT Register (IIONeurone* lpIO);
	virtual HRESULT Unregister (IIONeurone* lpIO);
	virtual VOID OutputValue (FLOAT fValue, ULONG iPin);
	virtual VOID InputValue (FLOAT fValue, ULONG iPin);
	virtual HRESULT GetLinkName (LPSTR lpszName, INT cchMaxName);
	virtual HRESULT GetLinkSource (__deref_out INeuralSource** lplpSource);
	virtual VOID SetSourceData (PVOID pvData);
	virtual PVOID GetSourceData (VOID);
	virtual HRESULT Remove (VOID);

protected:
	VOID UnregisterAndDetachSource (VOID);
};

class CNeuralLinks : public INeuralLinks
{
protected:
	TNamedMapA<CNeuralLink*> m_mapLinks;
	TNamedMapA<INeuralSource*> m_mapSources;

public:
	CNeuralLinks ();
	~CNeuralLinks ();

	HRESULT Initialize (VOID);
	VOID Unload (VOID);

	// INeuralLinks
	virtual VOID EditLink (HWND hwndParent, LPCSTR szName);
	virtual VOID FillLinksList (HWND hwndBox, LPCSTR lpcszSelected, BOOL fIsComboBox);
	virtual VOID FillSources (HWND hwndBox);
	virtual HRESULT GetNeuralLink (LPCSTR lpcszName, __deref_out INeuralLink** ppLink);
	virtual HRESULT CreateLink (__in_opt HWND hwndParent, LPCSTR lpcszSource, LPCSTR lpcszName);
	virtual HRESULT Load (LPBYTE* lplpData, DWORD* lpcbData);
	virtual HRESULT Save (ISequentialStream* lpStream);

	VOID SendPulses (VOID);
	HRESULT Unlink (__in INeuralLink* lpLink);

protected:
	HRESULT BindLinkWithSource (CNeuralLink* lpLink, LPCSTR lpcszName);
};