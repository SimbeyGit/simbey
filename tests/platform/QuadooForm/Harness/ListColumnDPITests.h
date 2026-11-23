static HRESULT ListColumnDPI()
{
	HRESULT hr;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IQuadooConstructionContext> loadContext;
	TStackRef<IQuadooFormSerializer> serializer;
	TStackRef<IJSONObject> definition;
	Check(Parse(L"{\"type\":\"Form\",\"children\":[{\"type\":\"ListView\",\"name\":\"List\",\"properties\":{\"Columns\":[{\"Caption\":\"Name\",\"Width\":101},{\"Caption\":\"Details\",\"Width\":173}]}}]}",&definition));
	Check(QuadooGetFormSerializer(&serializer));
	for(UINT virtualMode = 0; virtualMode < 2; virtualMode++)
	{
		for(UINT dpi = 96; dpi <= 192; dpi += 48)
		{
			TStackRef<IQuadooControl> root;
			TStackRef<IQuadooControl> copy;
			TStackRef<IQuadooControl> control;
			TStackRef<IQuadooControl> loaded;
			TStackRef<IQuadooControlContainer> parent;
			TStackRef<IQuadooControlContainer> copyParent;
			TStackRef<IQuadooWinControl> native;
			TStackRef<IQuadooListView> view;
			TStackRef<IQuadooProperties> properties;
			TStackRef<IQuadooMessageHandler> messages;
			TStackRef<IJSONObject> saved;
			TStackRef<IJSONValue> columns;
			TStackRef<IJSONValue> value;
			TStackRef<IJSONArray> array;
			TStackRef<IJSONObject> column;
			UINT index;
			HWND window;
			BOOL handled;
			LRESULT result;
			Check(Context(dpi,NULL,&context));
			Check(QuadooCreateControl(context,NULL,definition,&root));
			Check(root->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&parent)));
			Check(parent->FindControl(RSTRING_CAST(L"List"),&control));
			Check(SetListBool(control,L"OwnerData",virtualMode));
			Check(control->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native)));
			Check(native->GetHandle(&window));
			REQUIRE(GetWindowLongW(ListView_GetHeader(window),GWL_STYLE) & WS_VISIBLE);
			REQUIRE(ListView_GetColumnWidth(window,0) == MulDiv(101,dpi,96));
			REQUIRE(ListView_GetColumnWidth(window,1) == MulDiv(173,dpi,96));
			REQUIRE(ListView_SetColumnWidth(window,1,151));
			Check(control->QueryInterface(__uuidof(IQuadooMessageHandler),reinterpret_cast<PVOID*>(&messages)));
			NMHEADERW notification = {0};
			notification.hdr.hwndFrom = ListView_GetHeader(window);
			notification.hdr.code = HDN_ENDTRACKW;
			notification.iItem = 1;
			Check(messages->HandleMessage(window,WM_NOTIFY,0,reinterpret_cast<LPARAM>(&notification),&handled,&result));
			// Serialize before the posted header-sync message is dispatched.
			Check(serializer->Serialize(root,&saved));
			Check(Property(control,L"Columns",&properties,&index));
			Check(properties->GetValueJSON(index,&columns));
			Check(columns->GetArray(&array));
			Check(array->GetValue(1,&value));
			Check(value->GetObject(&column));
			value.Release();
			Check(column->FindNonNullValueW(L"Width",&value));
			INT logical;
			Check(value->GetInteger(&logical));
			REQUIRE(logical == MulDiv(151,96,dpi));
			Check(Context(144,NULL,&loadContext));
			Check(QuadooCreateControl(loadContext,NULL,saved,&copy));
			Check(copy->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&copyParent)));
			Check(copyParent->FindControl(RSTRING_CAST(L"List"),&loaded));
			native.Release();
			Check(loaded->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native)));
			Check(native->GetHandle(&window));
			REQUIRE(GetWindowLongW(ListView_GetHeader(window),GWL_STYLE) & WS_VISIBLE);
			REQUIRE(ListView_GetColumnWidth(window,0) == MulDiv(101,144,96));
			REQUIRE(ListView_GetColumnWidth(window,1) == MulDiv(logical,144,96));
			{
				TStackRef<IQuadooWinControl> rootNative;
				TStackRef<IQuadooLayoutControl> layout;
				TStackRef<IQuadooListView> rootView;
				TStackRef<IQuadooListColumns> rootColumns;
				TStackRef<IQuadooListColumn> resized;
				QuadooForm::Rect bounds;
				UINT nextDPI = dpi == 144 ? 192 : 144;
				Check(control->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&rootNative)));
				Check(control->QueryInterface(__uuidof(IQuadooLayoutControl),reinterpret_cast<PVOID*>(&layout)));
				Check(control->QueryInterface(__uuidof(IQuadooListView),reinterpret_cast<PVOID*>(&rootView)));
				Check(rootNative->GetHandle(&window));
				REQUIRE(ListView_SetColumnWidth(window,1,157));
				notification.hdr.hwndFrom = ListView_GetHeader(window);
				Check(messages->HandleMessage(window,WM_NOTIFY,0,reinterpret_cast<LPARAM>(&notification),&handled,&result));
				// A DPI change can arrive before the queued header-sync message.
				Check(control->GetBounds(&bounds));
				Check(layout->Arrange(&bounds,nextDPI));
				Check(rootNative->GetHandle(&window));
				Check(rootView->GetColumns(&rootColumns));
				Check(rootColumns->GetAt(1,&resized));
				Check(resized->GetWidth(&logical));
				REQUIRE(logical == MulDiv(157,96,dpi));
				REQUIRE(ListView_GetColumnWidth(window,1) == MulDiv(logical,nextDPI,96));
			}
			Check(copy->Destroy());
			Check(root->Destroy());
			context.Release();
			loadContext.Release();
		}
	}
Cleanup:
	return hr;
}
