#include <windows.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Window\DialogHost.h"
#include "Published\SIF.h"
#include "Selector\SelectSIFDlg.h"
#include "ReplaceWallDlg.h"

CReplaceWallDlg::CReplaceWallDlg (TRStrMap<TEXTURE*>& mapTextures) :
	CBaseDialog(IDD_REPLACE_WALLS),
	m_mapTextures(mapTextures)
{
	m_wzOld[0] = L'\0';
	m_wzNew[0] = L'\0';
}

CReplaceWallDlg::~CReplaceWallDlg ()
{
}

BOOL CReplaceWallDlg::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	BOOL fHandled = FALSE;

	switch(message)
	{
	case WM_INITDIALOG:
		CenterHost();
		break;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			if(FAILED(ReadTextures()))
				break;
			__fallthrough;
		case IDCANCEL:
			End(LOWORD(wParam));
			break;

		case IDC_SELECT_OLD_TEXTURE:
			SelectTexture(IDC_OLD_TEXTURE);
			break;
		case IDC_SELECT_NEW_TEXTURE:
			SelectTexture(IDC_NEW_TEXTURE);
			break;
		}
		break;
	}

	return fHandled;
}

HRESULT CReplaceWallDlg::ReadTextures (VOID)
{
	HRESULT hr;
	HWND hwnd;

	Check(GetWindow(&hwnd));
	CheckIf(0 == GetDlgItemText(hwnd, IDC_OLD_TEXTURE, m_wzOld, ARRAYSIZE(m_wzOld)), E_FAIL);
	CheckIf(0 == GetDlgItemText(hwnd, IDC_NEW_TEXTURE, m_wzNew, ARRAYSIZE(m_wzNew)), E_FAIL);

Cleanup:
	return hr;
}

HRESULT CReplaceWallDlg::SelectTexture (INT idTextureCtrl)
{
	HRESULT hr;
	CDialogHost dlgHost(GetInstance());
	CSelectSIFDlg dlgSelect(IDD_SELECT_SIF, IDC_ITEMS);
	TStackRef<ISimbeyInterchangeFile> srSIF;
	RSTRING rstrTitle = NULL;
	HWND hwnd;
	DWORD idLayer = 0;
	WCHAR wzName[32];

	Check(GetWindow(&hwnd));
	if(0 == GetDlgItemText(hwnd, idTextureCtrl, wzName, ARRAYSIZE(wzName)))
		wzName[0] = L'\0';

	Check(sifCreateNew(&srSIF));

	for(sysint i = 0; i < m_mapTextures.Length(); i++)
	{
		RSTRING rstrName;
		TEXTURE* pTexture;

		Check(m_mapTextures.GetKeyAndValue(i, &rstrName, &pTexture));

		if(!TCompareLeftIAssert(pTexture->pcwzName, L"FLOOR") && !TCompareLeftIAssert(pTexture->pcwzName, L"CEIL"))
		{
			TStackRef<ISimbeyInterchangeFileLayer> srLayer;
			Check(srSIF->AddLayerFromBits(pTexture->xSize, pTexture->ySize, pTexture->stmBits32.TGetReadPtr<BYTE>(), 32, pTexture->xSize * 4, &srLayer, NULL));
			Check(srLayer->SetName(pTexture->pcwzName));

			if(0 == idLayer && 0 == TStrCmpAssert(wzName, pTexture->pcwzName))
				idLayer = srLayer->GetLayerID();
		}
	}

	Check(RStrCreateW(LSP(L"Wall Textures"), &rstrTitle));
	Check(dlgSelect.Initialize());
	Check(dlgSelect.AddSIF(rstrTitle, srSIF));
	if(0 != idLayer)
		dlgSelect.SetSelection(rstrTitle, idLayer);
	Check(dlgHost.Display(hwnd, &dlgSelect));
	CheckIfIgnore(IDOK != dlgHost.GetReturnValue(), E_ABORT);

	RStrRelease(rstrTitle); rstrTitle = NULL;
	Check(dlgSelect.GetSelection(&rstrTitle, &idLayer));

	{
		TStackRef<ISimbeyInterchangeFileLayer> srLayer;

		Check(srSIF->GetLayer(idLayer, &srLayer));
		Check(srLayer->GetName(wzName, ARRAYSIZE(wzName)));

		SetDlgItemText(hwnd, idTextureCtrl, wzName);
	}

Cleanup:
	RStrRelease(rstrTitle);
	if(srSIF)
		srSIF->Close();
	return hr;
}
