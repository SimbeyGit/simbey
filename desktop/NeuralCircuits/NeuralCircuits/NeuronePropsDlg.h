#pragma once

#include "Library\Window\BaseDialog.h"

class CNeuronePropsDlg : public CBaseDialog
{
protected:
	INeuralLinks* m_lpLinks;
	INeurone* m_lpNeurone;
	HWND m_hThreshold;
	HWND m_hPin;
	HWND m_hLink;
	HWND m_hEditLinks;
	HWND m_hBias;

	INT m_nDefaultPin;

public:
	CNeuronePropsDlg (INeuralLinks* lpLinks, INeurone* lpNeurone);
	~CNeuronePropsDlg ();

protected:
	VOID EditLinks (VOID);
	VOID FillLinks (VOID);

	VOID Save (VOID);

	VOID DlgCreate (VOID);
	VOID DlgCommand (USHORT id, USHORT iCode);

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);
};