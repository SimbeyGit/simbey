#include <windows.h>
#include "BaseWindow.h"

template <typename T>
HRESULT TAddItemToTArray (TArray<T>*& pArray, __out T* ptItem, T tLowerBound, T tUpperBound)
{
	HRESULT hr;

	if(NULL == pArray)
		pArray = __new TArray<T>;
	if(pArray)
	{
		T tItem = tLowerBound;
		T* prgItems;
		sysint cItems;
		sysint nInsertAfter = -1;

		pArray->GetData(&prgItems, &cItems);

		for(sysint i = 0; i < cItems; i++)
		{
			if(tItem < prgItems[i])
				break;
			tItem = prgItems[i] + 1;
			nInsertAfter++;
		}

		if(tItem < tUpperBound)
		{
			hr = pArray->InsertAt(tItem, nInsertAfter + 1);
			if(SUCCEEDED(hr))
				*ptItem = tItem;
		}
		else
			hr = E_FAIL;
	}
	else
		hr = E_OUTOFMEMORY;

	return hr;
}

template <typename T>
HRESULT TRemoveItemFromTArray (TArray<T>* pArray, T tItem)
{
	HRESULT hr;

	if(pArray)
	{
		T* prgItems;
		sysint cItems;

		hr = S_FALSE;

		pArray->GetData(&prgItems, &cItems);
		for(sysint i = 0; i < cItems; i++)
		{
			if(prgItems[i] == tItem)
			{
				pArray->Remove(i, NULL);
				hr = S_OK;
				break;
			}
			else if(prgItems[i] > tItem)
				break;
		}
	}
	else
		hr = E_UNEXPECTED;

	return hr;
}

CBaseWindow::CBaseWindow ()
{
	m_hwnd = NULL;
	m_lplpSubclasses = NULL;
	m_cSubclasses = 0;

	m_paUserMessages = NULL;
	m_paTimers = NULL;
}

CBaseWindow::~CBaseWindow ()
{
	ClearAllSubclasses();

	__delete m_paUserMessages;
	__delete m_paTimers;
}

// IOleWindow

HRESULT WINAPI CBaseWindow::GetWindow (HWND* lphwnd)
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

HRESULT WINAPI CBaseWindow::ContextSensitiveHelp (BOOL fEnterMode)
{
	UNREFERENCED_PARAMETER(fEnterMode);

	return S_FALSE;
}

// IBaseWindow

BOOL CBaseWindow::InvokeMessageHandler (BaseWindowMessage::Flag eTarget, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	BOOL fHandled = FALSE;

	if(eTarget & BaseWindowMessage::SubclassedHandlers)
	{
		for(sysint i = 0; i < m_cSubclasses; i++)
		{
			if(m_lplpSubclasses[i]->OnSubclassMessage(this, uMsg, wParam, lParam, lResult))
			{
				fHandled = TRUE;
				break;
			}
		}
	}

	if((eTarget & BaseWindowMessage::BaseWindowHandler) && !fHandled)
		fHandled = DefWindowProc(uMsg, wParam, lParam, lResult);

	if((eTarget & BaseWindowMessage::DefaultWindowProc) && !fHandled)
		::DefWindowProc(m_hwnd, uMsg, wParam, lParam);

	return fHandled;
}

HRESULT CBaseWindow::ShowWindow (INT nCmdShow)
{
	HRESULT hr;

	if(m_hwnd)
	{
		::ShowWindow(m_hwnd, nCmdShow);	// The return value is not an error indicator.
		hr = S_OK;
	}
	else
		hr = E_HANDLE;

	return hr;
}

HRESULT CBaseWindow::Destroy (VOID)
{
	HRESULT hr = E_FAIL;
	if(m_hwnd)
	{
		if(::DestroyWindow(m_hwnd))
			hr = S_OK;
		else
			hr = S_FALSE;
	}
	return hr;
}

