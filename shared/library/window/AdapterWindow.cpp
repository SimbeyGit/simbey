#include <windows.h>
#include "..\Core\Assert.h"
#include "..\Core\Pointers.h"
#include "AdapterWindow.h"

HRESULT QueryAdapter (CBaseWindow* pAdapter, REFIID iid, LPVOID* lplpvObject)
{
	HRESULT hr = E_POINTER;
	if(lplpvObject)
	{
		hr = S_OK;

		if(IID_IOleWindow == iid)
			*lplpvObject = static_cast<IOleWindow*>(pAdapter);
		else if(__uuidof(IBaseWindow) == iid)
			*lplpvObject = static_cast<IBaseWindow*>(pAdapter);
		else if(IID_IUnknown == iid)
			*lplpvObject = static_cast<IBaseWindow*>(pAdapter);
		else
			hr = E_NOINTERFACE;

		if(SUCCEEDED(hr))
			pAdapter->AddRef();
	}
	return hr;
}

CAdapterWindow::CAdapterWindow ()
{
	m_cRef = 1;

	m_hInstance = NULL;
	m_pCallback = NULL;
}

CAdapterWindow::~CAdapterWindow ()
{
	Assert(NULL == m_pCallback);
}

// IUnknown

HRESULT WINAPI CAdapterWindow::QueryInterface (REFIID iid, LPVOID* lplpvObject)
{
	return QueryAdapter(this, iid, lplpvObject);
}

ULONG WINAPI CAdapterWindow::AddRef (VOID)
{
	return (ULONG)InterlockedIncrement((LONG*)&m_cRef);
}

ULONG WINAPI CAdapterWindow::Release (VOID)
{
	ULONG c = (ULONG)InterlockedDecrement((LONG*)&m_cRef);
	if(0 == c)
		__delete this;
	return c;
}

HRESULT CAdapterWindow::Register (PCWSTR pcwzClass, HINSTANCE hInstance)
{
	WNDCLASSEX wnd = {0};

	wnd.cbSize = sizeof(WNDCLASSEX);
	wnd.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;
	wnd.cbClsExtra = 0;
	wnd.cbWndExtra = 0;
	wnd.hInstance = hInstance;
	wnd.hIcon = NULL;
	wnd.hCursor = LoadCursor(NULL, IDC_ARROW);
	wnd.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wnd.lpszMenuName = NULL;
	wnd.lpszClassName = pcwzClass;

	return RegisterClass(&wnd, NULL);
}

HRESULT CAdapterWindow::Unregister (PCWSTR pcwzClass, HINSTANCE hInstance)
{
	return UnregisterClass(pcwzClass, hInstance);
}

HRESULT CAdapterWindow::AttachAsChild (HINSTANCE hInstance, __in IAdapterWindowCallback* pCallback, PCWSTR pcwzClass, PCWSTR pcwzData, INT x, INT y, INT nWidth, INT nHeight, HWND hwndParent, INT nCmdShow)
{
	return Attach(hInstance, pCallback, 0, WS_CHILD, pcwzClass, pcwzData, x, y, nWidth, nHeight, hwndParent, nCmdShow);
}

HRESULT CAdapterWindow::Attach (HINSTANCE hInstance, __in IAdapterWindowCallback* pCallback, DWORD dwExStyles, DWORD dwStyles, PCWSTR pcwzClass, PCWSTR pcwzData, INT x, INT y, INT nWidth, INT nHeight, HWND hwndParent, INT nCmdShow)
{
	HRESULT hr = S_FALSE;

	if(NULL == m_pCallback)
	{
		m_hInstance = hInstance;

		m_pCallback = pCallback;
		m_pCallback->AddRef();

		m_pCallback->OnAttachingAdapter(this);

		if(SW_SHOW == nCmdShow || SW_SHOWNORMAL == nCmdShow)
			dwStyles |= WS_VISIBLE;

		hr = Create(dwExStyles, dwStyles, pcwzClass, pcwzData, x, y, nWidth, nHeight, hwndParent, nCmdShow);
		if(FAILED(hr))
		{
			m_pCallback->OnDetachingAdapter(this);

			m_pCallback->Release();
			m_pCallback = NULL;
		}
	}

	return hr;
}

