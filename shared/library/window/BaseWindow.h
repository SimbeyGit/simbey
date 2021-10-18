#pragma once

#include "..\Core\Array.h"
#include "PlatformCompat.h"
#include "IBaseWindow.h"

class CBaseWindow : public IBaseWindow
{
	template <typename TControl>
	friend class TDialogControl;

protected:
	HWND m_hwnd;
	ISubclassedHandler** m_lplpSubclasses;
	sysint m_cSubclasses;

	TArray<UINT>* m_paUserMessages;
	TArray<UINT_PTR>* m_paTimers;

public:
	CBaseWindow ();
	virtual ~CBaseWindow ();

	// IOleWindow
	HRESULT WINAPI GetWindow (HWND* lphwnd);
	HRESULT WINAPI ContextSensitiveHelp (BOOL fEnterMode);

	// IBaseWindow
	virtual BOOL InvokeMessageHandler (BaseWindowMessage::Flag eTarget, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult);
	virtual HRESULT ShowWindow (INT nCmdShow);
	virtual HRESULT Destroy (VOID);
	virtual HRESULT Invalidate (BOOL fEraseBackground);
	virtual HRESULT Move (INT x, INT y, INT nWidth, INT nHeight, BOOL fRepaint);
	virtual HMENU GetMenu (VOID);
	virtual HRESULT AttachSubclassHandler (ISubclassedHandler* lpSubclass);
	virtual HRESULT DetachSubclassHandler (ISubclassedHandler* lpSubclass);
	virtual HRESULT RegisterUserMessage (__out UINT* pnUserMessage);
	virtual HRESULT UnregisterUserMessage (UINT nUserMessage);
	virtual HRESULT RegisterTimer (UINT msElapse, __out UINT_PTR* pidTimer);
	virtual HRESULT UnregisterTimer (UINT_PTR idTimer);

	// AdjustWindowSize() accepts CW_USEDEFAULT for x or y.
	static HRESULT AdjustWindowSize (DWORD dwStyle, DWORD dwExStyle, BOOL fMenu, __inout INT& x, __inout INT& y, __inout INT& nWidth, __inout INT& nHeight);

protected:
	virtual HINSTANCE GetInstance (VOID) = 0;
	virtual VOID OnFinalDestroy (HWND hwnd) = 0;

	virtual HRESULT FinalizeAndShow (DWORD dwStyle, INT nCmdShow);
	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

	HRESULT Create (DWORD dwExStyle, DWORD dwStyle, LPCTSTR lpcszClass, LPCTSTR lpcszTitle, INT x, INT y, INT nWidth, INT nHeight, HWND hwndParent, INT nCmdShow);
	HRESULT CreateWithId (DWORD dwExStyle, DWORD dwStyle, LPCTSTR lpcszClass, LPCTSTR lpcszTitle, INT x, INT y, INT nWidth, INT nHeight, HWND hwndParent, INT nCmdShow, WORD wControlId);
	HRESULT AttachBaseWinProc (VOID);
	VOID ClearAllSubclasses (VOID);

	static HRESULT RegisterClass (const WNDCLASSEX* lpWndClass, ATOM* lpAtom);
	static HRESULT UnregisterClass (LPCTSTR lpcszClass, HINSTANCE hInstance);

private:
	BOOL InnerWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

	static LRESULT WINAPI _DefWinProcCreate (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
	static LRESULT WINAPI _DefWinProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
};

template <typename TControl>
class TDialogControl : public CBaseWindow
{
public:
	TDialogControl () {}
	~TDialogControl () {}

protected:
	static HRESULT RegisterControl (const WNDCLASSEX* pWndClass, ATOM* pAtom)
	{
		HRESULT hr = E_INVALIDARG;
		if(pWndClass && pWndClass->lpszClassName && pWndClass->hInstance)
		{
			WNDCLASSEX wnd;
			ATOM atom;

			CopyMemory(&wnd, pWndClass, sizeof(wnd));
			wnd.lpfnWndProc = _ControlCreate;

			atom = ::RegisterClassEx(&wnd);
			if(pAtom)
				*pAtom = atom;

			if(0 == atom)
				hr = HRESULT_FROM_WIN32(GetLastError());
			else
				hr = S_OK;
		}
		return hr;
	}

private:
	static LRESULT WINAPI _ControlCreate (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		LRESULT lResult = 0;
		if(FIRST_WM_MESSAGE == message)
		{
			LPCREATESTRUCT lpCreate = (LPCREATESTRUCT)lParam;
			TControl* pCtrl = __new TControl;
			if(pCtrl)
			{
				CBaseWindow* pWindow = pCtrl;	// Ensure the correct pointer is used.
				pWindow->m_hwnd = hwnd;
				SetWindowLongPtr(hwnd,GWL_USERDATA,(__int3264)(LONG_PTR)pWindow);
				SetWindowLongPtr(hwnd,GWL_WNDPROC,(__int3264)(LONG_PTR)_DefWinProc);
				lResult = _DefWinProc(hwnd,message,wParam,lParam);
			}
			else
				DestroyWindow(hwnd);
		}
		else
			lResult = ::DefWindowProc(hwnd,message,wParam,lParam);
		return lResult;
	}
};
