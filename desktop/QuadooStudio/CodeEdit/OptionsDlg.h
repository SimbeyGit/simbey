#pragma once

#include "Library\Window\BaseDialog.h"
#include "Published\CodeEdit.h"

class COptionsDlg :
	public CBaseDialog
{
public:
	LOGFONT m_lfEdit;
	COLORREF m_rgbCustColors[16];

private:
	static UINT_PTR m_idLastSubclass;

	COLORREF* m_prgColors;
	COLORREF m_rgbTempColorList[TXC_MAX_COLORS];
	COLORREF m_rgbAutoColorList[TXC_MAX_COLORS];

	COLORREF m_crPreviewFG, m_crPreviewBG;

	HICON m_hIcon2, m_hIcon3;
	HFONT m_hNormalFont, m_hBoldFont, m_hPreviewFont;

	UINT_PTR m_idPreviewSubclass;
	INT m_nFontPointSize;

public:
	COptionsDlg (COLORREF* prgColors);
	~COptionsDlg ();

	inline INT GetFontPointSize (VOID) { return m_nFontPointSize; }

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

private:
	void SetComboItemHeight (HWND hwndCombo, int nMinHeight);
	void FillFontComboList (HWND hwndCombo);
	void UpdatePreviewPane (HWND hwnd);
	void InitSizeList (HWND hwnd);
	void SelectColorInList (UINT uComboIdx, short itemIdx);

	VOID InitializeFontOptions (VOID);
	BOOL FontCombo_DrawItem (DRAWITEMSTRUCT* dis);
	BOOL ColorCombo_DrawItem (UINT_PTR uCtrlId, DRAWITEMSTRUCT* dis, BOOL fSelectImage);
	VOID OnPreviewPaint (HWND hwndPreview);

	static int CALLBACK EnumFontSizes (ENUMLOGFONTEX* lpelfe, NEWTEXTMETRICEX* lpntme, DWORD FontType, LPARAM lParam);
	static int CALLBACK EnumFontNames (ENUMLOGFONTEX* lpelfe, NEWTEXTMETRICEX* lpntme, DWORD FontType, LPARAM lParam);
	static LRESULT CALLBACK PreviewWndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
};
