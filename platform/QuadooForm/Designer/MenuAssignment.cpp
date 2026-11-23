#include "Designer.h"

namespace QFD
{
	HRESULT CDesignerEditor::AddDropdownChoice(HWND combo, TArray<DropdownChoice>& choices, REFGUID reference, LONGLONG number, PCWSTR name)
	{
		HRESULT hr;
		DropdownChoice choice;
		choice.reference = reference;
		choice.number = number;
		Check(choices.Append(choice));
		CheckIf(SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name)) < 0,E_OUTOFMEMORY);
	Cleanup:
		return hr;
	}
	HRESULT CDesignerEditor::BeginReference(UINT row, IJSONValue* initial)
	{
		HRESULT hr;
		HWND combo = NULL;
		TArray<DropdownChoice> choices;
		CRString property;
		CRString original;
		GUID identity = GUID_NULL;
		INT selected = 0;
		BOOL popupProperty;
		BOOL matched = !initial;
		CheckIf(row >= static_cast<UINT>(m_rows.Length()),E_INVALIDARG);
		property = m_rows[row].name;
		popupProperty = property == RSTRING_CAST(L"PopupMenu") ? TRUE : FALSE;
		if(initial && initial->GetType() == JSON::String)
		{
			Check(initial->GetString(&original));
			matched = !original.Length();
			initial->GetStringAsGuid(identity);
		}
		combo = CreateWindowExW(0,L"COMBOBOX",L"",WS_CHILD | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST | CBS_NOINTEGRALHEIGHT,
			0,0,1,200,m_list,reinterpret_cast<HMENU>(120),GetModuleHandle(NULL),NULL);
		CheckIf(!combo,HrEnsureLastError());
		Check(AddDropdownChoice(combo,choices,GUID_NULL,0,L"(none)"));
		for(sysint n = 0; n < m_nodes.Length(); n++)
		{
			IQuadooComponent* candidate = m_nodes[n].component;
			QuadooForm::ComponentState state;
			Check(candidate->GetState(&state));
			if(state >= QuadooForm::Destroying)
			{
				continue;
			}
			if(property == RSTRING_CAST(L"MainMenu") || property == RSTRING_CAST(L"PopupMenu"))
			{
				TStackRef<IQuadooMenu> menu;
				HRESULT query = candidate->QueryInterface(__uuidof(IQuadooMenu),reinterpret_cast<PVOID*>(&menu));
				if(query == E_NOINTERFACE)
				{
					continue;
				}
				BOOL popup;
				Check(query);
				CheckIf(!menu,E_UNEXPECTED);
				Check(menu->IsPopup(&popup));
				if(popup != popupProperty)
				{
					continue;
				}
			}
			if(property == RSTRING_CAST(L"Images") || property == RSTRING_CAST(L"LargeImages") ||
				property == RSTRING_CAST(L"SmallImages") || property == RSTRING_CAST(L"StateImages"))
			{
				TStackRef<IQuadooImageList> images;
				HRESULT query = candidate->QueryInterface(__uuidof(IQuadooImageList),reinterpret_cast<PVOID*>(&images));
				if(query == E_NOINTERFACE)
				{
					continue;
				}
				Check(query);
				CheckIf(!images,E_UNEXPECTED);
			}

			CRString name;
			GUID id;
			Check(candidate->GetName(&name));
			Check(candidate->GetID(&id));
			Check(AddDropdownChoice(combo,choices,id,0,name.c_wstr()));
			if(IsEqualGUID(id,identity) || (original.Length() && original == static_cast<RSTRING>(name)))
			{
				selected = static_cast<INT>(choices.Length() - 1);
				matched = TRUE;
			}
		}
		if(!matched)
		{
			// Preserve an unresolved or opaque existing value until another choice
			// is explicitly accepted. This also preserves legacy references by name.
			CRString label;
			if(original.Length())
			{
				Check(RStrFormatW(&label,L"(unresolved: %r)",static_cast<RSTRING>(original)));
			}
			else
			{
				label = RSTRING_CAST(L"(current value)");
			}
			Check(AddDropdownChoice(combo,choices,GUID_NULL,0,label.c_wstr()));
			selected = static_cast<INT>(choices.Length() - 1);
		}
		Check(BeginDropdown(row,&combo,selected,choices));
	Cleanup:
		if(combo)
		{
			DestroyWindow(combo);
		}
		return hr;
	}
	HRESULT CDesignerEditor::BeginChoices(UINT row, IQuadooPropertyDescriptor* descriptor, IJSONValue* initial)
	{
		HRESULT hr;
		HWND combo = NULL;
		TArray<DropdownChoice> choices;
		QuadooProperty::Type type;
		LONGLONG current;
		INT selected = -1;
		UINT count;
		CheckIf(row >= static_cast<UINT>(m_rows.Length()) || !descriptor || !initial,E_INVALIDARG);
		type = m_rows[row].type;
		if(type == QuadooProperty::Boolean)
		{
			bool value;
			Check(initial->GetBoolean(&value));
			current = value ? 1 : 0;
			count = 2;
		}
		else
		{
			CheckIf(type != QuadooProperty::Enum && type != QuadooProperty::Int && type != QuadooProperty::Long,E_INVALIDARG);
			if(type == QuadooProperty::Long)
			{
				Check(initial->GetLongInteger(&current));
			}
			else
			{
				INT value;
				Check(initial->GetInteger(&value));
				current = value;
			}
			Check(descriptor->GetEnumCount(&count));
			CheckIf(!count || count > 4096,E_INVALIDARG);
		}
		combo = CreateWindowExW(0,L"COMBOBOX",L"",WS_CHILD | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST | CBS_NOINTEGRALHEIGHT,
			0,0,1,200,m_list,reinterpret_cast<HMENU>(120),GetModuleHandle(NULL),NULL);
		CheckIf(!combo,HrEnsureLastError());
		for(UINT n = 0; n < count; n++)
		{
			CRString label;
			LONGLONG number;
			if(type == QuadooProperty::Boolean)
			{
				label = n ? RSTRING_CAST(L"True") : RSTRING_CAST(L"False");
				number = n;
			}
			else
			{
				Check(descriptor->GetEnumValue(n,&label,&number));
			}
			Check(AddDropdownChoice(combo,choices,GUID_NULL,number,label.c_wstr()));
			if(number == current)
			{
				selected = n;
			}
		}
		if(selected < 0)
		{
			// Opening an inspector must not replace a value absent from its choices.
			CRString label;
			Check(RStrFormatW(&label,L"(current: %I64d)",current));
			Check(AddDropdownChoice(combo,choices,GUID_NULL,current,label.c_wstr()));
			selected = static_cast<INT>(choices.Length() - 1);
		}
		Check(BeginDropdown(row,&combo,selected,choices));
	Cleanup:
		if(combo)
		{
			DestroyWindow(combo);
		}
		return hr;
	}
	HRESULT CDesignerEditor::BeginDropdown(UINT row, HWND* combo, INT selected, TArray<DropdownChoice>& choices)
	{
		HRESULT hr;
		GUID control;
		CRString property;
		CheckIf(row >= static_cast<UINT>(m_rows.Length()) || !combo || !*combo || selected < 0 || selected >= choices.Length(),E_INVALIDARG);
		property = m_rows[row].name;
		Check(GetSelectedID(&control));
		if(m_rows[row].mixed)
		{
			Check(AddDropdownChoice(*combo,choices,GUID_NULL,0,L"(multiple values)"));
			selected = static_cast<INT>(choices.Length() - 1);
		}
		CheckIf(!SetWindowSubclass(*combo,InlineProc,1,reinterpret_cast<DWORD_PTR>(this)),HrEnsureLastError());
		SendMessageW(*combo,WM_SETFONT,SendMessageW(m_list,WM_GETFONT,0,0),FALSE);
		SendMessageW(*combo,CB_SETCURSEL,selected,0);
		if(m_inline)
		{
			DestroyWindow(m_inline);
		}
		m_inline = *combo;
		*combo = NULL;
		m_editDropdown = TRUE;
		m_dropdownType = m_rows[row].type;
		m_dropdownInitial = selected;
		m_dropdownChoices.Swap(choices);
		m_editControl = control;
		m_editProperty = property;
		m_editInitial = NULL;
		m_editRow = row;
		m_editing = TRUE;
		m_inlineComposing = FALSE;
		ListView_EnsureVisible(m_list,row,FALSE);
		ListView_SetItemState(m_list,row,LVIS_SELECTED | LVIS_FOCUSED,LVIS_SELECTED | LVIS_FOCUSED);
		PositionInline();
		ShowWindow(m_inline,SW_SHOWNOACTIVATE);
		SetFocus(m_inline);
		SendMessageW(m_inline,CB_SHOWDROPDOWN,TRUE,0);
	Cleanup:
		return hr;
	}
	HRESULT CDesignerEditor::DropdownValue(IJSONValue** value, BOOL* changed)
	{
		HRESULT hr = S_OK;
		INT selected = static_cast<INT>(SendMessageW(m_inline,CB_GETCURSEL,0,0));
		CheckIf(!value || !changed,E_POINTER);
		*value = NULL;
		*changed = selected != m_dropdownInitial;
		CheckIf(selected < 0 || selected >= m_dropdownChoices.Length(),E_INVALIDARG);
		CheckIfIgnore(!*changed,S_OK);
		if(m_dropdownType == QuadooProperty::ObjectReference)
		{
			WCHAR text[40];
			INT length;
			CheckIfIgnore(!selected,S_OK);
			CheckIf(IsEqualGUID(m_dropdownChoices[selected].reference,GUID_NULL),E_INVALIDARG);
			length = StringFromGUID2(m_dropdownChoices[selected].reference,text,ARRAYSIZE(text));
			CheckIf(!length,E_FAIL);
			Check(JSONCreateStringW(text,length - 1,value));
		}
		else if(m_dropdownType == QuadooProperty::Boolean)
		{
			Check(JSONCreateBool(m_dropdownChoices[selected].number != 0,value));
		}
		else if(m_dropdownType == QuadooProperty::Long)
		{
			Check(JSONCreateLongInteger(m_dropdownChoices[selected].number,value));
		}
		else
		{
			CheckIf(m_dropdownChoices[selected].number < INT_MIN || m_dropdownChoices[selected].number > INT_MAX,E_INVALIDARG);
			Check(JSONCreateInteger(static_cast<INT>(m_dropdownChoices[selected].number),value));
		}
	Cleanup:
		return hr;
	}
}
