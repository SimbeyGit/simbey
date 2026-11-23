#include "Runtime.h"
#include "ComponentRuntime.h"
#include "MenuRuntime.h"

namespace QF
{
	static const DWORD Stored = QuadooForm::PropertySerializable;
	static const PropertySpec itemProperties[] =
	{
		{L"Caption",QuadooProperty::String,Stored,0,1},
		{L"Enabled",QuadooProperty::Boolean,Stored,1,2},
		{L"Visible",QuadooProperty::Boolean,Stored,1,3},
		{L"Checked",QuadooProperty::Boolean,Stored,0,4},
		{L"Separator",QuadooProperty::Boolean,Stored,0,5},
		{L"ShortCut",QuadooProperty::String,Stored,0,6},
		{L"AutoCheck",QuadooProperty::Boolean,Stored,0,7},
		{L"Order",QuadooProperty::Int,Stored,0,8}
	};
	static const EventSpec itemEvents[] = {{L"OnClick",0,0,NULL,2}};
	static const TypeSpec itemType = {L"MenuItem",&NonvisualType,itemProperties,ARRAYSIZE(itemProperties),itemEvents,ARRAYSIZE(itemEvents),IDI_PALETTE_MENUITEM};
	static const TypeSpec mainType = {L"MainMenu",&NonvisualType,NULL,0,NULL,0,IDI_PALETTE_MAINMENU};
	static const TypeSpec popupType = {L"PopupMenu",&NonvisualType,NULL,0,NULL,0,IDI_PALETTE_POPUPMENU};

	HRESULT MenuProperty(IQuadooComponent* component, PCWSTR name, IJSONValue** value)
	{
		HRESULT hr;
		TStackRef<IQuadooProperties> properties;
		TStackRef<IQuadooPropertyDescriptors> descriptors;
		UINT count;

		CheckIf(!component || !value,E_POINTER);
		*value = NULL;

		Check(component->GetProperties(&properties));
		Check(properties->GetDescriptors(&descriptors));
		Check(descriptors->GetCount(&count));
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IQuadooPropertyDescriptor> descriptor;
			CRString candidate;

			Check(descriptors->GetAt(n,&descriptor));
			Check(descriptor->GetName(&candidate));
			if(SameString(candidate,RSTRING_CAST(name)))
			{
				Check(properties->GetValueJSON(n,value));
				goto Cleanup;
			}
		}
		hr = DISP_E_MEMBERNOTFOUND;

