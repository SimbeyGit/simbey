#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "SampleHostedSvc.h"

HRESULT WINAPI CSampleHostedSvc::CreateService (IServiceStatus* pStatus, __deref_out IHostedService** ppService)
{
	*ppService = __new CSampleHostedSvc(pStatus);
	return *ppService ? S_OK : E_OUTOFMEMORY;
}

CSampleHostedSvc::CSampleHostedSvc (IServiceStatus* pStatus) :
	m_pStatus(pStatus)
{
	m_pStatus->AddRef();
}

CSampleHostedSvc::~CSampleHostedSvc ()
{
	m_pStatus->Release();
}

PCWSTR WINAPI CSampleHostedSvc::_GetServiceRegKey (VOID)
{
	return L"Software\\Simbey\\SampleHostedSvc";
}

// IHostedService

PCWSTR CSampleHostedSvc::GetServiceRegKey (VOID)
{
	return _GetServiceRegKey();
}

HRESULT WINAPI CSampleHostedSvc::GetServiceProperty (SvcHost::Property eProperty, __out_ecount(cchMaxBuffer) PWSTR pwzBuffer, INT cchMaxBuffer)
{
	HRESULT hr;

	switch(eProperty)
	{
	case SvcHost::Name:
		hr = TStrCchCpy(pwzBuffer, cchMaxBuffer, L"SampleHostedSvc");
		break;
	case SvcHost::Description:
		hr = TStrCchCpy(pwzBuffer, cchMaxBuffer, L"Demonstrates how to create a basic Win32 service with SvcHost.dll");
		break;
	case SvcHost::Display:
		hr = TStrCchCpy(pwzBuffer, cchMaxBuffer, L"Sample Hosted Service");
		break;
	default:
		hr = E_INVALIDARG;
		break;
	}

	return hr;
}

HRESULT WINAPI CSampleHostedSvc::SetBasePath (PCWSTR pcwzBasePath)
{
	return S_OK;
}

BOOL WINAPI CSampleHostedSvc::Initialize (VOID)
{
	// Perform service initializations here
	return TRUE;
}

BOOL WINAPI CSampleHostedSvc::Start (VOID)
{
	// Add service starting requirements here
	return TRUE;
}

VOID WINAPI CSampleHostedSvc::Stop (VOID)
{
	// Add service stopping requirements here
}

VOID WINAPI CSampleHostedSvc::Uninitialize (VOID)
{
	// Perform service uninitializations here
}
