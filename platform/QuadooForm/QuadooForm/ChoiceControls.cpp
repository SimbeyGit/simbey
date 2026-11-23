#include "Runtime.h"

namespace QF
{
// A snapshot keeps sibling interfaces alive across delegates, observer callbacks,
// and hierarchy destruction. Group operations use only published capabilities.
class CRadioSiblings
{
public:
	TArray<IQuadooRadioButton*> items;
	~CRadioSiblings() { for(sysint n = 0; n < items.Length(); n++) items[n]->Release(); }
	HRESULT Read(IQuadooControlContainer* parent)
	{
		HRESULT hr;
		UINT count;
		CheckIf(!parent, QuadooForm::EInvalidState);
		Check(parent->GetChildCount(&count));
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IQuadooControl> child;
			TStackRef<IQuadooRadioButton> radio;
			Check(parent->GetChild(n, &child)); Check(QueryOptional(child, &radio));
			if(radio)
			{
				INT order; Check(radio->GetTabOrder(&order));
				sysint position;
				for(position = 0; position < items.Length(); position++)
				{
					INT other; Check(items[position]->GetTabOrder(&other));
					if(order < other) break;
				}
				IQuadooRadioButton* item = radio;
				Check(items.InsertAt(&item, position)); radio.Detach();
			}
		}
	Cleanup:
		return hr;
	}
};

HRESULT CCheckedButton::NativeCreated() { return ApplyNative(L""); }
HRESULT CCheckedButton::ApplyNative(PCWSTR name)
{
	HRESULT hr;
	Check(CWinControl::ApplyNative(name));
	if(m_hwnd)
	{
		m_syncCheck = TRUE;
		SendMessageW(m_hwnd, BM_SETCHECK, NativeCheckState(), 0);
		m_syncCheck = FALSE;
	}
Cleanup:
	return hr;
}
HRESULT CCheckedButton::Command(UINT notification)
{
	HRESULT hr = S_FALSE;
	if(notification == BN_CLICKED && m_state == QuadooForm::Ready)
	{
		Check(Click()); Check(CheckAlive()); Check(Dispatch(L"OnClick"));
	}
Cleanup:
	return hr;
}
LRESULT CCheckedButton::Message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled)
{
	if(msg == BM_SETCHECK && !m_syncCheck && m_state < QuadooForm::Destroying)
	{
		ReportCallback(ChangeCheckState(static_cast<INT>(wp)), L"Check state");
		*handled = TRUE; return 0;
	}
	return CWinControl::Message(hwnd, msg, wp, lp, content, handled);
}

VOID CCheckBox::NativeStyle(PCWSTR* className, DWORD* style, DWORD* extended)
{
	CWinControl::NativeStyle(className, style, extended);
	*className = L"BUTTON"; *style |= (Boolean(L"ThreeState") ? BS_3STATE : BS_CHECKBOX) | WS_TABSTOP;
}
HRESULT CCheckBox::CreateNative()
{
	HRESULT hr;
	CheckIf(Integer(L"CheckState") == BST_INDETERMINATE && !Boolean(L"ThreeState"), E_INVALIDARG);
	Check(CWinControl::CreateNative());
Cleanup:
	return hr;
}
HRESULT CCheckBox::SetValue(UINT index, QuadooProperty::Type type, IJSONValue* value)
{
	HRESULT hr;
	TStackRef<IJSONValue> normalized;
	Check(CheckAlive()); CheckIf(index >= static_cast<UINT>(m_values.Length()), DISP_E_MEMBERNOTFOUND);
	Check(NormalizeValue(m_values[index].spec, value, &normalized));
	PCWSTR name = m_values[index].spec->name;
	if(0 == TStrCmpAssert(name, L"CheckState"))
	{
		INT state; Check(normalized->GetInteger(&state));
		CheckIf(state == BST_INDETERMINATE && !Boolean(L"ThreeState"), E_INVALIDARG);
	}
	else if(0 == TStrCmpAssert(name, L"ThreeState"))
	{
		bool enabled; Check(normalized->GetBoolean(&enabled));
		CheckIf(!enabled && Integer(L"CheckState") == BST_INDETERMINATE, E_INVALIDARG);
	}
	Check(CControl::SetValue(index, type, normalized));
Cleanup:
	return hr;
}
HRESULT CCheckBox::ApplyNative(PCWSTR name)
{
	if(m_hwnd && 0 == TStrCmpAssert(name, L"ThreeState"))
		SendMessageW(m_hwnd, BM_SETSTYLE, Boolean(L"ThreeState") ? BS_3STATE : BS_CHECKBOX, TRUE);
	return CCheckedButton::ApplyNative(name);
}
HRESULT CCheckBox::PropertyChanged(PCWSTR name)
{
	return 0 == TStrCmpAssert(name, L"CheckState") && m_state == QuadooForm::Ready ? Dispatch(L"OnChange") : S_OK;
}
HRESULT CCheckBox::ChangeCheckState(INT state)
{
	HRESULT hr;
	TStackRef<IJSONValue> value;
	Check(JSONCreateInteger(state, &value)); Check(SetNamedValue(L"CheckState", QuadooProperty::Int, value));
Cleanup:
	return hr;
}
HRESULT CCheckBox::Click()
{
	INT state = Integer(L"CheckState");
	return ChangeCheckState((state + 1) % (Boolean(L"ThreeState") ? 3 : 2));
}

