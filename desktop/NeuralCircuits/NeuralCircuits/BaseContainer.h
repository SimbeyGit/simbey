#pragma once

interface IOleCommandTarget;
interface IWindowless;

// {0F1F1F7F-1251-48bb-83E8-86AC93B0AD76}
extern const GUID IID_IBaseContainer;

interface IBaseContainer : public IUnknown
{
	virtual HRESULT WINAPI GetHwnd (HWND* lphwnd) = 0;
	virtual HRESULT WINAPI GetCapture (IWindowless** lplpObject) = 0;
	virtual HRESULT WINAPI SetCapture (IWindowless* lpObject, BOOL fCapture) = 0;
	virtual HRESULT WINAPI GetFocus (IWindowless** lplpObject) = 0;
	virtual HRESULT WINAPI SetFocus (IWindowless* lpObject) = 0;
	virtual HRESULT WINAPI SetTimer (IWindowless* lpObject, UINT uElapse, INT* lpnTimer) = 0;
	virtual HRESULT WINAPI KillTimer (INT nTimer) = 0;
	virtual HRESULT WINAPI GetDC (IWindowless* lpObject, HDC* lphdc) = 0;
	virtual HRESULT WINAPI ReleaseDC (IWindowless* lpObject, HDC hdc) = 0;
	virtual HRESULT WINAPI InvalidateContainer (VOID) = 0;
	virtual HRESULT WINAPI OnWindowlessMessage (IWindowless* lpObject, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* lplResult) = 0;
	virtual HRESULT WINAPI ScreenToWindowless (IWindowless* lpObject, LPPOINT lpPoint) = 0;
	virtual HRESULT WINAPI TrackPopupMenu (HMENU hMenu, INT x, INT y, IOleCommandTarget* lpTarget) = 0;
};