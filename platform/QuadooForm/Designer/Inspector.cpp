#include "Designer.h"
#include "..\QuadooForm\ColorNames.h"
#include <commdlg.h>
#include "Library\Util\Formatting.h"
#include "Library\ChooseFile.h"
#include "..\Imaging\ImageResource.h"

namespace QFD
{
	HRESULT CDesignerEditor::OpenSelectedHandler()
	{
		HRESULT hr;
		INT row = ListView_GetNextItem(m_list,-1,LVNI_SELECTED);
		CRString name;
		CheckIfIgnore(m_readOnly,S_FALSE);
		CheckIf(SelectionCount() != 1 || !m_document.SourceSite() || !m_events || row < 0 || row >= m_rows.Length() || m_rows[row].type != QuadooProperty::Event,E_ACCESSDENIED);
		name = m_rows[row].name;
		Check(FinishInline(TRUE));
		Check(m_document.SourceSite()->OpenHandler(m_selected,name));
	Cleanup:
		return hr;
	}
	static HRESULT ValueText(IJSONValue* value, QuadooProperty::Type type, RSTRING* text)
	{
		HRESULT hr;
		CMemoryStream stream;
		CheckIf(!text,E_POINTER);
		*text = NULL;
		if(value && (type == QuadooProperty::String || type == QuadooProperty::Event || type == QuadooProperty::Memo))
		{
			Check(value->GetString(text));
		}
		else if(type == QuadooProperty::Color)
		{
			DWORD dwValue;

			CheckIf(NULL == value, E_INVALIDARG);
			Check(value->GetInteger(reinterpret_cast<INT*>(&dwValue)));

			if(dwValue & 0x80000000)
			{
				DWORD idxSysColor = dwValue & 0x00FFFFFF;

				if(idxSysColor < ARRAYSIZE(QuadooColors::c_rgSysColors))
				{
					Check(RStrCreateW(TStrLenAssert(QuadooColors::c_rgSysColors[idxSysColor]), QuadooColors::c_rgSysColors[idxSysColor], text));
				}
				else
				{
					Check(RStrFormatW(text, L"Unknown: %d", idxSysColor));
				}
			}
			else
			{
				Check(RStrFormatW(text, L"#%.2X%.2X%.2X", GetRValue(dwValue), GetGValue(dwValue), GetBValue(dwValue)));
			}
		}
		else
		{
			Check(JSONSerialize(value,&stream));
			Check(RStrCreateW(stream.DataRemaining() / sizeof(WCHAR),stream.TGetReadPtr<WCHAR>(),text));
		}
	Cleanup:
		return hr;
	}

