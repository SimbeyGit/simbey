#include <windows.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Util\Formatting.h"
#include "Published\JSON.h"
#include "RunWebServiceDlg.h"

#define SECURITY_FLAG_IGNORE_UNKNOWN_CA         0x00000100

#define	WM_ISSUES_COMPLETED						(WM_USER + 1)

///////////////////////////////////////////////////////////////////////////////
// CWebCall
///////////////////////////////////////////////////////////////////////////////

CWebCall::CWebCall (CWinHttp* pWinHttp, HWND hwndCallback) :
	m_pWinHttp(pWinHttp),
	m_pRequest(NULL),
	m_hwndCallback(hwndCallback),
	m_pwzResponseHeaders(NULL),
	m_hrResult(E_PENDING),
	m_dwStatus(0)
{
}

CWebCall::~CWebCall ()
{
	Assert(NULL == m_pRequest);

	SafeDeleteArray(m_pwzResponseHeaders);
}

HRESULT CWebCall::StartAsync (HINTERNET hServer, PCWSTR pcwzVerb, PCWSTR pcwzResource, BOOL fSecure, DWORD dwSecurityOptions,
	PCWSTR pcwzHeaders, INT cchHeaders, RSTRING rstrUser, RSTRING rstrPassword)
{
	HRESULT hr;
	PCWSTR prgDefaultAcceptTypes[] = { L"*/*", NULL };

	m_pRequest = __new CAsyncWinHttp(m_pWinHttp, this);
	CheckAlloc(m_pRequest);

	Check(m_pRequest->OpenRequest(hServer, pcwzVerb, pcwzResource, NULL, prgDefaultAcceptTypes,
		fSecure ? WINHTTP_FLAG_SECURE : 0, dwSecurityOptions, pcwzHeaders, cchHeaders,
		RStrToWide(rstrUser), RStrLen(rstrUser), RStrToWide(rstrPassword), RStrLen(rstrPassword)));

Cleanup:
	return hr;
}

VOID CWebCall::AbortAndWait (VOID)
{
	if(m_pRequest && SUCCEEDED(m_pRequest->Abort(E_ABORT)))
		WaitForSingleObject(m_pRequest->GetCompletionEvent(), INFINITE);
}

// IAttachable

VOID CWebCall::AttachReference (IUnknown* punkOwner)
{
}

VOID CWebCall::DetachReference (IUnknown* punkOwner)
{
}

// IAsyncWinHttpCallback

VOID CWebCall::OnRequestError (WINHTTP_ASYNC_RESULT* pResult)
{
}

DWORD CWebCall::OnReadPostDataSize (VOID)
{
	return m_stmPostData.DataRemaining();
}

HRESULT CWebCall::OnReadPostData (LPBYTE pBuffer, DWORD cbMaxBuffer, DWORD* pcbRead)
{
	return m_stmPostData.Read(pBuffer, cbMaxBuffer, pcbRead);
}

HRESULT CWebCall::OnGetWriteBuffer (LPBYTE* ppbWriteBuffer, __out DWORD* pcbMaxWrite)
{
	*pcbMaxWrite = 65536;
	return m_stmResponse.WriteAdvance(ppbWriteBuffer, *pcbMaxWrite);
}

VOID CWebCall::OnAdjustWrittenSize (DWORD cbWritten, DWORD cbUnwritten)
{
	m_stmResponse.PopWritePtr(cbUnwritten, NULL);
}

HRESULT CWebCall::OnAllocateHeadersW (INT cchHeaders, __out PWSTR* ppwzHeaders)
{
	HRESULT hr;

	m_pwzResponseHeaders = __new WCHAR[cchHeaders + 1];
	CheckAlloc(m_pwzResponseHeaders);
	m_pwzResponseHeaders[cchHeaders] = L'\0';

	*ppwzHeaders = m_pwzResponseHeaders;
	hr = S_OK;

Cleanup:
	return hr;
}

VOID CWebCall::OnCompletion (HRESULT hrRequestResult, DWORD dwStatusCode)
{
	SafeRelease(m_pRequest);

	m_hrResult = hrRequestResult;
	m_dwStatus = dwStatusCode;
	PostMessage(m_hwndCallback, WM_ISSUES_COMPLETED, 0, 0);
}

