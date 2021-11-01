#pragma once

#include "Library\Window\BaseDialog.h"

class CInputPad;

class CInputPadSize : public CBaseDialog
{
protected:
	CInputPad* m_lpInputPad;
	HWND m_hwndSize;

public:
	CInputPadSize (CInputPad* lpInputPad);
	~CInputPadSize ();

protected:
	VOID DlgCreate (VOID);
	VOID DlgCommand (USHORT id, USHORT iCode);

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);
};