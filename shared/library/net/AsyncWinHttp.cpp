#include <malloc.h>
#include <windows.h>
#include "..\Core\CoreDefs.h"
#include "AsyncWinHttp.h"

#define	ASYNC_BUFFER_LENGTH			8192

///////////////////////////////////////////////////////////////////////////////
// CWinHttp
///////////////////////////////////////////////////////////////////////////////

CWinHttp::CWinHttp () :
	m_hWinHttp(NULL),
	m_pfnWinHttpOpen(NULL),
	m_pfnWinHttpConnect(NULL),
	m_pfnWinHttpOpenRequest(NULL),
	m_pfnWinHttpAddRequestHeaders(NULL),
	m_pfnWinHttpSendRequest(NULL),
	m_pfnWinHttpReceiveResponse(NULL),
	m_pfnWinHttpReadData(NULL),
	m_pfnWinHttpWriteData(NULL),
	m_pfnWinHttpQueryHeaders(NULL),
	m_pfnWinHttpQueryDataAvailable(NULL),
	m_pfnWinHttpCloseHandle(NULL),
	m_pfnWinHttpSetStatusCallback(NULL),
	m_pfnWinHttpSetOption(NULL),
	m_pfnWinHttpQueryOption(NULL),
	m_pfnWinHttpCrackUrl(NULL),
	m_pfnWinHttpGetIEProxyConfigForCurrentUser(NULL)
{
}

CWinHttp::~CWinHttp ()
{
	Close();
}

HRESULT CWinHttp::Initialize (VOID)
{
	HRESULT hr;

	CheckIfIgnore(NULL != m_hWinHttp, S_FALSE);

	m_hWinHttp = LoadLibrary(TEXT("WINHTTP.DLL"));
	if(m_hWinHttp == NULL)
	{
		m_hWinHttp = LoadLibrary(TEXT("WINHTTP5.DLL"));
		CheckIfGetLastError(NULL == m_hWinHttp);
	}

	Check(TGetFunction(m_hWinHttp, "WinHttpOpen", &m_pfnWinHttpOpen));
	Check(TGetFunction(m_hWinHttp, "WinHttpCloseHandle", &m_pfnWinHttpCloseHandle));
	Check(TGetFunction(m_hWinHttp, "WinHttpConnect", &m_pfnWinHttpConnect));
	Check(TGetFunction(m_hWinHttp, "WinHttpOpenRequest", &m_pfnWinHttpOpenRequest));
	Check(TGetFunction(m_hWinHttp, "WinHttpAddRequestHeaders", &m_pfnWinHttpAddRequestHeaders));
	Check(TGetFunction(m_hWinHttp, "WinHttpSendRequest", &m_pfnWinHttpSendRequest));
	Check(TGetFunction(m_hWinHttp, "WinHttpReceiveResponse", &m_pfnWinHttpReceiveResponse));
	Check(TGetFunction(m_hWinHttp, "WinHttpQueryDataAvailable", &m_pfnWinHttpQueryDataAvailable));
	Check(TGetFunction(m_hWinHttp, "WinHttpQueryHeaders", &m_pfnWinHttpQueryHeaders));
	Check(TGetFunction(m_hWinHttp, "WinHttpReadData", &m_pfnWinHttpReadData));
	Check(TGetFunction(m_hWinHttp, "WinHttpWriteData", &m_pfnWinHttpWriteData));
	Check(TGetFunction(m_hWinHttp, "WinHttpSetStatusCallback", &m_pfnWinHttpSetStatusCallback));
	Check(TGetFunction(m_hWinHttp, "WinHttpSetOption", &m_pfnWinHttpSetOption));
	Check(TGetFunction(m_hWinHttp, "WinHttpQueryOption", &m_pfnWinHttpQueryOption));
	Check(TGetFunction(m_hWinHttp, "WinHttpCrackUrl", &m_pfnWinHttpCrackUrl));
	Check(TGetFunction(m_hWinHttp, "WinHttpGetIEProxyConfigForCurrentUser", &m_pfnWinHttpGetIEProxyConfigForCurrentUser));

Cleanup:
	if(FAILED(hr))
		Close();
	return hr;
}

VOID CWinHttp::Close (VOID)
{
	if(m_hWinHttp)
	{
		FreeLibrary(m_hWinHttp);
		m_hWinHttp = NULL;
	}
}

