#pragma once

class CDarkMode
{
private:
	bool m_fDarkMode;
	bool m_fHasThemes;

public:
	CDarkMode ();
	~CDarkMode ();

	VOID Update (VOID);

	bool IsDarkMode (VOID);
	bool HasThemes (VOID);
	VOID StylizeTitleBar (HWND hwnd);
};
