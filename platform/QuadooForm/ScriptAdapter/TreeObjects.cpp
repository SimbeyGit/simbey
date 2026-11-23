#include "TreeObjects.h"

namespace AQVM
{
	interface __declspec(uuid("90BB1EB6-AF0B-46EE-871E-A1F9BF6A04E1")) ITreeNodeObject : IUnknown
	{
		STDMETHOD(GetNode)(IQuadooTreeNode** node) = 0;
		STDMETHOD_(VOID, DetachCache)(VOID) = 0;
	};
	static HRESULT Index(const QuadooVM::QVARIANT* value, UINT* index)
	{
		HRESULT hr;
		LONGLONG number;
		Check(VariantInteger(value,&number));
		CheckIf(number < 0 || number > UINT_MAX,DISP_E_BADINDEX);
		*index = static_cast<UINT>(number);
	Cleanup:
		return hr;
	}
	static QuadooVM::QVARIANT* Argument(QuadooVM::QVPARAMS* parameters, UINT index)
	{
		// QuadooScript pushes arguments in reverse order.
		return parameters->pqvArgs + parameters->cArgs - index - 1;
	}
	class CTreeNodeObject : public CBaseUnknown, public CQuadooObjectImpl, public ITreeNodeObject
	{
		TStackRef<IQuadooTreeNode> m_node;
		TStackRef<IQuadooTreeNodes> m_nodes;
		CInstanceLink* m_cache; // Weak; the cache clears this before releasing entries.
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooObject)
			UNK_INTERFACE(ITreeNodeObject)
		END_UNK_MAP
		CTreeNodeObject(IQuadooTreeNode* node, IQuadooTreeNodes* nodes, CInstanceLink* cache) : m_node(node), m_nodes(nodes), m_cache(cache)
		{
		}
		STDMETHOD_(VOID, DetachCache)(VOID)
		{
			m_cache = NULL;
		}
		STDMETHOD(GetNode)(IQuadooTreeNode** node)
		{
			HRESULT hr = S_OK;
			CheckIf(!node,E_POINTER);
			*node = m_node;
			(*node)->AddRef();
		Cleanup:
			return hr;
		}
		STDMETHOD(GetProperty)(IQuadooVM*, RSTRING name, QuadooVM::QVARIANT* result)
		{
			HRESULT hr;
			PCWSTR text = RStrToWide(name);
			CheckIf(!text || !result,E_INVALIDARG);
			result->eType = QuadooVM::Null;
			if(!lstrcmpW(text,L"Text"))
			{
				Check(m_node->GetText(&result->rstrVal));
				result->eType = QuadooVM::String;
			}
			else if(!lstrcmpW(text,L"ID"))
			{
				GUID id;
				WCHAR buffer[40];
				Check(m_node->GetID(&id));
				INT length = StringFromGUID2(id,buffer,ARRAYSIZE(buffer));
				CheckIf(!length,E_FAIL);
				Check(RStrCreateW(length - 1,buffer,&result->rstrVal));
				result->eType = QuadooVM::String;
			}
			else if(!lstrcmpW(text,L"Parent"))
			{
				TStackRef<IQuadooTreeNode> parent;
				Check(m_node->GetParent(&parent));
				Check(WrapTreeNode(parent,m_nodes,result,m_cache));
			}
			else if(!lstrcmpW(text,L"Children") || !lstrcmpW(text,L"Items"))
			{
				CheckIf(!m_nodes,QuadooForm::EInvalidState);
				Check(WrapTreeNodes(m_nodes,m_node,result,m_cache));
			}
			else if(!lstrcmpW(text,L"Expanded") || !lstrcmpW(text,L"Checked") || !lstrcmpW(text,L"Attached"))
			{
				BOOL flag;
				if(!lstrcmpW(text,L"Expanded"))
				{
					Check(m_node->GetExpanded(&flag));
				}
				else if(!lstrcmpW(text,L"Checked"))
				{
					Check(m_node->GetChecked(&flag));
				}
				else
				{
					Check(m_node->GetAttached(&flag));
				}
				result->eType = QuadooVM::Bool;
				result->fVal = flag != FALSE;
			}
			else
			{
				INT number;
				if(!lstrcmpW(text,L"ImageIndex"))
				{
					Check(m_node->GetImageIndex(&number));
				}
				else if(!lstrcmpW(text,L"SelectedImageIndex"))
				{
					Check(m_node->GetSelectedImageIndex(&number));
				}
				else if(!lstrcmpW(text,L"Index") || !lstrcmpW(text,L"ChildCount"))
				{
					UINT index;
					if(!lstrcmpW(text,L"Index"))
					{
						Check(m_node->GetIndex(&index));
					}
					else
					{
						Check(m_node->GetChildCount(&index));
					}
					number = index;
				}
				else
				{
					Check(DISP_E_MEMBERNOTFOUND);
				}
				result->eType = QuadooVM::I4;
				result->lVal = number;
			}
			hr = S_OK;
		Cleanup:
			return hr;
		}
		STDMETHOD(SetProperty)(IQuadooVM*, RSTRING name, QuadooVM::QVARIANT* value)
		{
			HRESULT hr;
			PCWSTR text = RStrToWide(name);
			CheckIf(!text || !value,E_INVALIDARG);
			if(!lstrcmpW(text,L"Text"))
			{
				CheckIf(value->eType != QuadooVM::String,DISP_E_TYPEMISMATCH);
				Check(m_node->SetText(value->rstrVal));
			}
			else if(!lstrcmpW(text,L"Expanded") || !lstrcmpW(text,L"Checked"))
			{
				CheckIf(value->eType != QuadooVM::Bool,DISP_E_TYPEMISMATCH);
				if(!lstrcmpW(text,L"Expanded"))
				{
					Check(m_node->SetExpanded(value->fVal));
				}
				else
				{
					Check(m_node->SetChecked(value->fVal));
				}
			}
			else
			{
				LONGLONG number;
				Check(VariantInteger(value,&number));
				CheckIf(number < -1 || number > INT_MAX,DISP_E_OVERFLOW);
				if(!lstrcmpW(text,L"ImageIndex"))
				{
					Check(m_node->SetImageIndex(static_cast<INT>(number)));
				}
				else if(!lstrcmpW(text,L"SelectedImageIndex"))
				{
					Check(m_node->SetSelectedImageIndex(static_cast<INT>(number)));
				}
				else
				{
					Check(DISP_E_MEMBERNOTFOUND);
				}
			}
		Cleanup:
			return hr;
		}
		STDMETHOD(Invoke)(IQuadooVM*, RSTRING method, QuadooVM::QVPARAMS* parameters, QuadooVM::QVARIANT* result)
		{
			HRESULT hr;
			PCWSTR text = RStrToWide(method);
			CheckIf(!text || !parameters || !result,E_INVALIDARG);
			result->eType = QuadooVM::Null;
			if(!lstrcmpW(text,L"Delete"))
			{
				CheckIf(parameters->cArgs,DISP_E_BADPARAMCOUNT);
				CheckIf(!m_nodes,QuadooForm::EInvalidState);
				Check(m_nodes->Delete(m_node));
			}
			else if(!lstrcmpW(text,L"Expand") || !lstrcmpW(text,L"Collapse"))
			{
				CheckIf(parameters->cArgs,DISP_E_BADPARAMCOUNT);
				Check(m_node->SetExpanded(!lstrcmpW(text,L"Expand")));
			}
			else if(!lstrcmpW(text,L"MoveTo"))
			{
				TStackRef<IQuadooTreeNode> parent;
				UINT index;
				CheckIf(parameters->cArgs != 2,DISP_E_BADPARAMCOUNT);
				CheckIf(!m_nodes,QuadooForm::EInvalidState);
				Check(UnwrapTreeNode(Argument(parameters,0),&parent));
				Check(Index(Argument(parameters,1),&index));
				Check(m_nodes->Move(m_node,parent,index));
			}
			else
			{
				hr = DISP_E_MEMBERNOTFOUND;
			}
		Cleanup:
			return hr;
		}
	};
	class CTreeNodesObject : public CBaseUnknown, public CQuadooObjectImpl
	{
		TStackRef<IQuadooTreeNodes> m_nodes;
		TStackRef<IQuadooTreeNode> m_parent;
		TStackRef<CInstanceLink> m_cache;
		HRESULT Count(UINT* count)
		{
			return m_parent ? m_parent->GetChildCount(count) : m_nodes->GetCount(count);
		}
		HRESULT At(UINT index, IQuadooTreeNode** node)
		{
			return m_parent ? m_parent->GetChild(index,node) : m_nodes->GetAt(index,node);
		}
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooObject)
		END_UNK_MAP
		CTreeNodesObject(IQuadooTreeNodes* nodes, IQuadooTreeNode* parent, CInstanceLink* cache) : m_nodes(nodes), m_parent(parent), m_cache(cache)
		{
		}
		STDMETHOD(GetProperty)(IQuadooVM*, RSTRING name, QuadooVM::QVARIANT* result)
		{
			HRESULT hr;
			UINT count;
			CheckIf(!name || !result,E_INVALIDARG);
			CheckIf(lstrcmpW(RStrToWide(name),L"Count"),DISP_E_MEMBERNOTFOUND);
			Check(Count(&count));
			result->eType = QuadooVM::I4;
			result->lVal = count;
		Cleanup:
			return hr;
		}
		STDMETHOD(Invoke)(IQuadooVM*, RSTRING method, QuadooVM::QVPARAMS* parameters, QuadooVM::QVARIANT* result)
		{
			HRESULT hr;
			PCWSTR text = RStrToWide(method);
			UINT index;
			UINT count;
			TStackRef<IQuadooTreeNode> node;
			CheckIf(!text || !parameters || !result,E_INVALIDARG);
			result->eType = QuadooVM::Null;
			if(!lstrcmpW(text,L"GetAt") || !lstrcmpW(text,L"GetItem"))
			{
				CheckIf(parameters->cArgs != 1,DISP_E_BADPARAMCOUNT);
				Check(Index(Argument(parameters,0),&index));
				Check(At(index,&node));
				Check(WrapTreeNode(node,m_nodes,result,m_cache));
			}
			else if(!lstrcmpW(text,L"Add") || !lstrcmpW(text,L"Insert"))
			{
				BOOL append = !lstrcmpW(text,L"Add");
				CheckIf(parameters->cArgs != (append ? 1 : 2),DISP_E_BADPARAMCOUNT);
				QuadooVM::QVARIANT* caption = Argument(parameters,append ? 0 : 1);
				CheckIf(caption->eType != QuadooVM::String,DISP_E_TYPEMISMATCH);
				if(append)
				{
					Check(Count(&index));
				}
				else
				{
					Check(Index(Argument(parameters,0),&index));
				}
				Check(m_nodes->Insert(m_parent,index,caption->rstrVal,&node));
				Check(WrapTreeNode(node,m_nodes,result,m_cache));
			}
			else if(!lstrcmpW(text,L"Find"))
			{
				TStackRef<IJSONValue> value;
				GUID id;
				CheckIf(parameters->cArgs != 1,DISP_E_BADPARAMCOUNT);
				CheckIf(Argument(parameters,0)->eType != QuadooVM::String,DISP_E_TYPEMISMATCH);
				Check(JSONCreateString(Argument(parameters,0)->rstrVal,&value));
				Check(value->GetStringAsGuid(id));
				Check(m_nodes->Find(id,&node));
				Check(WrapTreeNode(node,m_nodes,result,m_cache));
			}
			else if(!lstrcmpW(text,L"Delete"))
			{
				CheckIf(parameters->cArgs != 1,DISP_E_BADPARAMCOUNT);
				if(Argument(parameters,0)->eType == QuadooVM::Object)
				{
					Check(UnwrapTreeNode(Argument(parameters,0),&node));
				}
				else
				{
					Check(Index(Argument(parameters,0),&index));
					Check(At(index,&node));
				}
				Check(m_nodes->Delete(node));
			}
			else if(!lstrcmpW(text,L"Move"))
			{
				TStackRef<IQuadooTreeNode> parent;
				CheckIf(parameters->cArgs != 3,DISP_E_BADPARAMCOUNT);
				Check(UnwrapTreeNode(Argument(parameters,0),&node));
				Check(UnwrapTreeNode(Argument(parameters,1),&parent));
				Check(Index(Argument(parameters,2),&index));
				Check(m_nodes->Move(node,parent,index));
			}

			else if(!lstrcmpW(text,L"Clear"))
			{
				CheckIf(parameters->cArgs,DISP_E_BADPARAMCOUNT);
				if(!m_parent)
				{
					Check(m_nodes->Clear());
				}
				else
				{
					Check(Count(&count));
					while(count)
					{
						node.Release();
						Check(At(--count,&node));
						Check(m_nodes->Delete(node));
					}
				}
			}
			else
			{
				hr = DISP_E_MEMBERNOTFOUND;
			}
		Cleanup:
			return hr;
		}
	};
	VOID ClearTreeObjects(CInstanceLink* cache)
	{
		for(sysint n = 0; n < cache->treeObjects.Length(); n++)
		{
			TStackRef<ITreeNodeObject> object;
			if(SUCCEEDED(cache->treeObjects[n]->QueryInterface(__uuidof(ITreeNodeObject),reinterpret_cast<PVOID*>(&object))))
			{
				object->DetachCache();
			}
			cache->treeObjects[n]->Release();
		}
		cache->treeObjects.Clear();
	}
	HRESULT WrapTreeNode(IQuadooTreeNode* node, IQuadooTreeNodes* nodes, QuadooVM::QVARIANT* result, CInstanceLink* cache)
	{
		HRESULT hr = S_OK;
		TStackRef<CTreeNodeObject> created;
		result->eType = QuadooVM::Null;
		CheckIfIgnore(!node,S_OK);
		if(cache)
		{
			BOOL attached;
			Check(node->GetAttached(&attached));
			if(!attached)
			{
				cache = NULL;
			}
		}
		if(cache)
		{
			for(sysint n = cache->treeObjects.Length(); n > 0; n--)
			{
				TStackRef<ITreeNodeObject> object;
				TStackRef<IQuadooTreeNode> candidate;
				BOOL attached;
				Check(cache->treeObjects[n - 1]->QueryInterface(__uuidof(ITreeNodeObject),reinterpret_cast<PVOID*>(&object)));
				Check(object->GetNode(&candidate));
				Check(candidate->GetAttached(&attached));
				if(!attached)
				{
					object->DetachCache();
					cache->treeObjects[n - 1]->Release();
					cache->treeObjects.Remove(n - 1,NULL);
				}
				else if(SameIdentity(candidate,node))
				{
					result->pObject = cache->treeObjects[n - 1];
					result->pObject->AddRef();
					result->eType = QuadooVM::Object;
					goto Cleanup;
				}
			}
		}
		created.Attach(__new CTreeNodeObject(node,nodes,cache));
		CheckAlloc(created);
		if(cache)
		{
			Check(cache->treeObjects.Append(static_cast<IQuadooObject*>(created)));
			created->AddRef();
		}
		result->pObject = created.Detach();
		result->eType = QuadooVM::Object;
	Cleanup:
		return hr;
	}
	HRESULT WrapTreeNodes(IQuadooTreeNodes* nodes, IQuadooTreeNode* parent, QuadooVM::QVARIANT* result, CInstanceLink* cache)
	{
		HRESULT hr = S_OK;
		result->eType = QuadooVM::Null;
		result->pObject = __new CTreeNodesObject(nodes,parent,cache);
		CheckAlloc(result->pObject);
		result->eType = QuadooVM::Object;
	Cleanup:
		return hr;
	}
	HRESULT UnwrapTreeNode(const QuadooVM::QVARIANT* value, IQuadooTreeNode** node)
	{
		HRESULT hr = S_OK;
		TStackRef<ITreeNodeObject> object;
		*node = NULL;
		CheckIfIgnore(value->eType == QuadooVM::Null,S_OK);
		CheckIf(value->eType != QuadooVM::Object,DISP_E_TYPEMISMATCH);
		Check(value->pObject->QueryInterface(__uuidof(ITreeNodeObject),reinterpret_cast<PVOID*>(&object)));
		Check(object->GetNode(node));
	Cleanup:
		return hr;
	}
}