	HRESULT CDesignerEditor::Inspector()
	{
		HRESULT hr;
		TStackRef<IQuadooComponent> control;
		CSelectionProperties properties;
		UINT count;

		SendMessageW(m_list,WM_SETREDRAW,FALSE,0);
		ClearRows();
		ListView_DeleteAllItems(m_list);
		CheckIfIgnore(!m_document.Root(),S_OK);
		Check(SelectedComponent(&control));
		Check(SelectionProperties(properties));
		count = properties.Count();
		CheckIf(count > 4096,E_INVALIDARG);
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IQuadooPropertyDescriptor> descriptor; TStackRef<IJSONValue> value;
			Row row; CRString name, text;
			UINT choices;
			Check(properties.GetAt(n,&row.index,&descriptor,&row.flags));
			Check(descriptor->GetType(&row.type));
			Check(descriptor->GetEnumCount(&choices));
			row.dropdown = row.type == QuadooProperty::ObjectReference || row.type == QuadooProperty::Boolean ||
				(choices && (row.type == QuadooProperty::Enum || row.type == QuadooProperty::Int || row.type == QuadooProperty::Long));
			if((row.flags & (QuadooForm::PropertyHidden | QuadooForm::PropertyRuntimeOnly)) || (row.type == QuadooProperty::Event) != (m_events != FALSE))
				continue;
			Check(descriptor->GetName(&name));
			Check(properties.GetValue(row.index,&value,&row.mixed));

			if(row.type == QuadooProperty::Color)
			{
				CheckIf(!value,E_INVALIDARG);
				Check(value->GetInteger(&row.color));
			}
			else
				row.color = 0;

			if(row.type == QuadooProperty::Collection && name == RSTRING_CAST(L"Images") && CanExecute(ImageListEditor))
			{
				TStackRef<IJSONArray> images;
				if(value)
				{
					Check(value->GetArray(&images));
				}
				Check(RStrFormatW(&text,L"%u images: double-click to edit",images ? static_cast<UINT>(images->Count()) : 0));
			}
			else if(row.type == QuadooProperty::Collection && name == RSTRING_CAST(L"Panels") && CanExecute(AddStatusPanel))
			{
				TStackRef<IJSONArray> panels;
				Check(value->GetArray(&panels));
				Check(RStrFormatW(&text,L"%u panels: edit in Objects tree",static_cast<UINT>(panels->Count())));
			}
			else if(row.type == QuadooProperty::Collection && name == RSTRING_CAST(L"Columns") && CanExecute(ListColumnsEditor))
			{
				TStackRef<IJSONArray> columns;
				Check(value->GetArray(&columns));
				Check(RStrFormatW(&text,L"%u columns: double-click to edit",static_cast<UINT>(columns->Count())));
			}
			else if(row.type == QuadooProperty::Collection && name == RSTRING_CAST(L"Items") && CanExecute(TreeNodesEditor))
			{
				TStackRef<IJSONArray> nodes;
				Check(value->GetArray(&nodes));
				Check(RStrFormatW(&text,L"%u root nodes: double-click to edit",static_cast<UINT>(nodes->Count())));
			}

			else if(row.type == QuadooProperty::Icon || row.type == QuadooProperty::Picture)
				text = RSTRING_CAST(value ? L"Image: double-click to edit" : L"(none): double-click to browse");
			else
			{
				Check(ValueText(value,row.type,&text));
				if(value && choices && row.type != QuadooProperty::Flags)
				{
					LONGLONG number;
					if(row.type == QuadooProperty::Long)
					{
						Check(value->GetLongInteger(&number));
					}
					else
					{
						INT integer;
						Check(value->GetInteger(&integer));
						number = integer;
					}
					for(UINT choice = 0; choice < choices; choice++)
					{
						CRString label;
						LONGLONG candidate;
						Check(descriptor->GetEnumValue(choice,&label,&candidate));
						if(number == candidate)
						{
							text = label;
							break;
						}
					}
				}
			}
			if(row.type == QuadooProperty::ObjectReference)
			{
				GUID id;
				TStackRef<IQuadooComponent> reference;
				if(!value || (value->GetType() == JSON::String && text == RSTRING_CAST(L"\"\"")))
				{
					text = RSTRING_CAST(L"(none)");
				}
				if(value && SUCCEEDED(value->GetStringAsGuid(id)) && SUCCEEDED(m_document.FindComponent(id,&reference)))
				{
					text = NULL;
					Check(reference->GetName(&text));
				}
			}
			if(row.mixed)
			{
				text = RSTRING_CAST(L"(multiple values)");
			}
			row.name = name;
			Check(m_rows.Append(row)); name.Detach();
			if(row.flags & QuadooForm::PropertyReadOnly)
			{
				CRString note;
				Check(RStrFormatW(&note,L"%ls (layout/read-only)",text.c_wstr()));
				text = note;
			}
			LVITEMW item;
			ZeroMemory(&item,sizeof(item));
			item.mask = LVIF_TEXT;
			item.iItem = static_cast<INT>(m_rows.Length() - 1);
			item.pszText = const_cast<PWSTR>(RStrToWide(row.name));
			ListView_InsertItem(m_list,&item);
			ListView_SetItemText(m_list,item.iItem,1,const_cast<PWSTR>(text.c_wstr()));
		}
		if(!m_events)
		{
			if(SelectionCount() > 1)
			{
				CRString text;
				Check(RStrFormatW(&text,L"%u controls selected",SelectionCount()));
				m_status = text;
				goto Cleanup;
			}
			TStackRef<IQuadooControl> visual;
			hr = control->QueryInterface(__uuidof(IQuadooControl),reinterpret_cast<PVOID*>(&visual));
			if(hr == E_NOINTERFACE)
			{
				CRString name, type, text; Check(control->GetName(&name)); Check(control->GetTypeName(&type));
				Check(RStrFormatW(&text,L"%ls (%ls) | Nonvisual component",name.c_wstr(),type.c_wstr())); m_status = text;
				hr = S_OK;
			}
			else
			{
				Check(hr);
				QuadooForm::Rect requested, effective; CRString text;
				Check(visual->GetBounds(&requested)); Check(visual->GetEffectiveBounds(&effective));
				Check(RStrFormatW(&text,L"%d, %d; %d x %d",effective.Left,effective.Top,effective.Width,effective.Height));
				LVITEMW item;
				ZeroMemory(&item,sizeof(item));
				item.mask = LVIF_TEXT;
				item.iItem = static_cast<INT>(m_rows.Length());
				item.pszText = L"Effective bounds";
				ListView_InsertItem(m_list,&item);
				ListView_SetItemText(m_list,item.iItem,1,const_cast<PWSTR>(text.c_wstr()));
				CRString name; control->GetName(&name); text = static_cast<RSTRING>(NULL);
				if(SUCCEEDED(RStrFormatW(&text,L"%ls | Client %d x %d | Requested %d,%d %d x %d",name.c_wstr(),IntProperty(m_document.Root(),L"Width"),IntProperty(m_document.Root(),L"Height"),requested.Left,requested.Top,requested.Width,requested.Height)))
					m_status = text;
			}
			TStackRef<IQuadooDesignPlaceholder> placeholder;
			if(SUCCEEDED(control->QueryInterface(__uuidof(IQuadooDesignPlaceholder),reinterpret_cast<PVOID*>(&placeholder)))) { QuadooForm::PlaceholderReason reason; CRString detail; Check(placeholder->GetReason(&reason,&detail)); m_status = detail; }
		}
	Cleanup:
		SendMessageW(m_list,WM_SETREDRAW,TRUE,0);
		RedrawWindow(m_list,NULL,NULL,RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
		return hr;
	}

