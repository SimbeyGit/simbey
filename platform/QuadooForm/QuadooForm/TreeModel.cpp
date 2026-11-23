#include "TreeView.h"

namespace QF
{
	VOID CTreeView::ReleaseNodes(TArray<CTreeNode*>& all)
	{
		for(sysint n = 0; n < all.Length(); n++)
		{
			all[n]->attached = FALSE;
			all[n]->parent = NULL;
			all[n]->children.Clear();
			all[n]->Release();
		}
		all.Clear();
	}
	HRESULT CTreeView::ParseNodes(IJSONArray* array, CTreeNode* parent, TArray<CTreeNode*>& all,
		TArray<CTreeNode*>& siblings, UINT depth)
	{
		HRESULT hr = S_OK;
		CheckIf(depth > 128,QuadooForm::EInvalidDefinition);
		for(sysint n = 0; n < array->Count(); n++)
		{
			TStackRef<IJSONValue> value;
			TStackRef<IJSONValue> children;
			TStackRef<IJSONObject> object;
			TStackRef<IJSONArray> items;
			TStackRef<CTreeNode> node;
			CheckIf(all.Length() >= 4096,QuadooForm::EInvalidDefinition);
			Check(array->GetValue(n,&value));
			CheckIf(!value,QuadooForm::EInvalidDefinition);
			Check(value->GetObject(&object));
			node.Attach(__new CTreeNode(m_link));
			CheckAlloc(node);
			if(object->HasField(RSTRING_CAST(L"id")))
			{
				value.Release();
				Check(object->FindNonNullValueW(L"id",&value));
				Check(value->GetStringAsGuid(node->id));
			}
			else
			{
				Check(CoCreateGuid(&node->id));
			}
			CheckIf(IsEqualGUID(node->id,GUID_NULL),QuadooForm::EInvalidDefinition);
			for(sysint i = 0; i < all.Length(); i++)
			{
				CheckIf(IsEqualGUID(all[i]->id,node->id),QuadooForm::EDuplicateID);
			}
			Check(ReadString(object,L"Text",L"",&node->text));
			CheckIf(RStrLen(node->text) > 32767,E_INVALIDARG);
			for(INT c = 0; c < RStrLen(node->text); c++)
			{
				CheckIf(!node->text.c_wstr()[c],E_INVALIDARG);
			}
			Check(ReadInt(object,L"ImageIndex",-1,&node->image));
			Check(ReadInt(object,L"SelectedImageIndex",-1,&node->selectedImage));
			CheckIf(node->image < -1 || node->selectedImage < -1,E_INVALIDARG);
			static PCWSTR const booleanNames[] = {L"Expanded",L"Checked"};
			for(UINT i = 0; i < ARRAYSIZE(booleanNames); i++)
			{
				bool flag = false;
				if(object->HasField(RSTRING_CAST(booleanNames[i])))
				{
					value.Release();
					Check(object->FindNonNullValueW(booleanNames[i],&value));
					Check(value->GetBoolean(&flag));
				}
				if(i == 0)
				{
					node->expanded = flag;
				}
				else
				{
					node->checked = flag;
				}
			}
			node->parent = parent;
			Check(all.Append(node));
			node->AddRef();
			Check(siblings.Append(node));
			if(object->HasField(RSTRING_CAST(L"Items")))
			{
				Check(object->FindNonNullValueW(L"Items",&children));
				Check(children->GetArray(&items));
				Check(ParseNodes(items,node,all,node->children,depth + 1));
			}
		}
	Cleanup:
		return hr;
	}
	HRESULT CTreeView::LoadNodes(IJSONValue* value)
	{
		HRESULT hr;
		TStackRef<CTreeView> keepAlive(this);
		TStackRef<IJSONArray> array;
		TArray<CTreeNode*> candidate;
		TArray<CTreeNode*> candidateRoots;
		CTreeNode* previous = m_selected;
		CheckIf(!value,E_INVALIDARG);
		Check(value->GetArray(&array));
		Check(ParseNodes(array,NULL,candidate,candidateRoots,1));
		nodes.Swap(candidate);
		roots.Swap(candidateRoots);
		m_selected = NULL;
		hr = m_hwnd ? Rebuild() : S_OK;
		if(FAILED(hr))
		{
			nodes.Swap(candidate);
			roots.Swap(candidateRoots);
			m_selected = previous;
		}
	Cleanup:
		ReleaseNodes(candidate);
		return hr;
	}
	static HRESULT TreeBoolean(IJSONObject* object, PCWSTR name, BOOL flag)
	{
		HRESULT hr;
		TStackRef<IJSONValue> value;
		Check(JSONCreateBool(flag != FALSE,&value));
		Check(object->AddValueW(name,value));
	Cleanup:
		return hr;
	}
	HRESULT CTreeView::WriteNodes(const TArray<CTreeNode*>& siblings, IJSONValue** value)
	{
		HRESULT hr;
		TStackRef<IJSONArray> array;
		Check(JSONCreateArray(&array));
		for(sysint n = 0; n < siblings.Length(); n++)
		{
			TStackRef<IJSONObject> object;
			TStackRef<IJSONValue> child;
			TStackRef<IJSONValue> wrapped;
			CRString id;
			CTreeNode* node = siblings[n];
			Check(JSONCreateObject(&object));
			Check(GuidString(node->id,&id));
			Check(PutString(object,L"id",id));
			Check(PutString(object,L"Text",node->text));
			Check(PutInt(object,L"ImageIndex",node->image));
			Check(PutInt(object,L"SelectedImageIndex",node->selectedImage));
			Check(TreeBoolean(object,L"Expanded",node->expanded));
			Check(TreeBoolean(object,L"Checked",node->checked));
			Check(WriteNodes(node->children,&child));
			Check(object->AddValueW(L"Items",child));
			Check(JSONWrapObject(object,&wrapped));
			Check(array->Add(wrapped));
		}
		Check(JSONWrapArray(array,value));
	Cleanup:
		return hr;
	}
	HRESULT CTreeView::Resolve(IQuadooTreeNode* node, CTreeNode** result)
	{
		HRESULT hr = S_OK;
		*result = NULL;
		if(node)
		{
			for(sysint n = 0; n < nodes.Length(); n++)
			{
				if(SameObject(node,static_cast<IQuadooTreeNode*>(nodes[n])))
				{
					*result = nodes[n];
					goto Cleanup;
				}
			}
			hr = E_INVALIDARG;
		}
	Cleanup:
		return hr;
	}
	HRESULT CTreeView::Mutable()
	{
		HRESULT hr;
		Check(CheckAlive());
		CheckIf(m_eventDepth || m_silent,QuadooForm::EInvalidState);
	Cleanup:
		return hr;
	}
	HRESULT CTreeView::Changed()
	{
		HRESULT hr;
		TStackRef<CTreeView> keepAlive(this);
		m_eventDepth++;
		Check(Dirty(L"Items"));
		Check(Dirty(L"SelectedNode"));
		Check(FlushNotifications());
		Check(CheckAlive());
	Cleanup:
		m_eventDepth--;
		return hr;
	}
	UINT CTreeView::Depth(CTreeNode* node)
	{
		UINT depth = 0;
		while(node)
		{
			depth++;
			node = node->parent;
		}
		return depth;
	}
	UINT CTreeView::Height(CTreeNode* node)
	{
		UINT height = 1;
		for(sysint n = 0; n < node->children.Length(); n++)
		{
			height = max(height,1 + Height(node->children[n]));
		}
		return height;
	}
	HRESULT CTreeView::InsertNode(IQuadooTreeNode* parent, UINT index, RSTRING text, IQuadooTreeNode** result)
	{
		HRESULT hr;
		TStackRef<CTreeView> keepAlive(this);
		TStackRef<CTreeNode> node;
		CTreeNode* owner;
		CheckIf(!result,E_POINTER);
		*result = NULL;
		Check(Mutable());
		CheckIf(!text || !RStrIsWide(text) || RStrLen(text) > 32767,E_INVALIDARG);
		for(INT c = 0; c < RStrLen(text); c++)
		{
			CheckIf(!RStrToWide(text)[c],E_INVALIDARG);
		}
		Check(Resolve(parent,&owner));
		CheckIf(nodes.Length() >= 4096 || Depth(owner) >= 128,E_INVALIDARG);
		{
			TArray<CTreeNode*>& siblings = owner ? owner->children : roots;
			CheckIf(index > static_cast<UINT>(siblings.Length()),E_INVALIDARG);
			Check(nodes.Reserve(1));
			Check(siblings.Reserve(1));
			node.Attach(__new CTreeNode(m_link));
			CheckAlloc(node);
			Check(CoCreateGuid(&node->id));
			node->parent = owner;
			node->text = text;
			Check(nodes.Append(node));
			Check(siblings.InsertAt(static_cast<CTreeNode*>(node),index));
			hr = Rebuild();
			if(FAILED(hr))
			{
				siblings.Remove(index,NULL);
				nodes.Remove(nodes.Length() - 1,NULL);
				goto Cleanup;
			}
		}
		// Establish the collection's ownership before notifying observers.
		// Keep the caller's reference local until the operation returns success.
		node->AddRef();
		Check(Changed());
		*result = node.Detach();
	Cleanup:
		return hr;
	}
	VOID CTreeView::RemoveBranch(CTreeNode* node)
	{
		for(sysint n = node->children.Length(); n > 0; n--)
		{
			RemoveBranch(node->children[n - 1]);
		}
		node->children.Clear();
		for(sysint n = 0; n < nodes.Length(); n++)
		{
			if(nodes[n] == node)
			{
				nodes.Remove(n,NULL);
				break;
			}
		}
		node->attached = FALSE;
		node->parent = NULL;
		node->item = NULL;
		node->Release();
	}
	HRESULT CTreeView::DeleteNode(IQuadooTreeNode* value)
	{
		HRESULT hr;
		TStackRef<CTreeView> keepAlive(this);
		CTreeNode* node;
		CTreeNode* previous = m_selected;
		UINT index;
		Check(Mutable());
		Check(Resolve(value,&node));
		CheckIf(!node,E_INVALIDARG);
		Check(node->GetIndex(&index));
		{
			TArray<CTreeNode*>& siblings = node->parent ? node->parent->children : roots;
			CTreeNode* selected = m_selected;
			while(selected && selected != node)
			{
				selected = selected->parent;
			}
			if(selected)
			{
				m_selected = NULL;
			}
			siblings.Remove(index,NULL);
			hr = Rebuild();
			if(FAILED(hr))
			{
				siblings.InsertAt(node,index);
				m_selected = previous;
				goto Cleanup;
			}
		}
		{
			TStackRef<CTreeNode> old(previous);
			RemoveBranch(node);
			Check(Changed());
			if(previous != m_selected)
			{
				Check(Event(L"OnChange",m_selected,old));
			}
		}
	Cleanup:
		return hr;
	}
	HRESULT CTreeView::MoveNode(IQuadooTreeNode* value, IQuadooTreeNode* parent, UINT index)
	{
		HRESULT hr;
		TStackRef<CTreeView> keepAlive(this);
		CTreeNode* node;
		CTreeNode* owner;
		UINT from;
		Check(Mutable());
		Check(Resolve(value,&node));
		Check(Resolve(parent,&owner));
		CheckIf(!node,E_INVALIDARG);
		for(CTreeNode* current = owner; current; current = current->parent)
		{
			CheckIf(current == node,E_INVALIDARG);
		}
		CheckIf(Depth(owner) + Height(node) > 128,E_INVALIDARG);
		Check(node->GetIndex(&from));
		{
			TArray<CTreeNode*>& old = node->parent ? node->parent->children : roots;
			TArray<CTreeNode*>& target = owner ? owner->children : roots;
			CTreeNode* previousParent = node->parent;
			CheckIf(index >= static_cast<UINT>(target.Length()) + (&target != &old ? 1 : 0),E_INVALIDARG);
			Check(target.Reserve(1));
			old.Remove(from,NULL);
			Check(target.InsertAt(node,index));
			node->parent = owner;
			hr = Rebuild();
			if(FAILED(hr))
			{
				target.Remove(index,NULL);
				old.InsertAt(node,from);
				node->parent = previousParent;
				goto Cleanup;
			}
		}
		Check(Changed());
	Cleanup:
		return hr;
	}
	HRESULT CTreeView::ClearNodes()
	{
		HRESULT hr;
		TStackRef<CTreeView> keepAlive(this);
		TStackRef<IJSONArray> array;
		TStackRef<IJSONValue> value;
		TStackRef<CTreeNode> old(m_selected);
		Check(Mutable());
		Check(JSONCreateArray(&array));
		Check(JSONWrapArray(array,&value));
		Check(LoadNodes(value));
		Check(Changed());
		if(old)
		{
			Check(Event(L"OnChange",NULL,old));
		}
	Cleanup:
		return hr;
	}
	HRESULT CTreeView::EditNode(CTreeNode* node, UINT property, RSTRING text, INT number)
	{
		HRESULT hr;
		TStackRef<CTreeView> keepAlive(this);
		CRString previousText(node->text);
		INT previous;
		Check(Mutable());
		Check(node->Alive());
		CheckIf(property > 4,E_INVALIDARG);
		if(property == 0)
		{
			CheckIf(!text || !RStrIsWide(text) || RStrLen(text) > 32767,E_INVALIDARG);
			for(INT c = 0; c < RStrLen(text); c++)
			{
				CheckIf(!RStrToWide(text)[c],E_INVALIDARG);
			}
			CheckIfIgnore(SameString(text,node->text),S_FALSE);
			previous = 0;
		}
		else
		{
			CheckIf(property < 3 && number < -1,E_INVALIDARG);
			previous = property == 1 ? node->image : property == 2 ? node->selectedImage : property == 3 ? node->expanded : node->checked;
			if(property >= 3)
			{
				number = number != FALSE;
			}
			CheckIfIgnore(previous == number,S_FALSE);
		}
		if(property == 3)
		{
			Check(ExpandNode(node,number));
			goto Cleanup;
		}

		if(property == 0)
		{
			node->text = text;
		}
		else if(property == 1)
		{
			node->image = number;
		}
		else if(property == 2)
		{
			node->selectedImage = number;
		}
		else
		{
			node->checked = number;
		}
		hr = Rebuild();
		if(FAILED(hr))
		{
			if(property == 0)
			{
				node->text = previousText;
			}
			else if(property == 1)
			{
				node->image = previous;
			}
			else if(property == 2)
			{
				node->selectedImage = previous;
			}
			else
			{
				node->checked = previous;
			}
			goto Cleanup;
		}
		Check(Changed());
		if(property == 4)
		{
			Check(Event(L"OnCheck",node));
		}
	Cleanup:
		return hr;
	}
}
