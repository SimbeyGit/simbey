#pragma once

#include "BaseWindow.h"

#define	ADAPTER_WINDOW_CLASS		L"AdapterWindowCls"

interface __declspec(uuid("C671D45E-3675-415a-A90D-B50A43FA8EC6")) IAdapterWindowCallback : public IUnknown
{
	// Named DefWindowProc for compatibility with BEGIN_WM_MAP/END_WM_MAP.
	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult) = 0;

	virtual VOID OnAttachingAdapter (IBaseWindow* pAdapter) = 0;
	virtual VOID OnDetachingAdapter (IBaseWindow* pAdapter) = 0;
};

class CAdapterWindow : public CBaseWindow
{
private:
	ULONG m_cRef;

protected:
	HINSTANCE m_hInstance;
	IAdapterWindowCallback* m_pCallback;

public:
	CAdapterWindow ();
	virtual ~CAdapterWindow ();

	// IUnknown
	HRESULT WINAPI QueryInterface (REFIID iid, LPVOID* lplpvObject);
	ULONG WINAPI AddRef (VOID);
	ULONG WINAPI Release (VOID);

	HRESULT AttachAsChild (HINSTANCE hInstance, __in IAdapterWindowCallback* pCallback, PCWSTR pcwzClass, PCWSTR pcwzData, INT x, INT y, INT nWidth, INT nHeight, HWND hwndParent, INT nCmdShow);
	HRESULT Attach (HINSTANCE hInstance, __in IAdapterWindowCallback* pCallback, DWORD dwExStyles, DWORD dwStyles, PCWSTR pcwzClass, PCWSTR pcwzData, INT x, INT y, INT nWidth, INT nHeight, HWND hwndParent, INT nCmdShow);

	static HRESULT Register (PCWSTR pcwzClass, HINSTANCE hInstance);
	static HRESULT Unregister (PCWSTR pcwzClass, HINSTANCE hInstance);

protected:
	virtual HINSTANCE GetInstance (VOID);
	virtual VOID OnFinalDestroy (HWND hwnd);

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);
};

class CDialogControlAdapter : public TDialogControl<CDialogControlAdapter>
{
private:
	ULONG m_cRef;

	IAdapterWindowCallback* m_pCallback;

public:
	CDialogControlAdapter ();
	virtual ~CDialogControlAdapter ();

	// IUnknown
	HRESULT WINAPI QueryInterface (REFIID iid, LPVOID* lplpvObject);
	ULONG WINAPI AddRef (VOID);
	ULONG WINAPI Release (VOID);

	static HRESULT Register (PCWSTR pcwzClass, HINSTANCE hInstance, __out ATOM* pAtom);
	static HRESULT Unregister (PCWSTR pcwzClass, HINSTANCE hInstance);
	static HRESULT Attach (HWND hwnd, IAdapterWindowCallback* pCallback);

	VOID AttachCallback (IAdapterWindowCallback* pCallback);

private:
	virtual HINSTANCE GetInstance (VOID);
	virtual VOID OnFinalDestroy (HWND hwnd);

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);
};
