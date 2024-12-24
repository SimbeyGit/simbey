#pragma once

#include "Library\Core\RStrMap.h"
#include "Library\Window\BaseDialog.h"
#include "PaintItems.h"

class CReplaceWallDlg : public CBaseDialog
{
private:
	TRStrMap<TEXTURE*>& m_mapTextures;
	WCHAR m_wzOld[16], m_wzNew[16];

public:
	CReplaceWallDlg (TRStrMap<TEXTURE*>& mapTextures);
	~CReplaceWallDlg ();

	PCWSTR GetOldTexture (VOID) { return m_wzOld; }
	PCWSTR GetNewTexture (VOID) { return m_wzNew; }

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

private:
	HRESULT ReadTextures (VOID);
	HRESULT SelectTexture (INT idTextureCtrl);
};
