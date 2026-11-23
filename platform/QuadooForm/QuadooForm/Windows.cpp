#include "Runtime.h"
#include "..\Published\QuadooContainerMessages.h"
#include "MenuRuntime.h"

namespace QF
{
static const WCHAR ClassName[] = L"QuadooForm.Native.v2";
static const WCHAR WindowProperty[] = L"QuadooForm.Window.v2";
static const UINT DpiChanged = 0x02e0;
CWinControl* CWinControl::FocusRoot(HWND window)
{
	CWinControl* root = NULL;
	for(UINT depth = 0; window && depth < 256; depth++)
	{
		WindowData* data = static_cast<WindowData*>(GetPropW(window,WindowProperty));
		if(data)
		{
			root = data->owner;
		}
		if(!(GetWindowLongW(window,GWL_STYLE) & WS_CHILD))
		{
			break;
		}
		window = ::GetParent(window);
	}
	return root;
}

HRESULT CWinControl::CollectFocusControls(IQuadooControl* control, TArray<CWinControl*>& controls, UINT depth)
{
	HRESULT hr;
	TStackRef<IQuadooWinControl> native;
	TStackRef<IQuadooControlContainer> container;
	QuadooForm::ComponentState state;
	CheckIf(depth >= 128,QuadooForm::EInvalidDefinition);
	Check(control->GetState(&state));
	if(state != QuadooForm::Ready)
	{
		hr = S_OK;
		goto Cleanup;
	}
	Check(QueryOptional(control,&native));
	if(native)
	{
		HWND window;
		Check(native->GetHandle(&window));
		WindowData* data = static_cast<WindowData*>(GetPropW(window,WindowProperty));
		if(data && !data->content)
		{
			Check(controls.Append(data->owner));
			data->owner->AddRef();
		}
	}
	Check(QueryOptional(control,&container));
	if(container)
	{
		UINT count;
		Check(container->GetChildCount(&count));
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IQuadooControl> child;
			Check(container->GetChild(n,&child));
			Check(CollectFocusControls(child,controls,depth + 1));
		}
	}
Cleanup:
	return hr;
}

VOID CWinControl::NotifyFocus(HWND next)
{
	HRESULT hr;
	TStackRef<CWinControl> previousRoot(FocusRoot(m_hwnd));
	TStackRef<CWinControl> nextRoot(FocusRoot(next));
	TArray<CWinControl*> controls;
	BOOL differentRoots = static_cast<CWinControl*>(nextRoot) != static_cast<CWinControl*>(previousRoot);
	UINT previousSequence = previousRoot ? ++previousRoot->m_focusSequence : 0;
	UINT nextSequence = nextRoot && differentRoots ? ++nextRoot->m_focusSequence : previousSequence;
	if(previousRoot)
	{
		Check(CollectFocusControls(previousRoot->Control(),controls));
	}
	if(nextRoot && differentRoots)
	{
		Check(CollectFocusControls(nextRoot->Control(),controls));
	}
	// Reverse traversal leaves the innermost controls first. The common ancestry
	// remains entered when focus moves between siblings or internal edit HWNDs.
	for(sysint n = controls.Length(); n > 0; n--)
	{
		CWinControl* control = controls[n - 1];
		BOOL inside = next && (control->m_hwnd == next || IsChild(control->m_hwnd,next));
		if(control->m_entered && !inside)
		{
			control->m_entered = FALSE;
			if(control->m_state == QuadooForm::Ready)
			{
				Check(control->Dispatch(L"OnLeave"));
			}
		}
		if((previousRoot && previousRoot->m_focusSequence != previousSequence) ||
			(nextRoot && nextRoot->m_focusSequence != nextSequence))
		{
			hr = S_OK;
			goto Cleanup;
		}
	}
	for(sysint n = 0; n < controls.Length(); n++)
	{
		CWinControl* control = controls[n];
		BOOL inside = next && IsWindow(control->m_hwnd) && (control->m_hwnd == next || IsChild(control->m_hwnd,next));
		if(!control->m_entered && inside && control->m_state == QuadooForm::Ready)
		{
			control->m_entered = TRUE;
			Check(control->Dispatch(L"OnEnter"));
		}
		if((previousRoot && previousRoot->m_focusSequence != previousSequence) ||
			(nextRoot && nextRoot->m_focusSequence != nextSequence))
		{
			hr = S_OK;
			goto Cleanup;
		}
	}
	hr = S_OK;
Cleanup:
	ReportCallback(hr,L"Focus enter/leave");
	for(sysint n = 0; n < controls.Length(); n++)
	{
		controls[n]->Release();
	}
}

