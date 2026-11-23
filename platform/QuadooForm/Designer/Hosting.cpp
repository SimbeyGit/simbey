#include "Designer.h"

namespace QFD
{
	LRESULT CALLBACK CDesignerEditor::PaletteHostProc(HWND window, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data)
	{
		CDesignerEditor* editor = reinterpret_cast<CDesignerEditor*>(data);
		if(msg == WM_NOTIFY) return SendMessageW(editor->m_window,msg,wp,lp);
		if(msg == WM_SIZE)
		{
			RECT client; GetClientRect(window,&client);
			INT title = MulDiv(24,editor->m_dpi,96);
			MoveWindow(GetDlgItem(window,111),4,2,max(1,client.right - 8),title - 2,TRUE);
			MoveWindow(editor->m_tools,0,title,client.right,max(1,client.bottom - title),TRUE);
			ListView_SetColumnWidth(editor->m_tools,0,max(20,client.right - GetSystemMetrics(SM_CXVSCROLL)));
			if(!editor->m_paletteLaidOut && client.bottom > title + 40 && client.right > 20)
			{
				// An embedded palette starts at 1x1. Reveal Pointer once after the
				// host supplies its usable size, never on later resizes/tool resets.
				ListView_EnsureVisible(editor->m_tools,0,FALSE);
				editor->m_paletteLaidOut = TRUE;
			}
			return 0;
		}
		if(msg == WM_NCDESTROY) RemoveWindowSubclass(window,PaletteHostProc,id);
		return DefSubclassProc(window,msg,wp,lp);
	}
	HRESULT CDesignerEditor::GetOutlineEntry(UINT index, OutlineEntry* entry)
	{
		HRESULT hr;
		TStackRef<IQuadooControlContainer> parent;
		TStackRef<IQuadooDesignPlaceholder> placeholder;
		CheckIf(!entry,E_POINTER);
		CheckIf(index >= static_cast<UINT>(m_nodes.Length()),E_INVALIDARG);
		entry->name = static_cast<RSTRING>(NULL); entry->type = static_cast<RSTRING>(NULL);
		entry->id = m_nodes[index].id; entry->parent = GUID_NULL;
		entry->flags = m_nodes[index].control ? OutlineVisual : 0;
		Check(m_nodes[index].component->GetName(&entry->name));
		Check(m_nodes[index].component->GetTypeName(&entry->type));
		if(m_nodes[index].control)
		{
			Check(m_nodes[index].control->GetParent(&parent));
			if(parent)
			{
				Check(parent->GetID(&entry->parent));
			}
		}
		else
		{
			TStackRef<IQuadooMenuItem> item;
			TStackRef<IQuadooComponentOutline> outline;
			if(SUCCEEDED(m_nodes[index].component->QueryInterface(&outline)))
			{
				Check(outline->GetOutlineParentID(&entry->parent));
			}
			else if(SUCCEEDED(m_nodes[index].component->QueryInterface(__uuidof(IQuadooMenuItem),reinterpret_cast<PVOID*>(&item))))
			{
				Check(item->GetMenuParentID(&entry->parent));
			}
			else
			{
				Check(m_document.Root()->GetID(&entry->parent));
			}
		}
		if(SUCCEEDED(m_nodes[index].component->QueryInterface(__uuidof(IQuadooDesignPlaceholder),reinterpret_cast<PVOID*>(&placeholder)))) entry->flags |= OutlinePlaceholder;
	Cleanup:
		return hr;
	}
	HRESULT CDesignerEditor::GetSelectionInfo(RSTRING* name, QuadooForm::Rect* bounds, BOOL* visual)
	{
		HRESULT hr;
		TStackRef<IQuadooComponent> component;
		TStackRef<IQuadooControl> control;
		CheckIf(!name || !bounds || !visual,E_POINTER);
		*name = NULL; *visual = FALSE; ZeroMemory(bounds,sizeof(*bounds));
		Check(SelectedComponent(&component));
		if(SelectionCount() > 1)
		{
			Check(RStrFormatW(name,L"%u controls",SelectionCount()));
		}
		else
		{
			Check(component->GetName(name));
		}
		hr = component->QueryInterface(__uuidof(IQuadooControl),reinterpret_cast<PVOID*>(&control));
		if(hr == E_NOINTERFACE) hr = S_OK;
		else
		{
			Check(hr);
			Check(control->GetBounds(bounds));
			*visual = TRUE;
		}
	Cleanup:
		return hr;
	}
	BOOL CDesignerEditor::CanExecute(UINT command)
	{
		if(!m_document.Root()) return FALSE;
		if(m_readOnly && command != SelectRoot && command != SelectParent && command != Copy &&
			command != Grid && command != Snap && command != ResetLayout && (command < Zoom50 || command > Zoom200))
		{
			return FALSE;
		}
		if(SelectionCount() > 1 && (command == MenuEditor || command == ImageListEditor || command == TreeNodesEditor || command == ListColumnsEditor ||
			command == TabOrder || command == CreateOpenHandler || command == SendToBack || command == SendToFront))
		{
			return FALSE;
		}
		{
			TStackRef<IQuadooComponent> component;
			TStackRef<IQuadooStatusPanels> panels;
			TStackRef<IQuadooStatusPanel> panel;
			if(SUCCEEDED(SelectedComponent(&component)) &&
				(SUCCEEDED(component->QueryInterface(&panels)) || SUCCEEDED(component->QueryInterface(&panel))))
			{
				if(command == Cut || command == Copy || command == Paste || command == SendToBack || command == SendToFront)
				{
					return FALSE;
				}
				if(command == DeleteSelection)
				{
					return panel != NULL;
				}
			}
		}
		switch(command)
		{
		case AddStatusPanel:
		case MoveStatusPanelLeft:
		case MoveStatusPanelRight:
			{
				TStackRef<IQuadooComponent> component;
				TStackRef<IQuadooStatusBar> bar;
				TStackRef<IQuadooStatusPanels> panels;
				TStackRef<IQuadooStatusPanel> panel;
				UINT index;
				if(SelectionCount() != 1 || FAILED(SelectedComponent(&component)))
				{
					return FALSE;
				}
				if(command == AddStatusPanel)
				{
					return SUCCEEDED(component->QueryInterface(&bar)) || SUCCEEDED(component->QueryInterface(&panels)) ||
						SUCCEEDED(component->QueryInterface(&panel));
				}
				if(FAILED(component->QueryInterface(&panel)) || FAILED(panel->GetIndex(&index)))
				{
					return FALSE;
				}
				if(command == MoveStatusPanelLeft)
				{
					return index > 0;
				}
				TStackRef<IQuadooComponentOutline> outline;
				TStackRef<IQuadooComponent> group;
				GUID parent;
				UINT count;
				return SUCCEEDED(component->QueryInterface(&outline)) && SUCCEEDED(outline->GetOutlineParentID(&parent)) &&
					SUCCEEDED(m_document.FindComponent(parent,&group)) && SUCCEEDED(group->QueryInterface(&panels)) &&
					SUCCEEDED(panels->GetCount(&count)) && index + 1 < count;
			}
		case Preview:
			return m_document.IsForm();
		case DeleteSelection:
		case SelectParent:
		case Cut:
		case Copy:
		case SendToBack:
		case SendToFront:
			{ GUID root; return SUCCEEDED(m_document.Root()->GetID(&root)) && !IsEqualGUID(root,m_selected); }
		case TreeNodesEditor:
			{
				TStackRef<IQuadooComponent> component;
				TStackRef<IQuadooTreeView> tree;
				return SUCCEEDED(SelectedComponent(&component)) &&
					SUCCEEDED(component->QueryInterface(__uuidof(IQuadooTreeView),reinterpret_cast<PVOID*>(&tree)));
			}
		case ListColumnsEditor:
			{
				TStackRef<IQuadooComponent> component;
				TStackRef<IQuadooListView> view;
				return SUCCEEDED(SelectedComponent(&component)) &&
					SUCCEEDED(component->QueryInterface(__uuidof(IQuadooListView),reinterpret_cast<PVOID*>(&view)));
			}
		case ImageListEditor:
			{
				TStackRef<IQuadooComponent> component;
				CRString type;
				return SUCCEEDED(SelectedComponent(&component)) && SUCCEEDED(component->GetTypeName(&type)) && type == RSTRING_CAST(L"ImageList");
			}
		case MenuEditor:
			{
				TStackRef<IQuadooComponent> selected;
				TStackRef<IQuadooControl> control;
				TStackRef<IQuadooMenuItems> items;
				TStackRef<IQuadooComponentHost> host;
				TStackRef<IQuadooComponentCollection> components;
				UINT count;
				if(FAILED(SelectedComponent(&selected)) ||
					(FAILED(selected->QueryInterface(__uuidof(IQuadooControl),reinterpret_cast<PVOID*>(&control))) &&
					FAILED(selected->QueryInterface(__uuidof(IQuadooMenuItems),reinterpret_cast<PVOID*>(&items)))))
				{
					return FALSE;
				}
				if(FAILED(m_document.Root()->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&host))) || FAILED(host->GetComponents(&components)) || FAILED(components->GetCount(&count)))
				{
					return FALSE;
				}
				for(UINT n = 0; n < count; n++)
				{
					TStackRef<IQuadooComponent> component;
					TStackRef<IQuadooMenu> menu;
					if(SUCCEEDED(components->GetAt(n,&component)) && SUCCEEDED(component->QueryInterface(__uuidof(IQuadooMenu),reinterpret_cast<PVOID*>(&menu))))
					{
						return TRUE;
					}
				}
				return FALSE;
			}
		case TabOrder:
			{
				TStackRef<IQuadooComponent> component;
				TStackRef<IQuadooControlContainer> parent;
				return SUCCEEDED(SelectedComponent(&component)) && SUCCEEDED(component->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&parent)));
			}
		case Paste:
			return IsClipboardFormatAvailable(CF_UNICODETEXT);
		case CreateOpenHandler:
			return m_document.SourceSite() && m_events && ListView_GetNextItem(m_list,-1,LVNI_SELECTED) >= 0;
		case ToggleEnabled:
			{
				CSelectionProperties properties;
				UINT index;
				TStackRef<IQuadooPropertyDescriptor> descriptor;
				QuadooProperty::Type type;
				DWORD flags;
				return SUCCEEDED(SelectionProperties(properties)) && SUCCEEDED(properties.Find(RSTRING_CAST(L"Enabled"),&index,&flags)) &&
					SUCCEEDED(properties.GetDescriptor(index,&descriptor)) && SUCCEEDED(descriptor->GetType(&type)) && type == QuadooProperty::Boolean &&
					!(flags & (QuadooForm::PropertyReadOnly | QuadooForm::PropertyRuntimeOnly));
			}
		case SelectRoot:
		case Zoom50:
		case Zoom75:
		case Zoom100:
		case Zoom150:
		case Zoom200:
		case Grid:
		case Snap:
		case ResetLayout:
		case About:
			return TRUE;
		}
		return FALSE;
	}

	BOOL CDesignerEditor::IsCommandChecked(UINT command)
	{
		if(command == ToggleEnabled)
		{
			for(sysint n = 0; n < m_selection.Length(); n++)
			{
				TStackRef<IQuadooComponent> component;
				if(FAILED(m_document.FindComponent(m_selection[n],&component)) || !BoolProperty(component,L"Enabled"))
				{
					return FALSE;
				}
			}
			return m_selection.Length() != 0;
		}
		if(command == Grid) return m_grid;
		if(command == Snap) return m_snap;
		if(command >= Zoom50 && command <= Zoom200)
		{
			const UINT values[] = { 50,75,100,150,200 };
			return m_zoom == values[command - Zoom50];
		}
		return FALSE;
	}

	HRESULT CDesignerEditor::ExecuteCommand(UINT command, HWND owner)
	{
		HRESULT hr;
		Check(FinishInline(TRUE));
		CheckIf(!CanExecute(command),E_ACCESSDENIED);
		switch(command)
		{
		case SendToBack:
		case SendToFront:
			Check(ReorderSelection(command == SendToFront));
			break;
		case Cut:
		case Copy:
			Check(CopySelection(command == Cut));
			break;
		case Paste:
			Check(PasteSelection());
			break;
		case CreateOpenHandler:
			hr = OpenSelectedHandler();
			break;
		case ToggleEnabled:
			{
				CSelectionProperties properties;
				TStackRef<IJSONValue> value;
				UINT index;
				DWORD flags;

				Check(SelectionProperties(properties));
				Check(properties.Find(RSTRING_CAST(L"Enabled"),&index,&flags));
				Check(JSONCreateBool(!IsCommandChecked(ToggleEnabled),&value));
				Check(SetSelectedProperty(index,value));
			} break;
		case AddStatusPanel:
		case MoveStatusPanelLeft:
		case MoveStatusPanelRight:
			Check(EditStatusPanels(command));
			break;
		case DeleteSelection:
			Check(Delete());
			break;
		case SelectParent:
		case SelectRoot:
			SelectAncestor(command == SelectRoot);
			break;
		case Preview:
			{
				LRESULT result;
				Check(PreviewForm(owner,&result));
			}
			break;
		case TreeNodesEditor:
			Check(EditTreeNodes());
			break;
		case ListColumnsEditor:
			Check(EditListColumns());
			break;
		case ImageListEditor:
			Check(EditImageList());
			break;
		case MenuEditor:
			Check(EditMenu());
			break;
		case TabOrder:
			Check(EditTabOrder());
			break;
		case Zoom50:
		case Zoom75:
		case Zoom100:
		case Zoom150:
		case Zoom200:
			{
				const UINT values[] = { 50,75,100,150,200 };
				SetZoom(values[command - Zoom50]);
			}
			break;
		case Grid:
			SetGrid(!m_grid);
			break;
		case Snap:
			SetSnap(!m_snap);
			break;
		case ResetLayout:
			ResetPanes();
			break;
		case About:
			MessageBoxW(owner,L"QuadooForm Designer\nNative Form and Frame editor",L"About QuadooForm Designer",MB_OK | MB_ICONINFORMATION);
			break;
		}
		Notify();
		hr = S_OK;
	Cleanup:
		return hr;
	}
}
