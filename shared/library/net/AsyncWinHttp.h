#pragma once

#include "..\Core\BaseUnknown.h"
#include "..\Core\IAttachable.h"

// Using WinHttp:
// http://msdn.microsoft.com/en-us/library/aa383138(VS.85).aspx

#define INTERNET_INVALID_PORT_NUMBER			0		// use the protocol-specific default

#define	INTERNET_DEFAULT_PORT					0
#define INTERNET_DEFAULT_FTP_PORT				21		// default for FTP servers
#define INTERNET_DEFAULT_GOPHER_PORT			70		//    "     "  gopher "
#define INTERNET_DEFAULT_HTTP_PORT				80		//    "     "  HTTP   "
#define INTERNET_DEFAULT_HTTPS_PORT				443		//    "     "  HTTPS  "
#define INTERNET_DEFAULT_SOCKS_PORT				1080	// default for SOCKS firewall servers.

// WinHttpOpen dwAccessType values (also for WINHTTP_PROXY_INFO::dwAccessType)
#define WINHTTP_ACCESS_TYPE_DEFAULT_PROXY		0
#define WINHTTP_ACCESS_TYPE_NO_PROXY			1
#define WINHTTP_ACCESS_TYPE_NAMED_PROXY			3
#define WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY		4

// WinHttpOpen prettifiers for optional parameters
#define	WINHTTP_NO_PROXY_NAME					NULL
#define	WINHTTP_NO_PROXY_BYPASS					NULL

#define WINHTTP_NO_CLIENT_CERT_CONTEXT			NULL

// WinHttpOpenRequest prettifers for optional parameters
#define WINHTTP_NO_REFERER						NULL
#define WINHTTP_DEFAULT_ACCEPT_TYPES			NULL

// WinHttpSendRequest prettifiers for optional parameters.
#define WINHTTP_NO_ADDITIONAL_HEADERS			NULL
#define WINHTTP_NO_REQUEST_DATA					NULL

#define WINHTTP_HEADER_NAME_BY_INDEX			NULL
#define WINHTTP_NO_OUTPUT_BUFFER				NULL
#define WINHTTP_NO_HEADER_INDEX					NULL

// flags for WinHttpOpenRequest():
#define WINHTTP_FLAG_SECURE						0x00800000	// use SSL if applicable (HTTPS)
#define WINHTTP_FLAG_ESCAPE_PERCENT				0x00000004	// if escaping enabled, escape percent as well
#define WINHTTP_FLAG_NULL_CODEPAGE				0x00000008	// assume all symbols are ASCII, use fast convertion
#define WINHTTP_FLAG_BYPASS_PROXY_CACHE			0x00000100	// add "pragma: no-cache" request header
#define WINHTTP_FLAG_REFRESH					WINHTTP_FLAG_BYPASS_PROXY_CACHE
#define WINHTTP_FLAG_ESCAPE_DISABLE				0x00000040	// disable escaping
#define WINHTTP_FLAG_ESCAPE_DISABLE_QUERY		0x00000080	// if escaping enabled escape path part, but do not escape query

#define WINHTTP_SECURITY_FLAG_IGNORE_UNKNOWN_CA         0x00000100
#define WINHTTP_SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE   0x00000200
#define WINHTTP_SECURITY_FLAG_IGNORE_CERT_DATE_INVALID  0x00002000	// expired X509 Cert.
#define WINHTTP_SECURITY_FLAG_IGNORE_CERT_CN_INVALID    0x00001000	// bad common name in X509 Cert.

// Values for dwModifiers parameter of WinHttpAddRequestHeaders()
#define WINHTTP_ADDREQ_INDEX_MASK						0x0000FFFF
#define WINHTTP_ADDREQ_FLAGS_MASK						0xFFFF0000

// WINHTTP_ADDREQ_FLAG_ADD_IF_NEW - the header will only be added if it
// doesn't already exist
#define WINHTTP_ADDREQ_FLAG_ADD_IF_NEW					0x10000000

