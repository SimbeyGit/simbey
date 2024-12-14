#pragma once

#include "Library\Core\RStrMap.h"
#include "PaintItems.h"
#include "ConfigDlg.h"

class CPropertiesDlg : public CBaseDialog
{
private:
	CConfigDlg* m_pdlgConfig;
	INT m_nLighting;
	TRStrMap<TEXTURE*>& m_mapTextures;

public:
	CPropertiesDlg (CConfigDlg* pdlgConfig, INT nLighting, TRStrMap<TEXTURE*>& mapTextures);
	~CPropertiesDlg ();

	INT GetLighting (VOID) { return m_nLighting; }

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

private:
	HRESULT BrowseFlats (UINT idField, PCWSTR pcwzPrefix);
};
