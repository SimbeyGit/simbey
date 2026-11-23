class CStoredListEvents : public CBaseUnknown, public IQuadooEventsBinder, public IQuadooDelegate
{
public:
	UINT inserted;
	UINT deleted;
	UINT changed;
	UINT selected;
	UINT checked;
	UINT comparing;
	UINT popups;
	INT popupColumn;
	INT oldState;
	INT newState;
	INT mask;
	BOOL cancel;
	BOOL replaceCheck;
	BOOL replaceText;
	BOOL destroy;
	BOOL reenter;
	BOOL readData;
	BOOL customCompare;
	HRESULT nested;
	TStackRef<IQuadooListItem> last;
	IQuadooListItems* items;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooEventsBinder)
		UNK_INTERFACE(IQuadooDelegate)
	END_UNK_MAP
	CStoredListEvents() : inserted(0), deleted(0), changed(0), selected(0), checked(0), comparing(0), popups(0),
		popupColumn(-1), oldState(0), newState(0), mask(0),
		cancel(FALSE), replaceCheck(FALSE), replaceText(FALSE), destroy(FALSE), reenter(FALSE),
		readData(FALSE), customCompare(FALSE), nested(S_OK), items(NULL)
	{
	}
	STDMETHOD(BindEvent)(RSTRING, IQuadooDelegate** value)
	{
		*value = this;
		AddRef();
		return S_OK;
	}
	STDMETHOD(PostBind)(IQuadooControl*)
	{
		return S_OK;
	}
	STDMETHOD(Invoke)(IQuadooEventArgs* args)
	{
		HRESULT hr;
		CRString name;
		TStackRef<IUnknown> object;
		TStackRef<IQuadooEventObjectArgs> objects;
		Check(args->GetEventName(&name));
		Check(args->QueryInterface(__uuidof(IQuadooEventObjectArgs),reinterpret_cast<PVOID*>(&objects)));
		UINT index = name == RSTRING_CAST(L"OnChanging") || name == RSTRING_CAST(L"OnEditing") ||
			name == RSTRING_CAST(L"OnEdited") || name == RSTRING_CAST(L"OnItemChecking") ? 1 : 0;
		Check(objects->GetObject(index,&object));
		last.Release();
		if(object)
		{
			Check(object->QueryInterface(__uuidof(IQuadooListItem),reinterpret_cast<PVOID*>(&last)));
		}
		if(name == RSTRING_CAST(L"OnInsert"))
		{
			inserted++;
		}
		else if(name == RSTRING_CAST(L"OnDeletion"))
		{
			TStackRef<IUnknown> data;
			deleted++;
			Check(last->GetData(&data));
			readData = data != NULL;
		}
		else if(name == RSTRING_CAST(L"OnChange"))
		{
			changed++;
		}
		else if(name == RSTRING_CAST(L"OnSelectItem"))
		{
			selected++;
		}
		else if(name == RSTRING_CAST(L"OnItemChecked"))
		{
			checked++;
		}
		else if(name == RSTRING_CAST(L"OnCompare"))
		{
			TStackRef<IUnknown> second;
			TStackRef<IQuadooListItem> other;
			CRString firstText;
			CRString secondText;
			comparing++;
			Check(objects->GetObject(1,&second));
			Check(second->QueryInterface(__uuidof(IQuadooListItem),reinterpret_cast<PVOID*>(&other)));
			Check(last->GetCaption(&firstText));
			Check(other->GetCaption(&secondText));
			Check(args->SetInt(3,customCompare ? 0 : lstrcmpiW(firstText.c_wstr(),secondText.c_wstr())));
		}
		else if(name == RSTRING_CAST(L"OnContextPopup"))
		{
			popups++;
			Check(args->GetInt(1,&popupColumn));
			Check(args->SetBool(4,TRUE));
		}
		else if(name == RSTRING_CAST(L"OnChanging") || name == RSTRING_CAST(L"OnEditing") ||
			name == RSTRING_CAST(L"OnEdited") || name == RSTRING_CAST(L"OnItemChecking"))
		{
			Check(args->SetBool(0,cancel));
			if(name == RSTRING_CAST(L"OnChanging"))
			{
				Check(args->GetInt(2,&mask));
				Check(args->GetInt(3,&oldState));
				Check(args->GetInt(4,&newState));
			}
			if(replaceCheck && name == RSTRING_CAST(L"OnItemChecking"))
			{
				Check(args->SetBool(2,FALSE));
			}
			if(replaceText && name == RSTRING_CAST(L"OnEdited"))
			{
				Check(args->SetString(3,RSTRING_CAST(L"replacement")));
			}
		}
		if(reenter)
		{
			nested = items->Clear();
		}
		if(destroy)
		{
			TStackRef<IQuadooComponent> sender;
			Check(args->GetSender(&sender));
			Check(sender->Destroy());
		}
		hr = S_OK;
	Cleanup:
		return hr;
	}
};
class CListDataCounter : public CBaseUnknown
{
	UINT* m_count;
public:
	BEGIN_UNK_MAP
		UNK_INTERFACE_PATH(IUnknown,CBaseUnknown)
	END_UNK_MAP
	CListDataCounter(UINT* count) : m_count(count)
	{
		(*m_count)++;
	}
	~CListDataCounter()
	{
		(*m_count)--;
	}
};
static HRESULT SetListBool(IQuadooComponent* control, PCWSTR name, BOOL value)
{
	HRESULT hr;
	TStackRef<IQuadooProperties> properties;
	UINT index;
	Check(Property(control,name,&properties,&index));
	Check(properties->SetValueBool(index,value));
Cleanup:
	return hr;
}
static HRESULT ListStoredItems()
{
	HRESULT hr;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONObject> serialized;
	TStackRef<IQuadooControl> root;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooControl> clone;
	TStackRef<IQuadooControlContainer> form;
	TStackRef<IQuadooListView> view;
	TStackRef<IQuadooListView> copied;
	TStackRef<IQuadooListItems> items;
	TStackRef<IQuadooListItems> cloneItems;
	TStackRef<IQuadooListColumns> columns;
	TStackRef<IQuadooListColumn> column;
	TStackRef<IQuadooListItem> first;
	TStackRef<IQuadooListItem> second;
	TStackRef<IQuadooListItem> third;
	TStackRef<IQuadooListItem> found;
	TStackRef<IQuadooWinControl> native;
	TStackRef<IQuadooFormSerializer> serializer;
	TStackRef<IRStringArray> strings;
	TStackRef<IRStringArray> snapshot;
	TStackRef<CStoredListEvents> events;
	TStackRef<CListDataCounter> payload;
	CRString text;
	GUID id;
	UINT count;
	UINT index;
	UINT payloads = 0;
	BOOL flag;
	INT number;
	HWND window;
	events.Attach(__new CStoredListEvents);
	CheckAlloc(events);
	Check(Context(96,events,&context));
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"Form1\",\"children\":[{\"type\":\"ListView\",\"name\":\"List1\","
		L"\"properties\":{\"Width\":350,\"Height\":160,\"CheckBoxes\":true,\"MultiSelect\":true,\"Columns\":[{\"Caption\":\"Name\"},{\"Caption\":\"Value\"}]},"
		L"\"events\":{\"OnInsert\":\"E\",\"OnDeletion\":\"E\",\"OnChanging\":\"E\",\"OnChange\":\"E\",\"OnSelectItem\":\"E\","
		L"\"OnItemChecking\":\"E\",\"OnItemChecked\":\"E\",\"OnEditing\":\"E\",\"OnEdited\":\"E\",\"OnCompare\":\"E\",\"OnContextPopup\":\"E\"}}]}",&definition));
	Check(QuadooCreateControl(context,NULL,definition,&root));
	Check(root->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&form)));
	Check(form->FindControl(RSTRING_CAST(L"List1"),&control));
	Check(control->QueryInterface(__uuidof(IQuadooListView),reinterpret_cast<PVOID*>(&view)));
	Check(control->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native)));
	Check(view->GetItems(&items));
	Check(view->GetColumns(&columns));
	events->items = items;
	Check(native->GetHandle(&window));
	Check(view->BeginUpdate());
	Check(items->Insert(0,RSTRING_CAST(L"Bravo"),&first));
	Check(items->Insert(0,RSTRING_CAST(L"Alpha"),&second));
	Check(items->Insert(2,RSTRING_CAST(L"Bravo"),&third));
	Check(view->EndUpdate());
	REQUIRE(events->inserted == 3 && ListView_GetItemCount(window) == 3);
	Check(first->GetItemIndex(&index));
	REQUIRE(index == 1);
	Check(first->GetID(&id));
	Check(items->Find(id,&found));
	REQUIRE(found == static_cast<IQuadooListItem*>(first));
	found.Release();
	REQUIRE(items->Insert(4,RSTRING_CAST(L"bad"),&found) == E_INVALIDARG && !found);
	Check(ScCreateRStringArray(&strings));
	Check(strings->AddRString(RSTRING_CAST(L"one")));
	Check(strings->AddRString(RSTRING_CAST(L"hidden")));
	Check(first->SetSubItems(strings));
	Check(strings->Replace(0,RSTRING_CAST(L"outside")));
	Check(first->GetSubItem(0,&text));
	REQUIRE(text == RSTRING_CAST(L"one"));
	Check(first->GetSubItems(&snapshot));
	Check(snapshot->Replace(0,RSTRING_CAST(L"snapshot")));
	text = NULL;
	Check(first->GetSubItem(0,&text));
	REQUIRE(text == RSTRING_CAST(L"one"));
	{
		WCHAR buffer[64];
		ListView_GetItemText(window,1,1,buffer,ARRAYSIZE(buffer));
		REQUIRE(!lstrcmpW(buffer,L"one"));
		UINT before = events->changed;
		REQUIRE(first->SetCaption(RSTRING_CAST(L"Bravo")) == S_FALSE);
		REQUIRE(events->changed == before);
	}
	Check(first->SetSubItem(3,RSTRING_CAST(L"extra")));
	text = NULL;
	Check(first->GetSubItem(2,&text));
	REQUIRE(text == RSTRING_CAST(L""));
	Check(first->SetImageIndex(1000));
	Check(first->GetImageIndex(&number));
	REQUIRE(number == 1000);
	Check(first->SetSelected(TRUE));
	Check(third->SetSelected(TRUE));
	Check(third->SetFocused(TRUE));
	Check(view->GetSelectedCount(&count));
	REQUIRE(count == 2 && ListView_GetSelectedCount(window) == 2);
	found.Release();
	Check(view->GetSelected(&found));
	REQUIRE(found == static_cast<IQuadooListItem*>(third));
	found.Release();
	Check(view->GetNextSelected(-1,&found));
	REQUIRE(found == static_cast<IQuadooListItem*>(first));
	Check(first->SetChecked(TRUE));
	REQUIRE(events->checked == 1 && ListView_GetCheckState(window,1));
	REQUIRE(events->mask == QuadooForm::ListChangeState &&
		events->oldState == (INDEXTOSTATEIMAGEMASK(1) | LVIS_SELECTED) &&
		events->newState == (INDEXTOSTATEIMAGEMASK(2) | LVIS_SELECTED));
	events->cancel = TRUE;
	REQUIRE(first->SetChecked(FALSE) == S_FALSE);
	REQUIRE(view->SetSelected(second) == S_FALSE);
	Check(first->GetChecked(&flag));
	REQUIRE(flag && ListView_GetCheckState(window,1));
	events->cancel = FALSE;
	events->replaceCheck = TRUE;
	REQUIRE(third->SetChecked(TRUE) == S_FALSE);
	events->replaceCheck = FALSE;
	Check(items->Move(first,0));
	Check(first->GetItemIndex(&index));
	REQUIRE(index == 0 && ListView_GetCheckState(window,0));
	Check(view->Sort(0,TRUE));
	Check(first->GetItemIndex(&index));
	REQUIRE(index == 1);
	Check(third->GetItemIndex(&index));
	REQUIRE(index == 2);
	events->customCompare = TRUE;
	Check(view->Sort(0,FALSE));
	Check(first->GetItemIndex(&index));
	REQUIRE(index == 1);
	events->customCompare = FALSE;
	events->reenter = TRUE;
	Check(first->SetCaption(RSTRING_CAST(L"changed")));
	REQUIRE(events->nested == QuadooForm::EInvalidState);
	events->reenter = FALSE;
	Check(columns->Insert(1,RSTRING_CAST(L"Inserted"),&column));
	text = NULL;
	Check(first->GetSubItem(0,&text));
	REQUIRE(text == RSTRING_CAST(L""));
	text = NULL;
	Check(first->GetSubItem(1,&text));
	REQUIRE(text == RSTRING_CAST(L"one"));
	Check(columns->Move(column,0));
	text = NULL;
	Check(first->GetCaption(&text));
	REQUIRE(text == RSTRING_CAST(L""));
	Check(columns->Delete(column));
	text = NULL;
	Check(first->GetCaption(&text));
	REQUIRE(text == RSTRING_CAST(L"changed"));
	Check(native->GetHandle(&window));
	REQUIRE(ListView_GetItemCount(window) == 3 && ListView_GetSelectedCount(window) == 2);
	{
		UINT before = events->checked;
		SendMessageW(window,WM_KEYDOWN,VK_SPACE,0);
		Check(first->GetChecked(&flag));
		REQUIRE(flag);
		Check(third->GetChecked(&flag));
		REQUIRE(flag && events->checked == before + 1);
		SendMessageW(window,WM_KEYDOWN,VK_SPACE,0);
		Check(first->GetChecked(&flag));
		REQUIRE(!flag);
		Check(third->GetChecked(&flag));
		REQUIRE(!flag && events->checked == before + 3);
		before = events->checked;
		ListView_SetCheckState(window,1,TRUE);
		Check(first->GetChecked(&flag));
		REQUIRE(flag && ListView_GetCheckState(window,1) && events->checked == before + 1);
		events->cancel = TRUE;
		ListView_SetCheckState(window,1,FALSE);
		Check(first->GetChecked(&flag));
		REQUIRE(flag && ListView_GetCheckState(window,1) && events->checked == before + 1);
		events->cancel = FALSE;
	}
	REQUIRE(SetListBool(control,L"OwnerData",TRUE) == QuadooForm::EInvalidState);
	payload.Attach(__new CListDataCounter(&payloads));
	CheckAlloc(payload);
	Check(first->SetData(payload));
	payload.Release();
	REQUIRE(payloads == 1);
	{
		TStackRef<IQuadooProperties> values;
		TStackRef<IJSONValue> stored;
		UINT property;
		Check(Property(control,L"Items",&values,&property));
		Check(values->GetValueJSON(property,&stored));
		REQUIRE(values->SetValueJSON(property,stored) == S_FALSE);
		Check(first->GetAttached(&flag));
		REQUIRE(flag && payloads == 1);
	}
	Check(QuadooGetFormSerializer(&serializer));
	Check(serializer->Serialize(root,&serialized));
	Check(QuadooCreateControl(context,NULL,serialized,&clone));
	{
		TStackRef<IQuadooControlContainer> cloneForm;
		TStackRef<IQuadooControl> cloneControl;
		Check(clone->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&cloneForm)));
		Check(cloneForm->FindControl(RSTRING_CAST(L"List1"),&cloneControl));
		Check(cloneControl->QueryInterface(__uuidof(IQuadooListView),reinterpret_cast<PVOID*>(&copied)));
	}
	Check(copied->GetItems(&cloneItems));
	found.Release();
	Check(cloneItems->Find(id,&found));
	Check(found->GetItemIndex(&index));
	REQUIRE(index == 1);
	Check(copied->GetSelectedCount(&count));
	REQUIRE(count == 0);
	{
		TStackRef<IUnknown> data;
		Check(found->GetData(&data));
		REQUIRE(!data);
		REQUIRE(items->Delete(found) == E_INVALIDARG);
	}
	Check(clone->Destroy());
	// A real native edit uses the form's keyboard preprocessing path.
	{
		TStackRef<IQuadooFormKeyboard> keyboard;
		TStackRef<IQuadooKeyboardInput> input;
		HWND edit;
		BOOL handled;
		BOOL owns;
		MSG message = {0};
		Check(root->QueryInterface(__uuidof(IQuadooFormKeyboard),reinterpret_cast<PVOID*>(&keyboard)));
		Check(control->QueryInterface(__uuidof(IQuadooKeyboardInput),reinterpret_cast<PVOID*>(&input)));
		const IID* paths[] = {&__uuidof(IQuadooKeyboardInput),&__uuidof(IQuadooListView),&__uuidof(IQuadooWinControl)};
		Check(CheckCOMIdentity(control,paths,ARRAYSIZE(paths)));
		Check(root->SetVisible(TRUE));
		Check(view->BeginLabelEdit(second));
		edit = ListView_GetEditControl(window);
		REQUIRE(edit && GetFocus() == edit);
		SetWindowTextW(edit,L"native edit");
		message.hwnd = edit;
		message.message = WM_KEYDOWN;
		message.wParam = VK_RETURN;
		Check(input->OwnsKeyboardMessage(&message,&owns));
		REQUIRE(owns);
		Check(keyboard->PreprocessMessage(&message,&handled));
		REQUIRE(!handled);
		DispatchMessageW(&message);
		REQUIRE(!ListView_GetEditControl(window));
		Check(input->OwnsKeyboardMessage(&message,&owns));
		REQUIRE(!owns);
		text = NULL;
		Check(second->GetCaption(&text));
		REQUIRE(text == RSTRING_CAST(L"native edit"));
		Check(view->BeginLabelEdit(second));
		edit = ListView_GetEditControl(window);
		SetWindowTextW(edit,L"discard");
		message.hwnd = edit;
		message.wParam = VK_ESCAPE;
		Check(keyboard->PreprocessMessage(&message,&handled));
		REQUIRE(!handled);
		DispatchMessageW(&message);
		text = NULL;
		Check(second->GetCaption(&text));
		REQUIRE(text == RSTRING_CAST(L"native edit"));
	}
	{
		RECT cell;
		POINT screen;
		Check(view->SetSelected(NULL));
		ListView_GetSubItemRect(window,0,1,LVIR_BOUNDS,&cell);
		screen.x = cell.left + 5;
		screen.y = (cell.top + cell.bottom) / 2;
		ClientToScreen(window,&screen);
		SendMessageW(window,WM_CONTEXTMENU,reinterpret_cast<WPARAM>(window),MAKELPARAM(screen.x,screen.y));
		REQUIRE(events->popups == 1 && events->last == static_cast<IQuadooListItem*>(second) && events->popupColumn == 1);
		Check(view->GetSelectedCount(&count));
		REQUIRE(count == 1);
		ListView_GetSubItemRect(window,1,1,LVIR_BOUNDS,&cell);
		screen.x = cell.left + 5;
		screen.y = (cell.top + cell.bottom) / 2;
		ClientToScreen(window,&screen);
		events->cancel = TRUE;
		SendMessageW(window,WM_CONTEXTMENU,reinterpret_cast<WPARAM>(window),MAKELPARAM(screen.x,screen.y));
		REQUIRE(events->popups == 1);
		events->cancel = FALSE;
		SendMessageW(window,WM_CONTEXTMENU,reinterpret_cast<WPARAM>(window),MAKELPARAM(-1,-1));
		REQUIRE(events->popups == 2 && events->last == static_cast<IQuadooListItem*>(second));
	}
	Check(items->Delete(first));
	REQUIRE(events->readData && payloads == 0);
	Check(first->GetAttached(&flag));
	REQUIRE(!flag);
	text = NULL;
	REQUIRE(first->GetCaption(&text) == QuadooForm::EDestroyed);
	Check(first->GetID(&id));
	Check(items->GetCount(&count));
	REQUIRE(count == 2 && ListView_GetItemCount(window) == 2);
	{
		TStackRef<IUnknown> one;
		TStackRef<IUnknown> two;
		Check(second->QueryInterface(IID_IUnknown,reinterpret_cast<PVOID*>(&one)));
		Check(static_cast<IQuadooListItem*>(second)->QueryInterface(IID_IUnknown,reinterpret_cast<PVOID*>(&two)));
		REQUIRE(one == static_cast<IUnknown*>(two));
	}
	// Synthetic notifications exercise the native edit acceptance protocol.
	{
		TStackRef<IQuadooMessageHandler> handler;
		NMLVDISPINFOW edit = {0};
		BOOL handled;
		LRESULT result;
		Check(control->QueryInterface(__uuidof(IQuadooMessageHandler),reinterpret_cast<PVOID*>(&handler)));
		edit.hdr.hwndFrom = window;
		edit.hdr.code = LVN_BEGINLABELEDITW;
		Check(handler->HandleMessage(window,WM_NOTIFY,0,reinterpret_cast<LPARAM>(&edit),&handled,&result));
		REQUIRE(handled && result == FALSE);
		edit.hdr.code = LVN_ENDLABELEDITW;
		edit.item.pszText = L"typed";
		events->replaceText = TRUE;
		Check(handler->HandleMessage(window,WM_NOTIFY,0,reinterpret_cast<LPARAM>(&edit),&handled,&result));
		REQUIRE(result == FALSE);
		text = NULL;
		Check(second->GetCaption(&text));
		REQUIRE(text == RSTRING_CAST(L"replacement"));
	}
	events->destroy = TRUE;
	REQUIRE(third->SetCaption(RSTRING_CAST(L"destroy")) == QuadooForm::EDestroyed);
	Check(third->GetAttached(&flag));
	REQUIRE(!flag);
	Check(root->Destroy());
	hr = S_OK;
Cleanup:
	if(clone)
	{
		clone->Destroy();
	}
	if(root)
	{
		root->Destroy();
	}
	return hr;
}
