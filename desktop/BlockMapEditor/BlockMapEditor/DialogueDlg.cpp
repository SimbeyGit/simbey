#include <windows.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Util\Formatting.h"
#include "Library\Window\DialogHost.h"
#include "Published\JSON.h"
#include "DialogueDlg.h"

CConversationDlg::CConversationDlg (IJSONObject* pConversation) :
	CBaseDialog(IDD_CONVERSATION),
	m_pConversation(pConversation)
{
	m_pConversation->AddRef();
}

CConversationDlg::~CConversationDlg ()
{
	m_pConversation->Release();
}

BOOL CConversationDlg::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	switch(message)
	{
	case WM_INITDIALOG:
		Load();
		CenterHost();
		break;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			if(FAILED(Save()))
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

HRESULT CConversationDlg::Load (VOID)
{
	HRESULT hr;
	DWORD id;
	TStackRef<IJSONValue> srv;
	WCHAR wzID[32];
	RSTRING rstrActor = NULL, rstrScript = NULL;

	Check(m_pConversation->FindNonNullValueW(L"id", &srv));
	Check(srv->GetDWord(&id));

	Check(Formatting::TUInt32ToAsc(id, wzID, ARRAYSIZE(wzID), 10, NULL));
	SetWindowText(GetDlgItem(IDC_CONVERSATION_ID), wzID);

	srv.Release();
	Check(m_pConversation->FindNonNullValueW(L"actor", &srv));
	Check(srv->GetString(&rstrActor));
	SetWindowText(GetDlgItem(IDC_ACTOR_ID), RStrToWide(rstrActor));

	srv.Release();
	Check(m_pConversation->FindNonNullValueW(L"script", &srv));
	Check(srv->GetString(&rstrScript));
	SetWindowText(GetDlgItem(IDC_DIALOGUE_SCRIPT), RStrToWide(rstrScript));

Cleanup:
	RStrRelease(rstrScript);
	RStrRelease(rstrActor);
	return hr;
}

HRESULT CConversationDlg::Save (VOID)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	HWND hwndScript = GetDlgItem(IDC_DIALOGUE_SCRIPT);
	WCHAR wzID[32], wzActor[64], *pwzPtr;
	DWORD id;
	INT cchActor, cchScript;
	RSTRING rstrScript = NULL;

	CheckIf(0 == GetWindowText(GetDlgItem(IDC_CONVERSATION_ID), wzID, ARRAYSIZE(wzID)), E_FAIL);
	id = Formatting::TAscToUInt32(wzID);
	CheckIf(0 == id, E_FAIL);

	cchActor = GetWindowText(GetDlgItem(IDC_ACTOR_ID), wzActor, ARRAYSIZE(wzActor));

	if(id <= INT_MAX)
		Check(JSONCreateInteger(id, &srv));
	else
		Check(JSONCreateLongInteger(id, &srv));
	Check(m_pConversation->AddValueW(L"id", srv));

	srv.Release();
	Check(JSONCreateStringW(wzActor, cchActor, &srv));
	Check(m_pConversation->AddValueW(L"actor", srv));

	cchScript = GetWindowTextLength(hwndScript);
	Check(RStrAllocW(cchScript, &rstrScript, &pwzPtr));
	GetWindowText(hwndScript, pwzPtr, cchScript + 1);

	srv.Release();
	Check(JSONCreateString(rstrScript, &srv));
	Check(m_pConversation->AddValueW(L"script", srv));

Cleanup:
	RStrRelease(rstrScript);
	return hr;
}

CDialogueDlg::CDialogueDlg (HINSTANCE hInstance, IJSONObject* pProps) :
	CBaseDialog(IDD_DIALOGUE),
	m_hInstance(hInstance),
	m_pProps(pProps)
{
	m_pProps->AddRef();
}

CDialogueDlg::~CDialogueDlg ()
{
	m_pProps->Release();
}

BOOL CDialogueDlg::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	switch(message)
	{
	case WM_INITDIALOG:
		RefreshList();
		UpdateButtons(FALSE);
		CenterHost();
		break;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDC_CONVERSATIONS:
			if(HIWORD(wParam) == LBN_SELCHANGE)
			{
				HWND hwndConversations = (HWND)lParam;
				UpdateButtons(LB_ERR != SendMessage(hwndConversations, LB_GETCURSEL, 0, 0));
			}
			break;

		case IDC_ADD_DIALOGUE:
			AddDialogue();
			break;
		case IDC_EDIT_DIALOGUE:
			EditDialogue();
			break;
		case IDC_DELETE_DIALOGUE:
			DeleteDialogue();
			break;

		case IDOK:
		case IDCANCEL:
			End(LOWORD(wParam));
			break;
		}
		break;
	}

	return FALSE;
}

