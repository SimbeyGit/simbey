#include "Designer.h"

namespace QFD
{
class CMenuEditorDialog
{
	CDesignerDocument* m_original;
	CDesignerDocument m_work;
	TArray<GUID> m_nodes;
	HWND m_window;
	HWND m_tree;
	HWND m_workspace;
	BOOL m_loading;
	HRESULT m_error;
	HWND Child(PCWSTR type, PCWSTR text, DWORD style, INT id, INT x, INT y, INT width, INT height)
	{
		RECT rect = {x,y,x + width,y + height};
		MapDialogRect(m_window,&rect);
		HWND child = CreateWindowExW(0,type,text,WS_CHILD | WS_VISIBLE | style,rect.left,rect.top,rect.right - rect.left,rect.bottom - rect.top,m_window,reinterpret_cast<HMENU>(id),GetModuleHandle(NULL),NULL);
		if(child)
		{
			SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),FALSE);
		}
		return child;
	}
	HRESULT Text(INT id, RSTRING* text)
	{
		HRESULT hr;
		PWSTR write;
		INT length = GetWindowTextLengthW(GetDlgItem(m_window,id));
		CheckIf(length > 1024,E_INVALIDARG);
		Check(RStrAllocW(length,text,&write));
		GetDlgItemTextW(m_window,id,write,length + 1);
	Cleanup:
		return hr;
	}
	HRESULT SetTextProperty(IQuadooComponent* component, INT field, PCWSTR property)
	{
		HRESULT hr;
		CRString text;
		TStackRef<IJSONValue> value;
		TStackRef<IQuadooProperties> properties;
		UINT index;
		Check(Text(field,&text));
		Check(Property(component,property,&properties,&index));
		Check(JSONCreateString(text,&value));
		Check(m_work.SetProperty(component,index,value));
	Cleanup:
		return hr;
	}
	HRESULT SaveFields()
	{
		HRESULT hr = S_OK;
		TStackRef<IQuadooComponent> component;
		TStackRef<IQuadooMenuItem> item;
		CheckIfIgnore(m_loading || IsEqualGUID(selected,GUID_NULL),S_OK);
		Check(m_work.FindComponent(selected,&component));
		Check(SetTextProperty(component,611,L"Name"));
		if(SUCCEEDED(component->QueryInterface(__uuidof(IQuadooMenuItem),reinterpret_cast<PVOID*>(&item))))
		{
			Check(SetTextProperty(component,612,L"Caption"));
			Check(SetTextProperty(component,613,L"ShortCut"));
			Check(SetTextProperty(component,614,L"OnClick"));
			static PCWSTR const names[] = {L"Enabled",L"Checked",L"Visible",L"Separator",L"AutoCheck"};
			for(UINT n = 0; n < ARRAYSIZE(names); n++)
			{
				TStackRef<IQuadooProperties> properties;
				TStackRef<IJSONValue> value;
				UINT index;
				Check(Property(component,names[n],&properties,&index));
				Check(JSONCreateBool(IsDlgButtonChecked(m_window,615 + n) == BST_CHECKED,&value));
				Check(m_work.SetProperty(component,index,value));
			}
		}
	Cleanup:
		return hr;
	}
	HRESULT LoadFields()
	{
		HRESULT hr;
		TStackRef<IQuadooComponent> component;
		TStackRef<IQuadooMenuItem> item;
		CRString name;
		Check(m_work.FindComponent(selected,&component));
		Check(component->GetName(&name));
		SetDlgItemTextW(m_window,611,name.c_wstr());
		BOOL menuItem = SUCCEEDED(component->QueryInterface(__uuidof(IQuadooMenuItem),reinterpret_cast<PVOID*>(&item)));
		static PCWSTR const names[] = {L"Caption",L"ShortCut",L"OnClick"};
		for(UINT n = 0; n < ARRAYSIZE(names); n++)
		{
			CRString value;
			if(menuItem)
			{
				Check(StringProperty(component,names[n],&value));
			}
			SetDlgItemTextW(m_window,612 + n,value ? value.c_wstr() : L"");
			EnableWindow(GetDlgItem(m_window,612 + n),menuItem);
		}
		static PCWSTR const flags[] = {L"Enabled",L"Checked",L"Visible",L"Separator",L"AutoCheck"};
		for(UINT n = 0; n < ARRAYSIZE(flags); n++)
		{
			CheckDlgButton(m_window,615 + n,menuItem && BoolProperty(component,flags[n]) ? BST_CHECKED : BST_UNCHECKED);
			EnableWindow(GetDlgItem(m_window,615 + n),menuItem);
		}
		EnableWindow(GetDlgItem(m_window,620),menuItem);
		EnableWindow(GetDlgItem(m_window,623),menuItem);
		EnableWindow(GetDlgItem(m_window,624),menuItem);
	Cleanup:
		return hr;
	}
	HRESULT Attach(IQuadooComponent* component, HTREEITEM parent, UINT depth)
	{
		HRESULT hr;
		TStackRef<IQuadooMenuItems> children;
		CRString name;
		CRString caption;
		CRString label;
		GUID id;
		TVINSERTSTRUCTW insert = {0};
		CheckIf(depth >= 128,E_INVALIDARG);
		Check(component->GetName(&name));
		Check(component->GetID(&id));
		if(depth)
		{
			Check(StringProperty(component,L"Caption",&caption));
			Check(RStrFormatW(&label,L"%r (%r)%ls",static_cast<RSTRING>(caption),static_cast<RSTRING>(name),BoolProperty(component,L"Enabled",TRUE) ? L"" : L" [disabled]"));
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
		Check(m_nodes.Append(id));
		HTREEITEM node = TreeView_InsertItem(m_tree,&insert);
		CheckIf(!node,E_OUTOFMEMORY);
		if(IsEqualGUID(id,selected))
		{
			TreeView_SelectItem(m_tree,node);
		}
		Check(component->QueryInterface(__uuidof(IQuadooMenuItems),reinterpret_cast<PVOID*>(&children)));
		UINT count;
		Check(children->GetItemCount(&count));
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IQuadooComponent> item;
			Check(children->GetItem(n,&item));
			Check(Attach(item,node,depth + 1));
		}
		TreeView_Expand(m_tree,node,TVE_EXPAND);
	Cleanup:
		return hr;
	}
	HRESULT Rebuild()
	{
		HRESULT hr;
		TStackRef<IQuadooComponentHost> host;
		TStackRef<IQuadooComponentCollection> components;
		UINT count;
		m_loading = TRUE;
		TreeView_DeleteAllItems(m_tree);
		m_nodes.Clear();
		Check(m_work.Root()->QueryInterface(__uuidof(IQuadooComponentHost),reinterpret_cast<PVOID*>(&host)));
		Check(host->GetComponents(&components));
		Check(components->GetCount(&count));
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IQuadooComponent> component;
			TStackRef<IQuadooMenu> menu;
			Check(components->GetAt(n,&component));
			if(SUCCEEDED(component->QueryInterface(__uuidof(IQuadooMenu),reinterpret_cast<PVOID*>(&menu))))
			{
				Check(Attach(component,TVI_ROOT,0));
			}
		}
		if(!TreeView_GetSelection(m_tree))
		{
			HTREEITEM first = TreeView_GetRoot(m_tree);
			if(first)
			{
				TreeView_SelectItem(m_tree,first);
				selected = m_nodes[0];
			}
			else
			{
				selected = GUID_NULL;
			}
		}
		if(!IsEqualGUID(selected,GUID_NULL))
		{
			Check(LoadFields());
		}
	Cleanup:
		m_loading = FALSE;
		return hr;
	}
	HRESULT Initialize()
	{
		HRESULT hr;
		TStackRef<IJSONObject> definition;
		SetWindowTextW(m_window,L"Menu Editor");
		CheckIf(!Child(L"STATIC",L"Menus and menu items",SS_LEFT,640,8,6,180,12),HrEnsureLastError());
		m_tree = Child(WC_TREEVIEWW,L"",WS_BORDER | WS_TABSTOP | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS,610,8,20,188,218);
		CheckIf(!m_tree,HrEnsureLastError());
		static PCWSTR const labels[] = {L"Name:",L"Caption:",L"ShortCut:",L"OnClick:"};
		for(UINT n = 0; n < ARRAYSIZE(labels); n++)
		{
			CheckIf(!Child(L"STATIC",labels[n],SS_LEFT,650 + n,207,21 + n * 24,52,12),HrEnsureLastError());
			HWND field = Child(L"EDIT",L"",WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,611 + n,262,18 + n * 24,186,14);
			CheckIf(!field,HrEnsureLastError());
			SendMessageW(field,EM_SETLIMITTEXT,n ? 1024 : 256,0);
		}
		static PCWSTR const flags[] = {L"Enabled",L"Checked",L"Visible",L"Separator",L"AutoCheck"};
		for(UINT n = 0; n < ARRAYSIZE(flags); n++)
		{
			CheckIf(!Child(L"BUTTON",flags[n],WS_TABSTOP | BS_AUTOCHECKBOX,615 + n,208 + (n % 2) * 114,119 + (n / 2) * 18,108,14),HrEnsureLastError());
		}
		CheckIf(!Child(L"BUTTON",L"Add Sibling",WS_TABSTOP,620,8,241,60,16),HrEnsureLastError());
		CheckIf(!Child(L"BUTTON",L"Add Child",WS_TABSTOP,621,72,241,60,16),HrEnsureLastError());
		CheckIf(!Child(L"BUTTON",L"Delete",WS_TABSTOP,622,136,241,60,16),HrEnsureLastError());
		CheckIf(!Child(L"BUTTON",L"Up",WS_TABSTOP,623,208,184,54,16),HrEnsureLastError());
		CheckIf(!Child(L"BUTTON",L"Down",WS_TABSTOP,624,267,184,54,16),HrEnsureLastError());
		CheckIf(!Child(L"BUTTON",L"Apply Item",WS_TABSTOP,625,328,184,80,16),HrEnsureLastError());
		CheckIf(!Child(L"STATIC",L"Use & in captions for access keys.\r\nShortcuts: Ctrl+O, Alt+F4, Shift+F5.\r\nOnClick names bind through the form/frame interface.",SS_LEFT,660,208,208,240,42),HrEnsureLastError());
		CheckIf(!Child(L"BUTTON",L"OK",BS_DEFPUSHBUTTON | WS_TABSTOP,IDOK,174,268,54,16),HrEnsureLastError());
		CheckIf(!Child(L"BUTTON",L"Cancel",WS_TABSTOP,IDCANCEL,234,268,54,16),HrEnsureLastError());
		m_workspace = CreateWindowExW(0,L"STATIC",L"",WS_CHILD,0,0,1,1,m_window,NULL,GetModuleHandle(NULL),NULL);
		CheckIf(!m_workspace,HrEnsureLastError());
		Check(m_original->Serialize(&definition));
		Check(m_work.Initialize(m_original->Registry()));
		Check(m_work.Load(m_workspace,definition));
		Check(Rebuild());
	Cleanup:
		return hr;
	}
	HRESULT Action(UINT command)
	{
		HRESULT hr;
		TStackRef<IQuadooComponent> component;
		Check(SaveFields());
		if(command == IDOK)
		{
			Check(m_work.Serialize(&result));
			EndDialog(m_window,IDOK);
			goto Cleanup;
		}
		Check(m_work.FindComponent(selected,&component));
		if(command == 620 || command == 621)
		{
			TStackRef<IQuadooComponent> parent;
			TStackRef<IQuadooMenuItem> item;
			TStackRef<IQuadooComponent> added;
			parent = component;
			if(command == 620 && SUCCEEDED(component->QueryInterface(__uuidof(IQuadooMenuItem),reinterpret_cast<PVOID*>(&item))))
			{
				GUID id;
				Check(item->GetMenuParentID(&id));
				parent.Release();
				Check(m_work.FindComponent(id,&parent));
			}
			Check(m_work.AddMenuItem(parent,&added));
			Check(added->GetID(&selected));
		}
		else if(command == 622)
		{
			Check(m_work.RemoveMenu(component));
			selected = GUID_NULL;
		}
		else if(command == 623 || command == 624)
		{
			Check(m_work.MoveMenuItem(component,command == 624));
		}
		Check(Rebuild());
	Cleanup:
		return hr;
	}
	VOID Error(HRESULT error)
	{
		CRString text;
		if(SUCCEEDED(RStrFormatW(&text,L"The menu edit could not be applied (0x%.8x). Check the name, shortcut, separator children and event handler.",error)))
		{
			MessageBoxW(m_window,text.c_wstr(),L"Menu Editor",MB_OK | MB_ICONERROR);
		}
	}
	static INT_PTR CALLBACK Procedure(HWND window, UINT message, WPARAM wp, LPARAM lp)
	{
		CMenuEditorDialog* dialog = reinterpret_cast<CMenuEditorDialog*>(GetWindowLongPtrW(window,DWLP_USER));
		if(message == WM_INITDIALOG)
		{
			dialog = reinterpret_cast<CMenuEditorDialog*>(lp);
			dialog->m_window = window;
			SetWindowLongPtrW(window,DWLP_USER,lp);
			dialog->m_error = dialog->Initialize();
			if(FAILED(dialog->m_error))
			{
				EndDialog(window,IDCANCEL);
			}
			return TRUE;
		}
		if(!dialog)
		{
			return FALSE;
		}
		if(message == WM_CLOSE || (message == WM_COMMAND && LOWORD(wp) == IDCANCEL))
		{
			EndDialog(window,IDCANCEL);
			return TRUE;
		}
		if(message == WM_COMMAND && HIWORD(wp) == BN_CLICKED)
		{
			UINT command = LOWORD(wp);
			if(command == IDOK || (command >= 620 && command <= 625))
			{
				HRESULT hr = dialog->Action(command);
				if(FAILED(hr))
				{
					dialog->Error(hr);
				}
				return TRUE;
			}
		}
		if(message == WM_NOTIFY && !dialog->m_loading)
		{
			NMHDR* notification = reinterpret_cast<NMHDR*>(lp);
			if(notification->hwndFrom == dialog->m_tree && notification->code == TVN_SELCHANGINGW)
			{
				HRESULT hr = dialog->SaveFields();
				if(FAILED(hr))
				{
					dialog->Error(hr);
					SetWindowLongPtrW(window,DWLP_MSGRESULT,TRUE);
					return TRUE;
				}
			}
			if(notification->hwndFrom == dialog->m_tree && notification->code == TVN_SELCHANGEDW)
			{
				NMTREEVIEWW* change = reinterpret_cast<NMTREEVIEWW*>(lp);
				if(change->itemNew.lParam >= 0 && change->itemNew.lParam < dialog->m_nodes.Length())
				{
					dialog->selected = dialog->m_nodes[change->itemNew.lParam];
					HRESULT hr = dialog->LoadFields();
					if(FAILED(hr))
					{
						dialog->Error(hr);
					}
				}
			}
		}
		return FALSE;
	}