VOID CRadioButton::NativeStyle(PCWSTR* className, DWORD* style, DWORD* extended)
{
	CWinControl::NativeStyle(className, style, extended);
	*className = L"BUTTON"; *style |= BS_RADIOBUTTON;
}
HRESULT CRadioButton::CreateNative()
{
	HRESULT hr;
	CRadioSiblings siblings;
	// Initial definitions must be unambiguous. Reject duplicate checked defaults
	// before registering or mutating an already-live group.
	if(Boolean(L"Checked"))
	{
		HWND group; Check(m_parent->GetHandle(&group));
		CheckIf(GetPropW(group, QuadooForm::RadioSelectionProperty), QuadooForm::EInvalidState);
		Check(siblings.Read(m_parent));
		for(sysint n = 0; n < siblings.items.Length(); n++)
		{
			BOOL checked; Check(siblings.items[n]->GetChecked(&checked)); CheckIf(checked, E_INVALIDARG);
		}
	}
	Check(CWinControl::CreateNative());
Cleanup:
	return hr;
}
HRESULT CRadioButton::SetValue(UINT index, QuadooProperty::Type type, IJSONValue* value)
{
	HRESULT hr;
	TStackRef<CRadioButton> keepAlive(this);
	TStackRef<IQuadooControlContainer> parent(m_parent);
	TStackRef<IJSONValue> normalized;
	CRadioSiblings siblings;
	HWND group = NULL;
	BOOL locked = FALSE, checked;
	HRESULT callback = S_OK;
	Check(CheckAlive()); CheckIf(index >= static_cast<UINT>(m_values.Length()), DISP_E_MEMBERNOTFOUND);
	if(0 != TStrCmpAssert(m_values[index].spec->name, L"Checked")) { Check(CControl::SetValue(index, type, value)); goto Cleanup; }
	CheckIf(type != QuadooProperty::Boolean, DISP_E_TYPEMISMATCH);
	Check(NormalizeValue(m_values[index].spec, value, &normalized));
	{ bool enabled; Check(normalized->GetBoolean(&enabled)); checked = enabled; }
	if(!checked || Boolean(L"Checked")) { Check(CControl::SetValue(index, type, normalized)); goto Cleanup; }
	CheckIf(!parent, QuadooForm::EInvalidState);
	Check(parent->GetHandle(&group)); CheckIf(!group, QuadooForm::EDestroyed);
	CheckIf(GetPropW(group, QuadooForm::RadioSelectionProperty), QuadooForm::EInvalidState);
	CheckIf(!SetPropW(group, QuadooForm::RadioSelectionProperty, this), HrEnsureLastError()); locked = TRUE;
	Check(siblings.Read(parent));
	for(sysint n = 0; n < siblings.items.Length(); n++)
	{
		IQuadooRadioButton* sibling = siblings.items[n];
		if(SameObject(sibling, Control())) continue;
		Check(sibling->GetChecked(&checked));
		if(checked)
		{
			HRESULT changed = sibling->SetChecked(FALSE);
			if(FAILED(changed) && SUCCEEDED(callback)) callback = changed;
			Check(CheckAlive());
			// A callback failure may follow a successful state change. Complete the
			// selection, but never check this control if a sibling remains checked.
			Check(sibling->GetChecked(&checked)); CheckIf(checked, FAILED(changed) ? changed : E_UNEXPECTED);
		}
	}
	Check(CControl::SetValue(index, type, normalized));
	if(FAILED(callback)) hr = callback;
Cleanup:
	if(locked && IsWindow(group) && GetPropW(group, QuadooForm::RadioSelectionProperty) == this) RemovePropW(group, QuadooForm::RadioSelectionProperty);
	return hr;
}
STDMETHODIMP CRadioButton::GetChecked(BOOL* checked)
{
	HRESULT hr;
	CheckIf(!checked, E_POINTER); *checked = FALSE; Check(CheckThread()); *checked = Boolean(L"Checked");
Cleanup:
	return hr;
}
STDMETHODIMP CRadioButton::SetChecked(BOOL checked)
{
	HRESULT hr;
	TStackRef<IJSONValue> value;
	Check(JSONCreateBool(!!checked, &value)); Check(SetNamedValue(L"Checked", QuadooProperty::Boolean, value));
Cleanup:
	return hr;
}
HRESULT CRadioButton::ChangeCheckState(INT state)
{
	return state == BST_UNCHECKED || state == BST_CHECKED ? SetChecked(state == BST_CHECKED) : E_INVALIDARG;
}
HRESULT CRadioButton::UpdateTabStops()
{
	HRESULT hr;
	CRadioSiblings siblings;
	IQuadooRadioButton* first = NULL;
	IQuadooRadioButton* selected = NULL;
	Check(siblings.Read(m_parent));
	for(sysint n = 0; n < siblings.items.Length(); n++)
	{
		IQuadooRadioButton* radio = siblings.items[n]; BOOL visible, enabled, checked;
		Check(radio->GetVisible(&visible)); Check(radio->GetEnabled(&enabled)); Check(radio->GetChecked(&checked));
		if(visible && enabled) { if(!first) first = radio; if(checked) selected = radio; }
	}
	for(sysint n = 0; n < siblings.items.Length(); n++)
	{
		HWND window; Check(siblings.items[n]->GetHandle(&window));
		LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
		BOOL stop = siblings.items[n] == (selected ? selected : first);
		SetWindowLongPtrW(window, GWL_STYLE, stop ? style | WS_TABSTOP : style & ~WS_TABSTOP);
	}
Cleanup:
	return hr;
}
HRESULT CRadioButton::PropertyChanged(PCWSTR name)
{
	HRESULT hr;
	Check(UpdateTabStops());
	if(0 == TStrCmpAssert(name, L"Checked") && m_state == QuadooForm::Ready) { Check(Dispatch(L"OnChange")); }
Cleanup:
	return hr;
}
HRESULT CRadioButton::ArrangeNative(const QuadooForm::Rect* bounds, BOOL changed)
{
	HRESULT hr;
	Check(CWinControl::ArrangeNative(bounds, changed)); Check(UpdateTabStops());
Cleanup:
	return hr;
}
HRESULT CRadioButton::Navigate(BOOL forward)
{
	HRESULT hr;
	CRadioSiblings siblings;
	sysint current = -1;
	Check(siblings.Read(m_parent));
	for(sysint n = 0; n < siblings.items.Length(); n++) if(SameObject(siblings.items[n], Control())) { current = n; break; }
	CheckIf(current < 0, QuadooForm::EInvalidState);
	for(sysint step = 1; step <= siblings.items.Length(); step++)
	{
		sysint index = (current + (forward ? step : siblings.items.Length() - step)) % siblings.items.Length();
		IQuadooRadioButton* radio = siblings.items[index]; BOOL enabled, visible;
		Check(radio->GetEnabled(&enabled)); Check(radio->GetVisible(&visible));
		if(enabled && visible)
		{
			Check(radio->SetChecked(TRUE)); Check(radio->SetFocus());
			// Reflected activation uses the same public/native path as a click.
			// The state is already selected, so only OnClick is raised here.
			HWND window; Check(radio->GetHandle(&window)); SendMessageW(window, BM_CLICK, 0, 0);
			goto Cleanup;
		}
	}
	hr = S_FALSE;
Cleanup:
	return hr;
}
LRESULT CRadioButton::Message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled)
{
	if(msg == WM_GETDLGCODE)
	{
		LRESULT code = m_windowData.original ? CallWindowProcW(m_windowData.original, hwnd, msg, wp, lp) : DLGC_RADIOBUTTON | DLGC_BUTTON;
		*handled = TRUE; return code | DLGC_WANTARROWS;
	}
	LRESULT result = CCheckedButton::Message(hwnd, msg, wp, lp, content, handled);
	if(!*handled && msg == WM_KEYDOWN && m_state == QuadooForm::Ready && (wp == VK_LEFT || wp == VK_UP || wp == VK_RIGHT || wp == VK_DOWN))
	{
		*handled = TRUE; ReportCallback(Navigate(wp == VK_RIGHT || wp == VK_DOWN), L"Radio navigation");
	}
	return result;
}

