#include "Runtime.h"

namespace QF
{
	// IRStringArray permits UTF-8 strings as well as wide strings. Own a fresh wide
	// copy so neither the caller's array nor its exposed GetArray buffer is retained.
	static HRESULT CopyItemText(RSTRING source, RSTRING* copy)
	{
		HRESULT hr;
		CRString wide;
		CheckIf(!copy, E_POINTER); *copy = NULL; CheckIf(!source, E_INVALIDARG);
		if(RStrIsWide(source)) wide = source;
		else { Check(RStrConvertToW(CP_UTF8, RStrLen(source), RStrToAnsi(source), &wide)); }
		CheckIf(RStrLen(wide) > 1000000, E_INVALIDARG);
		for(INT n = 0; n < RStrLen(wide); n++) { CheckIf(!wide.c_wstr()[n], E_INVALIDARG); }
		Check(RStrCreateW(RStrLen(wide), wide.c_wstr(), copy));
	Cleanup:
		return hr;
	}

	HRESULT NormalizeItems(IJSONValue* value, IJSONValue** normalized)
	{
		HRESULT hr;
		TStackRef<IJSONArray> source, copy;
		CheckIf(!normalized, E_POINTER); *normalized = NULL; CheckIf(!value, E_INVALIDARG);
		Check(value->GetArray(&source)); CheckIf(source->Count() < 0 || source->Count() > 1000000, E_INVALIDARG);
		Check(JSONCreateArray(&copy));
		for(sysint n = 0, count = source->Count(); n < count; n++)
		{
			CRString text, owned; TStackRef<IJSONValue> item;
			Check(source->GetString(n, &text)); Check(CopyItemText(text, &owned));
			Check(JSONCreateString(owned, &item)); Check(copy->Add(item));
		}
		Check(JSONWrapArray(copy, normalized));
	Cleanup:
		return hr;
	}

	STDMETHODIMP CItemsControl::GetItems(IRStringArray** items)
	{
		HRESULT hr;
		TStackRef<IRStringArray> snapshot;
		TStackRef<IJSONValue> value; TStackRef<IJSONArray> array;
		CheckIf(!items, E_POINTER); *items = NULL; Check(CheckThread());
		Check(StoredValue(L"Items", &value)); Check(value->GetArray(&array));
		Check(ScCreateRStringArray(&snapshot));
		for(sysint n = 0; n < array->Count(); n++)
		{
			CRString text; Check(array->GetString(n, &text));
			Check(snapshot->AddStringW(text.c_wstr(), RStrLen(text)));
		}
		*items = snapshot.Detach();
	Cleanup:
		return hr;
	}

	STDMETHODIMP CItemsControl::SetItems(IRStringArray* items)
	{
		HRESULT hr;
		TStackRef<IJSONArray> array; TStackRef<IJSONValue> value;
		TStackRef<CControl> keepAlive(this);
		Check(CheckAlive()); CheckIf(!items, E_INVALIDARG);
		sysint count = items->Length(); CheckIf(count < 0 || count > 1000000, E_INVALIDARG);
		Check(JSONCreateArray(&array));
		for(sysint n = 0; n < count; n++)
		{
			CRString text, owned; TStackRef<IJSONValue> item;
			Check(items->GetRString(n, &text)); Check(CopyItemText(text, &owned));
			Check(JSONCreateString(owned, &item)); Check(array->Add(item));
		}
		Check(CheckAlive()); CheckIf(items->Length() != count, E_INVALIDARG);
		Check(JSONWrapArray(array, &value)); Check(SetNamedValue(L"Items", QuadooProperty::List, value));
	Cleanup:
		return hr;
	}

