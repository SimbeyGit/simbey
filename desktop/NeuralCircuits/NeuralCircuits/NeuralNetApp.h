#pragma once

#include <docobj.h>
#include "Library\Window\BaseWindow.h"
#include "BaseContainer.h"

class CNeuralDocument;

class CNeuralNetApp : public CBaseWindow, public IOleCommandTarget, public IBaseContainer
{
private:
	ULONG m_cRef;

protected:
	HINSTANCE m_hInstance;
	HMENU m_hMenu;

	BOOL m_fCapture;
	CNeuralDocument* m_lpDoc;
	HMODULE m_hOleAcc;

	IWindowless** m_lpTimers;
	INT m_cActiveTimers;
	INT m_cMaxTimers;

public:
	CNeuralNetApp (HINSTANCE hInstance);
	~CNeuralNetApp ();

	HRESULT Initialize (INT nWidth, INT nHeight, INT nCmdShow);
	VOID Uninitialize (VOID);

	HRESULT RegisterFileType (VOID);
	HRESULT UnregisterFileType (VOID);
	HRESULT ExecCommand (LPCSTR lpcszCommand);
	HRESULT UpdateAppTitle (VOID);
	BOOL PreTranslateMessage (LPMSG lpmsg);

	// IUnknown
	HRESULT WINAPI QueryInterface (REFIID iid, LPVOID* lplpvObject);
	ULONG WINAPI AddRef (VOID);
	ULONG WINAPI Release (VOID);

	// IOleCommandTarget
	HRESULT WINAPI QueryStatus (const GUID* lpguidCmdGroup, ULONG cCmds, OLECMD* lprgCmds, OLECMDTEXT* lpCmdText);
	HRESULT WINAPI Exec (const GUID* lpguidCmdGroup, DWORD nCmdID, DWORD nCmdExecOpt, VARIANTARG* lpvaIn, VARIANTARG* lpvaOut);

	// IBaseContainer
	virtual HRESULT WINAPI GetHwnd (HWND* lphwnd);
	virtual HRESULT WINAPI GetCapture (IWindowless** lplpObject);
	virtual HRESULT WINAPI SetCapture (IWindowless* lpObject, BOOL fCapture);
	virtual HRESULT WINAPI GetFocus (IWindowless** lplpObject);
	virtual HRESULT WINAPI SetFocus (IWindowless* lpObject);
	virtual HRESULT WINAPI SetTimer (IWindowless* lpObject, UINT uElapse, INT* lpnTimer);
	virtual HRESULT WINAPI KillTimer (INT nTimer);
	virtual HRESULT WINAPI GetDC (IWindowless* lpObject, HDC* lphdc);
	virtual HRESULT WINAPI ReleaseDC (IWindowless* lpObject, HDC hdc);
	virtual HRESULT WINAPI InvalidateContainer (VOID);
	virtual HRESULT WINAPI OnWindowlessMessage (IWindowless* lpObject, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* lplResult);
	virtual HRESULT WINAPI ScreenToWindowless (IWindowless* lpObject, LPPOINT lpPoint);
	virtual HRESULT WINAPI TrackPopupMenu (HMENU hMenu, INT x, INT y, IOleCommandTarget* lpTarget);

	static HRESULT Register (HINSTANCE hInstance);

protected:
	virtual HINSTANCE GetInstance (VOID);
	virtual VOID OnFinalDestroy (HWND hwnd);

	virtual HRESULT FinalizeAndShow (DWORD dwStyle, INT nCmdShow);
	virtual HMENU GetMenu (VOID);

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);
};