// WINHTTP_ADDREQ_FLAG_ADD - if WINHTTP_ADDREQ_FLAG_REPLACE is set but
// the header is not found then if this flag is set, the header is added
// anyway, so long as there is a valid header-value
#define WINHTTP_ADDREQ_FLAG_ADD							0x20000000

// WINHTTP_ADDREQ_FLAG_COALESCE - coalesce headers with same name. e.g.
// "Accept: text/*" and "Accept: audio/*" with this flag results in a single
// header: "Accept: text/*, audio/*"
#define WINHTTP_ADDREQ_FLAG_COALESCE_WITH_COMMA			0x40000000
#define WINHTTP_ADDREQ_FLAG_COALESCE_WITH_SEMICOLON		0x01000000
#define WINHTTP_ADDREQ_FLAG_COALESCE					WINHTTP_ADDREQ_FLAG_COALESCE_WITH_COMMA

// WINHTTP_ADDREQ_FLAG_REPLACE - replaces the specified header. Only one header can
// be supplied in the buffer. If the header to be replaced is not the first
// in a list of headers with the same name, then the relative index should be
// supplied in the low 8 bits of the dwModifiers parameter. If the header-value
// part is missing, then the header is removed
#define WINHTTP_ADDREQ_FLAG_REPLACE						0x80000000

#define WINHTTP_IGNORE_REQUEST_TOTAL_LENGTH				0

// WinHttpSendRequest prettifiers for optional parameters.
#define WINHTTP_NO_ADDITIONAL_HEADERS					NULL
#define WINHTTP_NO_REQUEST_DATA							NULL

typedef LPVOID HINTERNET;
typedef HINTERNET* LPHINTERNET;

typedef WORD INTERNET_PORT;

typedef int WINHTTP_INTERNET_SCHEME, *LPWINHTTP_INTERNET_SCHEME;

#define WINHTTP_INTERNET_SCHEME_HTTP					(1)
#define WINHTTP_INTERNET_SCHEME_HTTPS	     			(2)
#define WINHTTP_INTERNET_SCHEME_FTP						(3)
#define WINHTTP_INTERNET_SCHEME_SOCKS	     			(4)

#define	WINHTTP_QUERY_CONTENT_TYPE						1
#define	WINHTTP_QUERY_CONTENT_LENGTH					5
#define WINHTTP_QUERY_RAW_HEADERS_CRLF					22

#define WINHTTP_FLAG_ASYNC								0x10000000

#define WINHTTP_OPTION_SECURITY_FLAGS					31
#define WINHTTP_OPTION_PROXY							38
#define WINHTTP_OPTION_SECURE_PROTOCOLS					84

#define WINHTTP_OPTION_USERNAME							0x1000
#define WINHTTP_OPTION_PASSWORD							0x1001
#define WINHTTP_OPTION_PROXY_USERNAME					0x1002
#define WINHTTP_OPTION_PROXY_PASSWORD					0x1003

#define WINHTTP_FLAG_SECURE_PROTOCOL_SSL2				0x00000008
#define WINHTTP_FLAG_SECURE_PROTOCOL_SSL3				0x00000020
#define WINHTTP_FLAG_SECURE_PROTOCOL_TLS1				0x00000080
#define WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1				0x00000200
#define WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2				0x00000800

