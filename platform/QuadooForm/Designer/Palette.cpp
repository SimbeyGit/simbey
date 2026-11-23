#include "Designer.h"
#include "DesignerIconData.h"

namespace QFD
{
	struct CDesignerEditor::PaletteUpdate
	{
		TStackRef<CDesignerEditor> staged;
		BOOL visible, focused;

		PaletteUpdate() :
			visible(FALSE), focused(FALSE)
		{
			staged.Attach(__new CDesignerEditor);
		}

		~PaletteUpdate()
		{
			if(staged && staged->m_tools)
				DestroyWindow(staged->m_tools);
			if(staged && staged->m_tray)
				DestroyWindow(staged->m_tray);
		}
	};

	HRESULT CDesignerEditor::PreparePalette(IQuadooControlRegistry* registry,PaletteUpdate** output)
	{
		HRESULT hr; PaletteUpdate* update=NULL; RECT rect; CRString tool,top,focused; INT offset=0;
		CheckIf(!output,E_POINTER); *output=NULL;
		Check(CommitPropertyEdit());
		update=__new PaletteUpdate; CheckAlloc(update); CheckAlloc(update->staged);
		// Preserve the child's own visibility, even when its hosted palette
		// parent is hidden by Code view or another inactive designer session.
		update->visible=(GetWindowLongPtrW(m_tools,GWL_STYLE)&WS_VISIBLE)!=0; update->focused=GetFocus()==m_tools;
		if(m_tool>0 && m_tool<=m_palette.Length()) tool=m_palette[m_tool-1].name;
		{ LVITEMW item; ZeroMemory(&item,sizeof(item)); item.iItem=ListView_GetNextItem(m_tools,-1,LVNI_FOCUSED); item.mask=LVIF_PARAM;
		  if(item.iItem>=0 && ListView_GetItem(m_tools,&item) && item.lParam>0 && item.lParam<=m_palette.Length()) focused=m_palette[item.lParam-1].name; }
		INT first=ListView_GetTopIndex(m_tools);
		{ LVITEMW item; ZeroMemory(&item,sizeof(item)); item.iItem=first; item.mask=LVIF_PARAM;
		  if(ListView_GetItem(m_tools,&item) && item.lParam>0 && item.lParam<=m_palette.Length()) top=m_palette[item.lParam-1].name;
		  if(ListView_GetItemRect(m_tools,first,&rect,LVIR_BOUNDS)) offset=rect.top; }
		GetWindowRect(m_tools,&rect); MapWindowPoints(NULL,GetParent(m_tools),reinterpret_cast<POINT*>(&rect),2);
		update->staged->m_dpi=m_dpi;
		Check(update->staged->m_document.Initialize(registry));
		update->staged->m_tools=CreateWindowExW(0,WC_LISTVIEWW,L"",WS_CHILD | WS_TABSTOP | LVS_REPORT | LVS_NOCOLUMNHEADER | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS,rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top,GetParent(m_tools),NULL,GetModuleHandle(NULL),NULL);
		CheckIf(!update->staged->m_tools,HrEnsureLastError());
		update->staged->m_tray=CreateWindowExW(0,WC_LISTVIEWW,L"",WS_CHILD | LVS_REPORT | LVS_SHAREIMAGELISTS,0,0,1,1,m_window,NULL,GetModuleHandle(NULL),NULL);
		CheckIf(!update->staged->m_tray,HrEnsureLastError());
		{ LVCOLUMNW column; ZeroMemory(&column,sizeof(column)); column.mask=LVCF_WIDTH; column.cx=ListView_GetColumnWidth(m_tools,0); CheckIf(ListView_InsertColumn(update->staged->m_tools,0,&column)<0,E_OUTOFMEMORY); }
		SendMessageW(update->staged->m_tools,WM_SETFONT,SendMessageW(m_tools,WM_GETFONT,0,0),FALSE);
		ListView_SetExtendedListViewStyle(update->staged->m_tools,ListView_GetExtendedListViewStyle(m_tools));
		Check(update->staged->Palette());
		if(RStrLen(tool)) for(sysint n=0;n<update->staged->m_palette.Length();n++) if(tool==update->staged->m_palette[n].name)
		{
			update->staged->m_tool=static_cast<INT>(n+1); ListView_SetItemState(update->staged->m_tools,-1,0,LVIS_SELECTED); ListView_SetItemState(update->staged->m_tools,static_cast<INT>(n+1),LVIS_SELECTED,LVIS_SELECTED); break;
		}
		if(RStrLen(top)) for(sysint n=0;n<update->staged->m_palette.Length();n++) if(top==update->staged->m_palette[n].name)
		{
			RECT bounds; if(ListView_GetItemRect(update->staged->m_tools,static_cast<INT>(n+1),&bounds,LVIR_BOUNDS)) ListView_Scroll(update->staged->m_tools,0,bounds.top-offset); break;
		}
		if(RStrLen(focused)) for(sysint n=0;n<update->staged->m_palette.Length();n++) if(focused==update->staged->m_palette[n].name) { ListView_SetItemState(update->staged->m_tools,static_cast<INT>(n+1),LVIS_FOCUSED,LVIS_FOCUSED); break; }
		CheckIf(!SetWindowSubclass(update->staged->m_tools,PaletteProc,1,reinterpret_cast<DWORD_PTR>(this)),HrEnsureLastError());
		*output=update;
		update=NULL;
	Cleanup:
		__delete update;
		return hr;
	}

