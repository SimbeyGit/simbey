#pragma once

namespace WinUtil
{
	HRESULT WINAPI GetWindowText (HWND hwnd, LPSTR* lplpszText, INT* lpcchText);

	DOUBLE WINAPI ReadDoubleFromWindow (HWND hwnd);
	HRESULT WINAPI SetWindowDouble (HWND hwnd, DOUBLE fValue);

	VOID WINAPI SetListViewStyleEx (HWND hwnd, ULONG ulFlags);
}