/* Query flags */
#define WINHTTP_QUERY_MIME_VERSION						0
#define WINHTTP_QUERY_CONTENT_TYPE						1
#define WINHTTP_QUERY_CONTENT_TRANSFER_ENCODING			2
#define WINHTTP_QUERY_CONTENT_ID						3
#define WINHTTP_QUERY_CONTENT_DESCRIPTION				4
#define WINHTTP_QUERY_CONTENT_LENGTH					5
#define WINHTTP_QUERY_CONTENT_LANGUAGE					6
#define WINHTTP_QUERY_ALLOW								7
#define WINHTTP_QUERY_PUBLIC							8
#define WINHTTP_QUERY_DATE								9
#define WINHTTP_QUERY_EXPIRES							10
#define WINHTTP_QUERY_LAST_MODIFIED						11
#define WINHTTP_QUERY_MESSAGE_ID						12
#define WINHTTP_QUERY_URI								13
#define WINHTTP_QUERY_DERIVED_FROM						14
#define WINHTTP_QUERY_COST								15
#define WINHTTP_QUERY_LINK								16
#define WINHTTP_QUERY_PRAGMA							17
#define WINHTTP_QUERY_VERSION							18
#define WINHTTP_QUERY_STATUS_CODE						19
#define WINHTTP_QUERY_STATUS_TEXT						20
#define WINHTTP_QUERY_RAW_HEADERS						21
#define WINHTTP_QUERY_RAW_HEADERS_CRLF					22
#define WINHTTP_QUERY_CONNECTION						23
#define WINHTTP_QUERY_ACCEPT							24
#define WINHTTP_QUERY_ACCEPT_CHARSET					25
#define WINHTTP_QUERY_ACCEPT_ENCODING					26
#define WINHTTP_QUERY_ACCEPT_LANGUAGE					27
#define WINHTTP_QUERY_AUTHORIZATION						28
#define WINHTTP_QUERY_CONTENT_ENCODING					29
#define WINHTTP_QUERY_FORWARDED							30
#define WINHTTP_QUERY_FROM								31
#define WINHTTP_QUERY_IF_MODIFIED_SINCE					32
#define WINHTTP_QUERY_LOCATION							33
#define WINHTTP_QUERY_ORIG_URI							34
#define WINHTTP_QUERY_REFERER							35
#define WINHTTP_QUERY_RETRY_AFTER						36
#define WINHTTP_QUERY_SERVER							37
#define WINHTTP_QUERY_TITLE								38
#define WINHTTP_QUERY_USER_AGENT						39
#define WINHTTP_QUERY_WWW_AUTHENTICATE					40
#define WINHTTP_QUERY_PROXY_AUTHENTICATE				41
#define WINHTTP_QUERY_ACCEPT_RANGES						42
#define WINHTTP_QUERY_SET_COOKIE						43
#define WINHTTP_QUERY_COOKIE							44
#define WINHTTP_QUERY_REQUEST_METHOD					45
#define WINHTTP_QUERY_REFRESH							46
#define WINHTTP_QUERY_CONTENT_DISPOSITION				47
#define WINHTTP_QUERY_AGE								48
#define WINHTTP_QUERY_CACHE_CONTROL						49
#define WINHTTP_QUERY_CONTENT_BASE						50
#define WINHTTP_QUERY_CONTENT_LOCATION					51
#define WINHTTP_QUERY_CONTENT_MD5						52
#define WINHTTP_QUERY_CONTENT_RANGE						53
#define WINHTTP_QUERY_ETAG								54
#define WINHTTP_QUERY_HOST								55
#define WINHTTP_QUERY_IF_MATCH							56
#define WINHTTP_QUERY_IF_NONE_MATCH						57
#define WINHTTP_QUERY_IF_RANGE							58
#define WINHTTP_QUERY_IF_UNMODIFIED_SINCE				59
#define WINHTTP_QUERY_MAX_FORWARDS						60
#define WINHTTP_QUERY_PROXY_AUTHORIZATION				61
#define WINHTTP_QUERY_RANGE								62
#define WINHTTP_QUERY_TRANSFER_ENCODING					63
#define WINHTTP_QUERY_UPGRADE							64
#define WINHTTP_QUERY_VARY								65
#define WINHTTP_QUERY_VIA								66
#define WINHTTP_QUERY_WARNING							67
#define WINHTTP_QUERY_EXPECT							68
#define WINHTTP_QUERY_PROXY_CONNECTION					69
#define WINHTTP_QUERY_UNLESS_MODIFIED_SINCE				70
#define WINHTTP_QUERY_PROXY_SUPPORT						75
#define WINHTTP_QUERY_AUTHENTICATION_INFO				76
#define WINHTTP_QUERY_PASSPORT_URLS						77
#define WINHTTP_QUERY_PASSPORT_CONFIG					78
#define WINHTTP_QUERY_MAX								78
#define WINHTTP_QUERY_CUSTOM							65535
#define WINHTTP_QUERY_FLAG_REQUEST_HEADERS				0x80000000
#define WINHTTP_QUERY_FLAG_SYSTEMTIME					0x40000000
#define WINHTTP_QUERY_FLAG_NUMBER						0x20000000

