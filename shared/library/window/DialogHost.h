#pragma once

#include "PlatformCompat.h"

class CBaseDialog;

class CDialogHost : public IOleWindow
{
#ifdef	DIALOG_HOST_PROPERTY_SHEETS
private:
	static CDialogHost* m_pPropertySheetContext;
#endif

private:
	ULONG m_cRef;

	HINSTANCE m_hInstance;
	HWND m_hwnd;
	DWORD m_dwReturnValue;

	BOOL m_fHasFrame;
	BOOL m_fModeless;

	CBaseDialog** m_lpPages;
	INT m_cPages;

public:
	CDialogHost (HINSTANCE hInstance);
	virtual ~CDialogHost ();

	// IUnknown
	HRESULT WINAPI QueryInterface (REFIID iid, LPVOID* lplpvObject);
	ULONG WINAPI AddRef (VOID);
	ULONG WINAPI Release (VOID);

	// IOleWindow
	HRESULT WINAPI GetWindow (HWND* lphwnd);
	HRESULT WINAPI ContextSensitiveHelp (BOOL fEnterMode);

	VOID SetReturnValue (DWORD dwValue);
	DWORD GetReturnValue (VOID);

	HRESULT CreateModeless (HWND hwnd, CBaseDialog* lpPage);
	HRESULT Display (HWND hwnd, CBaseDialog* lpPage);
#ifdef	DIALOG_HOST_PROPERTY_SHEETS
	HRESULT DisplaySheet (HWND hwnd, LPTSTR lpszTitle, INT nIconRes, CBaseDialog** lpPages, INT cPages, BOOL fWizard = FALSE);
#endif

	BOOL IsDialogMessage (LPMSG lpMsg);

	BOOL Center (VOID);
	VOID Destroy (VOID);

	HINSTANCE GetInstance (VOID);

private:
	HRESULT CreatePageArray (CBaseDialog** lpPages, INT cPages);

#ifdef	DIALOG_HOST_PROPERTY_SHEETS
	static INT CALLBACK DlgSheetProcedure (HWND hDlg, UINT message, LPARAM lParam);
#endif
	static DLGRESULT CALLBACK DlgProcedureCreate (HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
};
