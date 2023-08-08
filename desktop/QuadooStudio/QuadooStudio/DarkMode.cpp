#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "DarkMode.h"

bool IsColorLight (COLORREF cr)
{
	return (((5 * GetGValue(cr)) + (2 * GetRValue(cr)) + GetBValue(cr)) > (8 * 128));
}

CDarkMode::CDarkMode () :
	m_fDarkMode(false),
	m_fHasThemes(false)
{
}

CDarkMode::~CDarkMode ()
{
}

VOID CDarkMode::Update (VOID)
{
	HKEY hKey;
	if(ERROR_SUCCESS == RegOpenKey(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", &hKey))
	{
		DWORD dwType, dwValue = TRUE, cbValue = sizeof(dwValue);
		RegQueryValueExW(hKey, L"AppsUseLightTheme", NULL, &dwType, reinterpret_cast<PBYTE>(&dwValue), &cbValue);
		RegCloseKey(hKey);

		m_fDarkMode = FALSE == dwValue;
		m_fHasThemes = true;
	}
	else
	{
		m_fDarkMode = !IsColorLight(GetSysColor(COLOR_WINDOW));
		m_fHasThemes = false;
	}
}

bool CDarkMode::IsDarkMode (VOID)
{
	return m_fDarkMode;
}

bool CDarkMode::HasThemes (VOID)
{
	return m_fHasThemes;
}

VOID CDarkMode::StylizeTitleBar (HWND hwnd)
{
	HMODULE hDWM = LoadLibrary(L"DWMAPI.DLL");
	if(hDWM)
	{
		HRESULT (WINAPI* pfnDwmSetWindowAttribute)(HWND,DWORD,LPCVOID,DWORD);
		if(SUCCEEDED(TGetFunction(hDWM, "DwmSetWindowAttribute", &pfnDwmSetWindowAttribute)))
		{
			BOOL fValue = m_fDarkMode;
			pfnDwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &fValue, sizeof(fValue));
		}
		FreeLibrary(hDWM);
	}
}