public:
	GUID selected;
	TStackRef<IJSONObject> result;
	CMenuEditorDialog(CDesignerDocument* document, REFGUID selection) : m_original(document), m_window(NULL), m_tree(NULL), m_workspace(NULL), m_loading(FALSE), m_error(S_OK), selected(selection)
	{
	}
	HRESULT Show(HWND owner)
	{
		HRESULT hr;
		#pragma pack(push,2)
		struct EmptyDialog
		{
			DLGTEMPLATE dialog;
			WORD menu;
			WORD windowClass;
			WORD title;
		};
		#pragma pack(pop)
		EmptyDialog definition = {0};
		definition.dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_CENTER;
		definition.dialog.cx = 460;
		definition.dialog.cy = 292;
		INT_PTR answer = DialogBoxIndirectParamW(GetModuleHandle(NULL),&definition.dialog,owner,Procedure,reinterpret_cast<LPARAM>(this));
		hr = answer == -1 ? HrEnsureLastError() : FAILED(m_error) ? m_error : answer == IDOK ? S_OK : S_FALSE;
		m_work.Clear();
		return hr;
	}
};
HRESULT CDesignerEditor::EditMenu()
{
	HRESULT hr;
	Check(FinishInline(TRUE));
	{
		CMenuEditorDialog dialog(&m_document,m_selected);
		hr = dialog.Show(m_window);
		if(hr == S_OK)
		{
			m_changeDepth++;
			hr = m_document.ApplyMenuDefinition(dialog.result);
			m_changeDepth--;
			Check(hr);
			m_selection.Clear();
			m_selected = dialog.selected;
			Check(Refresh(TRUE));
		}
	}
Cleanup:
	return hr;
}
}