CWinControl::CWinControl(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) :
	CControl(factory, context, parent), m_hwnd(NULL), m_replacing(FALSE), m_entered(FALSE), m_focusSequence(0)
{
	m_windowData.owner = this;
	m_windowData.content = FALSE;
	m_windowData.original = NULL;
}
CWinControl::~CWinControl() { Assert(!m_hwnd); }
CForm::CForm(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : Super(factory, context, parent), m_owner(NULL), m_closing(FALSE), m_modal(FALSE), m_smallIcon(NULL), m_largeIcon(NULL), m_keyboardBusy(FALSE), m_suppressedTarget(NULL), m_suppressedScan(0), m_menu(NULL), m_menuAdjusting(FALSE), m_menuPending(FALSE), m_menuLoops(0) {}
CScrollingContainer::CScrollingContainer(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : CContainer(factory, context, parent), m_content(NULL)
{
	ZeroMemory(&m_scroll, sizeof(m_scroll)); m_contentData.owner = this; m_contentData.content = TRUE; m_contentData.original = NULL;
}
VOID CWinControl::NativeStyle(PCWSTR* className, DWORD* style, DWORD* extended) { *style |= WS_CHILD; }
VOID CContainer::NativeStyle(PCWSTR* className, DWORD* style, DWORD* extended) { CWinControl::NativeStyle(className, style, extended); *style |= WS_CLIPCHILDREN; *extended |= WS_EX_CONTROLPARENT; }
VOID CButton::NativeStyle(PCWSTR* className, DWORD* style, DWORD* extended) { CWinControl::NativeStyle(className,style,extended); *className = L"BUTTON"; *style |= BS_PUSHBUTTON | WS_TABSTOP; }
VOID CTextEdit::NativeStyle(PCWSTR* className, DWORD* style, DWORD* extended) { CWinControl::NativeStyle(className,style,extended); *className = L"EDIT"; *style |= ES_AUTOHSCROLL | WS_TABSTOP; *extended |= WS_EX_CLIENTEDGE; }
VOID CTextBox::NativeStyle(PCWSTR* className, DWORD* style, DWORD* extended)
{
	CTextEdit::NativeStyle(className,style,extended);
	if(Boolean(L"Password"))
	{
		*style |= ES_PASSWORD;
	}
}
VOID CScrollingContainer::NativeStyle(PCWSTR* className, DWORD* style, DWORD* extended) { CContainer::NativeStyle(className,style,extended); if(Boolean(L"AutoScroll")) *style |= WS_HSCROLL | WS_VSCROLL; }
HRESULT CWinControl::AdjustNativeRect(RECT* rect) { return S_OK; }
HRESULT CForm::AdjustNativeRect(RECT* rect)
{
	HRESULT hr = S_OK;
	QuadooForm::ContextMode mode; Check(m_context->GetMode(&mode));
	if(mode != QuadooForm::Design)
	{
		INT x = rect->left, y = rect->top;
		DWORD style = WS_CLIPSIBLINGS, extended = 0; PCWSTR className = NULL;
		NativeStyle(&className,&style,&extended);
		// Scrollbars reduce the viewport rather than enlarging the stored client size.
		style &= ~(WS_HSCROLL | WS_VSCROLL);
		CheckIf(!AdjustForWindowDPI(rect,style,extended,m_hwnd), HrEnsureLastError());
		INT width = rect->right - rect->left, height = rect->bottom - rect->top;
		rect->left = x; rect->top = y; rect->right = x + width; rect->bottom = y + height;
	}
Cleanup:
	return hr;
}
HRESULT CWinControl::NativeCreated() { return S_OK; }
HRESULT CForm::NativeCreated()
{
	HRESULT hr = S_OK;
	Check(CScrollingContainer::NativeCreated()); Check(ApplyIcons()); Check(ApplyWindowStyle());
	if(m_autoDPI) { m_dpi = SystemDPI(m_hwnd); Check(RecreateFont()); Check(Arrange(&m_bounds,m_dpi)); Check(ApplyIcons()); }
Cleanup:
	return hr;
}
HRESULT CScrollingContainer::NativeCreated()
{
	HRESULT hr = S_OK;
	m_content = CreateWindowExW(WS_EX_CONTROLPARENT, ClassName, L"", WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
		0,0,MulDiv(m_bounds.Width,m_dpi,96),MulDiv(m_bounds.Height,m_dpi,96),m_hwnd,NULL,g_hInstance,&m_contentData);
	CheckIf(!m_content, HrEnsureLastError());
Cleanup:
	return hr;
}

HRESULT CWinControl::RegisterWindowClass()
{
	HRESULT hr = S_OK;
	WNDCLASSEXW cls;
	ZeroMemory(&cls, sizeof(cls)); cls.cbSize = sizeof(cls);
	if(!GetClassInfoExW(g_hInstance, ClassName, &cls))
	{
		cls.style = CS_DBLCLKS; cls.lpfnWndProc = WindowProc; cls.hInstance = g_hInstance;
		cls.hCursor = LoadCursor(NULL, IDC_ARROW); cls.lpszClassName = ClassName;
		if(!RegisterClassExW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) hr = HrEnsureLastError();
	}
	return hr;
}

HRESULT CWinControl::CreateNative()
{
	HRESULT hr;
	HWND parent = NULL;
	DWORD style = WS_CLIPSIBLINGS, extended = 0;
	PCWSTR className = ClassName;
	TStackRef<IQuadooChildWindowHost> host;
	RECT bounds;
	CRString caption;
	if(m_parent)
	{
		Check(m_parent->QueryInterface(__uuidof(IQuadooChildWindowHost), reinterpret_cast<PVOID*>(&host)));
		TStackRef<IQuadooViewportDock> dock;
		Check(QueryOptional(Control(),&dock));
		if(dock)
		{
			Check(m_parent->GetHandle(&parent));
		}
		else
		{
			Check(host->GetChildHostHandle(&parent));
		}
		CheckIf(!parent,QuadooForm::EDestroyed);
	}
	else { Check(m_context->GetHostHandle(&parent)); }
	Check(RecreateFont()); NativeStyle(&className,&style,&extended);
	BOOL ownClass = 0 == TStrCmpAssert(className,ClassName);
	if(ownClass) { Check(RegisterWindowClass()); }
	bounds.left = MulDiv(m_bounds.Left,m_dpi,96); bounds.top = MulDiv(m_bounds.Top,m_dpi,96);
	bounds.right = bounds.left + MulDiv(m_bounds.Width,m_dpi,96); bounds.bottom = bounds.top + MulDiv(m_bounds.Height,m_dpi,96);
	Check(AdjustNativeRect(&bounds));
	UINT text = FindProperty(L"Text"); if(text == static_cast<UINT>(m_values.Length())) text = FindProperty(L"Caption");
	if(text < static_cast<UINT>(m_values.Length())) { Check(m_values[text].value->GetString(&caption)); }
	m_hwnd = CreateWindowExW(extended,className,caption.c_wstr(),style,bounds.left,bounds.top,bounds.right-bounds.left,bounds.bottom-bounds.top,parent,NULL,g_hInstance,ownClass ? &m_windowData : NULL);
	CheckIf(!m_hwnd,HrEnsureLastError());
	if(!ownClass)
	{
		CheckIf(!SetPropW(m_hwnd,WindowProperty,&m_windowData),HrEnsureLastError()); AddRef(); SetLastError(0);
		m_windowData.original = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(m_hwnd,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(WindowProc)));
		if(!m_windowData.original) { hr = HrEnsureLastError(); RemovePropW(m_hwnd,WindowProperty); Release(); goto Cleanup; }
	}
	SendMessageW(m_hwnd,WM_SETFONT,reinterpret_cast<WPARAM>(m_font),FALSE); EnableWindow(m_hwnd,Boolean(L"Enabled"));
	Check(NativeCreated());
Cleanup:
	return hr;
}

// Stage a complete standard-control HWND before retiring the old one. Item/style
// updates can then fail without losing the old native content or registration.
HRESULT CWinControl::ReplaceNative()
{
	HRESULT hr;
	HWND previous = m_hwnd, candidate = NULL;
	WindowData previousData = m_windowData;
	BOOL entered = FALSE, visible = FALSE, focus = FALSE;
	HWND after = NULL;
	CheckIf(!previous || m_replacing, QuadooForm::EInvalidState);
	visible = (GetWindowLongW(previous, GWL_STYLE) & WS_VISIBLE) != 0;
	focus = GetFocus() == previous || IsChild(previous, GetFocus());
	after = GetWindow(previous, GW_HWNDPREV);
	CheckIf(!SetPropW(previous, WindowProperty, &previousData), HrEnsureLastError());
	m_replacing = TRUE; entered = TRUE; m_hwnd = NULL; m_windowData.original = NULL;
	hr = CreateNative(); candidate = m_hwnd; Check(hr);
	Check(ArrangeNative(&m_effective, TRUE));
	CheckIf(!SetWindowPos(candidate, after ? after : HWND_TOP, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE), HrEnsureLastError());
	CheckIf(!DestroyWindow(previous), HrEnsureLastError()); previous = NULL;
	if(visible) ::ShowWindow(candidate, SW_SHOWNA);
	if(focus) ::SetFocus(candidate);
	hr = S_OK;
Cleanup:
	if(entered)
	{
		if(previous)
		{
			if(candidate) DestroyWindow(candidate);
			m_hwnd = previous; m_windowData = previousData;
			if(!SetPropW(previous, WindowProperty, &m_windowData) && SUCCEEDED(hr)) hr = HrEnsureLastError();
			SendMessageW(previous, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), FALSE);
			if(focus) ::SetFocus(previous);
		}
		else m_hwnd = candidate;
		m_replacing = FALSE;
	}
	return hr;
}