BOOL CWinHttp::CloseHandle (HINTERNET hInternet)
{
	return m_pfnWinHttpCloseHandle(hInternet);
}

HINTERNET CWinHttp::OpenRequest (HINTERNET hConnection, LPCWSTR lpszVerb, LPCWSTR lpszObject, LPCWSTR lpszVersion, LPCWSTR lpszReferrer, LPCWSTR* lpAcceptTypes, DWORD dwFlags)
{
	LPCWSTR lpDefault[2] = {L"*/*", NULL};
	if(lpAcceptTypes == NULL)
		lpAcceptTypes = lpDefault;
	return m_pfnWinHttpOpenRequest(hConnection,lpszVerb,lpszObject,lpszVersion,lpszReferrer,lpAcceptTypes,dwFlags);
}

BOOL CWinHttp::SendRequest (HINTERNET hRequest, LPCWSTR lpszHeaders, DWORD dwHeaders, LPVOID lpData, DWORD dwData, DWORD dwTotal, PVOID pvContext)
{
	return m_pfnWinHttpSendRequest(hRequest,lpszHeaders,dwHeaders,lpData,dwData,dwTotal,reinterpret_cast<DWORD_PTR>(pvContext));
}

BOOL CWinHttp::ReceiveResponse (HINTERNET hRequest)
{
	return m_pfnWinHttpReceiveResponse(hRequest,NULL);
}

BOOL CWinHttp::QueryDataAvailable (HINTERNET hRequest, LPDWORD lpdwData)
{
	if(lpdwData)
		*lpdwData = 0;
	return m_pfnWinHttpQueryDataAvailable(hRequest,lpdwData);
}

BOOL CWinHttp::QueryHeaders (HINTERNET hRequest, DWORD dwInfoLevel, LPCWSTR lpszName, LPVOID lpBuffer, LPDWORD lpdwBuffer, LPDWORD lpdwIndex)
{
	return m_pfnWinHttpQueryHeaders(hRequest,dwInfoLevel,lpszName,lpBuffer,lpdwBuffer,lpdwIndex);
}

BOOL CWinHttp::ReadData (HINTERNET hRequest, LPVOID lpBuffer, DWORD dwRead, LPDWORD lpdwBytesRead)
{
	return m_pfnWinHttpReadData(hRequest,lpBuffer,dwRead,lpdwBytesRead);
}

WINHTTP_STATUS_CALLBACK CWinHttp::SetStatusCallback (HINTERNET hInternet, WINHTTP_STATUS_CALLBACK lpfnCallback, DWORD dwFlags, DWORD* lpdwReserved)
{
	return m_pfnWinHttpSetStatusCallback(hInternet,lpfnCallback,dwFlags,lpdwReserved);
}

BOOL CWinHttp::SetOption (HINTERNET hInternet, DWORD dwOption, LPVOID lpBuffer, DWORD cbBuffer)
{
	return m_pfnWinHttpSetOption(hInternet,dwOption,lpBuffer,cbBuffer);
}

HRESULT CWinHttp::CrackURL (PCWSTR pcwzURL, INT cchURL, PWSTR pwzScheme, INT cchMaxScheme, WINHTTP_INTERNET_SCHEME* pnScheme, PWSTR pwzHost, INT cchMaxHost, PWSTR pwzPath, INT cchMaxPath)
{
	HRESULT hr;
	WINHTTP_URL_COMPONENTS url;

	ZeroMemory(&url, sizeof(url));
	url.dwStructSize = sizeof(url);

	url.lpszScheme = pwzScheme;
	url.dwSchemeLength = cchMaxScheme;

	url.lpszHostName = pwzHost;
	url.dwHostNameLength = cchMaxHost;

	url.lpszUrlPath = pwzPath;
	url.dwUrlPathLength = cchMaxPath;

	if(0 < cchMaxScheme)
		pwzScheme[0] = L'\0';
	if(0 < cchMaxHost)
		pwzHost[0] = L'\0';
	if(0 < cchMaxPath)
		pwzPath[0] = L'\0';

	CheckIfGetLastError(!m_pfnWinHttpCrackUrl(pcwzURL, cchURL, 0, &url));
	*pnScheme = url.nScheme;
	hr = S_OK;

Cleanup:
	return hr;
}

