#pragma once

#include "Library\Util\RString.h"
#include "Library\Window\BaseDialog.h"
#include "Published\SIF.h"
#include "SIFSelector.h"

class CSelectSIFDlg : public CBaseDialog
{
private:
	CSIFSelector* m_pSelector;
	UINT m_idItems;

public:
	CSelectSIFDlg (UINT idDialog, UINT idItems);
	~CSelectSIFDlg ();

	HRESULT Initialize (VOID);
	HRESULT AddSIF (RSTRING rstrTitle, ISimbeyInterchangeFile* pSIF);
	VOID SetSelection (RSTRING rstrTitle, DWORD idLayer);
	HRESULT GetSelection (__out RSTRING* prstrTitle, __out DWORD* pidSelection);

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);
};
