#include "ListView.h"

namespace QF
{
	CListLink::CListLink(CFactory* factory) : m_factory(factory), view(NULL), thread(GetCurrentThreadId())
	{
		m_factory->AddInstance();
	}
	CListLink::~CListLink()
	{
		m_factory->RemoveInstance();
	}
	HRESULT CListLink::Alive()
	{
		HRESULT hr = S_OK;
		QuadooForm::ComponentState state;
		CheckIf(thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
		CheckIf(!view,QuadooForm::EDestroyed);
		Check(view->GetState(&state));
		CheckIf(state >= QuadooForm::Destroying,QuadooForm::EDestroyed);
	Cleanup:
		return hr;
	}
	CListColumn::CListColumn(CListLink* owner) : link(owner), width(100),
		alignment(QuadooForm::ListColumnLeft), display(0), attached(TRUE)
	{
		id = GUID_NULL;
	}
	HRESULT CListColumn::Alive()
	{
		HRESULT hr;
		Check(link->Alive());
		CheckIf(!attached,QuadooForm::EDestroyed);
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListColumn::GetID(GUID* value)
	{
		HRESULT hr = S_OK;
		CheckIf(!value,E_POINTER);
		*value = GUID_NULL;
		CheckIf(link->thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
		*value = id;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListColumn::GetAttached(BOOL* value)
	{
		HRESULT hr = S_OK;
		CheckIf(!value,E_POINTER);
		*value = FALSE;
		CheckIf(link->thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
		*value = attached && link->view != NULL;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListColumn::GetIndex(UINT* value)
	{
		HRESULT hr = S_OK;
		CheckIf(!value,E_POINTER);
		*value = 0;
		Check(Alive());
		for(sysint n = 0; n < link->view->columns.Length(); n++)
		{
			if(link->view->columns[n] == this)
			{
				*value = static_cast<UINT>(n);
				goto Cleanup;
			}
		}
		hr = E_UNEXPECTED;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListColumn::GetCaption(RSTRING* value)
	{
		HRESULT hr = S_OK;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		Check(Alive());
		RStrSet(*value,caption);
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListColumn::GetWidth(INT* value)
	{
		HRESULT hr = S_OK;
		CheckIf(!value,E_POINTER);
		*value = 0;
		Check(Alive());
		Check(link->view->SyncHeader());
		*value = width;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListColumn::GetAlignment(QuadooForm::ListColumnAlignment* value)
	{
		HRESULT hr = S_OK;
		CheckIf(!value,E_POINTER);
		*value = QuadooForm::ListColumnLeft;
		Check(Alive());
		*value = alignment;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListColumn::GetDisplayIndex(UINT* value)
	{
		HRESULT hr = S_OK;
		CheckIf(!value,E_POINTER);
		*value = 0;
		Check(Alive());
		Check(link->view->SyncHeader());
		*value = display;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListColumn::SetCaption(RSTRING value)
	{
		HRESULT hr;
		Check(Alive());
		Check(link->view->EditColumn(this,0,value,0));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListColumn::SetWidth(INT value)
	{
		HRESULT hr;
		Check(Alive());
		Check(link->view->EditColumn(this,1,NULL,value));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListColumn::SetAlignment(QuadooForm::ListColumnAlignment value)
	{
		HRESULT hr;
		Check(Alive());
		Check(link->view->EditColumn(this,2,NULL,value));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListColumn::SetDisplayIndex(UINT value)
	{
		HRESULT hr;
		Check(Alive());
		Check(link->view->EditColumn(this,3,NULL,static_cast<INT>(value)));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListColumn::AutoSize(QuadooForm::ListColumnAutoSize mode)
	{
		HRESULT hr;
		Check(Alive());
		Check(link->view->AutoSizeColumn(this,mode));
	Cleanup:
		return hr;
	}
	CListColumns::CListColumns(CListLink* link) : m_link(link)
	{
	}
	STDMETHODIMP CListColumns::GetCount(UINT* count)
	{
		HRESULT hr;
		CheckIf(!count,E_POINTER);
		*count = 0;
		Check(m_link->Alive());
		*count = static_cast<UINT>(m_link->view->columns.Length());
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListColumns::GetAt(UINT index, IQuadooListColumn** column)
	{
		HRESULT hr;
		CheckIf(!column,E_POINTER);
		*column = NULL;
		Check(m_link->Alive());
		CheckIf(index >= static_cast<UINT>(m_link->view->columns.Length()),DISP_E_BADINDEX);
		*column = m_link->view->columns[index];
		(*column)->AddRef();
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListColumns::Find(REFGUID id, IQuadooListColumn** column)
	{
		HRESULT hr;
		CheckIf(!column,E_POINTER);
		*column = NULL;
		Check(m_link->Alive());
		for(sysint n = 0; n < m_link->view->columns.Length(); n++)
		{
			CListColumn* candidate = m_link->view->columns[n];
			if(IsEqualGUID(candidate->id,id))
			{
				*column = candidate;
				candidate->AddRef();
				goto Cleanup;
			}
		}
		hr = S_FALSE;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListColumns::Insert(UINT index, RSTRING caption, IQuadooListColumn** column)
	{
		HRESULT hr;
		CheckIf(!column,E_POINTER);
		*column = NULL;
		Check(m_link->Alive());
		Check(m_link->view->InsertColumn(index,caption,column));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListColumns::Delete(IQuadooListColumn* column)
	{
		HRESULT hr;
		Check(m_link->Alive());
		Check(m_link->view->DeleteColumn(column));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListColumns::Move(IQuadooListColumn* column, UINT index)
	{
		HRESULT hr;
		Check(m_link->Alive());
		Check(m_link->view->MoveColumn(column,index));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListColumns::Clear(VOID)
	{
		HRESULT hr;
		Check(m_link->Alive());
		Check(m_link->view->ClearColumns());
	Cleanup:
		return hr;
	}
}
