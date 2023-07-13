#pragma once

#include "Library\Window\BaseDialog.h"

interface IJSONObject;

class CRunParamsDlg :
	public CBaseDialog
{
private:
	IJSONObject* m_pProject;
	RSTRING m_rstrEngine;
	RSTRING m_rstrProjectDir;

public:
	CRunParamsDlg (IJSONObject* pProject, RSTRING rstrEngine, RSTRING rstrProjectDir);
	~CRunParamsDlg ();

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

private:
	HRESULT ReadArgsAndDir (VOID);
	HRESULT WriteArgsAndDir (VOID);
};
