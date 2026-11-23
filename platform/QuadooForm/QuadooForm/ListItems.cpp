#include "ListView.h"

namespace QF
{
	HRESULT CListView::Stored()
	{
		HRESULT hr;
		Check(CheckAlive());
		CheckIf(Boolean(L"OwnerData"),QuadooForm::EInvalidState);
	Cleanup:
		return hr;
	}
	HRESULT CListView::UnsupportedRows()
	{
		HRESULT hr;
		Check(CheckAlive());
		hr = E_NOTIMPL;
	Cleanup:
		return hr;
	}
	HRESULT CListView::CheckboxMode()
	{
		HRESULT hr;
		Check(CheckAlive());
		CheckIf(!Boolean(L"CheckBoxes"),QuadooForm::EInvalidState);
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListView::GetItems(IQuadooListItems** value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		Check(CheckAlive());
		*value = m_items;
		(*value)->AddRef();
	Cleanup:
		return hr;
	}
	HRESULT CListView::ResolveItem(IQuadooListItem* value, UINT* index)
	{
		HRESULT hr = E_INVALIDARG;
		UINT candidate;
		*index = 0;
		if(value && SUCCEEDED(value->GetItemIndex(&candidate)) && candidate < static_cast<UINT>(rows.Length()))
		{
			TStackRef<CListItem> item(rows[candidate]);
			if(SameObject(value,static_cast<IQuadooListItem*>(item)))
			{
				*index = candidate;
				hr = S_OK;
			}
		}
		return hr;
	}
	HRESULT CListView::NewEvent(PCWSTR name, CEventArgs** value)
	{
		HRESULT hr = S_OK;
		QuadooForm::ContextMode mode;
		*value = NULL;
		CheckIfIgnore(m_state != QuadooForm::Ready || m_replacing || m_nativeSync,S_OK);
		Check(m_context->GetMode(&mode));
		CheckIfIgnore(mode == QuadooForm::Design,S_OK);
		for(sysint n = 0; n < m_bindings.Length(); n++)
		{
			if(!lstrcmpW(m_bindings[n].spec->name,name) && m_bindings[n].delegate)
			{
				TStackRef<CEventArgs> args;
				args.Attach(__new CEventArgs(m_bindings[n].spec,Control()));
				CheckAlloc(args);
				Check(args->Initialize());
				*value = args.Detach();
				break;
			}
		}
	Cleanup:
		return hr;
	}
	HRESULT CListView::InvokeEvent(CEventArgs* args)
	{
		HRESULT hr = S_OK;
		TStackRef<CListView> keepAlive(this);
		CRString name;
		BOOL entered = FALSE;
		CheckIfIgnore(!args,S_OK);
		Check(args->GetEventName(&name));
		m_events++;
		entered = TRUE;
		for(sysint n = 0; n < m_bindings.Length(); n++)
		{
			if(!lstrcmpW(m_bindings[n].spec->name,name.c_wstr()) && m_bindings[n].delegate)
			{
				TStackRef<IQuadooDelegate> delegate(m_bindings[n].delegate);
				Check(delegate->Invoke(args));
				break;
			}
		}
		Check(CheckAlive());
	Cleanup:
		if(entered)
		{
			m_events--;
		}
		return hr;
	}
	HRESULT CListView::ItemEvent(PCWSTR name, CListItem* item, UINT mask)
	{
		HRESULT hr;
		TStackRef<CEventArgs> args;
		TStackRef<CListItem> keepItem(item);
		Check(NewEvent(name,&args));
		if(args)
		{
			args->SetNativeObject(0,static_cast<IQuadooListItem*>(item));
			args->SetNativeValue(1,mask);
			Check(InvokeEvent(args));
		}
	Cleanup:
		return hr;
	}
	HRESULT CListView::Changing(CListItem* item, UINT mask, UINT oldState, UINT newState, BOOL* cancel)
	{
		HRESULT hr;
		TStackRef<CEventArgs> args;
		*cancel = FALSE;
		Check(NewEvent(L"OnChanging",&args));
		if(args)
		{
			UINT affected = ((mask & QuadooForm::ListChangeState) ? LVIS_STATEIMAGEMASK : 0) |
				((mask & QuadooForm::ListChangeSelected) ? LVIS_SELECTED : 0) |
				((mask & QuadooForm::ListChangeFocused) ? LVIS_FOCUSED : 0);
			UINT current = INDEXTOSTATEIMAGEMASK(item->state + 1) |
				(item->selected ? LVIS_SELECTED : 0) | (item->focused ? LVIS_FOCUSED : 0);
			oldState = (oldState & affected) | (current & ~affected);
			newState = (newState & affected) | (current & ~affected);
			args->SetNativeObject(1,static_cast<IQuadooListItem*>(item));
			args->SetNativeValue(2,mask);
			args->SetNativeValue(3,oldState);
			args->SetNativeValue(4,newState);
			Check(InvokeEvent(args));
			Check(args->GetBool(0,cancel));
		}
	Cleanup:
		return hr;
	}
	HRESULT CListView::CheckProposal(CListItem* item, INT* state, BOOL* cancel)
	{
		HRESULT hr;
		TStackRef<CEventArgs> args;
		*cancel = FALSE;
		Check(NewEvent(L"OnItemChecking",&args));
		if(args)
		{
			BOOL checked;
			args->SetNativeObject(1,static_cast<IQuadooListItem*>(item));
			args->SetNativeValue(2,*state == 1);
			Check(InvokeEvent(args));
			Check(args->GetBool(0,cancel));
			Check(args->GetBool(2,&checked));
			*state = checked ? 1 : 0;
		}
	Cleanup:
		return hr;
	}
	HRESULT CListView::NotifyItems()
	{
		HRESULT hr;
		TStackRef<CListView> keepAlive(this);
		m_busy++;
		Check(Dirty(L"Items"));
		Check(FlushNotifications());
		Check(CheckAlive());
	Cleanup:
		m_busy--;
		return hr;
	}
	HRESULT CListView::ApplyStates(CListItem* item)
	{
		LVITEMW native = {0};
		native.stateMask = LVIS_STATEIMAGEMASK | LVIS_SELECTED | LVIS_FOCUSED;
		native.state = INDEXTOSTATEIMAGEMASK(item->state + 1) |
			(item->selected ? LVIS_SELECTED : 0) | (item->focused ? LVIS_FOCUSED : 0);
		m_nativeSync++;
		BOOL result = static_cast<BOOL>(SendMessageW(m_hwnd,LVM_SETITEMSTATE,item->index,reinterpret_cast<LPARAM>(&native)));
		m_nativeSync--;
		return result ? S_OK : E_FAIL;
	}
	HRESULT CListView::PopulateItems()
	{
		HRESULT hr = S_OK;
		BOOL entered = FALSE;
		CheckIf(Boolean(L"OwnerData") && rows.Length(),E_INVALIDARG);
		m_nativeSync++;
		entered = TRUE;
		for(sysint n = 0; n < rows.Length(); n++)
		{
			LVITEMW native = {0};
			CListItem* row = rows[n];
			native.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM | LVIF_STATE;
			native.iItem = static_cast<INT>(n);
			native.pszText = LPSTR_TEXTCALLBACKW;
			native.iImage = I_IMAGECALLBACK;
			native.lParam = reinterpret_cast<LPARAM>(row);
			native.stateMask = LVIS_STATEIMAGEMASK | LVIS_SELECTED | LVIS_FOCUSED;
			native.state = INDEXTOSTATEIMAGEMASK(row->state + 1) |
				(row->selected ? LVIS_SELECTED : 0) | (row->focused ? LVIS_FOCUSED : 0);
			CheckIf(SendMessageW(m_hwnd,LVM_INSERTITEMW,0,reinterpret_cast<LPARAM>(&native)) != n,E_OUTOFMEMORY);
			for(sysint cell = 1; cell < columns.Length(); cell++)
			{
				native.mask = LVIF_TEXT;
				native.iSubItem = static_cast<INT>(cell);
				CheckIf(!SendMessageW(m_hwnd,LVM_SETITEMW,0,reinterpret_cast<LPARAM>(&native)),E_FAIL);
			}
		}
	Cleanup:
		if(entered)
		{
			m_nativeSync--;
		}
		return hr;
	}
	HRESULT CListView::InsertItem(UINT index, RSTRING text, IQuadooListItem** value)
	{
		HRESULT hr;
		TStackRef<CListView> keepAlive(this);
		TStackRef<CListItem> item;
		BOOL inserted = FALSE;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		BOOL entered = FALSE;
		Check(Mutable());
		m_busy++;
		entered = TRUE;
		Check(Stored());
		Check(ValidateListText(text));
		CheckIf(index > static_cast<UINT>(rows.Length()) || rows.Length() >= 1000000,E_INVALIDARG);
		Check(rows.Reserve(1));
		item.Attach(__new CListItem(m_link));
		CheckAlloc(item);
		Check(CoCreateGuid(&item->id));
		Check(ScCreateRStringArray(&item->strings));
		item->caption = text;
		item->state = Boolean(L"CheckBoxes") ? 0 : -1;
		Check(rows.InsertAt(static_cast<CListItem*>(item),index));
		inserted = TRUE;
		for(sysint n = index; n < rows.Length(); n++)
		{
			rows[n]->index = static_cast<UINT>(n);
		}
		{
			LVITEMW native = {0};
			native.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM | LVIF_STATE;
			native.iItem = index;
			native.pszText = LPSTR_TEXTCALLBACKW;
			native.iImage = I_IMAGECALLBACK;
			native.lParam = reinterpret_cast<LPARAM>(static_cast<CListItem*>(item));
			native.stateMask = LVIS_STATEIMAGEMASK;
			native.state = INDEXTOSTATEIMAGEMASK(item->state + 1);
			m_nativeSync++;
			hr = SendMessageW(m_hwnd,LVM_INSERTITEMW,0,reinterpret_cast<LPARAM>(&native)) == index ? S_OK : E_OUTOFMEMORY;
			if(SUCCEEDED(hr))
			{
				for(sysint cell = 1; cell < columns.Length(); cell++)
				{
					native.mask = LVIF_TEXT;
					native.iSubItem = static_cast<INT>(cell);
					if(!SendMessageW(m_hwnd,LVM_SETITEMW,0,reinterpret_cast<LPARAM>(&native)))
					{
						hr = E_FAIL;
						ListView_DeleteItem(m_hwnd,index);
						break;
					}
				}
			}
			m_nativeSync--;
			Check(hr);
		}
		item->AddRef();
		inserted = FALSE;
		Check(ItemEvent(L"OnInsert",item));
		Check(NotifyItems());
		*value = item.Detach();
	Cleanup:
		if(entered)
		{
			m_busy--;
		}
		if(inserted)
		{
			rows.Remove(index,NULL);
			for(sysint n = index; n < rows.Length(); n++)
			{
				rows[n]->index = static_cast<UINT>(n);
			}
		}
		return hr;
	}
	HRESULT CListView::DeleteItem(IQuadooListItem* value)
	{
		HRESULT hr;
		TStackRef<CListView> keepAlive(this);
		TStackRef<CListItem> item;
		UINT index;
		BOOL entered = FALSE;
		Check(Mutable());
		m_busy++;
		entered = TRUE;
		Check(Stored());
		Check(ResolveItem(value,&index));
		m_nativeSync++;
		hr = ListView_DeleteItem(m_hwnd,index) ? S_OK : E_FAIL;
		m_nativeSync--;
		Check(hr);
		item.Attach(rows[index]);
		rows.Remove(index,NULL);
		for(sysint n = index; n < rows.Length(); n++)
		{
			rows[n]->index = static_cast<UINT>(n);
		}
		// Data remains readable until after the deletion callback.
		hr = ItemEvent(L"OnDeletion",item);
		item->attached = FALSE;
		item->data.Release();
		Check(hr);
		Check(NotifyItems());
	Cleanup:
		if(entered)
		{
			m_busy--;
		}
		return hr;
	}
	HRESULT CListView::ClearItems()
	{
		HRESULT hr = S_OK;
		HRESULT eventResult = S_OK;
		TStackRef<CListView> keepAlive(this);
		TArray<CListItem*> previous;
		BOOL entered = FALSE;
		Check(Mutable());
		m_busy++;
		entered = TRUE;
		Check(Stored());
		CheckIfIgnore(!rows.Length(),S_FALSE);
		m_nativeSync++;
		hr = ListView_DeleteAllItems(m_hwnd) ? S_OK : E_FAIL;
		m_nativeSync--;
		Check(hr);
		rows.Swap(previous);
		for(sysint n = 0; n < previous.Length(); n++)
		{
			HRESULT notified = ItemEvent(L"OnDeletion",previous[n]);
			if(FAILED(notified))
			{
				eventResult = notified;
			}
			if(m_state >= QuadooForm::Destroying)
			{
				break;
			}
		}
		ReleaseItems(previous);
		Check(eventResult);
		Check(NotifyItems());
	Cleanup:
		if(entered)
		{
			m_busy--;
		}
		return hr;
	}
	static INT CALLBACK OrderItems(LPARAM a, LPARAM b, LPARAM)
	{
		UINT first = reinterpret_cast<CListItem*>(a)->index;
		UINT second = reinterpret_cast<CListItem*>(b)->index;
		return first < second ? -1 : first > second ? 1 : 0;
	}
	HRESULT CListView::ApplyOrder(TArray<CListItem*>& order)
	{
		HRESULT hr;
		for(sysint n = 0; n < order.Length(); n++)
		{
			order[n]->index = static_cast<UINT>(n);
		}
		m_nativeSync++;
		hr = ListView_SortItems(m_hwnd,OrderItems,0) ? S_OK : E_FAIL;
		m_nativeSync--;
		if(SUCCEEDED(hr))
		{
			rows.Swap(order);
			Check(NotifyItems());
		}
		else
		{
			for(sysint n = 0; n < rows.Length(); n++)
			{
				rows[n]->index = static_cast<UINT>(n);
			}
		}
	Cleanup:
		return hr;
	}
	HRESULT CListView::MoveItem(IQuadooListItem* value, UINT index)
	{
		HRESULT hr;
		TStackRef<CListView> keepAlive(this);
		TArray<CListItem*> order;
		UINT previous;
		BOOL entered = FALSE;
		Check(Mutable());
		m_busy++;
		entered = TRUE;
		Check(Stored());
		Check(ResolveItem(value,&previous));
		CheckIf(index >= static_cast<UINT>(rows.Length()),E_INVALIDARG);
		CheckIfIgnore(index == previous,S_FALSE);
		for(sysint n = 0; n < rows.Length(); n++)
		{
			Check(order.Append(rows[n]));
		}
		order.Remove(previous,NULL);
		Check(order.InsertAt(rows[previous],index));
		Check(ApplyOrder(order));
	Cleanup:
		if(entered)
		{
			m_busy--;
		}
		return hr;
	}
	HRESULT CListView::CheckItems(CListItem* focused)
	{
		HRESULT hr;
		TStackRef<CListView> keepAlive(this);
		TArray<INT> previous;
		TArray<INT> next;
		BOOL entered = FALSE;
		BOOL changed = FALSE;
		Check(Mutable());
		Check(CheckboxMode());
		m_busy++;
		entered = TRUE;
		for(sysint n = 0; n < rows.Length(); n++)
		{
			INT state = rows[n]->state;
			if(rows[n] == focused || (focused->selected && rows[n]->selected))
			{
				BOOL cancel;
				state = focused->state == 1 ? 0 : 1;
				if(state != rows[n]->state)
				{
					Check(CheckProposal(rows[n],&state,&cancel));
					CheckIfIgnore(cancel,S_FALSE);
					if(state != rows[n]->state)
					{
						Check(Changing(rows[n],QuadooForm::ListChangeState,INDEXTOSTATEIMAGEMASK(rows[n]->state + 1),
							INDEXTOSTATEIMAGEMASK(state + 1),&cancel));
						CheckIfIgnore(cancel,S_FALSE);
					}
				}
			}
			Check(previous.Append(rows[n]->state));
			Check(next.Append(state));
			changed = changed || state != rows[n]->state;
		}
		CheckIfIgnore(!changed,S_FALSE);
		for(sysint n = 0; n < rows.Length(); n++)
		{
			if(next[n] != previous[n])
			{
				rows[n]->state = next[n];
				hr = ApplyStates(rows[n]);
				if(FAILED(hr))
				{
					for(sysint restore = 0; restore <= n; restore++)
					{
						rows[restore]->state = previous[restore];
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
				Check(ItemEvent(L"OnChange",rows[n],QuadooForm::ListChangeState));
				Check(ItemEvent(L"OnItemChecked",rows[n]));
			}
		}
		Check(NotifyItems());
	Cleanup:
		if(entered)
		{
			m_busy--;
		}
		return hr;
	}
	HRESULT CListView::EditItem(CListItem* item, UINT mask, RSTRING text, IRStringArray* strings, INT number, IUnknown* data)
	{
		HRESULT hr;
		TStackRef<CListView> keepAlive(this);
		TStackRef<CListItem> keepItem(item);
		TStackRef<IRStringArray> candidate;
		UINT index;
		BOOL cancel;
		BOOL entered = FALSE;
		Check(Mutable());
		m_busy++;
		entered = TRUE;
		Check(Stored());
		Check(ResolveItem(item,&index));
		if(mask == QuadooForm::ListChangeCaption)
		{
			Check(ValidateListText(text));
			CheckIfIgnore(SameString(item->caption,text),S_FALSE);
		}
		else if(mask == QuadooForm::ListChangeSubItems)
		{
			INT comparison;
			CheckIf(!strings || strings->Length() < 0 || strings->Length() > 4096,E_INVALIDARG);
			Check(ScCreateRStringArray(&candidate));
			for(sysint n = 0; n < strings->Length(); n++)
			{
				CRString cell;
				Check(strings->GetRString(n,&cell));
				Check(ValidateListText(cell));
				Check(candidate->AddRString(cell));
			}
			Check(item->strings->Compare(candidate,&comparison));
			CheckIfIgnore(!comparison,S_FALSE);
		}
		else if(mask == QuadooForm::ListChangeImage)
		{
			CheckIf(number < -1,E_INVALIDARG);
			CheckIfIgnore(item->image == number,S_FALSE);
		}
		else if(mask == QuadooForm::ListChangeState)
		{
			CheckIf(number < (Boolean(L"CheckBoxes") ? 0 : -1) || number > (Boolean(L"CheckBoxes") ? 1 : 14),E_INVALIDARG);
			CheckIfIgnore(item->state == number,S_FALSE);
			if(Boolean(L"CheckBoxes"))
			{
				Check(CheckProposal(item,&number,&cancel));
				CheckIfIgnore(cancel || number == item->state,S_FALSE);
			}
		}
		else
		{
			CheckIf(mask != QuadooForm::ListChangeData,E_INVALIDARG);
			CheckIfIgnore((!data && !item->data) || SameObject(data,item->data),S_FALSE);
		}
		Check(Changing(item,mask,INDEXTOSTATEIMAGEMASK(item->state + 1),
			INDEXTOSTATEIMAGEMASK((mask == QuadooForm::ListChangeState ? number : item->state) + 1),&cancel));
		CheckIfIgnore(cancel,S_FALSE);
		if(mask == QuadooForm::ListChangeCaption)
		{
			item->caption = text;
		}
		else if(mask == QuadooForm::ListChangeSubItems)
		{
			item->strings = candidate;
		}
		else if(mask == QuadooForm::ListChangeImage)
		{
			item->image = number;
		}
		else if(mask == QuadooForm::ListChangeState)
		{
			INT previous = item->state;
			item->state = number;
			hr = ApplyStates(item);
			if(FAILED(hr))
			{
				item->state = previous;
				goto Cleanup;
			}
		}
		else
		{
			item->data = data;
		}
		ListView_RedrawItems(m_hwnd,index,index);
		Check(ItemEvent(L"OnChange",item,mask));
		if(mask == QuadooForm::ListChangeState && Boolean(L"CheckBoxes"))
		{
			Check(ItemEvent(L"OnItemChecked",item));
		}
		if(mask != QuadooForm::ListChangeData)
		{
			Check(NotifyItems());
		}
	Cleanup:
		if(entered)
		{
			m_busy--;
		}
		return hr;
	}
}