HINSTANCE CAdapterWindow::GetInstance (VOID)
{
	return m_hInstance;
}

VOID CAdapterWindow::OnFinalDestroy (HWND hwnd)
{
	if(m_pCallback)
	{
		m_pCallback->OnDetachingAdapter(this);
		m_pCallback->Release();
		m_pCallback = NULL;
	}
}

BOOL CAdapterWindow::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	BOOL fHandled = m_pCallback->DefWindowProc(message, wParam, lParam, lResult);
	if(!fHandled)
		fHandled = __super::DefWindowProc(message, wParam, lParam, lResult);
	return fHandled;
}

///////////////////////////////////////////////////////////////////////////////

CDialogControlAdapter::CDialogControlAdapter ()
{
	m_cRef = 1;

	m_pCallback = NULL;
}

CDialogControlAdapter::~CDialogControlAdapter ()
{
	Assert(NULL == m_pCallback);
}

// IUnknown

HRESULT WINAPI CDialogControlAdapter::QueryInterface (REFIID iid, LPVOID* lplpvObject)
{
	return QueryAdapter(this, iid, lplpvObject);
}

ULONG WINAPI CDialogControlAdapter::AddRef (VOID)
{
	return (ULONG)InterlockedIncrement((LONG*)&m_cRef);
}

ULONG WINAPI CDialogControlAdapter::Release (VOID)
{
	ULONG c = (ULONG)InterlockedDecrement((LONG*)&m_cRef);
	if(0 == c)
		__delete this;
	return c;
}

HRESULT CDialogControlAdapter::Register (PCWSTR pcwzClass, HINSTANCE hInstance, __out ATOM* pAtom)
{
	WNDCLASSEX wnd = {0};

	wnd.cbSize = sizeof(WNDCLASSEX);
	wnd.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;
	wnd.cbClsExtra = 0;
	wnd.cbWndExtra = 0;
	wnd.hInstance = hInstance;
	wnd.hIcon = NULL;
	wnd.hCursor = LoadCursor(NULL, IDC_ARROW);
	wnd.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wnd.lpszMenuName = NULL;
	wnd.lpszClassName = pcwzClass;

	return TDialogControl<CDialogControlAdapter>::RegisterControl(&wnd, pAtom);
}

HRESULT CDialogControlAdapter::Unregister (PCWSTR pcwzClass, HINSTANCE hInstance)
{
	return UnregisterClass(pcwzClass, hInstance);
}

HRESULT CDialogControlAdapter::Attach (HWND hwnd, IAdapterWindowCallback* pCallback)
{
	HRESULT hr = S_OK;
	if(hwnd)
	{
		CBaseWindow* pWindow = reinterpret_cast<CBaseWindow*>(static_cast<LONG_PTR>(GetWindowLongPtr(hwnd, GWL_USERDATA)));
		CDialogControlAdapter* pAdapter = static_cast<CDialogControlAdapter*>(pWindow);
		if(pAdapter)
			pAdapter->AttachCallback(pCallback);
		else
			hr = E_FAIL;
	}
	else
		hr = E_INVALIDARG;
	return hr;
}

VOID CDialogControlAdapter::AttachCallback (IAdapterWindowCallback* pCallback)
{
	m_pCallback = pCallback;
	m_pCallback->AddRef();
	m_pCallback->OnAttachingAdapter(this);
	InvalidateRect(m_hwnd, NULL, FALSE);
}

HINSTANCE CDialogControlAdapter::GetInstance (VOID)
{
	return reinterpret_cast<HINSTANCE>(static_cast<LONG_PTR>(GetWindowLongPtr(m_hwnd, GWL_USERDATA)));
}

VOID CDialogControlAdapter::OnFinalDestroy (HWND hwnd)
{
	if(m_pCallback)
	{
		m_pCallback->OnDetachingAdapter(this);
		m_pCallback->Release();
		m_pCallback = NULL;
	}
}

BOOL CDialogControlAdapter::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	BOOL fHandled;
	if(m_pCallback)
		fHandled = m_pCallback->DefWindowProc(message, wParam, lParam, lResult);
	else
		fHandled = FALSE;
	if(!fHandled)
		fHandled = __super::DefWindowProc(message, wParam, lParam, lResult);
	return fHandled;
}
