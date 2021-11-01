#pragma once

#include "Library\Window\BaseDialog.h"

class CInputPad;

class CInputPadProps : public CBaseDialog
{
protected:
	CInputPad* m_lpInputPad;
	HWND m_hwndSetValue;
	HWND m_hwndClearValue;

public:
	CInputPadProps (CInputPad* lpInputPad);
	~CInputPadProps ();

protected:
	VOID DlgCreate (VOID);
	VOID DlgCommand (USHORT id, USHORT iCode);

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);
};