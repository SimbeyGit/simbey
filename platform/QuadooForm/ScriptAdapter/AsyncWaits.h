#pragma once

#include "ApplicationRuntime.h"

namespace AQVM
{
	// Immutable after registration. The object pointer is only an opaque value
	// for PostMessage; the worker never calls or releases it.
	struct AsyncDelivery
	{
		HWND window;
		UINT message;
		UINT_PTR ticket;
		LPARAM object;
		DWORD postError;
	};
	BOOL PostAsyncDelivery(const AsyncDelivery& delivery);

	class CAsyncWait : public CBaseUnknown
	{
		struct NativeContext
		{
			AsyncDelivery delivery;
			HWND notificationWindow;
			UINT notificationMessage;
			DWORD forcedNotificationError;
			volatile LONG notificationError;
			volatile LONG canceled;
			volatile LONG fired;
			volatile LONG finished;
			volatile LONG error;
#ifdef QUADOO_ASYNC_TESTS
			HANDLE posted;
			HANDLE finish;
			HANDLE notified;
			HANDLE finishNotification;
#endif
		};
		NativeContext m_native;
		HANDLE m_handle;
		HANDLE m_registration;
		IQuadooObject* m_source;
		static VOID CALLBACK WaitCallback(PVOID context, BOOLEAN timeout);
	public:
		CAsyncWait* cleanupNext;
		BEGIN_UNK_MAP
			UNK_INTERFACE_PATH(IUnknown,CBaseUnknown)
		END_UNK_MAP
		CAsyncWait();
		~CAsyncWait();
		HRESULT Initialize(IQuadooObject* source, const AsyncDelivery& delivery, HWND notificationWindow = NULL, UINT notificationMessage = 0);
		HRESULT Register();
		VOID Cancel();
		HRESULT Drain();
		VOID ReleaseSource();
		BOOL Fired();
		BOOL Failed();
		HRESULT PostResult();
		DWORD NotificationError();
		// Exceptional permanent unregister failure: retain native state until
		// process exit, without script references or executing code in a VM DLL.
		VOID Quarantine();
	};

#ifdef QUADOO_ASYNC_TESTS
	struct AsyncWaitTestHooks
	{
		DWORD duplicateError;
		DWORD registerError;
		DWORD notificationError;
		DWORD postError;
		UINT unregisterFailures;
		HANDLE posted;
		HANDLE finish;
		HANDLE notified;
		HANDLE finishNotification;
		HANDLE drainEntered;
		BOOL waitBeforeRegisterReturn;
		UINT waits;
		UINT handles;
		UINT registrations;
		volatile LONG notifications;
		UINT maintenanceCalls;
		UINT_PTR lastTicket;
	};
	AsyncWaitTestHooks& AsyncWaitTests();
#endif
}
