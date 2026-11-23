#include "ListView.h"

namespace QF
{
	CListRequest::CListRequest(CListLink* owner, UINT row, ULONGLONG epoch, BOOL checks) :
		link(owner), index(row), generation(epoch), image(-1), state(checks ? 0 : -1),
		selected(FALSE), focused(FALSE), checkbox(checks), writable(FALSE), identity(NULL)
	{
	}
	CListRequest::~CListRequest()
	{
		if(identity && link->view)
		{
			link->view->ForgetRequest(identity);
		}
	}
	HRESULT CListRequest::Initialize()
	{
		HRESULT hr;
		Check(RStrCreateW(LSP(L""),&caption));
		Check(ScCreateRStringArray(&strings));
	Cleanup:
		return hr;
	}
	HRESULT CListRequest::Readable()
	{
		return link->thread == GetCurrentThreadId() ? S_OK : QuadooForm::EWrongThread;
	}
	HRESULT CListRequest::Writable()
	{
		HRESULT hr;
		Check(Readable());
		CheckIf(!writable || !link->view || !link->view->VirtualAttached(index,generation),QuadooForm::EInvalidState);
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListRequest::GetID(GUID* value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = GUID_NULL;
		Check(Readable());
		*value = GUID_NULL;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListRequest::GetAttached(BOOL* value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = FALSE;
		Check(Readable());
		*value = link->view && link->view->VirtualAttached(index,generation);
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListRequest::GetIsVirtual(BOOL* value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = FALSE;
		Check(Readable());
		*value = TRUE;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListRequest::GetDataGeneration(ULONGLONG* value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = 0;
		Check(Readable());
		*value = generation;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListRequest::GetItemIndex(UINT* value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = 0;
		Check(Readable());
		*value = index;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListRequest::GetCaption(RSTRING* value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		Check(Readable());
		RStrSet(*value,caption);
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListRequest::GetSubItems(IRStringArray** value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		Check(Readable());
		Check(strings->Clone(value));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListRequest::GetImageIndex(INT* value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = -1;
		Check(Readable());
		*value = image;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListRequest::GetStateIndex(INT* value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = -1;
		Check(Readable());
		*value = state;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListRequest::GetChecked(BOOL* value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = FALSE;
		Check(Readable());
		CheckIf(!checkbox,QuadooForm::EInvalidState);
		*value = state == 1;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListRequest::GetData(IUnknown** value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		Check(Readable());
		*value = data;
		if(*value)
		{
			(*value)->AddRef();
		}
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListRequest::GetSelected(BOOL* value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = FALSE;
		Check(Readable());
		*value = selected;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListRequest::GetFocused(BOOL* value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = FALSE;
		Check(Readable());
		*value = focused;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListRequest::SetCaption(RSTRING value)
	{
		HRESULT hr;
		Check(Writable());
		Check(ValidateListText(value));
		caption = value;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListRequest::SetImageIndex(INT value)
	{
		HRESULT hr;
		Check(Writable());
		CheckIf(value < -1,E_INVALIDARG);
		image = value;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListRequest::SetStateIndex(INT value)
	{
		HRESULT hr;
		Check(Writable());
		CheckIf(value < (checkbox ? 0 : -1) || value > (checkbox ? 1 : 14),E_INVALIDARG);
		state = value;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListRequest::SetChecked(BOOL value)
	{
		HRESULT hr;
		Check(Writable());
		CheckIf(!checkbox,QuadooForm::EInvalidState);
		state = value ? 1 : 0;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListRequest::SetData(IUnknown* value)
	{
		HRESULT hr;
		Check(Writable());
		data = value;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListRequest::SetSelected(BOOL value)
	{
		HRESULT hr;
		Check(Writable());
		selected = value != FALSE;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListRequest::SetFocused(BOOL value)
	{
		HRESULT hr;
		Check(Writable());
		focused = value != FALSE;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListRequest::SetSubItems(IRStringArray* value)
	{
		HRESULT hr;
		TStackRef<IRStringArray> copy;
		sysint count;
		Check(Writable());
		CheckIf(!value,E_INVALIDARG);
		count = value->Length();
		CheckIf(count < 0 || count > 4096,E_INVALIDARG);
		Check(ScCreateRStringArray(&copy));
		for(sysint n = 0; n < count; n++)
		{
			CRString cell;
			Check(value->GetRString(n,&cell));
			Check(ValidateListText(cell));
			Check(copy->AddRString(cell));
		}
		strings = copy;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListRequest::GetSubItem(UINT cell, RSTRING* value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		Check(Readable());
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
	STDMETHODIMP CListRequest::SetSubItem(UINT cell, RSTRING value)
	{
		HRESULT hr;
		TStackRef<IRStringArray> copy;
		Check(Writable());
		CheckIf(cell >= 4096,E_INVALIDARG);
		Check(ValidateListText(value));
		Check(strings->Clone(&copy));
		while(copy->Length() <= static_cast<sysint>(cell))
		{
			Check(copy->AddRString(RSTRING_CAST(L"")));
		}
		Check(copy->Replace(cell,value));
		strings = copy;
	Cleanup:
		return hr;
	}
}
