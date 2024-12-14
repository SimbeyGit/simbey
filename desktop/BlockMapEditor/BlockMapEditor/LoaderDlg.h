#pragma once

#include "Library\Core\BaseUnknown.h"
#include "Library\Window\BaseDialog.h"

interface ILoaderThread
{
	virtual HRESULT LoaderCallback (HWND hwndStatus) = 0;
};

class CLoaderDlg : public CBaseDialog
{
private:
	ILoaderThread* m_pCallback;
	HANDLE m_hThread;

public:
	CLoaderDlg (ILoaderThread* pCallback);
	~CLoaderDlg ();

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

private:
	static DWORD WINAPI _LoaderThread (PVOID pvParam);
};
