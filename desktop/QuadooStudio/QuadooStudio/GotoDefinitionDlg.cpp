#include <windows.h>
#include <commctrl.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Util\RString.h"
#include "Library\Util\Formatting.h"
#include "Library\DPI.h"
#include "GotoDefinitionDlg.h"

CGotoDefinitionDlg::CGotoDefinitionDlg (IQuadooDefinitions* pDefs) :
	CBaseDialog(IDD_GOTO_DEFINITION),
	m_pDefs(pDefs),
	m_idxDef((DWORD)-1)
{
	m_pDefs->AddRef();

	if(1 == pDefs->Count())
		m_idxDef = 0;
}

CGotoDefinitionDlg::~CGotoDefinitionDlg ()
{
	m_pDefs->Release();
}

BOOL CGotoDefinitionDlg::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	switch(message)
	{
	case WM_INITDIALOG:
		LoadDefs();
		CenterHost();
		break;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			if(!SetSelectedDef())
				break;
			__fallthrough;
		case IDCANCEL:
			End(LOWORD(wParam));
			break;
		}
		break;
	}

	return FALSE;
}

BOOL CGotoDefinitionDlg::SetSelectedDef (VOID)
{
	HWND hwndDefs = GetDlgItem(IDC_DEFINITIONS);
	if(1 == ListView_GetSelectedCount(hwndDefs))
	{
		INT cItems = ListView_GetItemCount(hwndDefs);

		for(INT i = 0; i < cItems; i++)
		{
			if(ListView_GetItemState(hwndDefs, i, LVIS_SELECTED))
			{
				m_idxDef = i;
				return TRUE;
			}
		}
	}
	return FALSE;
}

HRESULT CGotoDefinitionDlg::LoadDefs (VOID)
{
	HRESULT hr;
	RSTRING rstrFile = NULL;
	DWORD cDefs = m_pDefs->Count();
	HWND hwndDefs = GetDlgItem(IDC_DEFINITIONS);

	Assert(0 < cDefs);

	LV_COLUMN lvColumn = {0};

	lvColumn.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvColumn.fmt = LVCFMT_LEFT;

	ListView_SetExtendedListViewStyle(hwndDefs, LVS_EX_FULLROWSELECT);

	lvColumn.iSubItem = 0;
	lvColumn.cx = (INT)DPI::Scale(48.0f);
	lvColumn.pszText = L"Line";
	ListView_InsertColumn(hwndDefs, lvColumn.iSubItem, &lvColumn);

	lvColumn.iSubItem = 1;
	lvColumn.cx = (INT)DPI::Scale(90.0f);
	lvColumn.pszText = L"Type";
	ListView_InsertColumn(hwndDefs, lvColumn.iSubItem, &lvColumn);

	lvColumn.iSubItem = 2;
	lvColumn.cx = (INT)DPI::Scale(250.0f);
	lvColumn.pszText = L"File";
	ListView_InsertColumn(hwndDefs, lvColumn.iSubItem, &lvColumn);

	for(DWORD i = 0; i < cDefs; i++)
	{
		INT nLine;
		PCWSTR pcwzToken;
		LV_ITEM lvItem = {0};
		WCHAR wzLine[16];

		lvItem.mask = LVIF_TEXT;
		lvItem.pszText = wzLine;
		lvItem.cchTextMax = ARRAYSIZE(wzLine);
		lvItem.iItem = ListView_GetItemCount(hwndDefs);
		lvItem.iSubItem = 0;

		Check(m_pDefs->GetDefinition(i, &rstrFile, &nLine, &pcwzToken));

		Check(Formatting::TUInt32ToAsc(nLine, wzLine, ARRAYSIZE(wzLine), 10, NULL));
		ListView_InsertItem(hwndDefs, &lvItem);

		lvItem.pszText = const_cast<PWSTR>(pcwzToken);
		lvItem.iSubItem = 1;
		ListView_SetItem(hwndDefs, &lvItem);

		lvItem.pszText = const_cast<PWSTR>(RStrToWide(rstrFile));
		lvItem.iSubItem = 2;
		ListView_SetItem(hwndDefs, &lvItem);

		RStrRelease(rstrFile); rstrFile = NULL;
	}

Cleanup:
	RStrRelease(rstrFile);
	return hr;
}
