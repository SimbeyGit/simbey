#include <windows.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Window\DialogHost.h"
#include "Published\SIF.h"
#include "Selector\SelectSIFDlg.h"
#include "PropertiesDlg.h"

CPropertiesDlg::CPropertiesDlg (CConfigDlg* pdlgConfig, INT nLighting, TRStrMap<TEXTURE*>& mapTextures) :
	CBaseDialog(IDD_PROPERTIES),
	m_pdlgConfig(pdlgConfig),
	m_nLighting(nLighting),
	m_mapTextures(mapTextures)
{
}

CPropertiesDlg::~CPropertiesDlg ()
{
}

BOOL CPropertiesDlg::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	BOOL fHandled = FALSE;
	HWND hwnd;

	switch(message)
	{
	case WM_INITDIALOG:
		GetWindow(&hwnd);
		SetDlgItemInt(hwnd, IDC_LIGHTING, m_nLighting, TRUE);

		SetDlgItemText(hwnd, IDC_CEILING_NAME, m_pdlgConfig->m_wzCeilingName);
		SetDlgItemText(hwnd, IDC_FLOOR_NAME, m_pdlgConfig->m_wzFloorName);
		SetDlgItemText(hwnd, IDC_CUTOUT_NAME, m_pdlgConfig->m_wzCutoutName);

		SetFocus(GetDlgItem(IDC_FLOOR_NAME));

		CenterHost();
		break;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDC_BROWSE_FLOOR:
			BrowseFlats(IDC_FLOOR_NAME, L"FLOOR");
			break;
		case IDC_BROWSE_CEILING:
			BrowseFlats(IDC_CEILING_NAME, L"CEIL");
			break;
		case IDC_BROWSE_CUTOUT:
			BrowseFlats(IDC_CUTOUT_NAME, NULL);
			break;
		case IDOK:
			{
				BOOL fSuccess;

				GetWindow(&hwnd);
				m_nLighting = GetDlgItemInt(hwnd, IDC_LIGHTING, &fSuccess, TRUE);
				if(!fSuccess)
					break;
				GetDlgItemText(hwnd, IDC_CEILING_NAME, m_pdlgConfig->m_wzCeilingName, ARRAYSIZE(m_pdlgConfig->m_wzCeilingName));
				GetDlgItemText(hwnd, IDC_FLOOR_NAME, m_pdlgConfig->m_wzFloorName, ARRAYSIZE(m_pdlgConfig->m_wzFloorName));
				GetDlgItemText(hwnd, IDC_CUTOUT_NAME, m_pdlgConfig->m_wzCutoutName, ARRAYSIZE(m_pdlgConfig->m_wzCutoutName));
			}
			__fallthrough;
		case IDCANCEL:
			End(LOWORD(wParam));
			break;
		}
		break;
	}

	return fHandled;
}

HRESULT CPropertiesDlg::BrowseFlats (UINT idField, PCWSTR pcwzPrefix)
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
	if(0 == GetDlgItemText(hwnd, idField, wzName, ARRAYSIZE(wzName)))
		wzName[0] = L'\0';

	Check(sifCreateNew(&srSIF));

	for(sysint i = 0; i < m_mapTextures.Length(); i++)
	{
		RSTRING rstrName;
		TEXTURE* pTexture;

		Check(m_mapTextures.GetKeyAndValue(i, &rstrName, &pTexture));

		if(NULL == pcwzPrefix || TCompareLeftIAssert(pTexture->pcwzName, pcwzPrefix))
		{
			TStackRef<ISimbeyInterchangeFileLayer> srLayer;
			Check(srSIF->AddLayerFromBits(pTexture->xSize, pTexture->ySize, pTexture->stmBits32.TGetReadPtr<BYTE>(), 32, pTexture->xSize * 4, &srLayer, NULL));
			Check(srLayer->SetName(pTexture->pcwzName));

			if(0 == idLayer && 0 == TStrCmpAssert(wzName, pTexture->pcwzName))
				idLayer = srLayer->GetLayerID();
		}
	}

	Check(RStrFormatW(&rstrTitle, L"Tiles: %ls", pcwzPrefix ? pcwzPrefix : L"All Textures"));
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

		SetDlgItemText(hwnd, idField, wzName);
	}

Cleanup:
	RStrRelease(rstrTitle);
	if(srSIF)
		srSIF->Close();
	return hr;
}
