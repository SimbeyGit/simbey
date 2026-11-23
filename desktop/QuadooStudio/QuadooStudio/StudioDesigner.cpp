#include <windows.h>
#include "resource.h"
#include "Library\DPI.h"
#include "Library\ChooseFile.h"
#include "Library\Util\TextHelpers.h"
#include "QuadooProject.h"
#include "Tabs.h"
#include "ApplicationProject.h"
#include "QuadooStudio.h"

CProjectFile* CQuadooProject::ActiveFile()
{
	sysint tab = m_pTabs ? m_pTabs->GetActiveTab() : -1;
	return tab >= 0 ? static_cast<CProjectFile*>(m_pTabs->TGetTabData<CProjectTab>(tab)) : NULL;
}

HRESULT CQuadooProject::FindSource(RSTRING path, CProjectFile** file)
{
	HRESULT hr;
	CProjectTab* tab = NULL;
	CheckIf(!file,E_POINTER);
	*file = NULL;
	Check(m_mapFiles.Find(path,&tab));
	*file = static_cast<CProjectFile*>(tab);
Cleanup:
	return hr;
}

QFD::CDesignerEditor* CQuadooProject::ActiveDesigner()
{
	CProjectFile* file = ActiveFile();
	return file && file->m_form && file->m_form->design ? file->m_form->editor : NULL;
}

BOOL CQuadooProject::DesignerActive() { return ActiveDesigner() != NULL; }
HWND CQuadooProject::ActivePalette() { QFD::CDesignerEditor* editor = ActiveDesigner(); return editor ? editor->PaletteWindow() : NULL; }

VOID CQuadooProject::SetApplicationDebugReadOnly(BOOL readOnly)
{
	if(m_applicationDebugReadOnly != readOnly)
	{
		m_applicationDebugReadOnly = readOnly;
		if(m_pEditor)
			m_pEditor->SetReadOnly(readOnly || m_pDebugger != NULL);
		for(sysint n = 0; n < m_mapFiles.Length(); n++)
		{
			CProjectFile* file = static_cast<CProjectFile*>(*m_mapFiles.GetValuePtr(n));
			if(file->m_form && file->m_form->editor)
				file->m_form->editor->SetReadOnly(readOnly);
		}
	}
}

VOID CQuadooProject::HideDesigners()
{
	for(sysint n = 0; n < m_mapFiles.Length(); n++)
	{
		CProjectFile* file = static_cast<CProjectFile*>(*m_mapFiles.GetValuePtr(n));
		if(file->m_form) file->m_form->Hide();
	}
}

BOOL CQuadooProject::IsApplication()
{
	CRString target;
	return m_pProject && SUCCEEDED(GetProjectTarget(&target)) && target == RSTRING_CAST(L"application");
}

BOOL CQuadooProject::TranslateProjectMessage(MSG* message)
{
	BOOL fHandled;
	QFD::CDesignerEditor* editor = ActiveDesigner();
	if(editor)
	{
		HWND outline = NULL;
		if(message && message->hwnd == m_hwndTree)
		{
			CProjectTreeNode* node = TreeNode(TreeView_GetSelection(m_hwndTree));
			if(node && !node->source && node->file == ActiveFile())
				outline = m_hwndTree;
		}
		fHandled = editor->TranslateEditorMessage(message,outline);
	}
	else
		fHandled = FALSE;
	return fHandled;
}

HRESULT CQuadooProject::SetDesignView(BOOL design)
{
	HRESULT hr;
	CProjectFile* file = ActiveFile();
	BOOL previous = FALSE;
	CheckIf(!file || !file->m_form,E_INVALIDARG);
	previous = file->m_form->design;
	if(ActiveDesigner())
	{
		Check(ActiveDesigner()->CommitPropertyEdit());
	}
	file->m_form->design = design;
	hr = SwitchToFile(file);
	if(FAILED(hr)) file->m_form->design = previous;
Cleanup:
	return hr;
}

CProjectTreeNode* CQuadooProject::TreeNode(HTREEITEM item)
{
	for(sysint n = 0; n < m_treeNodes.Length(); n++)
	{
		if(m_treeNodes[n]->item == item)
			return m_treeNodes[n];
	}
	return NULL;
}

