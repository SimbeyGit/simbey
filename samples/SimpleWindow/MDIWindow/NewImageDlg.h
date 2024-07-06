#pragma once

#include "Library\Window\BaseDialog.h"

class CNewImageDlg : public CBaseDialog
{
public:
	SIZE m_size;
	COLORREF m_cr;

public:
	CNewImageDlg ();
	~CNewImageDlg ();

private:
	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);
};