HRESULT CControl::RecreateFont()
{
	HRESULT hr;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONValue> flag, fontValue;
	CRString name;
	INT size = 9;
	bool bold = false, italic = false;
	LOGFONTW font;
	HFONT replacement = NULL;
	ZeroMemory(&font, sizeof(font));
	Check(StoredValue(L"Font", &fontValue));
	if(fontValue)
	{
		Check(fontValue->GetObject(&definition));
		Check(ReadString(definition, L"Name", L"Segoe UI", &name)); Check(ReadInt(definition, L"Size", 9, &size));
		if(definition->HasField(RSTRING_CAST(L"Bold"))) { Check(definition->FindNonNullValueW(L"Bold", &flag)); Check(flag->GetBoolean(&bold)); flag.Release(); }
		if(definition->HasField(RSTRING_CAST(L"Italic"))) { Check(definition->FindNonNullValueW(L"Italic", &flag)); Check(flag->GetBoolean(&italic)); }
	}
	else { Check(RStrCreateW(8, L"Segoe UI", &name)); }
	font.lfHeight = -MulDiv(size, m_dpi, 72); font.lfWeight = bold ? FW_BOLD : FW_NORMAL;
	font.lfItalic = italic; font.lfCharSet = DEFAULT_CHARSET; font.lfQuality = CLEARTYPE_QUALITY;
	Check(TStrCchCpy(font.lfFaceName, ARRAYSIZE(font.lfFaceName), name.c_wstr()));
	replacement = CreateFontIndirectW(&font); CheckIf(!replacement, HrEnsureLastError());
	if(m_font) DeleteObject(m_font); m_font = replacement;
	hr = S_OK;
Cleanup:
	return hr;
}

HRESULT CControl::ApplyNative(PCWSTR name)
{
	HRESULT hr = S_OK;
	if(0 == TStrCmpAssert(name,L"Font")) { Check(RecreateFont()); }
	if(m_parent)
	{
		TStackRef<IQuadooChildWindowHost> host; HWND hwnd;
		if(SUCCEEDED(m_parent->QueryInterface(__uuidof(IQuadooChildWindowHost),reinterpret_cast<PVOID*>(&host))) && SUCCEEDED(host->GetChildHostHandle(&hwnd))) InvalidateRect(hwnd,NULL,TRUE);
	}
Cleanup:
	return hr;
}
HRESULT CWinControl::ApplyNative(PCWSTR name)
{
	HRESULT hr;
	Check(CControl::ApplyNative(name));
	if(m_hwnd)
	{
		if(0 == TStrCmpAssert(name,L"Font")) SendMessageW(m_hwnd,WM_SETFONT,reinterpret_cast<WPARAM>(m_font),TRUE);
		if(0 == TStrCmpAssert(name,L"Caption") || 0 == TStrCmpAssert(name,L"Text"))
		{ CRString text; Check(StoredString(name, &text)); CheckIf(!SetWindowTextW(m_hwnd,text.c_wstr()),HrEnsureLastError()); }
		else if(0 == TStrCmpAssert(name,L"Visible") && m_state == QuadooForm::Ready) ::ShowWindow(m_hwnd,Boolean(L"Visible") ? SW_SHOWNA : SW_HIDE);
		else if(0 == TStrCmpAssert(name,L"Enabled")) EnableWindow(m_hwnd,Boolean(L"Enabled"));
		if(0 == TStrCmpAssert(name,L"ForeColor") || 0 == TStrCmpAssert(name,L"BackColor")) InvalidateRect(m_hwnd,NULL,TRUE);
	}
Cleanup:
	return hr;
}
HRESULT CLabel::ApplyNative(PCWSTR name)
{
	HRESULT hr;
	Check(CControl::ApplyNative(name));
	if(0 == TStrCmpAssert(name, L"Visible") && m_state == QuadooForm::Ready) { Check(Dispatch(Boolean(L"Visible") ? L"OnShow" : L"OnHide")); }
Cleanup:
	return hr;
}
HRESULT CTextEdit::ApplyNative(PCWSTR name)
{
	HRESULT hr;
	if(0 == TStrCmpAssert(name,L"Text"))
	{
		if(m_nativeTextChange) { m_nativeTextChange = FALSE; hr = S_OK; }
		else { CRString text; Check(StoredString(L"Text",&text)); Check(WriteNativeText(text)); }
	}
	else hr = CWinControl::ApplyNative(name);
Cleanup:
	return hr;
}
HRESULT CTextBox::ApplyNative(PCWSTR name)
{
	HRESULT hr = S_OK;
	if(!TStrCmpAssert(name,L"Password"))
	{
		if(m_hwnd)
		{
			// This message also sets/clears ES_PASSWORD without replacing the HWND.
			SendMessageW(m_hwnd,EM_SETPASSWORDCHAR,Boolean(L"Password") ? L'*' : 0,0);
			InvalidateRect(m_hwnd,NULL,FALSE);
		}
	}
	else
	{
		Check(CTextEdit::ApplyNative(name));
	}
Cleanup:
	return hr;
}
HRESULT CTextEdit::WriteNativeText(RSTRING text)
{
	HRESULT hr = S_OK;
	if(m_hwnd)
	{
		BOOL previous = m_syncText; m_syncText = TRUE;
		BOOL set = SetWindowTextW(m_hwnd,RStrToWide(text)); m_syncText = previous;
		CheckIf(!set,HrEnsureLastError());
	}
Cleanup:
	return hr;
}
HRESULT CTextEdit::PropertyChanged(PCWSTR name)
{
	return 0 == TStrCmpAssert(name,L"Text") && m_state == QuadooForm::Ready ? Dispatch(L"OnChange") : S_OK;
}
HRESULT CWinControl::DestroyNative() { return m_hwnd && !DestroyWindow(m_hwnd) ? HrEnsureLastError() : S_OK; }
HRESULT CScrollingContainer::DestroyNative()
{
	HRESULT hr = S_OK;
	if(m_content && !DestroyWindow(m_content)) hr = HrEnsureLastError();
	HRESULT native = CWinControl::DestroyNative(); if(SUCCEEDED(hr)) hr = native;
	return hr;
}