// Callback Flags
#define WINHTTP_CALLBACK_STATUS_RESOLVING_NAME			0x00000001
#define WINHTTP_CALLBACK_STATUS_NAME_RESOLVED			0x00000002
#define WINHTTP_CALLBACK_STATUS_CONNECTING_TO_SERVER	0x00000004
#define WINHTTP_CALLBACK_STATUS_CONNECTED_TO_SERVER		0x00000008
#define WINHTTP_CALLBACK_STATUS_SENDING_REQUEST			0x00000010
#define WINHTTP_CALLBACK_STATUS_REQUEST_SENT			0x00000020
#define WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE		0x00000040
#define WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED		0x00000080
#define WINHTTP_CALLBACK_STATUS_CLOSING_CONNECTION		0x00000100
#define WINHTTP_CALLBACK_STATUS_CONNECTION_CLOSED		0x00000200
#define WINHTTP_CALLBACK_STATUS_HANDLE_CREATED			0x00000400
#define WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING			0x00000800
#define WINHTTP_CALLBACK_STATUS_DETECTING_PROXY			0x00001000
#define WINHTTP_CALLBACK_STATUS_REDIRECT				0x00004000
#define WINHTTP_CALLBACK_STATUS_INTERMEDIATE_RESPONSE	0x00008000
#define WINHTTP_CALLBACK_STATUS_SECURE_FAILURE			0x00010000
#define WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE		0x00020000
#define WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE			0x00040000
#define WINHTTP_CALLBACK_STATUS_READ_COMPLETE			0x00080000
#define WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE			0x00100000
#define WINHTTP_CALLBACK_STATUS_REQUEST_ERROR			0x00200000
#define WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE	0x00400000

#define WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS           (WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE   \
                                                        | WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE     \
                                                        | WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE        \
                                                        | WINHTTP_CALLBACK_STATUS_READ_COMPLETE         \
                                                        | WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE        \
                                                        | WINHTTP_CALLBACK_STATUS_REQUEST_ERROR)

#define WINHTTP_CALLBACK_FLAG_RESOLVE_NAME				(WINHTTP_CALLBACK_STATUS_RESOLVING_NAME | WINHTTP_CALLBACK_STATUS_NAME_RESOLVED)
#define WINHTTP_CALLBACK_FLAG_CONNECT_TO_SERVER			(WINHTTP_CALLBACK_STATUS_CONNECTING_TO_SERVER | WINHTTP_CALLBACK_STATUS_CONNECTED_TO_SERVER)
#define WINHTTP_CALLBACK_FLAG_SEND_REQUEST				(WINHTTP_CALLBACK_STATUS_SENDING_REQUEST | WINHTTP_CALLBACK_STATUS_REQUEST_SENT)
#define WINHTTP_CALLBACK_FLAG_RECEIVE_RESPONSE			(WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE | WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED)
#define WINHTTP_CALLBACK_FLAG_CLOSE_CONNECTION			(WINHTTP_CALLBACK_STATUS_CLOSING_CONNECTION | WINHTTP_CALLBACK_STATUS_CONNECTION_CLOSED)
#define WINHTTP_CALLBACK_FLAG_HANDLES					(WINHTTP_CALLBACK_STATUS_HANDLE_CREATED | WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING)
#define WINHTTP_CALLBACK_FLAG_DETECTING_PROXY			WINHTTP_CALLBACK_STATUS_DETECTING_PROXY
#define WINHTTP_CALLBACK_FLAG_REDIRECT					WINHTTP_CALLBACK_STATUS_REDIRECT
#define WINHTTP_CALLBACK_FLAG_INTERMEDIATE_RESPONSE		WINHTTP_CALLBACK_STATUS_INTERMEDIATE_RESPONSE
#define WINHTTP_CALLBACK_FLAG_SECURE_FAILURE			WINHTTP_CALLBACK_STATUS_SECURE_FAILURE
#define WINHTTP_CALLBACK_FLAG_SENDREQUEST_COMPLETE		WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE
#define WINHTTP_CALLBACK_FLAG_HEADERS_AVAILABLE			WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE
#define WINHTTP_CALLBACK_FLAG_DATA_AVAILABLE			WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE
#define WINHTTP_CALLBACK_FLAG_READ_COMPLETE				WINHTTP_CALLBACK_STATUS_READ_COMPLETE
#define WINHTTP_CALLBACK_FLAG_WRITE_COMPLETE			WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE
#define WINHTTP_CALLBACK_FLAG_REQUEST_ERROR				WINHTTP_CALLBACK_STATUS_REQUEST_ERROR

