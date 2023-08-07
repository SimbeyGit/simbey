#pragma once

#include "Library\Window\BaseDialog.h"
#include "Published\QuadooParser.h"

class CGotoDefinitionDlg :
	public CBaseDialog
{
private:
	IQuadooDefinitions* m_pDefs;
	DWORD m_idxDef;

public:
	CGotoDefinitionDlg (IQuadooDefinitions* pDefs);
	~CGotoDefinitionDlg ();

	inline DWORD GetSelectedDef (VOID) { return m_idxDef; }

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

private:
	BOOL SetSelectedDef (VOID);
	HRESULT LoadDefs (VOID);
};