///////////////////////////////////////////////////////////////////////////////
// CRunWebServiceDlg
///////////////////////////////////////////////////////////////////////////////

CRunWebServiceDlg::CRunWebServiceDlg (IJSONObject* pProject, RSTRING rstrWebTarget) :
	CBaseDialog(IDD_RUN_WEBSERVICE),
	m_pProject(pProject),
	m_hSession(NULL),
	m_hServer(NULL),
	m_pWebCall(NULL)
{
	m_pProject->AddRef();
	RStrSet(m_rstrWebTarget, rstrWebTarget);
}

CRunWebServiceDlg::~CRunWebServiceDlg ()
{
	Assert(NULL == m_pWebCall);

	RStrRelease(m_rstrWebTarget);
	m_pProject->Release();
}

BOOL CRunWebServiceDlg::DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
	switch(message)
	{
	case WM_INITDIALOG:
		if(FAILED(m_WinHttp.Initialize()))
		{
			lResult = FALSE;
			return TRUE;
		}

		SetWindowText(GetDlgItem(IDC_VERB), L"GET");
		SetWindowText(GetDlgItem(IDC_CONTENT_TYPE), L"application/json");
#ifdef	_WIN64
		SetWindowText(GetDlgItem(IDC_AGENT), L"Mozilla/5 (Windows; Win64; x64) QuadooStudio");
#else
		SetWindowText(GetDlgItem(IDC_AGENT), L"Mozilla/5 (Windows; Win32; x86) QuadooStudio");
#endif
		SetWindowText(GetDlgItem(IDC_BODY), L"{\"action\":\"hello\"}");
		CenterHost();
		SetFocus(GetDlgItem(IDOK));
		break;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			RunWebService();
			break;
		case IDCANCEL:
			End(0);
			break;
		}
		break;

	case WM_CLOSE:
		CloseHandles();
		break;

	case WM_ISSUES_COMPLETED:
		if(m_pWebCall)
		{
			WCHAR wzStatus[16];

			Formatting::TUInt32ToAsc(m_pWebCall->m_dwStatus, wzStatus, ARRAYSIZE(wzStatus), 10, NULL);
			SetWindowText(GetDlgItem(IDC_STATUS), wzStatus);

			if(m_pWebCall->m_pwzResponseHeaders)
			{
				PCWSTR pcwzContentType = TStrIStr(m_pWebCall->m_pwzResponseHeaders, L"Content-Type:");
				if(pcwzContentType)
				{
					pcwzContentType += 13;
					while(Formatting::IsSpace(*pcwzContentType))
						pcwzContentType++;

					if(0 == TStrICmpNAssert(pcwzContentType, SLP(L"text/")) || TStrICmpNAssert(pcwzContentType, SLP(L"application/json")) || TStrICmpNAssert(pcwzContentType, SLP(L"application/xml")))
					{
						RSTRING rstrResponseW;

						if(SUCCEEDED(RStrConvertToW(CP_UTF8, m_pWebCall->m_stmResponse.TDataRemaining<CHAR>(), m_pWebCall->m_stmResponse.TGetReadPtr<CHAR>(), &rstrResponseW)))
						{
							SetWindowText(GetDlgItem(IDC_RESPONSE), RStrToWide(rstrResponseW));
							RStrRelease(rstrResponseW);
						}
					}
					else
					{
						// TODO
					}
				}
			}

			if(FAILED(m_pWebCall->m_hrResult))
			{
				HWND hwnd;
				WCHAR wzError[100];
				GetWindow(&hwnd);
				Formatting::TPrintF(wzError, ARRAYSIZE(wzError), NULL, L"HTTP Error Encountered: %.8X", m_pWebCall->m_hrResult);
				MessageBox(hwnd, wzError, L"Error", MB_ICONERROR | MB_OK);
			}

			CloseHandles();
			EnableWindow(GetDlgItem(IDOK), TRUE);
		}
		break;
	}

	return FALSE;
}