	STDMETHODIMP CItemsControl::GetItemIndex(INT* index)
	{
		HRESULT hr;
		CheckIf(!index, E_POINTER); *index = -1; Check(CheckThread()); *index = Integer(L"ItemIndex"); hr = S_OK;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CItemsControl::SetItemIndex(INT index)
	{
		HRESULT hr; TStackRef<IJSONValue> value;
		Check(CheckAlive()); Check(JSONCreateInteger(index, &value)); Check(SetNamedValue(L"ItemIndex", QuadooProperty::Int, value));
	Cleanup:
		return hr;
	}

	HRESULT CItemsControl::CreateNative()
	{
		HRESULT hr;
		TStackRef<IJSONValue> value; TStackRef<IJSONArray> items;
		Check(StoredValue(L"Items", &value)); Check(value->GetArray(&items));
		INT index = Integer(L"ItemIndex"); CheckIf(index < -1 || index >= items->Count(), E_INVALIDARG);
		if(HasText())
		{
			CRString text; TStackRef<IJSONValue> selected;
			Check(StoredString(L"Text", &text));
			if(index >= 0) { text = static_cast<RSTRING>(NULL); Check(items->GetString(index, &text)); }
			else if(!Editable()) { CheckIf(RStrLen(text), E_INVALIDARG); }
			CRString owned; Check(CopyItemText(text, &owned)); Check(JSONCreateString(owned, &selected)); Check(SetStoredValue(L"Text", selected));
		}
		Check(CWinControl::CreateNative());
	Cleanup:
		return hr;
	}

	HRESULT CItemsControl::NativeCreated()
	{
		HRESULT hr;
		TStackRef<IJSONValue> value; TStackRef<IJSONArray> items;
		m_syncItems = TRUE;
		Check(StoredValue(L"Items", &value)); Check(value->GetArray(&items));
		for(sysint n = 0; n < items->Count(); n++)
		{
			CRString text; Check(items->GetString(n, &text));
			LRESULT added = SendMessageW(m_hwnd, AddMessage(), 0, reinterpret_cast<LPARAM>(text.c_wstr()));
			CheckIf(added == CB_ERRSPACE, E_OUTOFMEMORY); CheckIf(added != n, E_FAIL);
		}
		INT index = Integer(L"ItemIndex");
		CheckIf(SendMessageW(m_hwnd, SelectMessage(), index, 0) != index, E_FAIL);
		if(HasText() && Editable() && index == -1)
		{ CRString text; Check(StoredString(L"Text", &text)); CheckIf(!SetWindowTextW(m_hwnd, text.c_wstr()), HrEnsureLastError()); }
		hr = S_OK;
	Cleanup:
		m_syncItems = FALSE;
		return hr;
	}

	HRESULT CItemsControl::SetValue(UINT index, QuadooProperty::Type type, IJSONValue* value)
	{
		HRESULT hr;
		TStackRef<IJSONValue> normalized; TStackRef<IJSONValue> oldItems; TStackRef<IJSONArray> items;
		CRString text;
		Check(CheckAlive()); CheckIf(index >= static_cast<UINT>(m_values.Length()), DISP_E_MEMBERNOTFOUND);
		PCWSTR name = m_values[index].spec->name;
		BOOL itemProperty = 0 == TStrCmpAssert(name, L"Items"), selection = 0 == TStrCmpAssert(name, L"ItemIndex");
		BOOL textProperty = HasText() && 0 == TStrCmpAssert(name, L"Text"), style = HasText() && 0 == TStrCmpAssert(name, L"Style");
		if(!itemProperty && !selection && !textProperty && !style) { hr = CWinControl::SetValue(index, type, value); goto Cleanup; }
		CheckIf(type != m_values[index].spec->type && !(style && type == QuadooProperty::Int), DISP_E_TYPEMISMATCH);
		Check(NormalizeValue(m_values[index].spec, value, &normalized));
		Check(StoredValue(L"Items", &oldItems)); Check((itemProperty ? normalized : oldItems)->GetArray(&items));
		INT selected = Integer(L"ItemIndex");
		if(selection) { Check(normalized->GetInteger(&selected)); CheckIf(selected >= items->Count(), E_INVALIDARG); }
		else if(itemProperty && selected >= items->Count()) selected = -1;
		if(HasText())
		{
			Check(StoredString(L"Text", &text));
			if(textProperty)
			{
				CRString replacement, owned; Check(normalized->GetString(&replacement)); Check(CopyItemText(replacement, &owned));
				if(!Editable()) { CheckIf(!SameString(text, owned), E_INVALIDARG); }
				if(m_nativeChange) selected = static_cast<INT>(SendMessageW(m_hwnd, SelectionMessage(), 0, 0));
				if(!SameString(text, owned)) selected = -1;
				text = owned;
			}
			if(selected >= 0) { text = static_cast<RSTRING>(NULL); Check(items->GetString(selected, &text)); }
			else
			{
				INT newStyle = Integer(L"Style"); if(style) { Check(normalized->GetInteger(&newStyle)); }
				if(newStyle == QuadooForm::DropDownList) text = RSTRING_CAST(L"");
			}
		}
		Check(ChangeItems(itemProperty ? normalized : oldItems, selected, text, style ? normalized : NULL));
	Cleanup:
		return hr;
	}

	HRESULT CItemsControl::ChangeItems(IJSONValue* items, INT index, RSTRING text, IJSONValue* style)
	{
		HRESULT hr;
		static PCWSTR const names[] = { L"Items", L"ItemIndex", L"Text", L"Style" };
		TStackRef<IJSONValue> next[4], previous[4];
		UINT slots[4], count = HasText() ? 4 : 2;
		BOOL changed[4] = { FALSE, FALSE, FALSE, FALSE }, entered = FALSE;
		TStackRef<CControl> keepAlive(this);
		next[0] = items; Check(JSONCreateInteger(index, &next[1]));
		if(HasText()) { Check(JSONCreateString(text, &next[2])); if(style) next[3] = style; else { Check(StoredValue(L"Style", &next[3])); } }
		for(UINT n = 0; n < count; n++)
		{
			slots[n] = FindProperty(names[n]); CheckIf(slots[n] >= static_cast<UINT>(m_values.Length()), DISP_E_MEMBERNOTFOUND);
			changed[n] = 0 != JSONCompare(m_values[slots[n]].value, next[n]);
		}
		CheckIfIgnore(!changed[0] && !changed[1] && !changed[2] && !changed[3], S_FALSE);
		CheckIf(m_changing, QuadooForm::EInvalidState);
		m_changing = TRUE; entered = TRUE;
		for(UINT n = 0; n < count; n++) { previous[n].Attach(m_values[slots[n]].value); m_values[slots[n]].value = next[n].Detach(); }
		if(m_hwnd)
		{
			if(changed[0] || changed[3]) { hr = ReplaceNative(); }
			else if(!m_nativeChange || SendMessageW(m_hwnd, SelectionMessage(), 0, 0) != index ||
				(m_nativeChange && HasText() && index >= 0 && changed[1]))
			{
				// CBN_SELCHANGE can precede Win32's edit-text update. Reapply the
				// selected index under suppression so callbacks also see native Text.
				COMBOBOXINFO info; DWORD start = 0, end = 0;
				ZeroMemory(&info,sizeof(info)); info.cbSize = sizeof(info);
				BOOL preserveCaret = m_nativeChange && index == -1 && HasText() && Editable() && GetComboBoxInfo(m_hwnd,&info);
				if(preserveCaret) SendMessageW(info.hwndItem,EM_GETSEL,reinterpret_cast<WPARAM>(&start),reinterpret_cast<LPARAM>(&end));
				m_syncItems = TRUE;
				hr = SendMessageW(m_hwnd, SelectMessage(), index, 0) == index ? S_OK : E_FAIL;
				if(SUCCEEDED(hr) && HasText() && Editable() && index == -1 && !SetWindowTextW(m_hwnd, RStrToWide(text))) hr = HrEnsureLastError();
				if(preserveCaret) SendMessageW(info.hwndItem,EM_SETSEL,start,end);
				m_syncItems = FALSE;
			}
			else
				hr = S_OK;
			if(FAILED(hr))
			{
				for(UINT n = 0; n < count; n++) { SafeRelease(m_values[slots[n]].value); m_values[slots[n]].value = previous[n].Detach(); }
				// A failed staged replacement leaves the previous HWND untouched.
				if(!changed[0] && !changed[3])
				{
					m_syncItems = TRUE; SendMessageW(m_hwnd, SelectMessage(), Integer(L"ItemIndex"), 0);
					if(HasText() && Editable() && Integer(L"ItemIndex") == -1) { CRString old; if(SUCCEEDED(StoredString(L"Text", &old))) SetWindowTextW(m_hwnd, old.c_wstr()); }
					m_syncItems = FALSE;
				}
				goto Cleanup;
			}
		}
		for(UINT n = 0; n < count; n++)
		{
			if(changed[n])
				m_values[slots[n]].dirty = TRUE;
		}
		Check(FlushNotifications());
		if(m_state == QuadooForm::Ready && changed[1]) { Check(Dispatch(L"OnSelectionChange")); }
		if(m_state == QuadooForm::Ready && changed[2]) { Check(Dispatch(L"OnChange")); }

	Cleanup:
		if(entered) m_changing = FALSE;
		return hr;
	}

	HRESULT CItemsControl::SelectionChanged()
	{
		HRESULT hr;
		BOOL previous = m_nativeChange;
		m_nativeChange = TRUE;
		hr = SetItemIndex(static_cast<INT>(SendMessageW(m_hwnd, SelectionMessage(), 0, 0)));
		m_nativeChange = previous;
		if(FAILED(hr) && m_hwnd) ReportCallback(SynchronizeSelection(), L"Selection rollback");
		return hr;
	}

	HRESULT CItemsControl::SynchronizeSelection()
	{
		HRESULT hr;
		BOOL previous = m_syncItems;
		INT index = Integer(L"ItemIndex");
		m_syncItems = TRUE;
		CheckIf(!m_hwnd, QuadooForm::EDestroyed);
		CheckIf(SendMessageW(m_hwnd, SelectMessage(), index, 0) != index, E_FAIL);
		if(HasText() && Editable() && index == -1)
		{
			CRString text;
			Check(StoredString(L"Text", &text));
			CheckIf(!SetWindowTextW(m_hwnd,text.c_wstr()), HrEnsureLastError());
		}
		hr = S_OK;
	Cleanup:
		m_syncItems = previous;
		return hr;
	}

	HRESULT CItemsControl::TextChanged()
	{
		HRESULT hr;
		CRString text; PWSTR write; TStackRef<IJSONValue> value;
		BOOL previous = m_nativeChange;
		INT length = GetWindowTextLengthW(m_hwnd);
		Check(RStrAllocW(length, &text, &write)); GetWindowTextW(m_hwnd, write, length + 1);
		Check(JSONCreateString(text, &value));
		m_nativeChange = TRUE; hr = SetNamedValue(L"Text", QuadooProperty::String, value); m_nativeChange = previous;
	Cleanup:
		if(FAILED(hr) && m_hwnd) ReportCallback(SynchronizeSelection(), L"Text rollback");
		return hr;
	}

	LRESULT CItemsControl::Message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled)
	{
		if(!m_syncItems && !m_replacing && m_state < QuadooForm::Destroying)
		{
			if(msg == SelectMessage())
			{
				*handled = TRUE; HRESULT hr = SetItemIndex(static_cast<INT>(wp)); ReportCallback(hr, L"OnSelectionChange");
				return SUCCEEDED(hr) ? Integer(L"ItemIndex") : -1;
			}
			if(msg == (HasText() ? CB_SELECTSTRING : LB_SELECTSTRING))
			{
				*handled = TRUE;
				LRESULT found = SendMessageW(hwnd, HasText() ? CB_FINDSTRING : LB_FINDSTRING, wp, lp);
				if(found != -1)
				{ HRESULT hr = SetItemIndex(static_cast<INT>(found)); ReportCallback(hr, L"OnSelectionChange"); if(FAILED(hr)) found = -1; }
				return found;
			}
			// Mutating native collections bypasses descriptor transactions. Reject those
			// messages; clients edit a snapshot and use SetItems instead.
			if(msg == ResetMessage() || msg == AddMessage() || (HasText() ?
				(msg == CB_INSERTSTRING || msg == CB_DELETESTRING || msg == CB_DIR) :
				(msg == LB_INSERTSTRING || msg == LB_DELETESTRING || msg == LB_DIR || msg == LB_SETCOUNT || msg == LB_ADDFILE)))
			{ *handled = TRUE; return -1; }
			if(HasText() && msg == WM_SETTEXT)
			{
				PCWSTR text = lp ? reinterpret_cast<PCWSTR>(lp) : L"";
				TStackRef<IJSONValue> value; HRESULT hr = JSONCreateStringW(text, TStrLenAssert(text), &value);
				if(SUCCEEDED(hr)) hr = SetNamedValue(L"Text", QuadooProperty::String, value);
				ReportCallback(hr, L"OnChange"); *handled = TRUE; return SUCCEEDED(hr);
			}
		}
		if(HasText() && (msg == WM_SETFOCUS || msg == WM_KILLFOCUS)) return 0; // CBN_* includes the internal edit.
		return CWinControl::Message(hwnd, msg, wp, lp, content, handled);
	}

