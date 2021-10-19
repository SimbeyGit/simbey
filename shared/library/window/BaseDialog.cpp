#include <windows.h>
#include "..\Core\CoreDefs.h"
#include "DialogHost.h"
#include "BaseDialog.h"

CBaseDialog::CBaseDialog (WORD idDialogRes) :
	m_idDialogRes(idDialogRes)  // Immutable
{
	m_cRef = 1;

	m_lpHost = NULL;
	m_hwnd = NULL;
}

CBaseDialog::~CBaseDialog ()
{
	Assert(m_lpHost == NULL);
}

// IUnknown

HRESULT WINAPI CBaseDialog::QueryInterface (REFIID iid, LPVOID* lplpvObject)
{
	HRESULT hr = E_INVALIDARG;
	if(lplpvObject)
	{
		if(iid == IID_IOleWindow)
			*lplpvObject = (IOleWindow*)this;
		else if(iid == IID_IUnknown)
			*lplpvObject = (IUnknown*)this;
		else
		{
			hr = E_NOINTERFACE;
			goto exit;
		}
		AddRef();
		hr = S_OK;
	}
exit:
	return hr;
}

ULONG WINAPI CBaseDialog::AddRef (VOID)
{
	return (ULONG)InterlockedIncrement((LONG*)&m_cRef);
}

ULONG WINAPI CBaseDialog::Release (VOID)
{
	ULONG c = (ULONG)InterlockedDecrement((LONG*)&m_cRef);
	if(c == 0)
		__delete this;
	return c;
}

// IOleWindow

HRESULT WINAPI CBaseDialog::GetWindow (HWND* lphwnd)
{
	HRESULT hr = E_INVALIDARG;
	if(lphwnd)
	{
		*lphwnd = m_hwnd;
		if(m_hwnd)
			hr = S_OK;
		else
			hr = E_FAIL;
	}
	return hr;
}

HRESULT WINAPI CBaseDialog::ContextSensitiveHelp (BOOL fEnterMode)
{
	UNREFERENCED_PARAMETER(fEnterMode);

	return S_FALSE;
}

VOID CBaseDialog::AttachHost (CDialogHost* lpHost)
{
	if(m_lpHost)
		m_lpHost->Release();
	m_lpHost = lpHost;
	if(m_lpHost)
		m_lpHost->AddRef();
}

HRESULT CBaseDialog::CreatePage (PROPSHEETPAGE* lpPropPage, HINSTANCE hInstance, BOOL fPropertySheet)
{
	HRESULT hr = E_INVALIDARG;
	if(lpPropPage && lpPropPage->dwSize >= sizeof(PROPSHEETPAGE))
	{
		lpPropPage->hInstance = hInstance;
		lpPropPage->pszTemplate = MAKEINTRESOURCE(m_idDialogRes);
		lpPropPage->pfnDlgProc = (fPropertySheet) ? DlgCreatePage : DlgProcedure;
		lpPropPage->lParam = (LPARAM)this;
		hr = S_OK;
	}
	return hr;
}

BOOL CBaseDialog::CenterHost (VOID)
{
	return (m_lpHost) ? m_lpHost->Center() : FALSE;
}

HWND CBaseDialog::GetDlgItem (INT nItem)
{
	return ::GetDlgItem(m_hwnd,nItem);
}

HWND CBaseDialog::GetFrame (VOID)
{
	HWND hwndFrame = NULL;
	if(m_lpHost)
		m_lpHost->GetWindow(&hwndFrame);
	return hwndFrame;
}

HINSTANCE CBaseDialog::GetInstance (VOID)
{
	return (m_lpHost) ? m_lpHost->GetInstance() : NULL;
}

VOID CBaseDialog::End (DWORD dwReturn)
{
	m_lpHost->SetReturnValue(dwReturn);
	m_lpHost->Destroy();
}

DLGRESULT CALLBACK CBaseDialog::DlgProcedure (HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	DLGRESULT fHandled = FALSE;
	CBaseDialog* lpDlg;

	if(WM_INITDIALOG == message)
	{
		lpDlg = reinterpret_cast<CBaseDialog*>(lParam);
		Assert(lpDlg);

		lpDlg->m_hwnd = hDlg;
		lpDlg->AddRef();

		SideAssertCompare(SetWindowLongPtr(hDlg, DWL_USER, lParam), 0);
	}
	else
		lpDlg = (CBaseDialog*)(LONG_PTR)GetWindowLongPtr(hDlg, DWL_USER);

	if(lpDlg)
	{
		LRESULT lResult = 0;
		fHandled = lpDlg->DefWindowProc(message,wParam,lParam,lResult);

		if(fHandled)
			fHandled = (__int3264)lResult;

		if(message == FINAL_WM_MESSAGE)
		{
			lpDlg->m_hwnd = NULL;
			SetWindowLong(hDlg,DWL_USER,0);
			lpDlg->Release();
		}
#ifndef	WINCE
		else if(message == WM_ENDSESSION)
			lpDlg->End(0);
#endif
	}

	return fHandled;
}

DLGRESULT CALLBACK CBaseDialog::DlgCreatePage (HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	DLGRESULT fHandled = FALSE;

	if(WM_INITDIALOG == message)
	{
		PROPSHEETPAGE* pPage = reinterpret_cast<PROPSHEETPAGE*>(lParam);

		SetWindowLongPtr(hDlg, DWL_DLGPROC, (__int3264)(LONG_PTR)DlgProcedure);
		fHandled = DlgProcedure(hDlg, message, wParam, pPage->lParam);
	}

	return fHandled;
}