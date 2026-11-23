#include "Runtime.h"
#include "MenuRuntime.h"

namespace QF
{
	static BOOL Token(PCWSTR text, INT length, PCWSTR other, INT count)
	{
		return length == count && CompareStringW(LOCALE_INVARIANT,NORM_IGNORECASE,text,length,other,count) == CSTR_EQUAL;
	}

	HRESULT ParseMenuShortcut(RSTRING text, ACCEL* accelerator)
	{
		HRESULT hr = S_OK;
		INT length = RStrLen(text);
		UINT start = 0;
		ZeroMemory(accelerator,sizeof(*accelerator));
		accelerator->fVirt = FVIRTKEY;
		CheckIfIgnore(!length,S_FALSE);
		CheckIf(!RStrIsWide(text) || length > 80,E_INVALIDARG);
		for(INT end = 0; end <= length; end++)
		{
			if(end == length || RStrToWide(text)[end] == L'+')
			{
				PCWSTR part = RStrToWide(text) + start;
				INT count = end - start;
				BYTE modifier = 0;
				CheckIf(!count,E_INVALIDARG);
				if(Token(part,count,SLP(L"Ctrl")) || Token(part,count,SLP(L"Control")))
				{
					modifier = FCONTROL;
				}
				else if(Token(part,count,SLP(L"Shift")))
				{
					modifier = FSHIFT;
				}
				else if(Token(part,count,SLP(L"Alt")))
				{
					modifier = FALT;
				}
				if(modifier)
				{
					CheckIf(end == length || (accelerator->fVirt & modifier),E_INVALIDARG);
					accelerator->fVirt |= modifier;
				}
				else
				{
					CheckIf(end != length,E_INVALIDARG);
					if(count == 1 && ((part[0] >= L'a' && part[0] <= L'z') || (part[0] >= L'A' && part[0] <= L'Z') || (part[0] >= L'0' && part[0] <= L'9')))
					{
						accelerator->key = part[0] >= L'a' ? part[0] - L'a' + L'A' : part[0];
					}
					else if(count >= 2 && count <= 3 && (part[0] == L'F' || part[0] == L'f'))
					{
						UINT number = 0;
						for(INT n = 1; n < count; n++)
						{
							CheckIf(part[n] < L'0' || part[n] > L'9',E_INVALIDARG);
							number = number * 10 + part[n] - L'0';
						}
						CheckIf(number < 1 || number > 24,E_INVALIDARG);
						accelerator->key = VK_F1 + number - 1;
					}
					else
					{
						struct Key
						{
							PCWSTR name;
							INT length;
							WORD key;
						};
						static const Key keys[] =
						{
							{SLP(L"Delete"),VK_DELETE},{SLP(L"Del"),VK_DELETE},
							{SLP(L"Insert"),VK_INSERT},{SLP(L"Ins"),VK_INSERT},
							{SLP(L"Enter"),VK_RETURN},{SLP(L"Return"),VK_RETURN},
							{SLP(L"Escape"),VK_ESCAPE},{SLP(L"Esc"),VK_ESCAPE},
							{SLP(L"Space"),VK_SPACE},{SLP(L"Tab"),VK_TAB},
							{SLP(L"Backspace"),VK_BACK},{SLP(L"Home"),VK_HOME},{SLP(L"End"),VK_END},
							{SLP(L"Left"),VK_LEFT},{SLP(L"Right"),VK_RIGHT},{SLP(L"Up"),VK_UP},{SLP(L"Down"),VK_DOWN},
							{SLP(L"PageUp"),VK_PRIOR},{SLP(L"PageDown"),VK_NEXT}
						};
						for(UINT n = 0; n < ARRAYSIZE(keys); n++)
						{
							if(Token(part,count,keys[n].name,keys[n].length))
							{
								accelerator->key = keys[n].key;
								break;
							}
						}
						CheckIf(!accelerator->key,E_INVALIDARG);
					}
				}
				start = end + 1;
			}
		}
	Cleanup:
		return hr;
	}

	CNativeMenu::CNativeMenu() : m_handle(NULL), m_accelerators(NULL)
	{
	}

	CNativeMenu::~CNativeMenu()
	{
		if(m_handle)
		{
			DestroyMenu(m_handle);
		}
		if(m_accelerators)
		{
			DestroyAcceleratorTable(m_accelerators);
		}
		for(sysint n = 0; n < m_items.Length(); n++)
		{
			m_items[n]->Release();
		}
	}

