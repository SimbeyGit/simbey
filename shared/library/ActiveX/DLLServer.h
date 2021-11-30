#pragma once

class CDLLServer
{
public:
	static CDLLServer* m_pThis;

	volatile LONG m_cReferences;
	HMODULE m_hModule;

public:
	CDLLServer ();
	virtual ~CDLLServer ();

	virtual HRESULT RegisterServer (VOID);
	virtual HRESULT UnregisterServer (VOID);

	virtual const IID& GetStaticClassID (VOID) = 0;
	virtual LPCTSTR GetStaticProgID (VOID) = 0;
	virtual LPCTSTR GetStaticModuleDescription (VOID) = 0;

	virtual BOOL Main (HINSTANCE hInstance, DWORD dwReason, PVOID pvReserved);
};

VOID DLLAddRef (VOID);
VOID DLLRelease (VOID);
