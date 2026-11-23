#pragma once

#include "Library\Core\Array.h"
#include "Library\Core\BaseUnknown.h"
#include "Published\QuadooVM.h"
#include "Published\WQVMInterfaces.h"

class CQVMWinHost;

class CQVMRunController :
	public CBaseUnknown,
	public IQVMMessagePumpController
{
private:
	QuadooVM::QVARIANT* m_pqvResult;
	BOOL m_fRunning;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IQVMMessagePumpController)
	END_UNK_MAP

public:
	CQVMRunController (QuadooVM::QVARIANT* pqvResult);

	virtual VOID STDMETHODCALLTYPE SetRunning (BOOL fRunning);
	virtual BOOL STDMETHODCALLTYPE GetRunning (VOID);
	virtual VOID STDMETHODCALLTYPE SetResult (QuadooVM::QVARIANT* pqv);
};

class CQVMMessagePump :
	public CBaseUnknown,
	public CQuadooObjectImpl,
	public IQVMMessagePump,
	public IQuadooWaitForEvents
{
private:
	CQVMWinHost* m_pHost;

	IQVMMessagePumpController* m_pController;
	CQVMMessagePump* m_pPrevPump;

	IQVMMessagePumpTask* m_pTask;
	TArray<IQVMMessageHandler*> m_aHandlers;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooObject)
		UNK_INTERFACE(IQVMMessagePump)
		UNK_INTERFACE(IQuadooWaitForEvents)
	END_UNK_MAP

public:
	CQVMMessagePump (CQVMWinHost* pHost);
	virtual ~CQVMMessagePump ();

	// IQuadooObject
	virtual HRESULT STDMETHODCALLTYPE Invoke (__in_opt IQuadooVM* pVM, RSTRING rstrMethod, QuadooVM::QVPARAMS* pqvParams, __out QuadooVM::QVARIANT* pqvResult);
	virtual HRESULT STDMETHODCALLTYPE GetProperty (__in_opt IQuadooVM* pVM, RSTRING rstrProperty, __out QuadooVM::QVARIANT* pqvResult);
	virtual HRESULT STDMETHODCALLTYPE SetProperty (__in_opt IQuadooVM* pVM, RSTRING rstrProperty, QuadooVM::QVARIANT* pqv);

	// IQVMMessagePump
	virtual HRESULT STDMETHODCALLTYPE ProcessMessages (VOID);
	virtual VOID STDMETHODCALLTYPE SetMessagePumpTask (__in_opt IQVMMessagePumpTask* pMessagePumpTask);
	virtual HRESULT STDMETHODCALLTYPE AddMessageHandler (IQVMMessageHandler* pMessageHandler);
	virtual HRESULT STDMETHODCALLTYPE RemoveMessageHandler (IQVMMessageHandler* pMessageHandler);
	virtual HRESULT STDMETHODCALLTYPE UseController (__in_opt IQVMMessagePumpController* pController);
	virtual VOID STDMETHODCALLTYPE RunTask (VOID);

	// IQuadooWaitForEvents
	virtual DWORD STDMETHODCALLTYPE WaitForEvents (DWORD cEvents, __in_ecount(cEvents) const HANDLE* prghEvents, DWORD msTimeout);

private:
	VOID RouteMessage (const MSG* pcmsg);
};