HRESULT CQuadooProject::AddTreeNode(CProjectTab* file, BOOL source, REFGUID id, HTREEITEM parent, RSTRING label, INT image, CProjectTreeNode** result)
{
	HRESULT hr;
	CProjectTreeNode* node = __new CProjectTreeNode(file,source);
	TVINSERTSTRUCTW insert = {0};
	CheckAlloc(node);
	node->id = id;
	insert.hParent = parent; insert.hInsertAfter = TVI_LAST;
	insert.item.mask = TVIF_PARAM | TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
	insert.item.pszText = const_cast<PWSTR>(RStrToWide(label));
	insert.item.lParam = reinterpret_cast<LPARAM>(node);
	insert.item.iImage = insert.item.iSelectedImage = image;
	Check(m_treeNodes.Append(node));
	node->item = TreeView_InsertItem(m_hwndTree,&insert);
	if(!node->item) { m_treeNodes.Remove(m_treeNodes.Length() - 1,NULL); hr = E_OUTOFMEMORY; goto Cleanup; }
	*result = node; node = NULL; hr = S_OK;
Cleanup:
	SafeDelete(node);
	return hr;
}

VOID CQuadooProject::ClearTreeNodes(CProjectTab* file)
{
	m_treeDepth++;
	for(sysint n = m_treeNodes.Length(); n > 0; n--)
	{
		CProjectTreeNode* node = m_treeNodes[n - 1];
		if(!file || node->file == file)
		{
			TreeView_DeleteItem(m_hwndTree,node->item);
			m_treeNodes.Remove(n - 1,NULL); __delete node;
		}
	}
	m_treeDepth--;
}

INT CQuadooProject::TreeIcon(RSTRING type)
{
	INT image = 2;
	HICON icon = NULL;
	TStackRef<IQuadooControlFactory> visual;
	TStackRef<IQuadooComponentFactory> component;
	TStackRef<IQuadooComponentRegistry> components;
	TStackRef<IQuadooPaletteIcon> provider;
	if(SUCCEEDED(m_treeIcons.Find(type,&image))) return image;
	if(!m_formRegistry) return 2;
	if(SUCCEEDED(m_formRegistry->FindFactory(type,&visual))) visual->QueryInterface(__uuidof(IQuadooPaletteIcon),reinterpret_cast<PVOID*>(&provider));
	else if(SUCCEEDED(m_formRegistry->QueryInterface(__uuidof(IQuadooComponentRegistry),reinterpret_cast<PVOID*>(&components))) && SUCCEEDED(components->FindComponentFactory(type,&component)))
		component->QueryInterface(__uuidof(IQuadooPaletteIcon),reinterpret_cast<PVOID*>(&provider));
	HIMAGELIST images = TreeView_GetImageList(m_hwndTree,TVSIL_NORMAL);
	INT width, height;
	if(images && provider && ImageList_GetIconSize(images,&width,&height) && provider->GetPaletteIcon(width,height,&icon) == S_OK && icon)
	{
		INT added = ImageList_AddIcon(images,icon);
		if(added >= 0) image = added;
	}
	if(icon) DestroyIcon(icon);
	// A missing package may be installed while this project remains open.
	// Retry a generic icon on the next outline refresh instead of caching absence.
	if(image != 2) m_treeIcons.Add(type,image);
	return image;
}