	LRESULT CDesignerEditor::DrawInspector (NMLVCUSTOMDRAW* draw)
	{
		if(draw->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
		if(draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) return CDRF_NOTIFYSUBITEMDRAW;
		if(draw->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM) && draw->iSubItem == 1 &&
			draw->nmcd.dwItemSpec < static_cast<UINT_PTR>(m_rows.Length()) && m_rows[draw->nmcd.dwItemSpec].type == QuadooProperty::Color &&
			!m_rows[draw->nmcd.dwItemSpec].mixed)
		{
			INT row = static_cast<INT>(draw->nmcd.dwItemSpec);
			RECT cell;
			HDC dc = draw->nmcd.hdc;
			if(!ListView_GetSubItemRect(m_list,row,1,LVIR_BOUNDS,&cell)) return CDRF_DODEFAULT;
			INT saved = SaveDC(dc);
			if(!saved) return CDRF_DODEFAULT;
			IntersectClipRect(dc,cell.left,cell.top,cell.right,cell.bottom);
			BOOL selected = (ListView_GetItemState(m_list,row,LVIS_SELECTED) & LVIS_SELECTED) != 0;
			BOOL focused = GetFocus() == m_list;
			INT background = selected ? (focused ? COLOR_HIGHLIGHT : COLOR_BTNFACE) : COLOR_WINDOW;
			INT foreground = selected && focused ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT;
			if(!IsWindowEnabled(m_list))
				foreground = COLOR_GRAYTEXT;
			FillRect(dc,&cell,GetSysColorBrush(background));
			INT padding = max(2,MulDiv(3,m_dpi,96));
			INT height = min(MulDiv(14,m_dpi,96),max(1,cell.bottom - cell.top - 2 * padding));
			RECT swatch = {cell.left + padding,cell.top + (cell.bottom - cell.top - height) / 2,cell.left + padding + MulDiv(22,m_dpi,96),0};
			swatch.bottom = swatch.top + height;
			SetDCBrushColor(dc,ResolveQuadooColor(m_rows[row].color));
			FillRect(dc,&swatch,static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
			FrameRect(dc,&swatch,GetSysColorBrush(COLOR_WINDOWTEXT));
			WCHAR text[128];
			ListView_GetItemText(m_list,row,1,text,ARRAYSIZE(text));
			cell.left = swatch.right + padding * 2;
			cell.right -= padding;
			SetBkMode(dc,TRANSPARENT);
			HFONT font = reinterpret_cast<HFONT>(SendMessageW(m_list,WM_GETFONT,0,0));
			if(font) SelectObject(dc,font);
			SetTextColor(dc,GetSysColor(foreground));
			DrawTextW(dc,text,-1,&cell,DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
			RestoreDC(dc,saved);
			return CDRF_SKIPDEFAULT;
		}
		return CDRF_DODEFAULT;
	}

	static BOOL InlineTextType(QuadooProperty::Type type)
	{
		return type == QuadooProperty::String || type == QuadooProperty::Event ||
			type == QuadooProperty::Int || type == QuadooProperty::Long ||
			type == QuadooProperty::Float || type == QuadooProperty::Enum;
	}

	static HRESULT InlineNumber(RSTRING text, IJSONValue** value)
	{
		HRESULT hr;
		CRString wrapped;
		TStackRef<IJSONValue> parsed;
		TStackRef<IJSONArray> array;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		// JSONParse accepts a value prefix. A protected one-element array
		// requires the entire edit to parse, including any trailing text.
		for(INT n = 0; n < RStrLen(text); n++)
		{
			WCHAR ch = RStrToWide(text)[n];
			CheckIf(ch == L'[' || ch == L']' || ch == L',',E_INVALIDARG);
		}
		Check(RStrFormatW(&wrapped,L"[%r]",text));
		Check(JSONParse(NULL,wrapped.c_wstr(),RStrLen(wrapped),&parsed));
		Check(parsed->GetArray(&array));
		CheckIf(array->Count() != 1,E_INVALIDARG);
		Check(array->GetValue(0,value));
		CheckIf(!*value,E_INVALIDARG);
	Cleanup:
		return hr;
	}

	HRESULT CDesignerEditor::BeginInline(UINT row, RSTRING initial)
	{
		HRESULT hr;
		CheckIf(row >= static_cast<UINT>(m_rows.Length()),E_INVALIDARG);
		if(m_inline && m_editDropdown)
		{
			DestroyWindow(m_inline);
		}
		m_editDropdown = FALSE;
		m_dropdownChoices.Clear();
		if(!m_inline)
		{
			m_inline = CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,0,0,1,1,m_list,reinterpret_cast<HMENU>(120),GetModuleHandle(NULL),NULL);
			CheckIf(!m_inline,HrEnsureLastError());
			if(!SetWindowSubclass(m_inline,InlineProc,1,reinterpret_cast<DWORD_PTR>(this)))
			{
				hr = HrEnsureLastError();
				DestroyWindow(m_inline);
				m_inline = NULL;
				Check(hr);
			}
			SendMessageW(m_inline,EM_SETLIMITTEXT,8 * 1024 * 1024,0);
		}
		Check(GetSelectedID(&m_editControl));
		m_editProperty = m_rows[row].name;
		m_editInitial = initial;
		ListView_EnsureVisible(m_list,row,FALSE);
		m_editRow = row;
		m_editing = TRUE;
		m_inlineComposing = FALSE;
		SendMessageW(m_inline,WM_SETFONT,SendMessageW(m_list,WM_GETFONT,0,0),FALSE);
		SetWindowTextW(m_inline,RStrToWide(initial));
		SendMessageW(m_inline,EM_SETMODIFY,FALSE,0);
		PositionInline();
		ShowWindow(m_inline,SW_SHOWNOACTIVATE);
		SetFocus(m_inline);
		SendMessageW(m_inline,EM_SETSEL,0,-1);
	Cleanup:
		return hr;
	}

	VOID CDesignerEditor::PositionInline()
	{
		if(m_editing && m_inline && m_editRow >= 0)
		{
			RECT rect;
			RECT client;
			GetClientRect(m_list,&client);
			if(ListView_GetSubItemRect(m_list,m_editRow,1,LVIR_BOUNDS,&rect))
			{
				rect.right = min(rect.right,client.right);
				rect.bottom = min(rect.bottom,client.bottom);
				INT height = max(1,rect.bottom - rect.top);
				if(m_editDropdown)
				{
					SendMessageW(m_inline,CB_SETITEMHEIGHT,static_cast<WPARAM>(-1),max(1,height - GetSystemMetrics(SM_CYEDGE) * 2));
					height *= min(9,max(2,static_cast<INT>(m_dropdownChoices.Length()) + 1));
				}
				SetWindowPos(m_inline,HWND_TOP,rect.left,rect.top,max(1,rect.right - rect.left),height,SWP_NOACTIVATE);
			}
		}
	}
	HRESULT CDesignerEditor::FinishInline(BOOL commit)
	{
		HRESULT hr = S_FALSE;
		TStackRef<IJSONValue> value;
		CRString text;
		CRString propertyName;
		UINT index;
		DWORD flags;
		BOOL changed = FALSE;
		BOOL finishing = FALSE;
		CheckIfIgnore(!m_editing || m_finishingEdit,S_FALSE);
		m_finishingEdit = TRUE;
		finishing = TRUE;
		propertyName = m_editProperty;
		if(commit)
		{
			if(m_editDropdown)
			{
				Check(DropdownValue(&value,&changed));
			}
			else
			{
				INT length = GetWindowTextLengthW(m_inline);
				PWSTR write;
				CheckIf(length > 8 * 1024 * 1024,E_INVALIDARG);
				Check(RStrAllocW(length,&text,&write));
				GetWindowTextW(m_inline,write,length + 1);
				changed = !(text == static_cast<RSTRING>(m_editInitial)) ||
					(m_editRow >= 0 && m_editRow < m_rows.Length() && m_rows[m_editRow].mixed && SendMessageW(m_inline,EM_GETMODIFY,0,0));
				if(changed)
				{
					CheckIf(m_editRow < 0 || m_editRow >= m_rows.Length(),E_UNEXPECTED);
					QuadooProperty::Type type = m_rows[m_editRow].type;
					if(type == QuadooProperty::String || type == QuadooProperty::Event)
					{
						Check(JSONCreateString(text,&value));
					}
					else
					{
						// Preserve JSON numeric precision and use the same descriptor
						// validation/selection transaction as other property editors.
						Check(InlineNumber(text,&value));
					}
				}
			}
			if(changed)
			{
				CSelectionProperties selection;
				Check(SelectionProperties(selection));
				Check(selection.Find(propertyName,&index,&flags));
				m_changeDepth++;
				hr = selection.SetValue(index,value);
				m_changeDepth--;
				Check(hr);
				changed = hr != S_FALSE;
			}
		}
		m_editing = FALSE;
		m_inlineComposing = FALSE;
		m_editRow = -1;
		ShowWindow(m_inline,SW_HIDE);
		if(changed || m_deferredRefresh)
		{
			Check(Refresh(TRUE));
			for(sysint n = 0; n < m_rows.Length(); n++)
			{
				if(propertyName == m_rows[n].name)
				{
					ListView_SetItemState(m_list,n,LVIS_FOCUSED | LVIS_SELECTED,LVIS_FOCUSED | LVIS_SELECTED);
					ListView_EnsureVisible(m_list,n,FALSE);
					break;
				}
			}
		}
	Cleanup:
		if(finishing)
		{
			m_finishingEdit = FALSE;
		}
		if(finishing && FAILED(hr) && m_editing)
		{
			CRString status;
			if(SUCCEEDED(RStrFormatW(&status,L"%ls rejected (0x%.8x). Correct the value or press Escape to cancel.",propertyName.c_wstr(),hr)))
			{
				m_status = status;
			}
			MessageBeep(MB_ICONERROR);
			PostMessageW(m_window,FocusInlineMessage,0,0);
			Notify();
		}
		return hr;
	}
	BOOL CDesignerEditor::TranslateEditorMessage(MSG* message, HWND outline)
	{
		Assert(NULL != message);

		if(message->message == WM_KEYDOWN)
		{
			if(m_editing && message->hwnd == m_inline)
			{
				// Host accelerators must not interpret Delete as deleting a component.
				// Save/Open/Preview first commit the field, then use the host's command.
				BOOL control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
				if(message->wParam == VK_F5 || (control && (message->wParam == 'S' || message->wParam == 'O')) ||
					(m_embedded && (message->wParam == VK_F12 || (control && (message->wParam == VK_F9 || message->wParam == VK_F4 || message->wParam == VK_TAB))))) return FAILED(FinishInline(TRUE));
				::TranslateMessage(message);
				DispatchMessageW(message);
				return TRUE;
			}
			if(!(GetKeyState(VK_MENU) & 0x8000) &&
				(message->hwnd == m_overlay || message->hwnd == m_canvas || message->hwnd == m_window || message->hwnd == m_tray ||
				(m_tree && message->hwnd == m_tree) || (outline && message->hwnd == outline)))
			{
				UINT command = 0;
				if((GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_SHIFT) & 0x8000))
				{
					if(message->wParam == 'X') command = Cut;
					else if(message->wParam == 'C') command = Copy;
					else if(message->wParam == 'V') command = Paste;
				}
				else if(message->wParam == VK_DELETE && !(GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_SHIFT) & 0x8000))
					command = DeleteSelection;
				if(command)
				{
					if(CanExecute(command)) { HRESULT hr = ExecuteCommand(command,GetAncestor(m_window,GA_ROOT)); if(FAILED(hr)) Error(hr,L"Command"); }
					return TRUE;
				}
			}
			BOOL control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
			BOOL shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
			if(control != shift && !(GetKeyState(VK_MENU) & 0x8000) &&
				(message->wParam == VK_LEFT || message->wParam == VK_RIGHT || message->wParam == VK_UP || message->wParam == VK_DOWN) &&
				(message->hwnd == m_overlay || message->hwnd == m_canvas || message->hwnd == m_window ||
				(m_tree && message->hwnd == m_tree) || (outline && message->hwnd == outline)))
			{
				if(m_readOnly)
					return TRUE;
				HRESULT hr = shift ? ResizeSelection(static_cast<UINT>(message->wParam)) : NudgeSelection(static_cast<UINT>(message->wParam));
				if(FAILED(hr))
				{
					CRString status;
					if(SUCCEEDED(RStrFormatW(&status,L"Unable to %ls the selected controls (0x%.8x).",shift ? L"resize" : L"nudge",hr)))
					{
						m_status = status;
					}
					MessageBeep(MB_ICONERROR);
					Notify();
				}
				return TRUE;
			}
		}
		return FALSE;
	}
	LRESULT CALLBACK CDesignerEditor::InlineProc(HWND window, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data)
	{
		CDesignerEditor* editor = reinterpret_cast<CDesignerEditor*>(data);
		switch(msg)
		{
		case WM_GETDLGCODE: return DefSubclassProc(window,msg,wp,lp) | DLGC_WANTALLKEYS;
		case WM_IME_STARTCOMPOSITION: editor->m_inlineComposing = TRUE; break;
		case WM_IME_ENDCOMPOSITION: editor->m_inlineComposing = FALSE; break;
		case WM_CHAR: if(!editor->m_inlineComposing && (wp == VK_RETURN || wp == VK_ESCAPE || wp == VK_TAB)) return 0; break;
		case WM_KEYDOWN:
			if(!editor->m_inlineComposing && (wp == VK_RETURN || wp == VK_ESCAPE || wp == VK_TAB || (wp == VK_F4 && !editor->m_editDropdown)))
			{
				INT row = editor->m_editRow;
				if(SUCCEEDED(editor->FinishInline(wp != VK_ESCAPE)))
				{
					SetFocus(editor->m_list);
					if(wp == VK_F4 && row >= 0) editor->EditRow(row,TRUE);
					else if(wp == VK_TAB)
					{
						INT step = GetKeyState(VK_SHIFT) & 0x8000 ? -1 : 1;
						for(INT next = row + step; next >= 0 && next < editor->m_rows.Length(); next += step)
						{
							if(!(editor->m_rows[next].flags & QuadooForm::PropertyReadOnly) &&
								(InlineTextType(editor->m_rows[next].type) || editor->m_rows[next].dropdown))
							{
								editor->EditRow(next);
								break;
							}
						}
					}
				}
				return 0;
			}
			break;
		case WM_KILLFOCUS: if(editor->m_editing && !editor->m_finishingEdit) PostMessageW(editor->m_window,CommitInlineMessage,0,0); break;
		case WM_NCDESTROY:
			RemoveWindowSubclass(window,InlineProc,id);
			if(editor->m_inline == window)
			{
				editor->m_inline = NULL;
				editor->m_editing = FALSE;
			}
			break;
		}
		return DefSubclassProc(window,msg,wp,lp);
	}
	LRESULT CALLBACK CDesignerEditor::InspectorProc(HWND window, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data)
	{
		CDesignerEditor* editor = reinterpret_cast<CDesignerEditor*>(data);
		if(msg == WM_COMMAND && reinterpret_cast<HWND>(lp) == editor->m_inline && editor->m_editDropdown && editor->m_editing && !editor->m_finishingEdit &&
			(HIWORD(wp) == CBN_SELENDOK || HIWORD(wp) == CBN_SELENDCANCEL))
		{
			if(SUCCEEDED(editor->FinishInline(HIWORD(wp) == CBN_SELENDOK)))
			{
				SetFocus(editor->m_list);
			}
			return 0;
		}
		if((msg == WM_HSCROLL || msg == WM_VSCROLL || msg == WM_MOUSEWHEEL) && FAILED(editor->FinishInline(TRUE))) return 0;
		LRESULT result = DefSubclassProc(window,msg,wp,lp);
		if(msg == WM_SIZE || msg == LVM_SETCOLUMNWIDTH) editor->PositionInline();
		if(msg == WM_NCDESTROY) RemoveWindowSubclass(window,InspectorProc,id);
		return result;
	}
	class CPropertyDialog
	{
		IQuadooPropertyDescriptor* m_descriptor;
		QuadooProperty::Type m_type;
		IJSONValue* m_initial;
		HWND m_edit, m_preview;
		TArray<HWND> m_checks;
		TArray<LONGLONG> m_choices;
		HICON m_icon;
		TStackRef<QFI::CImageResource> m_picture;
		CRString m_name;
		HRESULT m_error;
		BOOL m_combo;
		HRESULT Initialize(HWND window)
		{
			HRESULT hr; CRString text; UINT count;
			Check(m_descriptor->GetName(&m_name)); Check(m_descriptor->GetEnumCount(&count)); SetWindowTextW(window,m_name.c_wstr());
			CreateWindowW(L"STATIC",m_type == QuadooProperty::List ? L"JSON array of strings; empty strings and duplicates are preserved." : L"Edit the requested value. OK validates it before committing.",WS_CHILD | WS_VISIBLE,12,10,520,35,window,NULL,GetModuleHandle(NULL),NULL);
			if(m_type == QuadooProperty::Picture)
			{
				m_preview = CreateWindowW(L"STATIC",L"",WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,24,55,80,80,window,reinterpret_cast<HMENU>(12),GetModuleHandle(NULL),NULL);
				CreateWindowW(L"BUTTON",L"Browse...",WS_CHILD | WS_VISIBLE | WS_TABSTOP,120,60,130,28,window,reinterpret_cast<HMENU>(10),GetModuleHandle(NULL),NULL);
				CreateWindowW(L"BUTTON",L"Clear Picture",WS_CHILD | WS_VISIBLE | WS_TABSTOP,260,60,110,28,window,reinterpret_cast<HMENU>(11),GetModuleHandle(NULL),NULL);
				CreateWindowW(L"STATIC",L"ICO, PNG, JPEG or BMP. ICO keeps all its resolutions. JPEG/BMP pixels are embedded as PNG; the original file is no longer needed. JPEG metadata is not retained, and PNG may be larger.",WS_CHILD | WS_VISIBLE,12,145,510,75,window,NULL,GetModuleHandle(NULL),NULL);
				Check(JSONClone(m_initial,&result,TRUE));
				Check(QFI::CImageResource::Create(result,&m_picture));
			}
			else if(m_type == QuadooProperty::Icon)
			{
				m_preview = CreateWindowW(L"STATIC",L"",WS_CHILD | WS_VISIBLE | SS_ICON,24,55,64,64,window,NULL,GetModuleHandle(NULL),NULL);
				CreateWindowW(L"BUTTON",L"Browse ICO/PNG...",WS_CHILD | WS_VISIBLE | WS_TABSTOP,110,60,140,28,window,reinterpret_cast<HMENU>(10),GetModuleHandle(NULL),NULL);
				CreateWindowW(L"BUTTON",L"Remove",WS_CHILD | WS_VISIBLE | WS_TABSTOP,255,60,100,28,window,reinterpret_cast<HMENU>(11),GetModuleHandle(NULL),NULL);
				CreateWindowW(L"STATIC",L"Choose ICO or PNG. PNG images are converted in memory to ICO with transparency. Large images are reduced to fit 256 x 256; proportions are preserved.",WS_CHILD | WS_VISIBLE,12,140,510,55,window,NULL,GetModuleHandle(NULL),NULL);
				Check(JSONClone(m_initial,&result,TRUE)); Check(QuadooLoadIconValue(result,48,48,&m_icon)); SendMessageW(m_preview,STM_SETICON,reinterpret_cast<WPARAM>(m_icon),0);
			}
			else if(m_type == QuadooProperty::Flags && count && count <= 12)
			{
				INT current = 0; if(m_initial) Check(m_initial->GetInteger(&current));
				for(UINT n = 0; n < count; n++)
				{
					CRString name; LONGLONG number; Check(m_descriptor->GetEnumValue(n,&name,&number)); Check(m_choices.Append(number));
					HWND check = CreateWindowW(L"BUTTON",name.c_wstr(),WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,18 + (n / 6) * 240,48 + (n % 6) * 25,230,24,window,NULL,GetModuleHandle(NULL),NULL); CheckIf(!check,HrEnsureLastError()); Check(m_checks.Append(check)); SendMessageW(check,BM_SETCHECK,(current & number) == number ? BST_CHECKED : BST_UNCHECKED,0);
				}
			}
			else if(m_type == QuadooProperty::Boolean || (count && m_type != QuadooProperty::Flags))
			{
				m_combo = TRUE; m_edit = CreateWindowW(L"COMBOBOX",L"",WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | (m_type == QuadooProperty::Boolean ? CBS_DROPDOWNLIST : CBS_DROPDOWN),12,50,500,180,window,NULL,GetModuleHandle(NULL),NULL); CheckIf(!m_edit,HrEnsureLastError());
				if(m_type == QuadooProperty::Boolean) { bool value; Check(m_initial->GetBoolean(&value)); SendMessageW(m_edit,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"False")); SendMessageW(m_edit,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"True")); SendMessageW(m_edit,CB_SETCURSEL,value ? 1 : 0,0); }
				else
				{
					LONGLONG current = 0; if(m_type == QuadooProperty::Long) Check(m_initial->GetLongInteger(&current)); else { INT number; Check(m_initial->GetInteger(&number)); current = number; }
					for(UINT n = 0; n < count; n++) { CRString name; LONGLONG number; Check(m_descriptor->GetEnumValue(n,&name,&number)); Check(m_choices.Append(number)); SendMessageW(m_edit,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_wstr())); if(current == number) SendMessageW(m_edit,CB_SETCURSEL,n,0); }
					if(SendMessageW(m_edit,CB_GETCURSEL,0,0) < 0) { Check(ValueText(m_initial,m_type,&text)); SetWindowTextW(m_edit,text.c_wstr()); }
				}
			}
			else
			{
				BOOL complex = (m_type != QuadooProperty::Int && m_type != QuadooProperty::Long && m_type != QuadooProperty::String && m_type != QuadooProperty::Event && m_type != QuadooProperty::Float) || (m_type == QuadooProperty::String && m_name != RSTRING_CAST(L"Name"));
				m_edit = CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOVSCROLL | (complex ? ES_MULTILINE | ES_WANTRETURN | WS_VSCROLL : ES_AUTOHSCROLL),12,48,510,complex ? 175 : 28,window,NULL,GetModuleHandle(NULL),NULL); CheckIf(!m_edit,HrEnsureLastError()); SendMessageW(m_edit,EM_SETLIMITTEXT,8 * 1024 * 1024,0);
				Check(ValueText(m_initial,m_type,&text)); SetWindowTextW(m_edit,text.c_wstr());
			}
			CreateWindowW(L"BUTTON",L"OK",WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,325,240,90,28,window,reinterpret_cast<HMENU>(IDOK),GetModuleHandle(NULL),NULL);
			CreateWindowW(L"BUTTON",L"Cancel",WS_CHILD | WS_VISIBLE | WS_TABSTOP,430,240,90,28,window,reinterpret_cast<HMENU>(IDCANCEL),GetModuleHandle(NULL),NULL);
			for(HWND child = GetWindow(window,GW_CHILD); child; child = GetWindow(child,GW_HWNDNEXT)) SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
			if(m_edit) SetFocus(m_edit);
	Cleanup:
			return hr;
		}
		HRESULT Read()
		{
			HRESULT hr = S_OK;
			if(m_type == QuadooProperty::Icon || m_type == QuadooProperty::Picture) goto Cleanup;
			if(m_checks.Length())
			{
				INT value = 0; for(sysint n = 0; n < m_checks.Length(); n++) if(SendMessageW(m_checks[n],BM_GETCHECK,0,0) == BST_CHECKED) value |= static_cast<INT>(m_choices[n]); Check(JSONCreateInteger(value,&result));
			}
			else if(m_type == QuadooProperty::Boolean) { Check(JSONCreateBool(SendMessageW(m_edit,CB_GETCURSEL,0,0) == 1,&result)); }
			else
			{
				INT selected = m_combo ? static_cast<INT>(SendMessageW(m_edit,CB_GETCURSEL,0,0)) : -1;
				if(selected >= 0 && selected < m_choices.Length()) { if(m_type == QuadooProperty::Long) Check(JSONCreateLongInteger(m_choices[selected],&result)); else Check(JSONCreateInteger(static_cast<INT>(m_choices[selected]),&result)); }
				else
				{
					INT length = GetWindowTextLengthW(m_edit); CheckIf(length > 8 * 1024 * 1024,E_INVALIDARG); CRString text; PWSTR write; Check(RStrAllocW(length,&text,&write)); GetWindowTextW(m_edit,write,length + 1);
					if(m_type == QuadooProperty::String || m_type == QuadooProperty::Event || m_type == QuadooProperty::Memo)
					{
						Check(JSONCreateString(text,&result));
					}
					else { Check(JSONParse(NULL,text.c_wstr(),RStrLen(text),&result)); }
				}
			}
		Cleanup:
			return hr;
		}
		HRESULT Browse(HWND window)
		{
			HRESULT hr = S_FALSE; CChooseFile chooser; HICON icon = NULL; TStackRef<IJSONValue> value;
			Check(chooser.Initialize());
			if(m_type == QuadooProperty::Picture)
			{
				TStackRef<QFI::CImageResource> picture;
				if(!chooser.OpenSingleFile(window,L"Choose Picture",L"Images (*.ico;*.png;*.jpg;*.jpeg;*.bmp)\0*.ico;*.png;*.jpg;*.jpeg;*.bmp\0\0")) goto Cleanup;
				Check(QFI::ImportFile(chooser.GetFile(0),&value));
				Check(QFI::CImageResource::Create(value,&picture));
				result = value; m_picture = picture;
				InvalidateRect(m_preview,NULL,FALSE);
				hr = S_OK;
				goto Cleanup;
			}
			if(!chooser.OpenSingleFile(window,L"Choose a form icon",L"Icon images (*.ico;*.png)\0*.ico;*.png\0ICO files (*.ico)\0*.ico\0PNG images (*.png)\0*.png\0\0")) { hr = S_FALSE; goto Cleanup; }
			Check(LoadIconFile(chooser.GetFile(0),&value));
			Check(QuadooLoadIconValue(value,48,48,&icon));
			result = value;
			SendMessageW(m_preview,STM_SETICON,reinterpret_cast<WPARAM>(icon),0); if(m_icon) DestroyIcon(m_icon); m_icon = icon; icon = NULL;
		Cleanup:
			if(icon) DestroyIcon(icon);
			return hr;
		}
		static INT_PTR CALLBACK Procedure(HWND window, UINT msg, WPARAM wp, LPARAM lp)
		{
			CPropertyDialog* data = reinterpret_cast<CPropertyDialog*>(GetWindowLongPtrW(window,DWLP_USER));
			if(msg == WM_INITDIALOG) { data = reinterpret_cast<CPropertyDialog*>(lp); SetWindowLongPtrW(window,DWLP_USER,lp); data->m_error = data->Initialize(window); if(FAILED(data->m_error)) EndDialog(window,IDCANCEL); return FALSE; }
			if(!data) return FALSE;
			if(msg == WM_DRAWITEM && wp == 12 && data->m_type == QuadooProperty::Picture)
			{
				DRAWITEMSTRUCT* item = reinterpret_cast<DRAWITEMSTRUCT*>(lp);
				for(INT y = item->rcItem.top; y < item->rcItem.bottom; y += 8)
					for(INT x = item->rcItem.left; x < item->rcItem.right; x += 8)
					{
						RECT tile = {x,y,min(x + 8,item->rcItem.right),min(y + 8,item->rcItem.bottom)};
						SetDCBrushColor(item->hDC,((x / 8 + y / 8) & 1) ? RGB(210,210,210) : RGB(255,255,255));
						FillRect(item->hDC,&tile,static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
					}
				if(data->m_picture) data->m_picture->Paint(item->hDC,&item->rcItem,96,QFI::Contain,4,QFI::HighQuality,255);
				return TRUE;
			}
			if(msg == WM_CLOSE)
			{
				data->m_error = S_OK;
				EndDialog(window,IDCANCEL);
				return TRUE;
			}
			if(msg == WM_COMMAND)
			{
				if(LOWORD(wp) == IDCANCEL) { data->m_error = S_OK; EndDialog(window,IDCANCEL); return TRUE; }
				if(LOWORD(wp) == IDOK)
				{
					data->m_error = data->Read();
					if(SUCCEEDED(data->m_error))
						EndDialog(window,IDOK);
					else
					{
						data->result.Release();
						MessageBoxW(window,L"Enter a valid value. Collections and compound values use JSON.",L"Invalid value",MB_OK | MB_ICONERROR);
					}
					return TRUE;
				}
				if(LOWORD(wp) == 10)
				{
					HRESULT hr = data->Browse(window);
					if(FAILED(hr))
					{
						if(data->m_type == QuadooProperty::Picture)
						{
							CRString text;
							if(SUCCEEDED(RStrFormatW(&text,L"The picture could not be decoded (0x%.8x). The previous picture is preserved.\r\n\r\nChoose an ICO, PNG, JPEG or BMP up to 8 MiB, 4096 pixels per dimension and 4 million decoded pixels. The normalized PNG must also fit 8 MiB.",hr))) MessageBoxW(window,text.c_wstr(),L"Invalid Picture",MB_OK | MB_ICONERROR);
							return TRUE;
						}
						PCWSTR reason = hr == E_OUTOFMEMORY ? L"There is not enough memory to convert this image." : L"The image could not be read or converted to an icon.";
						CRString message;
						if(SUCCEEDED(RStrFormatW(&message,L"%ls\r\n\r\nChoose a valid ICO (up to 1 MiB) or PNG (up to 8 MiB, 4 million pixels, and 4096 pixels per dimension).\r\n\r\nThe current icon has been preserved. Error 0x%.8x.",reason,hr)))
							MessageBoxW(window,message.c_wstr(),L"Invalid icon",MB_OK | MB_ICONERROR);
						else
							MessageBoxW(window,L"The image could not be converted. The current icon has been preserved.",L"Invalid icon",MB_OK | MB_ICONERROR);
					}
					return TRUE;
				}
				if(LOWORD(wp) == 11)
				{
					if(data->m_type == QuadooProperty::Picture) { data->m_picture.Release(); InvalidateRect(data->m_preview,NULL,FALSE); }
					else { SendMessageW(data->m_preview,STM_SETICON,0,0); if(data->m_icon) DestroyIcon(data->m_icon); data->m_icon = NULL; }
					data->result.Release();
					return TRUE;
				}
			}
			return FALSE;
		}
	public:
		TStackRef<IJSONValue> result;
		CPropertyDialog(IQuadooPropertyDescriptor* descriptor, QuadooProperty::Type type, IJSONValue* initial) : m_descriptor(descriptor), m_type(type), m_initial(initial), m_edit(NULL), m_preview(NULL), m_icon(NULL), m_error(S_OK), m_combo(FALSE) {}
		~CPropertyDialog() { if(m_icon) DestroyIcon(m_icon); }
		HRESULT Show(HWND owner)
		{
			#pragma pack(push,2)
			struct EmptyDialog { DLGTEMPLATE dialog; WORD menu, windowClass, title; };
			#pragma pack(pop)
			EmptyDialog definition; ZeroMemory(&definition,sizeof(definition)); definition.dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_CENTER; definition.dialog.cx = 365; definition.dialog.cy = 190;
			INT_PTR answer = DialogBoxIndirectParamW(GetModuleHandle(NULL),&definition.dialog,owner,Procedure,reinterpret_cast<LPARAM>(this));
			if(answer == -1) return HrEnsureLastError(); if(FAILED(m_error)) return m_error; return answer == IDOK ? S_OK : S_FALSE;
		}
	};

	VOID CDesignerEditor::EditRow(UINT row, BOOL dialog)
	{
		HRESULT hr;
		CSelectionProperties properties;
		TStackRef<IQuadooPropertyDescriptor> descriptor;
		TStackRef<IJSONValue> initial, value;
		BOOL mixed;
		CheckIfIgnore(m_readOnly || row >= static_cast<UINT>(m_rows.Length()) || (m_rows[row].flags & QuadooForm::PropertyReadOnly),S_FALSE);
		Check(FinishInline(TRUE));
		Check(SelectionProperties(properties));
		Check(properties.GetDescriptor(m_rows[row].index,&descriptor));
		Check(properties.GetValue(m_rows[row].index,&initial,&mixed));
		if(m_rows[row].type == QuadooProperty::Collection)
		{
			// Row names are retained RSTRING handles, not interned literals.
			CRString name = m_rows[row].name;
			if(name == RSTRING_CAST(L"Panels") && CanExecute(AddStatusPanel))
			{
				TStackRef<IQuadooComponent> component;
				TStackRef<IQuadooStatusBar> bar;
				TStackRef<IQuadooStatusPanels> panels;
				TStackRef<IQuadooComponent> group;
				GUID id;
				Check(SelectedComponent(&component));
				Check(component->QueryInterface(&bar));
				Check(bar->GetPanels(&panels));
				Check(panels->QueryInterface(&group));
				Check(group->GetID(&id));
				Check(SelectID(id));
				goto Cleanup;
			}
			if(name == RSTRING_CAST(L"Images") && CanExecute(ImageListEditor))
			{
				Check(EditImageList());
				goto Cleanup;
			}
			if(name == RSTRING_CAST(L"Items") && CanExecute(TreeNodesEditor))
			{
				Check(EditTreeNodes());
				goto Cleanup;
			}
			if(name == RSTRING_CAST(L"Columns") && CanExecute(ListColumnsEditor))
			{
				Check(EditListColumns());
				goto Cleanup;
			}
		}
		if(m_rows[row].type == QuadooProperty::ObjectReference)
		{
			Check(BeginReference(row,initial));
			goto Cleanup;
		}
		if(m_rows[row].dropdown)
		{
			Check(BeginChoices(row,descriptor,initial));
			goto Cleanup;
		}
		if(InlineTextType(m_rows[row].type) && !dialog)
		{
			CRString text;
			BOOL multiline = FALSE;
			if(m_rows[row].mixed)
			{
				text = RSTRING_CAST(L"");
			}
			else
			{
				Check(ValueText(initial,m_rows[row].type,&text));
			}
			for(INT n = 0; n < RStrLen(text); n++)
			{
				if(text.c_wstr()[n] == L'\r' || text.c_wstr()[n] == L'\n')
				{
					multiline = TRUE;
				}
			}
			if(!multiline)
			{
				Check(BeginInline(row,text));
				goto Cleanup;
			}
		}
		if(m_rows[row].type == QuadooProperty::Color)
		{
			INT number;
			COLORREF color;
			Check(initial->GetInteger(&number));
			hr = ShowQuadooColorPicker(m_window,number,&color);
			CheckIfIgnore(hr == S_FALSE,S_FALSE);
			Check(hr);
			Check(JSONCreateInteger(static_cast<INT>(color),&value));
		}
		else if(m_rows[row].type == QuadooProperty::Font)
		{
			TStackRef<IJSONObject> object; TStackRef<IJSONValue> part; CRString name; LOGFONTW font; CHOOSEFONTW choose; INT points = 9; bool bold = false, italic = false;
			ZeroMemory(&font,sizeof(font)); ZeroMemory(&choose,sizeof(choose)); Check(initial->GetObject(&object)); Check(object->FindNonNullValueW(L"Name",&part)); Check(part->GetString(&name)); part.Release(); Check(TStrCchCpy(font.lfFaceName,ARRAYSIZE(font.lfFaceName),name.c_wstr()));
			Check(object->FindNonNullValueW(L"Size",&part)); Check(part->GetInteger(&points)); part.Release(); Check(object->FindNonNullValueW(L"Bold",&part)); Check(part->GetBoolean(&bold)); part.Release(); Check(object->FindNonNullValueW(L"Italic",&part)); Check(part->GetBoolean(&italic)); part.Release();
			font.lfHeight = -MulDiv(points,m_dpi,72); font.lfWeight = bold ? FW_BOLD : FW_NORMAL; font.lfItalic = italic; choose.lStructSize = sizeof(choose); choose.hwndOwner = m_window; choose.lpLogFont = &font; choose.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT;
			CheckIfIgnore(!ChooseFontW(&choose),S_FALSE); object.Release(); Check(JSONCreateObject(&object)); name = static_cast<RSTRING>(NULL); Check(RStrCreateW(TStrLenAssert(font.lfFaceName),font.lfFaceName,&name)); Check(PutString(object,L"Name",name)); Check(PutInt(object,L"Size",(choose.iPointSize + 5) / 10)); Check(JSONCreateBool(font.lfWeight >= FW_BOLD,&part)); Check(object->AddValueW(L"Bold",part)); part.Release(); Check(JSONCreateBool(font.lfItalic != 0,&part)); Check(object->AddValueW(L"Italic",part)); Check(JSONWrapObject(object,&value));
		}
		else
		{
			// Descriptor values already provide independent JSON snapshots for lists
			// and other collections, including those supplied by external factories.
			CPropertyDialog dialog(descriptor,m_rows[row].type,initial); hr = dialog.Show(m_window); if(hr != S_OK) goto Cleanup; value = dialog.result;
		}
		Check(SetSelectedProperty(m_rows[row].index,value));
	Cleanup:
		if(FAILED(hr) && !m_editing) Error(hr,L"Edit property");
	}
}
