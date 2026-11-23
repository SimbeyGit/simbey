#include "Runtime.h"
#include "MenuRuntime.h"

namespace QF
{
	HRESULT CFrame::CreateNative()
	{
		HRESULT hr;
		QuadooForm::ContextMode mode;

		Check(m_context->GetMode(&mode));
		CheckIf(!m_parent && mode != QuadooForm::Design,E_INVALIDARG);
		Check(CContainer::CreateNative());

	Cleanup:
		return hr;
	}

	CForm::~CForm()
	{
		if(m_smallIcon)
			DestroyIcon(m_smallIcon);
		if(m_largeIcon)
			DestroyIcon(m_largeIcon);
	}

	VOID CForm::NativeStyle(PCWSTR* className, DWORD* style, DWORD* extended)
	{
		QuadooForm::ContextMode mode; m_context->GetMode(&mode);
		CScrollingContainer::NativeStyle(className,style,extended);
		if(mode != QuadooForm::Design)
		{
			*style &= ~WS_CHILD; m_context->GetHostHandle(&m_owner);
			INT border = Integer(L"BorderStyle"), icons = Integer(L"BorderIcons");
			if(border != QuadooForm::BorderNone)
			{
				*style |= WS_CAPTION;
				if(border == QuadooForm::BorderSizeable) *style |= WS_THICKFRAME;
				if(border == QuadooForm::BorderDialog) *extended |= WS_EX_DLGMODALFRAME;
				if(icons & QuadooForm::BorderSystemMenu)
				{
					*style |= WS_SYSMENU;
					if(icons & QuadooForm::BorderMinimize) *style |= WS_MINIMIZEBOX;
					if(icons & QuadooForm::BorderMaximize) *style |= WS_MAXIMIZEBOX;
				}
			}
			if(Integer(L"FormStyle") == QuadooForm::FormStayOnTop) *extended |= WS_EX_TOPMOST;
		}
	}

	HRESULT CForm::ApplyWindowStyle()
	{
		HRESULT hr;
		QuadooForm::ContextMode mode;
		DWORD style = WS_CLIPSIBLINGS, extended = 0;
		PCWSTR className = NULL;
		RECT bounds;
		RECT desired;
		BOOL adjusted = FALSE;
		BOOL previous = m_menuAdjusting;

		Check(m_context->GetMode(&mode));
		CheckIfIgnore(!m_hwnd || mode == QuadooForm::Design,S_OK);
		CheckIf(!GetClientRect(m_hwnd,&bounds),HrEnsureLastError());
		desired = bounds;
		if(m_menu)
		{
			m_menuAdjusting = TRUE;
			adjusted = TRUE;
		}
		// Client dimensions exclude visible scrollbars. Preserve the allocated
		// client area across style changes rather than subtracting the bars again.
		if(GetWindowLongW(m_hwnd,GWL_STYLE) & WS_VSCROLL) bounds.right += GetSystemMetrics(SM_CXVSCROLL);
		if(GetWindowLongW(m_hwnd,GWL_STYLE) & WS_HSCROLL) bounds.bottom += GetSystemMetrics(SM_CYHSCROLL);
		NativeStyle(&className,&style,&extended);
		const DWORD mask = WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
		SetWindowLongW(m_hwnd,GWL_STYLE,(GetWindowLongW(m_hwnd,GWL_STYLE) & ~mask) | (style & mask));
		SetWindowLongW(m_hwnd,GWL_EXSTYLE,(GetWindowLongW(m_hwnd,GWL_EXSTYLE) & ~(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST)) | (extended & (WS_EX_DLGMODALFRAME | WS_EX_TOPMOST)));
		{
			CheckIf(!AdjustForWindowDPI(&bounds,style & ~(WS_HSCROLL | WS_VSCROLL),extended,m_hwnd),HrEnsureLastError());
			CheckIf(!SetWindowPos(m_hwnd,extended & WS_EX_TOPMOST ? HWND_TOPMOST : HWND_NOTOPMOST,0,0,bounds.right - bounds.left,bounds.bottom - bounds.top,SWP_NOMOVE | SWP_NOACTIVATE | SWP_FRAMECHANGED),HrEnsureLastError());
		}
		if(m_menu)
		{
			Check(FitMenuClient(m_hwnd,desired.right,desired.bottom));
		}
		{
			HMENU menu = GetSystemMenu(m_hwnd,FALSE);
			if(menu)
				EnableMenuItem(menu,SC_CLOSE,MF_BYCOMMAND | ((Integer(L"BorderIcons") & QuadooForm::BorderClose) ? MF_ENABLED : MF_GRAYED));
		}

	Cleanup:
		if(adjusted)
		{
			m_menuAdjusting = previous;
		}
		return hr;
	}

