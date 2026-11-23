#include "Designer.h"

namespace QFD
{
static HRESULT MenuIDText(REFGUID id, RSTRING* text)
{
	HRESULT hr;
	WCHAR buffer[40];
	INT count = StringFromGUID2(id,buffer,ARRAYSIZE(buffer));
	CheckIf(!count,E_FAIL);
	Check(RStrCreateW(count - 1,buffer,text));
Cleanup:
	return hr;
}
static HRESULT ComponentsJSON(IJSONObject* root, IJSONArray** components)
{
	HRESULT hr;
	TStackRef<IJSONValue> value;
	Check(root->FindNonNullValueW(L"components",&value));
	Check(value->GetArray(components));
Cleanup:
	return hr;
}
static HRESULT MenuDefinition(IJSONArray* components, REFGUID id, IJSONObject** definition)
{
	HRESULT hr;
	CheckIf(!definition,E_POINTER);
	*definition = NULL;
	for(sysint n = 0; n < components->Count(); n++)
	{
		TStackRef<IJSONObject> item;
		TStackRef<IJSONValue> value;
		GUID current;
		Check(components->GetObject(n,&item));
		Check(item->FindNonNullValueW(L"id",&value));
		Check(value->GetStringAsGuid(current));
		if(IsEqualGUID(id,current))
		{
			*definition = item.Detach();
			goto Cleanup;
		}
	}
	hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
Cleanup:
	return hr;
}
HRESULT CDesignerDocument::SetMenuAssignment(IQuadooComponent* menu)
{
	HRESULT hr;
	TStackRef<IQuadooFormMenu> form;
	CheckIf(!m_form,E_INVALIDARG);
	Check(m_root->QueryInterface(__uuidof(IQuadooFormMenu),reinterpret_cast<PVOID*>(&form)));
	Check(form->SetMainMenu(menu));
	m_dirty = TRUE;
Cleanup:
	return hr;
}
HRESULT CDesignerDocument::ApplyMenuDefinition(IJSONObject* definition)
{
	return ApplyDefinition(definition);
}
HRESULT CDesignerDocument::AddMenuItem(IQuadooComponent* parent, IQuadooComponent** item)
{
	HRESULT hr;
	TStackRef<IQuadooComponent> found;
	TStackRef<IQuadooMenuItems> children;
	TStackRef<IQuadooComponentHost> host;
	TStackRef<IQuadooComponentCollection> components;
	TStackRef<IJSONObject> definition;
	TStackRef<IJSONObject> properties;
	TStackRef<IJSONValue> wrapped;
	TStackRef<ISourceEdit> sourceEdit;
	CRString parentID;
	CRString name;
	GUID id;
	UINT count;
	CheckIf(!item,E_POINTER);
	*item = NULL;
	CheckIf(!parent,E_INVALIDARG);
	Check(parent->GetID(&id));
	Check(FindComponent(id,&found));
	CheckIf(!SameObject(found,parent),E_INVALIDARG);
	Check(parent->QueryInterface(__uuidof(IQuadooMenuItems),reinterpret_cast<PVOID*>(&children)));
	Check(children->GetItemCount(&count));
	Check(MenuIDText(id,&parentID));
	Check(m_root->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&host)));
	Check(host->GetComponents(&components));
	for(UINT n = 1; n < 100000; n++)
	{
		TStackRef<IQuadooComponent> existing;
		name = NULL;
		Check(RStrFormatW(&name,L"MenuItem%u",n));
		hr = components->FindComponent(name,&existing);
		if(hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND))
		{
			break;
		}
		Check(hr);
		CheckIf(n == 99999,E_FAIL);
	}
	Check(JSONCreateObject(&definition));
	Check(JSONCreateObject(&properties));
	Check(PutString(definition,L"type",RSTRING_CAST(L"MenuItem")));
	Check(PutString(definition,L"name",name));
	Check(PutString(definition,L"menuParent",parentID));
	Check(PutString(properties,L"Caption",name));
	INT lastOrder = -1;
	for(UINT n = 0; n < count; n++)
	{
		TStackRef<IQuadooComponent> child;
		Check(children->GetItem(n,&child));
		lastOrder = max(lastOrder,IntProperty(child,L"Order"));
	}
	CheckIf(lastOrder == INT_MAX,E_INVALIDARG);
	Check(PutInt(properties,L"Order",lastOrder + 1));
	Check(JSONWrapObject(properties,&wrapped));
	Check(definition->AddValueW(L"properties",wrapped));
	Check(PrepareInsert(m_root,definition,TRUE,&sourceEdit));
	Check(QuadooCreateComponent(m_context,components,definition,item));
	m_dirty = TRUE;
