#include "TreeNodesEditor.h"

namespace QFD
{
	CTreeEditSession::CTreeEditSession() : m_editor(NULL), m_id(GUID_NULL), m_workspace(NULL)
	{
	}
	CTreeEditSession::~CTreeEditSession()
	{
		m_work.Clear();
		if(m_workspace)
		{
			DestroyWindow(m_workspace);
		}
	}
	HRESULT CTreeEditSession::Initialize(CDesignerEditor* editor, REFGUID id)
	{
		HRESULT hr;
		TStackRef<IJSONObject> definition;
		CheckIf(!editor || m_editor,E_INVALIDARG);
		m_workspace = CreateWindowExW(0,L"STATIC",L"",WS_CHILD,0,0,1,1,editor->Window(),NULL,GetModuleHandle(NULL),NULL);
		CheckIf(!m_workspace,HrEnsureLastError());
		Check(editor->Document().Serialize(&definition));
		Check(m_work.Initialize(editor->Document().Registry()));
		Check(m_work.Load(m_workspace,definition));
		Check(m_work.FindComponent(id,&m_component));
		Check(m_component->QueryInterface(__uuidof(IQuadooTreeView),reinterpret_cast<PVOID*>(&tree)));
		Check(tree->GetNodes(&nodes));
		m_editor = editor;
		m_id = id;
		Check(Properties(&m_baseline));
	Cleanup:
		return hr;
	}
	HRESULT CTreeEditSession::Properties(IJSONObject** properties)
	{
		HRESULT hr;
		TStackRef<IJSONObject> result;
		TStackRef<IJSONValue> items;
		TStackRef<IJSONValue> selected;
		CheckIf(!properties || !m_component,E_INVALIDARG);
		*properties = NULL;
		Check(JSONCreateObject(&result));
		Check(ReadProperty(m_component,L"Items",&items));
		Check(ReadProperty(m_component,L"SelectedNode",&selected));
		Check(result->AddValueW(L"Items",items));
		Check(result->AddValueW(L"SelectedNode",selected));
		*properties = result.Detach();
	Cleanup:
		return hr;
	}
	HRESULT CTreeEditSession::Apply()
	{
		HRESULT hr;
		TStackRef<IJSONObject> properties;
		Check(Properties(&properties));
		CheckIfIgnore(JSONCompareObjects(properties,m_baseline) == 0,S_FALSE);
		Check(m_editor->ApplyComponentProperties(m_id,properties));
		m_baseline = properties;
	Cleanup:
		return hr;
	}

