#pragma once

#include "Library\Window\BaseDialog.h"

interface INeurone;

class CConnectionsDlg : public CBaseDialog
{
protected:
	HWND m_hwndParent;
	INetDocObject* m_lpObject;
	ULONG m_iPin;

	BOOL m_bModified;

	HWND m_hList, m_hWeight;

public:
	CConnectionsDlg (HWND hwndParent, INetDocObject* lpObject, ULONG iPin);
	~CConnectionsDlg ();

protected:
	VOID DlgCreate (VOID);
	VOID DlgCommand (USHORT id, USHORT iCode);
	VOID DlgNotify (WPARAM id, NMHDR* lpnm);

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

	BOOL UpdateWeights (VOID);
	VOID Refresh (VOID);

	static VOID WINAPI ConnectionCallback (INT i, INeurone* lpSource, INeurone* lpTarget, ULONG iPin, FLOAT fWeight, LPVOID lpParam);
};