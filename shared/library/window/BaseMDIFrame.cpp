#include <windows.h>
#include "Library\Core\StringCore.h"
#include "BaseMDIFrame.h"

BOOL FindSubMenu (HMENU hMenu, PCWSTR pcwzFindLabel, __out HMENU* phSubMenu)
{
	INT cItems = GetMenuItemCount(hMenu);
	MENUITEMINFO mii;
	WCHAR wzLabel[256];

	mii.cbSize = sizeof(mii);
	mii.dwTypeData = wzLabel;

	for(INT i = 0; i < cItems; i++)
	{
		mii.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_SUBMENU;
		mii.cch = ARRAYSIZE(wzLabel);
		if(GetMenuItemInfo(hMenu, i, TRUE, (MENUITEMINFO*)&mii) && mii.hSubMenu)
		{
			INT cch = TStrLenAssert(wzLabel);
			for(INT n = 0; n < cch; n++)
			{
				if(wzLabel[n] == L'&')
				{
					MoveMemory(wzLabel + n, wzLabel + n + 1, (cch - n) * sizeof(WCHAR));
					cch--;
				}
			}

			if(0 == TStrCmpIAssert(wzLabel, pcwzFindLabel))
			{
				*phSubMenu = mii.hSubMenu;
				return TRUE;
			}
		}
	}

	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
// CBaseMDIFrame
///////////////////////////////////////////////////////////////////////////////

CBaseMDIFrame::CBaseMDIFrame () :
	m_hwndMDIClient(NULL)
{
}

CBaseMDIFrame::~CBaseMDIFrame ()
{
}

BOOL CBaseMDIFrame::ProcessMDIMessage (MSG* pMsg)
{
	Assert(NULL != m_hwndMDIClient);
	return TranslateMDISysAccel(m_hwndMDIClient, pMsg);
}

HWND CBaseMDIFrame::GetActiveMDIChild (__out_opt BOOL* pfMaximized)
{
	Assert(NULL != m_hwndMDIClient);
	return (HWND)SendMessage(m_hwndMDIClient, WM_MDIGETACTIVE, 0, (LPARAM)pfMaximized);
}

VOID CBaseMDIFrame::OnFinalDestroy (HWND hwnd)
{
	m_hwndMDIClient = NULL;
}

BOOL CBaseMDIFrame::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	if(WM_CREATE == message)
	{
		CLIENTCREATESTRUCT ccs;

		if(GetMDIMenuData(reinterpret_cast<HMENU*>(&ccs.hWindowMenu), &ccs.idFirstChild))
		{
			m_hwndMDIClient = CreateWindow(TEXT("MDICLIENT"), (LPCTSTR)NULL,
				WS_CHILD | WS_CLIPCHILDREN | WS_VSCROLL | WS_HSCROLL | MDIS_ALLCHILDSTYLES,
				0, 0, 0, 0, m_hwnd, NULL, GetInstance(), &ccs);

			if(m_hwndMDIClient)
				::ShowWindow(m_hwndMDIClient, SW_SHOW);
		}

		if(NULL == m_hwndMDIClient)
		{
			lResult = FALSE;
			return TRUE;
		}
	}

	return __super::DefWindowProc(message, wParam, lParam, lResult);
}

BOOL CBaseMDIFrame::SystemMessageHandler (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	lResult = ::DefFrameProc(m_hwnd, m_hwndMDIClient, message, wParam, lParam);
	return TRUE;
}

HRESULT CBaseMDIFrame::RegisterMDIFrame (const WNDCLASSEX* lpWndClass, ATOM* lpAtom)
{
	return RegisterClass(lpWndClass, lpAtom, _MDIFrameProcCreate);
}

// Private static methods

LRESULT WINAPI CBaseMDIFrame::_MDIFrameProcCreate (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT lResult = 0;
	if(message == FIRST_WM_MESSAGE)
	{
		LPCREATESTRUCT lpCreate = (LPCREATESTRUCT)lParam;
		CBaseMDIFrame* pFrame = (CBaseMDIFrame*)lpCreate->lpCreateParams;
		pFrame->AddRef();
		pFrame->m_hwnd = hwnd;
		SetWindowLongPtr(hwnd,GWL_USERDATA,(__int3264)(LONG_PTR)pFrame);
		SetWindowLongPtr(hwnd,GWL_WNDPROC,(__int3264)(LONG_PTR)_MDIFrameProc);
		lResult = _MDIFrameProc(hwnd, message, wParam, lParam);
	}
	else
		lResult = ::DefFrameProc(hwnd, NULL, message, wParam, lParam);
	return lResult;
}