	VOID CDesignerEditor::CommitPalette(PaletteUpdate* update)
	{
		m_changeDepth++;
		RemoveWindowSubclass(m_tools,PaletteProc,1);
		HWND old=m_tools; m_tools=update->staged->m_tools; update->staged->m_tools=old;
		m_palette.Swap(update->staged->m_palette); SwapData(m_images,update->staged->m_images); m_tool=update->staged->m_tool;
		SetWindowLongPtrW(old,GWLP_ID,0); SetWindowLongPtrW(m_tools,GWLP_ID,102);
		if(m_readOnly)
			EnableWindow(m_tools,FALSE);
		ListView_SetImageList(m_tray,m_images,LVSIL_SMALL);
		for(INT row=0;row<ListView_GetItemCount(m_tray);row++)
		{
			LVITEMW item; ZeroMemory(&item,sizeof(item)); item.iItem=row; item.mask=LVIF_PARAM;
			if(ListView_GetItem(m_tray,&item) && item.lParam>=0 && item.lParam<m_nodes.Length()) { item.mask=LVIF_IMAGE; item.iImage=ComponentImage(m_nodes[item.lParam].component); ListView_SetItem(m_tray,&item); }
		}
		ShowWindow(old,SW_HIDE); ShowWindow(m_tools,update->visible ? SW_SHOWNA : SW_HIDE); if(update->focused) SetFocus(m_tools);
		m_changeDepth--;
		__delete update;
	}

	VOID CDesignerEditor::DiscardPalette(PaletteUpdate* update)
	{
		__delete update;
	}

	static HRESULT AddEmbedded(HIMAGELIST images, const BYTE* data, UINT size, UINT pixels)
	{
		HRESULT hr = S_OK;
		HICON icon = CreateIconFromResourceEx(const_cast<BYTE*>(data),size,TRUE,0x30000,pixels,pixels,0);
		CheckIf(!icon,HrEnsureLastError());
		CheckIf(ImageList_AddIcon(images,icon) < 0,E_OUTOFMEMORY);
	Cleanup:
		if(icon) DestroyIcon(icon);
		return hr;
	}

	static BOOL ValidIcon(HICON icon, UINT pixels)
	{
		ICONINFO info;
		BITMAP bitmap;
		BOOL valid = FALSE;
		if(GetIconInfo(icon,&info))
		{
			HBITMAP image = info.hbmColor ? info.hbmColor : info.hbmMask;
			if(GetObject(image,sizeof(bitmap),&bitmap))
				valid = bitmap.bmWidth == static_cast<LONG>(pixels) && bitmap.bmHeight == static_cast<LONG>(pixels * (info.hbmColor ? 1 : 2));
			if(info.hbmColor) DeleteObject(info.hbmColor);
			if(info.hbmMask) DeleteObject(info.hbmMask);
		}
		return valid;
	}