#define	WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS			0xffffffff

#define	WINHTTP_OPTION_CONTEXT_VALUE					45

typedef HANDLE HINTERNET;

typedef VOID (CALLBACK* WINHTTP_STATUS_CALLBACK)(HINTERNET,PVOID,DWORD,LPVOID,DWORD);

struct WINHTTP_ASYNC_RESULT
{
	DWORD_PTR dwResult;
	DWORD dwError;
};

struct WINHTTP_URL_COMPONENTS
{
	DWORD   dwStructSize;       // size of this structure. Used in version check
	LPWSTR  lpszScheme;         // pointer to scheme name
	DWORD   dwSchemeLength;     // length of scheme name
	WINHTTP_INTERNET_SCHEME nScheme;    // enumerated scheme type (if known)
	LPWSTR  lpszHostName;       // pointer to host name
	DWORD   dwHostNameLength;   // length of host name
	INTERNET_PORT nPort;        // converted port number
	LPWSTR  lpszUserName;       // pointer to user name
	DWORD   dwUserNameLength;   // length of user name
	LPWSTR  lpszPassword;       // pointer to password
	DWORD   dwPasswordLength;   // length of password
	LPWSTR  lpszUrlPath;        // pointer to URL-path
	DWORD   dwUrlPathLength;    // length of URL-path
	LPWSTR  lpszExtraInfo;      // pointer to extra information (e.g. ?foo or #foo)
	DWORD   dwExtraInfoLength;  // length of extra information
};

struct WINHTTP_CURRENT_USER_IE_PROXY_CONFIG
{
	BOOL    fAutoDetect;
	LPWSTR  lpszAutoConfigUrl;
	LPWSTR  lpszProxy;
	LPWSTR  lpszProxyBypass;
};

struct WINHTTP_PROXY_INFO
{
	DWORD  dwAccessType;
	LPWSTR lpszProxy;
	LPWSTR lpszProxyBypass;
};

interface __declspec(uuid("0DE69E10-13A9-4a58-8181-3BE35CA81285")) IAsyncWinHttpCallback : IAttachable
{
	virtual VOID OnRequestError (WINHTTP_ASYNC_RESULT* pResult) = 0;
	virtual DWORD OnReadPostDataSize (VOID) = 0;
	virtual HRESULT OnReadPostData (LPBYTE pBuffer, DWORD cbMaxBuffer, DWORD* pcbRead) = 0;
	virtual HRESULT OnGetWriteBuffer (LPBYTE* ppbWriteBuffer, __out DWORD* pcbMaxWrite) = 0;
	virtual VOID OnAdjustWrittenSize (DWORD cbWritten, DWORD cbUnwritten) = 0;
	virtual HRESULT OnAllocateHeadersW (INT cchHeaders, __out PWSTR* ppwzHeaders) = 0;
	virtual VOID OnCompletion (HRESULT hrRequestResult, DWORD dwStatusCode) = 0;
};

