#include "ListView.h"

namespace QF
{
	HRESULT ValidateListText(RSTRING value)
	{
		HRESULT hr = S_OK;
		CheckIf(!value || !RStrIsWide(value) || RStrLen(value) > 32767,E_INVALIDARG);
		for(INT n = 0; n < RStrLen(value); n++)
		{
			CheckIf(!RStrToWide(value)[n],E_INVALIDARG);
		}
	Cleanup:
		return hr;
	}
	CListItem::CListItem(CListLink* owner) : link(owner), image(-1), state(-1),
		index(0), attached(TRUE), selected(FALSE), focused(FALSE)
	{
		id = GUID_NULL;
	}
	HRESULT CListItem::Alive()
	{
		HRESULT hr;
		Check(link->Alive());
		CheckIf(!attached,QuadooForm::EDestroyed);
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItem::GetID(GUID* value)
	{
		HRESULT hr = S_OK;
		CheckIf(!value,E_POINTER);
		*value = GUID_NULL;
		CheckIf(link->thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
		*value = id;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItem::GetAttached(BOOL* value)
	{
		HRESULT hr = S_OK;
		CheckIf(!value,E_POINTER);
		*value = FALSE;
		CheckIf(link->thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
		*value = attached && link->view != NULL;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItem::GetIsVirtual(BOOL* value)
	{
		HRESULT hr = S_OK;
		CheckIf(!value,E_POINTER);
		*value = FALSE;
		Check(Alive());
		*value = FALSE;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItem::GetDataGeneration(ULONGLONG* value)
	{
		HRESULT hr = S_OK;
		CheckIf(!value,E_POINTER);
		*value = 0;
		Check(Alive());
		*value = 0;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItem::GetItemIndex(UINT* value)
	{
		HRESULT hr = S_OK;
		CheckIf(!value,E_POINTER);
		*value = 0;
		Check(Alive());
		*value = index;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItem::GetCaption(RSTRING* value)
	{
		HRESULT hr = S_OK;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		Check(Alive());
		RStrSet(*value,caption);
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItem::GetSubItems(IRStringArray** value)
	{
		HRESULT hr = S_OK;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		Check(Alive());
		Check(strings->Clone(value));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItem::GetImageIndex(INT* value)
	{
		HRESULT hr = S_OK;
		CheckIf(!value,E_POINTER);
		*value = -1;
		Check(Alive());
		*value = image;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItem::GetStateIndex(INT* value)
	{
		HRESULT hr = S_OK;
		CheckIf(!value,E_POINTER);
		*value = -1;
		Check(Alive());
		*value = state;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItem::GetChecked(BOOL* value)
	{
		HRESULT hr = S_OK;
		CheckIf(!value,E_POINTER);
		*value = FALSE;
		Check(Alive());
		Check(link->view->CheckboxMode());
		*value = state == 1;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItem::GetData(IUnknown** value)
	{
		HRESULT hr = S_OK;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		Check(Alive());
		*value = data;
		if(*value)
		{
			(*value)->AddRef();
		}
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItem::GetSelected(BOOL* value)
	{
		HRESULT hr = S_OK;
		CheckIf(!value,E_POINTER);
		*value = FALSE;
		Check(Alive());
		*value = selected;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItem::GetFocused(BOOL* value)
	{
		HRESULT hr = S_OK;
		CheckIf(!value,E_POINTER);
		*value = FALSE;
		Check(Alive());
		*value = focused;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItem::SetCaption(RSTRING value)
	{
		HRESULT hr;
		Check(Alive());
		Check(link->view->EditItem(this,QuadooForm::ListChangeCaption,value,NULL,0,NULL));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItem::SetSubItems(IRStringArray* value)
	{
		HRESULT hr;
		Check(Alive());
		Check(link->view->EditItem(this,QuadooForm::ListChangeSubItems,NULL,value,0,NULL));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItem::SetImageIndex(INT value)
	{
		HRESULT hr;
		Check(Alive());
		Check(link->view->EditItem(this,QuadooForm::ListChangeImage,NULL,NULL,value,NULL));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItem::SetStateIndex(INT value)
	{
		HRESULT hr;
		Check(Alive());
		Check(link->view->EditItem(this,QuadooForm::ListChangeState,NULL,NULL,value,NULL));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItem::SetData(IUnknown* value)
	{
		HRESULT hr;
		Check(Alive());
		Check(link->view->EditItem(this,QuadooForm::ListChangeData,NULL,NULL,0,value));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItem::SetSelected(BOOL value)
	{
		HRESULT hr;
		Check(Alive());
		Check(link->view->SetItemSelected(index,value));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItem::SetFocused(BOOL value)
	{
		HRESULT hr;
		Check(Alive());
		CheckIfIgnore(!value && !focused,S_FALSE);
		Check(link->view->SetFocused(value ? this : NULL));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItem::SetChecked(BOOL value)
	{
		HRESULT hr;
		Check(Alive());
		Check(link->view->CheckboxMode());
		// Checked is meaningful only in checkbox mode; the view validates it.
		Check(link->view->EditItem(this,QuadooForm::ListChangeState,NULL,NULL,value ? 1 : 0,NULL));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItem::GetSubItem(UINT cell, RSTRING* value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		Check(Alive());
		CheckIf(cell >= 4096,E_INVALIDARG);
		if(cell < static_cast<UINT>(strings->Length()))
		{
			Check(strings->GetRString(cell,value));
		}
		else
		{
			Check(RStrCreateW(LSP(L""),value));
		}
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItem::SetSubItem(UINT cell, RSTRING value)
	{
		HRESULT hr;
		TStackRef<IRStringArray> copy;
		Check(Alive());
		CheckIf(cell >= 4096,E_INVALIDARG);
		Check(ValidateListText(value));
		Check(strings->Clone(&copy));
		while(copy->Length() <= static_cast<sysint>(cell))
		{
			Check(copy->AddRString(RSTRING_CAST(L"")));
		}
		Check(copy->Replace(cell,value));
		Check(SetSubItems(copy));
	Cleanup:
		return hr;
	}
	CListItems::CListItems(CListLink* link) : m_link(link)
	{
	}
	STDMETHODIMP CListItems::GetCount(UINT* value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = 0;
		Check(m_link->Alive());
		Check(m_link->view->ItemCount(value));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItems::SetCount(UINT value)
	{
		HRESULT hr;
		Check(m_link->Alive());
		Check(m_link->view->VirtualCount(value,FALSE));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItems::GetAt(UINT index, IQuadooListItem** value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		Check(m_link->Alive());
		if(m_link->view->IsVirtual())
		{
			TStackRef<CListRequest> request;
			Check(m_link->view->FetchVirtual(index,31,0,&request));
			*value = request.Detach();
			goto Cleanup;
		}
		Check(m_link->view->Stored());
		CheckIf(index >= static_cast<UINT>(m_link->view->rows.Length()),DISP_E_BADINDEX);
		*value = m_link->view->rows[index];
		(*value)->AddRef();
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItems::Find(REFGUID id, IQuadooListItem** value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		Check(m_link->Alive());
		Check(m_link->view->Stored());
		hr = S_FALSE;
		for(sysint n = 0; n < m_link->view->rows.Length(); n++)
		{
			if(IsEqualGUID(id,m_link->view->rows[n]->id))
			{
				*value = m_link->view->rows[n];
				(*value)->AddRef();
				hr = S_OK;
				break;
			}
		}
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItems::Insert(UINT index, RSTRING caption, IQuadooListItem** value)
	{
		HRESULT hr;
		Check(m_link->Alive());
		Check(m_link->view->InsertItem(index,caption,value));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItems::Delete(IQuadooListItem* value)
	{
		HRESULT hr;
		Check(m_link->Alive());
		Check(m_link->view->DeleteItem(value));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItems::Move(IQuadooListItem* value, UINT index)
	{
		HRESULT hr;
		Check(m_link->Alive());
		Check(m_link->view->MoveItem(value,index));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListItems::Clear(VOID)
	{
		HRESULT hr;
		Check(m_link->Alive());
		Check(m_link->view->ClearItems());
	Cleanup:
		return hr;
	}
}