HRESULT CWinHttp::GetIEProxyConfigForCurrentUser (WINHTTP_CURRENT_USER_IE_PROXY_CONFIG* pConfig)
{
	HRESULT hr;
	if(m_pfnWinHttpGetIEProxyConfigForCurrentUser(pConfig))
		hr = S_OK;
	else
		hr = HRESULT_FROM_WIN32(GetLastError());
	return hr;
}

VOID CWinHttp::FreeProxyConfigData (WINHTTP_CURRENT_USER_IE_PROXY_CONFIG* pConfig)
{
	if(pConfig->lpszProxy)
		GlobalFree(pConfig->lpszProxy);
	if(pConfig->lpszProxyBypass)
		GlobalFree(pConfig->lpszProxyBypass);
	if(pConfig->lpszAutoConfigUrl)
		GlobalFree(pConfig->lpszAutoConfigUrl);
}

///////////////////////////////////////////////////////////////////////////////
// CAsyncWinHttp
///////////////////////////////////////////////////////////////////////////////

HRESULT CAsyncWinHttp::CreateAsyncEvent (__out HANDLE* phCompletion)
{
	HRESULT hr;

	*phCompletion = CreateEvent(NULL,TRUE,FALSE,NULL);
	CheckIfGetLastError(NULL == *phCompletion);
	hr = S_OK;

Cleanup:
	return hr;
}

CAsyncWinHttp::CAsyncWinHttp (CWinHttp* pWinHttp, HANDLE hCompletion) :
	m_pWinHttp(pWinHttp),
	m_pCallback(NULL),
	m_hCompletion(hCompletion),
	m_hRequest(NULL),
	m_cHandleLocks(0),
	m_pbPostBuffer(NULL),
	m_fOwnEvent(FALSE)
{
	InitializeCriticalSection(&m_cs);
}

CAsyncWinHttp::~CAsyncWinHttp ()
{
	Assert(NULL == m_hRequest);
	Assert(NULL == m_pCallback);

	if(m_fOwnEvent)
		SafeCloseHandle(m_hCompletion);

	SafeDeleteArray(m_pbPostBuffer);

	DeleteCriticalSection(&m_cs);
}

HRESULT CAsyncWinHttp::OpenRequest (IAsyncWinHttpCallback* pCallback,
									HINTERNET hServer, PCWSTR pcwzVerb, PCWSTR pcwzResource,
									PCWSTR pcwzReferrer, PCWSTR* ppcwzAcceptTypes, DWORD dwFlags, DWORD dwSecurityFlags,
									PCWSTR pcwzHeaders, INT cchHeaders,
									PCWSTR pcwzUserName, INT cchUserName, PCWSTR pcwzPassword, INT cchPassword)
{
	HRESULT hr;
	DWORD cbPostSize = pCallback->OnReadPostDataSize();
	bool fSetCallback = false;

	EnterCriticalSection(&m_cs);

	CheckIf(NULL != m_hRequest, E_UNEXPECTED);

	Assert(NULL == m_pCallback);
	SafeAttach(m_pCallback, pCallback, static_cast<IAsyncWinHttp*>(this));
	fSetCallback = true;

	if(NULL == m_hCompletion)
	{
		Check(CreateAsyncEvent(&m_hCompletion));
		m_fOwnEvent = true;
	}
	else
		ResetEvent(m_hCompletion);

	if(0 < cbPostSize)
	{
		if(NULL == m_pbPostBuffer)
		{
			m_pbPostBuffer = __new BYTE[ASYNC_BUFFER_LENGTH];
			CheckAlloc(m_pbPostBuffer);
		}
		m_fPostData = true;
	}
	else
		m_fPostData = false;

	m_hRequest = m_pWinHttp->OpenRequest(hServer, pcwzVerb, pcwzResource, NULL, pcwzReferrer, ppcwzAcceptTypes, dwFlags);
	CheckIfGetLastError(NULL == m_hRequest);

	AddRef();	// Matched with the Release() in WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING
	m_pWinHttp->SetStatusCallback(m_hRequest, _AsyncCallback,
		WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS | WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING, NULL);

	m_fAborted = false;
	m_fClosing = false;

	m_dwStatusCode = 0;
	m_hrRequestResult = HRESULT_FROM_WIN32(ERROR_GEN_FAILURE);

	if(0 < cchUserName)
	{
		CheckIfGetLastError(!m_pWinHttp->m_pfnWinHttpSetOption(m_hRequest, WINHTTP_OPTION_USERNAME, const_cast<PWSTR>(pcwzUserName), cchUserName));
	}

	if(0 < cchPassword)
	{
		CheckIfGetLastError(!m_pWinHttp->m_pfnWinHttpSetOption(m_hRequest, WINHTTP_OPTION_PASSWORD, const_cast<PWSTR>(pcwzPassword), cchPassword));
	}

	if(0 != dwSecurityFlags)
	{
		CheckIfGetLastError(!m_pWinHttp->m_pfnWinHttpSetOption(m_hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &dwSecurityFlags, sizeof(dwSecurityFlags)));
	}

	CheckIfGetLastError(!m_pWinHttp->SendRequest(m_hRequest, pcwzHeaders, cchHeaders, NULL, 0, cbPostSize, this));
	hr = S_OK;

Cleanup:
	if(FAILED(hr))
	{
		if(m_hRequest)
		{
			HANDLE hRequest = m_hRequest;
			m_hRequest = NULL;
			m_pWinHttp->CloseHandle(hRequest);
		}

		if(fSetCallback)
			SafeDetach(m_pCallback, static_cast<IAsyncWinHttp*>(this));
	}

	LeaveCriticalSection(&m_cs);
	return hr;
}

