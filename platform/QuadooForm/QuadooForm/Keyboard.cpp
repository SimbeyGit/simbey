#include "Runtime.h"
#include "MenuRuntime.h"
#include <imm.h>
#pragma comment(lib,"imm32.lib")

namespace QF
{
static BOOL KeyFlag(IQuadooControl* control, PCWSTR name)
{
	TStackRef<IJSONValue> value; bool flag = false;
	if(SUCCEEDED(LayoutProperty(control,name,&value)) && value) value->GetBoolean(&flag);
	return flag;
}
static BOOL KeyTargetReady(IQuadooControl* control, HWND window)
{
	QuadooForm::ComponentState state;
	return SUCCEEDED(control->GetState(&state)) && state == QuadooForm::Ready && IsWindow(window);
}
static HRESULT OwningForm(IQuadooControl* source, IQuadooForm** result)
{
	HRESULT hr = S_OK; TStackRef<IQuadooControl> current(source);
	*result = NULL;
	for(UINT depth = 0; current && depth < 128; depth++)
	{
		TStackRef<IQuadooControlContainer> parent;
		Check(QueryOptional(current,result)); if(*result) goto Cleanup;
		Check(current->GetParent(&parent)); current = parent;
	}
Cleanup:
	return hr;
}
VOID RequestKeyboardRefresh(IQuadooControl* control)
{
	TStackRef<IQuadooForm> form; HWND window;
	if(SUCCEEDED(OwningForm(control,&form)) && form && SUCCEEDED(form->GetHandle(&window))) PostMessageW(window,KeyboardRefreshMessage,0,0);
}
VOID NoteHandledKey(IQuadooControl* control, HWND target, LPARAM flags)
{
	TStackRef<IQuadooForm> form; TStackRef<IKeyboardState> state;
	if(SUCCEEDED(OwningForm(control,&form)) && form && SUCCEEDED(QueryOptional(form,&state)) && state) state->NoteHandled(target,(flags >> 16) & 0xff);
}
class CKeyboardTree
{
public:
	struct Node { IQuadooControl* control; INT parent; HWND window; BOOL eligible; };
	TArray<Node> nodes;
	~CKeyboardTree() { for(sysint n = 0; n < nodes.Length(); n++) nodes[n].control->Release(); }
	HRESULT Read(IQuadooControl* control, INT parent = -1, UINT depth = 0)
	{
		HRESULT hr; Node node; QuadooForm::ComponentState state; BOOL visible, enabled;
		TStackRef<IQuadooWinControl> native; TStackRef<IQuadooControlContainer> container;
		TArray<IQuadooControl*> children;
		CheckIf(depth >= 128 || nodes.Length() >= 4096,QuadooForm::EInvalidDefinition);
		node.control = control; node.parent = parent; node.window = NULL; node.eligible = FALSE;
		Check(control->GetState(&state)); Check(control->GetVisible(&visible)); Check(control->GetEnabled(&enabled));
		Check(QueryOptional(control,&native)); if(native) { Check(native->GetHandle(&node.window)); }
		node.eligible = state == QuadooForm::Ready && visible && enabled && (parent < 0 || nodes[parent].eligible);
		if(node.window) node.eligible = node.eligible && IsWindowVisible(node.window) && IsWindowEnabled(node.window);
		INT position = static_cast<INT>(nodes.Length()); Check(nodes.Append(node)); control->AddRef();
		Check(QueryOptional(control,&container));
		if(container)
		{
			UINT count; Check(container->GetChildCount(&count)); CheckIf(count > 4096,QuadooForm::EInvalidDefinition);
			for(UINT n = 0; n < count; n++)
			{
				TStackRef<IQuadooControl> child; Check(container->GetChild(n,&child)); CheckIf(!child,E_UNEXPECTED);
				INT order = LayoutInt(child,L"TabOrder",0); sysint at = children.Length();
				for(sysint c = 0; c < children.Length(); c++) if(order < LayoutInt(children[c],L"TabOrder",0)) { at = c; break; }
				Check(children.InsertAt(child,at)); child.Detach();
			}
			for(sysint n = 0; n < children.Length(); n++) { Check(Read(children[n],position,depth + 1)); }
		}
Cleanup:
		for(sysint n = 0; n < children.Length(); n++) children[n]->Release();
		return hr;
	}
	INT Target(HWND window)
	{
		for(sysint n = nodes.Length(); n > 0; n--) if(nodes[n - 1].window && (nodes[n - 1].window == window || IsChild(nodes[n - 1].window,window))) return static_cast<INT>(n - 1);
		return -1;
	}
	BOOL InScope(INT index, INT scope)
	{
		for(INT n = index; n >= 0; n = nodes[n].parent) if(n == scope) return TRUE;
		return FALSE;
	}
	HRESULT Choose(INT focus, BOOL cancel, IQuadooControl** result, BOOL focusedButton = TRUE)
	{
		HRESULT hr = S_OK; TStackRef<IQuadooButton> button; TStackRef<IQuadooControlContainer> focusContainer;
		*result = NULL;
		if(focus >= 0 && nodes[focus].eligible && focusedButton)
		{
			Check(QueryOptional(nodes[focus].control,&button));
			if(button && (!cancel || KeyFlag(nodes[focus].control,L"Cancel"))) { *result = nodes[focus].control; (*result)->AddRef(); goto Cleanup; }
		}
		if(focus >= 0) { Check(QueryOptional(nodes[focus].control,&focusContainer)); }
		for(INT scope = focus >= 0 ? (focusContainer ? focus : nodes[focus].parent) : 0; scope >= 0; scope = nodes[scope].parent)
			for(sysint n = 0; n < nodes.Length(); n++) if(nodes[n].eligible && InScope(static_cast<INT>(n),scope) && KeyFlag(nodes[n].control,cancel ? L"Cancel" : L"Default"))
			{
				button.Release(); Check(QueryOptional(nodes[n].control,&button));
				if(button) { *result = nodes[n].control; (*result)->AddRef(); goto Cleanup; }
			}
Cleanup:
		return hr;
	}
};
HRESULT CForm::UpdateDefaultButton()
{
	HRESULT hr = S_OK;
	CKeyboardTree tree;
	TStackRef<IQuadooControl> selected;
	QuadooForm::ContextMode mode;
	Check(m_context->GetMode(&mode));
	if(mode == QuadooForm::Design || m_state != QuadooForm::Ready)
	{
		goto Cleanup;
	}
	Check(tree.Read(Control()));
	Check(tree.Choose(tree.Target(GetFocus()),FALSE,&selected));
	for(sysint n = 0; n < tree.nodes.Length(); n++)
	{
		TStackRef<IQuadooButton> button;
		Check(QueryOptional(tree.nodes[n].control,&button));
		if(button && tree.nodes[n].window)
		{
			Check(SetDefaultButtonAppearance(button,selected && SameObject(selected,tree.nodes[n].control)));
		}
	}
Cleanup:
	return hr;
}
BOOL KeyboardComposing(HWND window)
{
	HIMC context = ImmGetContext(window); BOOL composing = FALSE;
	if(context) { composing = ImmGetCompositionStringW(context,GCS_COMPSTR,NULL,0) > 0; ImmReleaseContext(window,context); }
	return composing;
}
STDMETHODIMP CForm::PreprocessMessage(MSG* message, BOOL* handled)
{
	HRESULT hr;
	TStackRef<CForm> keepAlive(this);
	CKeyboardTree tree;
	TStackRef<IQuadooControl> selected;
	TStackRef<IQuadooButton> button;
	QuadooForm::ContextMode mode;
	BOOL entered = FALSE, cancelled = FALSE;
	HWND target;
	INT index;
	UINT scan;
	LRESULT code = 0;

	CheckIf(!handled,E_POINTER);
	*handled = FALSE;
	
	CheckIf(!message,E_INVALIDARG);
	Check(CheckAlive());

	Check(m_context->GetMode(&mode));
	if(mode == QuadooForm::Design || m_state != QuadooForm::Ready) goto Cleanup;
	if(message->message < WM_KEYFIRST || message->message > WM_KEYLAST) goto Cleanup;

	if(!m_hwnd || !IsWindowEnabled(m_hwnd) || (message->hwnd != m_hwnd && !IsChild(m_hwnd,message->hwnd)))
	{
		goto Cleanup;
	}
	Check(tree.Read(Control()));
	target = message->hwnd;
	index = tree.Target(target);

	// Resolve native child windows through their registered control wrapper.
	// The optional capability keeps native input policy with that control.
	if(index >= 0 && tree.nodes[index].eligible)
	{
		TStackRef<IQuadooKeyboardInput> input;
		BOOL owns = FALSE;
		Check(QueryOptional(tree.nodes[index].control,&input));
		if(input)
		{
			if(m_keyboardBusy)
			{
				*handled = TRUE;
				goto Cleanup;
			}
			m_keyboardBusy = TRUE;
			entered = TRUE;
			Check(input->OwnsKeyboardMessage(message,&owns));
			m_keyboardBusy = FALSE;
			entered = FALSE;
			if(m_state != QuadooForm::Ready || !KeyTargetReady(tree.nodes[index].control,target))
			{
				*handled = TRUE;
				goto Cleanup;
			}
			if(owns)
			{
				goto Cleanup;
			}
		}
	}
	if((message->message == WM_KEYDOWN || message->message == WM_SYSKEYDOWN) && !KeyboardComposing(message->hwnd) &&
		IsWindowEnabled(m_hwnd) && (message->hwnd == m_hwnd || IsChild(m_hwnd,message->hwnd)))
	{
		if(TranslateMenu(message))
		{
			*handled = TRUE;
			goto Cleanup;
		}
		Check(TranslatePopupShortcuts(Control(),message,handled));
		if(*handled)
		{
			goto Cleanup;
		}
	}
	if(GetAncestor(GetFocus(),GA_ROOT) != m_hwnd) goto Cleanup;
	if(index < 0 || !tree.nodes[index].eligible || target != GetFocus()) goto Cleanup;
	if(m_keyboardBusy)
	{
		*handled = TRUE;
		goto Cleanup;
	}
	m_keyboardBusy = TRUE;
	entered = TRUE;
	scan = (message->lParam >> 16) & 0xff;
	if(message->message == WM_KEYDOWN) { m_suppressedTarget = NULL; }
	if(message->message == WM_CHAR && m_suppressedTarget == target && scan == m_suppressedScan) { *handled = TRUE; goto Cleanup; }
	if(message->message == WM_KEYUP) m_suppressedTarget = NULL;
	if((message->message != WM_CHAR && message->wParam == VK_PROCESSKEY) || KeyboardComposing(target)) goto Cleanup;
	if(message->message == WM_KEYDOWN && message->wParam != VK_PROCESSKEY && !KeyboardComposing(target) && !(GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_SHIFT) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000) && (message->wParam == VK_RETURN || message->wParam == VK_ESCAPE))
	{
		code = SendMessageW(target,WM_GETDLGCODE,message->wParam,reinterpret_cast<LPARAM>(message));
		if(m_state != QuadooForm::Ready || !KeyTargetReady(tree.nodes[index].control,target) || GetFocus() != target) { *handled = TRUE; goto Cleanup; }
		BOOL claimed = (code & DLGC_WANTMESSAGE) != 0;
		// ComboBox edit children can leave dropdown arbitration to their parent.
		TStackRef<IQuadooComboBox> combo; Check(QueryOptional(tree.nodes[index].control,&combo));
		if(combo)
		{
			HWND window;
			Check(combo->GetHandle(&window));
			if(SendMessageW(window,CB_GETDROPPEDSTATE,0,0))
			{ *handled = TRUE; SendMessageW(window,message->message,message->wParam,message->lParam); goto Cleanup; }
		}
		if(!claimed)
		{
			Check(tree.Choose(index,message->wParam == VK_ESCAPE,&selected));
			if(selected)
			{
				Check(UpdateDefaultButton()); Check(selected->QueryInterface(__uuidof(IQuadooButton),reinterpret_cast<PVOID*>(&button)));
				*handled = TRUE; m_suppressedTarget = target; m_suppressedScan = scan;
				Check(button->Click()); goto Cleanup;
			}
		}
		// Dialog commands deliberately bypass preview, including native claims.
		goto TranslateDialog;
	}
	if((message->message == WM_KEYDOWN || message->message == WM_KEYUP || message->message == WM_CHAR) && (index != 0 || target != m_hwnd) && Boolean(L"KeyPreview") && (message->message == WM_CHAR || message->wParam != VK_TAB))
	{
		Check(Dispatch(message->message == WM_CHAR ? L"OnKeyPress" : message->message == WM_KEYDOWN ? L"OnKeyDown" : L"OnKeyUp",&cancelled,message->wParam));
		if(cancelled || m_state != QuadooForm::Ready || !KeyTargetReady(tree.nodes[index].control,target) || GetFocus() != target)
		{
			*handled = TRUE;
			if(message->message == WM_KEYDOWN) { m_suppressedTarget = target; m_suppressedScan = scan; }
			goto Cleanup;
		}
	}
TranslateDialog:
	if(message->message == WM_KEYDOWN && (message->wParam == VK_RETURN || message->wParam == VK_ESCAPE) && ((GetKeyState(VK_CONTROL) | GetKeyState(VK_SHIFT) | GetKeyState(VK_MENU)) & 0x8000)) goto Cleanup;
	Check(UpdateDefaultButton());
	if(m_state != QuadooForm::Ready || !KeyTargetReady(tree.nodes[index].control,target)) { *handled = TRUE; goto Cleanup; }
	*handled = IsDialogMessageW(m_hwnd,message);
Cleanup:
	if(entered) m_keyboardBusy = FALSE;
	if(FAILED(hr) && entered) { if(handled) *handled = TRUE; ReportCallback(hr,L"Keyboard preprocessing"); }
	return hr;
}
HRESULT CButton::ApplyNative(PCWSTR name)
{
	HRESULT hr; Check(CWinControl::ApplyNative(name)); RequestKeyboardRefresh(Control());
Cleanup:
	return hr;
}
}
