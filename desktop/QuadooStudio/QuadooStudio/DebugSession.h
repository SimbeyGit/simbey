#pragma once

#include <windows.h>
#include "Library\Core\Array.h"
#include "Library\Core\MemoryStream.h"
#include "Library\Util\RString.h"
#include "DebugProtocol.h"

struct QUADOO_CALL_STACK_FRAME
{
	RSTRING rstrName;
	DWORD ip;
	DWORD sp;
	DWORD fp;
	DWORD nExecutionDepth;
};

interface IStartDebuggerStatus
{
	virtual VOID ReportStatus (PCWSTR pcwzStatus, INT cchStatus) = 0;
	virtual BOOL CheckAbortFlag (VOID) = 0;
};

class CQuadooDebugSession
{
private:
	HANDLE m_hProcess;
	SOCKET m_sServer;
	BOOL m_fWinsock;
	BOOL m_fPaused;
	BOOL m_fLocationChanged;
	BOOL m_fException;
	BOOL m_fAutoStartDebugger;
	RSTRING m_rstrCurrentFile;
	DWORD m_nCurrentLine;
	CMemoryStream m_stmIncoming;
	TArray<QUADOO_CALL_STACK_FRAME> m_aStackFrames;
	BOOL m_fVariableValueChanged;
	DWORD m_nVariableRequestId;
	HRESULT m_hrVariableValue;
	RSTRING m_rstrVariableValue;

public:
	CQuadooDebugSession (BOOL fAutoStartDebugger);
	~CQuadooDebugSession ();

	HRESULT Initialize (VOID);
	HRESULT Start (PCWSTR pcwzDebugger, PCWSTR pcwzStartDir, __in_opt RSTRING rstrHost, INT nPort,
		PCWSTR pcwzScriptPath, RSTRING rstrScriptArgs,
		CMemoryStream* pstmQBC, CMemoryStream* pstmDebug, CMemoryStream* pstmBreakpoints,
		IStartDebuggerStatus* pDebuggerStatus);
	HRESULT AssociateSocket (HWND hwndNotify);

	BOOL IsActive (VOID) const;
	BOOL IsPaused (VOID) const { return m_fPaused; }
	BOOL IsCurrentLocation (PCWSTR pcwzFile, DWORD nLine) const;
	DWORD GetStackFrameCount (VOID) const { return static_cast<DWORD>(m_aStackFrames.Length()); }
	HRESULT GetStackFrame (DWORD idxFrame, __out PCWSTR* ppcwzName, __out DWORD* pnIP,
		__out DWORD* pnSP, __out DWORD* pnFP, __out DWORD* pnExecutionDepth) const;
	HRESULT GetCurrentLocation (__deref_out RSTRING* prstrFile, __out DWORD* pnLine, __out BOOL* pfException);
	HRESULT SendCommand (DWORD nMessage);
	HRESULT SendBreakpoint (BOOL fAdd, PCWSTR pcwzFile, DWORD nLine);
	HRESULT SendVariableQuery (const QUADOO_DEBUG_VARIABLE_QUERY& query);
	HRESULT GetVariableValue (__out DWORD* pnRequestId, __out HRESULT* phrValue,
		__deref_out RSTRING* prstrValue);
	HRESULT OnSocketEvent (WPARAM wParam, LPARAM lParam);
	VOID Stop (VOID);

private:
	HRESULT StartProcess (PCWSTR pcwzDebugger, PCWSTR pcwzStartDir, INT nPort, IStartDebuggerStatus* pDebuggerStatus);
	HRESULT Connect (__in_opt RSTRING rstrHost, INT nPort, IStartDebuggerStatus* pDebuggerStatus);
	HRESULT SendStartupData (PCWSTR pcwzScriptPath, RSTRING rstrScriptArgs,
		CMemoryStream* pstmQBC, CMemoryStream* pstmDebug, CMemoryStream* pstmBreakpoints);
	HRESULT SendDbgMsg (DWORD nMessage, __in_bcount_opt(cbPayload) const VOID* pvPayload, DWORD cbPayload);
	HRESULT SendAll (__in_bcount(cbData) const BYTE* pbData, DWORD cbData);
	HRESULT ReceiveIncoming (VOID);
	HRESULT ProcessIncomingMessages (VOID);
	HRESULT ProcessIncomingMessage (DWORD nMessage, __in_bcount(cbPayload) const BYTE* pbPayload, DWORD cbPayload);
	HRESULT StoreStackFrames (__in_bcount(cbPayload) const BYTE* pbPayload, DWORD cbPayload);
	HRESULT StoreVariableValue (__in_bcount(cbPayload) const BYTE* pbPayload, DWORD cbPayload);
	VOID ClearVariableValue (VOID);
	VOID ClearStackFrames (VOID);
	VOID CloseSocket (VOID);
};
