#pragma once

#include "RuntimeObjects.h"
#include "AsyncWaits.h"
#include "..\Published\QuadooContainerMessages.h"

namespace AQVM
{
	class CAsyncTarget;
	struct AsyncRequest;

	class CAsyncCalls : public CBaseUnknown
	{
		CApplicationRuntime* m_runtime;
		BOOL m_closed;
		BOOL m_reapingWaits;
		BOOL m_reapPending;
		CAsyncWait* m_cleanupWaits;
		TArray<CAsyncTarget*> m_targets;
		TMap<UINT_PTR,AsyncRequest*> m_requests;
		HRESULT Prepare(CInstanceLink* link, IQuadooComponent* component, IQuadooObject* source, IQuadooObject* callback);
		VOID FinishWait(AsyncRequest* request);
		HRESULT FindTarget(CInstanceLink* link, IQuadooControlContainer* container, HWND window, CAsyncTarget** target);
		VOID Retire(CAsyncTarget* target);
		VOID CancelRequests(CAsyncTarget* target);
		VOID RetireIdle(CAsyncTarget* target);
		VOID Diagnose(CApplicationRuntime* runtime, CAsyncTarget* target, HRESULT error, QuadooVM::QVARIANT* result, BOOL waiting = FALSE, DWORD notificationError = ERROR_SUCCESS);
	public:
		BEGIN_UNK_MAP
			UNK_INTERFACE_PATH(IUnknown,CBaseUnknown)
		END_UNK_MAP

		CAsyncCalls(CApplicationRuntime* runtime);
		~CAsyncCalls();

		HRESULT Enqueue(CInstanceLink* link, IQuadooComponent* component, IQuadooObject* callback);
		HRESULT EnqueueWait(CInstanceLink* link, IQuadooComponent* component, IQuadooObject* source, IQuadooObject* callback);
		VOID ReapWaits();
		HRESULT HandleMessage(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result);
		VOID CloseTarget(CAsyncTarget* target, BOOL destroyed);
		VOID CancelBinding(CInstanceLink* link);
		VOID ReapClosed();
		VOID Shutdown();
	};

#ifdef QUADOO_ASYNC_TESTS
	struct AsyncTestHooks
	{
		INT failAllocation;
		DWORD postError;
		BOOL failTicket;
		UINT requests;
		UINT targets;
	};
	AsyncTestHooks& AsyncTests();
#endif
}