interface __declspec(uuid("CF21BB11-040B-49eb-AD75-C3335E37CD1A")) IAsyncWinHttp : IUnknown
{
	virtual HRESULT OpenRequest (HINTERNET hServer, PCWSTR pcwzVerb, PCWSTR pcwzResource,
		PCWSTR pcwzReferrer, PCWSTR* ppcwzAcceptTypes, DWORD dwFlags, DWORD dwSecurityFlags,
		PCWSTR pcwzHeaders, INT cchHeaders,
		PCWSTR pcwzUserName, INT cchUserName, PCWSTR pcwzPassword, INT cchPassword) = 0;
	virtual HANDLE GetCompletionEvent (VOID) = 0;
	virtual HRESULT Abort (HRESULT hrResult = E_ABORT) = 0;
};

class CWinHttp
{
private:
	HMODULE m_hWinHttp;

public:
	HINTERNET (WINAPI* m_pfnWinHttpOpen)(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, DWORD);
	HINTERNET (WINAPI* m_pfnWinHttpConnect)(HINTERNET, LPCWSTR, INTERNET_PORT, DWORD);
	HINTERNET (WINAPI* m_pfnWinHttpOpenRequest)(HINTERNET, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR FAR*, DWORD);
	BOOL (WINAPI* m_pfnWinHttpAddRequestHeaders)(HINTERNET, LPCWSTR, DWORD, DWORD);
	BOOL (WINAPI* m_pfnWinHttpSendRequest)(HINTERNET, LPCWSTR, DWORD, LPVOID, DWORD, DWORD, DWORD_PTR);
	BOOL (WINAPI* m_pfnWinHttpReceiveResponse)(HINTERNET, LPVOID);
	BOOL (WINAPI* m_pfnWinHttpReadData)(HINTERNET, LPVOID, DWORD, LPDWORD);
	BOOL (WINAPI* m_pfnWinHttpWriteData)(HINTERNET, LPCVOID, DWORD, LPDWORD);
	BOOL (WINAPI* m_pfnWinHttpQueryHeaders)(HINTERNET,DWORD, LPCWSTR, LPVOID, LPDWORD, LPDWORD);
	BOOL (WINAPI* m_pfnWinHttpQueryDataAvailable)(HINTERNET, LPDWORD);
	BOOL (WINAPI* m_pfnWinHttpCloseHandle)(HINTERNET);
	WINHTTP_STATUS_CALLBACK (WINAPI* m_pfnWinHttpSetStatusCallback)(HINTERNET,WINHTTP_STATUS_CALLBACK,DWORD,DWORD*);
	BOOL (WINAPI* m_pfnWinHttpSetOption)(HINTERNET, DWORD, PVOID, DWORD);
	BOOL (WINAPI* m_pfnWinHttpQueryOption)(HINTERNET, DWORD, PVOID, LPDWORD);
	BOOL (WINAPI* m_pfnWinHttpCrackUrl)(LPCWSTR, DWORD, DWORD, WINHTTP_URL_COMPONENTS*);
	BOOL (WINAPI* m_pfnWinHttpGetIEProxyConfigForCurrentUser)(WINHTTP_CURRENT_USER_IE_PROXY_CONFIG*);

public:
	CWinHttp ();
	~CWinHttp ();

	HRESULT Initialize (VOID);
	VOID Close (VOID);

	BOOL CloseHandle (HINTERNET hInternet);