HRESULT CQuadooProject::SavedOutline(CProjectFile* file, IJSONObject* definition, HTREEITEM parent, UINT depth)
{
	HRESULT hr;
	TStackRef<IJSONValue> value;
	CRString name, type, label;
	GUID id = GUID_NULL;
	CProjectTreeNode* node;
	CheckIf(depth > 128,E_INVALIDARG);
	Check(definition->FindNonNullValueW(L"name",&value));
	Check(value->GetString(&name));
	value.Release();
	Check(definition->FindNonNullValueW(L"type",&value));
	Check(value->GetString(&type));
	value.Release();
	if(SUCCEEDED(definition->FindNonNullValueW(L"id",&value))) value->GetStringAsGuid(id);
	Check(RStrFormatW(&label,L"%ls (%ls)",name.c_wstr(),type.c_wstr()));
	Check(AddTreeNode(file,FALSE,id,parent,label,TreeIcon(type),&node));
	if(type == RSTRING_CAST(L"StatusBar"))
	{
		TStackRef<IJSONValue> propertiesValue;
		TStackRef<IJSONValue> panelsValue;
		TStackRef<IJSONObject> properties;
		TStackRef<IJSONArray> panels;
		CProjectTreeNode* collection;
		GUID groupID = id;
		groupID.Data1 ^= 0x53544154;
		groupID.Data4[7] ^= 0x50;
		Check(AddTreeNode(file,FALSE,groupID,node->item,RSTRING_CAST(L"Panels"),TreeIcon(type),&collection));
		if(SUCCEEDED(definition->FindNonNullValueW(L"properties",&propertiesValue)))
		{
			Check(propertiesValue->GetObject(&properties));
			if(SUCCEEDED(properties->FindNonNullValueW(L"Panels",&panelsValue)))
			{
				Check(panelsValue->GetArray(&panels));
				for(sysint n = 0; n < panels->Count(); n++)
				{
					TStackRef<IJSONObject> panel;
					TStackRef<IJSONValue> identity;
					TStackRef<IJSONValue> text;
					CRString caption;
					CRString panelLabel;
					GUID panelID;
					CProjectTreeNode* entry;
					Check(panels->GetObject(n,&panel));
					// Hand-authored definitions may omit panel IDs. The native
					// collection assigns them when the designer opens; only stable
					// saved identities can be selected in this preliminary outline.
					if(FAILED(panel->FindNonNullValueW(L"id",&identity)))
					{
						continue;
					}
					Check(identity->GetStringAsGuid(panelID));
					caption = RSTRING_CAST(L"");
					if(SUCCEEDED(panel->FindNonNullValueW(L"Text",&text)))
					{
						caption = static_cast<RSTRING>(NULL);
						Check(text->GetString(&caption));
					}
					Check(RStrFormatW(&panelLabel,L"[%u] %r",static_cast<UINT>(n),static_cast<RSTRING>(caption)));
					Check(AddTreeNode(file,FALSE,panelID,collection->item,panelLabel,TreeIcon(type),&entry));
				}
			}
		}
	}
	for(UINT group = 0; group < 2; group++)
	{
		TStackRef<IJSONArray> children; value.Release();
		if(FAILED(definition->FindNonNullValueW(group ? L"components" : L"children",&value))) continue;
		Check(value->GetArray(&children));
		for(sysint n = 0; n < children->Count(); n++)
		{
			TStackRef<IJSONObject> child;
			Check(children->GetObject(n,&child));
			Check(SavedOutline(file,child,node->item,depth + 1));
		}
	}
Cleanup:
	return hr;
}

HRESULT CQuadooProject::InitializeFormUnit(CProjectFile* file, IJSONObject* metadata)
{
	HRESULT hr;
	TStackRef<CStudioFormUnit> unit;
	TStackRef<IJSONObject> definition;
	PWSTR text = NULL; INT length = 0;
	CheckIf(!IsApplication() || file->m_form,E_INVALIDARG);
	if(!m_formRegistry)
	{
		Check(QuadooGetControlRegistry(&m_formRegistry));
	}
	unit.Attach(__new CStudioFormUnit);
	CheckAlloc(unit);
	Check(unit->Initialize(metadata,m_rstrProjectDir,file->m_rstrPath));
	for(sysint n = 0; n < m_mapFiles.Length(); n++)
	{
		CProjectFile* other = static_cast<CProjectFile*>(*m_mapFiles.GetValuePtr(n));
		CheckIf(other->m_form && (IsEqualGUID(other->m_form->id,unit->id) || !lstrcmpiW(other->m_form->definition.c_wstr(),unit->definition.c_wstr()) || !lstrcmpW(other->m_form->name.c_wstr(),unit->name.c_wstr())),E_INVALIDARG);
	}
	file->m_form = unit.Detach();
	file->m_form->owner = this;
	file->m_form->sourceFile = file;
	// Show saved hierarchy without instantiating controls or loading factories.
	hr = Text::LoadFromFile(file->m_form->definition.c_wstr(),&text,&length,CP_UTF8);
	if(SUCCEEDED(hr)) hr = QFD::Parse(text,length,&definition);
	if(SUCCEEDED(hr)) hr = SavedOutline(file,definition,file->m_hItem,0);
	if(FAILED(hr))
	{
		m_treeDepth++;
		for(sysint n = m_treeNodes.Length(); n > 0; n--)
		{
			CProjectTreeNode* stale = m_treeNodes[n - 1];
			if(stale->file == file && !stale->source)
			{
				TreeView_DeleteItem(m_hwndTree,stale->item);
				m_treeNodes.Remove(n - 1,NULL);
				__delete stale;
			}
		}
		m_treeDepth--;
		CProjectTreeNode* node; CRString label;
		Check(RStrFormatW(&label,L"%ls (%ls: definition unavailable)",file->m_form->name.c_wstr(),file->m_form->form ? L"Form" : L"Frame"));
		Check(AddTreeNode(file,FALSE,GUID_NULL,file->m_hItem,label,2,&node));
	}
	hr = S_OK;
Cleanup:
	SafeDeleteArray(text);
	return hr;
}

