#include "ListView.h"

namespace QF
{
	LRESULT CListView::VirtualNotification(NMHDR* header, BOOL* handled)
	{
		HRESULT hr = S_OK;
		TStackRef<CListView> keepAlive(this);
		TStackRef<CListRequest> request;
		TStackRef<CEventArgs> args;
		LRESULT result = 0;
		*handled = TRUE;
		switch(header->code)
		{
		case LVN_GETDISPINFOW:
			{
				NMLVDISPINFOW* info = reinterpret_cast<NMLVDISPINFOW*>(header);
				info->item.mask &= ~LVIF_DI_SETITEM;
				UINT fields = 0;
				if((info->item.mask & LVIF_TEXT) && info->item.pszText && info->item.cchTextMax > 0)
				{
					info->item.pszText[0] = 0;
				}
				if(info->item.mask & LVIF_IMAGE)
				{
					info->item.iImage = -1;
					fields |= QuadooForm::ListDataImage;
					if(Boolean(L"CheckBoxes"))
					{
						fields |= QuadooForm::ListDataState;
					}
				}
				if(info->item.mask & LVIF_STATE)
				{
					info->item.state &= ~LVIS_STATEIMAGEMASK;
					fields |= QuadooForm::ListDataState;
				}
				if(info->item.mask & LVIF_TEXT)
				{
					fields |= info->item.iSubItem ? QuadooForm::ListDataSubItems : QuadooForm::ListDataCaption;
				}
				CheckIfIgnore(info->item.iItem < 0 || static_cast<UINT>(info->item.iItem) >= m_virtualCount,S_OK);
				Check(FetchVirtual(info->item.iItem,fields,max(0,info->item.iSubItem),&request));
				if(info->item.mask & LVIF_TEXT)
				{
					CRString text;
					if(info->item.iSubItem)
					{
						Check(request->GetSubItem(info->item.iSubItem - 1,&text));
					}
					else
					{
						text = request->caption;
					}
					if(info->item.pszText && info->item.cchTextMax > 0)
					{
						lstrcpynW(info->item.pszText,text.c_wstr(),info->item.cchTextMax);
					}
				}
				if(info->item.mask & LVIF_IMAGE)
				{
					info->item.iImage = VirtualImageIndex(request->image,request->state == 1);
				}
				if(info->item.mask & LVIF_STATE)
				{
					info->item.state |= INDEXTOSTATEIMAGEMASK(request->state + 1);
				}
				info->item.mask &= ~LVIF_DI_SETITEM;
			}
			break;
		case LVN_ODCACHEHINT:
			{
				NMLVCACHEHINT* hint = reinterpret_cast<NMLVCACHEHINT*>(header);
				CheckIfIgnore(!m_virtualCount || hint->iFrom < 0 || hint->iTo < hint->iFrom ||
					static_cast<UINT>(hint->iFrom) >= m_virtualCount,S_OK);
				Check(NewEvent(L"OnDataHint",&args));
				if(args)
				{
					args->SetNativeValue(0,hint->iFrom);
					args->SetNativeValue(1,min(static_cast<UINT>(hint->iTo),m_virtualCount - 1));
					Check(InvokeEvent(args));
				}
			}
			break;
		case LVN_ODFINDITEMW:
			{
				NMLVFINDITEMW* find = reinterpret_cast<NMLVFINDITEMW*>(header);
				CRString text;
				INT index = -1;
				result = -1;
				CheckIfIgnore(!(find->lvfi.flags & LVFI_STRING) || !find->lvfi.psz || !m_virtualCount,S_OK);
				Check(NewEvent(L"OnDataFind",&args));
				if(args)
				{
					Check(RStrCreateW(TStrLenAssert(find->lvfi.psz),find->lvfi.psz,&text));
					Check(ValidateListText(text));
					args->SetNativeString(0,text);
					args->SetNativeValue(1,max(0,find->iStart));
					args->SetNativeValue(2,QuadooForm::ListFindIgnoreCase |
						((find->lvfi.flags & LVFI_PARTIAL) ? QuadooForm::ListFindPartial : 0) |
						((find->lvfi.flags & LVFI_WRAP) ? QuadooForm::ListFindWrap : 0));
					args->SetNativeValue(3,-1);
					Check(InvokeEvent(args));
					Check(args->GetInt(3,&index));
					CheckIf(index < -1 || (index >= 0 && static_cast<UINT>(index) >= m_virtualCount),E_INVALIDARG);
					result = index;
				}
			}
			break;
		case LVN_ITEMCHANGING:
			// Native OwnerData does not promise this notification.
			break;
		case LVN_ITEMCHANGED:
		case LVN_ODSTATECHANGED:
			{
				VirtualState state;
				CheckIfIgnore(m_state != QuadooForm::Ready || m_replacing || !m_virtualCount,S_OK);
				if(header->code == LVN_ODSTATECHANGED)
				{
					NMLVODSTATECHANGE* change = reinterpret_cast<NMLVODSTATECHANGE*>(header);
					CheckIfIgnore(change->iFrom < 0 || change->iTo < change->iFrom,S_OK);
					state.first = change->iFrom;
					state.last = change->iTo;
					state.previous = change->uOldState;
					state.next = change->uNewState;
				}
				else
				{
					NMLISTVIEW* change = reinterpret_cast<NMLISTVIEW*>(header);
					CheckIfIgnore(!(change->uChanged & LVIF_STATE),S_OK);
					state.first = change->iItem < 0 ? 0 : change->iItem;
					state.last = change->iItem < 0 ? m_virtualCount - 1 : change->iItem;
					state.previous = change->uOldState;
					state.next = change->uNewState;
				}
				CheckIfIgnore(!((state.previous ^ state.next) & (LVIS_SELECTED | LVIS_FOCUSED)),S_OK);
				hr = m_virtualStates.Append(state);
				if(FAILED(hr))
				{
					m_stateError = hr;
				}
				Check(hr);
				if(!m_nativeSync && !m_events)
				{
					Check(DrainVirtualStates());
				}
			}
			break;
		case LVN_BEGINLABELEDITW:
			{
				NMLVDISPINFOW* edit = reinterpret_cast<NMLVDISPINFOW*>(header);
				BOOL cancel = FALSE;
				QuadooForm::ContextMode mode;
				result = TRUE;
				Check(m_context->GetMode(&mode));
				CheckIfIgnore(m_nativeSync || m_busy || m_events || mode == QuadooForm::Design ||
					Boolean(L"ReadOnly") || !Bound(L"OnDataUpdate"),S_OK);
				Check(FetchVirtual(edit->item.iItem,31,0,&request));
				Check(NewEvent(L"OnEditing",&args));
				if(args)
				{
					args->SetNativeObject(1,static_cast<IQuadooListItem*>(request));
					Check(InvokeEvent(args));
					Check(args->GetBool(0,&cancel));
				}
				m_virtualEdit = cancel ? -1 : edit->item.iItem;
				m_editGeneration = m_generation;
				m_editing = !cancel;
				result = cancel;
			}
			break;
		case LVN_ENDLABELEDITW:
			{
				NMLVDISPINFOW* edit = reinterpret_cast<NMLVDISPINFOW*>(header);
				INT index = m_virtualEdit;
				BOOL cancel = FALSE;
				CRString text;
				m_virtualEdit = -1;
				m_editing = FALSE;
				CheckIfIgnore(m_nativeSync || index < 0 || !edit->item.pszText || m_editGeneration != m_generation,S_OK);
				Check(FetchVirtual(index,31,0,&request));
				Check(RStrCreateW(TStrLenAssert(edit->item.pszText),edit->item.pszText,&text));
				Check(NewEvent(L"OnEdited",&args));
				if(args)
				{
					args->SetNativeObject(1,static_cast<IQuadooListItem*>(request));
					args->SetNativeString(3,text);
					Check(InvokeEvent(args));
					Check(args->GetBool(0,&cancel));
					text = NULL;
					Check(args->GetString(3,&text));
				}
				if(!cancel)
				{
					Check(VirtualUpdate(index,0,text,FALSE,QuadooForm::ListChangeCaption));
				}
			}
			break;
		case LVN_ITEMACTIVATE:
			{
				NMITEMACTIVATE* activation = reinterpret_cast<NMITEMACTIVATE*>(header);
				CheckIfIgnore(activation->iItem < 0,S_OK);
				Check(FetchVirtual(activation->iItem,31,max(0,activation->iSubItem),&request));
				Check(VirtualItemEvent(L"OnItemActivate",request,activation->iSubItem));
			}
			break;
		default:
			*handled = FALSE;
			break;
		}
	Cleanup:
		ReportVirtualError(hr,L"OwnerData");
		return result;
	}
	HRESULT CListView::VirtualCheckGesture(INT index, BOOL multiple)
	{
		HRESULT hr;
		TStackRef<CListView> keepAlive(this);
		TStackRef<CListRequest> focused;
		ULONGLONG generation = m_generation;
		UINT count = m_virtualCount;
		Check(Mutable());
		CheckIf(index < 0 || static_cast<UINT>(index) >= count,E_INVALIDARG);
		Check(FetchVirtual(index,QuadooForm::ListDataState,0,&focused));
		BOOL checked = focused->state != 1;
		if(multiple && focused->selected)
		{
			// Enumerate only selected indexes; never manufacture an array sized
			// to Count. Each accepted backend update is its own transaction.
			INT selected = -1;
			while((selected = ListView_GetNextItem(m_hwnd,selected,LVNI_SELECTED)) >= 0)
			{
				Check(UpdateVirtualChecked(selected,checked));
				CheckIf(!VirtualAttached(index,generation) || m_virtualCount != count,QuadooForm::EInvalidState);
			}
		}
		else
		{
			Check(UpdateVirtualChecked(index,checked));
		}
	Cleanup:
		return hr;
	}
}
