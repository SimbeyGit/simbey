#include "Designer.h"

namespace QFD
{
	static const WCHAR EditorClass[] = L"QuadooForm.Designer.Editor.1";
	static const WCHAR CanvasClass[] = L"QuadooForm.Designer.Canvas.1";
	static const WCHAR OverlayClass[] = L"QuadooForm.Designer.Overlay.1";

	class CDesignerReport : public CBaseUnknown, public IQuadooConstructionReport
	{
	public:
		CRString detail;
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooConstructionReport)
		END_UNK_MAP
		STDMETHOD(OnError)(HRESULT error, RSTRING path, RSTRING message)
		{
			HRESULT hr = S_OK;
			if(!RStrLen(detail))
			{
				Check(RStrFormatW(&detail,L"%ls: %ls (0x%.8x)",RStrToWide(path),RStrToWide(message),error));
			}
		Cleanup:
			return hr;
		}
	};

	class CDesignerObserver : public CBaseUnknown, public IQuadooPropertyObserver
	{
		CDesignerEditor* m_editor;
		VOID Signal(IQuadooComponent* component, BOOL property)
		{
			if(m_editor)
			{
				for(sysint n = 0; n < m_editor->m_nodes.Length(); n++)
				{
					if(SameObject(component,m_editor->m_nodes[n].component))
					{
						m_editor->Changed(property);
						break;
					}
				}
			}
		}
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooPropertyObserver)
		END_UNK_MAP
		CDesignerObserver(CDesignerEditor* editor) : m_editor(editor) {}
		VOID Detach()
		{
			m_editor = NULL;
		}
		STDMETHOD(OnPropertyChanged)(IQuadooComponent* component, UINT) { Signal(component,TRUE); return S_OK; }
		STDMETHOD(OnBoundsChanged)(IQuadooControl* control, const QuadooForm::Rect*) { Signal(control,FALSE); return S_OK; }
	};

	CDesignerEditor::CDesignerEditor() : m_window(NULL), m_canvas(NULL), m_overlay(NULL), m_tree(NULL), m_tools(NULL), m_list(NULL), m_tabs(NULL), m_observer(NULL), m_selected(GUID_NULL), m_changeDepth(0), m_zoom(100), m_dpi(96), m_leftWidth(190), m_rightWidth(300), m_treeHeight(285), m_splitter(0), m_tool(0), m_refreshPosted(FALSE), m_grid(TRUE), m_snap(FALSE), m_events(FALSE), m_dragging(FALSE), m_newControl(FALSE), m_grip(-1)
	{
		m_paletteWindow = m_notifyWindow = NULL;
		m_session = 0;
		m_outlineRevision = 0;
		m_embedded = m_paletteLaidOut = FALSE;
		m_tray = NULL;
		m_images = NULL;
		m_paletteMouse = FALSE;
		m_inline = NULL;
		m_editControl = GUID_NULL;
		m_editRow = -1;
		m_editing = m_finishingEdit = m_inlineComposing = m_deferredRefresh = FALSE;
		m_editDropdown = FALSE;
		m_dropdownInitial = 0;
		m_dropdownType = QuadooProperty::String;
		m_clickSelection = GUID_NULL;
		m_scroll.X = m_scroll.Y = 0;
		m_arranging = m_layingOut = FALSE;
		m_readOnly = FALSE;
		SetRectEmpty(&m_ghost);
	}

	VOID CDesignerEditor::SetReadOnly(BOOL readOnly)
	{
		if(m_readOnly != readOnly)
		{
			m_readOnly = readOnly;
			if(readOnly)
			{
				FinishInline(FALSE);
				if(m_dragging)
					MouseUp(m_start,TRUE);
				ResetTool();
			}
			if(m_tools)
				EnableWindow(m_tools,!readOnly);
			if(m_overlay)
				InvalidateRect(m_overlay,NULL,FALSE);
		}
	}

	CDesignerEditor::~CDesignerEditor()
	{
		if(m_observer)
			m_observer->Detach();
		ClearNodes();
		ClearRows();
		ClearPalette();
		SafeRelease(m_observer);
	}

	static HRESULT RegisterClassName(PCWSTR name, WNDPROC procedure, HCURSOR cursor)
	{
		WNDCLASSEXW type;
		ZeroMemory(&type,sizeof(type));
		type.cbSize = sizeof(type);
		type.style = CS_DBLCLKS;
		type.hInstance = GetModuleHandle(NULL);
		type.lpfnWndProc = procedure;
		type.hCursor = cursor;
		type.lpszClassName = name;
		return RegisterClassExW(&type) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS ? S_OK : HrEnsureLastError();
	}

	static HWND Child(HWND parent, PCWSTR type, PCWSTR text, DWORD style, UINT id)
	{
		HWND child = CreateWindowExW(0,type,text,WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | style,0,0,1,1,parent,reinterpret_cast<HMENU>(id),GetModuleHandle(NULL),NULL);
		if(child)
			SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),FALSE);
		return child;
	}

	HRESULT CDesignerEditor::Create(HWND parent, IQuadooControlRegistry* registry)
	{
		return CreateHosted(parent,NULL,registry);
	}

	HRESULT CDesignerEditor::CreateHosted(HWND parent, const EditorHostOptions* options, IQuadooControlRegistry* registry)
	{
		HRESULT hr;
		CheckIf(options && (options->cbSize != sizeof(*options) || !IsWindow(options->paletteParent) || !IsWindow(options->notifyWindow)),E_INVALIDARG);
		m_embedded = options != NULL;
		if(options)
		{
			m_notifyWindow = options->notifyWindow;
			m_session = options->session;
		}
		CheckIf(m_window || !parent,E_INVALIDARG); Check(m_document.Initialize(registry));
		Check(RegisterClassName(EditorClass,WindowProc,LoadCursor(NULL,IDC_ARROW)));
		Check(RegisterClassName(CanvasClass,CanvasProc,LoadCursor(NULL,IDC_ARROW)));
		Check(RegisterClassName(OverlayClass,OverlayProc,LoadCursor(NULL,IDC_ARROW)));
		m_observer = __new CDesignerObserver(this);
		CheckAlloc(m_observer);
		m_window = CreateWindowExW(WS_EX_CONTROLPARENT,EditorClass,L"",WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,0,0,1,1,parent,NULL,GetModuleHandle(NULL),this); CheckIf(!m_window,HrEnsureLastError());
		if(!m_embedded)
		{
			CheckIf(!Child(m_window,L"STATIC",L"Objects",SS_LEFT,110),HrEnsureLastError());
			m_tree = Child(m_window,WC_TREEVIEWW,L"",TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_INFOTIP | WS_TABSTOP,101);
			CheckIf(!m_tree,HrEnsureLastError());
			CheckIf(!Child(m_window,L"STATIC",L"Toolbox",SS_LEFT,111),HrEnsureLastError());
		}
		else
		{
			m_paletteWindow = CreateWindowExW(0,L"STATIC",L"",WS_CHILD | WS_CLIPCHILDREN,0,0,1,1,options->paletteParent,NULL,GetModuleHandle(NULL),NULL);
			CheckIf(!m_paletteWindow,HrEnsureLastError());
			CheckIf(!SetWindowSubclass(m_paletteWindow,PaletteHostProc,1,reinterpret_cast<DWORD_PTR>(this)),HrEnsureLastError());
			CheckIf(!Child(m_paletteWindow,L"STATIC",L"Toolbox",SS_LEFT,111),HrEnsureLastError());
		}
		m_tools = Child(m_embedded ? m_paletteWindow : m_window,WC_LISTVIEWW,L"",LVS_REPORT | LVS_NOCOLUMNHEADER | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS | WS_TABSTOP,102);
		CheckIf(!Child(m_window,L"STATIC",L"Nonvisual components",SS_LEFT,112),HrEnsureLastError());
		m_tray = Child(m_window,WC_LISTVIEWW,L"",LVS_LIST | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS | WS_TABSTOP,107);
		CheckIf(!m_tools || !m_tray,HrEnsureLastError());
		CheckIf(!SetWindowSubclass(m_tools,PaletteProc,1,reinterpret_cast<DWORD_PTR>(this)),HrEnsureLastError());
		ListView_SetExtendedListViewStyle(m_tools,LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
		ListView_SetExtendedListViewStyle(m_tray,LVS_EX_INFOTIP | LVS_EX_DOUBLEBUFFER);
		{
			LVCOLUMNW column;
			ZeroMemory(&column,sizeof(column));
			column.mask = LVCF_WIDTH;
			column.cx = 175;
			ListView_InsertColumn(m_tools,0,&column);
		}
		ListView_SetColumnWidth(m_tray,0,175);
		m_tabs = Child(m_window,WC_TABCONTROLW,L"",WS_TABSTOP,103);
		m_list = Child(m_window,WC_LISTVIEWW,L"",LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP,104);
		CheckIf(!m_tools || !m_tabs || !m_list,HrEnsureLastError());
		CheckIf(!SetWindowSubclass(m_list,InspectorProc,1,reinterpret_cast<DWORD_PTR>(this)),HrEnsureLastError());
		ListView_SetExtendedListViewStyle(m_list,LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
		{
			LVCOLUMNW column;
			ZeroMemory(&column,sizeof(column));
			column.mask = LVCF_TEXT | LVCF_WIDTH;
			column.cx = 125;
			column.pszText = L"Property";
			ListView_InsertColumn(m_list,0,&column);
			column.cx = 160;
			column.pszText = L"Value";
			ListView_InsertColumn(m_list,1,&column);
		}
		{
			TCITEMW tab;
			ZeroMemory(&tab,sizeof(tab));
			tab.mask = TCIF_TEXT;
			tab.pszText = L"Properties";
			TabCtrl_InsertItem(m_tabs,0,&tab);
			tab.pszText = L"Events";
			TabCtrl_InsertItem(m_tabs,1,&tab);
		}
		// XP supports bottom-to-top child-window buffering, including the transparent
		// selection overlay. Keep this on the workspace; the inspector list view
		// supplies its own native double buffer.
		m_canvas = CreateWindowExW(WS_EX_COMPOSITED,CanvasClass,L"",WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_HSCROLL | WS_VSCROLL,0,0,1,1,m_window,reinterpret_cast<HMENU>(105),GetModuleHandle(NULL),this); CheckIf(!m_canvas,HrEnsureLastError());
		m_overlay = CreateWindowExW(WS_EX_TRANSPARENT,OverlayClass,L"",WS_CHILD | WS_VISIBLE,0,0,1,1,m_canvas,reinterpret_cast<HMENU>(106),GetModuleHandle(NULL),this); CheckIf(!m_overlay,HrEnsureLastError());
		Check(Palette());
		Layout();
		ResetTool();
		// Initial layout shows Pointer; later tool resets preserve the user's scroll.
		ListView_EnsureVisible(m_tools,0,FALSE);
	Cleanup:
		if(FAILED(hr) && m_window)
			DestroyWindow(m_window);
		return hr;
	}

	VOID CDesignerEditor::ClearRows()
	{
		for(sysint n = 0; n < m_rows.Length(); n++)
			RStrRelease(m_rows[n].name);
		m_rows.Clear();
	}

	VOID CDesignerEditor::ClearNodes()
	{
		for(sysint n = 0; n < m_nodes.Length(); n++)
		{
			TStackRef<IQuadooPropertyNotifications> notifications;
			if(m_observer && SUCCEEDED(m_nodes[n].component->QueryInterface(__uuidof(IQuadooPropertyNotifications),reinterpret_cast<PVOID*>(&notifications))))
				notifications->RemoveObserver(m_observer);
			m_nodes[n].component->Release();
		}
		m_nodes.Clear();
	}

	VOID CDesignerEditor::CloseSession()
	{
		m_notifyWindow = NULL;
		FinishInline(FALSE);
		if(m_observer)
			m_observer->Detach();
		ClearNodes();
		m_document.Clear();
		ClearPalette();
		if(IsWindow(m_paletteWindow))
			DestroyWindow(m_paletteWindow);
		m_paletteWindow = NULL;
	}

	static HRESULT AddPaletteRow(HWND list, RSTRING name, LPARAM data)
	{
		HRESULT hr = S_OK;
		LVITEMW item; ZeroMemory(&item,sizeof(item));
		item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
		item.iItem = ListView_GetItemCount(list);
		item.pszText = const_cast<PWSTR>(RStrToWide(name));
		item.lParam = data; item.iImage = 0;
		CheckIf(ListView_InsertItem(list,&item) < 0,E_OUTOFMEMORY);
	Cleanup:
		return hr;
	}

	HRESULT CDesignerEditor::Palette()
	{
		HRESULT hr;
		TStackRef<IQuadooFactoryCatalog> catalog;
		TStackRef<IQuadooComponentRegistry> components;
		UINT count;
		Check(m_document.Registry()->QueryInterface(__uuidof(IQuadooFactoryCatalog),reinterpret_cast<PVOID*>(&catalog)));
		Check(catalog->GetFactoryCount(&count));
		CheckIf(count > 4096,E_INVALIDARG);
		Check(AddPaletteRow(m_tools,RSTRING_CAST(L"Pointer"),0));
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IQuadooControlFactory> factory;
			CRString name;
			Tool tool;

			Check(catalog->GetFactory(n,&factory));
			Check(factory->GetTypeName(&name));
			if(name == RSTRING_CAST(L"Form") || name == RSTRING_CAST(L"Frame"))
				continue;
			tool.factory = factory;
			tool.name = name;
			tool.nonvisual = FALSE;
			tool.image = 1;
			Check(m_palette.Append(tool));
			factory.Detach(); name.Detach();
			Check(AddPaletteRow(m_tools,tool.name,m_palette.Length()));
		}
		hr = m_document.Registry()->QueryInterface(__uuidof(IQuadooComponentRegistry),reinterpret_cast<PVOID*>(&components));
		if(hr == E_NOINTERFACE)
			hr = S_OK;
		else
		{
			Check(hr);
			Check(components->GetComponentFactoryCount(&count));
			CheckIf(count > 4096,E_INVALIDARG);
			for(UINT n = 0; n < count; n++)
			{
				TStackRef<IQuadooComponentFactory> factory; CRString name; Tool tool;
				Check(components->GetComponentFactory(n,&factory));
				Check(factory->GetTypeName(&name));
				if(name == RSTRING_CAST(L"MenuItem"))
				{
					continue;
				}
				tool.factory = factory; tool.name = name; tool.nonvisual = TRUE; tool.image = 2;
				Check(m_palette.Append(tool));
				factory.Detach(); name.Detach();
				Check(AddPaletteRow(m_tools,tool.name,m_palette.Length()));
			}
		}
		Check(RebuildPaletteImages(m_dpi));
		ResetTool();
	Cleanup:
		return hr;
	}

	HRESULT CDesignerEditor::AttachNodes(IQuadooControl* control, HTREEITEM parent, UINT depth)
	{
		HRESULT hr;
		Node node; CRString name, type, label; TStackRef<IQuadooControlContainer> container; TStackRef<IQuadooPropertyNotifications> notifications;
		CheckIf(depth > 128,E_INVALIDARG); Check(control->GetID(&node.id)); Check(control->GetName(&name)); Check(control->GetTypeName(&type)); Check(RStrFormatW(&label,L"%ls (%ls)",name.c_wstr(),type.c_wstr()));
		TVINSERTSTRUCTW insert; ZeroMemory(&insert,sizeof(insert)); insert.hParent = parent; insert.hInsertAfter = TVI_LAST; insert.item.mask = TVIF_TEXT | TVIF_PARAM; insert.item.pszText = const_cast<PWSTR>(label.c_wstr()); insert.item.lParam = m_nodes.Length();
		node.tree = m_tree ? TreeView_InsertItem(m_tree,&insert) : NULL;
		CheckIf(m_tree && !node.tree,E_OUTOFMEMORY);
		node.component = control;
		node.control = control;
		node.expanded = depth == 0;
		Check(m_nodes.Append(node));
		control->AddRef();
		if(SUCCEEDED(control->QueryInterface(__uuidof(IQuadooPropertyNotifications),reinterpret_cast<PVOID*>(&notifications))))
		{
			Check(notifications->AddObserver(m_observer));
		}
		Check(AttachCollectionItems(control,node.tree,depth + 1));
		if(SUCCEEDED(control->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&container))))
		{
			UINT count; Check(container->GetChildCount(&count));
			for(UINT n = 0; n < count; n++) { TStackRef<IQuadooControl> child; Check(container->GetChild(n,&child)); Check(AttachNodes(child,node.tree,depth + 1)); }
		}
	Cleanup:
		return hr;
	}

	HRESULT CDesignerEditor::AttachMenuItems(IQuadooComponent* component, HTREEITEM parent, UINT depth)
	{
		HRESULT hr;
		TStackRef<IQuadooMenuItems> menu;
		UINT count;
		CheckIf(depth >= 128,E_INVALIDARG);
		hr = component->QueryInterface(__uuidof(IQuadooMenuItems),reinterpret_cast<PVOID*>(&menu));
		CheckIfIgnore(hr == E_NOINTERFACE,S_OK);
		Check(hr);
		Check(menu->GetItemCount(&count));
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IQuadooComponent> item;
			TStackRef<IQuadooPropertyNotifications> notifications;
			Node node;
			CRString name;
			CRString caption;
			CRString label;
			Check(menu->GetItem(n,&item));
			Check(item->GetID(&node.id));
			Check(item->GetName(&name));
			Check(StringProperty(item,L"Caption",&caption));
			Check(RStrFormatW(&label,L"%r (MenuItem: %r)",static_cast<RSTRING>(name),static_cast<RSTRING>(caption)));
			TVINSERTSTRUCTW insert = {0};
			insert.hParent = parent;
			insert.hInsertAfter = TVI_LAST;
			insert.item.mask = TVIF_TEXT | TVIF_PARAM;
			insert.item.pszText = const_cast<PWSTR>(label.c_wstr());
			insert.item.lParam = m_nodes.Length();
			node.tree = m_tree ? TreeView_InsertItem(m_tree,&insert) : NULL;
			CheckIf(m_tree && !node.tree,E_OUTOFMEMORY);
			node.component = item;
			node.control = NULL;
			node.expanded = FALSE;
			Check(m_nodes.Append(node));
			item->AddRef();
			if(SUCCEEDED(item->QueryInterface(__uuidof(IQuadooPropertyNotifications),reinterpret_cast<PVOID*>(&notifications))))
			{
				Check(notifications->AddObserver(m_observer));
			}
			Check(AttachMenuItems(item,node.tree,depth + 1));
		}
	Cleanup:
		return hr;
	}

	HRESULT CDesignerEditor::AttachComponents()
	{
		HRESULT hr;
		TStackRef<IQuadooComponentHost> host; TStackRef<IQuadooComponentCollection> components; UINT count;
		ListView_DeleteAllItems(m_tray);
		hr = m_document.Root()->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&host));
		if(hr == E_NOINTERFACE)
		{
			hr = S_OK;
			goto Cleanup;
		}
		Check(hr);
		CheckIf(!host,E_UNEXPECTED);
		Check(host->GetComponents(&components));
		Check(components->GetCount(&count));
		if(count)
		{
			TVINSERTSTRUCTW insert;
			ZeroMemory(&insert,sizeof(insert));
			insert.hParent = m_nodes[0].tree;
			insert.hInsertAfter = TVI_LAST;
			insert.item.mask = TVIF_TEXT | TVIF_PARAM; insert.item.pszText = L"Components";
			insert.item.lParam = -1;
			HTREEITEM group = m_tree ? TreeView_InsertItem(m_tree,&insert) : NULL;
			CheckIf(m_tree && !group,E_OUTOFMEMORY);
			for(UINT n = 0; n < count; n++)
			{
				Node node;
				TStackRef<IQuadooComponent> component;
				TStackRef<IQuadooPropertyNotifications> notifications;
				TStackRef<IQuadooDesignPlaceholder> placeholder; CRString name, type, label;

				Check(components->GetAt(n,&component));
				TStackRef<IQuadooMenuItem> menuItem;
				if(SUCCEEDED(component->QueryInterface(__uuidof(IQuadooMenuItem),reinterpret_cast<PVOID*>(&menuItem))))
				{
					continue;
				}
				Check(component->GetID(&node.id));
				Check(component->GetName(&name));
				Check(component->GetTypeName(&type));
				BOOL unresolved = SUCCEEDED(component->QueryInterface(__uuidof(IQuadooDesignPlaceholder),reinterpret_cast<PVOID*>(&placeholder)));
				Check(RStrFormatW(&label,L"[%ls] %ls (%ls)",unresolved ? L"?" : L"+",name.c_wstr(),type.c_wstr()));
				insert.hParent = group;
				insert.item.pszText = const_cast<PWSTR>(label.c_wstr());
				insert.item.lParam = m_nodes.Length();
				node.tree = m_tree ? TreeView_InsertItem(m_tree,&insert) : NULL;
				CheckIf(m_tree && !node.tree,E_OUTOFMEMORY);
				node.component = component;
				node.control = NULL;
				node.expanded = FALSE;

				LVITEMW item;
				ZeroMemory(&item,sizeof(item));
				item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE; item.iItem = ListView_GetItemCount(m_tray);
				item.pszText = const_cast<PWSTR>(name.c_wstr()); item.lParam = m_nodes.Length(); item.iImage = ComponentImage(component);
				CheckIf(ListView_InsertItem(m_tray,&item) < 0,E_OUTOFMEMORY);
				Check(m_nodes.Append(node)); component.Detach();
				if(SUCCEEDED(node.component->QueryInterface(__uuidof(IQuadooPropertyNotifications),reinterpret_cast<PVOID*>(&notifications))))
				{
					Check(notifications->AddObserver(m_observer));
				}
				Check(AttachMenuItems(node.component,node.tree,0));
			}
			if(m_tree)
				TreeView_Expand(m_tree,group,TVE_EXPAND);
		}
		hr = S_OK;
	Cleanup:
		return hr;
	}

	HRESULT CDesignerEditor::Refresh(BOOL tree)
	{
		HRESULT hr = S_OK;
		TArray<GUID> expanded;
		if(m_editing && !m_finishingEdit) { m_deferredRefresh = TRUE; return S_FALSE; }
		m_deferredRefresh = FALSE;
		m_changeDepth++;
		if(tree)
		{
			if(m_tree) SendMessageW(m_tree,WM_SETREDRAW,FALSE,0);
			SendMessageW(m_tray,WM_SETREDRAW,FALSE,0);
		}
		if(tree)
		{
			if(m_tree)
			{
				for(sysint n = 0; n < m_nodes.Length(); n++)
				{
					if(TreeView_GetItemState(m_tree,m_nodes[n].tree,TVIS_EXPANDED))
					{
						Check(expanded.Append(m_nodes[n].id));
					}
				}
			}
			ClearNodes();
			if(m_tree)
				TreeView_DeleteAllItems(m_tree);
			m_outlineRevision++;
			if(m_document.Root())
			{
				Check(AttachNodes(m_document.Root(),TVI_ROOT,0));
				Check(AttachComponents());
				for(sysint n = 0; n < m_nodes.Length(); n++)
				{
					BOOL expand = n == 0;
					for(sysint e = 0; e < expanded.Length(); e++)
					{
						if(IsEqualGUID(expanded[e],m_nodes[n].id))
							expand = TRUE;
					}
					if(m_tree && expand)
						TreeView_Expand(m_tree,m_nodes[n].tree,TVE_EXPAND);
				}
			}
		}
		{
			Check(NormalizeSelection());
			ListView_SetItemState(m_tray,-1,0,LVIS_SELECTED | LVIS_FOCUSED);
			for(INT row = 0; row < ListView_GetItemCount(m_tray); row++)
			{
				LVITEMW item; ZeroMemory(&item,sizeof(item)); item.mask = LVIF_PARAM; item.iItem = row;
				if(ListView_GetItem(m_tray,&item) && item.lParam >= 0 && item.lParam < m_nodes.Length() && IsEqualGUID(m_selected,m_nodes[item.lParam].id))
					ListView_SetItemState(m_tray,row,LVIS_SELECTED | LVIS_FOCUSED,LVIS_SELECTED | LVIS_FOCUSED);
			}
			for(sysint n = 0; m_tree && n < m_nodes.Length(); n++)
			{
				if(IsEqualGUID(m_selected,m_nodes[n].id))
				{
					TreeView_SelectItem(m_tree,m_nodes[n].tree);
					TreeView_EnsureVisible(m_tree,m_nodes[n].tree);
					break;
				}
			}
		}
		Check(Inspector()); ArrangeSurface();
	Cleanup:
		if(tree)
		{
			if(m_tree)
			{
				SendMessageW(m_tree,WM_SETREDRAW,TRUE,0);
				RedrawWindow(m_tree,NULL,NULL,RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
			}
			SendMessageW(m_tray,WM_SETREDRAW,TRUE,0);
			RedrawWindow(m_tray,NULL,NULL,RDW_INVALIDATE | RDW_ERASE | RDW_FRAME);
		}
		m_changeDepth--; Notify();
		return hr;
	}

	VOID CDesignerEditor::Notify()
	{
		if(m_window)
			PostMessageW(m_notifyWindow ? m_notifyWindow : GetParent(m_window),ChangedMessage,m_session,m_outlineRevision);
	}

	VOID CDesignerEditor::Changed(BOOL property)
	{
		if(m_changeDepth || !m_window)
			return;
		if(property)
			m_document.SetDirty(TRUE);
		if(!m_refreshPosted)
		{
			m_refreshPosted = TRUE;
			PostMessageW(m_window,RefreshMessage,0,0);
		}
	}

	HRESULT CDesignerEditor::NewDocument(BOOL form)
	{
		HRESULT hr = FinishInline(TRUE); if(FAILED(hr)) return hr;
		m_changeDepth++; hr = m_document.New(m_canvas,form); m_changeDepth--;
		if(SUCCEEDED(hr))
		{
			m_selection.Clear();
			m_selected = GUID_NULL;
			m_scroll.X = 0;
			m_scroll.Y = 0;
			hr = Refresh(TRUE);
		}
		return hr;
	}

	HRESULT CDesignerEditor::OpenDocument(PCWSTR path)
	{
		HRESULT hr = FinishInline(TRUE); if(FAILED(hr)) return hr;
		m_changeDepth++; hr = m_document.Open(m_canvas,path); m_changeDepth--;
		if(SUCCEEDED(hr))
		{
			m_selection.Clear();
			m_selected = GUID_NULL;
			m_scroll.X = 0;
			m_scroll.Y = 0;
			hr = Refresh(TRUE);
		}
		return hr;
	}

	HRESULT CDesignerEditor::LoadDefinition(IJSONObject* definition)
	{
		HRESULT hr = FinishInline(TRUE); if(FAILED(hr)) return hr;
		m_changeDepth++; hr = m_document.Load(m_canvas,definition); m_changeDepth--;
		if(SUCCEEDED(hr))
		{
			m_selection.Clear();
			m_selected = GUID_NULL;
			hr = Refresh(TRUE);
		}
		return hr;
	}

	HRESULT CDesignerEditor::SaveDocument(PCWSTR path)
	{
		HRESULT hr = FinishInline(TRUE);
		if(SUCCEEDED(hr))
			hr = m_document.Save(path);
		Notify();
		return hr;
	}

	HRESULT CDesignerEditor::Selected(IQuadooControl** control)
	{
		return m_document.Find(m_selected,control);
	}

	HRESULT CDesignerEditor::SelectedComponent(IQuadooComponent** component)
	{
		return m_document.FindComponent(m_selected,component);
	}

	HRESULT CDesignerEditor::GetSelectedID(GUID* id)
	{
		if(!id)
			return E_POINTER;
		*id = m_selected;
		return S_OK;
	}

	HRESULT CDesignerEditor::SelectID(REFGUID id, BOOL add)
	{
		HRESULT hr;
		TStackRef<IQuadooComponent> component;
		TStackRef<IQuadooControl> control;
		Check(m_document.FindComponent(id,&component));
		hr = component->QueryInterface(__uuidof(IQuadooControl),reinterpret_cast<PVOID*>(&control));
		if(hr == E_NOINTERFACE)
		{
			Check(FinishInline(TRUE));
			hr = UpdateSelection(id,add);
			CheckIfIgnore(hr == S_FALSE,S_FALSE);
			Check(hr);
			Check(Refresh(FALSE));
		}
		else
		{
			Check(hr);
			Check(Select(control,add));
		}
	Cleanup:
		if(hr == S_FALSE && !control)
		{
			Refresh(FALSE);
		}
		return hr;
	}

	HRESULT CDesignerEditor::Select(IQuadooControl* control, BOOL add)
	{
		HRESULT hr;
		TStackRef<IQuadooControl> current;
		TStackRef<IQuadooControl> selected;
		BOOL entered = FALSE;
		GUID id;
		CheckIf(!control,E_INVALIDARG);
		Check(control->GetID(&id));
		Check(FinishInline(TRUE));
		hr = UpdateSelection(id,add);
		CheckIfIgnore(hr == S_FALSE,S_FALSE);
		Check(hr);
		Check(m_document.Find(id,&selected));
		current = selected;
		m_changeDepth++; entered = TRUE;
		for(UINT depth = 0; current && depth < 128; depth++)
		{
			TStackRef<IQuadooTabPage> page; TStackRef<IQuadooControlContainer> parent;
			Check(current->GetParent(&parent));
			if(parent && SUCCEEDED(current->QueryInterface(__uuidof(IQuadooTabPage),reinterpret_cast<PVOID*>(&page))))
			{
				TStackRef<IQuadooTabControl> tabs;
				Check(parent->QueryInterface(__uuidof(IQuadooTabControl),reinterpret_cast<PVOID*>(&tabs)));
				Check(tabs->SetActivePage(page));
			}
			current = parent;
		}
		Check(Refresh(FALSE));
		{
			RECT rect, client; if(SUCCEEDED(ControlRect(selected,&rect)) && GetClientRect(m_canvas,&client))
			{
				if(rect.left < 8)
					m_scroll.X = max(0,m_scroll.X + rect.left - 8);
				else if(rect.right > client.right - 8)
					m_scroll.X += rect.right - client.right + 8;
				if(rect.top < 8)
					m_scroll.Y = max(0,m_scroll.Y + rect.top - 8);
				else if(rect.bottom > client.bottom - 8)
					m_scroll.Y += rect.bottom - client.bottom + 8;
				ArrangeSurface();
			}
		}
	Cleanup:
		if(entered)
			m_changeDepth--;
		else if(m_tree && ((FAILED(hr) && m_editing) || hr == S_FALSE))
		{
			m_changeDepth++;
			for(sysint n = 0; n < m_nodes.Length(); n++)
			{
				if(IsEqualGUID(m_nodes[n].id,m_selected))
				{
					TreeView_SelectItem(m_tree,m_nodes[n].tree);
					break;
				}
			}
			m_changeDepth--;
		}
		return hr;
	}

	VOID CDesignerEditor::SelectAncestor(BOOL root)
	{
		TStackRef<IQuadooControl> control;
		TStackRef<IQuadooControlContainer> parent;
		TStackRef<IQuadooComponent> component;
		TStackRef<IQuadooComponentOutline> outline;
		if(root && m_document.Root())
			Select(m_document.Root());
		else if(SUCCEEDED(Selected(&control)) && SUCCEEDED(control->GetParent(&parent)) && parent)
			Select(parent);
		else if(SUCCEEDED(SelectedComponent(&component)) && SUCCEEDED(component->QueryInterface(&outline)))
		{
			GUID id;
			if(SUCCEEDED(outline->GetOutlineParentID(&id)))
			{
				SelectID(id);
			}
		}
		else if(m_document.Root())
		{
			Select(m_document.Root());
		}
	}

	HRESULT CDesignerEditor::Delete()
	{
		HRESULT hr;
		TStackRef<IQuadooComponent> component;
		TStackRef<IQuadooControl> control;
		TStackRef<IQuadooControlContainer> parent;
		GUID parentID;

		Check(FinishInline(TRUE));
		Check(SelectedComponent(&component));
		{
			TStackRef<IQuadooStatusPanel> panel;
			if(SUCCEEDED(component->QueryInterface(&panel)))
			{
				Check(EditStatusPanels(DeleteSelection));
				goto Cleanup;
			}
		}
		hr = component->QueryInterface(__uuidof(IQuadooControl),reinterpret_cast<PVOID*>(&control));
		if(hr == E_NOINTERFACE)
		{
			Check(m_document.Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&parent)));
		}
		else
		{
			Check(hr);
			Check(control->GetParent(&parent));
		}
		CheckIf(!parent,E_ACCESSDENIED);
		Check(parent->GetID(&parentID));
		m_changeDepth++;
		if(SelectionCount() > 1)
		{
			hr = m_document.RemoveControls(m_selection.GetItemPtr(0),SelectionCount());
		}
		else
		{
			hr = m_document.Remove(component);
		}
		m_changeDepth--;
		if(SUCCEEDED(hr))
		{
			m_selection.Clear();
			m_selected = parentID;
		}
		Refresh(TRUE);

	Cleanup:
		return hr;
	}

	HRESULT CDesignerEditor::SetSelectedProperty(UINT index, IJSONValue* value)
	{
		HRESULT hr;
		CSelectionProperties properties;
		CheckIf(m_readOnly,E_ACCESSDENIED);
		Check(FinishInline(TRUE));
		Check(SelectionProperties(properties));
		m_changeDepth++;
		hr = properties.SetValue(index,value);
		m_changeDepth--;
		Refresh(TRUE);
	Cleanup:
		return hr;
	}

	HRESULT CDesignerEditor::AddControl(RSTRING type, IQuadooControlContainer* parent, const QuadooForm::Rect* bounds, GUID* id)
	{
		HRESULT hr;
		TStackRef<IQuadooControl> control;
		CheckIf(!id,E_POINTER);
		*id = GUID_NULL;
		CheckIf(m_readOnly,E_ACCESSDENIED);
		Check(FinishInline(TRUE));
		m_changeDepth++; hr = m_document.Add(parent,type,bounds,&control); m_changeDepth--;
		Check(hr);
		Check(control->GetID(id));
		Check(Refresh(TRUE));
		Check(Select(control));
	Cleanup:
		return hr;
	}

	HRESULT CDesignerEditor::AddComponent(RSTRING type, GUID* id)
	{
		HRESULT hr;
		TStackRef<IQuadooComponent> component;
		CheckIf(!id,E_POINTER);
		*id = GUID_NULL;
		CheckIf(m_readOnly,E_ACCESSDENIED);
		Check(FinishInline(TRUE));
		m_changeDepth++; hr = m_document.AddComponent(type,&component); m_changeDepth--;
		Check(hr); Check(component->GetID(id)); Check(Refresh(TRUE)); Check(SelectID(*id));
	Cleanup:
		return hr;
	}

	HRESULT CDesignerEditor::SetSelectionBounds(const QuadooForm::Rect* bounds)
	{
		HRESULT hr; TStackRef<IQuadooControl> control; QuadooForm::Rect previous; DWORD flags;
		CheckIf(!bounds,E_INVALIDARG);
		Check(FinishInline(TRUE));
		Check(Selected(&control));
		Check(control->GetBounds(&previous));
		Check(SelectionCapabilities(&flags));
		if(SelectionCount() > 1)
		{
			CheckIf(bounds->Width != previous.Width || bounds->Height != previous.Height,E_ACCESSDENIED);
			LONGLONG dx = static_cast<LONGLONG>(bounds->Left) - previous.Left;
			LONGLONG dy = static_cast<LONGLONG>(bounds->Top) - previous.Top;
			CheckIf(dx < INT_MIN || dx > INT_MAX || dy < INT_MIN || dy > INT_MAX,E_INVALIDARG);
			m_changeDepth++;
			hr = m_document.MoveControls(m_selection.GetItemPtr(0),SelectionCount(),static_cast<INT>(dx),static_cast<INT>(dy));
			m_changeDepth--;
			Refresh(TRUE);
			goto Cleanup;
		}
		CheckIf((bounds->Width != previous.Width && !(flags & QuadooForm::DesignResizeWidth)) || (bounds->Height != previous.Height && !(flags & QuadooForm::DesignResizeHeight)),E_ACCESSDENIED);
		CheckIf((bounds->Left != previous.Left && !(flags & QuadooForm::DesignMoveX)) || (bounds->Top != previous.Top && !(flags & QuadooForm::DesignMoveY)),E_ACCESSDENIED);
		CheckIf(bounds->Width < IntProperty(control,L"MinWidth") || bounds->Height < IntProperty(control,L"MinHeight") || (IntProperty(control,L"MaxWidth") && bounds->Width > IntProperty(control,L"MaxWidth")) || (IntProperty(control,L"MaxHeight") && bounds->Height > IntProperty(control,L"MaxHeight")),E_INVALIDARG);
		m_changeDepth++; hr = m_document.SetBounds(control,bounds); m_changeDepth--; Refresh(FAILED(hr));
	Cleanup:
		return hr;
	}

	HRESULT CDesignerEditor::NudgeSelection(UINT key)
	{
		HRESULT hr;
		TStackRef<IQuadooControl> control;
		QuadooForm::Rect bounds;
		DWORD flags;
		INT dx = key == VK_LEFT ? -1 : key == VK_RIGHT ? 1 : 0;
		INT dy = key == VK_UP ? -1 : key == VK_DOWN ? 1 : 0;
		CheckIfIgnore(m_readOnly || m_dragging || !m_document.Root() || IsEqualGUID(m_selected,GUID_NULL) || (!dx && !dy),S_FALSE);
		Check(FinishInline(TRUE));
		hr = Selected(&control);
		CheckIfIgnore(hr == E_NOINTERFACE || hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND),S_FALSE);
		Check(hr);
		Check(SelectionCapabilities(&flags));
		CheckIfIgnore((dx && !(flags & QuadooForm::DesignMoveX)) || (dy && !(flags & QuadooForm::DesignMoveY)),S_FALSE);
		Check(control->GetBounds(&bounds));
		CheckIf((dx < 0 && bounds.Left == INT_MIN) || (dx > 0 && bounds.Left == INT_MAX) ||
			(dy < 0 && bounds.Top == INT_MIN) || (dy > 0 && bounds.Top == INT_MAX),E_INVALIDARG);
		// Bounds are logical design pixels. Do not apply mouse grid snapping or
		// convert through the zoomed/DPI-scaled selection rectangle.
		bounds.Left += dx;
		bounds.Top += dy;
		// Only position changes. Layout may clamp the effective size to Min/Max
		// constraints while retaining different requested dimensions; leave those
		// dimensions untouched rather than applying resize validation to the move.
		m_changeDepth++;
		if(SelectionCount() > 1)
		{
			hr = m_document.MoveControls(m_selection.GetItemPtr(0),SelectionCount(),dx,dy);
		}
		else
		{
			hr = m_document.SetBounds(control,&bounds);
		}
		m_changeDepth--;
		Refresh(SelectionCount() > 1 || FAILED(hr));
	Cleanup:
		return hr;
	}

	HRESULT CDesignerEditor::ResizeSelection(UINT key)
	{
		HRESULT hr;
		INT width = key == VK_LEFT ? -1 : key == VK_RIGHT ? 1 : 0;
		INT height = key == VK_UP ? -1 : key == VK_DOWN ? 1 : 0;
		CheckIfIgnore(m_readOnly || m_dragging || !m_document.Root() || !SelectionCount() || (!width && !height),S_FALSE);
		Check(FinishInline(TRUE));
		// Change requested sizes in logical design pixels. Position, grid snap,
		// screen DPI and designer zoom do not change the keyboard increment.
		m_changeDepth++;
		hr = m_document.ResizeControls(m_selection.GetItemPtr(0),SelectionCount(),width,height);
		m_changeDepth--;
		if(hr != S_FALSE)
		{
			Refresh(SelectionCount() > 1 || FAILED(hr));
		}
	Cleanup:
		return hr;
	}

	HRESULT CDesignerEditor::PreviewForm(HWND owner, LRESULT* result)
	{
		HRESULT hr; TStackRef<IJSONObject> definition; TStackRef<IQuadooConstructionContext> context; TStackRef<IQuadooControl> root; TStackRef<IQuadooForm> form;
		TStackRef<CDesignerEditor> keepAlive(this);
		TStackRef<CDesignerReport> report;
		QuadooForm::ContextOptions options;
		CheckIf(!result,E_POINTER);
		*result = 0;
		Check(FinishInline(TRUE));
		CheckIf(!m_document.IsForm(),E_INVALIDARG);
		Check(m_document.Serialize(&definition));
		report.Attach(__new CDesignerReport); CheckAlloc(report);
		ZeroMemory(&options,sizeof(options)); options.cbSize = sizeof(options); options.Mode = QuadooForm::Preview; options.hwndHost = owner; options.pRegistry = m_document.Registry();
		options.pReport = report;
		Check(QuadooCreateConstructionContext(&options,&context));
		Check(QuadooCreateControl(context,NULL,definition,&root));
		Check(root->QueryInterface(__uuidof(IQuadooForm),reinterpret_cast<PVOID*>(&form)));
		Check(form->ShowModal(result));
	Cleanup:
		if(root)
			root->Destroy();
		if(FAILED(hr) && report && RStrLen(report->detail))
			m_status = report->detail;
		else
		{
			CRString status;
			if(SUCCEEDED(RStrFormatW(&status,L"Preview: result %d, HRESULT 0x%.8x",result ? static_cast<INT>(*result) : 0,hr)))
				m_status = status;
		}
		Notify();
		return hr;
	}

	VOID CDesignerEditor::Error(HRESULT hr, PCWSTR operation)
	{
		CRString text;
		if(SUCCEEDED(RStrFormatW(&text,L"%ls failed (0x%.8x). Check the property's format, bounds and factory requirements.",operation,hr)))
		{
			m_status = text;
			Notify();
			MessageBoxW(m_window,text.c_wstr(),L"QuadooForm Designer",MB_OK | MB_ICONERROR);
		}
	}

	VOID CDesignerEditor::SetZoom(UINT percent)
	{
		if(percent < 50 || percent > 200)
			return;
		m_zoom = percent;
		ArrangeSurface();
		Notify();
	}

	VOID CDesignerEditor::SetDPI(UINT dpi)
	{
		if(dpi < 48 || dpi > 384 || dpi == m_dpi) return;
		HRESULT hr = RebuildPaletteImages(dpi);
		if(SUCCEEDED(hr))
		{
			m_dpi = dpi;
			Layout();
		}
		else
		{
			CRString status;
			if(SUCCEEDED(RStrFormatW(&status,L"Unable to resize palette icons (0x%.8x).",hr))) m_status = status;
		}
		Notify();
	}

	VOID CDesignerEditor::SetLayout(INT left, INT right, INT tree)
	{
		m_leftWidth = max(120,min(500,left));
		m_rightWidth = max(220,min(600,right));
		m_treeHeight = max(100,min(700,tree));
		Layout();
	}

	VOID CDesignerEditor::ResetPanes()
	{
		SetLayout(190,300,285);
	}
}
