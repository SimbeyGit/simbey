#pragma once

#include "Library\Core\MemoryStream.h"
#include "Library\Window\BaseDialog.h"
#include "Published\QuadooParser.h"
#include "DebugSession.h"

class CQuadooProject;

class CStartDebuggerDlg :
	public CBaseDialog,
	public IStartDebuggerStatus,
	public IQuadooCompilerStatus
{
private:
	CQuadooProject* m_pProject;
	PCWSTR m_pcwzDebugger;
	PCWSTR m_pcwzStartDir;
	PCWSTR m_pcwzScriptPath;
	RSTRING m_rstrScriptArgs;
	INT m_nPort;

	CMemoryStream m_stmQBC;
	CMemoryStream m_stmDebug;
	CMemoryStream m_stmBreakpoints;
	IQuadooDebugTree* m_pDebugTree;

	HWND m_hwndStatus;
	HICON m_hDebuggerIcon;
	HANDLE m_hStartThread;
	BOOL m_fAbort;
	CQuadooDebugSession* m_pDebugger;
	RSTRING m_rstrRemoteHost;

	bool m_fDisplayedErrors;

public:
	CStartDebuggerDlg (CQuadooProject* pProject, PCWSTR pcwzDebugger, PCWSTR pcwzStartDir, PCWSTR pcwzScriptPath, RSTRING rstrScriptArgs, INT nPort);
	~CStartDebuggerDlg ();

	HRESULT Start (VOID);

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

private:
	HRESULT SetDebuggerIcon (VOID);
	HRESULT StartAsync (VOID);

	static DWORD CALLBACK _StartAsync (PVOID pvParam);

	// IStartDebuggerStatus
	virtual VOID ReportStatus (PCWSTR pcwzStatus, INT cchStatus);
	virtual BOOL CheckAbortFlag (VOID);

	// IQuadooCompilerStatus
	virtual VOID STDMETHODCALLTYPE OnCompilerAddFile (PCWSTR pcwzFile, INT cchFile);
	virtual VOID STDMETHODCALLTYPE OnCompilerStatus (PCWSTR pcwzStatus);
	virtual VOID STDMETHODCALLTYPE OnCompilerError (HRESULT hrCode, INT nLine, PCWSTR pcwzFile, PCWSTR pcwzError);
	virtual STDMETHODIMP OnCompilerResolvePath (PCWSTR pcwzPath, __out_ecount(cchMaxAbsolutePath) PWSTR pwzAbsolutePath, INT cchMaxAbsolutePath);
};
