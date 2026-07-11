#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\MemoryStream.h"
#include "Library\Util\Formatting.h"
#include "Library\Util\RString.h"
#include "DebugProtocol.h"
#include "DebugSession.h"

CQuadooDebugSession::CQuadooDebugSession (BOOL fAutoStartDebugger) :
	m_hProcess(NULL),
	m_sServer(INVALID_SOCKET),
	m_fWinsock(FALSE),
	m_fPaused(FALSE),
	m_fLocationChanged(FALSE),
	m_fException(FALSE),
	m_fAutoStartDebugger(fAutoStartDebugger),
	m_rstrCurrentFile(NULL),
	m_nCurrentLine(0),
	m_fVariableValueChanged(FALSE),
	m_nVariableRequestId(0),
	m_hrVariableValue(S_FALSE),
	m_rstrVariableValue(NULL)
{
}

CQuadooDebugSession::~CQuadooDebugSession ()
{
	Stop();
	ClearStackFrames();
	ClearVariableValue();
	RStrRelease(m_rstrCurrentFile);
}

HRESULT CQuadooDebugSession::GetStackFrame (DWORD idxFrame, PCWSTR* ppcwzName, DWORD* pnIP,
	DWORD* pnSP, DWORD* pnFP, DWORD* pnExecutionDepth) const
{
	if(NULL == ppcwzName || NULL == pnIP || NULL == pnSP || NULL == pnFP || NULL == pnExecutionDepth)
		return E_POINTER;
	if(idxFrame >= static_cast<DWORD>(m_aStackFrames.Length()))
		return HRESULT_FROM_WIN32(ERROR_INVALID_INDEX);

	const QUADOO_CALL_STACK_FRAME& frame = m_aStackFrames[idxFrame];
	*ppcwzName = RStrToWide(frame.rstrName);
	*pnIP = frame.ip;
	*pnSP = frame.sp;
	*pnFP = frame.fp;
	*pnExecutionDepth = frame.nExecutionDepth;
	return S_OK;
}

HRESULT CQuadooDebugSession::GetCurrentLocation (RSTRING* prstrFile, DWORD* pnLine, BOOL* pfException)
{
	if(NULL == prstrFile || NULL == pnLine || NULL == pfException)
		return E_POINTER;

	RStrSet(*prstrFile, m_rstrCurrentFile);
	*pnLine = m_nCurrentLine;
	*pfException = m_fException;

	if(!m_fLocationChanged)
		return S_FALSE;

	m_fLocationChanged = FALSE;

	return S_OK;
}

HRESULT CQuadooDebugSession::Initialize (VOID)
{
	HRESULT hr;
	WSADATA wsaData;

	CheckWin32Error(WSAStartup(MAKEWORD(2, 2), &wsaData));
	m_fWinsock = TRUE;
	hr = S_OK;

Cleanup:
	return hr;
}

HRESULT CQuadooDebugSession::Start (PCWSTR pcwzDebugger, PCWSTR pcwzStartDir, __in_opt RSTRING rstrHost, INT nPort,
	PCWSTR pcwzScriptPath, RSTRING rstrScriptArgs,
	CMemoryStream* pstmQBC, CMemoryStream* pstmDebug, CMemoryStream* pstmBreakpoints,
	IStartDebuggerStatus* pDebuggerStatus)
{
	HRESULT hr;

	CheckIf(NULL == pcwzDebugger || NULL == pcwzScriptPath || NULL == pstmQBC || NULL == pstmDebug || NULL == pstmBreakpoints, E_INVALIDARG);
	CheckIf(1 > nPort || 65535 < nPort, E_INVALIDARG);
	CheckIf(NULL != m_hProcess, HRESULT_FROM_WIN32(ERROR_BUSY));

	if(m_fAutoStartDebugger)
		Check(StartProcess(pcwzDebugger, pcwzStartDir, nPort, pDebuggerStatus));
	Check(Connect(rstrHost, nPort, pDebuggerStatus));
	CheckIfIgnore(pDebuggerStatus->CheckAbortFlag(), E_ABORT);
	pDebuggerStatus->ReportStatus(SLP(L"Configuring debugger..."));
	Check(SendStartupData(pcwzScriptPath, rstrScriptArgs, pstmQBC, pstmDebug, pstmBreakpoints));

Cleanup:
	if(FAILED(hr))
		Stop();
	return hr;
}

