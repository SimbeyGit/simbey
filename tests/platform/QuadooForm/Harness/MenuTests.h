#include "MenuFixture.h"
class CMenuTestBinder : public CBaseUnknown, public IQuadooEventsBinder, public IQuadooDelegate
{
public:
	UINT binds;
	UINT clicks;
	IQuadooControl* root;
	BOOL destroyRoot;
	BOOL checkedInCallback;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooEventsBinder)
		UNK_INTERFACE(IQuadooDelegate)
	END_UNK_MAP
	CMenuTestBinder() : binds(0), clicks(0), root(NULL), destroyRoot(FALSE), checkedInCallback(FALSE)
	{
	}
	STDMETHOD(BindEvent)(RSTRING name, IQuadooDelegate** delegate)
	{
		HRESULT hr = S_OK;
		CheckIf(!delegate,E_POINTER);
		*delegate = NULL;
		CheckIf(!(CRString(name) == RSTRING_CAST(L"MenuClick")),QuadooForm::EHandlerNotFound);
		*delegate = this;
		AddRef();
		binds++;
	Cleanup:
		return hr;
	}
	STDMETHOD(PostBind)(IQuadooControl* control)
	{
		root = control;
		return S_OK;
	}
	STDMETHOD(Invoke)(IQuadooEventArgs* args)
	{
		HRESULT hr;
		TStackRef<IQuadooComponent> sender;
		TStackRef<IQuadooMenuItem> item;
		Check(args->GetSender(&sender));
		Check(sender->QueryInterface(__uuidof(IQuadooMenuItem),reinterpret_cast<PVOID*>(&item)));
		Check(item->GetChecked(&checkedInCallback));
		clicks++;
		if(destroyRoot)
		{
			Check(root->Destroy());
		}
	Cleanup:
		return hr;
	}
};
static HRESULT MenuFlag(IQuadooComponent* component, PCWSTR name, BOOL flag)
{
	HRESULT hr;
	TStackRef<IQuadooProperties> properties;
	UINT index;
	Check(Property(component,name,&properties,&index));
	Check(properties->SetValueBool(index,flag));
Cleanup:
	return hr;
}
static HRESULT MenuText(IQuadooComponent* component, PCWSTR name, RSTRING text)
{
	HRESULT hr;
	TStackRef<IQuadooProperties> properties;
	UINT index;
	Check(Property(component,name,&properties,&index));
	Check(properties->SetValueString(index,text));
Cleanup:
	return hr;
}
static HRESULT NativeMenusCore()
{
	HRESULT hr;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONObject> first;
	TStackRef<IJSONObject> second;
	TStackRef<IQuadooForm> form;
	TStackRef<IQuadooFormMenu> menus;
	TStackRef<IQuadooComponentHost> host;
	TStackRef<IQuadooComponentCollection> components;
	TStackRef<IQuadooComponent> mainMenu;
	TStackRef<IQuadooComponent> open;
	TStackRef<IQuadooComponent> file;
	TStackRef<IQuadooComponent> extra;
	TStackRef<IQuadooMenuItem> item;
	TStackRef<IQuadooFormSerializer> serializer;
	TStackRef<IQuadooControl> copy;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<CMenuTestBinder> binder;
	TStackRef<CMenuTestBinder> copyBinder;
	HWND window;
	RECT client;
	HMENU native = NULL;
	BOOL checked;
	binder.Attach(__new CMenuTestBinder);
	CheckAlloc(binder);
	Check(Parse(MenuTestDefinition,&definition));
	Check(QuadooCreateForm(binder,NULL,definition,&form));
	Check(form->GetHandle(&window));
	Check(form->QueryInterface(__uuidof(IQuadooFormMenu),reinterpret_cast<PVOID*>(&menus)));
	Check(form->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&host)));
	Check(host->GetComponents(&components));
	Check(menus->GetMainMenu(&mainMenu));
	REQUIRE(mainMenu != NULL && binder->binds == 3);
	native = GetMenu(window);
	REQUIRE(IsMenu(native) && GetMenuItemCount(native) == 3);
	REQUIRE(GetClientRect(window,&client) && client.right == 380 && client.bottom == 180);
	HMENU dropdown = GetSubMenu(native,0);
	REQUIRE(dropdown && GetMenuItemCount(dropdown) == 4);
	REQUIRE((GetMenuState(dropdown,1,MF_BYPOSITION) & MF_SEPARATOR) != 0);
	REQUIRE((GetMenuState(dropdown,3,MF_BYPOSITION) & (MF_CHECKED | MF_DISABLED)) == (MF_CHECKED | MF_DISABLED));
	Check(components->FindComponent(RSTRING_CAST(L"OpenItem"),&open));
	Check(components->FindComponent(RSTRING_CAST(L"FileMenu"),&file));
	Check(open->QueryInterface(__uuidof(IQuadooMenuItem),reinterpret_cast<PVOID*>(&item)));
	{
		const IID* paths[] = {&__uuidof(IQuadooComponent),&__uuidof(IQuadooNonvisualComponent),&__uuidof(IQuadooMenu),&__uuidof(IQuadooMenuItems),&__uuidof(IQuadooUniqueComponent)};
		Check(CheckCOMIdentity(mainMenu,paths,ARRAYSIZE(paths)));
		const IID* itemPaths[] = {&__uuidof(IQuadooComponent),&__uuidof(IQuadooNonvisualComponent),&__uuidof(IQuadooMenuItem),&__uuidof(IQuadooMenuItems),&__uuidof(IQuadooProperties)};
		Check(CheckCOMIdentity(open,itemPaths,ARRAYSIZE(itemPaths)));
	}
	SendMessageW(window,WM_MENUCOMMAND,0,reinterpret_cast<LPARAM>(dropdown));
	Check(item->GetChecked(&checked));
	REQUIRE(binder->clicks == 1 && checked && binder->checkedInCallback);
	dropdown = GetSubMenu(GetMenu(window),0);
	SendMessageW(window,WM_MENUCOMMAND,0,reinterpret_cast<LPARAM>(GetSubMenu(dropdown,2)));
	REQUIRE(binder->clicks == 2);
	Check(item->SetEnabled(FALSE));
	REQUIRE(item->Click() == S_FALSE && binder->clicks == 2);
	Check(item->SetEnabled(TRUE));
	Check(item->Toggle());
	Check(item->GetChecked(&checked));
	REQUIRE(!checked);
	for(INT width = 80; width <= 900; width = width == 80 ? 160 : width == 160 ? 380 : 900)
	{
		QuadooForm::Rect bounds = {0,0,width,180};
		Check(form->SetBounds(&bounds));
		REQUIRE(GetClientRect(window,&client) && client.right == width && client.bottom == 180);
		QuadooForm::Rect requested;
		Check(form->GetBounds(&requested));
		REQUIRE(requested.Width == width && requested.Height == 180);
		TStackRef<IQuadooControl> child;
		Check(form->FindControl(RSTRING_CAST(L"ChildButton"),&child));
		Check(child->GetBounds(&requested));
		REQUIRE(requested.Left == 11 && requested.Top == 13);
		if(width == 900)
		{
			break;
		}
	}
	Check(MenuText(file,L"Caption",RSTRING_CAST(L"A changed caption which forces wrapping in a narrow menu")));
	REQUIRE(GetClientRect(window,&client) && client.bottom == 180);
	Check(MenuFlag(file,L"Visible",FALSE));
	REQUIRE(GetMenuItemCount(GetMenu(window)) == 2);
	Check(MenuFlag(file,L"Visible",TRUE));
	SendMessageW(window,WM_ENTERMENULOOP,FALSE,0);
	native = GetMenu(window);
	Check(MenuText(file,L"Caption",RSTRING_CAST(L"&File")));
	REQUIRE(GetMenu(window) == native && IsMenu(native));
	SendMessageW(window,WM_EXITMENULOOP,FALSE,0);
	REQUIRE(!PumpPendingMessages());
	REQUIRE(GetMenu(window) != native && !IsMenu(native));
	Check(Context(96,NULL,&context));
	{
		TStackRef<IJSONObject> duplicate;
		Check(Parse(L"{\"type\":\"MainMenu\",\"name\":\"SecondMainMenu\"}",&duplicate));
		UINT before;
		UINT after;
		Check(components->GetCount(&before));
		REQUIRE(QuadooCreateComponent(context,components,duplicate,&extra) == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS) && !extra);
		Check(components->GetCount(&after));
		REQUIRE(before == after);
	}
	Check(QuadooGetFormSerializer(&serializer));
	Check(serializer->Serialize(form,&first));
	copyBinder.Attach(__new CMenuTestBinder);
	CheckAlloc(copyBinder);
	context.Release();
	Check(Context(96,copyBinder,&context));
	Check(QuadooCreateControl(context,NULL,first,&copy));
	Check(serializer->Serialize(copy,&second));
	REQUIRE(JSONCompareObjects(first,second) == 0);
	Check(copy->Destroy());
	copy.Release();
	Check(menus->SetMainMenu(NULL));
	REQUIRE(GetMenu(window) == NULL && GetClientRect(window,&client) && client.bottom == 180);
	Check(menus->SetMainMenu(mainMenu));
	REQUIRE(GetMenu(window) != NULL && GetClientRect(window,&client) && client.bottom == 180);
	REQUIRE(FAILED(MenuText(open,L"ShortCut",RSTRING_CAST(L"Ctrl+Bogus"))));
	{
		BYTE previousKeys[256];
		BYTE keys[256];
		TStackRef<IQuadooFormKeyboard> keyboard;
		TStackRef<IQuadooControl> child;
		TStackRef<IQuadooWinControl> childWindow;
		HWND target;
		BOOL handled = FALSE;
		Check(form->ShowForm(SW_SHOWNOACTIVATE));
		Check(form->FindControl(RSTRING_CAST(L"ChildButton"),&child));
		Check(child->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&childWindow)));
		Check(childWindow->GetHandle(&target));
		Check(form->QueryInterface(__uuidof(IQuadooFormKeyboard),reinterpret_cast<PVOID*>(&keyboard)));
		REQUIRE(GetKeyboardState(previousKeys));
		CopyMemory(keys,previousKeys,sizeof(keys));
		keys[VK_CONTROL] = 0x80;
		keys[VK_SHIFT] = 0;
		keys[VK_MENU] = 0;
		MSG message = {target,WM_KEYDOWN,L'O',0};
		UINT before = binder->clicks;
		REQUIRE(SetKeyboardState(keys));
		hr = keyboard->PreprocessMessage(&message,&handled);
		SetKeyboardState(previousKeys);
		Check(hr);
		REQUIRE(handled && binder->clicks == before + 1);
		TStackRef<IQuadooComponent> popupItem;
		Check(components->FindComponent(RSTRING_CAST(L"PopupItem"),&popupItem));
		Check(MenuText(popupItem,L"ShortCut",RSTRING_CAST(L"Ctrl+P")));
		message.wParam = L'P';
		handled = FALSE;
		REQUIRE(SetKeyboardState(keys));
		hr = keyboard->PreprocessMessage(&message,&handled);
		SetKeyboardState(previousKeys);
		Check(hr);
		REQUIRE(handled && binder->clicks == before + 2);
		Check(MenuFlag(popupItem,L"Enabled",FALSE));
		handled = FALSE;
		REQUIRE(SetKeyboardState(keys));
		hr = keyboard->PreprocessMessage(&message,&handled);
		SetKeyboardState(previousKeys);
		Check(hr);
		REQUIRE(!handled && binder->clicks == before + 2);
	}
	Check(SetInt(form,L"BorderStyle",QuadooForm::BorderDialog));
	REQUIRE(GetClientRect(window,&client) && client.bottom == 180);
	Check(SetInt(form,L"BorderStyle",QuadooForm::BorderSizeable));
	REQUIRE(GetClientRect(window,&client) && client.bottom == 180);
	binder->destroyRoot = TRUE;
	native = GetMenu(window);
	SendMessageW(window,WM_MENUCOMMAND,0,reinterpret_cast<LPARAM>(GetSubMenu(native,0)));
	REQUIRE(!IsWindow(window) && !IsMenu(native));
	REQUIRE(item->Click() == QuadooForm::EDestroyed);
