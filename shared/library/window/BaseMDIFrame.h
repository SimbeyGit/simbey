#pragma once

#include "BaseWindow.h"

BOOL FindSubMenu (HMENU hMenu, PCWSTR pcwzFindLabel, __out HMENU* phSubMenu);

class CBaseMDIFrame : public CBaseWindow
{
	friend class CBaseMDIChild;

protected:
	HWND m_hwndMDIClient;

public:
	CBaseMDIFrame ();
	virtual ~CBaseMDIFrame ();

	BOOL ProcessMDIMessage (MSG* pMsg);
	HWND GetActiveMDIChild (__out_opt BOOL* pfMaximized);
	HWND GetMDIClient (VOID) { return m_hwndMDIClient; }

protected:
	virtual VOID OnFinalDestroy (HWND hwnd);
	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);
	virtual BOOL SystemMessageHandler (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

	// Subclasses must implement GetMDIMenuData()
	virtual BOOL GetMDIMenuData (__out HMENU* phWindowMenu, __out UINT* pidFirstChild) = 0;

	static HRESULT RegisterMDIFrame (const WNDCLASSEX* lpWndClass, ATOM* lpAtom);

private:
	static LRESULT WINAPI _MDIFrameProcCreate (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
	static LRESULT WINAPI _MDIFrameProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
};

class CBaseMDIChild : public CBaseWindow
{
protected:
	HWND m_hwndMDIClient;

public:
	CBaseMDIChild ();
	virtual ~CBaseMDIChild ();

	BOOL Activate (VOID);
	BOOL Maximize (VOID);

	virtual HRESULT Destroy (VOID);

protected:
	HRESULT AttachToFrame (LPCTSTR pctstrClass, LPCTSTR pctstrTitle, DWORD dwStyle, INT x, INT y, INT nWidth, INT nHeight, CBaseMDIFrame* pFrame);

	virtual VOID OnFinalDestroy (HWND hwnd);
	virtual BOOL SystemMessageHandler (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

	static HRESULT RegisterMDIChild (const WNDCLASSEX* lpWndClass, ATOM* lpAtom);

private:
	static LRESULT WINAPI _MDIChildProcCreate (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
	static LRESULT WINAPI _MDIChildProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
};
