#pragma once

namespace WinUtil
{
	HRESULT WINAPI GetWindowText (HWND hwnd, LPSTR* lplpszText, INT* lpcchText);

	DOUBLE WINAPI ReadDoubleFromWindow (HWND hwnd);
	HRESULT WINAPI SetWindowDouble (HWND hwnd, DOUBLE fValue);

	INT WINAPI ReadIntFromWindow (HWND hwnd);
	HRESULT WINAPI SetWindowInt (HWND hwnd, INT nValue);

	VOID WINAPI SetListViewStyleEx (HWND hwnd, ULONG ulFlags);
}
