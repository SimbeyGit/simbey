#pragma once

class CModuleCookie
{
private:
	HMODULE m_hModule;
	PVOID m_pvCookie;

public:
	CModuleCookie ();
	~CModuleCookie ();

	HRESULT Load (PCWSTR pcwzModule);
	VOID Free (VOID);

	HMODULE operator* ()
	{
		return m_hModule;
	}
};
