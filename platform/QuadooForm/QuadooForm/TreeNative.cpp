#include "TreeView.h"
#include "MenuRuntime.h"

namespace QF
{
	static VOID SetTreeImages(HWND window, HIMAGELIST images)
	{
		if(TreeView_GetImageList(window,TVSIL_NORMAL) != images)
		{
			// Changing normal image dimensions can replace Win32's generated
			// checkbox list. The application owns both old and current lists.
			HIMAGELIST previous = TreeView_GetImageList(window,TVSIL_STATE);
			TreeView_SetImageList(window,images,TVSIL_NORMAL);
			HIMAGELIST current = TreeView_GetImageList(window,TVSIL_STATE);
			if(previous && previous != current)
			{
				ImageList_Destroy(previous);
			}
		}
	}
	CTreeImageObserver::CTreeImageObserver() : tree(NULL)
	{
	}
	STDMETHODIMP_(VOID) CTreeImageObserver::OnImageListChanged(IQuadooImageList*)
	{
		if(tree)
		{
			TStackRef<CTreeView> keepAlive(tree);
			tree->RefreshImages();
		}
	}
	VOID CTreeView::DetachImages()
	{
		if(m_imageObserver)
		{
			m_imageObserver->tree = NULL;
		}
		if(m_hwnd)
		{
			SetTreeImages(m_hwnd,NULL);
		}
		if(m_imageList && m_imageObserver)
		{
			m_imageList->RemoveImageListObserver(m_imageObserver);
		}
		m_snapshot.Release();
		m_imageObserver.Release();
		m_imageList.Release();
		m_images.Release();
	}
	HRESULT CTreeView::AttachImages(IQuadooComponent* component)
	{
		HRESULT hr = S_OK;
		TStackRef<IQuadooImageList> list;
		TStackRef<IQuadooImageListSnapshot> snapshot;
		TStackRef<CTreeImageObserver> observer;
		HIMAGELIST handle = NULL;
		BOOL subscribed = FALSE;
		Check(CheckAlive());
		if(component)
		{
			Check(component->QueryInterface(__uuidof(IQuadooImageList),reinterpret_cast<PVOID*>(&list)));
			Check(list->Acquire(m_dpi,&snapshot));
			Check(snapshot->GetHandle(&handle));
			observer.Attach(__new CTreeImageObserver);
			CheckAlloc(observer);
			subscribed = TRUE;
			Check(list->AddImageListObserver(observer));
			QuadooForm::ComponentState state;
			Check(component->GetState(&state));
			CheckIf(state >= QuadooForm::Destroying,QuadooForm::EDestroyed);
		}
		Check(CheckAlive());
		DetachImages();
		Check(CheckAlive());
		m_images = component;
		m_imageList = list;
		m_snapshot = snapshot;
		m_imageObserver = observer;
		if(observer)
		{
			observer->tree = this;
		}
		if(m_hwnd)
		{
			SetTreeImages(m_hwnd,handle);
		}
	Cleanup:
		if(FAILED(hr) && subscribed && list && observer)
		{
			list->RemoveImageListObserver(observer);
		}
		return hr;
	}
	HRESULT CTreeView::ResolveImages()
	{
		HRESULT hr;
		TStackRef<IJSONValue> value;
		TStackRef<IQuadooControlContainer> root;
		TStackRef<IQuadooComponentHost> host;
		TStackRef<IQuadooComponentCollection> collection;
		TStackRef<IQuadooComponent> component;
		GUID id;
		Check(StoredValue(L"Images",&value));
		if(value)
		{
			Check(value->GetStringAsGuid(id));
			Check(GetNamespace(Control(),&root));
			Check(root->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&host)));
			Check(host->GetComponents(&collection));
			Check(FindComponentID(collection,id,&component));
		}
		Check(AttachImages(component));
	Cleanup:
		return hr;
	}
	HRESULT CTreeView::RefreshImages()
	{
		HRESULT hr = S_OK;
		TStackRef<CTreeView> keepAlive(this);
		TStackRef<IQuadooImageListSnapshot> replacement;
		HIMAGELIST handle = NULL;
		if(m_imageList)
		{
			hr = m_imageList->Acquire(m_dpi,&replacement);
			if(hr == QuadooForm::EDestroyed)
			{
				DetachImages();
				m_eventDepth++;
				hr = CControl::SetValue(FindProperty(L"Images"),QuadooProperty::ObjectReference,NULL);
				m_eventDepth--;
				goto Cleanup;
			}
			Check(hr);
			Check(replacement->GetHandle(&handle));
		}
		if(m_hwnd)
		{
			SetTreeImages(m_hwnd,handle);
		}
		m_snapshot = replacement;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CTreeView::GetImages(IQuadooComponent** images)
	{
		HRESULT hr;
		CheckIf(!images,E_POINTER);
		*images = NULL;
		Check(CheckAlive());
		*images = m_images;
		if(*images)
		{
			(*images)->AddRef();
		}
	Cleanup:
		return hr;
	}
	STDMETHODIMP CTreeView::SetImages(IQuadooComponent* component)
	{
		HRESULT hr;
		GUID id;
		CRString text;
		TStackRef<IJSONValue> value;
		Check(Mutable());
		if(component)
		{
			Check(component->GetID(&id));
			Check(GuidString(id,&text));
			Check(JSONCreateString(text,&value));
			// Resolve the candidate through this form/frame, never by pointer alone.
			TStackRef<IQuadooControlContainer> root;
			TStackRef<IQuadooComponentHost> host;
			TStackRef<IQuadooComponentCollection> collection;
			TStackRef<IQuadooComponent> candidate;
			Check(GetNamespace(Control(),&root));
			Check(root->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&host)));
			Check(host->GetComponents(&collection));
			Check(FindComponentID(collection,id,&candidate));
			CheckIf(!SameObject(candidate,component),E_INVALIDARG);
		}
		Check(SetNamedValue(L"Images",QuadooProperty::ObjectReference,value));
	Cleanup:
		return hr;
	}
	HRESULT CTreeView::ExpandNode(CTreeNode* node, BOOL expanded)
	{
		HRESULT hr;
		TStackRef<CTreeView> keepAlive(this);
		TStackRef<CTreeNode> retained(node);
		TStackRef<CTreeNode> previous(m_selected);
		BOOL cancel;
		CTreeNode* selection = m_selected;
		CheckIfIgnore(node->expanded == expanded,S_FALSE);
		Check(Event(expanded ? L"OnExpanding" : L"OnCollapsing",node,NULL,&cancel));
		CheckIfIgnore(cancel,S_FALSE);
		Check(CheckAlive());
		if(!expanded)
		{
			for(CTreeNode* ancestor = selection ? selection->parent : NULL; ancestor; ancestor = ancestor->parent)
			{
				if(ancestor == node)
				{
					selection = node;
					break;
				}
			}
		}
		if(selection != m_selected)
		{
			Check(Event(L"OnChanging",selection,previous,&cancel));
			CheckIfIgnore(cancel,S_FALSE);
			Check(CheckAlive());
		}
		// TVM_EXPAND stops notifying once TVIS_EXPANDEDONCE is set. Always
		// supply our own logical events and silence any native duplicates.
		m_silent++;
		BOOL selected = selection == m_selected || TreeView_SelectItem(m_hwnd,selection ? selection->item : NULL);
		TreeView_Expand(m_hwnd,node->item,expanded ? TVE_EXPAND : TVE_COLLAPSE);
		BOOL actual = (TreeView_GetItemState(m_hwnd,node->item,TVIS_EXPANDED) & TVIS_EXPANDED) != 0;
		BOOL valid = selected && (!node->children.Length() || actual == expanded);
		if(!valid && selection != m_selected)
		{
			TreeView_SelectItem(m_hwnd,m_selected ? m_selected->item : NULL);
		}
		m_silent--;
		CheckIf(!valid,E_FAIL);
		node->expanded = expanded;
		m_selected = selection;
		Check(Changed());
		if(selection != previous)
		{
			Check(Event(L"OnChange",selection,previous));
		}
		Check(Event(expanded ? L"OnExpanded" : L"OnCollapsed",node));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CTreeView::HandleMessage(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result)
	{
		HRESULT hr;
		CheckIf(!handled || !result,E_POINTER);
		*handled = FALSE;
		*result = 0;
		Check(CheckAlive());
		if(window == m_hwnd && message == WM_NOTIFY)
		{
			*result = Message(window,message,wp,lp,FALSE,handled);
		}
		else
		{
			Check(Super::HandleMessage(window,message,wp,lp,handled,result));
		}
		hr = S_OK;
	Cleanup:
		return hr;
	}
	LRESULT CTreeView::Message(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled)
	{
		if(m_eventDepth && !m_silent && (message == TVM_SELECTITEM || message == TVM_EXPAND ||
			message == TVM_EDITLABELW || message == TVM_SETITEMW || message == TVM_INSERTITEMW || message == TVM_DELETEITEM))
		{
			*handled = TRUE;
			return FALSE;
		}
		if(message == WM_DESTROY)
		{
			// TVS_CHECKBOXES creates a separate state image list; Win32 does not
			// destroy it for the application (including failed staging HWNDs).
			// Release before the native WM_DESTROY frees its item/window data.
			SetTreeImages(window,NULL);
			HIMAGELIST state = TreeView_GetImageList(window,TVSIL_STATE);
			TreeView_SetImageList(window,NULL,TVSIL_STATE);
			if(state)
			{
				ImageList_Destroy(state);
			}
		}
		if(message == WM_MOUSEWHEEL)
		{
			*handled = FALSE;
			return 0;
		}
		if(message == WM_SYSCOLORCHANGE)
		{
			SendMessageW(window,TVM_SETBKCOLOR,0,ResolveQuadooColor(Integer(L"BackColor")));
			SendMessageW(window,TVM_SETTEXTCOLOR,0,ResolveQuadooColor(Integer(L"ForeColor")));
		}
		if(!m_silent && Boolean(L"CheckBoxes") && (message == WM_LBUTTONDOWN || (message == WM_KEYDOWN && wp == VK_SPACE)))
		{
			if(message == WM_KEYDOWN)
			{
				LRESULT keyboard = Super::Message(window,message,wp,lp,content,handled);
				if(*handled)
				{
					return keyboard;
				}
			}
			HTREEITEM item = TreeView_GetSelection(window);
			BOOL toggle = message == WM_KEYDOWN;
			if(message == WM_LBUTTONDOWN)
			{
				TVHITTESTINFO hit = {0};
				hit.pt.x = static_cast<SHORT>(LOWORD(lp));
				hit.pt.y = static_cast<SHORT>(HIWORD(lp));
				item = TreeView_HitTest(window,&hit);
				toggle = (hit.flags & TVHT_ONITEMSTATEICON) != 0;
			}
			if(toggle && item)
			{
				for(sysint n = 0; n < nodes.Length(); n++)
				{
					if(nodes[n]->item == item)
					{
						TStackRef<CTreeNode> node(nodes[n]);
						ReportCallback(node->SetChecked(!node->checked),L"OnCheck");
						*handled = TRUE;
						return 0;
					}
				}
			}
			if(message == WM_KEYDOWN)
			{
				return 0;
			}
		}
		if(message == WM_NOTIFY && lp && reinterpret_cast<NMHDR*>(lp)->hwndFrom == m_hwnd)
		{
			NMHDR* header = reinterpret_cast<NMHDR*>(lp);
			switch(header->code)
			{
			case TVN_SELCHANGINGW:
			case TVN_SELCHANGEDW:
			case TVN_ITEMEXPANDINGW:
			case TVN_ITEMEXPANDEDW:
			case TVN_BEGINLABELEDITW:
			case TVN_ENDLABELEDITW:
				break;
			default:
				return Super::Message(window,message,wp,lp,content,handled);
			}
			CTreeNode* node = NULL;
			CTreeNode* oldNode = NULL;
			HRESULT hr = S_OK;
			BOOL cancel = FALSE;
			LRESULT result = 0;
			BOOL label = header->code == TVN_BEGINLABELEDITW || header->code == TVN_ENDLABELEDITW;
			NMTREEVIEWW* change = reinterpret_cast<NMTREEVIEWW*>(lp);
			NMTVDISPINFOW* edit = reinterpret_cast<NMTVDISPINFOW*>(lp);
			LPARAM pointer = label ? edit->item.lParam : change->itemNew.lParam;
			for(sysint n = 0; n < nodes.Length(); n++)
			{
				if(reinterpret_cast<LPARAM>(nodes[n]) == pointer)
				{
					node = nodes[n];
				}
				if(!label && reinterpret_cast<LPARAM>(nodes[n]) == change->itemOld.lParam)
				{
					oldNode = nodes[n];
				}
			}
			TStackRef<CTreeNode> keepNode(node);
			TStackRef<CTreeNode> keepOld(oldNode);
			if(m_silent)
			{
				*handled = TRUE;
				return 0;
			}
			if(m_eventDepth)
			{
				*handled = TRUE;
				return header->code == TVN_SELCHANGINGW || header->code == TVN_ITEMEXPANDINGW || header->code == TVN_BEGINLABELEDITW;
			}

			switch(header->code)
			{
			case TVN_SELCHANGINGW:
				hr = Event(L"OnChanging",node,oldNode,&cancel);
				result = cancel || FAILED(hr) || m_eventDepth;
				break;
			case TVN_SELCHANGEDW:
				m_selected = node;
				hr = Changed();
				if(SUCCEEDED(hr))
				{
					hr = Event(L"OnChange",node,oldNode);
				}
				break;
			case TVN_ITEMEXPANDINGW:
				hr = Event((change->action & TVE_EXPAND) ? L"OnExpanding" : L"OnCollapsing",node,NULL,&cancel);
				result = cancel || FAILED(hr) || m_eventDepth;
				break;
			case TVN_ITEMEXPANDEDW:
				if(node)
				{
					node->expanded = (change->action & TVE_EXPAND) != 0;
					hr = Changed();
					if(SUCCEEDED(hr))
					{
						hr = Event(node->expanded ? L"OnExpanded" : L"OnCollapsed",node);
					}
				}
				break;
			case TVN_BEGINLABELEDITW:
				hr = Event(L"OnEditing",node,NULL,&cancel);
				result = cancel || FAILED(hr) || m_eventDepth || Boolean(L"ReadOnly");
				break;
			case TVN_ENDLABELEDITW:
				if(node && edit->item.pszText)
				{
					RSTRING text = NULL;
					hr = RStrCreateW(TStrLenAssert(edit->item.pszText),edit->item.pszText,&text);
					if(SUCCEEDED(hr))
					{
						hr = Event(L"OnEdited",node,NULL,&cancel,&text);
					}
					if(SUCCEEDED(hr) && !cancel)
					{
						hr = node->SetText(text);
					}
					RStrRelease(text);
				}
				// The logical setter committed the value. Never let the old HWND
				// commit another copy, particularly after a callback destroyed it.
				result = FALSE;
				break;
			default:
				return Super::Message(window,message,wp,lp,content,handled);
			}
			ReportCallback(hr,L"TreeView");
			*handled = TRUE;
			return result;
		}
		return Super::Message(window,message,wp,lp,content,handled);
	}
}