HANDLE CAsyncWinHttp::GetCompletionEvent (VOID)
{
	return m_hCompletion;
}

HRESULT CAsyncWinHttp::Abort (HRESULT hrResult)
{
	HRESULT hr;
	HINTERNET hRequest = NULL;

	EnterCriticalSection(&m_cs);

	if(!m_fClosing)
	{
		if(!m_fAborted)
		{
			m_fAborted = true;

			if(0 == m_cHandleLocks)
			{
				m_hrRequestResult = hrResult;
				SwapData(m_hRequest, hRequest);
				hr = S_OK;
			}
			else
				hr = E_PENDING;
		}
		else
			hr = E_PENDING;
	}
	else
		hr = E_FAIL;

	LeaveCriticalSection(&m_cs);

	if(hRequest)
		m_pWinHttp->CloseHandle(hRequest);

	return hr;
}

bool CAsyncWinHttp::LockHandle (VOID)
{
	bool fSuccess;

	EnterCriticalSection(&m_cs);

	if(m_fClosing || m_fAborted)
		fSuccess = false;
	else
	{
		m_cHandleLocks++;
		fSuccess = true;
	}

	LeaveCriticalSection(&m_cs);

	return fSuccess;
}

VOID CAsyncWinHttp::ReleaseHandle (HRESULT hr)
{
	HANDLE hRequest = NULL;

	EnterCriticalSection(&m_cs);

	Assert(0 < m_cHandleLocks);

	if(FAILED(hr) && !m_fAborted)
	{
		m_hrRequestResult = hr;
		m_fAborted = true;
	}

	if(0 == --m_cHandleLocks)
	{
		if(m_fAborted)
			m_fClosing = true;

		if(m_fClosing)
			SwapData(m_hRequest, hRequest);
	}

	LeaveCriticalSection(&m_cs);

	if(hRequest)
		m_pWinHttp->CloseHandle(hRequest);
}

VOID CAsyncWinHttp::SendRequestComplete (VOID)
{
	if(LockHandle())
	{
		HRESULT hr;

		if(m_fPostData)
			WriteComplete();
		else
			CheckIfGetLastError(!m_pWinHttp->m_pfnWinHttpReceiveResponse(m_hRequest, NULL));
		hr = S_OK;

	Cleanup:
		ReleaseHandle(hr);
	}
}

VOID CAsyncWinHttp::WriteComplete (VOID)
{
	if(LockHandle())
	{
		HRESULT hr;
		DWORD cbData;

		Check(m_pCallback->OnReadPostData(m_pbPostBuffer, ASYNC_BUFFER_LENGTH, &cbData));
		if(0 < cbData)
			CheckIfGetLastError(!m_pWinHttp->m_pfnWinHttpWriteData(m_hRequest, m_pbPostBuffer, cbData, NULL));
		else
			CheckIfGetLastError(!m_pWinHttp->m_pfnWinHttpReceiveResponse(m_hRequest, NULL));

	Cleanup:
		ReleaseHandle(hr);
	}
}