	Cleanup:
		return hr;
	}

	HRESULT MenuInt(IQuadooComponent* component, PCWSTR name, INT* value)
	{
		HRESULT hr;
		TStackRef<IJSONValue> json;
		Check(MenuProperty(component,name,&json));
		CheckIf(!json,E_INVALIDARG);
		Check(json->GetInteger(value));
	Cleanup:
		return hr;
	}

	HRESULT MenuBool(IQuadooComponent* component, PCWSTR name, BOOL* value)
	{
		HRESULT hr;
		TStackRef<IJSONValue> json;
		bool flag;
		CheckIf(!value,E_POINTER);
		Check(MenuProperty(component,name,&json));
		CheckIf(!json,E_INVALIDARG);
		Check(json->GetBoolean(&flag));
		*value = flag;
	Cleanup:
		return hr;
	}

	HRESULT FindComponentID(IQuadooComponentCollection* owner, REFGUID id, IQuadooComponent** component)
	{
		HRESULT hr;
		UINT count;
		CheckIf(!owner || !component,E_POINTER);
		*component = NULL;
		Check(owner->GetCount(&count));
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IQuadooComponent> item;
			GUID current;
			Check(owner->GetAt(n,&item));
			Check(item->GetID(&current));
			if(IsEqualGUID(current,id))
			{
				*component = item.Detach();
				goto Cleanup;
			}
		}
		hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	Cleanup:
		return hr;
	}

	static HRESULT Owner(IQuadooComponent* component, IQuadooComponentCollection** owner)
	{
		HRESULT hr;
		TStackRef<IQuadooNonvisualComponent> nonvisual;
		Check(component->QueryInterface(__uuidof(IQuadooNonvisualComponent),reinterpret_cast<PVOID*>(&nonvisual)));
		Check(nonvisual->GetOwner(owner));
		CheckIf(!*owner,QuadooForm::EDestroyed);
	Cleanup:
		return hr;
	}

	HRESULT MenuChildren(IQuadooComponent* parent, TArray<IQuadooComponent*>& items)
	{
		HRESULT hr;
		TStackRef<IQuadooComponentCollection> owner;
		GUID id;
		UINT count;
		Check(Owner(parent,&owner));
		Check(parent->GetID(&id));
		Check(owner->GetCount(&count));
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IQuadooComponent> item;
			TStackRef<IQuadooMenuItem> menu;
			QuadooForm::ComponentState state;
			GUID current;
			Check(owner->GetAt(n,&item));
			Check(item->GetState(&state));
			if(state >= QuadooForm::Destroying)
			{
				continue;
			}
			Check(QueryOptional(item,&menu));
			if(menu)
			{
				Check(menu->GetMenuParentID(&current));
				if(IsEqualGUID(current,id))
				{
					INT order;
					Check(MenuInt(item,L"Order",&order));
					sysint position = items.Length();
					while(position)
					{
						INT previous;
						Check(MenuInt(items[position - 1],L"Order",&previous));
						if(previous <= order)
						{
							break;
						}
						position--;
					}
					Check(items.InsertAt(static_cast<IQuadooComponent*>(item),position));
					item.Detach();
				}
			}
		}
	Cleanup:
		return hr;
	}

	static HRESULT ItemCount(IQuadooComponent* parent, UINT* count)
	{
		HRESULT hr;
		TArray<IQuadooComponent*> items;
		CheckIf(!count,E_POINTER);
		*count = 0;
		Check(MenuChildren(parent,items));
		*count = static_cast<UINT>(items.Length());
	Cleanup:
		for(sysint n = 0; n < items.Length(); n++)
		{
			items[n]->Release();
		}
		return hr;
	}

	static HRESULT ItemAt(IQuadooComponent* parent, UINT index, IQuadooComponent** item)
	{
		HRESULT hr;
		TArray<IQuadooComponent*> items;
		CheckIf(!item,E_POINTER);
		*item = NULL;
		Check(MenuChildren(parent,items));
		CheckIf(index >= static_cast<UINT>(items.Length()),DISP_E_BADINDEX);
		*item = items[index];
		(*item)->AddRef();
	Cleanup:
		for(sysint n = 0; n < items.Length(); n++)
		{
			items[n]->Release();
		}
		return hr;
	}

	HRESULT RefreshOwnerMenu(IQuadooComponentCollection* owner)
	{
		HRESULT hr = S_OK;
		TStackRef<IQuadooControlContainer> root;
		TStackRef<IQuadooFormMenu> form;
		QuadooForm::ComponentState state;
		if(owner)
		{
			Check(owner->GetRoot(&root));
			Check(root->GetState(&state));
			if(state == QuadooForm::Ready)
			{
				Check(QueryOptional(root,&form));
				if(form)
				{
					Check(form->RefreshMenu());
				}
			}
		}
	Cleanup:
		return hr;
	}

	static HRESULT DestroyMenuChildren(IQuadooComponent* parent)
	{
		HRESULT hr;
		TArray<IQuadooComponent*> items;
		Check(MenuChildren(parent,items));
		for(sysint n = items.Length(); n > 0; n--)
		{
			Check(items[n - 1]->Destroy());
		}
	Cleanup:
		for(sysint n = 0; n < items.Length(); n++)
		{
			items[n]->Release();
		}
		return hr;
	}

	class CMenuRoot : public CNonvisual, public IQuadooMenu, public IQuadooMenuItems
	{
	protected:
		virtual HRESULT DestroyObject()
		{
			HRESULT hr;
			TStackRef<IQuadooControlContainer> root;
			TStackRef<IQuadooFormMenu> form;
			TStackRef<IQuadooComponent> current;
			Check(m_owner->GetRoot(&root));
			Check(QueryOptional(root,&form));
			QuadooForm::ComponentState state;
			Check(root->GetState(&state));
			if(form && state < QuadooForm::Destroying)
			{
				Check(form->GetMainMenu(&current));
				if(SameObject(current,static_cast<IQuadooComponent*>(this)))
				{
					Check(form->SetMainMenu(NULL));
				}
			}
			Check(DestroyMenuChildren(this));
		Cleanup:
			return hr;
		}
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooMenu)
			UNK_INTERFACE(IQuadooMenuItems)
			UNK_CHAIN_TO(CNonvisual)
		END_UNK_MAP
		CMenuRoot(IQuadooConstructionContext* context, IQuadooComponentCollection* owner, IUnknown* factory, UINT* live) : CNonvisual(context,owner,factory,live)
		{
		}
		STDMETHOD(GetItemCount)(UINT* count)
		{
			return ItemCount(this,count);
		}
		STDMETHOD(GetItem)(UINT index, IQuadooComponent** item)
		{
			return ItemAt(this,index,item);
		}
	};

	class CMainMenu : public CMenuRoot, public IQuadooUniqueComponent
	{
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooUniqueComponent)
			UNK_CHAIN_TO(CMenuRoot)
		END_UNK_MAP
		CMainMenu(IQuadooConstructionContext* context, IQuadooComponentCollection* owner, IUnknown* factory, UINT* live) : CMenuRoot(context,owner,factory,live)
		{
		}
		static const TypeSpec* Type()
		{
			return &mainType;
		}
		HRESULT Construct(IJSONObject* definition, IQuadooPropertyDescriptors* properties, IQuadooEventDescriptors* events)
		{
			HRESULT hr;
			TStackRef<IQuadooControlContainer> root;
			TStackRef<IQuadooForm> form;
			Check(m_owner->GetRoot(&root));
			Check(root->QueryInterface(__uuidof(IQuadooForm),reinterpret_cast<PVOID*>(&form)));
			Check(CComponentObject::Construct(definition,properties,events));
		Cleanup:
			return hr;
		}
		STDMETHOD(IsPopup)(BOOL* popup)
		{
			if(popup)
			{
				*popup = FALSE;
			}
			return popup ? CheckAlive() : E_POINTER;
		}
		STDMETHOD(GetUniquenessKey)(GUID* key)
		{
			if(key)
			{
				*key = QuadooForm::MainMenuUniquenessKey;
			}
			return key ? CheckThread() : E_POINTER;
		}
	};

	class CPopupMenu : public CMenuRoot, public IQuadooPopupMenu, public IQuadooMessageHandler
	{
		BOOL m_tracking;
		CNativeMenu* m_trackingMenu;
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooPopupMenu)
			UNK_INTERFACE(IQuadooMessageHandler)
			UNK_CHAIN_TO(CMenuRoot)
		END_UNK_MAP
		CPopupMenu(IQuadooConstructionContext* context, IQuadooComponentCollection* owner, IUnknown* factory, UINT* live) : CMenuRoot(context,owner,factory,live), m_tracking(FALSE), m_trackingMenu(NULL)
		{
		}
		static const TypeSpec* Type()
		{
			return &popupType;
		}
		STDMETHOD(IsPopup)(BOOL* popup)
		{
			if(popup)
			{
				*popup = TRUE;
			}
			return popup ? CheckAlive() : E_POINTER;
		}
		STDMETHOD(HandleMessage)(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result)
		{
			HRESULT hr = S_OK;
			CheckIf(!handled || !result,E_POINTER);
			*handled = FALSE;
			*result = 0;
			Check(CheckThread());
			if(message == WM_INITMENUPOPUP && m_tracking && m_trackingMenu && m_trackingMenu->Owns(reinterpret_cast<HMENU>(wp)))
			{
				*handled = TRUE;
				Check(m_trackingMenu->Opening(reinterpret_cast<HMENU>(wp)));
			}
		Cleanup:
			return hr;
		}
		STDMETHOD(Popup)(HWND owner, INT x, INT y)
		{
			HRESULT hr;
			TStackRef<CPopupMenu> keepAlive(this);
			TStackRef<IQuadooControlContainer> root;
			TStackRef<CNativeMenu> menu;
			BOOL entered = FALSE;
			Check(CheckAlive());
			CheckIf(m_mode == QuadooForm::Design || m_tracking || m_state != QuadooForm::Ready,QuadooForm::EInvalidState);
			Check(m_owner->GetRoot(&root));
			if(!owner)
			{
				Check(root->GetHandle(&owner));
			}
			CheckIf(!IsWindow(owner) || GetWindowThreadProcessId(owner,NULL) != m_thread,E_INVALIDARG);
			menu.Attach(__new CNativeMenu);
			CheckAlloc(menu);
			Check(menu->Build(this,TRUE));
			m_tracking = TRUE;
			m_trackingMenu = menu;
			entered = TRUE;
			SetLastError(0);
			UINT command = TrackPopupMenuEx(menu->Handle(),TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,x,y,owner,NULL);
			if(command)
			{
				Check(menu->Invoke(command));
			}
			else
			{
				hr = GetLastError() ? HrEnsureLastError() : S_FALSE;
			}
		Cleanup:
			if(entered)
			{
				m_tracking = FALSE;
				m_trackingMenu = NULL;
			}
			return hr;
		}
	};

	class CMenuItem : public CNonvisual, public IQuadooMenuItem, public IQuadooMenuItems
	{
		GUID m_parentID;
		BOOL m_clicking;
	protected:
		virtual HRESULT ValidateProperty(PCWSTR name, IJSONValue* value)
		{
			HRESULT hr = S_OK;
			if(!TStrCmpAssert(name,L"Order"))
			{
				INT order;
				Check(value->GetInteger(&order));
				CheckIf(order < 0,E_INVALIDARG);
			}
			if(!TStrCmpAssert(name,L"Separator"))
			{
				bool separator;
				Check(value->GetBoolean(&separator));
				if(separator && m_state == QuadooForm::Ready)
				{
					UINT count;
					Check(GetItemCount(&count));
					CheckIf(count,E_INVALIDARG);
				}
			}
			if(value && value->GetType() == JSON::String)
			{
				CRString text;
				Check(value->GetString(&text));
				CheckIf(text.Length() > 1024,E_INVALIDARG);
				for(INT n = 0; n < text.Length(); n++)
				{
					CheckIf(text.c_wstr()[n] < L' ',E_INVALIDARG);
				}
				if(!TStrCmpAssert(name,L"ShortCut"))
				{
					ACCEL key;
					Check(ParseMenuShortcut(text,&key));
				}
			}
		Cleanup:
			return hr;
		}
		virtual HRESULT Activated()
		{
			return RefreshOwnerMenu(m_owner);
		}
		virtual HRESULT ChangedProperty(PCWSTR name)
		{
			return RefreshOwnerMenu(m_owner);
		}
		virtual HRESULT DestroyObject()
		{
			HRESULT hr;
			Check(DestroyMenuChildren(this));
			Check(RefreshOwnerMenu(m_owner));
		Cleanup:
			return hr;
		}
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooMenuItem)
			UNK_INTERFACE(IQuadooMenuItems)
			UNK_CHAIN_TO(CNonvisual)
		END_UNK_MAP
		CMenuItem(IQuadooConstructionContext* context, IQuadooComponentCollection* owner, IUnknown* factory, UINT* live) : CNonvisual(context,owner,factory,live), m_parentID(GUID_NULL), m_clicking(FALSE)
		{
		}
		static const TypeSpec* Type()
		{
			return &itemType;
		}
		HRESULT Construct(IJSONObject* definition, IQuadooPropertyDescriptors* properties, IQuadooEventDescriptors* events)
		{
			HRESULT hr;
			TStackRef<IJSONValue> parent;
			Check(definition->FindNonNullValueW(L"menuParent",&parent));
			Check(parent->GetStringAsGuid(m_parentID));
			CheckIf(IsEqualGUID(m_parentID,GUID_NULL),E_INVALIDARG);
			Check(CComponentObject::Construct(definition,properties,events));
		Cleanup:
			return hr;
		}
		STDMETHOD(GetMenuParentID)(GUID* id)
		{
			if(id)
			{
				*id = m_parentID;
			}
			return id ? CheckThread() : E_POINTER;
		}
		STDMETHOD(GetItemCount)(UINT* count)
		{
			return ItemCount(this,count);
		}
		STDMETHOD(GetItem)(UINT index, IQuadooComponent** item)
		{
			return ItemAt(this,index,item);
		}
		STDMETHOD(GetEnabled)(BOOL* enabled)
		{
			return GetValueBool(Find(L"Enabled"),enabled);
		}
		STDMETHOD(SetEnabled)(BOOL enabled)
		{
			return SetValueBool(Find(L"Enabled"),enabled);
		}
		STDMETHOD(GetChecked)(BOOL* checked)
		{
			return GetValueBool(Find(L"Checked"),checked);
		}
		STDMETHOD(SetChecked)(BOOL checked)
		{
			return SetValueBool(Find(L"Checked"),checked);
		}
		STDMETHOD(Toggle)(VOID)
		{
			return SetChecked(!Boolean(L"Checked"));
		}
		STDMETHOD(Click)(VOID)
		{
			HRESULT hr = S_FALSE;
			TStackRef<CMenuItem> keepAlive(this);
			TStackRef<IQuadooComponent> current(this);
			BOOL entered = FALSE;
			Check(CheckAlive());
			CheckIf(m_mode == QuadooForm::Design || m_state != QuadooForm::Ready || m_clicking,QuadooForm::EInvalidState);
			CheckIfIgnore(Boolean(L"Separator"),S_FALSE);
			for(UINT depth = 0; depth < 128; depth++)
			{
				TStackRef<IQuadooMenuItem> item;
				Check(QueryOptional(current,&item));
				if(!item)
				{
					break;
				}
				BOOL enabled;
				BOOL visible;
				GUID parent;
				Check(item->GetEnabled(&enabled));
				Check(MenuBool(current,L"Visible",&visible));
				CheckIfIgnore(!enabled || !visible,S_FALSE);
				Check(item->GetMenuParentID(&parent));
				current.Release();
				Check(FindComponentID(m_owner,parent,&current));
				CheckIf(depth == 127,QuadooForm::EInvalidDefinition);
			}
			m_clicking = TRUE;
			entered = TRUE;
			if(Boolean(L"AutoCheck"))
			{
				Check(Toggle());
			}
			Check(Dispatch(L"OnClick"));
		Cleanup:
			if(entered)
			{
				m_clicking = FALSE;
			}
			return hr;
		}
	};

	static HRESULT CheckUniqueTree(IQuadooControl* control, IQuadooComponent* candidate, REFGUID key, UINT depth)
	{
		HRESULT hr;
		TStackRef<IQuadooUniqueComponent> unique;
		TStackRef<IQuadooControlContainer> container;
		TStackRef<IQuadooFrame> frame;

		CheckIf(depth >= 128,QuadooForm::EInvalidDefinition);
		CheckIf(depth && SUCCEEDED(control->QueryInterface(__uuidof(IQuadooFrame),reinterpret_cast<PVOID*>(&frame))), S_OK);

		Check(QueryOptional(control,&unique));
		if(unique && !SameObject(control,candidate))
		{
			GUID other;
			Check(unique->GetUniquenessKey(&other));
			CheckIf(IsEqualGUID(key,other),HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS));
		}
		Check(QueryOptional(control,&container));
		if(container)
		{
			UINT count;
			Check(container->GetChildCount(&count));
			for(UINT n = 0; n < count; n++)
			{
				TStackRef<IQuadooControl> child;
				Check(container->GetChild(n,&child));
				Check(CheckUniqueTree(child,candidate,key,depth + 1));
			}
		}
	Cleanup:
		return hr;
	}

	static HRESULT ValidateUniqueCandidate(IQuadooControlContainer* root, IQuadooComponent* candidate,
		IQuadooControl* candidateTree, UINT depth)
	{
		HRESULT hr;
		TStackRef<IQuadooUniqueComponent> unique;
		TStackRef<IQuadooComponentHost> host;
		TStackRef<IQuadooComponentCollection> components;
		TStackRef<IQuadooControlContainer> container;
		TStackRef<IQuadooFrame> frame;
		GUID key;
		CheckIf(depth >= 128,QuadooForm::EInvalidDefinition);
		Check(QueryOptional(candidate,&frame));
		// A prepopulated frame's children were registered in its own namespace.
		CheckIfIgnore(frame && !SameObject(root,candidate),S_OK);
		Check(QueryOptional(candidate,&unique));
		{
			TStackRef<IQuadooMenu> menu;
			Check(QueryOptional(candidate,&menu));
			if(menu)
			{
				BOOL popup;
				Check(menu->IsPopup(&popup));
				if(!popup)
				{
					TStackRef<IQuadooForm> form;
					Check(root->QueryInterface(__uuidof(IQuadooForm),reinterpret_cast<PVOID*>(&form)));
					CheckIf(!unique,QuadooForm::EInvalidDefinition);
					Check(unique->GetUniquenessKey(&key));
					CheckIf(!IsEqualGUID(key,QuadooForm::MainMenuUniquenessKey),QuadooForm::EInvalidDefinition);
				}
			}
		}
		if(!unique)
		{
			goto Children;
		}
		Check(unique->GetUniquenessKey(&key));
		CheckIf(IsEqualGUID(key,GUID_NULL),E_INVALIDARG);
		Check(CheckUniqueTree(root,candidate,key,0));
		if(candidateTree)
		{
			Check(CheckUniqueTree(candidateTree,candidate,key,0));
		}
		Check(QueryOptional(root,&host));
		if(host)
		{
			UINT count;
			Check(host->GetComponents(&components));
			Check(components->GetCount(&count));
			for(UINT n = 0; n < count; n++)
			{
				TStackRef<IQuadooComponent> item;
				TStackRef<IQuadooUniqueComponent> other;
				Check(components->GetAt(n,&item));
				Check(QueryOptional(item,&other));
				if(other && !SameObject(item,candidate))
				{
					GUID otherKey;
					Check(other->GetUniquenessKey(&otherKey));
					CheckIf(IsEqualGUID(key,otherKey),HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS));
				}
			}
		}
	Children:
		Check(QueryOptional(candidate,&container));
		if(container)
		{
			UINT count;
			Check(container->GetChildCount(&count));
			for(UINT n = 0; n < count; n++)
			{
				TStackRef<IQuadooControl> child;
				Check(container->GetChild(n,&child));
				Check(ValidateUniqueCandidate(root,child,candidateTree,depth + 1));
			}
		}
	Cleanup:
		return hr;
	}
	HRESULT ValidateUniqueComponent(IQuadooControlContainer* root, IQuadooComponent* candidate)
	{
		HRESULT hr;
		TStackRef<IQuadooControl> visual;
		Check(QueryOptional(candidate,&visual));
		Check(ValidateUniqueCandidate(root,candidate,visual,0));
	Cleanup:
		return hr;
	}
	static HRESULT ValidateMenuReferences(IQuadooControl* control, UINT depth);
	HRESULT ValidateMenuGraph(IQuadooControl* root)
	{
		HRESULT hr;
		TStackRef<IQuadooComponentHost> host;
		TStackRef<IQuadooComponentCollection> components;
		UINT count;
		Check(QueryOptional(root,&host));
		CheckIfIgnore(!host,S_OK);
		Check(host->GetComponents(&components));
		Check(ValidateMenuReferences(root,0));
		Check(components->GetCount(&count));
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IQuadooComponent> current;
			TStackRef<IQuadooMenuItem> item;
			Check(components->GetAt(n,&current));
			Check(QueryOptional(current,&item));
			if(item)
			{
				GUID id;
				Check(current->GetID(&id));
				for(UINT depth = 0; depth < 128; depth++)
				{
					GUID parent;
					Check(item->GetMenuParentID(&parent));
					CheckIf(IsEqualGUID(id,parent),QuadooForm::EInvalidDefinition);
					current.Release();
					Check(FindComponentID(components,parent,&current));
					TStackRef<IQuadooMenu> menu;
					Check(QueryOptional(current,&menu));
					if(menu)
					{
						break;
					}
					BOOL separator;
					Check(MenuBool(current,L"Separator",&separator));
					CheckIf(separator,QuadooForm::EInvalidDefinition);
					item.Release();
					Check(current->QueryInterface(__uuidof(IQuadooMenuItem),reinterpret_cast<PVOID*>(&item)));
					CheckIf(depth == 127,QuadooForm::EInvalidDefinition);
				}
			}
		}
	Cleanup:
		return hr;
	}

	HRESULT ValidateMenuReference(IQuadooControl* control, PCWSTR name, IJSONValue* value, BOOL resolve)
	{
		HRESULT hr;
		TStackRef<IQuadooControlContainer> root;
		TStackRef<IQuadooComponentHost> host;
		TStackRef<IQuadooComponentCollection> components;
		TStackRef<IQuadooComponent> component;
		TStackRef<IQuadooMenu> menu;
		GUID id;
		BOOL popup;

		CheckIfIgnore(TStrCmpAssert(name,L"MainMenu") && TStrCmpAssert(name,L"PopupMenu"),S_OK);
		CheckIfIgnore(!value,S_OK);
		Check(value->GetStringAsGuid(id));
		CheckIf(IsEqualGUID(id,GUID_NULL),E_INVALIDARG);
		CheckIfIgnore(!resolve,S_OK);
		Check(GetNamespace(control,&root));
		Check(root->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&host)));
		Check(host->GetComponents(&components));
		Check(FindComponentID(components,id,&component));
		Check(component->QueryInterface(__uuidof(IQuadooMenu),reinterpret_cast<PVOID*>(&menu)));
		Check(menu->IsPopup(&popup));
		CheckIf(popup != (TStrCmpAssert(name,L"PopupMenu") == 0),E_INVALIDARG);

	Cleanup:
		return hr;
	}
	static HRESULT ValidateMenuReferences(IQuadooControl* control, UINT depth)
	{
		HRESULT hr;
		TStackRef<IJSONValue> value;
		TStackRef<IQuadooControlContainer> container;
		TStackRef<IQuadooFrame> frame;
		CheckIf(depth >= 128,QuadooForm::EInvalidDefinition);
		CheckIfIgnore(depth && SUCCEEDED(control->QueryInterface(__uuidof(IQuadooFrame),reinterpret_cast<PVOID*>(&frame))),S_OK);
		static PCWSTR const names[] = {L"MainMenu",L"PopupMenu"};
		for(UINT n = 0; n < ARRAYSIZE(names); n++)
		{
			hr = MenuProperty(control,names[n],&value);
			if(hr == DISP_E_MEMBERNOTFOUND)
			{
				continue;
			}
			Check(hr);
			Check(ValidateMenuReference(control,names[n],value,TRUE));
			value.Release();
		}
		Check(QueryOptional(control,&container));
		if(container)
		{
			UINT count;
			Check(container->GetChildCount(&count));
			for(UINT n = 0; n < count; n++)
			{
				TStackRef<IQuadooControl> child;
				Check(container->GetChild(n,&child));
				Check(ValidateMenuReferences(child,depth + 1));
			}
		}
	Cleanup:
		return hr;
	}
	HRESULT RegisterMenuFactories(IQuadooComponentRegistry* registry)
	{
		HRESULT hr;
		TStackRef<TComponentFactory<CMainMenu> > mainMenu;
		TStackRef<TComponentFactory<CPopupMenu> > popupMenu;
		TStackRef<TComponentFactory<CMenuItem> > item;
		mainMenu.Attach(__new TComponentFactory<CMainMenu>);
		popupMenu.Attach(__new TComponentFactory<CPopupMenu>);
		item.Attach(__new TComponentFactory<CMenuItem>);
		CheckAlloc(mainMenu);
		CheckAlloc(popupMenu);
		CheckAlloc(item);
		Check(mainMenu->Initialize());
		Check(popupMenu->Initialize());
		Check(item->Initialize());
		Check(registry->RegisterComponentFactory(mainMenu));
		Check(registry->RegisterComponentFactory(popupMenu));
		Check(registry->RegisterComponentFactory(item));
	Cleanup:
		return hr;
	}
}