	HINTERNET OpenRequest (HINTERNET hConnection, LPCWSTR lpszVerb, LPCWSTR lpszObject, LPCWSTR lpszVersion, LPCWSTR lpszReferrer, LPCWSTR* lpAcceptTypes, DWORD dwFlags);
	BOOL SendRequest (HINTERNET hRequest, LPCWSTR lpszHeaders, DWORD dwHeaders, LPVOID lpData, DWORD dwData, DWORD dwTotal, PVOID pvContext);
	BOOL ReceiveResponse (HINTERNET hRequest);
	BOOL QueryDataAvailable (HINTERNET hRequest, LPDWORD lpdwData);
	BOOL QueryHeaders (HINTERNET hRequest, DWORD dwInfoLevel, LPCWSTR lpszName, LPVOID lpBuffer, LPDWORD lpdwBuffer, LPDWORD lpdwIndex);
	BOOL ReadData (HINTERNET hRequest, LPVOID lpBuffer, DWORD dwRead, LPDWORD lpdwBytesRead);
	WINHTTP_STATUS_CALLBACK SetStatusCallback (HINTERNET hInternet, WINHTTP_STATUS_CALLBACK lpfnCallback, DWORD dwFlags, DWORD* lpdwReserved);
	BOOL SetOption (HINTERNET hInternet, DWORD dwOption, LPVOID lpBuffer, DWORD cbBuffer);

	HRESULT CrackURL (PCWSTR pcwzURL, INT cchURL, PWSTR pwzScheme, INT cchMaxScheme, WINHTTP_INTERNET_SCHEME* pnScheme, PWSTR pwzHost, INT cchMaxHost, PWSTR pwzPath, INT cchMaxPath);
	HRESULT GetIEProxyConfigForCurrentUser (WINHTTP_CURRENT_USER_IE_PROXY_CONFIG* pConfig);
	VOID FreeProxyConfigData (WINHTTP_CURRENT_USER_IE_PROXY_CONFIG* pConfig);
};

class CAsyncWinHttp :
	public CBaseUnknown,
	public IAsyncWinHttp
{
private:
	CWinHttp* m_pWinHttp;

	CRITICAL_SECTION m_cs;

	IAsyncWinHttpCallback* m_pCallback;

	HANDLE m_hCompletion;
	HINTERNET m_hRequest;
	ULONG m_cHandleLocks;

	LPBYTE m_pbPostBuffer;
	LPBYTE m_pbReadBuffer;
	DWORD m_cbBuffer;

	BOOL m_fOwnEvent : 1,
		m_fAborted : 1,
		m_fClosing : 1,
		m_fPostData : 1;
	DWORD m_dwStatusCode;
	HRESULT m_hrRequestResult;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IAsyncWinHttp)
	END_UNK_MAP

	static HRESULT CreateAsyncEvent (__out HANDLE* phCompletion);

public:
	CAsyncWinHttp (CWinHttp* pWinHttp, IAsyncWinHttpCallback* pCallback, HANDLE hCompletion = NULL);
	~CAsyncWinHttp ();

	virtual HRESULT OpenRequest (HINTERNET hServer, PCWSTR pcwzVerb, PCWSTR pcwzResource,
		PCWSTR pcwzReferrer, PCWSTR* ppcwzAcceptTypes, DWORD dwFlags, DWORD dwSecurityFlags,
		PCWSTR pcwzHeaders, INT cchHeaders,
		PCWSTR pcwzUserName, INT cchUserName, PCWSTR pcwzPassword, INT cchPassword);
	virtual HANDLE GetCompletionEvent (VOID);
	virtual HRESULT Abort (HRESULT hrResult);

private:
	bool LockHandle (VOID);
	VOID ReleaseHandle (HRESULT hr);

	VOID SendRequestComplete (VOID);
	VOID WriteComplete (VOID);
	VOID HeadersAvailable (VOID);
	VOID ReadComplete (DWORD cbRead);

	VOID AsyncCallback (DWORD dwCode, PVOID pvInfo, DWORD dwLength);

	static VOID CALLBACK _AsyncCallback (HINTERNET hRequest, PVOID pvContext, DWORD dwCode, PVOID pvInfo, DWORD dwLength);
};