static INT CALLBACK CompareDesignerNodes(LPARAM first, LPARAM second, LPARAM)
{
	CProjectTreeNode* a = reinterpret_cast<CProjectTreeNode*>(first);
	CProjectTreeNode* b = reinterpret_cast<CProjectTreeNode*>(second);
	return a->creationOrder < b->creationOrder ? -1 : a->creationOrder > b->creationOrder ? 1 : 0;
}

HRESULT CQuadooProject::RefreshOutline(CProjectFile* file)
{
	HRESULT hr = S_OK;
	QFD::CDesignerEditor* editor = file->m_form->editor;
	HTREEITEM first = TreeView_GetFirstVisible(m_hwndTree);
	if(!editor || file->m_form->outlineRevision == editor->OutlineRevision()) goto Cleanup;
	m_treeDepth++; SendMessage(m_hwndTree,WM_SETREDRAW,FALSE,0);
	for(sysint n = 0; n < m_treeNodes.Length(); n++) if(m_treeNodes[n]->file == file) m_treeNodes[n]->seen = m_treeNodes[n]->source;
	for(UINT n = 0; n < editor->OutlineCount(); n++)
	{
		QFD::OutlineEntry entry;
		CProjectTreeNode* node = NULL;
		HTREEITEM parent = file->m_hItem;
		CRString label;
		hr = editor->GetOutlineEntry(n,&entry);
		if(FAILED(hr)) break;
		for(sysint p = 0; p < m_treeNodes.Length(); p++)
		{
			CProjectTreeNode* candidate = m_treeNodes[p];
			if(candidate->file != file || candidate->source) continue;
			if(IsEqualGUID(candidate->id,entry.id)) node = candidate;
			if(!IsEqualGUID(entry.parent,GUID_NULL) && IsEqualGUID(candidate->id,entry.parent)) parent = candidate->item;
		}
		hr = RStrFormatW(&label,L"%ls (%ls)%ls",entry.name.c_wstr(),entry.type.c_wstr(),entry.flags & QFD::OutlinePlaceholder ? L" [unavailable]" : L"");
		if(FAILED(hr)) break;
		if(!node)
			hr = AddTreeNode(file,FALSE,entry.id,parent,label,TreeIcon(entry.type),&node);
		else
		{
			TVITEMW item = {0}; item.mask = TVIF_TEXT; item.hItem = node->item; item.pszText = const_cast<PWSTR>(label.c_wstr());
			if(!TreeView_SetItem(m_hwndTree,&item)) hr = E_FAIL;
		}
		if(FAILED(hr)) break;
		node->seen = TRUE;
		node->creationOrder = n;
	}
	if(SUCCEEDED(hr))
	{
		for(sysint n = m_treeNodes.Length(); n > 0; n--)
		{
			CProjectTreeNode* node = m_treeNodes[n - 1];
			if(node->file == file && !node->seen)
			{
				TreeView_DeleteItem(m_hwndTree,node->item); m_treeNodes.Remove(n - 1,NULL); __delete node;
			}
		}
		// Keep the stable item handles, expansion and selection while changing
		// sibling order to match the designer's creation-order outline.
		for(sysint n = 0; n < m_treeNodes.Length(); n++)
		{
			CProjectTreeNode* node = m_treeNodes[n];
			if(node->file == file && !node->source && TreeView_GetChild(m_hwndTree,node->item))
			{
				TVSORTCB order = { node->item,CompareDesignerNodes,0 };
				if(!TreeView_SortChildrenCB(m_hwndTree,&order,FALSE))
				{
					hr = E_FAIL;
					break;
				}
			}
		}
		if(file->m_form->expandRoot)
		{
			for(sysint n = 0; n < m_treeNodes.Length(); n++)
			{
				if(m_treeNodes[n]->file == file && !m_treeNodes[n]->source && TreeView_GetParent(m_hwndTree,m_treeNodes[n]->item) == file->m_hItem && TreeView_GetChild(m_hwndTree,m_treeNodes[n]->item))
				{
					TreeView_Expand(m_hwndTree,m_treeNodes[n]->item,TVE_EXPAND);
					file->m_form->expandRoot = FALSE;
				}
			}
		}
		if(SUCCEEDED(hr))
			file->m_form->outlineRevision = editor->OutlineRevision();
	}
	if(TreeNode(first)) TreeView_SelectSetFirstVisible(m_hwndTree,first);
	SendMessage(m_hwndTree,WM_SETREDRAW,TRUE,0); InvalidateRect(m_hwndTree,NULL,FALSE); m_treeDepth--;
Cleanup:
	return hr;
}

