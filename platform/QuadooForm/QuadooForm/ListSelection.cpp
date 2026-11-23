#include "ListView.h"

namespace QF
{
	STDMETHODIMP CListView::GetSelected(IQuadooListItem** value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		Check(CheckAlive());
		if(IsVirtual())
		{
			INT index = ListView_GetNextItem(m_hwnd,-1,LVNI_SELECTED | LVNI_FOCUSED);
			if(index < 0)
			{
				index = ListView_GetNextItem(m_hwnd,-1,LVNI_SELECTED);
			}
			if(index >= 0)
			{
				TStackRef<CListRequest> request;
				Check(FetchVirtual(index,31,0,&request));
				*value = request.Detach();
			}
			goto Cleanup;
		}
		Check(Stored());
		for(sysint n = 0; n < rows.Length(); n++)
		{
			if(rows[n]->selected && (!*value || rows[n]->focused))
			{
				*value = rows[n];
				if(rows[n]->focused)
				{
					break;
				}
			}
		}
		if(*value)
		{
			(*value)->AddRef();
		}
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListView::GetFocused(IQuadooListItem** value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		Check(CheckAlive());
		if(IsVirtual())
		{
			INT index = ListView_GetNextItem(m_hwnd,-1,LVNI_FOCUSED);
			if(index >= 0)
			{
				TStackRef<CListRequest> request;
				Check(FetchVirtual(index,31,0,&request));
				*value = request.Detach();
			}
			goto Cleanup;
		}
		Check(Stored());
		for(sysint n = 0; n < rows.Length(); n++)
		{
			if(rows[n]->focused)
			{
				*value = rows[n];
				(*value)->AddRef();
				break;
			}
		}
	Cleanup:
		return hr;
	}
	HRESULT CListView::SelectStates(CListItem* item, BOOL value, BOOL focus, BOOL exclusive)
	{
		HRESULT hr = S_OK;
		TStackRef<CListView> keepAlive(this);
		TArray<UINT> previous;
		TArray<UINT> next;
		BOOL changed = FALSE;
		BOOL entered = FALSE;
		Check(Mutable());
		m_busy++;
		entered = TRUE;
		Check(Stored());
		for(sysint n = 0; n < rows.Length(); n++)
		{
			CListItem* row = rows[n];
			UINT oldState = (row->selected ? LVIS_SELECTED : 0) | (row->focused ? LVIS_FOCUSED : 0);
			UINT newState = oldState;
			if(focus)
			{
				newState = (newState & ~LVIS_FOCUSED) | (row == item && value ? LVIS_FOCUSED : 0);
			}
			else if(row == item)
			{
				newState = (newState & ~LVIS_SELECTED) | (value ? LVIS_SELECTED : 0);
			}
			else if(exclusive || (value && !Boolean(L"MultiSelect")))
			{
				newState &= ~LVIS_SELECTED;
			}
			Check(previous.Append(oldState));
			Check(next.Append(newState));
			if(newState != oldState)
			{
				BOOL cancel;
				Check(Changing(row,focus ? QuadooForm::ListChangeFocused : QuadooForm::ListChangeSelected,oldState,newState,&cancel));
				CheckIfIgnore(cancel,S_FALSE);
				changed = TRUE;
			}
		}
		CheckIfIgnore(!changed,S_FALSE);
		for(sysint n = 0; n < rows.Length(); n++)
		{
			rows[n]->selected = (next[n] & LVIS_SELECTED) != 0;
			rows[n]->focused = (next[n] & LVIS_FOCUSED) != 0;
		}
		for(sysint n = 0; n < rows.Length(); n++)
		{
			if(next[n] != previous[n])
			{
				hr = ApplyStates(rows[n]);
				if(FAILED(hr))
				{
					for(sysint restore = 0; restore < rows.Length(); restore++)
					{
						rows[restore]->selected = (previous[restore] & LVIS_SELECTED) != 0;
						rows[restore]->focused = (previous[restore] & LVIS_FOCUSED) != 0;
						ApplyStates(rows[restore]);
					}
					goto Cleanup;
				}
			}
		}
		for(sysint n = 0; n < rows.Length(); n++)
		{
			if(next[n] != previous[n])
			{
				Check(ItemEvent(L"OnChange",rows[n],focus ? QuadooForm::ListChangeFocused : QuadooForm::ListChangeSelected));
				if(!focus)
				{
					Check(ItemEvent(L"OnSelectItem",rows[n],rows[n]->selected));
				}
			}
		}
	Cleanup:
		if(entered)
		{
			m_busy--;
		}
		return hr;
	}
	STDMETHODIMP CListView::SetSelected(IQuadooListItem* value)
	{
		HRESULT hr;
		UINT index = 0;
		Check(Mutable());
		Check(CheckAlive());
		if(IsVirtual())
		{
			if(value)
			{
				Check(VirtualItemIndex(value,&index));
			}
			Check(VirtualSelection(value ? static_cast<INT>(index) : -1,TRUE,FALSE,TRUE));
			goto Cleanup;
		}
		Check(Stored());
		if(value)
		{
			Check(ResolveItem(value,&index));
		}
		Check(SelectStates(value ? rows[index] : NULL,TRUE,FALSE,TRUE));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListView::SetFocused(IQuadooListItem* value)
	{
		HRESULT hr;
		UINT index = 0;
		Check(Mutable());
		Check(CheckAlive());
		if(IsVirtual())
		{
			if(value)
			{
				Check(VirtualItemIndex(value,&index));
			}
			Check(VirtualSelection(value ? static_cast<INT>(index) : -1,TRUE,TRUE,TRUE));
			goto Cleanup;
		}
		Check(Stored());
		if(value)
		{
			Check(ResolveItem(value,&index));
		}
		Check(SelectStates(value ? rows[index] : NULL,TRUE,TRUE,TRUE));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListView::SetItemSelected(UINT index, BOOL selected)
	{
		HRESULT hr;
		Check(Mutable());
		Check(CheckAlive());
		if(IsVirtual())
		{
			CheckIf(index >= m_virtualCount,E_INVALIDARG);
			Check(VirtualSelection(index,selected,FALSE,FALSE));
			goto Cleanup;
		}
		Check(Stored());
		CheckIf(index >= static_cast<UINT>(rows.Length()),E_INVALIDARG);
		Check(SelectStates(rows[index],selected,FALSE,FALSE));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListView::GetSelectedCount(UINT* value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = 0;
		Check(CheckAlive());
		if(IsVirtual())
		{
			*value = ListView_GetSelectedCount(m_hwnd);
			hr = S_OK;
			goto Cleanup;
		}
		Check(Stored());
		for(sysint n = 0; n < rows.Length(); n++)
		{
			if(rows[n]->selected)
			{
				(*value)++;
			}
		}
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListView::GetNextSelected(INT after, IQuadooListItem** value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		Check(CheckAlive());
		if(IsVirtual())
		{
			CheckIf(after < -1 || (after >= 0 && static_cast<UINT>(after) >= m_virtualCount),E_INVALIDARG);
			INT index = ListView_GetNextItem(m_hwnd,after,LVNI_SELECTED);
			if(index >= 0)
			{
				TStackRef<CListRequest> request;
				Check(FetchVirtual(index,31,0,&request));
				*value = request.Detach();
				hr = S_OK;
			}
			else
			{
				hr = S_FALSE;
			}
			goto Cleanup;
		}
		Check(Stored());
		CheckIf(after < -1 || (after >= rows.Length() && after != -1),E_INVALIDARG);
		hr = S_FALSE;
		for(sysint n = after + 1; n < rows.Length(); n++)
		{
			if(rows[n]->selected)
			{
				*value = rows[n];
				(*value)->AddRef();
				hr = S_OK;
				break;
			}
		}
	Cleanup:
		return hr;
	}
	HRESULT CListView::CompareItems(CListItem* a, CListItem* b, UINT column, INT* value)
	{
		HRESULT hr;
		TStackRef<CEventArgs> args;
		CRString first;
		CRString second;
		Check(NewEvent(L"OnCompare",&args));
		if(args)
		{
			args->SetNativeObject(0,static_cast<IQuadooListItem*>(a));
			args->SetNativeObject(1,static_cast<IQuadooListItem*>(b));
			args->SetNativeValue(2,column);
			Check(InvokeEvent(args));
			Check(args->GetInt(3,value));
		}
		else
		{
			if(column)
			{
				Check(a->GetSubItem(column - 1,&first));
				Check(b->GetSubItem(column - 1,&second));
			}
			else
			{
				first = a->caption;
				second = b->caption;
			}
			*value = lstrcmpiW(first.c_wstr(),second.c_wstr());
		}
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListView::Sort(UINT column, BOOL ascending)
	{
		HRESULT hr;
		TStackRef<CListView> keepAlive(this);
		TArray<CListItem*> order;
		TArray<CListItem*> scratch;
		BOOL entered = FALSE;
		Check(Mutable());
		m_busy++;
		entered = TRUE;
		Check(Stored());
		CheckIf(column >= 4097,E_INVALIDARG);
		for(sysint n = 0; n < rows.Length(); n++)
		{
			Check(order.Append(rows[n]));
			Check(scratch.Append(static_cast<CListItem*>(NULL)));
		}
		// Stable bottom-up merge sort; callbacks precede the native commit.
		for(sysint width = 1; width < order.Length(); width *= 2)
		{
			for(sysint start = 0; start < order.Length(); start += width * 2)
			{
				sysint mid = min(start + width,order.Length());
				sysint end = min(start + width * 2,order.Length());
				sysint left = start;
				sysint right = mid;
				for(sysint out = start; out < end; out++)
				{
					INT compare = 0;
					if(left < mid && right < end)
					{
						Check(CompareItems(order[left],order[right],column,&compare));
					}
					if(left < mid && (right >= end || (ascending ? compare <= 0 : compare >= 0)))
					{
						scratch[out] = order[left++];
					}
					else
					{
						scratch[out] = order[right++];
					}
				}
			}
			order.Swap(scratch);
		}
		Check(ApplyOrder(order));
	Cleanup:
		if(entered)
		{
			m_busy--;
		}
		return hr;
	}
	STDMETHODIMP CListView::BeginLabelEdit(IQuadooListItem* value)
	{
		HRESULT hr;
		UINT index;
		QuadooForm::ContextMode mode;
		Check(Mutable());
		if(IsVirtual())
		{
			Check(VirtualItemIndex(value,&index));
		}
		else
		{
			Check(Stored());
			Check(ResolveItem(value,&index));
		}
		Check(m_context->GetMode(&mode));
		CheckIf(mode == QuadooForm::Design || Boolean(L"ReadOnly") || !IsWindowEnabled(m_hwnd) ||
			(IsVirtual() && !Bound(L"OnDataUpdate")),QuadooForm::EInvalidState);
		::SetFocus(m_hwnd);
		CheckIfIgnore(!ListView_EditLabel(m_hwnd,index),S_FALSE);
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListView::EnsureVisible(IQuadooListItem* value, BOOL partialOK)
	{
		HRESULT hr;
		UINT index;
		Check(CheckAlive());
		if(IsVirtual())
		{
			Check(VirtualItemIndex(value,&index));
		}
		else
		{
			Check(ResolveItem(value,&index));
		}
		CheckIf(!ListView_EnsureVisible(m_hwnd,index,partialOK),E_FAIL);
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListView::RefreshRange(UINT first, UINT last)
	{
		HRESULT hr;
		Check(Mutable());
		Check(CheckAlive());
		CheckIf(first > last || last >= (IsVirtual() ? m_virtualCount : static_cast<UINT>(rows.Length())),E_INVALIDARG);
		CheckIf(!ListView_RedrawItems(m_hwnd,first,last),E_FAIL);
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListView::GetDataGeneration(ULONGLONG* value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = 0;
		Check(CheckAlive());
		*value = IsVirtual() ? m_generation : 0;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListView::ResetVirtualData(UINT count)
	{
		return VirtualCount(count,TRUE);
	}
	STDMETHODIMP CListView::UpdateVirtualText(UINT index, UINT column, RSTRING text)
	{
		return VirtualUpdate(index,column,text,FALSE,column ? QuadooForm::ListChangeSubItems : QuadooForm::ListChangeCaption);
	}
	STDMETHODIMP CListView::UpdateVirtualChecked(UINT index, BOOL checked)
	{
		return VirtualUpdate(index,0,NULL,checked,QuadooForm::ListChangeState);
	}
}
