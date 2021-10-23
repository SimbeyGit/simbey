#pragma once

#include <prsht.h>
#include "PlatformCompat.h"

class CDialogHost;

#pragma warning(push)
#pragma warning(disable:4512)

class CBaseDialog : public IOleWindow
{
private:
	ULONG m_cRef;

	CDialogHost* m_lpHost;
	HWND m_hwnd;

protected:
	const WORD m_idDialogRes;

public:
	CBaseDialog (WORD idDialogRes);
	virtual ~CBaseDialog ();

	// IUnknown
	HRESULT WINAPI QueryInterface (REFIID iid, LPVOID* lplpvObject);
	ULONG WINAPI AddRef (VOID);
	ULONG WINAPI Release (VOID);

	// IOleWindow
	HRESULT WINAPI GetWindow (HWND* lphwnd);
	HRESULT WINAPI ContextSensitiveHelp (BOOL fEnterMode);

	VOID AttachHost (CDialogHost* lpHost);
	HRESULT CreatePage (PROPSHEETPAGE* lpPropPage, HINSTANCE hInstance, BOOL fPropertySheet);

protected:
	BOOL CenterHost (VOID);
	HWND GetDlgItem (INT nItem);
	HWND GetFrame (VOID);
	HINSTANCE GetInstance (VOID);

	virtual VOID End (DWORD dwReturn);

	// This must be overridden.
	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult) = 0;

private:
	static DLGRESULT CALLBACK DlgProcedure (HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	static DLGRESULT CALLBACK DlgCreatePage (HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
};

#pragma warning(pop)