HRESULT CBaseWindow::Invalidate (BOOL fEraseBackground)
{
	HRESULT hr;
	if(m_hwnd && InvalidateRect(m_hwnd,NULL,fEraseBackground))
		hr = S_OK;
	else
		hr = E_FAIL;
	return hr;
}

HRESULT CBaseWindow::Move (INT x, INT y, INT nWidth, INT nHeight, BOOL fRepaint)
{
	HRESULT hr;
	if(m_hwnd && MoveWindow(m_hwnd,x,y,nWidth,nHeight,fRepaint))
		hr = S_OK;
	else
		hr = E_FAIL;
	return hr;
}

HMENU CBaseWindow::GetMenu (VOID)
{
	return NULL;
}

HRESULT CBaseWindow::AttachSubclassHandler (ISubclassedHandler* lpSubclass)
{
	HRESULT hr = E_INVALIDARG;
	if(lpSubclass)
	{
		ISubclassedHandler** lplpNew = __new ISubclassedHandler*[m_cSubclasses + 1];
		if(lplpNew)
		{
			*lplpNew = lpSubclass;
			CopyMemory(lplpNew + 1, m_lplpSubclasses, m_cSubclasses * sizeof(ISubclassedHandler*));
			__delete_array m_lplpSubclasses;
			m_lplpSubclasses = lplpNew;
			m_cSubclasses++;

			lpSubclass->OnAttached(this);

			hr = S_OK;
		}
		else
			hr = E_OUTOFMEMORY;
	}
	return hr;
}

HRESULT CBaseWindow::DetachSubclassHandler (ISubclassedHandler* lpSubclass)
{
	HRESULT hr = E_INVALIDARG;
	if(lpSubclass)
	{
		for(sysint i = 0; i < m_cSubclasses; i++)
		{
			if(lpSubclass == m_lplpSubclasses[i])
			{
				MoveMemory(m_lplpSubclasses + i, m_lplpSubclasses + i + 1, (m_cSubclasses - (i + 1)) * sizeof(ISubclassedHandler*));
				m_cSubclasses--;
				hr = S_OK;
				break;
			}
		}

		if(SUCCEEDED(hr))
		{
			if(0 == m_cSubclasses)
			{
				__delete_array m_lplpSubclasses;
				m_lplpSubclasses = NULL;
			}
			else
			{
				ISubclassedHandler** lplpNew = __new ISubclassedHandler*[m_cSubclasses];
				if(lplpNew)
				{
					CopyMemory(lplpNew, m_lplpSubclasses, m_cSubclasses * sizeof(ISubclassedHandler*));
					__delete_array m_lplpSubclasses;
					m_lplpSubclasses = lplpNew;
				}
			}

			lpSubclass->OnDetached(this);
		}
	}
	return hr;
}

HRESULT CBaseWindow::RegisterUserMessage (__out UINT* pnUserMessage)
{
	return TAddItemToTArray<UINT>(m_paUserMessages, pnUserMessage, WM_USER + 100, WM_APP);
}

HRESULT CBaseWindow::UnregisterUserMessage (UINT nUserMessage)
{
	return TRemoveItemFromTArray(m_paUserMessages, nUserMessage);
}

HRESULT CBaseWindow::RegisterTimer (UINT msElapse, __out UINT_PTR* pidTimer)
{
	HRESULT hr = E_FAIL;
	if(m_hwnd)
	{
		hr = TAddItemToTArray<UINT_PTR>(m_paTimers, pidTimer, 1000, 10000);
		if(SUCCEEDED(hr) && 0 == SetTimer(m_hwnd, *pidTimer, msElapse, NULL))
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			TRemoveItemFromTArray(m_paTimers, *pidTimer);
		}
	}
	return hr;
}

HRESULT CBaseWindow::UnregisterTimer (UINT_PTR idTimer)
{
	HRESULT hr = TRemoveItemFromTArray(m_paTimers, idTimer);
	if(SUCCEEDED(hr))
		KillTimer(m_hwnd, idTimer);
	return hr;
}