STDMETHODIMP CWinControl::GetHandle(HWND* hwnd)
{
	HRESULT hr;
	CheckIf(!hwnd, E_POINTER); *hwnd = NULL; Check(CheckAlive()); CheckIf(!m_hwnd, QuadooForm::EDestroyed);
	*hwnd = m_hwnd;
Cleanup:
	return hr;
}

STDMETHODIMP CWinControl::SetFocus()
{
	HRESULT hr;
	Check(CheckAlive()); CheckIf(!m_hwnd || !Boolean(L"Enabled") || !Boolean(L"Visible"), QuadooForm::EInvalidState);
	::SetFocus(m_hwnd); hr = ::GetFocus() == m_hwnd || IsChild(m_hwnd, ::GetFocus()) ? S_OK : E_FAIL;
Cleanup:
	return hr;
}

STDMETHODIMP CContainer::GetChildHostHandle(HWND* hwnd) { return GetHandle(hwnd); }
STDMETHODIMP CScrollingContainer::GetChildHostHandle(HWND* hwnd)
{
	HRESULT hr;
	CheckIf(!hwnd,E_POINTER); *hwnd = NULL; Check(CheckAlive()); CheckIf(!m_content,QuadooForm::EDestroyed); *hwnd = m_content;
Cleanup:
	return hr;
}

STDMETHODIMP CForm::ShowForm(INT command)
{
	HRESULT hr;
	TStackRef<IJSONValue> visible;
	BOOL changed;
	Check(CheckAlive()); CheckIf(m_state != QuadooForm::Ready, QuadooForm::EInvalidState);
	QuadooForm::ContextMode mode; Check(m_context->GetMode(&mode));
	if(command == SW_SHOW && mode != QuadooForm::Design)
	{ if(Integer(L"WindowState") == QuadooForm::WindowMinimized) command = SW_SHOWMINIMIZED; else if(Integer(L"WindowState") == QuadooForm::WindowMaximized) command = SW_SHOWMAXIMIZED; }
	changed = Boolean(L"Visible") != (command != SW_HIDE);
	Check(JSONCreateBool(command != SW_HIDE, &visible)); Check(SetStoredValue(L"Visible", visible));
	::ShowWindow(m_hwnd, command); Check(CheckAlive());
	if(changed) { Check(QueueChange(L"Visible")); }
	hr = S_OK;
Cleanup:
	return hr;
}

STDMETHODIMP CForm::HideForm() { return ShowForm(SW_HIDE); }

STDMETHODIMP CForm::ShowModal(LRESULT* result)
{
	HRESULT hr;
	TStackRef<CControl> keepAlive(this);
	TStackRef<IJSONValue> resultValue;
	BOOL restoreOwner = FALSE, quit = FALSE, entered = FALSE;
	INT quitCode = 0;
	MSG message;
	LONGLONG modalResult;
	HWND owner = m_owner;
	CheckIf(!result, E_POINTER); *result = 0;
	Check(CheckAlive()); CheckIf(m_state != QuadooForm::Ready || m_modal, QuadooForm::EInvalidState);
	m_modal = TRUE; entered = TRUE; m_callbackError = S_OK;
	if(owner && IsWindow(owner) && IsWindowEnabled(owner)) { EnableWindow(owner, FALSE); restoreOwner = TRUE; }
	Check(ShowForm(SW_SHOW));
	while(m_state < QuadooForm::Destroying)
	{
		INT received = GetMessageW(&message, NULL, 0, 0);
		if(received == 0) { quit = TRUE; quitCode = static_cast<INT>(message.wParam); hr = HRESULT_FROM_WIN32(ERROR_CANCELLED); Destroy(); goto Cleanup; }
		CheckIf(received == -1, HrEnsureLastError());
		BOOL processed; Check(PreprocessMessage(&message,&processed));
		if(!processed) { TranslateMessage(&message); DispatchMessageW(&message); }
		Check(m_callbackError);
	}
	Check(StoredValue(L"ModalResult", &resultValue)); Check(resultValue->GetLongInteger(&modalResult)); *result = static_cast<LRESULT>(modalResult);
Cleanup:
	if(entered && FAILED(hr) && m_hwnd) ::ShowWindow(m_hwnd, SW_HIDE);
	if(restoreOwner && IsWindow(owner)) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
	if(entered) m_modal = FALSE;
	if(quit) PostQuitMessage(quitCode);
	return hr;
}

STDMETHODIMP CForm::Close()
{
	HRESULT hr;
	BOOL cancelled = FALSE;
	BOOL entered = FALSE;
	TStackRef<CControl> keepAlive(this);
	Check(CheckThread()); CheckIfIgnore(m_state >= QuadooForm::Destroying || m_closing, S_FALSE);
	CheckIf(m_state != QuadooForm::Ready, QuadooForm::EInvalidState);
	m_closing = TRUE; entered = TRUE;
	Check(Dispatch(L"OnCloseQuery", &cancelled));
	CheckIfIgnore(m_state >= QuadooForm::Destroying || cancelled, S_FALSE);
	hr = Dispatch(L"OnClose");
	ReportCallback(hr, L"OnClose");
	{
		HRESULT destroyResult = Destroy(); if(SUCCEEDED(hr)) hr = destroyResult;
	}
Cleanup:
	if(entered) m_closing = FALSE;
	return hr;
}

VOID CWinControl::NativeDestroyed(BOOL content)
{
	if(m_replacing) return;
	m_hwnd = NULL;
	if(m_state < QuadooForm::Destroying) { HRESULT hr = Destroy(); if(FAILED(hr)) ReportCallback(hr,L"OnDestroy"); }
}
VOID CScrollingContainer::NativeDestroyed(BOOL content)
{
	if(!content) CWinControl::NativeDestroyed(FALSE);
	else { m_content = NULL; if(m_state < QuadooForm::Destroying) { HRESULT hr = Destroy(); if(FAILED(hr)) ReportCallback(hr,L"OnDestroy"); } }
}