	VOID CComboBox::NativeStyle(PCWSTR* className, DWORD* style, DWORD* extended)
	{
		CWinControl::NativeStyle(className, style, extended); *className = L"COMBOBOX";
		*style |= WS_TABSTOP | WS_VSCROLL | CBS_AUTOHSCROLL | CBS_NOINTEGRALHEIGHT | (Editable() ? CBS_DROPDOWN : CBS_DROPDOWNLIST);
	}

	HRESULT CComboBox::NativeCreated()
	{
		HRESULT hr;
		Check(CItemsControl::NativeCreated());
		if(Editable())
		{
			COMBOBOXINFO info; ZeroMemory(&info,sizeof(info)); info.cbSize = sizeof(info);
			CheckIf(!GetComboBoxInfo(m_hwnd,&info), HrEnsureLastError());
			// Keyboard input targets the edit child, not the ComboBox HWND. Each edit
			// owns its own subclass reference, including while a replacement is staged.
			CheckIf(!SetWindowSubclass(info.hwndItem,EditProc,reinterpret_cast<UINT_PTR>(this),reinterpret_cast<DWORD_PTR>(this)), E_FAIL);
			AddRef();
		}
	Cleanup:
		return hr;
	}

	LRESULT CALLBACK CComboBox::EditProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data)
	{
		CComboBox* control = reinterpret_cast<CComboBox*>(data);
		BOOL handled = FALSE; LRESULT result = 0;
		control->AddRef();
		if(!control->m_replacing && control->m_state == QuadooForm::Ready && (msg == WM_SETFOCUS || msg == WM_KILLFOCUS))
		{
			control->NotifyFocus(msg == WM_SETFOCUS ? hwnd : reinterpret_cast<HWND>(wp));
		}
		if(msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_CHAR || msg == WM_MOUSEWHEEL || msg == WM_GETDLGCODE)
			result = control->CWinControl::Message(hwnd,msg,wp,lp,FALSE,&handled);
		else if(msg == WM_NCDESTROY)
		{
			RemoveWindowSubclass(hwnd,EditProc,id);
			control->Release();
		}
		if(!handled && IsWindow(hwnd))
			result = DefSubclassProc(hwnd,msg,wp,lp);
		control->Release();
		return result;
	}

	VOID CListBox::NativeStyle(PCWSTR* className, DWORD* style, DWORD* extended)
	{
		CWinControl::NativeStyle(className, style, extended); *className = L"LISTBOX";
		*style |= WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT; *extended |= WS_EX_CLIENTEDGE;
	}

	HRESULT CComboBox::AdjustNativeRect(RECT* rect)
	{
		rect->bottom += MulDiv(Integer(L"DropDownHeight"), m_dpi, 96); return S_OK;
	}

	HRESULT CComboBox::ApplyNative(PCWSTR name)
	{
		HRESULT hr;
		Check(CItemsControl::ApplyNative(name));
		if(m_hwnd && 0 == TStrCmpAssert(name,L"DropDownHeight")) { Check(ArrangeNative(&m_effective,TRUE)); }
		if(m_hwnd && (0 == TStrCmpAssert(name,L"ForeColor") || 0 == TStrCmpAssert(name,L"BackColor")))
		{
			COMBOBOXINFO info; ZeroMemory(&info,sizeof(info)); info.cbSize = sizeof(info);
			if(GetComboBoxInfo(m_hwnd,&info))
			{ if(info.hwndItem) InvalidateRect(info.hwndItem,NULL,TRUE); if(info.hwndList) InvalidateRect(info.hwndList,NULL,TRUE); }
		}
	Cleanup:
		return hr;
	}

	HRESULT CComboBox::ArrangeNative(const QuadooForm::Rect* bounds, BOOL changed)
	{
		HRESULT hr;
		Check(CWinControl::ArrangeNative(bounds, changed));
		if(m_hwnd)
		{
			// The Win32 height includes the drop list. Set the selection-field height
			// separately so logical Height continues to describe the collapsed control.
			RECT physical; CheckIf(!GetWindowRect(m_hwnd, &physical), HrEnsureLastError());
			INT current = static_cast<INT>(SendMessageW(m_hwnd, CB_GETITEMHEIGHT, -1, 0));
			INT desired = MulDiv(bounds->Height, m_dpi, 96), border = physical.bottom - physical.top - current;
			// Standard ComboBox field heights are limited by the common control.
			// Docking may offer a much taller rectangle; retain valid native geometry.
			CheckIf(SendMessageW(m_hwnd, CB_SETITEMHEIGHT, -1, min(255,max(1,desired - border))) == CB_ERR, E_FAIL);
		}
	Cleanup:
		return hr;
	}

	LRESULT CComboBox::Message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled)
	{
		if(msg == WM_CTLCOLOREDIT || msg == WM_CTLCOLORSTATIC || msg == WM_CTLCOLORLISTBOX)
		{
			HDC dc = reinterpret_cast<HDC>(wp); COLORREF back = ResolveQuadooColor(Integer(L"BackColor"));
			SetTextColor(dc,ResolveQuadooColor(Integer(L"ForeColor"))); SetBkColor(dc,back); SetDCBrushColor(dc,back);
			*handled = TRUE; return reinterpret_cast<LRESULT>(GetStockObject(DC_BRUSH));
		}
		return CItemsControl::Message(hwnd,msg,wp,lp,content,handled);
	}

	HRESULT CComboBox::Command(UINT notification)
	{
		if(m_replacing || m_state != QuadooForm::Ready) return S_FALSE;
		if(notification == CBN_SETFOCUS || notification == CBN_KILLFOCUS) return Dispatch(notification == CBN_SETFOCUS ? L"OnFocus" : L"OnBlur");
		if(m_syncItems) return S_FALSE;
		if(notification == CBN_SELCHANGE) return SelectionChanged();
		if(notification == CBN_EDITCHANGE && Editable()) return TextChanged();
		return S_FALSE;
	}
	HRESULT CListBox::Command(UINT notification)
	{
		return !m_replacing && m_state == QuadooForm::Ready && notification == LBN_SELCHANGE ? SelectionChanged() : S_FALSE;
	}
}
