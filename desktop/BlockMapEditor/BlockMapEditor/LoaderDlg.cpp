#include <windows.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Util\Formatting.h"
#include "LoaderDlg.h"

CLoaderDlg::CLoaderDlg (ILoaderThread* pCallback) :
	CBaseDialog(IDD_LOADER),
	m_pCallback(pCallback),
	m_hThread(NULL)
{
}

CLoaderDlg::~CLoaderDlg ()
{
}

BOOL CLoaderDlg::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	BOOL fHandled = FALSE;

	switch(message)
	{
	case WM_INITDIALOG:
		{
			DWORD idThread;
			m_hThread = CreateThread(NULL, 0, _LoaderThread, this, 0, &idThread);
			if(m_hThread)
				CenterHost();
			else
			{
				lResult = FALSE;
				fHandled = TRUE;
			}
		}
		break;

	case WM_COMMAND:
		if(IDOK == LOWORD(wParam))
		{
			if(FAILED((HRESULT)lParam))
			{
				HWND hwnd;
				WCHAR wzError[100];
				SideAssertHr(GetWindow(&hwnd));
				Formatting::TPrintF(wzError, ARRAYSIZE(wzError), NULL, L"Failed to load resource package: 0x%.8X", lParam);
				MessageBox(hwnd, wzError, L"Loader Error", MB_OK);
			}
			End(lParam);
		}
		break;

	case WM_DESTROY:
		if(m_hThread)
		{
			WaitForSingleObject(m_hThread, INFINITE);
			CloseHandle(m_hThread);
			m_hThread = NULL;
		}
		break;
	}

	return fHandled;
}

DWORD WINAPI CLoaderDlg::_LoaderThread (PVOID pvParam)
{
	CLoaderDlg* pThis = reinterpret_cast<CLoaderDlg*>(pvParam);
	HRESULT hr = pThis->m_pCallback->LoaderCallback(pThis->GetDlgItem(IDC_STATUS));
	HWND hwnd;

	if(SUCCEEDED(pThis->GetWindow(&hwnd)))
		PostMessage(hwnd, WM_COMMAND, IDOK, hr);

	return hr;
}
