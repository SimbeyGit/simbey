#include "ListView.h"

namespace QF
{
	LRESULT CListView::ItemNotification(NMHDR* header, BOOL* handled)
	{
		HRESULT hr = S_OK;
		TStackRef<CListView> keepAlive(this);
		TStackRef<CListItem> item;
		TStackRef<CEventArgs> args;
		UINT index = static_cast<UINT>(reinterpret_cast<NMLISTVIEW*>(header)->iItem);
		BOOL cancel = FALSE;
		LRESULT result = 0;
		if(IsVirtual())
		{
			return VirtualNotification(header,handled);
		}
		if(header->code == LVN_GETDISPINFOW)
		{
			NMLVDISPINFOW* info = reinterpret_cast<NMLVDISPINFOW*>(header);
			index = info->item.iItem;
			*handled = TRUE;
			if(index < static_cast<UINT>(rows.Length()))
			{
				item = rows[index];
				if(info->item.mask & LVIF_TEXT)
				{
					CRString text;
					if(info->item.iSubItem)
					{
						hr = item->GetSubItem(info->item.iSubItem - 1,&text);
					}
					else
					{
						text = item->caption;
					}
					if(info->item.pszText && info->item.cchTextMax > 0)
					{
						lstrcpynW(info->item.pszText,SUCCEEDED(hr) && text ? text.c_wstr() : L"",info->item.cchTextMax);
					}
				}
				if(info->item.mask & LVIF_IMAGE)
				{
					HIMAGELIST images = ListView_GetImageList(m_hwnd,
						Integer(L"ViewStyle") == QuadooForm::ListViewIcon ? LVSIL_NORMAL : LVSIL_SMALL);
					info->item.iImage = images && item->image < ImageList_GetImageCount(images) ? item->image : -1;
				}
			}
			return 0;
		}
		switch(header->code)
		{
		case LVN_ITEMCHANGING:
		case LVN_ITEMCHANGED:
		case LVN_BEGINLABELEDITW:
		case LVN_ENDLABELEDITW:
		case LVN_ITEMACTIVATE:
			break;
		default:
			return 0;
		}
		*handled = TRUE;
		if(header->code == LVN_BEGINLABELEDITW || header->code == LVN_ENDLABELEDITW)
		{
			index = reinterpret_cast<NMLVDISPINFOW*>(header)->item.iItem;
		}
		if(m_nativeSync || m_busy || m_replacing || m_state != QuadooForm::Ready)
		{
			return 0;
		}
		if(m_events)
		{
			return header->code == LVN_ITEMCHANGING || header->code == LVN_BEGINLABELEDITW;
		}
		if(index >= static_cast<UINT>(rows.Length()))
		{
			return header->code == LVN_BEGINLABELEDITW;
		}
		item = rows[index];
		switch(header->code)
		{
		case LVN_ITEMCHANGING:
			{
				NMLISTVIEW* change = reinterpret_cast<NMLISTVIEW*>(header);
				UINT difference = change->uOldState ^ change->uNewState;
				UINT mask = ((difference & LVIS_SELECTED) ? QuadooForm::ListChangeSelected : 0) |
					((difference & LVIS_FOCUSED) ? QuadooForm::ListChangeFocused : 0) |
					((difference & LVIS_STATEIMAGEMASK) ? QuadooForm::ListChangeState : 0);
				if((change->uChanged & LVIF_STATE) && mask)
				{
					QuadooForm::ContextMode mode;
					Check(m_context->GetMode(&mode));
					if(mode == QuadooForm::Design && (mask & QuadooForm::ListChangeState))
					{
						result = TRUE;
						break;
					}
					if((mask & QuadooForm::ListChangeState) && Boolean(L"CheckBoxes"))
					{
						// Commit through one logical path so a handler can replace
						// Checked as well as cancel it. Veto the original native copy.
						INT proposed = static_cast<INT>((change->uNewState & LVIS_STATEIMAGEMASK) >> 12) - 1;
						hr = EditItem(item,QuadooForm::ListChangeState,NULL,NULL,proposed,NULL);
						result = TRUE;
					}
					else
					{
						hr = Changing(item,mask,change->uOldState,change->uNewState,&cancel);
						result = cancel || FAILED(hr);
					}
				}
			}
			break;
		case LVN_ITEMCHANGED:
			{
				NMLISTVIEW* change = reinterpret_cast<NMLISTVIEW*>(header);
				UINT mask = 0;
				if(change->uChanged & LVIF_STATE)
				{
					if(item->selected != ((change->uNewState & LVIS_SELECTED) != 0))
					{
						mask |= QuadooForm::ListChangeSelected;
					}
					if(item->focused != ((change->uNewState & LVIS_FOCUSED) != 0))
					{
						mask |= QuadooForm::ListChangeFocused;
					}
					INT state = item->state;
					if((change->uOldState ^ change->uNewState) & LVIS_STATEIMAGEMASK)
					{
						state = static_cast<INT>((change->uNewState & LVIS_STATEIMAGEMASK) >> 12) - 1;
					}
					if(item->state != state)
					{
						mask |= QuadooForm::ListChangeState;
					}
					item->selected = (change->uNewState & LVIS_SELECTED) != 0;
					item->focused = (change->uNewState & LVIS_FOCUSED) != 0;
					item->state = state;
					if(mask)
					{
						hr = ItemEvent(L"OnChange",item,mask);
					}
					if(SUCCEEDED(hr) && (mask & QuadooForm::ListChangeSelected))
					{
						hr = ItemEvent(L"OnSelectItem",item,item->selected);
					}
					if(SUCCEEDED(hr) && (mask & QuadooForm::ListChangeState))
					{
						hr = NotifyItems();
					}
				}
			}
			break;
		case LVN_BEGINLABELEDITW:
			{
				QuadooForm::ContextMode mode;
				Check(m_context->GetMode(&mode));
				result = TRUE;
				CheckIfIgnore(mode == QuadooForm::Design || Boolean(L"ReadOnly"),S_OK);
				Check(NewEvent(L"OnEditing",&args));
				if(args)
				{
					args->SetNativeObject(1,static_cast<IQuadooListItem*>(item));
					Check(InvokeEvent(args));
					Check(args->GetBool(0,&cancel));
				}
				result = cancel;
				m_editing = !cancel;
			}
			break;
		case LVN_ENDLABELEDITW:
			{
				NMLVDISPINFOW* edit = reinterpret_cast<NMLVDISPINFOW*>(header);
				CRString text;
				m_editing = FALSE;
				CheckIfIgnore(!edit->item.pszText,S_OK);
				Check(RStrCreateW(TStrLenAssert(edit->item.pszText),edit->item.pszText,&text));
				Check(NewEvent(L"OnEdited",&args));
				if(args)
				{
					args->SetNativeObject(1,static_cast<IQuadooListItem*>(item));
					args->SetNativeString(3,text);
					Check(InvokeEvent(args));
					Check(args->GetBool(0,&cancel));
					text = NULL;
					Check(args->GetString(3,&text));
				}
				if(!cancel)
				{
					Check(item->SetCaption(text));
				}
				// Model owns the accepted text, including event replacements.
				// Returning FALSE prevents native storage replacing the callback.
				result = FALSE;
			}
			break;
		case LVN_ITEMACTIVATE:
			Check(NewEvent(L"OnItemActivate",&args));
			if(args)
			{
				args->SetNativeObject(0,static_cast<IQuadooListItem*>(item));
				args->SetNativeValue(1,reinterpret_cast<NMITEMACTIVATE*>(header)->iSubItem);
				Check(InvokeEvent(args));
			}
			break;
		}
	Cleanup:
		if(FAILED(hr) && header->code == LVN_BEGINLABELEDITW)
		{
			result = TRUE;
			m_editing = FALSE;
		}
		if(FAILED(hr) && header->code == LVN_ITEMCHANGING)
		{
			result = TRUE;
		}
		ReportCallback(hr,L"ListView");
		return result;
	}
}