VOID CControl::ReportCallback(HRESULT error, PCWSTR eventName)
{
	if(FAILED(error))
	{
		m_callbackError = error;
		if(m_context)
		{
			CRString detail;
			if(SUCCEEDED(RStrFormatW(&detail, L"%ls: callback %ls failed (0x%.8x)", m_name.c_wstr(), eventName, error))) m_context->ReportError(error, m_name, detail);
		}
		TStackRef<IQuadooControlContainer> current(m_parent);
		for(UINT depth = 0; current && depth < 256; depth++)
		{
			TStackRef<IUpdateParticipant> participant;
			TStackRef<IQuadooControlContainer> parent;
			if(SUCCEEDED(current->QueryInterface(__uuidof(IUpdateParticipant), reinterpret_cast<PVOID*>(&participant)))) participant->RecordCallbackError(error);
			if(FAILED(current->GetParent(&parent))) break;
			current = parent;
		}
	}
}

LRESULT CALLBACK CWinControl::WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	WindowData* data = reinterpret_cast<WindowData*>(GetPropW(hwnd, WindowProperty));
	if(!data && msg == WM_NCCREATE)
	{
		data = static_cast<WindowData*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
		if(data)
		{
			if(!SetPropW(hwnd, WindowProperty, data)) return FALSE;
			data->owner->AddRef();
			if(!data->content) data->owner->m_hwnd = hwnd;
		}
	}
	if(!data) return DefWindowProcW(hwnd, msg, wp, lp);
	CWinControl* control = data->owner;
	WNDPROC original = data->original;
	BOOL content = data->content, handled = FALSE;
	control->AddRef();
	if(!control->m_replacing && control->m_state == QuadooForm::Ready && (msg == WM_SETFOCUS || msg == WM_KILLFOCUS))
	{
		control->NotifyFocus(msg == WM_SETFOCUS ? hwnd : reinterpret_cast<HWND>(wp));
	}
	LRESULT result = control->Message(hwnd, msg, wp, lp, content, &handled);
	if(!handled && (msg == WM_NCDESTROY || IsWindow(hwnd))) result = original ? CallWindowProcW(original, hwnd, msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
	if(msg == WM_NCDESTROY)
	{
		if(control->m_entered && !control->m_replacing)
		{
			control->NotifyFocus(::GetFocus());
		}
		RemovePropW(hwnd, WindowProperty); control->NativeDestroyed(content); control->Release();
	}
	control->Release();
	return result;
}