LRESULT WINAPI CBaseMDIFrame::_MDIFrameProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT lResult = 0;
	CBaseMDIFrame* pFrame = (CBaseMDIFrame*)(LONG_PTR)GetWindowLongPtr(hwnd, GWL_USERDATA);
	if(FALSE == pFrame->InnerWindowProc(message, wParam, lParam, lResult))
		lResult = ::DefFrameProc(hwnd, FINAL_WM_MESSAGE == message ? NULL : pFrame->m_hwndMDIClient, message, wParam, lParam);
	return lResult;
}

///////////////////////////////////////////////////////////////////////////////
// CBaseMDIChild
///////////////////////////////////////////////////////////////////////////////

CBaseMDIChild::CBaseMDIChild () :
	m_hwndMDIClient(NULL)
{
}

CBaseMDIChild::~CBaseMDIChild ()
{
	Assert(NULL == m_hwndMDIClient);
}

BOOL CBaseMDIChild::Activate (VOID)
{
	if(m_hwndMDIClient)
	{
		Assert(NULL != m_hwnd);
		SendMessage(m_hwndMDIClient, WM_MDIACTIVATE, (WPARAM)m_hwnd, 0);
		return TRUE;
	}
	return FALSE;
}

BOOL CBaseMDIChild::Maximize (VOID)
{
	if(m_hwndMDIClient)
	{
		Assert(NULL != m_hwnd);
		SendMessage(m_hwndMDIClient, WM_MDIMAXIMIZE, (WPARAM)m_hwnd, 0);
		return TRUE;
	}
	return FALSE;
}

HRESULT CBaseMDIChild::Destroy (VOID)
{
	HRESULT hr = E_FAIL;
	if(m_hwndMDIClient)
	{
		Assert(NULL != m_hwnd);
		SendMessage(m_hwndMDIClient, WM_MDIDESTROY, (WPARAM)m_hwnd, 0);
		hr = S_OK;
	}
	return hr;
}

HRESULT CBaseMDIChild::AttachToFrame (LPCTSTR pctstrClass, LPCTSTR pctstrTitle, DWORD dwStyle, INT x, INT y, INT nWidth, INT nHeight, CBaseMDIFrame* pFrame)
{
	HRESULT hr;

	CheckIf(NULL == pFrame, E_POINTER);

	HWND hwndMDIClient = pFrame->m_hwndMDIClient;
	CheckIf(NULL == hwndMDIClient, HRESULT_FROM_WIN32(ERROR_NON_MDICHILD_WINDOW));

	m_hwndMDIClient = hwndMDIClient;
	Check(Create(WS_EX_MDICHILD, dwStyle, pctstrClass, pctstrTitle, x, y, nWidth, nHeight, hwndMDIClient, SW_SHOW));

Cleanup:
	return hr;
}

VOID CBaseMDIChild::OnFinalDestroy (HWND hwnd)
{
	m_hwndMDIClient = NULL;
}

BOOL CBaseMDIChild::SystemMessageHandler (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	lResult = DefMDIChildProc(m_hwnd, message, wParam, lParam);
	return FALSE;
}

HRESULT CBaseMDIChild::RegisterMDIChild (const WNDCLASSEX* lpWndClass, ATOM* lpAtom)
{
	return RegisterClass(lpWndClass, lpAtom, _MDIChildProcCreate);
}

LRESULT WINAPI CBaseMDIChild::_MDIChildProcCreate (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT lResult = 0;
	if(message == FIRST_WM_MESSAGE)
	{
		LPCREATESTRUCT lpCreate = (LPCREATESTRUCT)lParam;
		MDICREATESTRUCT* pmdiCreate = (MDICREATESTRUCT*)lpCreate->lpCreateParams;
		CBaseMDIChild* pChild = (CBaseMDIChild*)pmdiCreate->lParam;
		pChild->AddRef();
		pChild->m_hwnd = hwnd;
		SetWindowLongPtr(hwnd,GWL_USERDATA,(__int3264)(LONG_PTR)pChild);
		SetWindowLongPtr(hwnd,GWL_WNDPROC,(__int3264)(LONG_PTR)_MDIChildProc);
		lResult = _MDIChildProc(hwnd, message, wParam, lParam);
	}
	else
		lResult = ::DefMDIChildProc(hwnd, message, wParam, lParam);
	return lResult;
}

LRESULT WINAPI CBaseMDIChild::_MDIChildProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT lResult = 0;
	CBaseMDIChild* pChild = (CBaseMDIChild*)(LONG_PTR)GetWindowLongPtr(hwnd, GWL_USERDATA);
	if(FALSE == pChild->InnerWindowProc(message, wParam, lParam, lResult))
		lResult = ::DefMDIChildProc(hwnd, message, wParam, lParam);
	return lResult;
}
