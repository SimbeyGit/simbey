#pragma once

#include "Library\Window\BaseDialog.h"

class CEditLinksDlg : public CBaseDialog
{
protected:
	INeuralLinks* m_lpLinks;
	LPCSTR m_lpcszSelectedLink;
	BOOL m_fMadeChanges;

	HWND m_hLinks;
	HWND m_hSources;
	HWND m_hName;

public:
	CEditLinksDlg (INeuralLinks* lpLinks, LPCSTR lpcszSelectedLink);
	virtual ~CEditLinksDlg ();

	VOID DlgCommand (USHORT id, USHORT iCode);

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

protected:
	VOID RefreshLinks (VOID);
	BOOL GetSelectedLink (__out_ecount(cchMaxLink) LPSTR lpszLink, INT cchMaxLink);
};