LRESULT CWinControl::Message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled)
{
	if(msg == WM_CONTEXTMENU && m_state == QuadooForm::Ready)
	{
		QuadooForm::ContextMode mode;
		if(SUCCEEDED(m_context->GetMode(&mode)) && mode != QuadooForm::Design)
		{
			POINT screen = {static_cast<SHORT>(LOWORD(lp)),static_cast<SHORT>(HIWORD(lp))};
			ReportCallback(ShowControlPopup(Control(),screen,handled),L"PopupMenu");
			if(*handled)
			{
				return 0;
			}
		}
	}
	LRESULT result = 0;
	if(m_replacing) return 0;
	if((msg == WM_KEYDOWN || msg == WM_GETDLGCODE) && wp == VK_TAB && (GetKeyState(VK_CONTROL) & 0x8000))
	{
		TStackRef<IQuadooTabControl> tabs;
		TStackRef<IQuadooControlContainer> ancestor;
		QueryOptional(Control(),&tabs);
		if(!tabs && SUCCEEDED(GetParent(&ancestor)))
			for(UINT depth = 0; ancestor && depth < 256; depth++)
			{
				if(SUCCEEDED(ancestor->QueryInterface(__uuidof(IQuadooTabControl),reinterpret_cast<PVOID*>(&tabs)))) break;
				TStackRef<IQuadooControlContainer> next; if(FAILED(ancestor->GetParent(&next))) break; ancestor = next;
			}
		if(tabs)
		{
			if(msg == WM_GETDLGCODE) { *handled = TRUE; return DLGC_WANTTAB | DLGC_WANTMESSAGE; }
			if(!SameObject(tabs,Control()))
			{ HWND tabWindow; if(SUCCEEDED(tabs->GetHandle(&tabWindow))) { SendMessageW(tabWindow,msg,wp,lp); *handled = TRUE; return 0; } }
		}
	}
	switch(msg)
	{
	case WM_SYSCOLORCHANGE:
	case WM_THEMECHANGED:
	case WM_ENABLE:
		RedrawWindow(hwnd,NULL,NULL,RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
		break;
	case WM_SHOWWINDOW:
		if(!content && m_state == QuadooForm::Ready) ReportCallback(Dispatch(wp ? L"OnShow" : L"OnHide"), L"Visibility");
		break;
	case WM_SETFOCUS: case WM_KILLFOCUS:
		if(!content && m_state == QuadooForm::Ready) RequestKeyboardRefresh(Control());
		if(!content && m_state == QuadooForm::Ready) ReportCallback(Dispatch(msg == WM_SETFOCUS ? L"OnFocus" : L"OnBlur"), L"Focus");
		if(msg == WM_SETFOCUS && !content && m_state == QuadooForm::Ready)
		{
			TStackRef<IQuadooControlContainer> ancestor(m_parent);
			for(UINT depth = 0; ancestor && depth < 256; depth++)
			{
				TStackRef<IQuadooScrollable> scrolling; TStackRef<IQuadooControlContainer> next;
				if(SUCCEEDED(ancestor->QueryInterface(__uuidof(IQuadooScrollable),reinterpret_cast<PVOID*>(&scrolling))))
				{
					HWND viewport; RECT child, client; QuadooForm::Point offset;
					if(SUCCEEDED(ancestor->GetHandle(&viewport)) && GetWindowRect(hwnd,&child) && GetClientRect(viewport,&client) && SUCCEEDED(scrolling->GetScrollPosition(&offset)))
					{
						MapWindowPoints(NULL,viewport,reinterpret_cast<POINT*>(&child),2); UINT dpi = ControlDPI(ancestor);
						INT x = child.left < 0 ? child.left : child.right > client.right ? child.right - client.right : 0;
						INT y = child.top < 0 ? child.top : child.bottom > client.bottom ? child.bottom - client.bottom : 0;
						if(x || y) { offset.X = max(0,offset.X + MulDiv(x,96,dpi)); offset.Y = max(0,offset.Y + MulDiv(y,96,dpi)); scrolling->SetScrollPosition(&offset); }
					}
				}
				if(FAILED(ancestor->GetParent(&next))) break; ancestor = next;
			}
		}
		break;
	case WM_KEYDOWN: case WM_KEYUP: case WM_CHAR:
		if(!content && m_state == QuadooForm::Ready)
		{
			BOOL cancelled; ReportCallback(Dispatch(msg == WM_CHAR ? L"OnKeyPress" : msg == WM_KEYDOWN ? L"OnKeyDown" : L"OnKeyUp", &cancelled, wp), L"Keyboard");
			*handled = cancelled || m_state >= QuadooForm::Destroying;
			if(*handled && msg == WM_KEYDOWN) NoteHandledKey(Control(),hwnd,lp);
		}
		break;
	case WM_MOUSEWHEEL:
		if(m_parent)
		{
			HWND parent;
			if(SUCCEEDED(m_parent->GetHandle(&parent)) && parent)
			{
				SendMessageW(parent,msg,wp,lp);
				*handled = TRUE;
			}
		}
		break;
	}
	return *handled ? result : 0;
}

LRESULT CForm::Message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled)
{
	LRESULT result = 0;
	if(!content && msg == WM_SIZE && m_menuAdjusting)
	{
		*handled = TRUE;
		return 0;
	}
	if(!content && msg == WM_MENUCOMMAND && m_menu && m_menu->Owns(reinterpret_cast<HMENU>(lp)))
	{
		*handled = TRUE;
		TStackRef<CNativeMenu> active(m_menu);
		ReportCallback(active->InvokePosition(reinterpret_cast<HMENU>(lp),static_cast<UINT>(wp)),L"Menu OnClick");
		return 0;
	}
	if(!content && msg == WM_COMMAND && !lp && HIWORD(wp) == 1 && m_menu)
	{
		*handled = TRUE;
		ReportCallback(m_menu->Invoke(LOWORD(wp)),L"Menu shortcut");
		return 0;
	}
	if(!content && msg == WM_INITMENUPOPUP && m_menu && m_menu->Owns(reinterpret_cast<HMENU>(wp)))
	{
		TStackRef<CNativeMenu> active(m_menu);
		ReportCallback(active->Opening(reinterpret_cast<HMENU>(wp)),L"Menu opening");
		*handled = TRUE;
		return 0;
	}
	if(!content && msg == WM_ENTERMENULOOP)
	{
		m_menuLoops++;
	}
	if(!content && msg == WM_EXITMENULOOP)
	{
		if(m_menuLoops)
		{
			m_menuLoops--;
		}
		if(!m_menuLoops && m_menuPending)
		{
			PostMessageW(hwnd,MenuRefreshMessage,0,0);
		}
	}
	if(!content && msg == MenuRefreshMessage && m_state == QuadooForm::Ready)
	{
		*handled = TRUE;
		ReportCallback(RefreshMenu(),L"Menu update");
		return 0;
	}
	switch(msg)
	{
	case KeyboardRefreshMessage:
		if(!content && m_state == QuadooForm::Ready) { *handled = TRUE; ReportCallback(UpdateDefaultButton(),L"Default button appearance"); }
		break;
	case WM_CLOSE:
		if(!content) { *handled = TRUE; ReportCallback(Close(), L"OnCloseQuery"); }
		break;
	case WM_SYSCOMMAND:
		if((wp & 0xfff0) == SC_CLOSE && !(Integer(L"BorderIcons") & QuadooForm::BorderClose)) { *handled = TRUE; return 0; }
		break;
	case WM_GETMINMAXINFO:
		if(!content && (Integer(L"MinWidth") || Integer(L"MinHeight") || Integer(L"MaxWidth") || Integer(L"MaxHeight")))
		{
			DefWindowProcW(hwnd, msg, wp, lp);
			MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(lp);
			RECT rect = { 0, 0, 0, 0 };
			AdjustForWindowDPI(&rect, static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE)), static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE)), hwnd);
			if(GetMenu(hwnd))
			{
				RECT outer;
				RECT client;
				if(GetWindowRect(hwnd,&outer) && GetClientRect(hwnd,&client))
				{
					// Use the current native non-client extent, including wrapped rows.
					rect.bottom = outer.bottom - outer.top - client.bottom + rect.top;
				}
			}
			if(Integer(L"MinWidth")) info->ptMinTrackSize.x = MulDiv(Integer(L"MinWidth"), m_dpi, 96) + rect.right - rect.left;
			if(Integer(L"MinHeight")) info->ptMinTrackSize.y = MulDiv(Integer(L"MinHeight"), m_dpi, 96) + rect.bottom - rect.top;
			if(Integer(L"MaxWidth")) info->ptMaxTrackSize.x = MulDiv(Integer(L"MaxWidth"), m_dpi, 96) + rect.right - rect.left;
			if(Integer(L"MaxHeight")) info->ptMaxTrackSize.y = MulDiv(Integer(L"MaxHeight"), m_dpi, 96) + rect.bottom - rect.top;
			*handled = TRUE;
		}
		break;
	case DpiChanged:
		if(!content && m_state == QuadooForm::Ready)
		{
			RECT* bounds = reinterpret_cast<RECT*>(lp); m_dpi = LOWORD(wp);
			ReportCallback(RecreateFont(), L"DPI"); SendMessageW(m_hwnd,WM_SETFONT,reinterpret_cast<WPARAM>(m_font),TRUE);
			ReportCallback(ApplyIcons(), L"DPI icon");
			SetWindowPos(hwnd, NULL, bounds->left, bounds->top, bounds->right - bounds->left, bounds->bottom - bounds->top, SWP_NOZORDER | SWP_NOACTIVATE);
			ReportCallback(UpdateLayout(), L"DPI"); *handled = TRUE;
		}
		break;
	case WM_SIZE:
		if(!content && m_state >= QuadooForm::Binding && m_state < QuadooForm::Destroying)
		{
			RECT client; GetClientRect(hwnd,&client); INT width = MulDiv(client.right,96,m_dpi), height = MulDiv(client.bottom,96,m_dpi);
			if(width != m_effective.Width || height != m_effective.Height) { m_effective.Width = width; m_effective.Height = height; m_boundsDirty = TRUE; }
		}
		break;
	}
	return *handled ? result : CScrollingContainer::Message(hwnd,msg,wp,lp,content,handled);
}

