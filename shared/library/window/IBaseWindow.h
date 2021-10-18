#pragma once

#define	DECL_WM_HANDLER(fnHandler) \
	BOOL fnHandler(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)

#define	BEGIN_WM_MAP \
	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult) \
	{

#define HANDLE_WM(wmConstant, fnHandler) \
		if(wmConstant == message) \
		{ \
			if(fnHandler(message, wParam, lParam, lResult)) \
			{ \
				return TRUE; \
			} \
		}

#define	HANDLE_ANY(fnHandler) \
		if(fnHandler(message, wParam, lParam, lResult)) \
		{ \
			return TRUE; \
		}

#define	DELEGATE_ANY(pDelegate) \
		if(pDelegate) \
		{ \
			if(pDelegate->OnDelegateMessage(this, message, wParam, lParam, lResult)) \
			{ \
				return TRUE; \
			} \
		}

#define	DELEGATE_PARENT(ParentClass) \
		HANDLE_ANY(ParentClass::DefWindowProc)

#define	END_WM_MAP \
		return FALSE; \
	}

namespace BaseWindowMessage
{
	enum Flag
	{
		SubclassedHandlers = 1,
		BaseWindowHandler = 2,
		DefaultWindowProc = 4
	};
};

interface IBaseWindow;

interface ISubclassedHandler
{
	virtual VOID OnAttached (IBaseWindow* lpWindow) = 0;
	virtual VOID OnDetached (IBaseWindow* lpWindow) = 0;
	virtual BOOL OnSubclassMessage (IBaseWindow* lpWindow, UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult) = 0;
};

interface __declspec(uuid("60BA8858-E9C8-4f22-BD55-361CDF9BEC75")) IBaseWindow : public IOleWindow
{
	virtual BOOL InvokeMessageHandler (BaseWindowMessage::Flag eTarget, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult) = 0;
	virtual HRESULT ShowWindow (INT nCmdShow) = 0;
	virtual HRESULT Destroy (VOID) = 0;
	virtual HRESULT Invalidate (BOOL fEraseBackground) = 0;
	virtual HRESULT Move (INT x, INT y, INT nWidth, INT nHeight, BOOL fRepaint) = 0;
	virtual HMENU GetMenu (VOID) = 0;
	virtual HRESULT AttachSubclassHandler (ISubclassedHandler* lpSubclass) = 0;
	virtual HRESULT DetachSubclassHandler (ISubclassedHandler* lpSubclass) = 0;
	virtual HRESULT RegisterUserMessage (__out UINT* pnUserMessage) = 0;
	virtual HRESULT UnregisterUserMessage (UINT nUserMessage) = 0;
	virtual HRESULT RegisterTimer (UINT msElapse, __out UINT_PTR* pidTimer) = 0;
	virtual HRESULT UnregisterTimer (UINT_PTR idTimer) = 0;
};
