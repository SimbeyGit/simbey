// Exercise choice editors through the inspector's normal list-view notifications.
static HRESULT ChoiceDropdown(QFD::CDesignerEditor* editor, PCWSTR property, UINT count, HWND* combo)
{
	HRESULT hr = S_OK;
	WCHAR type[32];
	*combo = StartInline(editor,property);
	REQUIRE(*combo && IsWindowVisible(*combo));
	GetClassNameW(*combo,type,ARRAYSIZE(type));
	REQUIRE(!lstrcmpiW(type,L"ComboBox"));
	REQUIRE((GetWindowLongW(*combo,GWL_STYLE) & CBS_DROPDOWNLIST) == CBS_DROPDOWNLIST);
	REQUIRE(SendMessageW(*combo,CB_GETCOUNT,0,0) == count);
Cleanup:
	return hr;
}
static HRESULT ChooseProperty(QFD::CDesignerEditor* editor, PCWSTR property, UINT count, INT selection)
{
	HRESULT hr;
	HWND combo;
	Check(ChoiceDropdown(editor,property,count,&combo));
	REQUIRE(SendMessageW(combo,CB_SETCURSEL,selection,0) == selection);
	SendMessageW(GetParent(combo),WM_COMMAND,MAKEWPARAM(120,CBN_SELENDOK),reinterpret_cast<LPARAM>(combo));
	REQUIRE(!IsWindowVisible(combo));
Cleanup:
	return hr;
}
static HRESULT InlineChoices()
{
	HRESULT hr;
	HWND host = Host();
	HWND combo;
	TStackRef<QFD::CDesignerEditor> editor;
	TStackRef<IQuadooProperties> properties;
	TStackRef<IQuadooPropertyDescriptors> descriptors;
	TStackRef<IJSONObject> before;
	TStackRef<IJSONObject> after;
	TStackRef<IJSONObject> saved;
	TStackRef<IJSONValue> value;
	UINT count;
	UINT index;
	WCHAR label[128];
	CheckIf(!host,HrEnsureLastError());
	Check(CreateEditor(host,&editor));
	Check(editor->NewDocument(TRUE));
	Check(editor->Document().Serialize(&before));
	editor->Document().SetDirty(FALSE);
	Check(editor->Document().Root()->GetProperties(&properties));
	Check(properties->GetDescriptors(&descriptors));
	Check(descriptors->GetCount(&count));
	// Every editable Boolean/enum descriptor uses its labels and current value.
	for(UINT n = 0; n < count; n++)
	{
		TStackRef<IQuadooPropertyDescriptor> descriptor;
		CRString name;
		QuadooProperty::Type type;
		DWORD flags;
		UINT choices;
		INT current;
		Check(descriptors->GetAt(n,&descriptor));
		Check(descriptor->GetType(&type));
		Check(descriptor->GetFlags(&flags));
		Check(descriptor->GetEnumCount(&choices));
		if((type != QuadooProperty::Boolean && (type != QuadooProperty::Enum || !choices)) ||
			(flags & (QuadooForm::PropertyReadOnly | QuadooForm::PropertyHidden | QuadooForm::PropertyRuntimeOnly)))
		{
			continue;
		}
		Check(descriptor->GetName(&name));
		Check(properties->GetValueJSON(n,&value));
		if(type == QuadooProperty::Boolean)
		{
			bool flag;
			Check(value->GetBoolean(&flag));
			current = flag ? 1 : 0;
			choices = 2;
		}
		else
		{
			Check(value->GetInteger(&current));
		}
		value.Release();
		Check(ChoiceDropdown(editor,name.c_wstr(),choices,&combo));
		INT selected = static_cast<INT>(SendMessageW(combo,CB_GETCURSEL,0,0));
		REQUIRE(selected >= 0 && selected < static_cast<INT>(choices));
		for(UINT choice = 0; choice < choices; choice++)
		{
			CRString expected;
			LONGLONG number;
			if(type == QuadooProperty::Boolean)
			{
				expected = choice ? RSTRING_CAST(L"True") : RSTRING_CAST(L"False");
				number = choice;
			}
			else
			{
				Check(descriptor->GetEnumValue(choice,&expected,&number));
			}
			REQUIRE(SendMessageW(combo,CB_GETLBTEXTLEN,choice,0) < ARRAYSIZE(label));
			SendMessageW(combo,CB_GETLBTEXT,choice,reinterpret_cast<LPARAM>(label));
			REQUIRE(!lstrcmpW(label,expected.c_wstr()));
			if(choice == static_cast<UINT>(selected))
			{
				REQUIRE(number == current);
			}
		}
		// Merely accepting the existing selection must not dirty the document.
		SendMessageW(combo,WM_KEYDOWN,VK_RETURN,0);
		REQUIRE(!editor->Document().Dirty());
	}
	Check(editor->Document().Serialize(&after));
	REQUIRE(JSONCompareObjects(before,after) == 0);
	after.Release();
	Check(ChooseProperty(editor,L"BorderStyle",4,1));
	REQUIRE(QFD::IntProperty(editor->Document().Root(),L"BorderStyle") == QuadooForm::BorderSingle);
	Check(ChooseProperty(editor,L"WindowState",3,2));
	REQUIRE(QFD::IntProperty(editor->Document().Root(),L"WindowState") == QuadooForm::WindowMaximized);
	Check(ChooseProperty(editor,L"FormStyle",2,1));
	REQUIRE(QFD::IntProperty(editor->Document().Root(),L"FormStyle") == QuadooForm::FormStayOnTop);
	Check(ChooseProperty(editor,L"KeyPreview",2,1));
	REQUIRE(QFD::BoolProperty(editor->Document().Root(),L"KeyPreview"));
	editor->Document().SetDirty(FALSE);
	Check(ChoiceDropdown(editor,L"KeyPreview",2,&combo));
	SendMessageW(combo,CB_SETCURSEL,0,0);
	SendMessageW(combo,WM_KEYDOWN,VK_ESCAPE,0);
	REQUIRE(QFD::BoolProperty(editor->Document().Root(),L"KeyPreview") && !editor->Document().Dirty());
	Check(ChoiceDropdown(editor,L"FormStyle",2,&combo));
	SendMessageW(combo,CB_SETCURSEL,0,0);
	SendMessageW(GetParent(combo),WM_COMMAND,MAKEWPARAM(120,CBN_SELENDCANCEL),reinterpret_cast<LPARAM>(combo));
	REQUIRE(QFD::IntProperty(editor->Document().Root(),L"FormStyle") == QuadooForm::FormStayOnTop && !editor->Document().Dirty());
	Check(ChoiceDropdown(editor,L"BorderStyle",4,&combo));
	SendMessageW(combo,CB_SETCURSEL,static_cast<WPARAM>(-1),0);
	REQUIRE(editor->CommitPropertyEdit() == E_INVALIDARG && IsWindowVisible(combo));
	SendMessageW(combo,WM_KEYDOWN,VK_ESCAPE,0);
	REQUIRE(!editor->Document().Dirty());
	Check(ChoiceDropdown(editor,L"KeyPreview",2,&combo));
	SendMessageW(combo,CB_SETCURSEL,0,0);
	Check(editor->CommitPropertyEdit());
	REQUIRE(!QFD::BoolProperty(editor->Document().Root(),L"KeyPreview",TRUE));
	// A dropdown must never be reused as a free-text EDIT or vice versa.
	combo = StartInline(editor,L"Caption");
	REQUIRE(combo != NULL);
	GetClassNameW(combo,label,ARRAYSIZE(label));
	REQUIRE(!lstrcmpiW(label,L"Edit"));
	SetWindowTextW(combo,L"Inline choices");
	SendMessageW(combo,WM_KEYDOWN,VK_RETURN,0);
	Check(ChoiceDropdown(editor,L"BorderStyle",4,&combo));
	SendMessageW(combo,WM_KEYDOWN,VK_ESCAPE,0);
	Check(editor->Document().Serialize(&saved));
	Check(editor->LoadDefinition(saved));
	Check(editor->Document().Serialize(&after));
	REQUIRE(JSONCompareObjects(saved,after) == 0);
	properties.Release();
	Check(QFD::Property(editor->Document().Root(),L"KeyPreview",&properties,&index));
	Check(properties->GetValueJSON(index,&value));
	REQUIRE(value && value->GetType() == JSON::Boolean);
	value.Release();
	properties.Release();
	Check(QFD::Property(editor->Document().Root(),L"BorderStyle",&properties,&index));
	Check(properties->GetValueJSON(index,&value));
	REQUIRE(value && value->GetType() == JSON::Integer);
Cleanup:
	editor.Release();
	if(host)
	{
		DestroyWindow(host);
	}
	return hr;
}
