#pragma once

#include "Library\Core\BaseUnknown.h"
#include "Library\Window\BaseDialog.h"

class CConfigDlg : public CBaseDialog
{
public:
	WCHAR m_wzTexturePath[MAX_PATH];
	WCHAR m_wzBehaviorPath[MAX_PATH];
	WCHAR m_wzZDBSPPath[MAX_PATH];

	WCHAR m_wzFloorName[12];
	WCHAR m_wzCeilingName[12];
	WCHAR m_wzCutoutName[12];

public:
	CConfigDlg ();
	~CConfigDlg ();

	HRESULT Load (PCWSTR pcszRegPath);
	HRESULT Save (PCWSTR pcszRegPath);

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

private:
	static HRESULT FindAnyPK3 (PCWSTR pcwzFolder, __out_ecount(cchMaxPK3) PWSTR pwzPK3, INT cchMaxPK3);

	HRESULT ReadFromDialog (VOID);
	HRESULT ReadAndVerifyPath (INT idControl, PWSTR pwzPath);
};