Cleanup:
	if(copy)
	{
		copy->Destroy();
	}
	if(form)
	{
		form->Destroy();
	}
	return hr;
}
static HRESULT MenuFailuresAndResources()
{
	HRESULT hr;
	TStackRef<IJSONObject> definition;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IQuadooControl> root;
	TStackRef<CMenuTestBinder> binder;
	for(UINT problem = 0; problem < 7; problem++)
	{
		TStackRef<IJSONValue> value;
		TStackRef<IJSONArray> list;
		TStackRef<IJSONObject> target;
		TStackRef<IJSONObject> properties;
		definition.Release();
		Check(Parse(MenuTestDefinition,&definition));
		Check(definition->FindNonNullValueW(L"components",&value));
		Check(value->GetArray(&list));
		value.Release();
		Check(list->GetObject(problem == 3 ? 1 : 2,&target));
		if(problem == 0)
		{
			Check(JSONCreateString(RSTRING_CAST(L"{F2000000-0000-0000-0000-000000000099}"),&value));
			Check(target->AddValueW(L"menuParent",value));
		}
		else if(problem == 1)
		{
			Check(JSONCreateString(RSTRING_CAST(L"{F2000000-0000-0000-0000-000000000012}"),&value));
			Check(target->AddValueW(L"menuParent",value));
		}
		else if(problem == 2)
		{
			Check(definition->FindNonNullValueW(L"properties",&value));
			Check(value->GetObject(&properties));
			value.Release();
			Check(JSONCreateString(RSTRING_CAST(L"{F2000000-0000-0000-0000-000000000020}"),&value));
			Check(properties->AddValueW(L"MainMenu",value));
		}
		else if(problem == 3)
		{
			Check(target->FindNonNullValueW(L"properties",&value));
			Check(value->GetObject(&properties));
			value.Release();
			Check(JSONCreateBool(true,&value));
			Check(properties->AddValueW(L"Separator",value));
		}
		else if(problem == 4)
		{
			TStackRef<IJSONObject> duplicate;
			Check(Parse(L"{\"type\":\"MainMenu\",\"name\":\"ExtraMainMenu\"}",&duplicate));
			Check(JSONWrapObject(duplicate,&value));
			Check(list->Add(value));
		}
		else if(problem == 5)
		{
			Check(target->FindNonNullValueW(L"properties",&value));
			Check(value->GetObject(&properties));
			value.Release();
			Check(JSONCreateString(RSTRING_CAST(L"Ctrl+NoSuchKey"),&value));
			Check(properties->AddValueW(L"ShortCut",value));
		}
		else
		{
			Check(target->FindNonNullValueW(L"properties",&value));
			Check(value->GetObject(&properties));
			value.Release();
			Check(JSONCreateString(RSTRING_CAST(L"Ctrl+Shift+N"),&value));
			Check(properties->AddValueW(L"ShortCut",value));
		}
		binder.Attach(__new CMenuTestBinder);
		CheckAlloc(binder);
		context.Release();
		Check(Context(96,binder,&context));
		REQUIRE(FAILED(QuadooCreateControl(context,NULL,definition,&root)) && !root);
		REQUIRE(binder->clicks == 0);
		if(problem < 6)
		{
			REQUIRE(binder->binds == 0);
		}
		binder.Release();
	}
	definition.Release();
	Check(Parse(MenuTestDefinition,&definition));
	{
		CGuiResourceMonitor resources(L"Native menu and accelerator lifetime");
		while(!resources.Complete())
		{
			TStackRef<IQuadooFormMenu> menus;
			TStackRef<IQuadooComponent> mainMenu;
			HWND window;
			HMENU native;
			context.Release();
			Check(Context(96,NULL,&context,NULL,QuadooForm::Preview));
			Check(QuadooCreateControl(context,NULL,definition,&root));
			{
				TStackRef<IQuadooWinControl> control;
				Check(root->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&control)));
				Check(control->GetHandle(&window));
			}
			Check(root->QueryInterface(__uuidof(IQuadooFormMenu),reinterpret_cast<PVOID*>(&menus)));
			Check(menus->GetMainMenu(&mainMenu));
			native = GetMenu(window);
			REQUIRE(IsMenu(native));
			Check(menus->SetMainMenu(NULL));
			REQUIRE(!IsMenu(native));
			Check(menus->SetMainMenu(mainMenu));
			native = GetMenu(window);
			Check(root->Destroy());
			REQUIRE(!IsWindow(window) && !IsMenu(native));
			root.Release();
			menus.Release();
			mainMenu.Release();
			Check(resources.AfterCycle());
		}
	}
