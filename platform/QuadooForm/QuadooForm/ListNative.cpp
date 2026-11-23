#include "ListView.h"

namespace QF
{
	static const UINT ReadHeaderMessage = WM_APP + 0x349;
	HRESULT CListView::PopulateColumns()
	{
		HRESULT hr;
		TArray<INT> order;
		INT existing = Header_GetItemCount(ListView_GetHeader(m_hwnd));
		CheckIf(existing && existing != columns.Length(),E_UNEXPECTED);
		if(columns.Length())
		{
			Check(order.Reserve(columns.Length()));
		}
		for(sysint n = 0; n < columns.Length(); n++)
		{
			Check(order.Append(0));
		}
		for(sysint n = 0; n < columns.Length(); n++)
		{
			CListColumn* column = columns[n];
			LVCOLUMNW native = {0};
			static const INT alignment[] = {LVCFMT_LEFT,LVCFMT_CENTER,LVCFMT_RIGHT};
			native.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT | LVCF_SUBITEM;
			native.pszText = const_cast<PWSTR>(column->caption.c_wstr());
			// Column/model/JSON widths are logical pixels at 96 DPI. Scale only
			// at this HWND boundary, including initial Report-view creation.
			native.cx = MulDiv(column->width,m_dpi,96);
			native.fmt = alignment[column->alignment];
			native.iSubItem = static_cast<INT>(n);
			if(existing)
			{
				CheckIf(!SendMessageW(m_hwnd,LVM_SETCOLUMNW,n,reinterpret_cast<LPARAM>(&native)),E_FAIL);
			}
			else
			{
				CheckIf(SendMessageW(m_hwnd,LVM_INSERTCOLUMNW,n,reinterpret_cast<LPARAM>(&native)) != n,E_OUTOFMEMORY);
			}
			CheckIf(column->display >= static_cast<UINT>(order.Length()),E_UNEXPECTED);
			order[column->display] = static_cast<INT>(n);
		}
		CheckIf(order.Length() && !ListView_SetColumnOrderArray(m_hwnd,static_cast<INT>(order.Length()),&order[0]),E_FAIL);
		hr = S_OK;
	Cleanup:
		return hr;
	}
	HRESULT CListView::ReadNativeColumns()
	{
		HRESULT hr;
		TArray<INT> order;
		TArray<INT> widths;
		BOOL changed = FALSE;
		Check(CheckAlive());
		CheckIf(m_busy || m_events,QuadooForm::EInvalidState);
		if(columns.Length())
		{
			Check(order.Reserve(columns.Length()));
			Check(widths.Reserve(columns.Length()));
		}
		for(sysint n = 0; n < columns.Length(); n++)
		{
			Check(order.Append(-1));
			INT physical = ListView_GetColumnWidth(m_hwnd,n);
			INT logical = physical == MulDiv(columns[n]->width,m_nativeDPI,96) ? columns[n]->width : max(0,MulDiv(physical,96,m_nativeDPI));
			Check(widths.Append(logical));
		}
		CheckIf(order.Length() && !ListView_GetColumnOrderArray(m_hwnd,static_cast<INT>(order.Length()),&order[0]),E_FAIL);
		for(sysint n = 0; n < order.Length(); n++)
		{
			CheckIf(order[n] < 0 || order[n] >= columns.Length(),E_UNEXPECTED);
			for(sysint previous = 0; previous < n; previous++)
			{
				CheckIf(order[previous] == order[n],E_UNEXPECTED);
			}
		}
		for(sysint n = 0; n < columns.Length(); n++)
		{
			if(columns[n]->width != widths[n] || columns[order[n]]->display != static_cast<UINT>(n))
			{
				changed = TRUE;
			}
			columns[n]->width = widths[n];
			columns[order[n]]->display = static_cast<UINT>(n);
		}
		m_headerPending = FALSE;
		if(changed)
		{
			Check(NotifyColumns());
		}
	Cleanup:
		return hr;
	}
	HRESULT CListView::AutoSizeColumn(CListColumn* column, QuadooForm::ListColumnAutoSize mode)
	{
		HRESULT hr;
		UINT index;
		HDC dc = NULL;
		HGDIOBJ previous = NULL;
		SIZE size = {0};
		Check(Mutable());
		Check(ResolveColumn(column,&index));
		CheckIf(mode != QuadooForm::ListSizeToHeader && mode != QuadooForm::ListSizeToContent,E_INVALIDARG);
		if(IsVirtual() && mode == QuadooForm::ListSizeToContent)
		{
			INT width;
			Check(VirtualWidth(index,&width));
			Check(EditColumn(column,1,NULL,width));
			goto Cleanup;
		}
		if(mode == QuadooForm::ListSizeToHeader || rows.Length())
		{
			dc = GetDC(m_hwnd);
			CheckIf(!dc,HrEnsureLastError());
			previous = SelectObject(dc,m_font);
			if(mode == QuadooForm::ListSizeToHeader)
			{
				CheckIf(!GetTextExtentPoint32W(dc,column->caption.c_wstr(),RStrLen(column->caption),&size),HrEnsureLastError());
			}
			else
			{
				for(sysint row = 0; row < rows.Length(); row++)
				{
					CRString text;
					SIZE measured;
					if(index)
					{
						Check(rows[row]->GetSubItem(index - 1,&text));
					}
					else
					{
						text = rows[row]->caption;
					}
					CheckIf(!GetTextExtentPoint32W(dc,text.c_wstr(),RStrLen(text),&measured),HrEnsureLastError());
					size.cx = max(size.cx,measured.cx);
				}
				if(!index)
				{
					INT width;
					INT height;
					HIMAGELIST smallImages = ListView_GetImageList(m_hwnd,LVSIL_SMALL);
					HIMAGELIST state = ListView_GetImageList(m_hwnd,LVSIL_STATE);
					BOOL hasImage = FALSE;
					BOOL hasState = FALSE;
					for(sysint row = 0; row < rows.Length(); row++)
					{
						hasImage = hasImage || (smallImages && rows[row]->image >= 0 && rows[row]->image < ImageList_GetImageCount(smallImages));
						hasState = hasState || rows[row]->state >= 0;
					}
					if(hasImage && ImageList_GetIconSize(smallImages,&width,&height))
					{
						size.cx += width;
					}
					if(hasState && state && ImageList_GetIconSize(state,&width,&height))
					{
						size.cx += width;
					}
				}
			}
		}
		// An empty view has no content to measure yet. Use a small nonzero cell.
		if(dc)
		{
			SelectObject(dc,previous);
			ReleaseDC(m_hwnd,dc);
			dc = NULL;
		}
		Check(EditColumn(column,1,NULL,max(16,MulDiv(size.cx,96,m_dpi) + 16)));
	Cleanup:
		if(dc)
		{
			if(previous)
			{
				SelectObject(dc,previous);
			}
			ReleaseDC(m_hwnd,dc);
		}
		return hr;
	}
	HRESULT CListView::ColumnClick(UINT index)
	{
		HRESULT hr = S_OK;
		TStackRef<CListView> keepAlive(this);
		TStackRef<CListColumn> column;
		QuadooForm::ContextMode mode;
		BOOL entered = FALSE;
		CheckIfIgnore(m_busy || m_events || m_state != QuadooForm::Ready || !Boolean(L"ColumnClick"),S_OK);
		Check(m_context->GetMode(&mode));
		CheckIfIgnore(mode == QuadooForm::Design,S_OK);
		CheckIf(index >= static_cast<UINT>(columns.Length()),E_INVALIDARG);
		column = columns[index];
		m_events++;
		entered = TRUE;
		for(sysint n = 0; n < m_bindings.Length(); n++)
		{
			if(!lstrcmpW(m_bindings[n].spec->name,L"OnColumnClick") && m_bindings[n].delegate)
			{
				TStackRef<IQuadooDelegate> delegate(m_bindings[n].delegate);
				TStackRef<CEventArgs> args;
				args.Attach(__new CEventArgs(m_bindings[n].spec,Control()));
				CheckAlloc(args);
				Check(args->Initialize());
				args->SetNativeObject(0,static_cast<IQuadooListColumn*>(column));
				args->SetNativeValue(1,index);
				Check(delegate->Invoke(args));
				Check(CheckAlive());
				break;
			}
		}
	Cleanup:
		if(entered)
		{
			m_events--;
		}
		return hr;
	}
	HRESULT CListView::ContextPopup(POINT screen, HWND origin)
	{
		HRESULT hr;
		TStackRef<CListView> keepAlive(this);
		QuadooForm::ContextMode mode;
		BOOL handled = FALSE;
		BOOL entered = FALSE;
		RECT header;
		TStackRef<IQuadooListItem> item;
		INT column = -1;
		Check(CheckAlive());
		Check(m_context->GetMode(&mode));
		CheckIfIgnore(mode == QuadooForm::Design || m_busy || m_events,S_FALSE);
		// A header context request is deliberately separate from row popups.
		CheckIfIgnore(origin == ListView_GetHeader(m_hwnd),S_FALSE);
		if(screen.x == -1 && screen.y == -1)
		{
			RECT client;
			GetClientRect(m_hwnd,&client);
			screen.x = 0;
			screen.y = 0;
			if(Integer(L"ViewStyle") == QuadooForm::ListViewReport && Boolean(L"ShowColumnHeaders") &&
				GetWindowRect(ListView_GetHeader(m_hwnd),&header))
			{
				screen.y = min(max(0,header.bottom - header.top),max(0,client.bottom - 1));
			}
			Check(GetFocused(&item));
			if(item)
			{
				UINT index;
				Check(item->GetItemIndex(&index));
				RECT bounds;
				if(ListView_GetItemRect(m_hwnd,index,&bounds,LVIR_BOUNDS))
				{
					screen.x = max(0,min(bounds.left,max(0,client.right - 1)));
					screen.y = max(screen.y,min(bounds.bottom,max(0,client.bottom - 1)));
					column = 0;
				}
			}
			ClientToScreen(m_hwnd,&screen);
		}
		else if(Integer(L"ViewStyle") == QuadooForm::ListViewReport && Boolean(L"ShowColumnHeaders") &&
			GetWindowRect(ListView_GetHeader(m_hwnd),&header) && PtInRect(&header,screen))
		{
			hr = S_FALSE;
			goto Cleanup;
		}
		else
		{
			LVHITTESTINFO hit = {0};
			hit.pt = screen;
			ScreenToClient(m_hwnd,&hit.pt);
			INT index = Integer(L"ViewStyle") == QuadooForm::ListViewReport ?
				ListView_SubItemHitTest(m_hwnd,&hit) : ListView_HitTest(m_hwnd,&hit);
			UINT count;
			Check(ItemCount(&count));
			if(index >= 0 && static_cast<UINT>(index) < count)
			{
				if(IsVirtual())
				{
					TStackRef<CListRequest> request;
					Check(FetchVirtual(index,31,max(0,hit.iSubItem),&request));
					item.Attach(request.Detach());
				}
				else
				{
					item = rows[index];
				}
				column = hit.iSubItem;
				if(!(ListView_GetItemState(m_hwnd,index,LVIS_SELECTED) & LVIS_SELECTED))
				{
					Check(SetSelected(item));
					CheckIfIgnore(!(ListView_GetItemState(m_hwnd,index,LVIS_SELECTED) & LVIS_SELECTED),S_FALSE);
					Check(SetFocused(item));
					CheckIfIgnore(!(ListView_GetItemState(m_hwnd,index,LVIS_FOCUSED) & LVIS_FOCUSED),S_FALSE);
					if(IsVirtual())
					{
						TStackRef<CListRequest> request;
						Check(FetchVirtual(index,31,max(0,hit.iSubItem),&request));
						item.Attach(request.Detach());
					}
				}
			}
		}
		m_events++;
		entered = TRUE;
		for(sysint n = 0; n < m_bindings.Length(); n++)
		{
			if(!lstrcmpW(m_bindings[n].spec->name,L"OnContextPopup") && m_bindings[n].delegate)
			{
				TStackRef<IQuadooDelegate> delegate(m_bindings[n].delegate);
				TStackRef<CEventArgs> args;
				args.Attach(__new CEventArgs(m_bindings[n].spec,Control()));
				CheckAlloc(args);
				Check(args->Initialize());
				args->SetNativeObject(0,static_cast<IQuadooListItem*>(item));
				args->SetNativeValue(1,column);
				args->SetNativeValue(2,screen.x);
				args->SetNativeValue(3,screen.y);
				Check(delegate->Invoke(args));
				Check(args->GetBool(4,&handled));
				Check(CheckAlive());
				break;
			}
		}
		m_events--;
		entered = FALSE;
		if(!handled)
		{
			Check(ShowControlPopup(Control(),screen,&handled));
		}
		hr = S_OK;
	Cleanup:
		if(entered)
		{
			m_events--;
		}
		return hr;
	}
	STDMETHODIMP CListView::OwnsKeyboardMessage(const MSG* message, BOOL* owns)
	{
		HRESULT hr;
		CheckIf(!owns,E_POINTER);
		*owns = FALSE;
		CheckIf(!message,E_INVALIDARG);
		Check(CheckAlive());
		// The native label editor commits/cancels through its parent. Leave its
		// keys, characters and shortcuts to native dispatch while it exists.
		*owns = message->message >= WM_KEYFIRST && message->message <= WM_KEYLAST &&
			message->hwnd && message->hwnd == ListView_GetEditControl(m_hwnd);
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListView::HandleMessage(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result)
	{
		HRESULT hr;
		CheckIf(!handled || !result,E_POINTER);
		*handled = FALSE;
		*result = 0;
		Check(CheckAlive());
		if(window == m_hwnd && message == WM_NOTIFY)
		{
			*result = Message(window,message,wp,lp,FALSE,handled);
			hr = S_OK;
		}
		else
		{
			Check(Super::HandleMessage(window,message,wp,lp,handled,result));
		}
	Cleanup:
		return hr;
	}
	LRESULT CListView::Message(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled)
	{
		if(m_events && !m_nativeSync && (message == LVM_INSERTITEMW || message == LVM_DELETEITEM ||
			message == LVM_DELETEALLITEMS || message == LVM_SETITEMW || message == LVM_SETITEMSTATE ||
			message == LVM_SORTITEMS || message == LVM_EDITLABELW || message == LVM_SETITEMCOUNT ||
			message == LVM_INSERTCOLUMNW || message == LVM_DELETECOLUMN || message == LVM_SETCOLUMNW ||
			message == LVM_SETCOLUMNORDERARRAY))
		{
			*handled = TRUE;
			return message == LVM_INSERTITEMW || message == LVM_INSERTCOLUMNW ? -1 : FALSE;
		}
		if(message == WM_DESTROY)
		{
			if(window == m_hwnd)
			{
				m_editing = FALSE;
			}
			// LVS_SHAREIMAGELISTS leaves every role owned by its snapshot, except
			// the checkbox state list generated by this particular HWND.
			HIMAGELIST checks = reinterpret_cast<HIMAGELIST>(RemovePropW(window,L"QuadooForm.ListView.Checks"));
			ListView_SetImageList(window,NULL,LVSIL_NORMAL);
			ListView_SetImageList(window,NULL,LVSIL_SMALL);
			ListView_SetImageList(window,NULL,LVSIL_STATE);
			if(checks)
			{
				ImageList_Destroy(checks);
			}
		}
		if(message == ReadHeaderMessage)
		{
			*handled = TRUE;
			if(m_headerPending && !m_busy && !m_events)
			{
				ReportCallback(ReadNativeColumns(),L"Columns");
			}
			return 0;
		}
		if(message == WM_CONTEXTMENU && m_state == QuadooForm::Ready)
		{
			POINT screen = {static_cast<SHORT>(LOWORD(lp)),static_cast<SHORT>(HIWORD(lp))};
			*handled = TRUE;
			HRESULT popup = ContextPopup(screen,reinterpret_cast<HWND>(wp));
			if(IsVirtual())
			{
				ReportVirtualError(popup,L"PopupMenu");
			}
			else
			{
				ReportCallback(popup,L"PopupMenu");
			}
			return 0;
		}
		if(message == WM_MOUSEWHEEL)
		{
			*handled = FALSE;
			return 0;
		}
		if(message == WM_KEYDOWN && wp == VK_SPACE && Boolean(L"CheckBoxes"))
		{
			LRESULT keyboard = Super::Message(window,message,wp,lp,content,handled);
			if(*handled)
			{
				return keyboard;
			}
			QuadooForm::ContextMode mode;
			if(FAILED(m_context->GetMode(&mode)) || mode == QuadooForm::Design)
			{
				*handled = TRUE;
				return 0;
			}
			if(IsVirtual())
			{
				INT index = ListView_GetNextItem(window,-1,LVNI_FOCUSED);
				if(index >= 0)
				{
					ReportVirtualError(VirtualCheckGesture(index,TRUE),L"OnDataUpdate");
				}
				*handled = TRUE;
				return 0;
			}
			TStackRef<IQuadooListItem> focused;
			if(SUCCEEDED(GetFocused(&focused)) && focused)
			{
				UINT index;
				if(SUCCEEDED(ResolveItem(focused,&index)))
				{
					ReportCallback(CheckItems(rows[index]),L"OnItemChecking");
				}
			}
			*handled = TRUE;
			return 0;
		}
		if(message == WM_SYSCOLORCHANGE)
		{
			SendMessageW(window,LVM_SETBKCOLOR,0,ResolveQuadooColor(Integer(L"BackColor")));
			SendMessageW(window,LVM_SETTEXTBKCOLOR,0,ResolveQuadooColor(Integer(L"BackColor")));
			SendMessageW(window,LVM_SETTEXTCOLOR,0,ResolveQuadooColor(Integer(L"ForeColor")));
		}
		if(IsVirtual() && (message == WM_THEMECHANGED || message == WM_SYSCOLORCHANGE))
		{
			ReportVirtualError(RefreshVirtualImages(),L"Check images");
		}
		if(IsVirtual() && Boolean(L"CheckBoxes") && (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK))
		{
			QuadooForm::ContextMode mode;
			POINT point = {static_cast<SHORT>(LOWORD(lp)),static_cast<SHORT>(HIWORD(lp))};
			INT index;
			if(SUCCEEDED(m_context->GetMode(&mode)) && mode != QuadooForm::Design && VirtualCheckHit(point,&index))
			{
				ReportVirtualError(VirtualCheckGesture(index,FALSE),L"OnDataUpdate");
				*handled = TRUE;
				return 0;
			}
		}
		if(message == WM_NOTIFY && lp)
		{
			NMHDR* header = reinterpret_cast<NMHDR*>(lp);
			if(header->hwndFrom == window)
			{
				LRESULT value = ItemNotification(header,handled);
				if(*handled)
				{
					return value;
				}
			}
			if(header->hwndFrom == window && header->code == LVN_COLUMNCLICK)
			{
				*handled = TRUE;
				if(!m_replacing)
				{
					ReportCallback(ColumnClick(reinterpret_cast<NMLISTVIEW*>(lp)->iSubItem),L"OnColumnClick");
				}
				return 0;
			}
			if(header->hwndFrom == ListView_GetHeader(window) && !m_busy && !m_replacing &&
				(header->code == HDN_ENDTRACKW || header->code == HDN_ENDTRACKA || header->code == HDN_ENDDRAG))
			{
				m_headerPending = TRUE;
				PostMessageW(window,ReadHeaderMessage,0,0);
			}
		}
		if(message == WM_KEYDOWN && wp == VK_F2 && !m_busy && !m_events)
		{
			LRESULT keyboard = Super::Message(window,message,wp,lp,content,handled);
			if(*handled)
			{
				return keyboard;
			}
			QuadooForm::ContextMode mode;
			if(FAILED(m_context->GetMode(&mode)) || mode == QuadooForm::Design)
			{
				*handled = TRUE;
				return 0;
			}
			TStackRef<IQuadooListItem> item;
			if(SUCCEEDED(GetFocused(&item)) && item && !Boolean(L"ReadOnly"))
			{
				ReportCallback(BeginLabelEdit(item),L"OnEditing");
				*handled = TRUE;
				return 0;
			}
			return keyboard;
		}
		return Super::Message(window,message,wp,lp,content,handled);
	}
}