	HRESULT CForm::ApplyIcons()
	{
		HRESULT hr;
		TStackRef<IJSONValue> value;
		HICON smallIcon = NULL, largeIcon = NULL;
		Check(StoredValue(L"Icon",&value));
		Check(QuadooLoadIconValue(value,min(256,max(1,MulDiv(GetSystemMetrics(SM_CXSMICON),m_dpi,SystemDPI(NULL)))),min(256,max(1,MulDiv(GetSystemMetrics(SM_CYSMICON),m_dpi,SystemDPI(NULL)))),&smallIcon));
		Check(QuadooLoadIconValue(value,min(256,max(1,MulDiv(GetSystemMetrics(SM_CXICON),m_dpi,SystemDPI(NULL)))),min(256,max(1,MulDiv(GetSystemMetrics(SM_CYICON),m_dpi,SystemDPI(NULL)))),&largeIcon));
		if(m_hwnd)
		{
			SendMessageW(m_hwnd,WM_SETICON,ICON_SMALL,reinterpret_cast<LPARAM>(smallIcon ? smallIcon : LoadIcon(NULL,IDI_APPLICATION)));
			SendMessageW(m_hwnd,WM_SETICON,ICON_BIG,reinterpret_cast<LPARAM>(largeIcon ? largeIcon : LoadIcon(NULL,IDI_APPLICATION)));
		}
		if(m_smallIcon) DestroyIcon(m_smallIcon); if(m_largeIcon) DestroyIcon(m_largeIcon);
		m_smallIcon = smallIcon; m_largeIcon = largeIcon; smallIcon = largeIcon = NULL;
	Cleanup:
		if(smallIcon) DestroyIcon(smallIcon); if(largeIcon) DestroyIcon(largeIcon);
		return hr;
	}

	HRESULT CForm::ApplyNative(PCWSTR name)
	{
		HRESULT hr;
		if(0 == TStrCmpAssert(name,L"Visible") && m_state == QuadooForm::Ready)
		{ Check(ShowForm(Boolean(L"Visible") ? SW_SHOW : SW_HIDE)); }
		else { Check(CScrollingContainer::ApplyNative(name)); }
		if(0 == TStrCmpAssert(name,L"MainMenu") && m_state >= QuadooForm::Binding)
		{
			Check(RefreshMenu());
		}
		if(0 == TStrCmpAssert(name,L"Icon")) { Check(ApplyIcons()); }
		if(0 == TStrCmpAssert(name,L"BorderStyle") || 0 == TStrCmpAssert(name,L"BorderIcons") || 0 == TStrCmpAssert(name,L"FormStyle")) { Check(ApplyWindowStyle()); }
		if(0 == TStrCmpAssert(name,L"WindowState") && m_state == QuadooForm::Ready && IsWindowVisible(m_hwnd)) { Check(ShowForm(SW_SHOW)); }
	Cleanup:
		return hr;
	}

	HRESULT CForm::DestroyNative()
	{
		if(m_hwnd && m_menu)
		{
			if(m_menuLoops)
			{
				EndMenu();
			}
			SetMenu(m_hwnd,NULL);
		}
		SafeRelease(m_menu);
		m_menu = NULL;
		HRESULT hr = CScrollingContainer::DestroyNative();
		if(m_smallIcon) { DestroyIcon(m_smallIcon); m_smallIcon = NULL; }
		if(m_largeIcon) { DestroyIcon(m_largeIcon); m_largeIcon = NULL; }
		return hr;
	}

	STDMETHODIMP CForm::CompleteModal (LRESULT result)
	{
		HRESULT hr;
		TStackRef<CControl> keepAlive(this);
		TStackRef<IJSONValue> previous;
		TStackRef<IJSONValue> proposed;

		Check(CheckAlive());
		CheckIfIgnore(!result || m_closing,S_FALSE);
		Check(StoredValue(L"ModalResult",&previous));
		Check(JSONCreateLongInteger(result,&proposed));
		Check(SetStoredValue(L"ModalResult",proposed));
		hr = Close();
		if(m_state < QuadooForm::Destroying)
		{
			HRESULT restore = SetStoredValue(L"ModalResult",previous);
			if(FAILED(restore) && SUCCEEDED(hr))
			{
				hr = restore;
			}
		}

	Cleanup:
		return hr;
	}
}
