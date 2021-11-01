#pragma once

class CEnumVariant : public IEnumVARIANT
{
private:
	ULONG m_cRef;

protected:
	VARIANT* m_lpvList;
	ULONG m_cList;
	ULONG m_iReadPtr;

public:
	CEnumVariant ();
	~CEnumVariant ();

	VOID AttachList (VARIANT* lpvList, ULONG cList);

	// IUnknown
	HRESULT WINAPI QueryInterface (REFIID iid, LPVOID* lplpvObject);
	ULONG WINAPI AddRef (VOID);
	ULONG WINAPI Release (VOID);

	// IEnumVARIANT
	HRESULT WINAPI Next (ULONG celt, VARIANT* rgVar, ULONG* lpCeltFetched);
	HRESULT WINAPI Skip (ULONG celt);
	HRESULT WINAPI Reset (VOID);
	HRESULT WINAPI Clone (IEnumVARIANT** lplpEnum);

protected:
	VOID Clear (VOID);
};