#include <windows.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Util\Formatting.h"
#include "NeuralAPI.h"
#include "InputPad.h"
#include "InputPadSize.h"

CInputPadSize::CInputPadSize (CInputPad* lpInputPad) : CBaseDialog(IDD_INPUTPADSIZE)
{
	m_lpInputPad = lpInputPad;
	m_lpInputPad->AddRef();
}

CInputPadSize::~CInputPadSize ()
{
	m_lpInputPad->Release();
}

VOID CInputPadSize::DlgCreate (VOID)
{
	INT nSize = m_lpInputPad->GetSquareSize();
	CHAR szSize[32];

	m_hwndSize = GetDlgItem(IDC_PAD_SIZE);

	Formatting::TPrintF(szSize, ARRAYSIZE(szSize), NULL, "%d", nSize);
	SetWindowText(m_hwndSize,szSize);

	CenterHost();
}

VOID CInputPadSize::DlgCommand (USHORT id, USHORT iCode)
{
	UNREFERENCED_PARAMETER(iCode);

	CHAR szSize[32];

	switch(id)
	{
	case IDOK:
		if(GetWindowText(m_hwndSize,szSize,sizeof(szSize)) < sizeof(szSize))
		{
			if(SUCCEEDED(m_lpInputPad->SetSquareSize(Formatting::TAscToUInt32(szSize))))
				End(TRUE);
		}
		break;

	case IDCANCEL:
		End(FALSE);
		break;
	}
}

BOOL CInputPadSize::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	UNREFERENCED_PARAMETER(lParam);

	BOOL fHandled = FALSE;

	switch(message)
	{
	case WM_INITDIALOG:
		DlgCreate();
		fHandled = TRUE;
		lResult = TRUE;
		break;
	case WM_COMMAND:
		DlgCommand(LOWORD(wParam),HIWORD(wParam));
		break;
	}

	return fHandled;
}