#pragma once

#include "Library\Window\BaseDialog.h"

class CGetTextDlg : public CBaseDialog
{
protected:
	LPSTR m_lpszText;

public:
	CGetTextDlg ();
	virtual ~CGetTextDlg ();

	HRESULT SetText (LPCSTR lpcszText);
	LPCSTR GetText (VOID);

protected:
	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);
};