LRESULT CContainer::Message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled)
{
	LRESULT result = 0;
	if(!content && m_context)
	{
		UINT callbackMessage = QuadooForm::ContainerCallbackMessage();
		if(msg == WM_NCDESTROY || (callbackMessage && msg == callbackMessage))
		{
			// The binder consumes its own errors as diagnostics. Feeding them to
			// ReportCallback would incorrectly make deferred calls fatal/modal.
			QuadooForm::ForwardContainerMessage(m_context,hwnd,msg,wp,lp,handled,&result);
			if(*handled)
			{
				return result;
			}
		}
	}
	switch(msg)
	{
	case WM_INITMENUPOPUP:
	case WM_TIMER:
		if(!content && m_state == QuadooForm::Ready)
			ReportCallback(DispatchComponentMessage(Container(),hwnd,msg,wp,lp,handled,&result),L"Component timer");
		break;
	case WM_SIZE:
		if(!content && m_state >= QuadooForm::Binding && m_state < QuadooForm::Destroying)
		{
			ReportCallback(UpdateLayout(), L"Layout");
		}
		break;
	case WM_COMMAND:
	case WM_CTLCOLORBTN: case WM_CTLCOLOREDIT: case WM_CTLCOLORSTATIC: case WM_CTLCOLORLISTBOX:
		if(lp)
		{
			for(sysint n = 0; n < m_children.Length(); n++)
			{
				TStackRef<IQuadooControl> child(m_children[n].control);
				TStackRef<IQuadooWinControl> window; HWND childWindow;
				if(FAILED(child->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&window))) || FAILED(window->GetHandle(&childWindow)) || childWindow != reinterpret_cast<HWND>(lp)) continue;
				TStackRef<IQuadooMessageHandler> handler;
				if(SUCCEEDED(child->QueryInterface(__uuidof(IQuadooMessageHandler),reinterpret_cast<PVOID*>(&handler)))) ReportCallback(handler->HandleMessage(childWindow,msg,wp,lp,handled,&result),L"Native message");
				if(!*handled && msg != WM_COMMAND)
				{
					HDC dc = reinterpret_cast<HDC>(wp); COLORREF back = ResolveQuadooColor(LayoutInt(child,L"BackColor",static_cast<INT>(0x80000000 | COLOR_BTNFACE)));
					SetTextColor(dc,ResolveQuadooColor(LayoutInt(child,L"ForeColor",static_cast<INT>(0x80000000 | COLOR_WINDOWTEXT)))); SetBkColor(dc,back); SetDCBrushColor(dc,back);
					result = reinterpret_cast<LRESULT>(GetStockObject(DC_BRUSH)); *handled = TRUE;
				}
				break;
			}
		}
		break;
	case WM_NOTIFY:
		if(lp)
		{
			NMHDR* notification = reinterpret_cast<NMHDR*>(lp);
			for(sysint n = 0; n < m_children.Length(); n++)
			{
				TStackRef<IQuadooControl> child(m_children[n].control);
				TStackRef<IQuadooWinControl> window; HWND childWindow;
				if(FAILED(child->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&window))) || FAILED(window->GetHandle(&childWindow)) || childWindow != notification->hwndFrom)
					continue;

				TStackRef<IQuadooMessageHandler> handler;
				if(SUCCEEDED(child->QueryInterface(__uuidof(IQuadooMessageHandler),reinterpret_cast<PVOID*>(&handler))))
					ReportCallback(handler->HandleMessage(childWindow,msg,wp,lp,handled,&result),L"Native notification");
				break;
			}
		}
		break;
	case WM_DRAWITEM:
		if(lp)
		{
			ReportCallback(ForwardDrawItem(Container(),reinterpret_cast<const DRAWITEMSTRUCT*>(lp),wp,handled,&result),L"Native drawing");
		}
		break;
	case WM_ERASEBKGND:
		*handled = TRUE;
		result = 1;
		break;
	case WM_PAINT:
	case WM_PRINTCLIENT:
		{
			PAINTSTRUCT paint; HDC dc = msg == WM_PAINT ? BeginPaint(hwnd, &paint) : reinterpret_cast<HDC>(wp); RECT client; GetClientRect(hwnd, &client);
			PaintBackground(dc, &client);
			if(PaintChildren(content))
			{
				for(sysint n = 0; n < m_children.Length(); n++)
				{
					TStackRef<IQuadooStaticControl> srControl;

					if(SUCCEEDED(m_children[n].control->QueryInterface(__uuidof(IQuadooStaticControl), reinterpret_cast<PVOID*>(&srControl))))
					{
						BOOL visible;
						QuadooForm::Rect bounds;

						if(SUCCEEDED(srControl->GetVisible(&visible)) && visible &&
							SUCCEEDED(srControl->GetEffectiveBounds(&bounds)))
						{
							RECT physical = { MulDiv(bounds.Left, m_dpi, 96), MulDiv(bounds.Top, m_dpi, 96), MulDiv(bounds.Left + bounds.Width, m_dpi, 96), MulDiv(bounds.Top + bounds.Height, m_dpi, 96) };
							srControl->Paint(dc, &physical);
						}
					}
				}
			}
			if(msg == WM_PAINT)
				EndPaint(hwnd, &paint);
			*handled = TRUE;
		}
		break;
	case WM_CONTEXTMENU:
		if(PaintChildren(content) && m_state == QuadooForm::Ready && lp != -1)
		{
			QuadooForm::ContextMode mode;
			if(SUCCEEDED(m_context->GetMode(&mode)) && mode != QuadooForm::Design)
			{
				POINT screen = {static_cast<SHORT>(LOWORD(lp)),static_cast<SHORT>(HIWORD(lp))};
				POINT local = screen;
				ScreenToClient(hwnd,&local);
				for(sysint n = m_children.Length(); n > 0; n--)
				{
					TStackRef<IQuadooStaticControl> child;
					if(SUCCEEDED(m_children[n - 1].control->QueryInterface(__uuidof(IQuadooStaticControl),reinterpret_cast<PVOID*>(&child))))
					{
						BOOL hit;
						if(SUCCEEDED(child->HitTest(MulDiv(local.x,96,m_dpi),MulDiv(local.y,96,m_dpi),&hit)) && hit)
						{
							ReportCallback(ShowControlPopup(child,screen,handled),L"PopupMenu");
							break;
						}
					}
				}
			}
		}
		break;
	case WM_LBUTTONUP:
		if(PaintChildren(content) && m_state == QuadooForm::Ready)
			for(sysint n = m_children.Length(); n > 0; n--)
			{
				TStackRef<IQuadooStaticControl> label;
				if(SUCCEEDED(m_children[n - 1].control->QueryInterface(__uuidof(IQuadooStaticControl), reinterpret_cast<PVOID*>(&label))))
				{
					BOOL hit; if(SUCCEEDED(label->HitTest(MulDiv(static_cast<short>(LOWORD(lp)), 96, m_dpi), MulDiv(static_cast<short>(HIWORD(lp)), 96, m_dpi), &hit)) && hit)
					{
						TStackRef<IQuadooMessageHandler> handler;
						if(SUCCEEDED(label->QueryInterface(__uuidof(IQuadooMessageHandler),reinterpret_cast<PVOID*>(&handler)))) ReportCallback(handler->HandleMessage(hwnd,msg,wp,lp,handled,&result),L"OnClick");
						break;
					}
				}
			}
		break;
	}
	return *handled ? result : CWinControl::Message(hwnd,msg,wp,lp,content,handled);
}