	HMENU CNativeMenu::Handle() const
	{
		return m_handle;
	}

	HRESULT CNativeMenu::BuildItems(IQuadooComponent* parent, HMENU menu, BOOL enabled, UINT depth)
	{
		HRESULT hr;
		TStackRef<IQuadooMenuItems> children;
		TStackRef<IQuadooMenuItem> parentItem;
		UINT count;
		MENUINFO info = {sizeof(info)};
		CheckIf(depth >= 128,QuadooForm::EInvalidDefinition);
		Check(parent->QueryInterface(__uuidof(IQuadooMenuItems),reinterpret_cast<PVOID*>(&children)));
		Check(children->GetItemCount(&count));
		info.fMask = MIM_STYLE | MIM_MENUDATA;
		info.dwStyle = MNS_NOTIFYBYPOS;
		for(sysint n = 0; n < m_items.Length(); n++)
		{
			if(SameObject(m_items[n],parent))
			{
				info.dwMenuData = n + 1;
				break;
			}
		}
		CheckIf(!SetMenuInfo(menu,&info),HrEnsureLastError());
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IQuadooComponent> component;
			TStackRef<IQuadooMenuItem> item;
			TStackRef<IQuadooMenuItems> subitems;
			TStackRef<IJSONValue> value;
			CRString caption;
			CRString shortcut;
			CRString display;
			BOOL visible;
			BOOL active;
			BOOL checked;
			BOOL separator;
			UINT descendants;
			HMENU popup = NULL;
			Check(children->GetItem(n,&component));
			Check(component->QueryInterface(__uuidof(IQuadooMenuItem),reinterpret_cast<PVOID*>(&item)));
			Check(MenuBool(component,L"Visible",&visible));
			if(!visible)
			{
				continue;
			}
			Check(item->GetEnabled(&active));
			Check(item->GetChecked(&checked));
			Check(MenuBool(component,L"Separator",&separator));
			Check(component->QueryInterface(__uuidof(IQuadooMenuItems),reinterpret_cast<PVOID*>(&subitems)));
			Check(subitems->GetItemCount(&descendants));
			CheckIf(separator && descendants,QuadooForm::EInvalidDefinition);
			Check(MenuProperty(component,L"Caption",&value));
			Check(value->GetString(&caption));
			value.Release();
			Check(MenuProperty(component,L"ShortCut",&value));
			Check(value->GetString(&shortcut));
			if(shortcut.Length())
			{
				Check(RStrFormatW(&display,L"%r\t%r",static_cast<RSTRING>(caption),static_cast<RSTRING>(shortcut)));
			}
			else
			{
				display = caption;
			}
			CheckIf(m_items.Length() >= 4096,QuadooForm::EInvalidDefinition);
			Check(m_items.Append(component));
			component->AddRef();
			UINT command = static_cast<UINT>(m_items.Length());
			MENUITEMINFOW entry = {sizeof(entry)};
			entry.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_ID;
			entry.fType = separator ? MFT_SEPARATOR : MFT_STRING;
			entry.fState = (active && enabled ? MFS_ENABLED : MFS_DISABLED) | (checked ? MFS_CHECKED : MFS_UNCHECKED);
			entry.wID = command;
			if(!separator)
			{
				entry.fMask |= MIIM_STRING;
				entry.dwTypeData = const_cast<PWSTR>(display.c_wstr());
				entry.cch = display.Length();
			}
			if(descendants)
			{
				popup = CreatePopupMenu();
				CheckIf(!popup,HrEnsureLastError());
				entry.fMask |= MIIM_SUBMENU;
				entry.hSubMenu = popup;
			}
			if(!InsertMenuItemW(menu,GetMenuItemCount(menu),TRUE,&entry))
			{
				if(popup)
				{
					DestroyMenu(popup);
				}
				Check(E_OUTOFMEMORY);
			}
			if(popup)
			{
				Check(BuildItems(component,popup,active && enabled,depth + 1));
			}
			if(shortcut.Length() && !separator && !descendants)
			{
				ACCEL key;
				Check(ParseMenuShortcut(shortcut,&key));
				key.cmd = static_cast<WORD>(command);
				for(sysint k = 0; k < m_keys.Length(); k++)
				{
					CheckIf(m_keys[k].key == key.key && m_keys[k].fVirt == key.fVirt,QuadooForm::EInvalidDefinition);
				}
				Check(m_keys.Append(key));
			}
		}
	Cleanup:
		return hr;
	}

	HRESULT CNativeMenu::Build(IQuadooComponent* root, BOOL popup)
	{
		HRESULT hr;
		CheckIf(m_handle,QuadooForm::EInvalidState);
		m_handle = popup ? CreatePopupMenu() : CreateMenu();
		CheckIf(!m_handle,HrEnsureLastError());
		Check(BuildItems(root,m_handle,TRUE,0));
		if(m_keys.Length())
		{
			m_accelerators = CreateAcceleratorTableW(&m_keys[0],static_cast<INT>(m_keys.Length()));
			CheckIf(!m_accelerators,HrEnsureLastError());
		}
	Cleanup:
		return hr;
	}

	BOOL CNativeMenu::Translate(HWND window, MSG* message)
	{
		TStackRef<CNativeMenu> keepAlive(this);
		return m_accelerators && TranslateAcceleratorW(window,m_accelerators,message);
	}

	HRESULT CNativeMenu::Invoke(UINT command)
	{
		HRESULT hr;
		TStackRef<IQuadooMenuItem> item;
		TStackRef<CNativeMenu> keepAlive(this);
		CheckIf(!command || command > static_cast<UINT>(m_items.Length()),DISP_E_BADINDEX);
		Check(m_items[command - 1]->QueryInterface(__uuidof(IQuadooMenuItem),reinterpret_cast<PVOID*>(&item)));
		Check(item->Click());
	Cleanup:
		return hr;
	}

	static BOOL OwnsNativeMenu(HMENU parent, HMENU menu, UINT depth)
	{
		if(parent == menu)
		{
			return TRUE;
		}
		if(depth < 128)
		{
			for(INT n = 0; n < GetMenuItemCount(parent); n++)
			{
				HMENU child = GetSubMenu(parent,n);
				if(child && OwnsNativeMenu(child,menu,depth + 1))
				{
					return TRUE;
				}
			}
		}
		return FALSE;
	}

	BOOL CNativeMenu::Owns(HMENU menu) const
	{
		return menu && m_handle && OwnsNativeMenu(m_handle,menu,0);
	}

	HRESULT CNativeMenu::InvokePosition(HMENU menu, UINT position)
	{
		HRESULT hr;
		TStackRef<CNativeMenu> keepAlive(this);
		CheckIf(!Owns(menu),E_INVALIDARG);
		UINT command = GetMenuItemID(menu,position);
		CheckIf(command == static_cast<UINT>(-1),E_INVALIDARG);
		Check(Invoke(command));
	Cleanup:
		return hr;
	}

	HRESULT CNativeMenu::Opening(HMENU menu)
	{
		HRESULT hr;
		TStackRef<CNativeMenu> keepAlive(this);
		MENUINFO info = {sizeof(info)};
		info.fMask = MIM_MENUDATA;
		CheckIf(!Owns(menu),E_INVALIDARG);
		CheckIf(!GetMenuInfo(menu,&info),HrEnsureLastError());
		if(info.dwMenuData)
		{
			Check(Invoke(static_cast<UINT>(info.dwMenuData)));
		}
		Check(Synchronize());
	Cleanup:
		return hr;
	}

	static HRESULT SyncMenu(HMENU menu, const TArray<IQuadooComponent*>& items, BOOL parentEnabled, UINT depth)
	{
		HRESULT hr = S_OK;
		CheckIf(depth >= 128,E_INVALIDARG);
		for(INT n = 0; n < GetMenuItemCount(menu); n++)
		{
			MENUITEMINFOW entry = {sizeof(entry)};
			entry.fMask = MIIM_ID | MIIM_SUBMENU | MIIM_FTYPE;
			CheckIf(!GetMenuItemInfoW(menu,n,TRUE,&entry),HrEnsureLastError());
			CheckIf(!entry.wID || entry.wID > static_cast<UINT>(items.Length()),E_INVALIDARG);
			IQuadooComponent* component = items[entry.wID - 1];
			BOOL enabled;
			BOOL checked;
			BOOL visible;
			QuadooForm::ComponentState state;
			Check(component->GetState(&state));
			Check(MenuBool(component,L"Enabled",&enabled));
			Check(MenuBool(component,L"Checked",&checked));
			Check(MenuBool(component,L"Visible",&visible));
			enabled = enabled && parentEnabled && visible && state < QuadooForm::Destroying;
			MENUITEMINFOW update = {sizeof(update)};
			update.fMask = MIIM_STATE;
			update.fState = (enabled ? MFS_ENABLED : MFS_DISABLED) | (checked ? MFS_CHECKED : MFS_UNCHECKED);
			CRString caption;
			CRString shortcut;
			CRString display;
			TStackRef<IJSONValue> value;
			if(!(entry.fType & MFT_SEPARATOR))
			{
				Check(MenuProperty(component,L"Caption",&value));
				Check(value->GetString(&caption));
				value.Release();
				Check(MenuProperty(component,L"ShortCut",&value));
				Check(value->GetString(&shortcut));
				if(shortcut.Length())
				{
					Check(RStrFormatW(&display,L"%r	%r",static_cast<RSTRING>(caption),static_cast<RSTRING>(shortcut)));
				}
				else
				{
					display = caption;
				}
				update.fMask |= MIIM_STRING;
				update.dwTypeData = const_cast<PWSTR>(display.c_wstr());
				update.cch = display.Length();
			}
			CheckIf(!SetMenuItemInfoW(menu,n,TRUE,&update),HrEnsureLastError());
			if(entry.hSubMenu)
			{
				Check(SyncMenu(entry.hSubMenu,items,enabled,depth + 1));
			}
		}
	Cleanup:
		return hr;
	}

	HRESULT CNativeMenu::Synchronize()
	{
		return SyncMenu(m_handle,m_items,TRUE,0);
	}

	static HRESULT MatchPopupShortcut(IQuadooComponent* component, MSG* message, BOOL* handled, UINT depth)
	{
		HRESULT hr;
		TStackRef<IQuadooMenuItems> items;
		TStackRef<IQuadooMenuItem> item;
		UINT count;
		CheckIf(depth >= 128,QuadooForm::EInvalidDefinition);
		Check(QueryOptional(component,&item));
		if(item)
		{
			BOOL visible;
			BOOL enabled;
			BOOL separator;
			Check(item->GetEnabled(&enabled));
			Check(MenuBool(component,L"Visible",&visible));
			Check(MenuBool(component,L"Separator",&separator));
			CheckIfIgnore(!enabled || !visible || separator,S_OK);
		}
		Check(component->QueryInterface(__uuidof(IQuadooMenuItems),reinterpret_cast<PVOID*>(&items)));
		Check(items->GetItemCount(&count));
		if(!count && item)
		{
			TStackRef<IJSONValue> value;
			CRString shortcut;
			ACCEL key;
			Check(MenuProperty(component,L"ShortCut",&value));
			Check(value->GetString(&shortcut));
			hr = ParseMenuShortcut(shortcut,&key);
			CheckIfIgnore(hr == S_FALSE,S_OK);
			Check(hr);
			BYTE modifiers = (GetKeyState(VK_CONTROL) & 0x8000 ? FCONTROL : 0) |
				(GetKeyState(VK_SHIFT) & 0x8000 ? FSHIFT : 0) | (GetKeyState(VK_MENU) & 0x8000 ? FALT : 0);
			if(message->wParam == key.key && (key.fVirt & (FCONTROL | FSHIFT | FALT)) == modifiers)
			{
				Check(item->Click());
				*handled = TRUE;
			}
		}
		for(UINT n = 0; n < count && !*handled; n++)
		{
			TStackRef<IQuadooComponent> child;
			Check(items->GetItem(n,&child));
			Check(MatchPopupShortcut(child,message,handled,depth + 1));
		}
	Cleanup:
		return hr;
	}

	HRESULT TranslatePopupShortcuts(IQuadooControl* root, MSG* message, BOOL* handled, UINT depth)
	{
		HRESULT hr;
		TStackRef<IQuadooComponentHost> host;
		TStackRef<IQuadooControlContainer> container;
		CheckIf(depth >= 128,QuadooForm::EInvalidDefinition);
		Check(QueryOptional(root,&host));
		if(host)
		{
			TStackRef<IQuadooComponentCollection> components;
			UINT count;
			Check(host->GetComponents(&components));
			Check(components->GetCount(&count));
			for(UINT n = 0; n < count && !*handled; n++)
			{
				TStackRef<IQuadooComponent> component;
				TStackRef<IQuadooMenu> menu;
				Check(components->GetAt(n,&component));
				Check(QueryOptional(component,&menu));
				if(menu)
				{
					BOOL popup;
					Check(menu->IsPopup(&popup));
					if(popup)
					{
						Check(MatchPopupShortcut(component,message,handled,0));
					}
				}
			}
		}
		CheckIfIgnore(*handled,S_OK);
		Check(QueryOptional(root,&container));
		if(container)
		{
			UINT count;
			Check(container->GetChildCount(&count));
			for(UINT n = 0; n < count && !*handled; n++)
			{
				TStackRef<IQuadooControl> child;
				BOOL visible;
				BOOL enabled;
				Check(container->GetChild(n,&child));
				Check(child->GetVisible(&visible));
				Check(child->GetEnabled(&enabled));
				if(visible && enabled)
				{
					Check(TranslatePopupShortcuts(child,message,handled,depth + 1));
				}
			}
		}
	Cleanup:
		return hr;
	}

	HRESULT FitMenuClient(HWND window, INT width, INT height)
	{
		HRESULT hr = S_OK;
		for(UINT attempt = 0; attempt < 8; attempt++)
		{
			RECT client;
			RECT outer;
			CheckIf(!GetClientRect(window,&client) || !GetWindowRect(window,&outer),HrEnsureLastError());
			INT dx = width - client.right;
			INT dy = height - client.bottom;
			if(!dx && !dy)
			{
				goto Cleanup;
			}
			CheckIf(!SetWindowPos(window,NULL,0,0,outer.right - outer.left + dx,outer.bottom - outer.top + dy,SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING),HrEnsureLastError());
		}
		hr = E_FAIL;
	Cleanup:
		return hr;
	}

	STDMETHODIMP CForm::GetMainMenu(IQuadooComponent** menu)
	{
		HRESULT hr;
		TStackRef<IJSONValue> value;
		TStackRef<IQuadooComponentCollection> owner;
		CheckIf(!menu,E_POINTER);
		*menu = NULL;
		Check(CheckThread());
		Check(StoredValue(L"MainMenu",&value));
		CheckIfIgnore(!value,S_OK);
		GUID id;
		Check(value->GetStringAsGuid(id));
		Check(GetComponents(&owner));
		Check(FindComponentID(owner,id,menu));
	Cleanup:
		return hr;
	}

	STDMETHODIMP CForm::SetMainMenu(IQuadooComponent* menu)
	{
		HRESULT hr;
		TStackRef<IJSONValue> value;
		TStackRef<IQuadooComponentCollection> owner;
		TStackRef<IQuadooComponent> found;
		TStackRef<IQuadooMenu> capability;
		CRString text;
		GUID id;
		Check(CheckAlive());
		if(menu)
		{
			BOOL popup;
			Check(menu->QueryInterface(__uuidof(IQuadooMenu),reinterpret_cast<PVOID*>(&capability)));
			Check(capability->IsPopup(&popup));
			CheckIf(popup,E_INVALIDARG);
			Check(menu->GetID(&id));
			Check(GetComponents(&owner));
			Check(FindComponentID(owner,id,&found));
			CheckIf(!SameObject(found,menu),E_INVALIDARG);
			Check(GuidString(id,&text));
			Check(JSONCreateString(text,&value));
		}
		Check(SetValueJSON(FindProperty(L"MainMenu"),value));
	Cleanup:
		return hr;
	}

	STDMETHODIMP CForm::RefreshMenu()
	{
		HRESULT hr;
		CNativeMenu* candidate = NULL;
		TStackRef<CForm> keepAlive(this);
		TStackRef<CNativeMenu> previous;
		TStackRef<IQuadooComponent> menu;
		QuadooForm::ContextMode mode;
		RECT client;
		BOOL entered = FALSE;
		Check(CheckAlive());
		Check(m_context->GetMode(&mode));
		Check(GetMainMenu(&menu));
		if(menu)
		{
			TStackRef<IQuadooMenu> capability;
			BOOL popup;
			Check(menu->QueryInterface(__uuidof(IQuadooMenu),reinterpret_cast<PVOID*>(&capability)));
			Check(capability->IsPopup(&popup));
			CheckIf(popup,E_INVALIDARG);
		}
		CheckIfIgnore(mode == QuadooForm::Design,S_OK);
		if(m_menuLoops)
		{
			m_menuPending = TRUE;
			hr = m_menu ? m_menu->Synchronize() : S_OK;
			goto Cleanup;
		}
		if(menu)
		{
			candidate = __new CNativeMenu;
			CheckAlloc(candidate);
			Check(candidate->Build(menu,FALSE));
		}
		CheckIf(!GetClientRect(m_hwnd,&client),HrEnsureLastError());
		m_menuAdjusting = TRUE;
		entered = TRUE;
		CheckIf(!SetMenu(m_hwnd,candidate ? candidate->Handle() : NULL),HrEnsureLastError());
		DrawMenuBar(m_hwnd);
		hr = FitMenuClient(m_hwnd,client.right,client.bottom);
		if(FAILED(hr))
		{
			SetMenu(m_hwnd,m_menu ? m_menu->Handle() : NULL);
			FitMenuClient(m_hwnd,client.right,client.bottom);
			goto Cleanup;
		}
		previous.Attach(m_menu);
		m_menu = candidate;
		candidate = NULL;
		DrawMenuBar(m_hwnd);
		m_menuPending = FALSE;
		m_menuAdjusting = FALSE;
		entered = FALSE;
		hr = UpdateLayout();
		if(FAILED(hr) && m_state < QuadooForm::Destroying && m_hwnd)
		{
			m_menuAdjusting = TRUE;
			entered = TRUE;
			SetMenu(m_hwnd,previous ? previous->Handle() : NULL);
			SafeRelease(m_menu);
			m_menu = previous.Detach();
			FitMenuClient(m_hwnd,client.right,client.bottom);
			DrawMenuBar(m_hwnd);
		}
	Cleanup:
		if(entered)
		{
			m_menuAdjusting = FALSE;
		}
		SafeRelease(candidate);
		return hr;
	}

	HRESULT CForm::ArrangeNative(const QuadooForm::Rect* bounds, BOOL changed)
	{
		HRESULT hr;
		BOOL previous = m_menuAdjusting;
		m_menuAdjusting = TRUE;
		Check(CWinControl::ArrangeNative(bounds,changed));
		if(m_hwnd && m_menu)
		{
			INT width = MulDiv(bounds->Width,m_dpi,96);
			INT height = MulDiv(bounds->Height,m_dpi,96);
			DWORD style = GetWindowLongW(m_hwnd,GWL_STYLE);
			if(style & WS_VSCROLL)
			{
				width -= GetSystemMetrics(SM_CXVSCROLL);
			}
			if(style & WS_HSCROLL)
			{
				height -= GetSystemMetrics(SM_CYHSCROLL);
			}
			Check(FitMenuClient(m_hwnd,max(0,width),max(0,height)));
		}
		m_menuAdjusting = previous;
		Check(UpdateLayout());
	Cleanup:
		m_menuAdjusting = previous;
		return hr;
	}

	BOOL CForm::TranslateMenu(MSG* message)
	{
		// TranslateAccelerator synchronously invokes callbacks which may rebuild or
		// destroy the form. Do not touch this menu object after that call returns.
		return m_menu && m_menu->Translate(m_hwnd,message);
	}
	HRESULT ShowControlPopup(IQuadooControl* control, POINT screen, BOOL* handled)
	{
		HRESULT hr;
		TStackRef<IJSONValue> reference;
		TStackRef<IQuadooControlContainer> root;
		TStackRef<IQuadooComponentHost> host;
		TStackRef<IQuadooComponentCollection> components;
		TStackRef<IQuadooComponent> component;
		TStackRef<IQuadooPopupMenu> popup;
		hr = MenuProperty(control,L"PopupMenu",&reference);
		CheckIfIgnore(hr == DISP_E_MEMBERNOTFOUND,S_FALSE);
		Check(hr);
		CheckIfIgnore(!reference,S_FALSE);
		GUID id;
		Check(reference->GetStringAsGuid(id));
		Check(GetNamespace(control,&root));
		Check(root->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&host)));
		Check(host->GetComponents(&components));
		Check(FindComponentID(components,id,&component));
		Check(component->QueryInterface(__uuidof(IQuadooPopupMenu),reinterpret_cast<PVOID*>(&popup)));
		if(screen.x == -1 && screen.y == -1)
		{
			HWND window;
			TStackRef<IQuadooWinControl> native;
			Check(control->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native)));
			Check(native->GetHandle(&window));
			screen.x = 0;
			screen.y = 0;
			ClientToScreen(window,&screen);
		}
		*handled = TRUE;
		Check(popup->Popup(NULL,screen.x,screen.y));
	Cleanup:
		return hr;
	}
}
