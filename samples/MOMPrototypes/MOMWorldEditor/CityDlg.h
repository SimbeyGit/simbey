#pragma once

#include "Library\Window\BaseDialog.h"

class CCityDlg : public CBaseDialog
{
public:
	IJSONObject* m_pCity;

public:
	CCityDlg (IJSONObject* pCity);
	~CCityDlg ();

private:
	HRESULT LoadData (VOID);
	HRESULT SaveData (VOID);

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);
};
