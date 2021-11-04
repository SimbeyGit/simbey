#include <windows.h>
#include "..\Core\CoreDefs.h"
#include "Published\SimbeyCore.h"
#include "DLLServer.h"

CDLLServer* CDLLServer::m_pThis = NULL;

CDLLServer::CDLLServer () :
	m_hModule(NULL)
{
	m_pThis = this;
}

CDLLServer::~CDLLServer ()
{
	m_pThis = NULL;
}

HRESULT CDLLServer::RegisterServer (VOID)
{
	return ScRegisterServer(m_hModule, GetStaticClassID(), GetStaticProgID(), GetStaticModuleDescription());
}

HRESULT CDLLServer::UnregisterServer (VOID)
{
	return ScUnregisterServer(GetStaticClassID(), GetStaticProgID());
}

BOOL CDLLServer::Main (HINSTANCE hInstance, DWORD dwReason, PVOID pvReserved)
{
	BOOL fHandled = FALSE;
	switch(dwReason)
	{
	case DLL_PROCESS_ATTACH:
		m_hModule = reinterpret_cast<HMODULE>(hInstance);
		fHandled = TRUE;
		break;
	case DLL_PROCESS_DETACH:
		m_hModule = NULL;
		fHandled = TRUE;
		break;
	}
	return fHandled;
}

VOID DLLAddRef (VOID)
{
	InterlockedIncrement(&CDLLServer::m_pThis->m_cReferences);
}

VOID DLLRelease (VOID)
{
	InterlockedDecrement(&CDLLServer::m_pThis->m_cReferences);
}

HRESULT WINAPI DllRegisterServer (VOID)
{
	return CDLLServer::m_pThis->RegisterServer();
}

HRESULT WINAPI DllUnregisterServer (VOID)
{
	return CDLLServer::m_pThis->UnregisterServer();
}

STDAPI DllCanUnloadNow (VOID)
{
	return (0 == InterlockedCompareExchange(&CDLLServer::m_pThis->m_cReferences, 0, 0)) ? S_OK : S_FALSE;
}

EXTERN_C BOOL WINAPI DllMain (HINSTANCE hInstance, DWORD dwReason, PVOID pvReserved)
{
	return CDLLServer::m_pThis->Main(hInstance, dwReason, pvReserved);
}
