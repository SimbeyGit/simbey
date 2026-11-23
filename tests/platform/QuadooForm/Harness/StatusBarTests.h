class CStatusRejectObserver : public CBaseUnknown, public IQuadooPropertyObserver
{
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooPropertyObserver)
	END_UNK_MAP
	STDMETHOD(OnPropertyChanged)(IQuadooComponent*, UINT)
	{
		return E_ABORT;
	}
	STDMETHOD(OnBoundsChanged)(IQuadooControl*, const QuadooForm::Rect*)
	{
		return S_OK;
	}
};
struct StatusThreadCall
{
	IQuadooStatusPanel* panel;
	HRESULT result;
};
static DWORD WINAPI StatusWrongThread(PVOID value)
{
	StatusThreadCall* call = static_cast<StatusThreadCall*>(value);
	INT width;
	call->result = call->panel->GetWidth(&width);
	return 0;
}
static HRESULT FixedStatusGrip(HWND window)
{
	HRESULT hr = S_OK;
	RECT before;
	RECT after;
	RECT parentBefore;
	RECT parentAfter;
	HWND parent = ::GetParent(window);
	HWND capture = GetCapture();
	REQUIRE(GetWindowRect(window,&before));
	REQUIRE(GetWindowRect(parent,&parentBefore));
	LPARAM point = MAKELPARAM(before.right - 2,before.bottom - 2);
	REQUIRE(SendMessageW(window,WM_NCHITTEST,0,point) == HTCLIENT);
	SendMessageW(window,WM_NCLBUTTONDOWN,HTBOTTOMRIGHT,point);
	SendMessageW(window,WM_NCLBUTTONDBLCLK,HTBOTTOMRIGHT,point);
	SendMessageW(window,WM_NCLBUTTONDOWN,HTBOTTOMLEFT,point);
	SendMessageW(window,WM_SYSCOMMAND,SC_SIZE | WMSZ_BOTTOMRIGHT,point);
	REQUIRE(GetWindowRect(window,&after) && EqualRect(&before,&after));
	REQUIRE(GetWindowRect(parent,&parentAfter) && EqualRect(&parentBefore,&parentAfter));
	REQUIRE(GetCapture() == capture);
Cleanup:
	return hr;
}
static HRESULT StatusBarNativeTests()
{
	HRESULT hr;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONObject> saved;
	TStackRef<IJSONObject> again;
	TStackRef<IJSONObject> fixture;
	TStackRef<IJSONValue> images;
	TStackRef<IQuadooControl> root;
	TStackRef<IQuadooControl> copy;
	TStackRef<IQuadooControlContainer> container;
	TStackRef<IQuadooControl> control;
	TStackRef<IQuadooControl> client;
	TStackRef<IQuadooStatusBar> bar;
	TStackRef<IQuadooStatusPanels> panels;
	TStackRef<IQuadooStatusPanel> first;
	TStackRef<IQuadooStatusPanel> second;
	TStackRef<IQuadooStatusPanel> third;
	TStackRef<IQuadooComponent> entry;
	TStackRef<IQuadooComponentHost> host;
	TStackRef<IQuadooComponentCollection> components;
	TStackRef<IQuadooComponent> imageList;
	TStackRef<IQuadooFormSerializer> serializer;
	TStackRef<IQuadooWinControl> window;
	TStackRef<IQuadooControlRegistry> registry;
	TStackRef<IQuadooControlFactory> factory;
	TStackRef<IQuadooFactoryLifetime> lifetime;
	HWND handle;
	HANDLE thread = NULL;
	UINT count;
	UINT baseline;
	INT number;
	INT edges[3];
	WCHAR text[100];
	GUID id;
	GUID current;
	QuadooForm::Rect bounds;
	Check(QuadooGetControlRegistry(&registry));
	Check(registry->FindFactory(RSTRING_CAST(L"StatusBar"),&factory));
	Check(factory->QueryInterface(&lifetime));
	Check(lifetime->GetLiveCount(&baseline));
	Check(Context(96,NULL,&context));
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"StatusForm\",\"properties\":{\"Width\":400,\"Height\":240},\"children\":[{\"type\":\"Panel\",\"name\":\"Client\",\"properties\":{\"Align\":5}},{\"type\":\"StatusBar\",\"name\":\"Status\"}]}",&definition));
	Check(Parse(ImageListNativeDefinition,&fixture));
	Check(fixture->FindNonNullValueW(L"components",&images));
	Check(definition->AddValueW(L"components",images));
	Check(QuadooCreateControl(context,NULL,definition,&root));
	Check(root->QueryInterface(&container));
	Check(container->FindControl(RSTRING_CAST(L"Status"),&control));
	Check(container->FindControl(RSTRING_CAST(L"Client"),&client));
	Check(control->QueryInterface(&bar));
	{
		const IID* paths[] = {&__uuidof(IQuadooStatusBar),&__uuidof(IQuadooWinControl),&__uuidof(IQuadooComponentOutline),&__uuidof(IQuadooViewportDock),&__uuidof(IQuadooUniqueComponent)};
		Check(CheckCOMIdentity(control,paths,ARRAYSIZE(paths)));
	}
	Check(control->QueryInterface(&window));
	Check(window->GetHandle(&handle));
	HWND parent;
	Check(container->GetHandle(&parent));
	REQUIRE(::GetParent(handle) == parent);
	REQUIRE(!(GetWindowLongW(handle,GWL_STYLE) & WS_TABSTOP));
	REQUIRE(GetWindowLongW(handle,GWL_STYLE) & SBARS_SIZEGRIP);
	Check(control->GetEffectiveBounds(&bounds));
	REQUIRE(bounds.Left == 0 && bounds.Top == 218 && bounds.Width == 400 && bounds.Height == 22);
	Check(client->GetEffectiveBounds(&bounds));
	REQUIRE(bounds.Width == 400 && bounds.Height == 218);
	Check(SetInt(control,L"MinHeight",30));
	Check(control->GetEffectiveBounds(&bounds));
	REQUIRE(bounds.Height == 30 && bounds.Width == 400 && bounds.Top == 210);
	Check(client->GetEffectiveBounds(&bounds));
	REQUIRE(bounds.Height == 210);
	Check(SetInt(control,L"MinHeight",0));
	{
		RECT rect;
		REQUIRE(GetWindowRect(handle,&rect));
		REQUIRE(SendMessageW(handle,WM_NCHITTEST,0,MAKELPARAM(rect.right - 2,rect.bottom - 2)) == HTBOTTOMRIGHT);
		static const INT borders[] = {QuadooForm::BorderSingle,QuadooForm::BorderNone,QuadooForm::BorderDialog};
		for(UINT n = 0; n < ARRAYSIZE(borders); n++)
		{
			Check(SetInt(root,L"BorderStyle",borders[n]));
			Check(FixedStatusGrip(handle));
		}
		Check(SetInt(root,L"BorderStyle",QuadooForm::BorderSizeable));
		REQUIRE(GetWindowRect(handle,&rect));
		REQUIRE(SendMessageW(handle,WM_NCHITTEST,0,MAKELPARAM(rect.right - 2,rect.bottom - 2)) == HTBOTTOMRIGHT);
		DWORD style = GetWindowLongW(parent,GWL_STYLE);
		SetWindowLongW(parent,GWL_STYLE,style | WS_MAXIMIZE);
		HRESULT fixed = FixedStatusGrip(handle);
		SetWindowLongW(parent,GWL_STYLE,style);
		Check(fixed);
	}
	Check(bar->GetPanels(&panels));
	Check(panels->GetCount(&count));
	REQUIRE(!count);
	Check(panels->Insert(0,&first));
	Check(first->SetText(RSTRING_CAST(L"Ready")));
	Check(first->SetWidth(120));
	Check(panels->Insert(1,&second));
	Check(second->SetText(RSTRING_CAST(L"Line 1")));
	Check(second->SetWidth(80));
	Check(panels->Insert(2,&third));
	Check(third->SetText(RSTRING_CAST(L"Info")));
	REQUIRE(SendMessageW(handle,SB_GETPARTS,3,reinterpret_cast<LPARAM>(edges)) == 3);
	REQUIRE(edges[0] == 120 && edges[1] == 200 && edges[2] == -1);
	SendMessageW(handle,SB_GETTEXTW,0,reinterpret_cast<LPARAM>(text));
	REQUIRE(!lstrcmpW(text,L"Ready"));
	Check(first->QueryInterface(&entry));
	Check(entry->GetID(&id));
	{
		const IID* paths[] = {&__uuidof(IQuadooComponent),&__uuidof(IQuadooProperties),&__uuidof(IQuadooStatusPanel),&__uuidof(IQuadooComponentOutline)};
		Check(CheckCOMIdentity(entry,paths,ARRAYSIZE(paths)));
	}
	Check(panels->Move(first,2));
	Check(first->GetIndex(&count));
	REQUIRE(count == 2);
	Check(entry->GetID(&current));
	REQUIRE(IsEqualGUID(current,id));
	Check(panels->Move(first,0));
	{
		TStackRef<IQuadooStatusPanel> extra;
		for(UINT n = 3; n < 255; n++)
		{
			extra.Release();
			Check(panels->Insert(n,&extra));
		}
		Check(extra->SetText(RSTRING_CAST(L"Last panel")));
		SendMessageW(handle,SB_GETTEXTW,254,reinterpret_cast<LPARAM>(text));
		REQUIRE(!lstrcmpW(text,L"Last panel"));
		extra.Release();
		REQUIRE(panels->Insert(255,&extra) == E_INVALIDARG && !extra);
		for(UINT n = 255; n > 3; n--)
		{
			extra.Release();
			Check(panels->GetAt(n - 1,&extra));
			Check(panels->Delete(extra));
		}
		Check(first->GetIndex(&count));
		REQUIRE(count == 0);
	}
	{
		TStackRef<IQuadooProperties> properties;
		TStackRef<IJSONValue> invalid;
		UINT property;
		Check(Property(control,L"Panels",&properties,&property));
		Check(JSONParse(NULL,SLP(L"[{\"Width\":-1}]"),&invalid));
		REQUIRE(properties->SetValueJSON(property,invalid) == E_INVALIDARG);
		Check(panels->GetCount(&count));
		REQUIRE(count == 3);
		Check(first->GetIndex(&count));
		REQUIRE(count == 0);
	}
	REQUIRE(SetInt(control,L"Align",QuadooForm::AlignLeft) == E_INVALIDARG);
	REQUIRE(first->SetWidth(-1) == E_INVALIDARG);
	REQUIRE(first->SetIcon(-2) == E_INVALIDARG);
	third.Release();
	REQUIRE(panels->GetAt(3,&third) == DISP_E_BADINDEX);
	Check(panels->GetAt(2,&third));
	{
		StatusThreadCall call = {first,S_OK};
		thread = CreateThread(NULL,0,StatusWrongThread,&call,0,NULL);
		CheckIf(!thread,HrEnsureLastError());
		REQUIRE(WaitForSingleObject(thread,10000) == WAIT_OBJECT_0);
		REQUIRE(call.result == QuadooForm::EWrongThread);
		CloseHandle(thread);
		thread = NULL;
	}
	Check(root->QueryInterface(&host));
	Check(host->GetComponents(&components));
	Check(components->FindComponent(RSTRING_CAST(L"ImageList1"),&imageList));
	Check(first->SetIcon(0));
	Check(bar->SetImages(imageList));
	REQUIRE(SendMessageW(handle,SB_GETICON,0,0) != 0);
	{
		TStackRef<IQuadooPropertyNotifications> notifications;
		TStackRef<CStatusRejectObserver> observer;
		TStackRef<IQuadooStatusPanel> extra;
		Check(control->QueryInterface(&notifications));
		observer.Attach(__new CStatusRejectObserver);
		CheckAlloc(observer);
		Check(notifications->AddObserver(observer));
		REQUIRE(first->SetText(RSTRING_CAST(L"Committed")) == E_ABORT);
		SendMessageW(handle,SB_GETTEXTW,0,reinterpret_cast<LPARAM>(text));
		REQUIRE(!lstrcmpW(text,L"Committed"));
		REQUIRE(panels->Insert(3,&extra) == E_ABORT && !extra);
		Check(panels->GetCount(&count));
		REQUIRE(count == 4);
		Check(notifications->RemoveObserver(observer));
		Check(panels->GetAt(3,&extra));
		Check(panels->Delete(extra));
		Check(first->SetText(RSTRING_CAST(L"Ready")));
	}
	Check(second->SetIcon(100000));
	REQUIRE(SendMessageW(handle,SB_GETICON,1,0) == 0);
	{
		DWORD userObjects = GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS);
		DWORD gdiObjects = GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
		for(UINT n = 0; n < 40; n++)
		{
			Check(first->SetIcon(n % 2));
		}
		Check(first->SetIcon(0));
		REQUIRE(GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS) <= userObjects);
		REQUIRE(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS) <= gdiObjects);
	}
	{
		TStackRef<IQuadooLayoutControl> layout;
		Check(root->QueryInterface(&layout));
		Check(root->GetBounds(&bounds));
		Check(layout->Arrange(&bounds,144));
		REQUIRE(SendMessageW(handle,SB_GETPARTS,3,reinterpret_cast<LPARAM>(edges)) == 3);
		REQUIRE(edges[0] == 180 && edges[1] == 300);
		Check(first->GetWidth(&number));
		REQUIRE(number == 120);
		ICONINFO info = {0};
		BITMAP bitmap;
		REQUIRE(GetIconInfo(reinterpret_cast<HICON>(SendMessageW(handle,SB_GETICON,0,0)),&info));
		BOOL sizeOK = info.hbmColor && GetObject(info.hbmColor,sizeof(bitmap),&bitmap) && bitmap.bmWidth == 24 && bitmap.bmHeight == 24;
		if(info.hbmColor)
		{
			DeleteObject(info.hbmColor);
		}
		if(info.hbmMask)
		{
			DeleteObject(info.hbmMask);
		}
		REQUIRE(sizeOK);
	}
	Check(bar->SetSizeGrip(FALSE));
	Check(window->GetHandle(&handle));
	REQUIRE(!(GetWindowLongW(handle,GWL_STYLE) & SBARS_SIZEGRIP));
	REQUIRE(SendMessageW(handle,SB_GETICON,0,0) != 0);
	{
		TStackRef<IQuadooProperties> properties;
		TStackRef<IQuadooControl> overflow;
		TStackRef<IJSONObject> overflowDefinition;
		TStackRef<IQuadooScrollable> scrolling;
		UINT index;
		RECT before;
		RECT after;
		Check(Property(root,L"AutoScroll",&properties,&index));
		Check(properties->SetValueBool(index,TRUE));
		Check(Parse(L"{\"type\":\"Button\",\"name\":\"Overflow\",\"properties\":{\"Left\":800,\"Top\":700}}",&overflowDefinition));
		Check(QuadooCreateControl(context,container,overflowDefinition,&overflow));
		Check(container->UpdateLayout());
		REQUIRE(GetWindowRect(handle,&before));
		Check(root->QueryInterface(&scrolling));
		QuadooForm::Point point = {50,60};
		Check(scrolling->SetScrollPosition(&point));
		REQUIRE(GetWindowRect(handle,&after) && EqualRect(&before,&after));
		RECT viewport;
		REQUIRE(GetClientRect(parent,&viewport));
		MapWindowPoints(NULL,parent,reinterpret_cast<POINT*>(&after),2);
		REQUIRE(after.left == 0 && after.right == viewport.right && after.bottom == viewport.bottom);
		Check(overflow->Destroy());
		Check(properties->SetValueBool(index,FALSE));
	}
	Check(QuadooGetFormSerializer(&serializer));
	Check(serializer->Serialize(root,&saved));
	Check(QuadooCreateControl(context,NULL,saved,&copy));
	{
		TStackRef<IQuadooFormSerializer> copied;
		Check(QuadooGetFormSerializer(&copied));
		Check(copied->Serialize(copy,&again));
		REQUIRE(!JSONCompareObjects(saved,again));
	}
	Check(copy->Destroy());
	copy.Release();
	Check(container->GetChildCount(&count));
	{
		TStackRef<IQuadooControl> duplicate;
		TStackRef<IQuadooControlContainer> wrongParent;
		fixture.Release();
		Check(Parse(L"{\"type\":\"StatusBar\",\"name\":\"Other\"}",&fixture));
		REQUIRE(QuadooCreateControl(context,container,fixture,&duplicate) == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS));
		UINT after;
		Check(container->GetChildCount(&after));
		REQUIRE(after == count);
		Check(client->QueryInterface(&wrongParent));
		REQUIRE(QuadooCreateControl(context,wrongParent,fixture,&duplicate) == E_INVALIDARG);
	}
	{
		TStackRef<IQuadooControl> frame;
		TStackRef<IQuadooControlContainer> frameContainer;
		fixture.Release();
		Check(Parse(L"{\"type\":\"Frame\",\"name\":\"Frame1\",\"children\":[{\"type\":\"StatusBar\",\"name\":\"FrameStatus\"}]}",&fixture));
		Check(QuadooCreateControl(context,container,fixture,&frame));
		Check(frame->QueryInterface(&frameContainer));
		TStackRef<IQuadooControl> frameStatus;
		Check(frameContainer->FindControl(RSTRING_CAST(L"FrameStatus"),&frameStatus));
		Check(frameStatus->GetEffectiveBounds(&bounds));
		REQUIRE(bounds.Left == 0 && bounds.Width > 0);
		TStackRef<IQuadooWinControl> frameWindow;
		HWND frameHandle;
		Check(frameStatus->QueryInterface(&frameWindow));
		Check(frameWindow->GetHandle(&frameHandle));
		Check(FixedStatusGrip(frameHandle));
		Check(frame->Destroy());
	}
	Check(panels->Delete(first));
	REQUIRE(first->GetWidth(&number) == QuadooForm::EDestroyed);
	Check(entry->GetID(&current));
	REQUIRE(IsEqualGUID(current,id));
	Check(second->SetIcon(0));
	REQUIRE(SendMessageW(handle,SB_GETICON,0,0) != 0);
	Check(imageList->Destroy());
	{
		TStackRef<IQuadooComponent> images;
		Check(bar->GetImages(&images));
		REQUIRE(!images && SendMessageW(handle,SB_GETICON,0,0) == 0);
	}
	Check(panels->Clear());
	REQUIRE(SendMessageW(handle,SB_GETICON,0,0) == 0);
	Check(root->Destroy());
	REQUIRE(panels->GetCount(&count) == QuadooForm::EDestroyed);
	root.Release();
	container.Release();
	control.Release();
	window.Release();
	bar.Release();
	panels.Release();
	first.Release();
	second.Release();
	third.Release();
	entry.Release();
	serializer.Release();
	Check(lifetime->GetLiveCount(&count));
	REQUIRE(count == baseline);
	REQUIRE(!IsWindow(handle));
	hr = S_OK;
Cleanup:
	if(thread)
	{
		CloseHandle(thread);
	}
	if(copy)
	{
		copy->Destroy();
	}
	if(root)
	{
		root->Destroy();
	}
	return hr;
}
