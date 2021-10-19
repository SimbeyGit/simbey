#include <windows.h>
#include "..\Core\CoreDefs.h"
#include "BaseDialog.h"
#include "DialogHost.h"

#ifdef	DIALOG_HOST_PROPERTY_SHEETS
CDialogHost* CDialogHost::m_pPropertySheetContext = NULL;
#endif

CDialogHost::CDialogHost (HINSTANCE hInstance)
{
	m_cRef = 1;

	m_hInstance = hInstance;
	m_hwnd = NULL;
	m_dwReturnValue = 0;

	m_fHasFrame = FALSE;
	m_fModeless = FALSE;

	m_lpPages = NULL;
	m_cPages = 0;
}

CDialogHost::~CDialogHost ()
{
	Assert(m_lpPages == NULL);
}

// IUnknown

HRESULT WINAPI CDialogHost::QueryInterface (REFIID iid, LPVOID* lplpvObject)
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

ULONG WINAPI CDialogHost::AddRef (VOID)
{
	return (ULONG)InterlockedIncrement((LONG*)&m_cRef);
}

ULONG WINAPI CDialogHost::Release (VOID)
{
	ULONG c = (ULONG)InterlockedDecrement((LONG*)&m_cRef);
	if(c == 0)
		__delete this;
	return c;
}

// IOleWindow

HRESULT WINAPI CDialogHost::GetWindow (HWND* lphwnd)
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

HRESULT WINAPI CDialogHost::ContextSensitiveHelp (BOOL fEnterMode)
{
	UNREFERENCED_PARAMETER(fEnterMode);

	return S_FALSE;
}

VOID CDialogHost::SetReturnValue (DWORD dwValue)
{
	m_dwReturnValue = dwValue;
}

DWORD CDialogHost::GetReturnValue (VOID)
{
	return m_dwReturnValue;
}

HRESULT CDialogHost::CreateModeless (HWND hwnd, CBaseDialog* lpPage)
{
	HRESULT hr = E_INVALIDARG;
	if(hwnd)
	{
		if(m_hwnd == NULL)
		{
			hr = CreatePageArray(&lpPage,1);
			if(SUCCEEDED(hr))
			{
				PROPSHEETPAGE Page = {0};
				Page.dwSize = sizeof(Page);
				hr = lpPage->CreatePage(&Page, m_hInstance, FALSE);
				if(SUCCEEDED(hr))
				{
					m_fHasFrame = FALSE;
					m_fModeless = TRUE;

					if(NULL == CreateDialogParam(Page.hInstance,Page.pszTemplate,hwnd,DlgProcedureCreate,(LPARAM)this))
						hr = E_FAIL;
				}
				if(FAILED(hr))
					Destroy();
			}
		}
		else
			hr = E_UNEXPECTED;
	}
	return hr;
}

HRESULT CDialogHost::Display (HWND hwnd, CBaseDialog* lpPage)
{
	HRESULT hr = E_INVALIDARG;
	if(hwnd)
	{
		if(m_hwnd == NULL)
		{
			hr = CreatePageArray(&lpPage,1);
			if(SUCCEEDED(hr))
			{
				PROPSHEETPAGE Page = {0};
				Page.dwSize = sizeof(Page);
				hr = lpPage->CreatePage(&Page, m_hInstance, FALSE);
				if(SUCCEEDED(hr))
				{
					m_fHasFrame = FALSE;
					m_fModeless = FALSE;

					DialogBoxParam(Page.hInstance,Page.pszTemplate,hwnd,DlgProcedureCreate,(LPARAM)this);

					hr = S_OK;
				}
				else
					Destroy();
			}
		}
		else
			hr = E_UNEXPECTED;
	}
	return hr;
}

#ifdef	DIALOG_HOST_PROPERTY_SHEETS

