#pragma once

#include "Library\Core\MemoryStream.h"
#include "Library\Window\BaseDialog.h"
#include "Library\IconTypes.h"
#include "Published\QuadooParser.h"

class CProjectCompilerDlg :
	public CBaseDialog,
	public IQuadooCompilerStatus
{
private:
	IJSONObject* m_pProject;
	RSTRING m_rstrTarget;
	RSTRING m_rstrEngine;
	RSTRING m_rstrProjectDir;
	PCWSTR m_pcwzScript;

	RSTRING m_rstrTemplate;

	CMemoryStream m_stmIcon;
	HICON m_hIcon;

	bool m_fDisplayedErrors;

public:
	CProjectCompilerDlg (IJSONObject* pProject, RSTRING rstrTarget, RSTRING rstrEngine, RSTRING rstrProjectDir, PCWSTR pcwzScript);
	~CProjectCompilerDlg ();

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

private:
	HRESULT Initialize (VOID);
	HRESULT DecodeIcon (RSTRING rstrIcon);

	HRESULT GetIconFromLoadedStream (INT nDesiredSize, __out HICON* phIcon);
	HRESULT SelectIcon (VOID);

	HRESULT WriteSettings (VOID);
	HRESULT Compile (VOID);

	// IQuadooCompilerStatus
	virtual VOID STDMETHODCALLTYPE OnCompilerAddFile (PCWSTR pcwzFile, INT cchFile);
	virtual VOID STDMETHODCALLTYPE OnCompilerStatus (PCWSTR pcwzStatus);
	virtual VOID STDMETHODCALLTYPE OnCompilerError (HRESULT hrCode, INT nLine, PCWSTR pcwzFile, PCWSTR pcwzError);
	virtual STDMETHODIMP OnCompilerResolvePath (PCWSTR pcwzPath, __out_ecount(cchMaxAbsolutePath) PWSTR pwzAbsolutePath, INT cchMaxAbsolutePath);

	static HRESULT AddIconToModule (HANDLE hModule, CMemoryStream& stmIcon, PCWSTR pcwzName, LONG idxIconOffset);
	static HRESULT GetIconCountFromModule (PCWSTR pcwzExecutable, __out LONG* pidxIconOffset);

	static int WINAPI _SortIcons (ICONDIRENTRY* pLeft, ICONDIRENTRY* pRight, PVOID pParam);
	static BOOL CALLBACK CountIcons (HMODULE hModule, PCWSTR pcwzType, PWSTR pwzName, LONG_PTR lParam);
};