HRESULT CQuadooProject::SelectTreeItem(HTREEITEM item)
{
	HRESULT hr = S_FALSE;
	CProjectTreeNode* node = TreeNode(item);
	GUID selected = GUID_NULL, id = GUID_NULL;
	CProjectFile* file = NULL; BOOL previous = FALSE;
	if(m_treeDepth || !node || node->source) goto Cleanup;
	file = static_cast<CProjectFile*>(node->file);
	id = node->id;
	previous = file->m_form->design;
	file->m_form->design = TRUE;
	hr = SwitchToFile(file);
	if(FAILED(hr)) { file->m_form->design = previous; goto Cleanup; }
	if(IsEqualGUID(id,GUID_NULL)) file->m_form->editor->SelectAncestor(TRUE);
	else
	{
		hr = file->m_form->editor->SelectID(id,(GetKeyState(VK_SHIFT) & 0x8000) != 0);
	}
Cleanup:
	if((FAILED(hr) || hr == S_FALSE) && ActiveDesigner())
	{
		ActiveDesigner()->GetSelectedID(&selected);
		m_treeDepth++;
		for(sysint n = 0; n < m_treeNodes.Length(); n++) if(!m_treeNodes[n]->source && m_treeNodes[n]->file == ActiveFile() && IsEqualGUID(m_treeNodes[n]->id,selected)) TreeView_SelectItem(m_hwndTree,m_treeNodes[n]->item);
		m_treeDepth--;
	}
	return hr;
}

HRESULT CQuadooProject::ExpandTreeItem(HTREEITEM)
{
	return S_OK;
}

HRESULT CQuadooProject::ReloadSavedOutline(CProjectFile* file)
{
	HRESULT hr;
	TStackRef<IJSONObject> definition;
	PWSTR text = NULL; INT length = 0;
	m_treeDepth++;
	for(sysint n = m_treeNodes.Length(); n > 0; n--)
	{
		CProjectTreeNode* node = m_treeNodes[n - 1];
		if(node->file == file && !node->source)
		{
			TreeView_DeleteItem(m_hwndTree,node->item); m_treeNodes.Remove(n - 1,NULL); __delete node;
		}
	}
	Check(Text::LoadFromFile(file->m_form->definition.c_wstr(),&text,&length,CP_UTF8));
	Check(QFD::Parse(text,length,&definition));
	Check(SavedOutline(file,definition,file->m_hItem,0));
Cleanup:
	m_treeDepth--;
	SafeDeleteArray(text);
	return hr;
}

VOID CQuadooProject::DesignerChanged(UINT_PTR session)
{
	PostMessageW(GetParent(m_hwnd),QFP::RefreshPackagesMessage,0,0);
	for(sysint n = 0; n < m_mapFiles.Length(); n++)
	{
		CProjectFile* file = static_cast<CProjectFile*>(*m_mapFiles.GetValuePtr(n));
		if(!file->m_form || file->m_form->session != session || !file->m_form->editor) continue;
		RefreshOutline(file);
		sysint tab = m_pTabs->FindTab(file);
		if(tab >= 0) m_pTabs->HighlightTab(tab,file->IsModified() || (m_pCodeFile == file && m_pEditor->IsModified()));
		if(file == ActiveFile() && DesignerActive())
		{
			GUID id; file->m_form->editor->GetSelectedID(&id);
			m_treeDepth++;
			for(sysint item = 0; item < m_treeNodes.Length(); item++)
			{
				if(m_treeNodes[item]->file == file && !m_treeNodes[item]->source && IsEqualGUID(m_treeNodes[item]->id,id))
				{
					if(TreeView_GetSelection(m_hwndTree) != m_treeNodes[item]->item) TreeView_SelectItem(m_hwndTree,m_treeNodes[item]->item);
					break;
				}
			}
			m_treeDepth--;
			UpdateDesignerStatus();
		}
		RECT rect; m_pTabs->GetTabsRect(0,0,rect); InvalidateRect(m_hwnd,&rect,FALSE);
		break;
	}
}

