#pragma once

#include "QuadooForm.h"
#include "Library\Core\CoreDefs.h"

namespace QuadooForm
{
	// Same-process protocol. LPARAM is opaque to the native runtime. A binder
	// that supports IQuadooMessageHandler owns and validates the actual payload.
	inline UINT ContainerCallbackMessage()
	{
		static volatile LONG registered = 0;
		UINT message = static_cast<UINT>(InterlockedCompareExchange(&registered,0,0));
		if(!message)
		{
			message = RegisterWindowMessageW(L"QuadooForm.ContainerCallback.{A56D858A-9897-4CC8-802E-256A676EB429}");
			if(message)
			{
				InterlockedCompareExchange(&registered,static_cast<LONG>(message),0);
			}
		}
		return message;
	}

	// Container implementations call this from their existing window procedure
	// for the callback message and their primary HWND's WM_NCDESTROY. The latter
	// is a notification only and must not suppress native window destruction.
	inline HRESULT ForwardContainerMessage(IQuadooConstructionContext* context, HWND window,
		UINT message, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result)
	{
		HRESULT hr;
		TStackRef<IQuadooEventsBinder> binder;
		TStackRef<IQuadooMessageHandler> handler;
		CheckIf(!context || !handled || !result,E_POINTER);
		*handled = FALSE;
		*result = 0;
		Check(context->GetBinder(&binder));
		if(binder)
		{
			hr = binder->QueryInterface(&handler);
			if(hr == E_NOINTERFACE)
			{
				hr = S_FALSE;
				goto Cleanup;
			}
			Check(hr);
			CheckIf(!handler,E_UNEXPECTED);
			Check(handler->HandleMessage(window,message,wp,lp,handled,result));
		}
	Cleanup:
		if(message == WM_NCDESTROY && handled)
		{
			*handled = FALSE;
		}
		return hr;
	}
}
