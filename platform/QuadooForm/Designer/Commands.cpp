#include "Designer.h"

namespace QFD
{
BOOL OpenDesignerClipboard(HWND owner)
{
	// Clipboard history and other applications can briefly hold the clipboard
	// after a change. Retry only contention, with a bounded 90 ms total delay.
	for(UINT attempt = 0; attempt < 10; attempt++)
	{
		if(OpenClipboard(owner)) return TRUE;
		DWORD error = GetLastError();
		if(error != ERROR_ACCESS_DENIED || attempt == 9) { SetLastError(error); return FALSE; }
		Sleep(10);
	}
	return FALSE;
}

// The clipboard uses ordinary Unicode JSON so definitions can also be inspected
// or exchanged with other applications. Never retain a locked clipboard handle.
HRESULT WriteDefinitionClipboard(HWND owner, IJSONObject* definition)
{
	HRESULT hr;
	TStackRef<IJSONValue> value;
	Check(JSONWrapObject(definition,&value));
	Check(WriteComponentClipboard(owner,value));
Cleanup:
	return hr;
}
HRESULT WriteComponentClipboard(HWND owner, IJSONValue* definition)
{
	HRESULT hr;
	CMemoryStream text;
	HGLOBAL memory = NULL;
	PWSTR buffer = NULL;
	BOOL opened = FALSE;
	CheckIf(!definition,E_INVALIDARG);
	Check(JSONSerialize(definition,&text));
	CheckIf(text.DataRemaining() > 64 * 1024 * 1024 - sizeof(WCHAR),E_INVALIDARG);
	memory = GlobalAlloc(GMEM_MOVEABLE,text.DataRemaining() + sizeof(WCHAR));
	CheckIf(!memory,E_OUTOFMEMORY);
	buffer = static_cast<PWSTR>(GlobalLock(memory));
	CheckIf(!buffer,HrEnsureLastError());
	CopyMemory(buffer,text.GetReadPtr(),text.DataRemaining());
	buffer[text.DataRemaining() / sizeof(WCHAR)] = 0;
	GlobalUnlock(memory);
	CheckIf(!OpenDesignerClipboard(owner),HrEnsureLastError());
	opened = TRUE;
	CheckIf(!EmptyClipboard(),HrEnsureLastError());
	CheckIf(!SetClipboardData(CF_UNICODETEXT,memory),HrEnsureLastError());
	memory = NULL;
	hr = S_OK;
Cleanup:
	if(opened) CloseClipboard();
	if(memory) GlobalFree(memory);
	return hr;
}

HRESULT ReadDefinitionClipboard(HWND owner, IJSONValue** definition)
{
	HRESULT hr;
	CRString text;
	HANDLE memory;
	PCWSTR buffer = NULL;
	BOOL opened = FALSE;
	SIZE_T size, length = 0;
	CheckIf(!definition,E_POINTER);
	*definition = NULL;
	CheckIf(!OpenDesignerClipboard(owner),HrEnsureLastError());
	opened = TRUE;
	memory = GetClipboardData(CF_UNICODETEXT);
	CheckIf(!memory,HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
	size = GlobalSize(memory);
	CheckIf(size < sizeof(WCHAR) || size > 64 * 1024 * 1024,E_INVALIDARG);
	buffer = static_cast<PCWSTR>(GlobalLock(memory));
	CheckIf(!buffer,HrEnsureLastError());
	while(length < size / sizeof(WCHAR) && buffer[length]) length++;
	CheckIf(length == size / sizeof(WCHAR),E_INVALIDARG);
	Check(RStrCreateW(static_cast<INT>(length),buffer,&text));
	GlobalUnlock(memory);
	buffer = NULL;
	CloseClipboard();
	opened = FALSE;
	Check(JSONParse(NULL,text.c_wstr(),RStrLen(text),definition));
	CheckIf(!*definition || ((*definition)->GetType() != JSON::Object && (*definition)->GetType() != JSON::Array),E_INVALIDARG);
Cleanup:
	if(buffer) GlobalUnlock(memory);
	if(opened) CloseClipboard();
	if(FAILED(hr) && definition) SafeRelease(*definition);
	return hr;
}

HMENU CreateControlMenu()
{
	HMENU menu = CreatePopupMenu();
	if(menu)
	{
		if(!AppendMenuW(menu,MF_STRING,SendToBack,L"Send to &Back") ||
			!AppendMenuW(menu,MF_STRING,SendToFront,L"Send to &Front") ||
			!AppendMenuW(menu,MF_SEPARATOR,0,NULL) ||
			!AppendMenuW(menu,MF_STRING,Cut,L"Cu&t\tCtrl+X") ||
			!AppendMenuW(menu,MF_STRING,Copy,L"&Copy\tCtrl+C") ||
			!AppendMenuW(menu,MF_STRING,Paste,L"&Paste\tCtrl+V") ||
			!AppendMenuW(menu,MF_STRING,DeleteSelection,L"&Delete\tDelete") ||
			!AppendMenuW(menu,MF_SEPARATOR,0,NULL) ||
			!AppendMenuW(menu,MF_STRING,ToggleEnabled,L"&Enabled") ||
			!AppendMenuW(menu,MF_STRING,TabOrder,L"Tab &Order...") ||
			!AppendMenuW(menu,MF_STRING,MenuEditor,L"&Menu Editor...") ||
			!AppendMenuW(menu,MF_STRING,ImageListEditor,L"&Image List Editor...") ||
			!AppendMenuW(menu,MF_STRING,TreeNodesEditor,L"Tree &Nodes Editor...") ||
			!AppendMenuW(menu,MF_STRING,ListColumnsEditor,L"ListView &Columns Editor...") ||
			!AppendMenuW(menu,MF_SEPARATOR,0,NULL) ||
			!AppendMenuW(menu,MF_STRING,AddStatusPanel,L"Add Status &Panel") ||
			!AppendMenuW(menu,MF_STRING,MoveStatusPanelLeft,L"Move Panel &Left") ||
			!AppendMenuW(menu,MF_STRING,MoveStatusPanelRight,L"Move Panel &Right"))
		{
			DestroyMenu(menu);
			menu = NULL;
		}
	}
	return menu;
}

VOID CDesignerEditor::UpdateCommandMenu(HMENU menu)
{
	for(INT n = 0; n < GetMenuItemCount(menu); n++)
	{
		UINT command = GetMenuItemID(menu,n);
		if(command >= DeleteSelection && command <= LastCommand)
		{
			EnableMenuItem(menu,command,MF_BYCOMMAND | (CanExecute(command) ? MF_ENABLED : MF_GRAYED));
			CheckMenuItem(menu,command,MF_BYCOMMAND | (IsCommandChecked(command) ? MF_CHECKED : MF_UNCHECKED));
		}
		HMENU child = GetSubMenu(menu,n);
		if(child) UpdateCommandMenu(child);
	}
}

HRESULT CDesignerEditor::SelectContextID(REFGUID id)
{
	return IsSelected(id) ? FinishInline(TRUE) : SelectID(id);
}

VOID CDesignerEditor::ContextMenu(HWND source, POINT screen)
{
	HRESULT hr = S_OK;
	BOOL keyboard = screen.x == -1 && screen.y == -1;
	POINT point = screen;
	if(m_dragging) return;
	if(!keyboard)
	{
		ScreenToClient(source,&point);
		if(source == m_tree)
		{
			TVHITTESTINFO hit; ZeroMemory(&hit,sizeof(hit)); hit.pt = point;
			HTREEITEM item = TreeView_HitTest(m_tree,&hit);
			TVITEMW entry; ZeroMemory(&entry,sizeof(entry)); entry.mask = TVIF_PARAM; entry.hItem = item;
			if(!item || !TreeView_GetItem(m_tree,&entry) || entry.lParam < 0 || entry.lParam >= m_nodes.Length()) return;
			hr = SelectContextID(m_nodes[entry.lParam].id);
		}
		else if(source == m_tray)
		{
			LVHITTESTINFO hit; ZeroMemory(&hit,sizeof(hit)); hit.pt = point;
			INT row = ListView_HitTest(m_tray,&hit);
			LVITEMW entry; ZeroMemory(&entry,sizeof(entry)); entry.mask = LVIF_PARAM; entry.iItem = row;
			if(row >= 0 && ListView_GetItem(m_tray,&entry) && entry.lParam >= 0 && entry.lParam < m_nodes.Length()) hr = SelectContextID(m_nodes[entry.lParam].id);
			else { GUID id; hr = m_document.Root()->GetID(&id); if(SUCCEEDED(hr)) hr = SelectContextID(id); }
		}
		else
		{
			point = screen; ScreenToClient(m_canvas,&point);
			hr = SelectMenuAt(point);
			if(hr == S_FALSE)
			{
				IQuadooControl* hit = HitControl(point);
				if(!hit)
				{
					hit = m_document.Root();
				}
				if(!hit)
				{
					return;
				}
				GUID id;
				hr = hit->GetID(&id);
				if(SUCCEEDED(hr))
				{
					hr = SelectContextID(id);
				}
			}
			SetFocus(m_overlay);
		}
	}
	else
	{
		RECT rect;
		if(source == m_tree && TreeView_GetItemRect(m_tree,TreeView_GetSelection(m_tree),&rect,TRUE)) { screen.x = rect.left; screen.y = rect.bottom; ClientToScreen(m_tree,&screen); }
		else if(source == m_tray) { screen.x = screen.y = 12; ClientToScreen(m_tray,&screen); }
		else if(SUCCEEDED(SelectionRect(&rect))) { screen.x = rect.left; screen.y = rect.bottom; ClientToScreen(m_canvas,&screen); }
		else return;
	}
	if(FAILED(hr)) { Error(hr,L"Select control"); return; }
	HMENU menu = CreateControlMenu();
	if(menu)
	{
		UpdateCommandMenu(menu);
		UINT command = TrackPopupMenu(menu,TPM_RETURNCMD | TPM_RIGHTBUTTON,screen.x,screen.y,0,m_window,NULL);
		DestroyMenu(menu);
		if(command) { hr = ExecuteCommand(command,GetAncestor(m_window,GA_ROOT)); if(FAILED(hr)) Error(hr,L"Command"); }
	}
}

HRESULT CDesignerEditor::CopySelection(BOOL cut)
{
	HRESULT hr;
	TStackRef<IJSONValue> definition;
	CheckIf(!SelectionCount(),E_ACCESSDENIED);
	Check(m_document.CopyControls(m_selection.GetItemPtr(0),SelectionCount(),&definition));
	Check(WriteComponentClipboard(m_window,definition));
	// A failed clipboard operation must never delete the source.
	if(cut)
	{
		Check(Delete());
	}
Cleanup:
	return hr;
}

HRESULT CDesignerEditor::PasteSelection()
{
	HRESULT hr;
	TStackRef<IJSONValue> definition;
	GUID added;
	GUID target = m_selected;
	TArray<GUID> pasted;
	CheckIf(m_readOnly,E_ACCESSDENIED);
	Check(ReadDefinitionClipboard(m_window,&definition));
	if(SelectionCount() > 1)
	{
		TStackRef<IQuadooControl> control;
		TStackRef<IQuadooControlContainer> parent;
		Check(Selected(&control));
		Check(control->GetParent(&parent));
		CheckIf(!parent,E_UNEXPECTED);
		Check(parent->GetID(&target));
	}
	m_changeDepth++;
	hr = m_document.PasteDefinition(target,definition,&added,&pasted);
	m_changeDepth--;
	Check(hr);
	m_selection.Clear();
	m_selected = added;
	Check(Refresh(TRUE));
	Check(SelectID(added));
	for(sysint n = 0; n < pasted.Length(); n++)
	{
		Check(SelectID(pasted[n],TRUE));
	}
Cleanup:
	return hr;
}

HRESULT CDesignerEditor::ReorderSelection(BOOL front)
{
	HRESULT hr;
	m_changeDepth++;
	hr = m_document.Reorder(m_selected,front);
	m_changeDepth--;
	Check(hr);
	Check(Refresh(TRUE));
Cleanup:
	return hr;
}
}