VOID CDialogueDlg::UpdateButtons (BOOL fHasSelection)
{
	EnableWindow(GetDlgItem(IDC_EDIT_DIALOGUE), fHasSelection);
	EnableWindow(GetDlgItem(IDC_DELETE_DIALOGUE), fHasSelection);
}

HRESULT CDialogueDlg::RefreshList (VOID)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	HWND hwndConversations = GetDlgItem(IDC_CONVERSATIONS);

	SendMessage(hwndConversations, LB_RESETCONTENT, 0, 0);

	hr = m_pProps->FindNonNullValueW(L"dialogue", &srv);
	if(SUCCEEDED(hr))
	{
		TStackRef<IJSONArray> srDialogue;

		Check(srv->GetArray(&srDialogue));

		for(sysint i = 0; i < srDialogue->Count(); i++)
		{
			TStackRef<IJSONObject> srConversation;
			DWORD id, idx;
			WCHAR wzID[32];

			Check(srDialogue->GetObject(i, &srConversation));

			srv.Release();
			Check(srConversation->FindNonNullValueW(L"id", &srv));
			Check(srv->GetDWord(&id));

			Check(Formatting::TUInt32ToAsc(id, wzID, ARRAYSIZE(wzID), 10, NULL));
			idx = SendMessage(hwndConversations, LB_ADDSTRING, 0, (LPARAM)wzID);
			SendMessage(hwndConversations, LB_SETITEMDATA, idx, i);
		}
	}

Cleanup:
	return hr;
}

HRESULT CDialogueDlg::DisplayConversation (IJSONObject* pConversation)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	HWND hwnd;
	CDialogHost dlgHost(m_hInstance);

	CConversationDlg dlgConversation(pConversation);

	Check(GetWindow(&hwnd));
	Check(dlgHost.Display(hwnd, &dlgConversation));
	CheckIfIgnore(IDOK != dlgHost.GetReturnValue(), E_ABORT);

Cleanup:
	return hr;
}

HRESULT CDialogueDlg::AddDialogue (VOID)
{
	HRESULT hr;
	TStackRef<IJSONObject> srConversation;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONArray> srDialogues;

	Check(JSONCreateObject(&srConversation));
	Check(DisplayConversation(srConversation));

	if(SUCCEEDED(m_pProps->FindNonNullValueW(L"dialogue", &srv)))
		Check(srv->GetArray(&srDialogues));
	else
	{
		Check(JSONCreateArray(&srDialogues));
		Check(JSONWrapArray(srDialogues, &srv));
		Check(m_pProps->AddValueW(L"dialogue", srv));
	}
	srv.Release();

	Check(JSONWrapObject(srConversation, &srv));
	Check(srDialogues->Add(srv));

	RefreshList();

Cleanup:
	return hr;
}

HRESULT CDialogueDlg::EditDialogue (VOID)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONArray> srConversations;
	TStackRef<IJSONObject> srConversation;
	HWND hwndConversations = GetDlgItem(IDC_CONVERSATIONS);
	INT idxConversation = SendMessage(hwndConversations, LB_GETCURSEL, 0, 0);

	CheckIf(LB_ERR == idxConversation, E_FAIL);
	Check(m_pProps->FindNonNullValueW(L"dialogue", &srv));
	Check(srv->GetArray(&srConversations));
	Check(srConversations->GetObject(idxConversation, &srConversation));
	Check(DisplayConversation(srConversation));

	RefreshList();

Cleanup:
	return hr;
}

HRESULT CDialogueDlg::DeleteDialogue (VOID)
{
	HRESULT hr;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONArray> srConversations;
	HWND hwndConversations = GetDlgItem(IDC_CONVERSATIONS);
	INT idxConversation = SendMessage(hwndConversations, LB_GETCURSEL, 0, 0);

	CheckIf(LB_ERR == idxConversation, E_FAIL);
	Check(m_pProps->FindNonNullValueW(L"dialogue", &srv));
	Check(srv->GetArray(&srConversations));
	Check(srConversations->Remove(idxConversation));

	if(0 == srConversations->Count())
		Check(m_pProps->RemoveValueW(L"dialogue"));

	RefreshList();

Cleanup:
	return hr;
}
