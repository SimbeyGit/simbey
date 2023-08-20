#pragma once

#include "Library\Window\BaseDialog.h"

interface IJSONObject;

class CWebParamsDlg :
	public CBaseDialog
{
private:
	IJSONObject* m_pProject;
	RSTRING m_rstrEngine;
	RSTRING m_rstrProjectDir;

public:
	CWebParamsDlg (IJSONObject* pProject, RSTRING rstrEngine, RSTRING rstrProjectDir);
	~CWebParamsDlg ();

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

private:
	HRESULT ReadServerAndPath (VOID);
	HRESULT WriteServerAndPath (VOID);
};
