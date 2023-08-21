#pragma once

#include "Library\Window\BaseDialog.h"
#include "Library\Core\MemoryStream.h"
#include "Library\Net\AsyncWinHttp.h"

interface IJSONObject;

class CWebCall :
	public CBaseUnknown,
	public IAsyncWinHttpCallback
{
public:
	CWinHttp* m_pWinHttp;
	CAsyncWinHttp* m_pRequest;
	CMemoryStream m_stmPostData;
	CMemoryStream m_stmResponse;

	HWND m_hwndCallback;

	PWSTR m_pwzResponseHeaders;
	HRESULT m_hrResult;
	DWORD m_dwStatus;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IAttachable)
		UNK_INTERFACE(IAsyncWinHttpCallback)
	END_UNK_MAP

public:
	CWebCall (CWinHttp* pWinHttp, HWND hwndCallback);
	~CWebCall ();

	HRESULT StartAsync (HINTERNET hServer, PCWSTR pcwzVerb, PCWSTR pcwzResource, BOOL fSecure, DWORD dwSecurityOptions,
		PCWSTR pcwzHeaders, INT cchHeaders, RSTRING rstrUser, RSTRING rstrPassword);
	VOID AbortAndWait (VOID);

	// IAttachable
	virtual VOID AttachReference (IUnknown* punkOwner);
	virtual VOID DetachReference (IUnknown* punkOwner);

	// IAsyncWinHttpCallback
	virtual VOID OnRequestError (WINHTTP_ASYNC_RESULT* pResult);
	virtual DWORD OnReadPostDataSize (VOID);
	virtual HRESULT OnReadPostData (LPBYTE pBuffer, DWORD cbMaxBuffer, DWORD* pcbRead);
	virtual HRESULT OnGetWriteBuffer (LPBYTE* ppbWriteBuffer, __out DWORD* pcbMaxWrite);
	virtual VOID OnAdjustWrittenSize (DWORD cbWritten, DWORD cbUnwritten);
	virtual HRESULT OnAllocateHeadersW (INT cchHeaders, __out PWSTR* ppwzHeaders);
	virtual VOID OnCompletion (HRESULT hrRequestResult, DWORD dwStatusCode);
};

class CRunWebServiceDlg :
	public CBaseDialog
{
private:
	CWinHttp m_WinHttp;
	IJSONObject* m_pProject;
	RSTRING m_rstrWebTarget;

	HINTERNET m_hSession;
	HINTERNET m_hServer;
	CWebCall* m_pWebCall;

public:
	CRunWebServiceDlg (IJSONObject* pProject, RSTRING rstrWebTarget);
	~CRunWebServiceDlg ();

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

private:
	HRESULT ReadField (INT id, __deref_out RSTRING* prstrValue, BOOL fRequired = FALSE);
	HRESULT AddHeader (CMemoryStream& stmHeadersW, PCWSTR pcwzName, INT idValue, BOOL fRequired = FALSE);
	HRESULT BuildResourceString (PCWSTR pcwzEndpoint, __deref_out RSTRING* prstrResource);
	HRESULT RunWebService (VOID);
	VOID CloseHandles (VOID);
};
