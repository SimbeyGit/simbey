#include "Designer.h"

namespace QFD
{
	HRESULT CDesignerEditor::AttachCollectionItems(IQuadooComponent* component, HTREEITEM parent, UINT depth)
	{
		HRESULT hr;
		TStackRef<IQuadooComponentOutline> outline;
		UINT count;
		CheckIf(depth >= 128,E_INVALIDARG);
		hr = component->QueryInterface(&outline);
		CheckIfIgnore(hr == E_NOINTERFACE,S_OK);
		Check(hr);
		Check(outline->GetOutlineCount(&count));
		CheckIf(count > 4096,E_INVALIDARG);
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IQuadooComponent> entry;
			TStackRef<IQuadooStatusPanel> panel;
			CRString name;
			CRString label;
			Node node;
			TVINSERTSTRUCTW insert = {0};
			Check(outline->GetOutlineItem(n,&entry));
			Check(entry->GetID(&node.id));
			Check(entry->GetName(&name));
			if(SUCCEEDED(entry->QueryInterface(&panel)))
			{
				CRString text;
				Check(panel->GetText(&text));
				Check(RStrFormatW(&label,L"[%u] %r",n,static_cast<RSTRING>(text)));
			}
			else
			{
				label = name;
			}
			insert.hParent = parent;
			insert.hInsertAfter = TVI_LAST;
			insert.item.mask = TVIF_TEXT | TVIF_PARAM;
			insert.item.pszText = const_cast<PWSTR>(label.c_wstr());
			insert.item.lParam = m_nodes.Length();
			node.component = entry;
			node.control = NULL;
			node.expanded = TRUE;
			node.tree = m_tree ? TreeView_InsertItem(m_tree,&insert) : NULL;
			CheckIf(m_tree && !node.tree,E_OUTOFMEMORY);
			Check(m_nodes.Append(node));
			entry.Detach();
			Check(AttachCollectionItems(node.component,node.tree,depth + 1));
		}
	Cleanup:
		return hr;
	}

	HRESULT CDesignerEditor::EditStatusPanels(UINT command)
	{
		HRESULT hr;
		TStackRef<IQuadooComponent> component;
		TStackRef<IQuadooStatusBar> bar;
		TStackRef<IQuadooStatusPanels> panels;
		TStackRef<IQuadooStatusPanel> panel;
		TStackRef<IQuadooComponent> entry;
		TStackRef<IJSONObject> snapshot;
		BOOL dirty = m_document.Dirty();
		BOOL entered = FALSE;
		GUID selected = m_selected;
		UINT count;
		UINT index;
		Check(FinishInline(TRUE));
		Check(SelectedComponent(&component));
		component->QueryInterface(&panel);
		for(UINT depth = 0; depth < 4; depth++)
		{
			TStackRef<IQuadooComponentOutline> outline;
			GUID parent;
			if(SUCCEEDED(component->QueryInterface(&bar)))
			{
				break;
			}
			Check(component->QueryInterface(&outline));
			Check(outline->GetOutlineParentID(&parent));
			component.Release();
			Check(m_document.FindComponent(parent,&component));
		}
		CheckIf(!bar,E_INVALIDARG);
		Check(bar->GetPanels(&panels));
		Check(panels->GetCount(&count));
		Check(m_document.Serialize(&snapshot));
		m_changeDepth++;
		entered = TRUE;
		if(command == AddStatusPanel)
		{
			Check(panels->Insert(count,&panel));
			Check(panel->QueryInterface(&entry));
		}
		else
		{
			CheckIf(!panel,E_INVALIDARG);
			Check(panel->GetIndex(&index));
			if(command == DeleteSelection)
			{
				Check(panels->Delete(panel));
				Check(panels->QueryInterface(&entry));
			}
			else
			{
				CheckIf(command != MoveStatusPanelLeft && command != MoveStatusPanelRight,E_INVALIDARG);
				CheckIf((command == MoveStatusPanelLeft && !index) ||
					(command == MoveStatusPanelRight && index + 1 >= count),DISP_E_BADINDEX);
				Check(panels->Move(panel,command == MoveStatusPanelLeft ? index - 1 : index + 1));
				Check(panel->QueryInterface(&entry));
			}
		}
		Check(entry->GetID(&selected));
		m_document.SetDirty(TRUE);
	Cleanup:
		if(entered)
		{
			if(FAILED(hr))
			{
				m_document.Load(GetParent(m_document.Host()),snapshot,m_document.Path());
				m_document.SetDirty(dirty);
			}
			m_changeDepth--;
			m_selection.Clear();
			m_selected = selected;
			HRESULT refresh = Refresh(TRUE);
			if(SUCCEEDED(hr))
			{
				hr = refresh;
			}
		}
		return hr;
	}
}