HRESULT CQuadooDebugSession::AssociateSocket (HWND hwndNotify)
{
	HRESULT hr;

	CheckIf(NULL == hwndNotify, E_INVALIDARG);
	CheckIf(SOCKET_ERROR == WSAAsyncSelect(m_sServer, hwndNotify, WM_QUADOO_DEBUG_SOCKET, FD_READ | FD_CLOSE), HRESULT_FROM_WIN32(WSAGetLastError()));
	hr = S_OK;

Cleanup:
	return hr;
}

BOOL CQuadooDebugSession::IsActive (VOID) const
{
	if(m_fAutoStartDebugger && m_hProcess)
	{
		DWORD dwExitCode;
		return NULL != m_hProcess && GetExitCodeProcess(m_hProcess, &dwExitCode) && STILL_ACTIVE == dwExitCode;
	}
	return INVALID_SOCKET != m_sServer;
}

BOOL CQuadooDebugSession::IsCurrentLocation (PCWSTR pcwzFile, DWORD nLine) const
{
	return m_fPaused && m_rstrCurrentFile && pcwzFile && m_nCurrentLine == nLine &&
		0 == TStrCmpIAssert(RStrToWide(m_rstrCurrentFile), pcwzFile);
}

HRESULT CQuadooDebugSession::SendCommand (DWORD nMessage)
{
	HRESULT hr;

	CheckIf(!IsActive(), HRESULT_FROM_WIN32(ERROR_PROCESS_ABORTED));
	Check(SendDbgMsg(nMessage, NULL, 0));
	if(QDM_STEP_INTO == nMessage || QDM_STEP_OVER == nMessage || QDM_CONTINUE == nMessage)
	{
		m_fPaused = FALSE;
		ClearStackFrames();
		ClearVariableValue();
	}

Cleanup:
	return hr;
}

HRESULT CQuadooDebugSession::SendBreakpoint (BOOL fAdd, PCWSTR pcwzFile, DWORD nLine)
{
	HRESULT hr;
	CMemoryStream stmPayload;
	QUADOO_DEBUG_BREAKPOINT breakpoint;
	DWORD cbFile, cb;

	CheckIf(NULL == pcwzFile, E_INVALIDARG);
	CheckIf(!IsActive(), HRESULT_FROM_WIN32(ERROR_PROCESS_ABORTED));
	breakpoint.cchFile = static_cast<DWORD>(TStrLenAssert(pcwzFile));
	breakpoint.nLine = nLine;
	Check(HrSafeMult(breakpoint.cchFile, static_cast<DWORD>(sizeof(WCHAR)), &cbFile));
	Check(stmPayload.Write(&breakpoint, sizeof(breakpoint), &cb));
	Check(stmPayload.Write(pcwzFile, cbFile, &cb));
	Check(SendDbgMsg(fAdd ? QDM_BREAKPOINT_ADD : QDM_BREAKPOINT_REMOVE,
		stmPayload.GetReadPtr(), stmPayload.DataRemaining()));

Cleanup:
	return hr;
}

HRESULT CQuadooDebugSession::SendVariableQuery (const QUADOO_DEBUG_VARIABLE_QUERY& query)
{
	HRESULT hr;

	CheckIf(!IsActive() || !m_fPaused, HRESULT_FROM_WIN32(ERROR_DEBUGGER_INACTIVE));
	Check(SendDbgMsg(QDM_VARIABLE_QUERY, &query, sizeof(query)));

Cleanup:
	return hr;
}

HRESULT CQuadooDebugSession::GetVariableValue (DWORD* pnRequestId, HRESULT* phrValue, RSTRING* prstrValue)
{
	if(NULL == pnRequestId || NULL == phrValue || NULL == prstrValue)
		return E_POINTER;
	if(!m_fVariableValueChanged)
		return S_FALSE;

	*pnRequestId = m_nVariableRequestId;
	*phrValue = m_hrVariableValue;
	RStrSet(*prstrValue, m_rstrVariableValue);
	m_fVariableValueChanged = FALSE;
	return S_OK;
}

HRESULT CQuadooDebugSession::OnSocketEvent (WPARAM wParam, LPARAM lParam)
{
	HRESULT hr = S_OK;
	INT nError;

	if(static_cast<SOCKET>(wParam) != m_sServer)
		return S_FALSE;

	nError = WSAGETSELECTERROR(lParam);
	CheckIf(0 != nError, HRESULT_FROM_WIN32(nError));

	switch(WSAGETSELECTEVENT(lParam))
	{
	case FD_READ:
		Check(ReceiveIncoming());
		break;
	case FD_CLOSE:
		hr = HRESULT_FROM_WIN32(ERROR_CONNECTION_ABORTED);
		break;
	}

Cleanup:
	if(FAILED(hr))
		CloseSocket();
	return hr;
}