HRESULT CBaseWindow::AdjustWindowSize (DWORD dwStyle, DWORD dwExStyle, BOOL fMenu, __inout INT& x, __inout INT& y, __inout INT& nWidth, __inout INT& nHeight)
{
	Assert(CW_USEDEFAULT != nWidth && CW_USEDEFAULT != nHeight);

	HRESULT hr;
	RECT WindowRect;

	if(CW_USEDEFAULT == x)
		WindowRect.left = 0;
	else
		WindowRect.left = (LONG)x;
	WindowRect.right = WindowRect.left + (LONG)nWidth;

	if(CW_USEDEFAULT == y)
		WindowRect.top = 0;
	else
		WindowRect.top = (LONG)y;
	WindowRect.bottom = WindowRect.top + (LONG)nHeight;

	if(AdjustWindowRectEx(&WindowRect, dwStyle, fMenu, dwExStyle))
	{
		if(CW_USEDEFAULT != x)
			x = WindowRect.left;
		if(CW_USEDEFAULT != y)
			y = WindowRect.top;

		nWidth = WindowRect.right - WindowRect.left;
		nHeight = WindowRect.bottom - WindowRect.top;

		hr = S_OK;
	}
	else
		hr = HRESULT_FROM_WIN32(GetLastError());

	return hr;
}

// Protected methods

HRESULT CBaseWindow::FinalizeAndShow (DWORD dwStyle, INT nCmdShow)
{
	// Child windows will show themselves.
	if((dwStyle & WS_CHILD) == 0)
	{
		::ShowWindow(m_hwnd, nCmdShow);
		UpdateWindow(m_hwnd);
	}
	return S_OK;
}

BOOL CBaseWindow::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	UNREFERENCED_PARAMETER(message);
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	UNREFERENCED_PARAMETER(lResult);

	return FALSE;
}

HRESULT CBaseWindow::Create (DWORD dwExStyle, DWORD dwStyle, LPCTSTR lpcszClass, LPCTSTR lpcszTitle, INT x, INT y, INT nWidth, INT nHeight, HWND hwndParent, INT nCmdShow)
{
	HRESULT hr = E_UNEXPECTED;
	if(lpcszClass && NULL == m_hwnd)
	{
		m_hwnd = ::CreateWindowEx(dwExStyle, lpcszClass, lpcszTitle, dwStyle, x, y, nWidth, nHeight, hwndParent, GetMenu(), GetInstance(), static_cast<CBaseWindow*>(this));
		if(m_hwnd)
		{
			hr = FinalizeAndShow(dwStyle,nCmdShow);
			if(FAILED(hr))
				Destroy();
		}
		else
		{
			hr = HRESULT_FROM_WIN32(GetLastError());

			// If the WM_CREATE handler caused creation to fail, just return E_ABORT;
			if(!FAILED(hr))
				hr = E_ABORT;
		}
	}
	return hr;
}

HRESULT CBaseWindow::CreateWithId (DWORD dwExStyle, DWORD dwStyle, LPCTSTR lpcszClass, LPCTSTR lpcszTitle, INT x, INT y, INT nWidth, INT nHeight, HWND hwndParent, INT nCmdShow, WORD wControlId)
{
	HRESULT hr = E_UNEXPECTED;
	if(lpcszClass && NULL == m_hwnd)
	{
		m_hwnd = ::CreateWindowEx(dwExStyle, lpcszClass, lpcszTitle, dwStyle, x, y, nWidth, nHeight, hwndParent, reinterpret_cast<HMENU>(wControlId), GetInstance(), static_cast<CBaseWindow*>(this));
		if(m_hwnd)
		{
			hr = FinalizeAndShow(dwStyle,nCmdShow);
			if(FAILED(hr))
				Destroy();
		}
		else
		{
			hr = HRESULT_FROM_WIN32(GetLastError());

			// If the WM_CREATE handler caused creation to fail, just return E_ABORT;
			if(!FAILED(hr))
				hr = E_ABORT;
		}
	}
	return hr;
}