Cleanup:
	if(FAILED(hr) && item && *item)
	{
		(*item)->Destroy();
		SafeRelease(*item);
	}
	return FinishSourceEdit(sourceEdit,hr);
}
static BOOL ContainsID(const TArray<GUID>& ids, REFGUID id)
{
	for(sysint n = 0; n < ids.Length(); n++)
	{
		if(IsEqualGUID(ids[n],id))
		{
			return TRUE;
		}
	}
	return FALSE;
}
HRESULT CDesignerDocument::CopySelectionDefinition(REFGUID id, IJSONValue** definition)
{
	HRESULT hr;
	TStackRef<IQuadooComponent> component;
	TStackRef<IQuadooMenuItems> menu;
	TStackRef<IJSONObject> object;
	TStackRef<IJSONArray> components;
	TStackRef<IJSONArray> copy;
	TArray<GUID> copied;
	CheckIf(!definition,E_POINTER);
	*definition = NULL;
	Check(FindComponent(id,&component));
	hr = component->QueryInterface(__uuidof(IQuadooMenuItems),reinterpret_cast<PVOID*>(&menu));
	if(hr == E_NOINTERFACE)
	{
		Check(CopyDefinition(id,&object));
		Check(JSONWrapObject(object,definition));
		goto Cleanup;
	}
	Check(hr);
	Check(copied.Append(id));
	Check(Serialize(&object));
	Check(ComponentsJSON(object,&components));
	Check(JSONCreateArray(&copy));
	for(sysint n = 0; n < copied.Length(); n++)
	{
		TStackRef<IJSONObject> item;
		TStackRef<IJSONValue> value;
		Check(MenuDefinition(components,copied[n],&item));
		Check(JSONWrapObject(item,&value));
		Check(copy->Add(value));
		for(sysint c = 0; c < components->Count(); c++)
		{
			TStackRef<IJSONObject> child;
			TStackRef<IJSONValue> parent;
			GUID parentID;
			GUID childID;
			Check(components->GetObject(c,&child));
			if(child->HasField(RSTRING_CAST(L"menuParent")))
			{
				Check(child->FindNonNullValueW(L"menuParent",&parent));
				Check(parent->GetStringAsGuid(parentID));
				parent.Release();
				if(IsEqualGUID(parentID,copied[n]))
				{
					Check(child->FindNonNullValueW(L"id",&parent));
					Check(parent->GetStringAsGuid(childID));
					if(!ContainsID(copied,childID))
					{
						Check(copied.Append(childID));
					}
				}
			}
		}
	}
	Check(JSONWrapArray(copy,definition));
Cleanup:
	return hr;
}
static HRESULT ClearMenuReferences(IJSONObject* object, const TArray<GUID>& removed, UINT depth = 0)
{
	HRESULT hr = S_OK;
	TStackRef<IJSONValue> value;
	TStackRef<IJSONObject> properties;
	CheckIf(depth >= 128,E_INVALIDARG);
	if(object->HasField(RSTRING_CAST(L"properties")))
	{
		Check(object->FindNonNullValueW(L"properties",&value));
		Check(value->GetObject(&properties));
		value.Release();
		static PCWSTR const fields[] = {L"MainMenu",L"PopupMenu"};
		for(UINT n = 0; n < ARRAYSIZE(fields); n++)
		{
			if(properties->HasField(RSTRING_CAST(fields[n])))
			{
				GUID id;
				Check(properties->FindValueW(fields[n],&value));
				if(value && SUCCEEDED(value->GetStringAsGuid(id)) && ContainsID(removed,id))
				{
					Check(properties->AddValueW(fields[n],NULL));
				}
				value.Release();
			}
		}
	}
	static PCWSTR const arrays[] = {L"children",L"components"};
	for(UINT n = 0; n < ARRAYSIZE(arrays); n++)
	{
		if(object->HasField(RSTRING_CAST(arrays[n])))
		{
			TStackRef<IJSONArray> children;
			Check(object->FindNonNullValueW(arrays[n],&value));
			Check(value->GetArray(&children));
			value.Release();
			for(sysint k = 0; k < children->Count(); k++)
			{
				TStackRef<IJSONObject> child;
				Check(children->GetObject(k,&child));
				Check(ClearMenuReferences(child,removed,depth + 1));
			}
		}
	}
Cleanup:
	return hr;
}
HRESULT CDesignerDocument::RemoveMenu(IQuadooComponent* component)
{
	HRESULT hr;
	TStackRef<IJSONObject> root;
	TStackRef<IJSONArray> components;
	TArray<GUID> removed;
	GUID id;
	Check(component->GetID(&id));
	Check(removed.Append(id));
	Check(Serialize(&root));
	Check(ComponentsJSON(root,&components));
	for(sysint parent = 0; parent < removed.Length(); parent++)
	{
		for(sysint n = 0; n < components->Count(); n++)
		{
			TStackRef<IJSONObject> item;
			TStackRef<IJSONValue> value;
			GUID parentID;
			Check(components->GetObject(n,&item));
			if(item->HasField(RSTRING_CAST(L"menuParent")))
			{
				Check(item->FindNonNullValueW(L"menuParent",&value));
				Check(value->GetStringAsGuid(parentID));
				value.Release();
				if(IsEqualGUID(parentID,removed[parent]))
				{
					Check(item->FindNonNullValueW(L"id",&value));
					Check(value->GetStringAsGuid(id));
					if(!ContainsID(removed,id))
					{
						Check(removed.Append(id));
					}
				}
			}
		}
	}
	for(sysint n = components->Count(); n > 0; n--)
	{
		TStackRef<IJSONObject> item;
		TStackRef<IJSONValue> value;
		Check(components->GetObject(n - 1,&item));
		Check(item->FindNonNullValueW(L"id",&value));
		Check(value->GetStringAsGuid(id));
		if(ContainsID(removed,id))
		{
			Check(components->Remove(n - 1,NULL));
		}
	}
	Check(ClearMenuReferences(root,removed));
	Check(ApplyDefinition(root));
Cleanup:
	return hr;
}
HRESULT CDesignerDocument::MoveMenuItem(IQuadooComponent* component, BOOL down, BOOL end)
{
	HRESULT hr;
	TStackRef<IQuadooMenuItem> item;
	TStackRef<IQuadooComponent> parent;
	TStackRef<IQuadooMenuItems> children;
	TStackRef<IJSONObject> root;
	TStackRef<IJSONArray> definitions;
	GUID parentID;
	UINT count;
	UINT selected = UINT_MAX;
	TArray<GUID> order;
	GUID moved;
	Check(component->QueryInterface(__uuidof(IQuadooMenuItem),reinterpret_cast<PVOID*>(&item)));
	Check(item->GetMenuParentID(&parentID));
	Check(FindComponent(parentID,&parent));
	Check(parent->QueryInterface(__uuidof(IQuadooMenuItems),reinterpret_cast<PVOID*>(&children)));
	Check(children->GetItemCount(&count));
	for(UINT n = 0; n < count; n++)
	{
		TStackRef<IQuadooComponent> child;
		GUID id;
		Check(children->GetItem(n,&child));
		Check(child->GetID(&id));
		Check(order.Append(id));
		if(SameObject(child,component))
		{
			selected = n;
		}
	}
	CheckIf(selected == UINT_MAX,E_INVALIDARG);
	CheckIfIgnore(down ? selected + 1 >= count : !selected,S_FALSE);
	UINT other = end ? (down ? count - 1 : 0) : down ? selected + 1 : selected - 1;
	moved = order[selected];
	for(UINT n = selected; n != other; n = down ? n + 1 : n - 1)
	{
		order[n] = order[down ? n + 1 : n - 1];
	}
	order[other] = moved;
	Check(Serialize(&root));
	Check(ComponentsJSON(root,&definitions));
	for(UINT n = 0; n < count; n++)
	{
		TStackRef<IJSONObject> definition;
		TStackRef<IJSONObject> properties;
		TStackRef<IJSONValue> value;
		Check(MenuDefinition(definitions,order[n],&definition));
		Check(definition->FindNonNullValueW(L"properties",&value));
		Check(value->GetObject(&properties));
		Check(PutInt(properties,L"Order",n));
	}
	Check(ApplyDefinition(root));
Cleanup:
	return hr;
}
}
