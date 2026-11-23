#include "ListView.h"

namespace QF
{
	HRESULT CListView::VirtualWidth(UINT column, INT* width)
	{
		HRESULT hr;
		TArray<RSTRING> strings;
		INT first = max(0,ListView_GetTopIndex(m_hwnd));
		INT limit = min(128,max(1,ListView_GetCountPerPage(m_hwnd) + 1));
		HDC dc = NULL;
		HGDIOBJ previous = NULL;
		HWND window = m_hwnd;
		INT pixels = 0;
		RECT client;
		GetClientRect(window,&client);
		for(INT n = 0; n < limit && static_cast<UINT>(first + n) < m_virtualCount; n++)
		{
			RECT item;
			RECT visible;
			if(ListView_GetItemRect(window,first + n,&item,LVIR_BOUNDS) && IntersectRect(&visible,&item,&client))
			{
				TStackRef<CListRequest> request;
				CRString text;
				Check(FetchVirtual(first + n,column ? QuadooForm::ListDataSubItems : QuadooForm::ListDataCaption,column,&request));
				if(column)
				{
					Check(request->GetSubItem(column - 1,&text));
				}
				else
				{
					text = request->caption;
				}
				Check(strings.Append(static_cast<RSTRING>(text)));
				RStrAddRef(text);
			}
		}
		dc = GetDC(window);
		CheckIf(!dc,HrEnsureLastError());
		previous = SelectObject(dc,m_font);
		for(sysint n = 0; n < strings.Length(); n++)
		{
			SIZE size;
			CheckIf(!GetTextExtentPoint32W(dc,RStrToWide(strings[n]),RStrLen(strings[n]),&size),HrEnsureLastError());
			pixels = max(pixels,size.cx);
		}
		if(!column)
		{
			const INT roles[] = {LVSIL_SMALL,LVSIL_STATE};
			for(UINT n = 0; n < ARRAYSIZE(roles); n++)
			{
				INT x;
				INT y;
				HIMAGELIST images = ListView_GetImageList(window,roles[n]);
				if(images && ImageList_GetIconSize(images,&x,&y))
				{
					pixels += x;
				}
			}
		}
		*width = max(16,MulDiv(pixels,96,m_dpi) + 16);
		hr = S_OK;
	Cleanup:
		if(dc)
		{
			SelectObject(dc,previous);
			ReleaseDC(window,dc);
		}
		for(sysint n = 0; n < strings.Length(); n++)
		{
			RStrRelease(strings[n]);
		}
		return hr;
	}
	HRESULT CListView::VirtualNative()
	{
		HRESULT hr;
		TStackRef<CListView> keepAlive(this);
		TStackRef<IQuadooImageListSnapshot> previous[3];
		TStackRef<IQuadooImageListSnapshot> derived[3];
		TArray<INT> order;
		TArray<INT> oldOrder;
		HIMAGELIST oldImages[3];
		UINT dpi = m_nativeDPI;
		UINT imageStyle = m_virtualNativeStyle;
		DWORD style = GetWindowLongW(m_hwnd,GWL_STYLE);
		DWORD extended = ListView_GetExtendedListViewStyle(m_hwnd);
		DWORD desired = 0;
		DWORD unused = 0;
		PCWSTR className = NULL;
		INT oldCount = Header_GetItemCount(ListView_GetHeader(m_hwnd));
		INT inserted = 0;
		BOOL committed = FALSE;
		BOOL redraw = !m_updates && (style & WS_VISIBLE);
		for(UINT n = 0; n < 3; n++)
		{
			previous[n] = m_images[n].snapshot;
			derived[n] = m_virtualImages[n];
			oldImages[n] = ListView_GetImageList(m_hwnd,n == 0 ? LVSIL_NORMAL : n == 1 ? LVSIL_SMALL : LVSIL_STATE);
		}
		m_busy++;
		m_nativeSync++;
		m_replacing = TRUE;
		if(redraw)
		{
			SendMessageW(m_hwnd,WM_SETREDRAW,FALSE,0);
		}
		CheckIf(oldCount < 0 || oldCount > 4096,E_UNEXPECTED);
		for(INT n = 0; n < oldCount; n++)
		{
			Check(oldOrder.Append(0));
		}
		CheckIf(oldCount && !ListView_GetColumnOrderArray(m_hwnd,oldCount,&oldOrder[0]),E_FAIL);
		for(sysint n = 0; n < columns.Length() + oldCount; n++)
		{
			Check(order.Append(0));
		}
		Check(RefreshImages());
		// Allocate all new native columns while the old columns still exist.
		// Failure removes only the candidates; old captions/widths remain owned
		// by the HWND. Virtual row state never moves to another HWND.
		for(sysint n = 0; n < columns.Length(); n++)
		{
			LVCOLUMNW column = {0};
			static const INT align[] = {LVCFMT_LEFT,LVCFMT_CENTER,LVCFMT_RIGHT};
			column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT | LVCF_SUBITEM;
			column.pszText = const_cast<PWSTR>(columns[n]->caption.c_wstr());
			column.cx = MulDiv(columns[n]->width,m_dpi,96);
			column.fmt = align[columns[n]->alignment];
			column.iSubItem = static_cast<INT>(n);
			CheckIf(SendMessageW(m_hwnd,LVM_INSERTCOLUMNW,oldCount + n,reinterpret_cast<LPARAM>(&column)) != oldCount + n,E_OUTOFMEMORY);
			inserted++;
			order[columns[n]->display] = oldCount + static_cast<INT>(n);
		}
		for(INT n = 0; n < oldCount; n++)
		{
			order[columns.Length() + n] = oldOrder[n];
		}
		CheckIf(order.Length() && !ListView_SetColumnOrderArray(m_hwnd,static_cast<INT>(order.Length()),&order[0]),E_OUTOFMEMORY);
		NativeStyle(&className,&desired,&unused);
		SetLastError(0);
		CheckIf(!SetWindowLongW(m_hwnd,GWL_STYLE,(style & 0xffff0000) | (desired & 0xffff)) && GetLastError(),HrEnsureLastError());
		desired = (Boolean(L"FullRowSelect") ? LVS_EX_FULLROWSELECT : 0) |
			(Boolean(L"GridLines") ? LVS_EX_GRIDLINES : 0) |
			(Boolean(L"ColumnReorder") ? LVS_EX_HEADERDRAGDROP : 0);
		ListView_SetExtendedListViewStyle(m_hwnd,desired);
		CheckIf(!SetWindowPos(m_hwnd,NULL,0,0,0,0,SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER |
			SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOREDRAW),HrEnsureLastError());
		// These validated deletions do not allocate. The allocation/failure
		// boundary above leaves the complete old header available for rollback.
		for(INT n = oldCount - 1; n >= 0; n--)
		{
			ListView_DeleteColumn(m_hwnd,n);
		}
		inserted = 0;
		committed = TRUE;
		for(sysint n = 0; n < columns.Length(); n++)
		{
			LVCOLUMNW column = {0};
			column.mask = LVCF_SUBITEM;
			column.iSubItem = static_cast<INT>(n);
			SendMessageW(m_hwnd,LVM_SETCOLUMNW,n,reinterpret_cast<LPARAM>(&column));
		}
		if(!Boolean(L"MultiSelect") && ListView_GetSelectedCount(m_hwnd) > 1)
		{
			INT chosen = ListView_GetNextItem(m_hwnd,-1,LVNI_FOCUSED | LVNI_SELECTED);
			if(chosen < 0)
			{
				chosen = ListView_GetNextItem(m_hwnd,-1,LVNI_SELECTED);
			}
			ListView_SetItemState(m_hwnd,-1,0,LVIS_SELECTED);
			if(chosen >= 0)
			{
				ListView_SetItemState(m_hwnd,chosen,LVIS_SELECTED,LVIS_SELECTED);
			}
		}
		m_nativeDPI = m_dpi;
		hr = S_OK;
	Cleanup:
		if(!committed)
		{
			for(INT n = inserted - 1; n >= 0; n--)
			{
				ListView_DeleteColumn(m_hwnd,oldCount + n);
			}
			SetWindowLongW(m_hwnd,GWL_STYLE,style);
			ListView_SetExtendedListViewStyle(m_hwnd,extended);
			for(UINT n = 0; n < 3; n++)
			{
				m_images[n].snapshot = previous[n];
			}
			IQuadooImageListSnapshot* restore[3] = {derived[0],derived[1],derived[2]};
			CommitVirtualImages(restore,oldImages);
			m_nativeDPI = dpi;
			m_virtualNativeStyle = imageStyle;
		}
		m_replacing = FALSE;
		m_nativeSync--;
		m_busy--;
		if(redraw)
		{
			SendMessageW(m_hwnd,WM_SETREDRAW,TRUE,0);
			if(!Boolean(L"Visible"))
			{
				ShowWindow(m_hwnd,SW_HIDE);
			}
			InvalidateRect(m_hwnd,NULL,TRUE);
		}
		return hr;
	}
}
