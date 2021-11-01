#pragma once

#define	EXT_FIRST_CUSTOM_MENU_CMD			500

interface IBaseWindow;

class CNeuralDocument;

HRESULT ExtLoad (IBaseWindow* pWindow);
VOID ExtUnload (VOID);
VOID ExtModifyAppMenu (HMENU hMenu);
VOID ExtAddFramesToMenu (HMENU hMenu);
BOOL ExtAddMoreNeuronesToMenu (HMENU hMenu);
VOID ExtSetDocument (CNeuralDocument* lpDoc);
VOID ExtQueryMenuCommand (OLECMD* lpCmd);
HRESULT ExtExecMenuCommand (DWORD nCmdID, DWORD nCmdExecOpt);
BOOL ExtCustomMessage (UINT msg, WPARAM wParam, LPARAM lParam, __out LRESULT& lResult);
HRESULT ExtGetLinkSources (INeuralSource*** lplplpSources, INT* lpcSources);
