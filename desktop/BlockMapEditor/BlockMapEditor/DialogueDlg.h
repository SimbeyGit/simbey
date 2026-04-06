#pragma once

#include "Library\Core\BaseUnknown.h"
#include "Library\Window\BaseDialog.h"

interface IJSONObject;

class CConversationDlg : public CBaseDialog
{
private:
	IJSONObject* m_pConversation;

public:
	CConversationDlg (IJSONObject* pConversation);
	~CConversationDlg ();

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

private:
	HRESULT Load (VOID);
	HRESULT Save (VOID);
};

class CDialogueDlg : public CBaseDialog
{
private:
	HINSTANCE m_hInstance;
	IJSONObject* m_pProps;

public:
	CDialogueDlg (HINSTANCE hInstance, IJSONObject* pProps);
	~CDialogueDlg ();

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

private:
	VOID UpdateButtons (BOOL fHasSelection);
	HRESULT RefreshList (VOID);

	HRESULT DisplayConversation (IJSONObject* pConversation);

	HRESULT AddDialogue (VOID);
	HRESULT EditDialogue (VOID);
	HRESULT DeleteDialogue (VOID);
};