HRESULT CRunWebServiceDlg::ReadField (INT id, __deref_out RSTRING* prstrValue, BOOL fRequired)
{
	HRESULT hr;
	HWND hwnd = GetDlgItem(id);
	INT cch;
	PWSTR pwzPtr;

	CheckIfGetLastError(NULL == hwnd);
	cch = GetWindowTextLength(hwnd);
	if(0 == cch && fRequired)
	{
		MessageBeep(MB_ICONQUESTION);
		SetFocus(hwnd);
		hr = E_INVALIDARG;
	}
	else
	{
		Check(RStrAllocW(cch, prstrValue, &pwzPtr));
		GetWindowText(hwnd, pwzPtr, cch + 1);
	}

Cleanup:
	return hr;
}

HRESULT CRunWebServiceDlg::AddHeader (CMemoryStream& stmHeadersW, PCWSTR pcwzName, INT idValue, BOOL fRequired)
{
	HRESULT hr;
	RSTRING rstrValueW = NULL, rstrLineW = NULL;
	ULONG cb;

	Check(ReadField(idValue, &rstrValueW, fRequired));
	Check(RStrFormatW(&rstrLineW, L"%ls: %r\r\n", pcwzName, rstrValueW));
	Check(stmHeadersW.TWrite(RStrToWide(rstrLineW), RStrLen(rstrLineW), &cb));

Cleanup:
	RStrRelease(rstrLineW);
	RStrRelease(rstrValueW);
	return hr;
}

HRESULT CRunWebServiceDlg::BuildResourceString (PCWSTR pcwzEndpoint, __deref_out RSTRING* prstrResource)
{
	HRESULT hr;
	RSTRING rstrQuery = NULL;

	Check(ReadField(IDC_QUERY_STRING, &rstrQuery));

	if(0 == RStrLen(rstrQuery))
		Check(RStrCreateW(TStrLenAssert(pcwzEndpoint), pcwzEndpoint, prstrResource));
	else if(L'?' == *RStrToWide(rstrQuery))
		Check(RStrFormatW(prstrResource, L"%ls%r", pcwzEndpoint, rstrQuery));
	else
		Check(RStrFormatW(prstrResource, L"%ls?%r", pcwzEndpoint, rstrQuery));

Cleanup:
	RStrRelease(rstrQuery);
	return hr;
}