VOID CQuadooDebugSession::Stop (VOID)
{
	ClearStackFrames();
	m_fPaused = FALSE;

	if(IsActive())
	{
		if(INVALID_SOCKET != m_sServer)
		{
			if(SUCCEEDED(SendDbgMsg(QDM_STOP, NULL, 0)))
			{
				if(WAIT_TIMEOUT == WaitForSingleObject(m_hProcess, 1500))
					TerminateProcess(m_hProcess, ERROR_CANCELLED);
			}
			else
				TerminateProcess(m_hProcess, ERROR_CANCELLED);
		}
		else
			TerminateProcess(m_hProcess, ERROR_CANCELLED);
	}

	SafeCloseHandle(m_hProcess);

	CloseSocket();
	if(m_fWinsock)
	{
		WSACleanup();
		m_fWinsock = FALSE;
	}
}

HRESULT CQuadooDebugSession::StartProcess (PCWSTR pcwzDebugger, PCWSTR pcwzStartDir, INT nPort, IStartDebuggerStatus* pDebuggerStatus)
{
	HRESULT hr;
	INT cchStatus;
	WCHAR wzCommand[MAX_PATH * 2];
	STARTUPINFO si = {0};
	PROCESS_INFORMATION pi = {0};
	PCWSTR pcwzDebuggerExe = TStrRChr(pcwzDebugger, L'\\');

	Assert(m_fAutoStartDebugger);

	if(pcwzDebuggerExe)
		pcwzDebuggerExe++;
	else
		pcwzDebuggerExe = pcwzDebugger;

	Check(Formatting::TPrintF(wzCommand, ARRAYSIZE(wzCommand), &cchStatus, L"Launching %ls", pcwzDebuggerExe));
	pDebuggerStatus->ReportStatus(wzCommand, cchStatus);

	si.cb = sizeof(si);
	Check(Formatting::TPrintF(wzCommand, ARRAYSIZE(wzCommand), NULL, L"\"%ls\" -debug-port %d", pcwzDebugger, nPort));
	CheckIfGetLastError(!CreateProcessW(pcwzDebugger, wzCommand, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, pcwzStartDir, &si, &pi));

	m_hProcess = pi.hProcess;
	CloseHandle(pi.hThread);

Cleanup:
	return hr;
}

HRESULT CQuadooDebugSession::Connect (__in_opt RSTRING rstrHost, INT nPort, IStartDebuggerStatus* pDebuggerStatus)
{
	HRESULT hr = HRESULT_FROM_WIN32(WSAECONNREFUSED);
	sockaddr_in address = {0};
	ADDRINFOW hints = {0}, *pAddresses = NULL;
	BOOL fRemoteHost = 0 < RStrLen(rstrHost);

	if(fRemoteHost)
	{
		WCHAR wzPort[16];
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		Check(Formatting::TInt32ToAsc(nPort, wzPort, ARRAYSIZE(wzPort), 10, NULL));
		INT nResult = GetAddrInfoW(RStrToWide(rstrHost), wzPort, &hints, &pAddresses);
		CheckIf(0 != nResult, HRESULT_FROM_WIN32(nResult));
	}
	else
	{
		address.sin_family = AF_INET;
		address.sin_port = htons(static_cast<u_short>(nPort));
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	}

	pDebuggerStatus->ReportStatus(SLP(L"Connecting..."));

	for(INT i = 0; i < 100; i++)
	{
		ADDRINFOW localAddress = {0};
		ADDRINFOW* pAddress;

		CheckIfIgnore(pDebuggerStatus->CheckAbortFlag(), E_ABORT);

		if(fRemoteHost)
			pAddress = pAddresses;
		else
		{
			localAddress.ai_family = AF_INET;
			localAddress.ai_socktype = SOCK_STREAM;
			localAddress.ai_protocol = IPPROTO_TCP;
			localAddress.ai_addr = reinterpret_cast<sockaddr*>(&address);
			localAddress.ai_addrlen = sizeof(address);
			pAddress = &localAddress;
		}

		for(; pAddress; pAddress = pAddress->ai_next)
		{
			m_sServer = socket(pAddress->ai_family, pAddress->ai_socktype, pAddress->ai_protocol);
			CheckIf(INVALID_SOCKET == m_sServer, HRESULT_FROM_WIN32(WSAGetLastError()));

			if(0 == connect(m_sServer, pAddress->ai_addr, static_cast<INT>(pAddress->ai_addrlen)))
			{
				hr = S_OK;
				break;
			}

			hr = HRESULT_FROM_WIN32(WSAGetLastError());
			closesocket(m_sServer);
			m_sServer = INVALID_SOCKET;
		}
		if(SUCCEEDED(hr))
			break;

		if(m_fAutoStartDebugger && !IsActive())
		{
			hr = HRESULT_FROM_WIN32(ERROR_PROCESS_ABORTED);
			break;
		}

		Sleep(50);
	}

Cleanup:
	if(pAddresses)
		FreeAddrInfoW(pAddresses);
	return hr;
}

