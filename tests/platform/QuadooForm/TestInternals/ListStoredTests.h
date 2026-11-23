static HRESULT ListStoredRollbackTests()
{
	HRESULT hr;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<QF::CFactory> factory;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IQuadooControl> root;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooControlContainer> form;
	TStackRef<IJSONObject> definition;
	TStackRef<IQuadooFormSerializer> serializer;
	TStackRef<IJSONObject> before;
	TStackRef<IJSONObject> after;
	TStackRef<IQuadooListView> view;
	TStackRef<IQuadooListItems> items;
	TStackRef<IQuadooListItem> first;
	TStackRef<IQuadooListItem> second;
	TStackRef<IQuadooListItem> failed;
	TStackRef<IQuadooListColumns> columns;
	TStackRef<IQuadooListColumn> column;
	TStackRef<IQuadooWinControl> native;
	TStackRef<IQuadooProperties> properties;
	TStackRef<IJSONValue> value;
	UINT index;
	UINT count;
	HWND original;
	HWND current;
	CRString text;
	QuadooForm::ContextOptions options = {0};
	Check(QuadooGetControlRegistry(&registry));
	factory.Attach(__new QF::TFactory<CFailListView>);
	CheckAlloc(factory);
	Check(factory->Initialize());
	Check(registry->RegisterFactory(factory));
	options.cbSize = sizeof(options);
	options.DPI = 96;
	options.pRegistry = registry;
	Check(QuadooCreateConstructionContext(&options,&context));
	Check(NativeDefinition(L"{\"type\":\"Form\",\"name\":\"Form1\",\"children\":[{\"type\":\"FailListView\",\"name\":\"List1\","
		L"\"properties\":{\"Columns\":[{\"Caption\":\"Name\"},{\"Caption\":\"Value\"}],\"Items\":[{\"Caption\":\"Bravo\",\"SubItems\":[\"detail\"]},{\"Caption\":\"Alpha\"}]}}]}",&definition));
	Check(QuadooCreateControl(context,NULL,definition,&root));
	Check(root->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&form)));
	Check(form->FindControl(RSTRING_CAST(L"List1"),&control));
	Check(control->QueryInterface(__uuidof(IQuadooListView),reinterpret_cast<PVOID*>(&view)));
	Check(control->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native)));
	Check(native->GetHandle(&original));
	Check(view->GetItems(&items));
	Check(items->GetAt(0,&first));
	Check(items->GetAt(1,&second));
	Check(view->GetColumns(&columns));
	Check(columns->GetAt(0,&column));
	Check(view->SetSelected(first));
	Check(view->SetFocused(first));
	Check(ImageProperty(control,L"Columns",&properties,&index));
	Check(properties->GetValueJSON(index,&value));
	VERIFY(properties->SetValueJSON(index,value) == S_FALSE);
	Check(native->GetHandle(&current));
	VERIFY(current == original);
	properties.Release();
	value.Release();
	Check(QuadooGetFormSerializer(&serializer));
	Check(serializer->Serialize(root,&before));
	{
		const UINT messages[] = {LVM_INSERTITEMW,LVM_SETITEMW,LVM_DELETEITEM,LVM_DELETEALLITEMS,LVM_SORTITEMS};
		for(UINT n = 0; n < ARRAYSIZE(messages); n++)
		{
			HRESULT rejected;
			s_failListMessage = messages[n];
			if(n < 2)
			{
				rejected = items->Insert(0,RSTRING_CAST(L"failed"),&failed);
				VERIFY(!failed);
			}
			else if(n == 2)
			{
				rejected = items->Delete(first);
			}
			else if(n == 3)
			{
				rejected = items->Clear();
			}
			else
			{
				rejected = view->Sort(0,TRUE);
			}
			VERIFY(FAILED(rejected) && s_failListMessage == 0);
			Check(items->GetCount(&count));
			VERIFY(count == 2 && ListView_GetItemCount(original) == 2);
			Check(first->GetItemIndex(&index));
			VERIFY(index == 0);
			after.Release();
			Check(serializer->Serialize(root,&after));
			VERIFY(JSONCompareObjects(before,after) == 0);
		}
	}
	// Fail a complete candidate HWND after rows and columns were populated.
	s_failListNative = TRUE;
	VERIFY(columns->Move(column,1) == E_OUTOFMEMORY);
	Check(native->GetHandle(&current));
	VERIFY(current == original && ListView_GetItemCount(current) == 2);
	Check(first->GetCaption(&text));
	VERIFY(text == RSTRING_CAST(L"Bravo"));
	text = NULL;
	Check(first->GetSubItem(0,&text));
	VERIFY(text == RSTRING_CAST(L"detail"));
	VERIFY(ListView_GetNextItem(current,-1,LVNI_SELECTED) == 0);
	Check(ImageProperty(control,L"Items",&properties,&index));
	Check(JSONParse(NULL,SLP(L"[{\"Caption\":\"candidate\"}]"),&value));
	VERIFY(properties->SetValueJSON(index,value) == E_OUTOFMEMORY);
	after.Release();
	Check(serializer->Serialize(root,&after));
	VERIFY(JSONCompareObjects(before,after) == 0);
	s_failListNative = FALSE;
	{
		static PCWSTR const invalid[] =
		{
			L"[{\"Caption\":\"bad\\u0000text\"}]",
			L"[{\"ImageIndex\":-2}]",
			L"[{\"StateIndex\":15}]",
			L"[{\"Checked\":true}]",
			L"[{\"Caption\":5}]",
			L"[{\"Unknown\":1}]",
			L"[{\"SubItems\":[5]}]",
			L"[{\"id\":\"{10000000-0000-0000-0000-000000000001}\"},{\"id\":\"{10000000-0000-0000-0000-000000000001}\"}]"
		};
		for(UINT n = 0; n < ARRAYSIZE(invalid); n++)
		{
			value.Release();
			Check(JSONParse(NULL,invalid[n],TStrLenAssert(invalid[n]),&value));
			VERIFY(FAILED(properties->SetValueJSON(index,value)));
			after.Release();
			Check(serializer->Serialize(root,&after));
			VERIFY(JSONCompareObjects(before,after) == 0);
		}
	}
	value.Release();
	Check(JSONParse(NULL,SLP(L"[{\"Caption\":\"replacement\",\"SubItems\":[\"value\"]}]"),&value));
	Check(properties->SetValueJSON(index,value));
	{
		BOOL attached;
		Check(first->GetAttached(&attached));
		VERIFY(!attached);
		Check(native->GetHandle(&current));
		VERIFY(current != original && ListView_GetItemCount(current) == 1);
	}
	Check(root->Destroy());
	hr = S_OK;
Cleanup:
	s_failListMessage = 0;
	s_failListNative = FALSE;
	if(root)
	{
		root->Destroy();
	}
	return hr;
}
