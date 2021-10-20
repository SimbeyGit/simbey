#include <windows.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Util\Formatting.h"
#include "Published\JSON.h"
#include "CityDlg.h"

CCityDlg::CCityDlg (IJSONObject* pCity) :
	CBaseDialog(IDD_CITY),
	m_pCity(pCity)
{
	m_pCity->AddRef();
}

CCityDlg::~CCityDlg ()
{
	m_pCity->Release();
}

HRESULT CCityDlg::LoadData (VOID)
{
	HRESULT hr = S_FALSE;
	TStackRef<IJSONValue> srv;
	RSTRING rstrName = NULL;

	if(SUCCEEDED(m_pCity->FindNonNullValueW(L"name", &srv)))
	{
		Check(srv->GetString(&rstrName));
		SetWindowText(GetDlgItem(IDC_NAME), RStrToWide(rstrName));
		srv.Release();
	}

	if(SUCCEEDED(m_pCity->FindNonNullValueW(L"population", &srv)))
	{
		WCHAR wzPopulation[16];
		INT nPopulation;
		Check(srv->GetInteger(&nPopulation));
		Check(Formatting::TInt32ToAsc(nPopulation, wzPopulation, ARRAYSIZE(wzPopulation), 10, NULL));
		SetWindowText(GetDlgItem(IDC_POPULATION), wzPopulation);
		srv.Release();
	}

	if(SUCCEEDED(m_pCity->FindNonNullValueW(L"flag", &srv)))
	{
		WCHAR wzFlag[16];
		INT nFlag;
		Check(srv->GetInteger(&nFlag));
		Check(Formatting::TInt32ToAsc(nFlag, wzFlag, ARRAYSIZE(wzFlag), 10, NULL));
		SetWindowText(GetDlgItem(IDC_FLAG), wzFlag);
		srv.Release();
	}

	if(SUCCEEDED(m_pCity->FindNonNullValueW(L"wall", &srv)))
	{
		bool fWall;
		Check(srv->GetBoolean(&fWall));
		if(fWall)
			SendMessage(GetDlgItem(IDC_WALL), BM_SETCHECK, BST_CHECKED, 0);
	}

Cleanup:
	RStrRelease(rstrName);
	return hr;
}

HRESULT CCityDlg::SaveData (VOID)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	WCHAR wzValue[256];
	INT cch, nPopulation, nFlag;

	cch = GetWindowText(GetDlgItem(IDC_NAME), wzValue, ARRAYSIZE(wzValue));
	CheckIf(0 == cch, E_FAIL);
	Check(JSONCreateStringW(wzValue, cch, &srv));
	Check(m_pCity->AddValueW(L"name", srv));
	srv.Release();

	cch = GetWindowText(GetDlgItem(IDC_POPULATION), wzValue, ARRAYSIZE(wzValue));
	CheckIf(0 == cch, E_FAIL);
	nPopulation = Formatting::TAscToInt32(wzValue);
	CheckIf(0 >= nPopulation, E_FAIL);
	Check(JSONCreateInteger(nPopulation, &srv));
	Check(m_pCity->AddValueW(L"population", srv));
	srv.Release();

	cch = GetWindowText(GetDlgItem(IDC_FLAG), wzValue, ARRAYSIZE(wzValue));
	CheckIf(0 == cch, E_FAIL);
	nFlag = Formatting::TAscToInt32(wzValue);
	CheckIf(0 > nFlag || nFlag > 16, E_FAIL);
	Check(JSONCreateInteger(nFlag, &srv));
	Check(m_pCity->AddValueW(L"flag", srv));

	if(BST_CHECKED & SendMessage(GetDlgItem(IDC_WALL), BM_GETCHECK, 0, 0))
	{
		Check(JSONParse(NULL, SLP(L"true"), &srv));
		Check(m_pCity->AddValueW(L"wall", srv));
	}
	else
		m_pCity->RemoveValueW(L"wall");

Cleanup:
	return hr;
}

BOOL CCityDlg::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	BOOL fHandled = FALSE;

	switch(message)
	{
	case WM_INITDIALOG:
		LoadData();
		SetFocus(GetDlgItem(IDC_NAME));
		CenterHost();
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			if(FAILED(SaveData()))
				break;
			__fallthrough;
		case IDCANCEL:
			End(LOWORD(wParam));
			fHandled = TRUE;
			break;
		case IDC_DELETE:
			End(LOWORD(wParam));
			fHandled = TRUE;
			break;
		}
		break;
	}

	return fHandled;
}