HRESULT CRunWebServiceDlg::RunWebService (VOID)
{
	HRESULT hr;
	HWND hwnd;
	RSTRING rstrVerbW = NULL, rstrAgentW = NULL, rstrServerW = NULL, rstrResourceW = NULL, rstrRequestW = NULL;
	RSTRING rstrCustomHeaderW = NULL, rstrUserW = NULL, rstrPasswordW = NULL;
	CMemoryStream stmHeadersW;
	BOOL fSecure = FALSE;
	const WCHAR wchNil = L'\0';
	ULONG cb;

	Assert(NULL == m_hSession);
	Assert(NULL == m_hServer);
	Assert(NULL == m_pWebCall);

	Check(GetWindow(&hwnd));

	Check(ReadField(IDC_VERB, &rstrVerbW, TRUE));
	Check(ReadField(IDC_AGENT, &rstrAgentW, TRUE));

	m_hSession = m_WinHttp.m_pfnWinHttpOpen(RStrToWide(rstrAgentW), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, NULL, NULL, WINHTTP_FLAG_ASYNC);
	CheckIf(NULL == m_hSession, E_FAIL);

	{
		PCWSTR pcwzEndpoint = RStrToWide(m_rstrWebTarget);
		INTERNET_PORT wPort = INTERNET_DEFAULT_HTTP_PORT;

		if(TCompareLeftIAssert(pcwzEndpoint, L"http://"))
			pcwzEndpoint += 7;
		else if(TCompareLeftIAssert(pcwzEndpoint, L"https://"))
		{
			pcwzEndpoint += 8;
			wPort = INTERNET_DEFAULT_HTTPS_PORT;
			fSecure = TRUE;
		}

		PCWSTR pcwzPort = TStrChr(pcwzEndpoint, L':');
		if(pcwzPort)
		{
			Check(RStrCreateW(static_cast<INT>(pcwzPort - pcwzEndpoint), pcwzEndpoint, &rstrServerW));
			wPort = static_cast<INTERNET_PORT>(Formatting::TAscToXUInt32(pcwzPort, 10, &pcwzEndpoint));
		}
		else
		{
			PCWSTR pcwzSlash = TStrChr(pcwzEndpoint, L'/');
			if(pcwzSlash)
				Check(RStrCreateW(static_cast<INT>(pcwzSlash - pcwzEndpoint), pcwzEndpoint, &rstrServerW));
			else
				Check(RStrCreateW(TStrLenAssert(pcwzEndpoint), pcwzEndpoint, &rstrServerW));
			pcwzEndpoint = pcwzSlash;
		}

		m_hServer = m_WinHttp.m_pfnWinHttpConnect(m_hSession, RStrToWide(rstrServerW), wPort, 0);
		CheckIfGetLastError(NULL == m_hServer);

		Check(BuildResourceString(pcwzEndpoint, &rstrResourceW));
	}

	m_pWebCall = __new CWebCall(&m_WinHttp, hwnd);
	CheckAlloc(m_pWebCall);

	Check(ReadField(IDC_BODY, &rstrRequestW));
	if(0 < RStrLen(rstrRequestW))
	{
		INT cbBody = WideCharToMultiByte(CP_UTF8, 0, RStrToWide(rstrRequestW), RStrLen(rstrRequestW), NULL, 0, NULL, NULL);
		PSTR pszBody;

		CheckIfGetLastError(0 == cbBody);
		Check(m_pWebCall->m_stmPostData.TWriteAdvance(&pszBody, cbBody));
		WideCharToMultiByte(CP_UTF8, 0, RStrToWide(rstrRequestW), RStrLen(rstrRequestW), pszBody, cbBody, NULL, NULL);
	}

	Check(AddHeader(stmHeadersW, L"Content-Type", IDC_CONTENT_TYPE, TRUE));
	Check(AddHeader(stmHeadersW, L"Referer", IDC_REFERRER));

	Check(ReadField(IDC_HEADER_NAME, &rstrCustomHeaderW));
	if(0 < RStrLen(rstrCustomHeaderW))
		Check(AddHeader(stmHeadersW, RStrToWide(rstrCustomHeaderW), IDC_HEADER_VALUE));

	Check(AddHeader(stmHeadersW, L"Cookie", IDC_COOKIES));
	Check(stmHeadersW.TWrite(&wchNil, 1, &cb));

	Check(ReadField(IDC_USERNAME, &rstrUserW));
	Check(ReadField(IDC_PASSWORD, &rstrPasswordW));

	Check(m_pWebCall->StartAsync(m_hServer, RStrToWide(rstrVerbW), RStrToWide(rstrResourceW),
		fSecure, SECURITY_FLAG_IGNORE_UNKNOWN_CA,
		stmHeadersW.TGetReadPtr<WCHAR>(), stmHeadersW.TDataRemaining<WCHAR>() - 1, rstrUserW, rstrPasswordW));

	EnableWindow(GetDlgItem(IDOK), FALSE);

Cleanup:
	if(FAILED(hr))
		CloseHandles();

	RStrRelease(rstrCustomHeaderW);

	RStrRelease(rstrUserW);
	RStrRelease(rstrPasswordW);

	RStrRelease(rstrRequestW);
	RStrRelease(rstrResourceW);
	RStrRelease(rstrServerW);
	RStrRelease(rstrAgentW);
	RStrRelease(rstrVerbW);
	return hr;
}

VOID CRunWebServiceDlg::CloseHandles (VOID)
{
	if(m_pWebCall)
	{
		m_pWebCall->AbortAndWait();
		SafeRelease(m_pWebCall);
	}

	if(m_hServer)
	{
		m_WinHttp.CloseHandle(m_hServer);
		m_hServer = NULL;
	}

	if(m_hSession)
	{
		m_WinHttp.CloseHandle(m_hSession);
		m_hSession = NULL;
	}
}
