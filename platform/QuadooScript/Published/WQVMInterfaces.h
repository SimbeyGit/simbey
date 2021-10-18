#pragma once

interface __declspec(uuid("8DA9E0C6-58B7-45d9-8707-6D653626351C")) IQVMMessagePumpTask : IUnknown
{
	virtual VOID STDMETHODCALLTYPE OnExecuteTask (VOID) = 0;
};

interface __declspec(uuid("2286ACCB-1C28-4dfa-8DFB-71B19B5F39FE")) IQVMMessageHandler : IUnknown
{
	virtual BOOL STDMETHODCALLTYPE OnHandleMessage (const MSG* pcmsg) = 0;
};

interface __declspec(uuid("FA807275-5A05-4670-9B3F-4208908C3636")) IQVMMessagePumpController : IUnknown
{
	virtual VOID STDMETHODCALLTYPE SetRunning (BOOL fRunning) = 0;
	virtual BOOL STDMETHODCALLTYPE GetRunning (VOID) = 0;
	virtual VOID STDMETHODCALLTYPE SetResult (QuadooVM::QVARIANT* pqv) = 0;
};

interface __declspec(uuid("A87EE9A5-7AB8-4c10-9337-3AEE6C3FFBC4")) IQVMMessagePump : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE ProcessMessages (VOID) = 0;
	virtual VOID STDMETHODCALLTYPE SetMessagePumpTask (__in_opt IQVMMessagePumpTask* pMessagePumpTask) = 0;
	virtual HRESULT STDMETHODCALLTYPE AddMessageHandler (IQVMMessageHandler* pMessageHandler) = 0;
	virtual HRESULT STDMETHODCALLTYPE RemoveMessageHandler (IQVMMessageHandler* pMessageHandler) = 0;
	virtual HRESULT STDMETHODCALLTYPE UseController (__in_opt IQVMMessagePumpController* pController) = 0;
	virtual VOID STDMETHODCALLTYPE RunTask (VOID) = 0;
};