HRESULT CQuadooDebugSession::SendStartupData (PCWSTR pcwzScriptPath, RSTRING rstrScriptArgs,
	CMemoryStream* pstmQBC, CMemoryStream* pstmDebug, CMemoryStream* pstmBreakpoints)
{
	HRESULT hr;
	DWORD dwVersion = QUADOO_DEBUG_PROTOCOL_VERSION;
	DWORD cbScriptPath, cbScriptArgs;

	Check(HrSafeMult(static_cast<DWORD>(TStrLenAssert(pcwzScriptPath)), static_cast<DWORD>(sizeof(WCHAR)), &cbScriptPath));
	Check(HrSafeMult(static_cast<DWORD>(RStrLen(rstrScriptArgs)), static_cast<DWORD>(sizeof(WCHAR)), &cbScriptArgs));

	Check(SendDbgMsg(QDM_HELLO, &dwVersion, sizeof(dwVersion)));
	Check(SendDbgMsg(QDM_SCRIPT_PATH, pcwzScriptPath, cbScriptPath));
	Check(SendDbgMsg(QDM_SCRIPT_ARGUMENTS, RStrToWide(rstrScriptArgs), cbScriptArgs));
	Check(SendDbgMsg(QDM_SCRIPT_BYTECODE, pstmQBC->GetReadPtr(), pstmQBC->DataRemaining()));
	Check(SendDbgMsg(QDM_DEBUG_DATA, pstmDebug->GetReadPtr(), pstmDebug->DataRemaining()));
	Check(SendDbgMsg(QDM_BREAKPOINTS, pstmBreakpoints->GetReadPtr(), pstmBreakpoints->DataRemaining()));
	Check(SendDbgMsg(QDM_START, NULL, 0));

Cleanup:
	return hr;
}

HRESULT CQuadooDebugSession::SendDbgMsg (DWORD nMessage, const VOID* pvPayload, DWORD cbPayload)
{
	HRESULT hr;
	QUADOO_DEBUG_MESSAGE_HEADER header;

	header.nMessage = nMessage;
	header.cbPayload = cbPayload;

	Check(SendAll(reinterpret_cast<const BYTE*>(&header), sizeof(header)));
	if(0 < cbPayload)
		Check(SendAll(reinterpret_cast<const BYTE*>(pvPayload), cbPayload));

Cleanup:
	return hr;
}

HRESULT CQuadooDebugSession::SendAll (const BYTE* pbData, DWORD cbData)
{
	HRESULT hr;
	DWORD cbSent = 0;

	CheckIf(INVALID_SOCKET == m_sServer, HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE));
	while(cbSent < cbData)
	{
		INT cb = send(m_sServer, reinterpret_cast<const CHAR*>(pbData + cbSent), static_cast<INT>(min(cbData - cbSent, static_cast<DWORD>(0x7FFFFFFF))), 0);
		if(SOCKET_ERROR == cb)
		{
			INT nError = WSAGetLastError();
			if(WSAEWOULDBLOCK == nError)
			{
				fd_set fdsWrite;
				timeval timeout = {5, 0};
				INT nSelect;

				FD_ZERO(&fdsWrite);
				FD_SET(m_sServer, &fdsWrite);
				nSelect = select(0, NULL, &fdsWrite, NULL, &timeout);
				CheckIf(SOCKET_ERROR == nSelect, HRESULT_FROM_WIN32(WSAGetLastError()));
				CheckIf(0 == nSelect, HRESULT_FROM_WIN32(ERROR_TIMEOUT));
				continue;
			}
			CheckIf(TRUE, HRESULT_FROM_WIN32(nError));
		}
		CheckIf(0 == cb, HRESULT_FROM_WIN32(ERROR_CONNECTION_ABORTED));
		cbSent += cb;
	}

	hr = S_OK;