	VOID CDesignerEditor::ClearPaletteImages()
	{
		if(m_images)
		{
			if(IsWindow(m_tools)) ListView_SetImageList(m_tools,NULL,LVSIL_SMALL);
			if(IsWindow(m_tray)) ListView_SetImageList(m_tray,NULL,LVSIL_SMALL);
			ImageList_Destroy(m_images);
			m_images = NULL;
		}
	}

	VOID CDesignerEditor::ClearPalette()
	{
		ClearPaletteImages();
		for(sysint n = 0; n < m_palette.Length(); n++)
		{
			SafeRelease(m_palette[n].factory);
			RStrRelease(m_palette[n].name);
		}
		m_palette.Clear();
	}

	HRESULT CDesignerEditor::RebuildPaletteImages(UINT dpi)
	{
		HRESULT hr;
		HIMAGELIST images = NULL;
		HICON icon = NULL;
		TArray<INT> indices;
		CRString diagnostic;
		UINT pixels = MulDiv(20,dpi,96);
		CheckIf(!pixels || pixels > 256,E_INVALIDARG);
		images = ImageList_Create(pixels,pixels,ILC_COLOR32 | ILC_MASK,4 + static_cast<INT>(m_palette.Length()),4);
		CheckIf(!images,E_OUTOFMEMORY);
		Check(AddEmbedded(images,IconPointer,sizeof(IconPointer),pixels));
		Check(AddEmbedded(images,IconGenericControl,sizeof(IconGenericControl),pixels));
		Check(AddEmbedded(images,IconGenericComponent,sizeof(IconGenericComponent),pixels));
		Check(AddEmbedded(images,IconUnavailable,sizeof(IconUnavailable),pixels));
		for(sysint n = 0; n < m_palette.Length(); n++)
		{
			TStackRef<IQuadooPaletteIcon> provider;
			INT image = m_palette[n].nonvisual ? 2 : 1;
			HRESULT loaded = m_palette[n].factory->QueryInterface(__uuidof(IQuadooPaletteIcon),reinterpret_cast<PVOID*>(&provider));
			if(SUCCEEDED(loaded) && provider) loaded = provider->GetPaletteIcon(pixels,pixels,&icon);
			if(loaded == S_OK && icon && ValidIcon(icon,pixels))
			{
				image = ImageList_AddIcon(images,icon);
				CheckIf(image < 0,E_OUTOFMEMORY);
			}
			else
			{
				CheckIf(loaded == E_OUTOFMEMORY,loaded);
				if(loaded != S_FALSE)
				{
					diagnostic = static_cast<RSTRING>(NULL);
					Check(RStrFormatW(&diagnostic,L"Palette icon unavailable for %ls (0x%.8x).",RStrToWide(m_palette[n].name),FAILED(loaded) ? loaded : E_INVALIDARG));
				}
			}
			if(icon) { DestroyIcon(icon); icon = NULL; }
			Check(indices.Append(image));
		}
		// Everything that can fail is staged before changing either live list view.
		m_changeDepth++;
		SendMessageW(m_tools,WM_SETREDRAW,FALSE,0);
		SendMessageW(m_tray,WM_SETREDRAW,FALSE,0);
		{
			HIMAGELIST previous = m_images;
			m_images = images; images = NULL;
			ListView_SetImageList(m_tools,m_images,LVSIL_SMALL);
			ListView_SetImageList(m_tray,m_images,LVSIL_SMALL);
			for(sysint n = 0; n < m_palette.Length(); n++) m_palette[n].image = indices[n];
			for(INT row = 0; row < ListView_GetItemCount(m_tools); row++)
			{
				LVITEMW item;
				ZeroMemory(&item,sizeof(item));
				item.mask = LVIF_PARAM;
				item.iItem = row;

				if(ListView_GetItem(m_tools,&item))
				{
					item.mask = LVIF_IMAGE;
					item.iImage = item.lParam > 0 && item.lParam <= m_palette.Length() ? m_palette[item.lParam - 1].image : 0;
					ListView_SetItem(m_tools,&item);
				}
			}
			for(INT row = 0; row < ListView_GetItemCount(m_tray); row++)
			{
				LVITEMW item; ZeroMemory(&item,sizeof(item)); item.mask = LVIF_PARAM; item.iItem = row;
				if(ListView_GetItem(m_tray,&item) && item.lParam >= 0 && item.lParam < m_nodes.Length())
				{
					item.mask = LVIF_IMAGE; item.iImage = ComponentImage(m_nodes[item.lParam].component);
					ListView_SetItem(m_tray,&item);
				}
			}
			if(previous)
				ImageList_Destroy(previous);
		}
		SendMessageW(m_tools,WM_SETREDRAW,TRUE,0);
		SendMessageW(m_tray,WM_SETREDRAW,TRUE,0);
		m_changeDepth--;
		if(RStrLen(diagnostic))
			m_status = diagnostic;
		RedrawWindow(m_tools,NULL,NULL,RDW_INVALIDATE | RDW_ERASE);
		RedrawWindow(m_tray,NULL,NULL,RDW_INVALIDATE | RDW_ERASE);
		hr = S_OK;
	Cleanup:
		if(icon) DestroyIcon(icon);
		if(images) ImageList_Destroy(images);
		return hr;
	}