	class CTreeNodesDialog
	{
		CDesignerEditor* m_editor;
		GUID m_id;
		CTreeEditSession m_session;
		TStackRef<IQuadooTreeNode> m_selected;
		TStackRef<IQuadooImageListSnapshot> m_images;
		TArray<IQuadooTreeNode*> m_rows;
		HWND m_window;
		HWND m_tree;
		BOOL m_loading;
		BOOL m_committed;
		HRESULT m_error;
		HWND Child(PCWSTR type, PCWSTR text, DWORD style, INT id, INT x, INT y, INT width, INT height)
		{
			RECT rect = {x,y,x + width,y + height};
			MapDialogRect(m_window,&rect);
			HWND child = CreateWindowExW(!lstrcmpW(type,L"EDIT") ? WS_EX_CLIENTEDGE : 0,type,text,
				WS_CHILD | WS_VISIBLE | style,rect.left,rect.top,rect.right - rect.left,rect.bottom - rect.top,
				m_window,reinterpret_cast<HMENU>(id),GetModuleHandle(NULL),NULL);
			if(child)
			{
				SendMessageW(child,WM_SETFONT,SendMessageW(m_window,WM_GETFONT,0,0),FALSE);
			}
			return child;
		}
		VOID ClearRows()
		{
			for(sysint n = 0; n < m_rows.Length(); n++)
			{
				m_rows[n]->Release();
			}
			m_rows.Clear();
		}
		VOID Error(HRESULT hr)
		{
			CRString text;
			if(SUCCEEDED(RStrFormatW(&text,L"Tree edit failed (0x%.8x). Image indexes must be -1 or greater. The document was not changed.",hr)))
			{
				MessageBoxW(m_window,text.c_wstr(),L"Tree Nodes Editor",MB_OK | MB_ICONERROR);
			}
		}
		HRESULT LoadFields()
		{
			HRESULT hr = S_OK;
			BOOL attached = FALSE;
			if(m_selected)
			{
				Check(m_selected->GetAttached(&attached));
			}
			for(UINT id = 811; id <= 815; id++)
			{
				EnableWindow(GetDlgItem(m_window,id),attached);
			}
			EnableWindow(GetDlgItem(m_window,822),attached);
			EnableWindow(GetDlgItem(m_window,823),attached);
			EnableWindow(GetDlgItem(m_window,828),attached);
			if(attached)
			{
				CRString text;
				INT image;
				INT selected;
				BOOL flag;
				UINT index;
				UINT count;
				TStackRef<IQuadooTreeNode> parent;
				Check(m_selected->GetText(&text));
				Check(m_selected->GetImageIndex(&image));
				Check(m_selected->GetSelectedImageIndex(&selected));
				SetDlgItemTextW(m_window,811,text.c_wstr());
				SetDlgItemInt(m_window,812,image,TRUE);
				SetDlgItemInt(m_window,813,selected,TRUE);
				Check(m_selected->GetExpanded(&flag));
				CheckDlgButton(m_window,814,flag ? BST_CHECKED : BST_UNCHECKED);
				Check(m_selected->GetChecked(&flag));
				CheckDlgButton(m_window,815,flag ? BST_CHECKED : BST_UNCHECKED);
				Check(m_selected->GetIndex(&index));
				Check(m_selected->GetParent(&parent));
				Check(parent ? parent->GetChildCount(&count) : m_session.nodes->GetCount(&count));
				EnableWindow(GetDlgItem(m_window,824),index > 0);
				EnableWindow(GetDlgItem(m_window,825),index + 1 < count);
				EnableWindow(GetDlgItem(m_window,826),index > 0);
				EnableWindow(GetDlgItem(m_window,827),parent != NULL);
			}
			else
			{
				SetDlgItemTextW(m_window,811,L"");
				for(UINT id = 824; id <= 827; id++)
				{
					EnableWindow(GetDlgItem(m_window,id),FALSE);
				}
			}
		Cleanup:
			return hr;
		}
		HRESULT SaveFields()
		{
			HRESULT hr = S_OK;
			BOOL validImage;
			BOOL validSelected;
			BOOL attached;
			INT image = GetDlgItemInt(m_window,812,&validImage,TRUE);
			INT selected = GetDlgItemInt(m_window,813,&validSelected,TRUE);
			CRString text;
			PWSTR write;
			CheckIfIgnore(m_loading || !m_selected,S_OK);
			Check(m_selected->GetAttached(&attached));
			CheckIfIgnore(!attached,S_OK);
			CheckIf(!validImage || !validSelected || image < -1 || selected < -1,E_INVALIDARG);
			INT length = GetWindowTextLengthW(GetDlgItem(m_window,811));
			CheckIf(length > 32767,E_INVALIDARG);
			Check(RStrAllocW(length,&text,&write));
			GetDlgItemTextW(m_window,811,write,length + 1);
			Check(m_selected->SetText(text));
			Check(m_selected->SetImageIndex(image));
			Check(m_selected->SetSelectedImageIndex(selected));
			Check(m_selected->SetExpanded(IsDlgButtonChecked(m_window,814) == BST_CHECKED));
			Check(m_selected->SetChecked(IsDlgButtonChecked(m_window,815) == BST_CHECKED));
			HTREEITEM item = TreeView_GetSelection(m_tree);
			if(item)
			{
				TVITEMW update = {0};
				update.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
				update.hItem = item;
				update.pszText = write;
				update.iImage = image < 0 || image >= 0xfffe ? I_IMAGENONE : image;
				update.iSelectedImage = selected < 0 ? update.iImage : selected >= 0xfffe ? I_IMAGENONE : selected;
				SendMessageW(m_tree,TVM_SETITEMW,0,reinterpret_cast<LPARAM>(&update));
			}
		Cleanup:
			return hr;
		}
		HRESULT AddRows(IQuadooTreeNode* parentNode, HTREEITEM parentItem, REFGUID selection, HTREEITEM* selectedItem)
		{
			HRESULT hr;
			UINT count;
			Check(parentNode ? parentNode->GetChildCount(&count) : m_session.nodes->GetCount(&count));
			for(UINT n = 0; n < count; n++)
			{
				TStackRef<IQuadooTreeNode> node;
				CRString text;
				GUID id;
				INT image;
				INT selectedImage;
				BOOL expanded;
				Check(parentNode ? parentNode->GetChild(n,&node) : m_session.nodes->GetAt(n,&node));
				Check(node->GetText(&text));
				Check(node->GetID(&id));
				Check(node->GetImageIndex(&image));
				Check(node->GetSelectedImageIndex(&selectedImage));
				Check(node->GetExpanded(&expanded));
				TVINSERTSTRUCTW insert = {0};
				insert.hParent = parentItem;
				insert.hInsertAfter = TVI_LAST;
				insert.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
				insert.item.pszText = const_cast<PWSTR>(text.c_wstr());
				insert.item.lParam = m_rows.Length();
				insert.item.iImage = image < 0 || image >= 0xfffe ? I_IMAGENONE : image;
				insert.item.iSelectedImage = selectedImage < 0 ? insert.item.iImage : selectedImage >= 0xfffe ? I_IMAGENONE : selectedImage;
				Check(m_rows.Append(node));
				node->AddRef();
				HTREEITEM item = reinterpret_cast<HTREEITEM>(SendMessageW(m_tree,TVM_INSERTITEMW,0,reinterpret_cast<LPARAM>(&insert)));
				CheckIf(!item,E_OUTOFMEMORY);
				if(IsEqualGUID(id,selection))
				{
					*selectedItem = item;
				}
				Check(AddRows(node,item,selection,selectedItem));
				if(expanded)
				{
					TreeView_Expand(m_tree,item,TVE_EXPAND);
				}
			}
		Cleanup:
			return hr;
		}
		HRESULT Rebuild()
		{
			HRESULT hr;
			GUID id = GUID_NULL;
			HTREEITEM item = NULL;
			if(m_selected)
			{
				Check(m_selected->GetID(&id));
			}
			m_loading = TRUE;
			TreeView_DeleteAllItems(m_tree);
			ClearRows();
			Check(AddRows(NULL,TVI_ROOT,id,&item));
			if(!item)
			{
				item = TreeView_GetRoot(m_tree);
				m_selected.Release();
				if(m_rows.Length())
				{
					m_selected = m_rows[0];
				}
			}
			TreeView_SelectItem(m_tree,item);
			Check(LoadFields());
		Cleanup:
			m_loading = FALSE;
			return hr;
		}
		HRESULT Action(UINT command)
		{
			HRESULT hr;
			TStackRef<IQuadooTreeNode> parent;
			TStackRef<IQuadooTreeNode> other;
			TStackRef<IQuadooTreeNode> grandparent;
			UINT index;
			UINT count;
			Check(SaveFields());
			if(command == IDOK || command == 0x3021)
			{
				Check(m_session.Apply());
				m_committed = TRUE;
				if(command == IDOK)
				{
					EndDialog(m_window,IDOK);
				}
				goto Cleanup;
			}
			if(command == 821 || command == 822)
			{
				if(command == 822)
				{
					parent = m_selected;
				}
				else if(m_selected)
				{
					Check(m_selected->GetParent(&parent));
				}
				Check(parent ? parent->GetChildCount(&count) : m_session.nodes->GetCount(&count));
				Check(m_session.nodes->Insert(parent,count,RSTRING_CAST(L"New node"),&other));
				if(parent)
				{
					Check(parent->SetExpanded(TRUE));
				}
				m_selected = other;
			}
			else if(command == 823)
			{
				Check(m_session.nodes->Delete(m_selected));
				m_selected.Release();
			}
			else if(command == 829)
			{
				Check(m_session.nodes->Clear());
				m_selected.Release();
			}
			else if(command >= 824 && command <= 827 && m_selected)
			{
				Check(m_selected->GetIndex(&index));
				Check(m_selected->GetParent(&parent));
				if(command == 824 || command == 825)
				{
					Check(m_session.nodes->Move(m_selected,parent,command == 824 ? index - 1 : index + 1));
				}
				else if(command == 826)
				{
					CheckIf(!index,E_INVALIDARG);
					Check(parent ? parent->GetChild(index - 1,&other) : m_session.nodes->GetAt(index - 1,&other));
					Check(other->GetChildCount(&count));
					Check(m_session.nodes->Move(m_selected,other,count));
					Check(other->SetExpanded(TRUE));
				}
				else
				{
					CheckIf(!parent,E_INVALIDARG);
					Check(parent->GetParent(&grandparent));
					Check(parent->GetIndex(&index));
					Check(m_session.nodes->Move(m_selected,grandparent,index + 1));
				}
			}
			Check(Rebuild());
		Cleanup:
			return hr;
		}
		HRESULT Initialize()
		{
			HRESULT hr;
			SetWindowTextW(m_window,L"Tree Nodes Editor");
			m_tree = Child(WC_TREEVIEWW,L"",WS_TABSTOP | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS,810,8,8,224,240);
			CheckIf(!m_tree,HrEnsureLastError());
			TreeView_SetUnicodeFormat(m_tree,TRUE);
			CheckIf(!Child(L"STATIC",L"Text",0,0,242,10,180,10),HrEnsureLastError());
			CheckIf(!Child(L"EDIT",L"",WS_TABSTOP | ES_AUTOHSCROLL,811,242,23,190,14),HrEnsureLastError());
			CheckIf(!Child(L"STATIC",L"Image index (-1 = none)",0,0,242,44,180,10),HrEnsureLastError());
			CheckIf(!Child(L"EDIT",L"-1",WS_TABSTOP | ES_AUTOHSCROLL,812,242,57,70,14),HrEnsureLastError());
			CheckIf(!Child(L"STATIC",L"Selected image (-1 = normal)",0,0,242,78,190,10),HrEnsureLastError());
			CheckIf(!Child(L"EDIT",L"-1",WS_TABSTOP | ES_AUTOHSCROLL,813,242,91,70,14),HrEnsureLastError());
			CheckIf(!Child(L"BUTTON",L"Expanded",WS_TABSTOP | BS_AUTOCHECKBOX,814,242,115,90,14),HrEnsureLastError());
			CheckIf(!Child(L"BUTTON",L"Checked",WS_TABSTOP | BS_AUTOCHECKBOX,815,342,115,90,14),HrEnsureLastError());
			static PCWSTR const actions[] = {L"Add Sibling",L"Add Child",L"Delete",L"Up",L"Down",L"Indent",L"Outdent",L"Apply Node",L"Clear All"};
			for(UINT n = 0; n < ARRAYSIZE(actions); n++)
			{
				CheckIf(!Child(L"BUTTON",actions[n],WS_TABSTOP,821 + n,242 + (n % 3) * 65,144 + (n / 3) * 24,60,18),HrEnsureLastError());
			}
			CheckIf(!Child(L"STATIC",L"Image indexes refer to the TreeView's assigned ImageList.\r\nChanges are committed together on Apply or OK.",0,0,242,220,192,28),HrEnsureLastError());
			CheckIf(!Child(L"BUTTON",L"OK",WS_TABSTOP | BS_DEFPUSHBUTTON,IDOK,242,260,58,18),HrEnsureLastError());
			CheckIf(!Child(L"BUTTON",L"Cancel",WS_TABSTOP,IDCANCEL,308,260,58,18),HrEnsureLastError());
			CheckIf(!Child(L"BUTTON",L"Apply",WS_TABSTOP,0x3021,374,260,58,18),HrEnsureLastError());
			Check(m_session.Initialize(m_editor,m_id));
			{
				TStackRef<IQuadooComponent> component;
				TStackRef<IQuadooImageList> list;
				Check(m_session.tree->GetImages(&component));
				if(component)
				{
					HIMAGELIST handle;
					HDC dc = GetDC(m_window);
					UINT dpi = dc ? GetDeviceCaps(dc,LOGPIXELSX) : 96;
					if(dc)
					{
						ReleaseDC(m_window,dc);
					}
					Check(component->QueryInterface(__uuidof(IQuadooImageList),reinterpret_cast<PVOID*>(&list)));
					Check(list->Acquire(dpi,&m_images));
					Check(m_images->GetHandle(&handle));
					TreeView_SetImageList(m_tree,handle,TVSIL_NORMAL);
				}
			}
			Check(Rebuild());
		Cleanup:
			return hr;
		}
		static INT_PTR CALLBACK Procedure(HWND window, UINT message, WPARAM wp, LPARAM lp)
		{
			CTreeNodesDialog* dialog = reinterpret_cast<CTreeNodesDialog*>(GetWindowLongPtrW(window,DWLP_USER));
			if(message == WM_INITDIALOG)
			{
				dialog = reinterpret_cast<CTreeNodesDialog*>(lp);
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
			if(message == WM_COMMAND && HIWORD(wp) == BN_CLICKED &&
				(LOWORD(wp) == IDOK || LOWORD(wp) == 0x3021 || (LOWORD(wp) >= 821 && LOWORD(wp) <= 829)))
			{
				HRESULT hr = dialog->Action(LOWORD(wp));
				if(FAILED(hr))
				{
					dialog->Error(hr);
				}
				return TRUE;
			}
			if(message == WM_NOTIFY && !dialog->m_loading && reinterpret_cast<NMHDR*>(lp)->hwndFrom == dialog->m_tree)
			{
				NMTREEVIEWW* change = reinterpret_cast<NMTREEVIEWW*>(lp);
				if(change->hdr.code == TVN_SELCHANGINGW)
				{
					HRESULT hr = dialog->SaveFields();
					if(FAILED(hr))
					{
						dialog->Error(hr);
						SetWindowLongPtrW(window,DWLP_MSGRESULT,TRUE);
						return TRUE;
					}
				}
				else if(change->hdr.code == TVN_SELCHANGEDW)
				{
					if(change->itemNew.lParam >= 0 && change->itemNew.lParam < dialog->m_rows.Length())
					{
						dialog->m_selected = dialog->m_rows[change->itemNew.lParam];
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
		CTreeNodesDialog(CDesignerEditor* editor, REFGUID id) : m_editor(editor), m_id(id), m_window(NULL),
			m_tree(NULL), m_loading(FALSE), m_committed(FALSE), m_error(S_OK)
		{
		}
		~CTreeNodesDialog()
		{
			ClearRows();
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
				WORD pointSize;
				WCHAR font[14];
			};
			#pragma pack(pop)
			EmptyDialog definition = {0};
			definition.dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_CENTER | DS_SETFONT;
			definition.dialog.cx = 444;
			definition.dialog.cy = 290;
			definition.pointSize = 8;
			CopyMemory(definition.font,L"MS Shell Dlg",sizeof(L"MS Shell Dlg"));
			INT_PTR answer = DialogBoxIndirectParamW(GetModuleHandle(NULL),&definition.dialog,owner,Procedure,reinterpret_cast<LPARAM>(this));
			hr = answer == -1 ? HrEnsureLastError() : FAILED(m_error) ? m_error : m_committed ? S_OK : S_FALSE;
			return hr;
		}
	};
	HRESULT CDesignerEditor::EditTreeNodes()
	{
		HRESULT hr;
		Check(FinishInline(TRUE));
		{
			CTreeNodesDialog dialog(this,m_selected);
			Check(dialog.Show(GetAncestor(m_window,GA_ROOT)));
		}
	Cleanup:
		return hr;
	}
}
