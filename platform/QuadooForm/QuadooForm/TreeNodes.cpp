#include "TreeView.h"

namespace QF
{
	CTreeLink::CTreeLink(CFactory* factory) : m_factory(factory), tree(NULL), thread(GetCurrentThreadId())
	{
		m_factory->AddInstance();
	}
	CTreeLink::~CTreeLink()
	{
		m_factory->RemoveInstance();
	}
	HRESULT CTreeLink::Alive()
	{
		HRESULT hr = S_OK;
		CheckIf(thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
		CheckIf(!tree,QuadooForm::EDestroyed);
		{
			QuadooForm::ComponentState state;
			Check(tree->GetState(&state));
			CheckIf(state >= QuadooForm::Destroying,QuadooForm::EDestroyed);
		}
	Cleanup:
		return hr;
	}
	CTreeNode::CTreeNode(CTreeLink* owner) : link(owner), parent(NULL), item(NULL),
		image(-1), selectedImage(-1), expanded(FALSE), checked(FALSE), attached(TRUE)
	{
		id = GUID_NULL;
	}
	HRESULT CTreeNode::Alive()
	{
		HRESULT hr;
		Check(link->Alive());
		CheckIf(!attached,QuadooForm::EDestroyed);
	Cleanup:
		return hr;
	}
	STDMETHODIMP CTreeNode::GetID(GUID* value)
	{
		HRESULT hr = S_OK;
		CheckIf(!value,E_POINTER);
		CheckIf(link->thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
		*value = id;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CTreeNode::GetAttached(BOOL* value)
	{
		HRESULT hr = S_OK;
		CheckIf(!value,E_POINTER);
		CheckIf(link->thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
		*value = attached && link->tree != NULL;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CTreeNode::GetIndex(UINT* value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		Check(Alive());
		{
			TArray<CTreeNode*>& siblings = parent ? parent->children : link->tree->roots;
			for(sysint n = 0; n < siblings.Length(); n++)
			{
				if(siblings[n] == this)
				{
					*value = static_cast<UINT>(n);
					hr = S_OK;
					goto Cleanup;
				}
			}
		}
		hr = E_UNEXPECTED;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CTreeNode::GetParent(IQuadooTreeNode** value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		Check(Alive());
		*value = parent;
		if(*value)
		{
			(*value)->AddRef();
		}
	Cleanup:
		return hr;
	}
	STDMETHODIMP CTreeNode::GetChildCount(UINT* value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		Check(Alive());
		*value = static_cast<UINT>(children.Length());
	Cleanup:
		return hr;
	}
	STDMETHODIMP CTreeNode::GetChild(UINT index, IQuadooTreeNode** value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		Check(Alive());
		CheckIf(index >= static_cast<UINT>(children.Length()),HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
		*value = children[index];
		(*value)->AddRef();
	Cleanup:
		return hr;
	}
	STDMETHODIMP CTreeNode::GetText(RSTRING* value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		Check(Alive());
		RStrSet(*value,text);
	Cleanup:
		return hr;
	}
	STDMETHODIMP CTreeNode::SetText(RSTRING value)
	{
		HRESULT hr;
		Check(Alive());
		Check(link->tree->EditNode(this,0,value,0));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CTreeNode::GetImageIndex(INT* value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		Check(Alive());
		*value = image;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CTreeNode::SetImageIndex(INT value)
	{
		HRESULT hr;
		Check(Alive());
		Check(link->tree->EditNode(this,1,NULL,value));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CTreeNode::GetSelectedImageIndex(INT* value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		Check(Alive());
		*value = selectedImage;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CTreeNode::SetSelectedImageIndex(INT value)
	{
		HRESULT hr;
		Check(Alive());
		Check(link->tree->EditNode(this,2,NULL,value));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CTreeNode::GetExpanded(BOOL* value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		Check(Alive());
		*value = expanded;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CTreeNode::SetExpanded(BOOL value)
	{
		HRESULT hr;
		Check(Alive());
		Check(link->tree->EditNode(this,3,NULL,value));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CTreeNode::GetChecked(BOOL* value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		Check(Alive());
		*value = checked;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CTreeNode::SetChecked(BOOL value)
	{
		HRESULT hr;
		Check(Alive());
		Check(link->tree->EditNode(this,4,NULL,value));
	Cleanup:
		return hr;
	}
	CTreeNodes::CTreeNodes(CTreeLink* link) : m_link(link)
	{
	}
	STDMETHODIMP CTreeNodes::GetCount(UINT* count)
	{
		HRESULT hr;
		CheckIf(!count,E_POINTER);
		Check(m_link->Alive());
		*count = static_cast<UINT>(m_link->tree->roots.Length());
	Cleanup:
		return hr;
	}
	STDMETHODIMP CTreeNodes::GetAt(UINT index, IQuadooTreeNode** node)
	{
		HRESULT hr;
		CheckIf(!node,E_POINTER);
		*node = NULL;
		Check(m_link->Alive());
		CheckIf(index >= static_cast<UINT>(m_link->tree->roots.Length()),HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
		*node = m_link->tree->roots[index];
		(*node)->AddRef();
	Cleanup:
		return hr;
	}
	STDMETHODIMP CTreeNodes::Find(REFGUID id, IQuadooTreeNode** node)
	{
		HRESULT hr;
		CheckIf(!node,E_POINTER);
		*node = NULL;
		Check(m_link->Alive());
		for(sysint n = 0; n < m_link->tree->nodes.Length(); n++)
		{
			if(IsEqualGUID(id,m_link->tree->nodes[n]->id))
			{
				*node = m_link->tree->nodes[n];
				(*node)->AddRef();
				hr = S_OK;
				goto Cleanup;
			}
		}
		hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	Cleanup:
		return hr;
	}
	STDMETHODIMP CTreeNodes::Insert(IQuadooTreeNode* parent, UINT index, RSTRING text, IQuadooTreeNode** node)
	{
		HRESULT hr;
		Check(m_link->Alive());
		Check(m_link->tree->InsertNode(parent,index,text,node));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CTreeNodes::Delete(IQuadooTreeNode* node)
	{
		HRESULT hr;
		Check(m_link->Alive());
		Check(m_link->tree->DeleteNode(node));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CTreeNodes::Move(IQuadooTreeNode* node, IQuadooTreeNode* parent, UINT index)
	{
		HRESULT hr;
		Check(m_link->Alive());
		Check(m_link->tree->MoveNode(node,parent,index));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CTreeNodes::Clear(VOID)
	{
		HRESULT hr;
		Check(m_link->Alive());
		Check(m_link->tree->ClearNodes());
	Cleanup:
		return hr;
	}
}
