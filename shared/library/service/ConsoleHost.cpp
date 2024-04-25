#include <windows.h>
#include "ConsoleHost.h"

CConsoleHost::CConsoleHost () :
	m_cArgs(0),
	m_pptzArgs(NULL),
	m_pService(NULL)
{
}

CConsoleHost::~CConsoleHost ()
{
	Assert(NULL == m_pService);
}

VOID CConsoleHost::AttachCommandLineArgs (int cArgs, PTSTR* pptzArgs)
{
	m_cArgs = cArgs;
	m_pptzArgs = pptzArgs;
}

// IServiceHost

HRESULT CConsoleHost::Install (IService* pService, PCTSTR /*pctzInstallPath*/)
{
	HRESULT hr;
	TCHAR tzDisplay[512];
	TCHAR tzPath[MAX_PATH];

	Check(pService->GetSvcDisp(tzDisplay, ARRAYSIZE(tzDisplay)));
	Check(pService->GetSvcPath(tzPath, ARRAYSIZE(tzPath)));
	Check(pService->PostInstall(tzPath, tzDisplay));

Cleanup:
	return hr;
}

HRESULT CConsoleHost::Uninstall (IService* /*pService*/)
{
	return E_NOTIMPL;
}

HRESULT CConsoleHost::RunService (IService* pService)
{
	HRESULT hr = S_OK;
	bool fConnected = false;
	DWORD dwPrevConsoleMode;

	CheckIf(NULL == pService, E_INVALIDARG);
	CheckIf(NULL != m_pService, E_UNEXPECTED);

	m_pService = pService;
	m_pService->AddRef();

	CheckIf(!m_pService->Connect(this, m_cArgs, m_pptzArgs), E_FAIL);
	fConnected = true;

	CheckIf(!m_pService->StartService(), E_FAIL);

	GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &dwPrevConsoleMode);
	SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), 0);

	while(WaitForSingleObject(GetStdHandle(STD_INPUT_HANDLE), INFINITE) == WAIT_OBJECT_0)
	{
		INPUT_RECORD recInput;
		DWORD dwTemp;

		if(!ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &recInput, 1, &dwTemp))
			break;
		if(KEY_EVENT == recInput.EventType &&
			recInput.Event.KeyEvent.bKeyDown &&
			27 == recInput.Event.KeyEvent.uChar.AsciiChar &&
			m_pService->QueryStopRequest(TRUE, &dwTemp))
			break;
	}

	SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), dwPrevConsoleMode);

Cleanup:
	if(m_pService)
	{
		if(fConnected)
		{
			m_pService->StopService();
		}

		m_pService->Disconnect();
		SafeRelease(m_pService);
	}

	return hr;
}

// IServiceHostEx

HRESULT CConsoleHost::AddDeviceNotification (PVOID /*pvNotificationFilter*/)
{
	// This *could* be supported using a hidden window.
	return E_NOTIMPL;
}
