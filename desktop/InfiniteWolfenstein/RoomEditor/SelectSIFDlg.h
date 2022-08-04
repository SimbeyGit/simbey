#pragma once

#include "Library\Util\RString.h"
#include "Library\Window\BaseDialog.h"
#include "Published\SIF.h"
#include "SIFSelector.h"

class CSelectSIFDlg : public CBaseDialog
{
private:
	CSIFSelector* m_pSelector;

public:
	CSelectSIFDlg ();
	~CSelectSIFDlg ();

	HRESULT Initialize (VOID);
	HRESULT AddSIF (RSTRING rstrTitle, ISimbeyInterchangeFile* pSIF);
	HRESULT GetSelection (__out RSTRING* prstrTitle, __out DWORD* pidSelection);

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);
};
