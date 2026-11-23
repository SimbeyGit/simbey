#include "ListView.h"
#include "..\Diagnostics.h"

namespace QF
{
	BOOL CListView::IsVirtual()
	{
		return Boolean(L"OwnerData");
	}
	BOOL CListView::VirtualAttached(UINT index, ULONGLONG generation)
	{
		return m_state < QuadooForm::Destroying && IsVirtual() && m_generation == generation && index < m_virtualCount;
	}
	BOOL CListView::Bound(PCWSTR name)
	{
		for(sysint n = 0; n < m_bindings.Length(); n++)
		{
			if(!lstrcmpW(m_bindings[n].spec->name,name) && m_bindings[n].delegate)
			{
				return TRUE;
			}
		}
		return FALSE;
	}
	VOID CListView::ReportVirtualError(HRESULT error, PCWSTR name)
	{
		// Display-provider failures must not poison the enclosing modal pump.
		if(FAILED(error) && m_context)
		{
			CRString detail;
			TStackRef<QuadooPrivate::IDiagnosticReport> report;
			if(SUCCEEDED(RStrFormatW(&detail,L"%ls: %ls failed (0x%.8x)",m_name.c_wstr(),name,error)))
			{
				if(SUCCEEDED(QueryOptional(m_context,&report)) && report)
				{
					report->ReportDiagnostic(error,m_name,detail);
				}
				else
				{
					m_context->ReportError(error,m_name,detail);
				}
			}
		}
	}
	VOID CListView::ForgetRequest(IUnknown* identity)
	{
		m_snapshots.Remove(identity,NULL);
	}
	HRESULT CListView::ItemCount(UINT* count)
	{
		HRESULT hr;
		CheckIf(!count,E_POINTER);
		*count = 0;
		Check(CheckAlive());
		*count = IsVirtual() ? m_virtualCount : static_cast<UINT>(rows.Length());
	Cleanup:
		return hr;
	}
	HRESULT CListView::NewRequest(UINT index, CListRequest** result)
	{
		HRESULT hr;
		TStackRef<CListRequest> request;
		TStackRef<IUnknown> identity;
		CheckIf(!result,E_POINTER);
		*result = NULL;
		Check(CheckAlive());
		CheckIf(!IsVirtual() || index >= m_virtualCount,DISP_E_BADINDEX);
		request.Attach(__new CListRequest(m_link,index,m_generation,Boolean(L"CheckBoxes")));
		CheckAlloc(request);
		Check(request->Initialize());
		request->selected = (ListView_GetItemState(m_hwnd,index,LVIS_SELECTED) & LVIS_SELECTED) != 0;
		request->focused = (ListView_GetItemState(m_hwnd,index,LVIS_FOCUSED) & LVIS_FOCUSED) != 0;
		Check(request->QueryInterface(IID_IUnknown,reinterpret_cast<PVOID*>(&identity)));
		Check(m_snapshots.Add(identity,TRUE));
		request->identity = identity;
		*result = request.Detach();
	Cleanup:
		return hr;
	}
	HRESULT CListView::FetchVirtual(UINT index, UINT fields, UINT column, CListRequest** result)
	{
		HRESULT hr;
		TStackRef<CListView> keepAlive(this);
		TStackRef<CListRequest> request;
		TStackRef<CEventArgs> args;
		BOOL entered = FALSE;
		HWND window = m_hwnd;
		CheckIf(!result,E_POINTER);
		*result = NULL;
		Check(CheckAlive());
		CheckIf(m_requests.Length() >= 32,QuadooForm::EInvalidState);
		for(sysint n = 0; n < m_requests.Length(); n++)
		{
			CheckIf(m_requests[n] == index,QuadooForm::EInvalidState);
		}
		Check(NewRequest(index,&request));
		Check(m_requests.Append(index));
		entered = TRUE;
		Check(NewEvent(L"OnData",&args));
		if(args)
		{
			args->SetNativeObject(0,static_cast<IQuadooListItem*>(request));
			args->SetNativeValue(1,fields);
			args->SetNativeValue(2,column);
			request->writable = TRUE;
			hr = InvokeEvent(args);
			request->writable = FALSE;
			Check(hr);
			CheckIf(!VirtualAttached(index,request->generation) || window != m_hwnd,QuadooForm::EInvalidState);
		}
		*result = request.Detach();
	Cleanup:
		if(request)
		{
			request->writable = FALSE;
		}
		if(entered)
		{
			m_requests.Remove(m_requests.Length() - 1,NULL);
		}
		return hr;
	}
	HRESULT CListView::VirtualItemIndex(IQuadooListItem* item, UINT* index)
	{
		HRESULT hr;
		TStackRef<IUnknown> identity;
		ULONGLONG generation;
		BOOL known;
		CheckIf(!item || !index,E_INVALIDARG);
		Check(item->QueryInterface(IID_IUnknown,reinterpret_cast<PVOID*>(&identity)));
		CheckIf(FAILED(m_snapshots.Find(identity,&known)),E_INVALIDARG);
		Check(item->GetItemIndex(index));
		Check(item->GetDataGeneration(&generation));
		CheckIf(!VirtualAttached(*index,generation),QuadooForm::EInvalidState);
	Cleanup:
		return hr;
	}
	HRESULT CListView::VirtualItemEvent(PCWSTR name, IQuadooListItem* item, UINT number)
	{
		HRESULT hr;
		TStackRef<CEventArgs> args;
		Check(NewEvent(name,&args));
		if(args)
		{
			args->SetNativeObject(0,item);
			args->SetNativeValue(1,number);
			Check(InvokeEvent(args));
		}
	Cleanup:
		return hr;
	}
	HRESULT CListView::VirtualCount(UINT count, BOOL reset)
	{
		HRESULT hr;
		TStackRef<CListView> keepAlive(this);
		UINT previous = m_virtualCount;
		ULONGLONG generation = m_generation;
		BOOL entered = FALSE;
		QuadooForm::ContextMode mode;
		Check(CheckAlive());
		CheckIf(!IsVirtual(),QuadooForm::EInvalidState);
		CheckIf(m_busy || m_events || m_replacing || m_nativeSync || m_requests.Length(),QuadooForm::EInvalidState);
		CheckIf(count > QuadooForm::ListViewMaximumVirtualCount,E_INVALIDARG);
		Check(m_context->GetMode(&mode));
		CheckIf(mode == QuadooForm::Design && count,QuadooForm::EInvalidState);
		CheckIfIgnore(!reset && count == previous,S_FALSE);
		CheckIf(m_generation == ~static_cast<ULONGLONG>(0),E_UNEXPECTED);
		m_busy++;
		m_nativeSync++;
		entered = TRUE;
		m_virtualCount = count;
		m_generation++;
		hr = ListView_SetItemCountEx(m_hwnd,count,LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL) ? S_OK : E_OUTOFMEMORY;
		if(FAILED(hr))
		{
			m_virtualCount = previous;
			m_generation = generation;
			goto Cleanup;
		}
		if(m_editing)
		{
			HWND edit = ListView_GetEditControl(m_hwnd);
			m_editing = FALSE;
			m_virtualEdit = -1;
			if(edit)
			{
				SendMessageW(edit,WM_KEYDOWN,VK_ESCAPE,0);
			}
		}
		if(reset)
		{
			ListView_SetItemState(m_hwnd,-1,0,LVIS_SELECTED | LVIS_FOCUSED);
		}
		InvalidateRect(m_hwnd,NULL,TRUE);
	Cleanup:
		if(entered)
		{
			m_nativeSync--;
			m_busy--;
		}
		m_virtualStates.Clear();
		m_stateError = S_OK;
		return hr;
	}
	HRESULT CListView::PopulateVirtual()
	{
		HRESULT hr = S_OK;
		if(IsVirtual())
		{
			m_nativeSync++;
			ListView_SetCallbackMask(m_hwnd,LVIS_STATEIMAGEMASK);
			hr = ListView_SetItemCountEx(m_hwnd,m_virtualCount,LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL) ? S_OK : E_OUTOFMEMORY;
			m_nativeSync--;
		}
		return hr;
	}
	HRESULT CListView::VirtualUpdate(UINT index, UINT column, RSTRING text, BOOL checked, UINT mask)
	{
		HRESULT hr;
		TStackRef<CListView> keepAlive(this);
		TStackRef<CListRequest> original;
		TStackRef<CListRequest> proposal;
		TStackRef<CEventArgs> args;
		BOOL cancel = FALSE;
		BOOL handled = FALSE;
		BOOL entered = FALSE;
		QuadooForm::ContextMode mode;
		Check(Mutable());
		CheckIf(!IsVirtual() || index >= m_virtualCount,QuadooForm::EInvalidState);
		CheckIf(column > 4096,E_INVALIDARG);
		Check(m_context->GetMode(&mode));
		CheckIf(mode == QuadooForm::Design,QuadooForm::EInvalidState);
		CheckIfIgnore(!Bound(L"OnDataUpdate"),S_FALSE);
		if(mask == QuadooForm::ListChangeState)
		{
			Check(CheckboxMode());
		}
		else
		{
			Check(ValidateListText(text));
		}
		Check(FetchVirtual(index,31,column,&original));
		Check(NewRequest(index,&proposal));
		proposal->caption = original->caption;
		proposal->strings.Release();
		Check(original->strings->Clone(&proposal->strings));
		proposal->data = original->data;
		proposal->image = original->image;
		proposal->state = original->state;
		if(mask == QuadooForm::ListChangeState)
		{
			CheckIfIgnore((proposal->state == 1) == (checked != FALSE),S_FALSE);
			Check(NewEvent(L"OnItemChecking",&args));
			if(args)
			{
				args->SetNativeObject(1,static_cast<IQuadooListItem*>(original));
				args->SetNativeValue(2,checked);
				Check(InvokeEvent(args));
				Check(args->GetBool(0,&cancel));
				Check(args->GetBool(2,&checked));
				CheckIfIgnore(cancel,S_FALSE);
				args.Release();
			}
			CheckIfIgnore((proposal->state == 1) == (checked != FALSE),S_FALSE);
			proposal->state = checked ? 1 : 0;
		}
		else if(column)
		{
			CRString old;
			Check(proposal->GetSubItem(column - 1,&old));
			CheckIfIgnore(SameString(old,text),S_FALSE);
			proposal->writable = TRUE;
			Check(proposal->SetSubItem(column - 1,text));
			proposal->writable = FALSE;
		}
		else
		{
			CheckIfIgnore(SameString(proposal->caption,text),S_FALSE);
			proposal->caption = text;
		}
		Check(NewEvent(L"OnDataUpdate",&args));
		CheckIfIgnore(!args,S_FALSE);
		args->SetNativeObject(1,static_cast<IQuadooListItem*>(proposal));
		args->SetNativeValue(2,mask);
		args->SetNativeValue(3,column);
		Check(m_requests.Append(index));
		entered = TRUE;
		proposal->writable = TRUE;
		hr = InvokeEvent(args);
		proposal->writable = FALSE;
		Check(hr);
		Check(args->GetBool(0,&cancel));
		Check(args->GetBool(4,&handled));
		CheckIfIgnore(cancel || !handled,S_FALSE);
		CheckIf(!VirtualAttached(index,proposal->generation),QuadooForm::EInvalidState);
		m_requests.Remove(m_requests.Length() - 1,NULL);
		entered = FALSE;
		ListView_RedrawItems(m_hwnd,index,index);
		Check(VirtualItemEvent(L"OnChange",proposal,mask));
		if(mask == QuadooForm::ListChangeState)
		{
			Check(VirtualItemEvent(L"OnItemChecked",proposal));
		}
	Cleanup:
		if(proposal)
		{
			proposal->writable = FALSE;
		}
		if(entered)
		{
			m_requests.Remove(m_requests.Length() - 1,NULL);
		}
		return hr;
	}
	HRESULT CListView::VirtualSelection(INT index, BOOL value, BOOL focus, BOOL exclusive)
	{
		HRESULT hr;
		TStackRef<CListView> keepAlive(this);
		TStackRef<CListRequest> request;
		TStackRef<CEventArgs> args;
		BOOL cancel;
		BOOL entered = FALSE;
		Check(Mutable());
		CheckIf(index < -1 || (index >= 0 && static_cast<UINT>(index) >= m_virtualCount),E_INVALIDARG);
		if(index < 0)
		{
			CheckIfIgnore(ListView_GetNextItem(m_hwnd,-1,focus ? LVNI_FOCUSED : LVNI_SELECTED) < 0,S_FALSE);
		}
		else
		{
			UINT bit = focus ? LVIS_FOCUSED : LVIS_SELECTED;
			BOOL current = (ListView_GetItemState(m_hwnd,index,bit) & bit) != 0;
			CheckIfIgnore(current == (value != FALSE) && (!exclusive || focus || ListView_GetSelectedCount(m_hwnd) <= 1),S_FALSE);
		}
		if(index >= 0)
		{
			Check(NewEvent(L"OnChanging",&args));
			if(args)
			{
				Check(FetchVirtual(index,31,0,&request));
				UINT state = ListView_GetItemState(m_hwnd,index,LVIS_SELECTED | LVIS_FOCUSED | LVIS_STATEIMAGEMASK);
				UINT bit = focus ? LVIS_FOCUSED : LVIS_SELECTED;
				args->SetNativeObject(1,static_cast<IQuadooListItem*>(request));
				args->SetNativeValue(2,focus ? QuadooForm::ListChangeFocused : QuadooForm::ListChangeSelected);
				args->SetNativeValue(3,state);
				args->SetNativeValue(4,value ? state | bit : state & ~bit);
				Check(InvokeEvent(args));
				Check(args->GetBool(0,&cancel));
				CheckIfIgnore(cancel,S_FALSE);
			}
		}
		m_busy++;
		m_nativeSync++;
		entered = TRUE;
		if(exclusive || focus || (value && !Boolean(L"MultiSelect")))
		{
			ListView_SetItemState(m_hwnd,-1,0,focus ? LVIS_FOCUSED : LVIS_SELECTED);
		}
		if(index >= 0)
		{
			ListView_SetItemState(m_hwnd,index,value ? (focus ? LVIS_FOCUSED : LVIS_SELECTED) : 0,
				focus ? LVIS_FOCUSED : LVIS_SELECTED);
		}
		m_nativeSync--;
		m_busy--;
		entered = FALSE;
		Check(DrainVirtualStates());
	Cleanup:
		if(entered)
		{
			m_nativeSync--;
			m_busy--;
		}
		return hr;
	}
	HRESULT CListView::DrainVirtualStates()
	{
		HRESULT hr;
		TArray<VirtualState> pending;
		HRESULT error = m_stateError;
		m_stateError = S_OK;
		m_virtualStates.Swap(pending);
		for(sysint n = 0; n < pending.Length(); n++)
		{
			TStackRef<CEventArgs> args;
			Check(NewEvent(L"OnDataStateChange",&args));
			if(args)
			{
				args->SetNativeValue(0,pending[n].first);
				args->SetNativeValue(1,pending[n].last);
				args->SetNativeValue(2,pending[n].previous);
				args->SetNativeValue(3,pending[n].next);
				Check(InvokeEvent(args));
			}
			if(pending[n].first == pending[n].last && pending[n].first < m_virtualCount)
			{
				TStackRef<CListRequest> request;
				UINT mask = (pending[n].previous ^ pending[n].next) & LVIS_SELECTED ?
					QuadooForm::ListChangeSelected : QuadooForm::ListChangeFocused;
				if(Bound(L"OnChange") || Bound(L"OnSelectItem"))
				{
					Check(FetchVirtual(pending[n].first,31,0,&request));
					Check(VirtualItemEvent(L"OnChange",request,mask));
					if(mask == QuadooForm::ListChangeSelected)
					{
						Check(VirtualItemEvent(L"OnSelectItem",request,(pending[n].next & LVIS_SELECTED) != 0));
					}
				}
			}
		}
		hr = error;
	Cleanup:
		return hr;
	}
}