VOID CGroupBox::NativeStyle(PCWSTR* className, DWORD* style, DWORD* extended)
{
	CContainer::NativeStyle(className, style, extended); *className = L"BUTTON"; *style |= BS_GROUPBOX;
}
HRESULT CGroupBox::ContentInsets(QuadooForm::Insets* insets)
{
	HRESULT hr;
	HDC dc = NULL;
	HGDIOBJ old = NULL;
	TEXTMETRICW metrics;
	Check(CContainer::ContentInsets(insets));
	dc = GetDC(m_hwnd); CheckIf(!dc, HrEnsureLastError()); old = SelectObject(dc, m_font);
	CheckIf(!GetTextMetricsW(dc, &metrics), HrEnsureLastError());
	insets->Left += 8; insets->Right += 8; insets->Bottom += 8;
	insets->Top += MulDiv(metrics.tmHeight, 96, m_dpi) + 4;
Cleanup:
	if(dc) { if(old) SelectObject(dc, old); ReleaseDC(m_hwnd, dc); }
	return hr;
}
VOID CGroupBox::PaintBackground(HDC dc, const RECT* bounds)
{
	CContainer::PaintBackground(dc, bounds);
	if(m_windowData.original) CallWindowProcW(m_windowData.original, m_hwnd, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT);
}
LRESULT CGroupBox::Message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled)
{
	// Native group boxes normally return HTTRANSPARENT. This container also hosts
	// painted controls and needs their hit testing routed through its own window.
	if(msg == WM_NCHITTEST) { *handled = TRUE; return HTCLIENT; }
	return CContainer::Message(hwnd, msg, wp, lp, content, handled);
}
}
