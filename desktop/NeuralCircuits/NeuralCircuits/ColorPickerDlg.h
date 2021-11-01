#pragma once

#include "Library\Window\BaseDialog.h"

class CColorPickerDlg : public CBaseDialog
{
public:
	static COLORREF m_crCustomColors[16];

protected:
	COLORREF m_crSolid;
	BYTE m_bAlpha;

public:
	CColorPickerDlg ();
	virtual ~CColorPickerDlg ();

	VOID SetSolidColor (COLORREF crSolid);
	VOID SetAlphaChannel (BYTE bAlpha);

	COLORREF GetSolidColor (VOID);
	BYTE GetAlphaChannel (VOID);

protected:
	VOID FillColors (VOID);
	VOID SelectColorByIndex (INT nSelected);
	BOOL ReadAlphaChannel (VOID);

	VOID DrawColorSwatch (LPDRAWITEMSTRUCT lpdis);

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);
};