Cleanup:
	if(FAILED(hr))
		CloseSocket();
	return hr;
}

HRESULT CQuadooDebugSession::ReceiveIncoming (VOID)
{
	HRESULT hr = S_OK;
	BYTE rgbBuffer[8192];

	for(;;)
	{
		INT cbReceived = recv(m_sServer, reinterpret_cast<CHAR*>(rgbBuffer), sizeof(rgbBuffer), 0);
		if(0 < cbReceived)
		{
			ULONG cbWritten;
			Check(m_stmIncoming.Write(rgbBuffer, cbReceived, &cbWritten));
			Check(ProcessIncomingMessages());
		}
		else if(0 == cbReceived)
		{
			hr = HRESULT_FROM_WIN32(ERROR_CONNECTION_ABORTED);
			break;
		}
		else
		{
			INT nError = WSAGetLastError();
			if(WSAEWOULDBLOCK != nError)
				hr = HRESULT_FROM_WIN32(nError);
			break;
		}
	}

Cleanup:
	return hr;
}

HRESULT CQuadooDebugSession::ProcessIncomingMessages (VOID)
{
	HRESULT hr = S_OK;

	while(m_stmIncoming.DataRemaining() >= sizeof(QUADOO_DEBUG_MESSAGE_HEADER))
	{
		QUADOO_DEBUG_MESSAGE_HEADER header;
		DWORD cbMessage;
		const BYTE* pbData = m_stmIncoming.GetReadPtr();

		CopyMemory(&header, pbData, sizeof(header));
		CheckIf(header.cbPayload > 64 * 1024 * 1024, HRESULT_FROM_WIN32(ERROR_INVALID_DATA));
		Check(HrSafeAdd(static_cast<DWORD>(sizeof(header)), header.cbPayload, &cbMessage));
		if(m_stmIncoming.DataRemaining() < cbMessage)
			break;

		Check(ProcessIncomingMessage(header.nMessage, pbData + sizeof(header), header.cbPayload));
		Check(m_stmIncoming.UpdateReadPtr(cbMessage));
	}

	if(0 == m_stmIncoming.DataRemaining())
		m_stmIncoming.Reset();

Cleanup:
	return hr;
}

HRESULT CQuadooDebugSession::ProcessIncomingMessage (DWORD nMessage, const BYTE* pbPayload, DWORD cbPayload)
{
	HRESULT hr;
	QUADOO_DEBUG_BREAKPOINT location;
	DWORD cbFile, cbExpected;

	if(QDM_STACK_FRAMES == nMessage)
		return StoreStackFrames(pbPayload, cbPayload);
	if(QDM_VARIABLE_VALUE == nMessage)
		return StoreVariableValue(pbPayload, cbPayload);
	if(QDM_BREAKPOINT_HIT != nMessage && QDM_EXCEPTION != nMessage)
		return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
	CheckIf(cbPayload < sizeof(location), HRESULT_FROM_WIN32(ERROR_INVALID_DATA));
	CopyMemory(&location, pbPayload, sizeof(location));
	Check(HrSafeMult(location.cchFile, static_cast<DWORD>(sizeof(WCHAR)), &cbFile));
	Check(HrSafeAdd(static_cast<DWORD>(sizeof(location)), cbFile, &cbExpected));
	CheckIf(cbExpected != cbPayload || 0 == location.cchFile || 0 == location.nLine, HRESULT_FROM_WIN32(ERROR_INVALID_DATA));
	RStrRelease(m_rstrCurrentFile);
	m_rstrCurrentFile = NULL;
	Check(RStrCreateW(location.cchFile, reinterpret_cast<PCWSTR>(pbPayload + sizeof(location)), &m_rstrCurrentFile));
	m_nCurrentLine = location.nLine;
	m_fException = QDM_EXCEPTION == nMessage;
	m_fLocationChanged = TRUE;
	m_fPaused = TRUE;

Cleanup:
	return hr;
}

VOID CQuadooDebugSession::ClearVariableValue (VOID)
{
	RStrRelease(m_rstrVariableValue);
	m_rstrVariableValue = NULL;
	m_fVariableValueChanged = FALSE;
	m_hrVariableValue = S_FALSE;
}