HRESULT CBaseWindow::AttachBaseWinProc (VOID)
{
	HRESULT hr = E_UNEXPECTED;
	if(m_hwnd)
	{
		SetWindowLongPtr(m_hwnd,GWL_WNDPROC,(__int3264)(LONG_PTR)_DefWinProc);
		hr = S_OK;
	}
	return hr;
}

VOID CBaseWindow::ClearAllSubclasses (VOID)
{
	if(0 < m_cSubclasses)
	{
		for(sysint i = 0; i < m_cSubclasses; i++)
		{
			m_lplpSubclasses[i]->OnDetached(this);
		}
		__delete_array m_lplpSubclasses;
		m_lplpSubclasses = NULL;
		m_cSubclasses = 0;
	}
}

HRESULT CBaseWindow::RegisterClass (const WNDCLASSEX* lpWndClass, ATOM* lpAtom)
{
	HRESULT hr = E_INVALIDARG;
	if(lpWndClass && lpWndClass->lpszClassName && lpWndClass->hInstance)
	{
		WNDCLASSEX wnd;
		ATOM atom;

		CopyMemory(&wnd, lpWndClass, sizeof(wnd));
		wnd.lpfnWndProc = _DefWinProcCreate;

		atom = ::RegisterClassEx(&wnd);
		if(lpAtom)
			*lpAtom = atom;

		if(0 == atom)
			hr = HRESULT_FROM_WIN32(GetLastError());
		else
			hr = S_OK;
	}
	return hr;
}

HRESULT CBaseWindow::UnregisterClass (LPCTSTR lpcszClass, HINSTANCE hInstance)
{
	HRESULT hr;
	if(::UnregisterClass(lpcszClass,hInstance))
		hr = S_OK;
	else
		hr = HRESULT_FROM_WIN32(GetLastError());
	return hr;
}

BOOL CBaseWindow::InnerWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	BOOL fHandled = InvokeMessageHandler((BaseWindowMessage::Flag)static_cast<int>(BaseWindowMessage::SubclassedHandlers | BaseWindowMessage::BaseWindowHandler), message, wParam, lParam, lResult);

	// Final destruction may only be invoked by a real window message.
	if(FINAL_WM_MESSAGE == message)
	{
		// Prevent re-entrant calls to Destroy() from destroying the window again.
		HWND hwnd = m_hwnd;
		m_hwnd = NULL;

		OnFinalDestroy(hwnd);

		Release();
	}

	return fHandled;
}

// Private static methods

LRESULT WINAPI CBaseWindow::_DefWinProcCreate (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT lResult = 0;
	if(message == FIRST_WM_MESSAGE)
	{
		LPCREATESTRUCT lpCreate = (LPCREATESTRUCT)lParam;
		CBaseWindow* lpWin = (CBaseWindow*)lpCreate->lpCreateParams;
		lpWin->AddRef();
		lpWin->m_hwnd = hwnd;
		SetWindowLongPtr(hwnd,GWL_USERDATA,(__int3264)(LONG_PTR)lpWin);
		SetWindowLongPtr(hwnd,GWL_WNDPROC,(__int3264)(LONG_PTR)_DefWinProc);
		lResult = _DefWinProc(hwnd,message,wParam,lParam);
	}
	else
		lResult = ::DefWindowProc(hwnd,message,wParam,lParam);
	return lResult;
}

LRESULT WINAPI CBaseWindow::_DefWinProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT lResult = 0;
	CBaseWindow* lpWin = (CBaseWindow*)(LONG_PTR)GetWindowLongPtr(hwnd,GWL_USERDATA);
	if(FALSE == lpWin->InnerWindowProc(message, wParam, lParam, lResult))
		lResult = ::DefWindowProc(hwnd,message,wParam,lParam);
	return lResult;
}