Cleanup:
	if(root)
	{
		root->Destroy();
	}
	return hr;
}
static BOOL s_menuDismissed;
static VOID CALLBACK DismissNativePopup(HWND window, UINT, UINT_PTR id, DWORD)
{
	s_menuDismissed = TRUE;
	KillTimer(window,id);
	EndMenu();
}
static HRESULT FramePopupMenus()
{
	HRESULT hr;
	TStackRef<IQuadooForm> form;
	TStackRef<IQuadooControl> frame;
	TStackRef<IQuadooControlContainer> parent;
	TStackRef<IQuadooConstructionContext> context;
	TStackRef<IQuadooComponentHost> host;
	TStackRef<IQuadooComponentCollection> components;
	TStackRef<IQuadooComponent> component;
	TStackRef<IQuadooPopupMenu> popup;
	TStackRef<IJSONObject> definition;
	HWND owner = NULL;
	HWND frameWindow;
	Check(Parse(L"{\"type\":\"Form\",\"name\":\"PopupOwner\"}",&definition));
	Check(QuadooCreateForm(NULL,NULL,definition,&form));
	Check(form->GetHandle(&owner));
	Check(form->ShowForm(SW_SHOWNOACTIVATE));
	Check(form->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&parent)));
	definition.Release();
	Check(Parse(L"{\"type\":\"Frame\",\"name\":\"PopupFrame\",\"components\":[{\"type\":\"PopupMenu\",\"name\":\"FramePopup\",\"id\":\"{F2100000-0000-0000-0000-000000000001}\"},{\"type\":\"MenuItem\",\"name\":\"FramePopupItem\",\"menuParent\":\"{F2100000-0000-0000-0000-000000000001}\",\"properties\":{\"Caption\":\"Popup action\"}}]}",&definition));
	Check(Context(96,NULL,&context));
	Check(QuadooCreateControl(context,parent,definition,&frame));
	Check(frame->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&host)));
	Check(host->GetComponents(&components));
	Check(components->FindComponent(RSTRING_CAST(L"FramePopup"),&component));
	Check(component->QueryInterface(__uuidof(IQuadooPopupMenu),reinterpret_cast<PVOID*>(&popup)));
	{
		TStackRef<IQuadooWinControl> native;
		Check(frame->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native)));
		Check(native->GetHandle(&frameWindow));
	}
	REQUIRE(GetMenu(frameWindow) == NULL && GetMenu(owner) == NULL);
	s_menuDismissed = FALSE;
	CheckIf(!SetTimer(owner,6021,30,DismissNativePopup),HrEnsureLastError());
	hr = popup->Popup(NULL,30,30);
	KillTimer(owner,6021);
	REQUIRE(hr == S_FALSE && s_menuDismissed);
	definition.Release();
	Check(Parse(L"{\"type\":\"MainMenu\",\"name\":\"NotAllowedOnFrame\"}",&definition));
	component.Release();
	REQUIRE(FAILED(QuadooCreateComponent(context,components,definition,&component)) && !component);
Cleanup:
	if(owner)
	{
		KillTimer(owner,6021);
	}
	if(frame)
	{
		frame->Destroy();
	}
	if(form)
	{
		form->Destroy();
	}
	return hr;
}
static HRESULT NativeMenus()
{
	HRESULT hr;
	Check(NativeMenusCore());
	Check(FramePopupMenus());
	Check(MenuFailuresAndResources());
Cleanup:
	return hr;
}