VOID CQuadooProject::UpdateDesignerStatus()
{
	if(!m_hwndStatus) return;
	QFD::CDesignerEditor* editor = ActiveDesigner();
	if(editor)
	{
		CRString name, xy, size, zoom; QuadooForm::Rect bounds; BOOL visual;
		editor->GetSelectionInfo(&name,&bounds,&visual);
		RStrFormatW(&xy,L"%d, %d",bounds.Left,bounds.Top);
		RStrFormatW(&size,L"%d x %d",bounds.Width,bounds.Height);
		RStrFormatW(&zoom,L"Zoom %u%%",editor->Zoom());
		SendMessage(m_hwndStatus,SB_SETTEXTW,0,reinterpret_cast<LPARAM>(RStrLen(ActiveFile()->m_form->sourceDiagnostic) ? ActiveFile()->m_form->sourceDiagnostic.c_wstr() : RStrToWide(editor->Status())));
		SendMessage(m_hwndStatus,SB_SETTEXTW,1,reinterpret_cast<LPARAM>(visual ? xy.c_wstr() : name.c_wstr()));
		SendMessage(m_hwndStatus,SB_SETTEXTW,2,reinterpret_cast<LPARAM>(visual ? size.c_wstr() : L""));
		SendMessage(m_hwndStatus,SB_SETTEXTW,3,reinterpret_cast<LPARAM>(zoom.c_wstr()));
	}
	else
	{
		CRString line, column;
		TEXT_EDIT_VIEW view = {0};
		ULONG row = 0, offset = 0;
		if(m_pCodeFile)
		{
			m_pEditor->GetTextEditView(&view);
			m_pEditor->GetTextDocument()->GetLineFromOffset(view.nCursorOffset,&row,&offset);
		}
		RStrFormatW(&line,L"Ln %u",row + 1);
		RStrFormatW(&column,L"Col %u",offset + 1);
		SendMessage(m_hwndStatus,SB_SETTEXTW,0,reinterpret_cast<LPARAM>(L"Ready"));
		SendMessage(m_hwndStatus,SB_SETTEXTW,1,reinterpret_cast<LPARAM>(m_pCodeFile ? line.c_wstr() : L""));
		SendMessage(m_hwndStatus,SB_SETTEXTW,2,reinterpret_cast<LPARAM>(m_pCodeFile ? column.c_wstr() : L""));
		SendMessage(m_hwndStatus,SB_SETTEXTW,3,reinterpret_cast<LPARAM>(L""));
	}
}

HRESULT CQuadooProject::DesignerCommand(UINT command)
{
	HRESULT hr;
	QFD::CDesignerEditor* editor = ActiveDesigner();
	CheckIf(!editor,OLECMDERR_E_DISABLED);
	Check(editor->ExecuteCommand(command,GetParent(m_hwnd)));
Cleanup:
	return hr;
}

HRESULT CQuadooProject::ExportDefinition()
{
	HRESULT hr;
	CChooseFile chooser;
	TStackRef<IJSONObject> definition;
	CRString initial;
	QFD::CDesignerEditor* editor = ActiveDesigner();
	CheckIf(!editor,OLECMDERR_E_DISABLED);
	Check(editor->CommitPropertyEdit());
	Check(chooser.Initialize());
	Check(RStrFormatW(&initial,L"%ls.%ls.json",ActiveFile()->m_form->name.c_wstr(),editor->Document().IsForm() ? L"qform" : L"qframe"));
	CheckIfIgnore(!chooser.SaveFile(m_hwnd,L"Export Definition Copy",initial.c_wstr(),L"QuadooForm JSON (*.json)\0*.json\0\0"),OLECMDERR_E_CANCELED);
	// An export cannot redirect the associated unit's storage or clear its dirty
	// state. Save File continues to save the paired source and definition.
	CheckIf(!lstrcmpiW(chooser.GetFile(0),ActiveFile()->m_form->definition.c_wstr()),E_INVALIDARG);
	Check(editor->Document().Serialize(&definition));
	Check(SaveStudioProject(definition,chooser.GetFile(0)));
Cleanup:
	return hr;
}
