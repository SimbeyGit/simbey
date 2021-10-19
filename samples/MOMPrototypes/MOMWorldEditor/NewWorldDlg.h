#pragma once

#include "Library\Window\BaseDialog.h"

class CNewWorldDlg : public CBaseDialog
{
public:
	INT m_nWidth, m_nHeight;

public:
	CNewWorldDlg (INT nWidth, INT nHeight);

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);
};