HRESULT CDialogHost::DisplaySheet (HWND hwnd, LPTSTR lpszTitle, INT nIconRes, CBaseDialog** lpPages, INT cPages, BOOL fWizard)
{
	HRESULT hr = E_INVALIDARG;
	if(hwnd)
	{
		if(m_hwnd == NULL)
		{
			m_fHasFrame = FALSE;

			hr = CreatePageArray(lpPages,cPages);
			if(SUCCEEDED(hr))
			{
				PROPSHEETPAGE* lpPropPages = __new PROPSHEETPAGE[cPages];
				if(lpPropPages)
				{
					ZeroMemory(lpPropPages,sizeof(PROPSHEETPAGE) * cPages);
					for(INT i = 0; i < cPages; i++)
					{
						lpPropPages[i].dwSize = sizeof(PROPSHEETPAGE);
						hr = lpPages[i]->CreatePage(lpPropPages + i, m_hInstance, TRUE);
						if(FAILED(hr))
							break;
					}
					if(SUCCEEDED(hr))
					{
						INT_PTR nReturn;
						PROPSHEETHEADER Header = {0};
						Header.dwSize = sizeof(Header);
						Header.dwFlags = PSH_PROPSHEETPAGE | PSH_USECALLBACK;
						Header.hInstance = m_hInstance;
						Header.hwndParent = hwnd;
						Header.pszCaption = lpszTitle;
						Header.pszIcon = MAKEINTRESOURCE(nIconRes);
						Header.pfnCallback = DlgSheetProcedure;
						Header.ppsp = lpPropPages;
						Header.nPages = cPages;

						if(fWizard)
							Header.dwFlags |= PSH_WIZARD;

						// Temporarily set the property sheet context.  This is the ONLY way to
						// make context available for PSCB_INITIALIZED.  Thanks, Microsoft...
						Assert(NULL == m_pPropertySheetContext);
						m_pPropertySheetContext = this;

						nReturn = PropertySheet(&Header);
						if(0 < nReturn)
							hr = S_OK;			// Changes made
						else if(0 == nReturn)
							hr = S_FALSE;		// No changes made
						else
							hr = HRESULT_FROM_WIN32(GetLastError());
					}
					__delete_array lpPropPages;
				}
				else
					hr = E_OUTOFMEMORY;
			}

			Destroy();
		}
	}
	return hr;
}

#endif

BOOL CDialogHost::IsDialogMessage (LPMSG lpMsg)
{
	return m_fModeless && ::IsDialogMessage(m_hwnd,lpMsg);
}

BOOL CDialogHost::Center (VOID)
{
	BOOL fSuccess = FALSE;

	if(m_hwnd)
	{
		RECT rect;
		INT xScreen = GetSystemMetrics(SM_CXSCREEN);
		INT yScreen = GetSystemMetrics(SM_CYSCREEN);
		GetWindowRect(m_hwnd,&rect);
		fSuccess = MoveWindow(m_hwnd,xScreen / 2 - (rect.right - rect.left) / 2,yScreen / 2 - (rect.bottom - rect.top) / 2,rect.right - rect.left,rect.bottom - rect.top,TRUE);
	}

	return fSuccess;
}

VOID CDialogHost::Destroy (VOID)
{
	if(m_hwnd)
	{
		if(m_fModeless)
		{
			DestroyWindow(m_hwnd);
			m_fModeless = FALSE;
		}
		else if(!m_fHasFrame)
			EndDialog(m_hwnd,m_dwReturnValue);
		m_hwnd = NULL;
	}
	if(m_lpPages)
	{
		for(INT i = 0; i < m_cPages; i++)
		{
			m_lpPages[i]->AttachHost(NULL);
			m_lpPages[i]->Release();
		}
		__delete_array m_lpPages;
		m_lpPages = NULL;
		m_cPages = 0;
	}
}

HINSTANCE CDialogHost::GetInstance (VOID)
{
	return m_hInstance;
}

HRESULT CDialogHost::CreatePageArray (CBaseDialog** lpPages, INT cPages)
{
	HRESULT hr = E_INVALIDARG;
	if(lpPages && cPages > 0)
	{
		m_lpPages = __new CBaseDialog*[cPages];
		if(m_lpPages)
		{
			for(INT i = 0; i < cPages; i++)
			{
				m_lpPages[i] = lpPages[i];
				m_lpPages[i]->AttachHost(this);
				m_lpPages[i]->AddRef();
			}
			m_cPages = cPages;
			hr = S_OK;
		}
		else
			hr = E_OUTOFMEMORY;
	}
	return hr;
}

#ifdef	DIALOG_HOST_PROPERTY_SHEETS

INT CALLBACK CDialogHost::DlgSheetProcedure (HWND hDlg, UINT message, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);

	switch(message)
	{
	case PSCB_INITIALIZED:
		if(m_pPropertySheetContext)
		{
			m_pPropertySheetContext->m_hwnd = hDlg;
			m_pPropertySheetContext = NULL;
		}
		break;
	}

	return 0;
}

#endif

DLGRESULT CALLBACK CDialogHost::DlgProcedureCreate (HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	DLGRESULT fHandled = FALSE;
	if(WM_INITDIALOG == message)
	{
		PROPSHEETPAGE Page = {0};
		CDialogHost* lpHost = (CDialogHost*)lParam;

		Assert(lpHost);
		Assert(!lpHost->m_fHasFrame);
		Assert(lpHost->m_cPages == 1);

		lpHost->m_hwnd = hDlg;

		Page.dwSize = sizeof(Page);
		if(SUCCEEDED(lpHost->m_lpPages[0]->CreatePage(&Page, lpHost->m_hInstance, FALSE)))
		{
			// Set the dialog procedure to the page's dialog procedure.
			SetWindowLongPtr(hDlg,DWL_DLGPROC,(__int3264)(LONG_PTR)Page.pfnDlgProc);

			// Call to initialize the page.
			fHandled = static_cast<DLGRESULT>(Page.pfnDlgProc(hDlg,message,wParam,Page.lParam));
		}
	}
	return fHandled;
}