BOOL CContainer::PaintChildren(BOOL content) const { return TRUE; }
HRESULT CContainer::ContentInsets(QuadooForm::Insets* insets) { return Insets(L"Padding", insets); }
VOID CContainer::PaintBackground(HDC dc, const RECT* bounds)
{
	SetDCBrushColor(dc, ResolveQuadooColor(Integer(L"BackColor"))); FillRect(dc, bounds, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
}
BOOL CScrollingContainer::PaintChildren(BOOL content) const { return content; }

LRESULT CScrollingContainer::Message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled)
{
	LRESULT result = 0;
	switch(msg)
	{
	case WM_HSCROLL: case WM_VSCROLL:
		if(!content && !lp)
		{
			SCROLLINFO info = { sizeof(info), SIF_ALL };
			INT bar = msg == WM_HSCROLL ? SB_HORZ : SB_VERT;
			GetScrollInfo(hwnd, bar, &info);
			QuadooForm::Point position = m_scroll; INT& offset = bar == SB_HORZ ? position.X : position.Y;
			switch(LOWORD(wp))
			{
			case SB_LINEUP: offset -= 16; break; case SB_LINEDOWN: offset += 16; break;
			case SB_PAGEUP: offset -= info.nPage; break; case SB_PAGEDOWN: offset += info.nPage; break;
			case SB_THUMBPOSITION: case SB_THUMBTRACK: offset = info.nTrackPos; break;
			case SB_TOP: offset = 0; break; case SB_BOTTOM: offset = info.nMax; break;
			}
			position.X = max(0,position.X); position.Y = max(0,position.Y);
			ReportCallback(SetScrollPosition(&position), L"Scroll"); *handled = TRUE;
		}
		break;
	case WM_MOUSEWHEEL:
		if(Boolean(L"AutoScroll"))
		{
			QuadooForm::Point position = m_scroll;
			position.Y -= MulDiv(static_cast<short>(HIWORD(wp)), 48, WHEEL_DELTA);
			position.X = max(0,position.X); position.Y = max(0,position.Y);
			ReportCallback(SetScrollPosition(&position), L"Scroll"); *handled = TRUE;
		}
		break;
	}
	return *handled ? result : CContainer::Message(hwnd,msg,wp,lp,content,handled);
}

STDMETHODIMP CLabel::Paint(HDC dc, const RECT* bounds)
{
	HRESULT hr;
	CRString text;
	RECT rect;
	INT saved = 0;
	Check(CheckAlive()); CheckIf(!dc || !bounds, E_INVALIDARG);
	Check(StoredString(L"Caption", &text));
	saved = SaveDC(dc); CheckIf(!saved, HrEnsureLastError());
	SelectObject(dc, m_font); SetTextColor(dc, ResolveQuadooColor(Integer(L"ForeColor"))); SetBkMode(dc, TRANSPARENT);
	rect = *bounds; DrawTextW(dc, text.c_wstr(), RStrLen(text), &rect, DT_LEFT | DT_TOP | DT_NOPREFIX);
	RestoreDC(dc, saved); hr = S_OK;
Cleanup:
	return hr;
}

STDMETHODIMP CLabel::HitTest(INT x, INT y, BOOL* hit)
{
	HRESULT hr;
	CheckIf(!hit, E_POINTER); *hit = FALSE; Check(CheckAlive());
	*hit = Boolean(L"Visible") && Boolean(L"Enabled") && x >= m_effective.Left && y >= m_effective.Top &&
		x < m_effective.Left + m_effective.Width && y < m_effective.Top + m_effective.Height;
Cleanup:
	return hr;
}

HRESULT CWinControl::Command(UINT notification) { return S_FALSE; }
HRESULT CButton::Command(UINT notification)
{
	HRESULT hr;
	TStackRef<CControl> keepAlive(this); TStackRef<IJSONValue> value;
	TStackRef<IQuadooControlContainer> ancestor;
	LONGLONG result;
	CheckIfIgnore(notification != BN_CLICKED || m_state != QuadooForm::Ready,S_FALSE);
	Check(Dispatch(L"OnClick")); CheckIfIgnore(m_state >= QuadooForm::Destroying,S_OK);
	Check(StoredValue(L"ModalResult",&value)); Check(value->GetLongInteger(&result)); CheckIfIgnore(!result,S_OK);
	Check(GetParent(&ancestor));
	for(UINT depth = 0; ancestor && depth < 256; depth++)
	{
		TStackRef<IQuadooModalDialog> modal; TStackRef<IQuadooForm> form; TStackRef<IQuadooControlContainer> next;
		if(SUCCEEDED(ancestor->QueryInterface(__uuidof(IQuadooForm),reinterpret_cast<PVOID*>(&form))))
		{
			hr = ancestor->QueryInterface(__uuidof(IQuadooModalDialog),reinterpret_cast<PVOID*>(&modal));
			if(hr == E_NOINTERFACE) hr = S_OK; else if(SUCCEEDED(hr)) hr = modal->CompleteModal(static_cast<LRESULT>(result));
			goto Cleanup;
		}
		Check(ancestor->GetParent(&next)); ancestor = next;
	}
	hr = S_OK;
Cleanup:
	return hr;
}
HRESULT CTextEdit::Command(UINT notification)
{
	HRESULT hr = S_FALSE;
	if(notification == EN_CHANGE && !m_syncText && m_state == QuadooForm::Ready)
	{
		INT length = GetWindowTextLengthW(m_hwnd); CRString text; PWSTR write; TStackRef<IJSONValue> value;
		Check(RStrAllocW(length,&text,&write)); GetWindowTextW(m_hwnd,write,length+1); Check(JSONCreateString(text,&value));
		// Skip only the write that publishes this native edit. OnChange callbacks
		// may assign Text again; those writes and nested native edits must work.
		m_nativeTextChange = TRUE; hr = SetNamedValue(L"Text",QuadooProperty::String,value); m_nativeTextChange = FALSE;
	}
Cleanup:
	return hr;
}
STDMETHODIMP CWinControl::HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result)
{
	HRESULT hr = S_OK;
	CheckIf(!handled || !result,E_POINTER); *handled = FALSE; *result = 0; Check(CheckAlive());
	if(hwnd == m_hwnd && msg == WM_COMMAND) { hr = Command(HIWORD(wp)); if(hr != S_FALSE) *handled = TRUE; }
Cleanup:
	return hr;
}
STDMETHODIMP CLabel::HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result)
{
	HRESULT hr = S_OK;
	CheckIf(!handled || !result,E_POINTER); *handled = FALSE; *result = 0; Check(CheckAlive());
	if(msg == WM_LBUTTONUP && m_state == QuadooForm::Ready) { *handled = TRUE; Check(Dispatch(L"OnClick")); }
Cleanup:
	return hr;
}
}