HRESULT CQuadooDebugSession::StoreVariableValue (const BYTE* pbPayload, DWORD cbPayload)
{
	HRESULT hr;
	QUADOO_DEBUG_VARIABLE_VALUE response;
	DWORD cbValue, cbExpected;

	CheckIf(cbPayload < sizeof(response), HRESULT_FROM_WIN32(ERROR_INVALID_DATA));
	CopyMemory(&response, pbPayload, sizeof(response));
	Check(HrSafeMult(response.cchValue, static_cast<DWORD>(sizeof(WCHAR)), &cbValue));
	Check(HrSafeAdd(static_cast<DWORD>(sizeof(response)), cbValue, &cbExpected));
	CheckIf(cbExpected != cbPayload || (FAILED(response.hr) && 0 != response.cchValue),
		HRESULT_FROM_WIN32(ERROR_INVALID_DATA));

	ClearVariableValue();
	if(0 < response.cchValue)
		Check(RStrCreateW(response.cchValue,
			reinterpret_cast<PCWSTR>(pbPayload + sizeof(response)), &m_rstrVariableValue));
	m_nVariableRequestId = response.requestId;
	m_hrVariableValue = response.hr;
	m_fVariableValueChanged = TRUE;
	hr = S_OK;

Cleanup:
	return hr;
}

VOID CQuadooDebugSession::ClearStackFrames (VOID)
{
	for(sysint i = 0; i < m_aStackFrames.Length(); i++)
		RStrRelease(m_aStackFrames[i].rstrName);
	m_aStackFrames.Clear();
}

HRESULT CQuadooDebugSession::StoreStackFrames (const BYTE* pbPayload, DWORD cbPayload)
{
	HRESULT hr;
	TArray<QUADOO_CALL_STACK_FRAME> aFrames;
	DWORD cFrames, idx = sizeof(cFrames);

	CheckIf(cbPayload < sizeof(cFrames), HRESULT_FROM_WIN32(ERROR_INVALID_DATA));
	CopyMemory(&cFrames, pbPayload, sizeof(cFrames));
	for(DWORD i = 0; i < cFrames; i++)
	{
		QUADOO_DEBUG_STACK_FRAME wireFrame;
		QUADOO_CALL_STACK_FRAME frame = {0};
		DWORD cbName, idxNext;

		CheckIf(cbPayload - idx < sizeof(wireFrame), HRESULT_FROM_WIN32(ERROR_INVALID_DATA));
		CopyMemory(&wireFrame, pbPayload + idx, sizeof(wireFrame));
		Check(HrSafeAdd(idx, static_cast<DWORD>(sizeof(wireFrame)), &idx));
		Check(HrSafeMult(wireFrame.cchName, static_cast<DWORD>(sizeof(WCHAR)), &cbName));
		Check(HrSafeAdd(idx, cbName, &idxNext));
		CheckIf(0 == wireFrame.cchName || idxNext > cbPayload, HRESULT_FROM_WIN32(ERROR_INVALID_DATA));

		Check(RStrCreateW(wireFrame.cchName, reinterpret_cast<PCWSTR>(pbPayload + idx), &frame.rstrName));
		frame.ip = wireFrame.ip;
		frame.sp = wireFrame.sp;
		frame.fp = wireFrame.fp;
		frame.nExecutionDepth = wireFrame.nExecutionDepth;
		hr = aFrames.Append(frame);
		if(FAILED(hr))
			RStrRelease(frame.rstrName);
		Check(hr);
		idx = idxNext;
	}
	CheckIf(idx != cbPayload, HRESULT_FROM_WIN32(ERROR_INVALID_DATA));

	ClearStackFrames();
	for(sysint i = 0; i < aFrames.Length(); i++)
	{
		Check(m_aStackFrames.Append(aFrames[i]));
		aFrames[i].rstrName = NULL;
	}
	hr = S_OK;

Cleanup:
	for(sysint i = 0; i < aFrames.Length(); i++)
		RStrRelease(aFrames[i].rstrName);
	if(FAILED(hr))
		ClearStackFrames();
	return hr;
}

VOID CQuadooDebugSession::CloseSocket (VOID)
{
	ClearStackFrames();
	ClearVariableValue();
	if(INVALID_SOCKET != m_sServer)
	{
		WSAAsyncSelect(m_sServer, NULL, 0, 0);
		shutdown(m_sServer, SD_BOTH);
		closesocket(m_sServer);
		m_sServer = INVALID_SOCKET;
	}
}