	INT CDesignerEditor::ComponentImage(IQuadooComponent* component)
	{
		TStackRef<IQuadooDesignPlaceholder> placeholder;
		CRString type;
		if(SUCCEEDED(component->QueryInterface(__uuidof(IQuadooDesignPlaceholder),reinterpret_cast<PVOID*>(&placeholder))))
			return 3;
		if(SUCCEEDED(component->GetTypeName(&type)))
		{
			for(sysint n = 0; n < m_palette.Length(); n++)
				if(m_palette[n].nonvisual && type == m_palette[n].name) return m_palette[n].image;
		}
		return 2;
	}

	VOID CDesignerEditor::ResetTool()
	{
		m_tool = 0;
		m_changeDepth++;
		// Native mouse/keyboard handling can reveal the focused row on a later
		// timer. Reset the tool selection without moving that focus to Pointer.
		ListView_SetItemState(m_tools,-1,0,LVIS_SELECTED);
		ListView_SetItemState(m_tools,0,LVIS_SELECTED,LVIS_SELECTED);
		m_changeDepth--;
	}

	VOID CDesignerEditor::ChooseTool(INT row)
	{
		LVITEMW item;

		if(m_readOnly)
			return;
		ZeroMemory(&item,sizeof(item));
		item.mask = LVIF_PARAM; item.iItem = row;
		if(row < 0 || !ListView_GetItem(m_tools,&item))
			return;

		m_tool = item.lParam > 0 && item.lParam <= m_palette.Length() ? static_cast<INT>(item.lParam) : 0;
		if(m_tool && m_palette[m_tool - 1].nonvisual)
		{
			GUID id;
			HRESULT hr = AddComponent(m_palette[m_tool - 1].name,&id);
			ResetTool();
			if(FAILED(hr))
				Error(hr,L"Add component");
		}
		else
			m_status = RSTRING_CAST(L"Choose a compatible container on the canvas. TabPage belongs to TabControl.");
		Notify();
	}

	LRESULT CALLBACK CDesignerEditor::PaletteProc(HWND window, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data)
	{
		CDesignerEditor* editor = reinterpret_cast<CDesignerEditor*>(data);
		BOOL previous = editor->m_paletteMouse;
		// Resetting selection during mouse-down can make mouse-up select the same
		// nonvisual entry again. Wait for NM_CLICK, after native selection finishes.
		if(msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP || msg == WM_LBUTTONDBLCLK || (msg == WM_MOUSEMOVE && (wp & MK_LBUTTON)))
			editor->m_paletteMouse = TRUE;
		if(msg == WM_NCDESTROY)
			RemoveWindowSubclass(window,PaletteProc,id);
		LRESULT result = DefSubclassProc(window,msg,wp,lp);
		editor->m_paletteMouse = previous;
		return result;
	}
}