VOID CAsyncWinHttp::HeadersAvailable (VOID)
{
	if(LockHandle())
	{
		HRESULT hr;

		DWORD cbData = sizeof(m_dwStatusCode);
		CheckIfGetLastError(!m_pWinHttp->m_pfnWinHttpQueryHeaders(m_hRequest,
			WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX,
			&m_dwStatusCode, &cbData, WINHTTP_NO_HEADER_INDEX));

		cbData = 0;
		m_pWinHttp->m_pfnWinHttpQueryHeaders(m_hRequest,
			WINHTTP_QUERY_RAW_HEADERS_CRLF,
			WINHTTP_HEADER_NAME_BY_INDEX,
			NULL, &cbData, WINHTTP_NO_HEADER_INDEX);

		if(0 < cbData)
		{
			PWSTR pwzHeaders;
			INT cchHeaders = cbData / sizeof(WCHAR);

			Check(m_pCallback->OnAllocateHeadersW(cchHeaders, &pwzHeaders));
			if(pwzHeaders)
			{
				CheckIfGetLastError(!m_pWinHttp->m_pfnWinHttpQueryHeaders(m_hRequest,
					WINHTTP_QUERY_RAW_HEADERS_CRLF,
					WINHTTP_HEADER_NAME_BY_INDEX,
					pwzHeaders, &cbData, WINHTTP_NO_HEADER_INDEX));
				m_pCallback->OnReceivedHeadersW(cchHeaders, pwzHeaders);
			}
		}

		Check(m_pCallback->OnGetWriteBuffer(&m_pbReadBuffer, &m_cbBuffer));
		CheckIfGetLastError(!m_pWinHttp->ReadData(m_hRequest, m_pbReadBuffer, m_cbBuffer, NULL));

	Cleanup:
		ReleaseHandle(hr);
	}
}

VOID CAsyncWinHttp::ReadComplete (DWORD cbRead)
{
	if(LockHandle())
	{
		HRESULT hr;

		m_pCallback->OnAdjustWrittenSize(cbRead, m_cbBuffer - cbRead);
		if(0 == cbRead)
		{
			EnterCriticalSection(&m_cs);
			if(!m_fClosing)
			{
				m_fClosing = true;
				m_hrRequestResult = S_OK;
			}
			LeaveCriticalSection(&m_cs);

			hr = S_OK;
		}
		else
		{
			Check(m_pCallback->OnGetWriteBuffer(&m_pbReadBuffer, &m_cbBuffer));
			CheckIfGetLastError(!m_pWinHttp->ReadData(m_hRequest, m_pbReadBuffer, m_cbBuffer, NULL));
		}

	Cleanup:
		ReleaseHandle(hr);
	}
}

VOID CAsyncWinHttp::AsyncCallback (DWORD dwCode, PVOID pvInfo, DWORD dwLength)
{
	switch(dwCode)
	{
	case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
		{
			WINHTTP_ASYNC_RESULT* pResult = reinterpret_cast<WINHTTP_ASYNC_RESULT*>(pvInfo);
			m_pCallback->OnRequestError(pResult);
			Abort(static_cast<HRESULT>(HRESULT_FROM_WIN32(pResult->dwError)));
		}
		break;
	case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
		SendRequestComplete();
		break;
	case WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE:
		WriteComplete();
		break;
	case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
		HeadersAvailable();
		break;
	case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
		ReadComplete(dwLength);
		break;
	case WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING:
		Assert(NULL == m_hRequest);
		m_pCallback->OnCompletion(m_hrRequestResult, m_dwStatusCode);
		SafeDetach(m_pCallback, static_cast<IAsyncWinHttp*>(this));
		SetEvent(m_hCompletion);
		Release();
		break;
	}
}

VOID CALLBACK CAsyncWinHttp::_AsyncCallback (HINTERNET hRequest, PVOID pvContext, DWORD dwCode, PVOID pvInfo, DWORD dwLength)
{
	CAsyncWinHttp* pThis = reinterpret_cast<CAsyncWinHttp*>(pvContext);
	if(pThis)
	{
		TStackRef<CAsyncWinHttp> ref(pThis);
		pThis->AsyncCallback(dwCode, pvInfo, dwLength);
	}
}
