#include <windows.h>
#include <commctrl.h>
#include "..\Core\CoreDefs.h"
#include "Formatting.h"
#include "WinUtil.h"

#define	LVM_GETEXTENDEDSTYLE		(LVM_FIRST + 55)
#define	LVM_SETEXTENDEDSTYLE		(LVM_FIRST + 54)

namespace WinUtil
{
	HRESULT WINAPI GetWindowText (HWND hwnd, LPSTR* lplpszText, INT* lpcchText)
	{
		HRESULT hr;

		INT cchText = GetWindowTextLength(hwnd);
		if(0 < cchText)
		{
			*lplpszText = __new CHAR[cchText + 1];
			if(*lplpszText)
			{
				::GetWindowTextA(hwnd,*lplpszText,cchText + 1);
				hr = S_OK;
			}
			else
				hr = E_OUTOFMEMORY;
		}
		else
		{
			*lplpszText = NULL;
			hr = S_OK;
		}

		if(lpcchText)
			*lpcchText = cchText;

		return hr;
	}

	DOUBLE WINAPI ReadDoubleFromWindow (HWND hwnd)
	{
		CHAR szValue[64], *lpszPtr = szValue;
		::GetWindowTextA(hwnd,szValue,64);
		while(*lpszPtr == '$' || *lpszPtr == 32)
			lpszPtr++;
		if(*lpszPtr == 0)
			return 0;
		return atof(lpszPtr);
	}

	HRESULT WINAPI SetWindowDouble (HWND hwnd, DOUBLE fValue)
	{
		HRESULT hr;
		CHAR szValue[64];

		Check(Formatting::TDoubleToString(fValue, szValue, ARRAYSIZE(szValue), 32, NULL));
		CheckIfGetLastError(!SetWindowTextA(hwnd,szValue));

	Cleanup:
		return hr;
	}

	INT WINAPI ReadIntFromWindow (HWND hwnd)
	{
		CHAR szValue[64];
		return 0 < GetWindowTextA(hwnd,szValue,64) ? Formatting::TAscToInt32(szValue) : 0;
	}

	HRESULT WINAPI SetWindowInt (HWND hwnd, INT nValue)
	{
		HRESULT hr;
		CHAR szValue[64];

		Check(Formatting::TInt32ToAsc(nValue, szValue, ARRAYSIZE(szValue), 10, NULL));
		CheckIfGetLastError(!SetWindowTextA(hwnd,szValue));

	Cleanup:
		return hr;
	}

	VOID WINAPI SetListViewStyleEx (HWND hwnd, ULONG ulFlags)
	{
		ULONG ulStyle = (ULONG)SendMessage(hwnd,LVM_GETEXTENDEDSTYLE,0,0);
		ulStyle |= ulFlags;
		SendMessage(hwnd,LVM_SETEXTENDEDSTYLE,0,ulStyle);
	}
}
